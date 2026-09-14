import { spawnSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import {
  cpSync,
  existsSync,
  lstatSync,
  mkdirSync,
  mkdtempSync,
  readFileSync,
  readdirSync,
  rmSync,
  writeFileSync,
} from 'node:fs';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

import { resolveDependency } from './dependencyLock.mjs';

// Compiles every upstream example in one graph with the SDK packages it imports. Generated example
// headers have their own tree; SDK headers continue to come from the canonical top-level generated/
// inventory. Refused modules remain first-class output so each compiler or host improvement produces
// a reviewable change even before an example becomes runnable.

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const options = process.argv.slice(2);
const check = options.includes('--check');
const unknown = options.filter((option) => option !== '--check');
if (unknown.length > 0) {
  process.stderr.write(`Unknown upstream example generation option(s): ${unknown.join(', ')}\n`);
  process.exit(1);
}

const flight = resolveDependency(root, 'flight');
const compiler = resolveDependency(root, 'flight-compiler');
const generatedRoot = path.join(root, 'examples', 'upstream', 'generated');
const bindingProfileFiles = [
  'bindings/runtime.json',
  'bindings/headless.json',
  'bindings/web-types.json',
  'bindings/sdl-gl.json',
  'bindings/sdl-app.json',
];
const bindingProfiles = loadBindingProfiles(bindingProfileFiles);
const inputFailure = validateInput(flight, compiler);
if (inputFailure !== undefined) {
  const message = `${inputFailure} Run \`npm run rehydrate\` and \`npm ci --prefix .dependencies/flight-compiler\`.\n`;
  if (check) {
    process.stdout.write(`Upstream example generation check skipped: ${message}`);
    process.exit(0);
  }
  process.stderr.write(message);
  process.exit(1);
}

runCompilerBuild(compiler.directory);
const compilerEntry = path.join(
  compiler.directory,
  'packages',
  'tool-compiler',
  'dist',
  'packages',
  'tool-compiler',
  'src',
  'index.js',
);
if (!existsSync(compilerEntry)) {
  process.stderr.write(`Pinned compiler build did not produce ${compilerEntry}\n`);
  process.exit(1);
}

const temporaryRoot = mkdtempSync(path.join(tmpdir(), 'flight-cpp-upstream-examples-'));
const candidateRoot = path.join(temporaryRoot, 'generated');

try {
  const result = await generateExamples(candidateRoot, flight, compiler, compilerEntry, bindingProfiles);
  if (check) {
    const drift = compareTrees(candidateRoot, generatedRoot);
    if (drift.length > 0) {
      process.stderr.write(`Generated upstream example inventory differs in ${String(drift.length)} path(s):\n`);
      for (const filename of drift.slice(0, 20)) process.stderr.write(`- ${filename}\n`);
      if (drift.length > 20) process.stderr.write(`- … and ${String(drift.length - 20)} more\n`);
      process.stderr.write('Run `npm run examples:generate` and commit examples/upstream/generated/.\n');
      process.exitCode = 1;
    } else {
      process.stdout.write(summary('Generated upstream example inventory is current', result));
    }
  } else {
    rmSync(generatedRoot, { force: true, recursive: true });
    cpSync(candidateRoot, generatedRoot, { recursive: true });
    process.stdout.write(summary('Generated upstream example inventory updated', result));
  }
} finally {
  rmSync(temporaryRoot, { force: true, recursive: true });
}

async function generateExamples(outputRoot, flightDependency, compilerDependency, compilerModule, profiles) {
  const {
    analyzeFlightWorkspace,
    compileTypeScriptPackageGraph,
    createCompilerModuleResolutionPlan,
    createCppCompilerBackend,
    parseTypeScriptSource,
  } = await import(pathToFileURL(compilerModule));

  const examples = readExamplePackages(flightDependency.directory);
  const sdkPackage = readJson(path.join(flightDependency.directory, 'packages', 'sdk', 'package.json'));
  const packageNames = new Set([
    '@flighthq/sdk',
    ...Object.keys(sdkPackage.dependencies ?? {}),
    ...examples.flatMap((example) => example.dependencies),
  ]);
  const inventory = analyzeFlightWorkspace({
    targetPackageNames: [...packageNames].sort(compareText),
    upstreamDirectory: flightDependency.directory,
  });
  const sdkDescriptors = inventory.packages
    .filter((package_) => packageNames.has(package_.name))
    .map((package_) => {
      const packageRoot = path.join(flightDependency.directory, package_.directory);
      return {
        dependencies: package_.dependencies.filter((dependency) => packageNames.has(dependency)).sort(compareText),
        name: package_.name,
        packageRoot,
        sources: listSourceFiles(path.join(packageRoot, 'src')),
        target: sdkTarget(package_.name),
      };
    });
  const exampleDescriptors = examples.map((example) => {
    const selection = selectNativeExampleSources(example.directory);
    return {
      dependencies: example.dependencies.filter((dependency) => packageNames.has(dependency)).sort(compareText),
      name: example.packageName,
      packageRoot: example.directory,
      renderer: selection.renderer,
      selectorRemap: selection.selectorRemap,
      sources: selection.sources,
      target: {
        includePrefix: `flight/examples/${example.cppName}`,
        namespace: `flight::examples::${example.cppName}`,
      },
      upstreamName: example.name,
      upstreamSourceModules: selection.upstreamSourceModules,
    };
  });
  const descriptors = [...sdkDescriptors, ...exampleDescriptors];
  const sources = descriptors.flatMap((package_) =>
    package_.sources.map((source) => ({
      packageName: package_.name,
      packageRoot: package_.packageRoot,
      sourceFile: parseTypeScriptSource(source.sourcePath, source.contents),
      upstreamDirectory: flightDependency.directory,
    })),
  );
  const compilation = compileTypeScriptPackageGraph({
    backend: createCppCompilerBackend(),
    backendOptions: {
      externalBindings: mergeBindingProfiles(profiles),
      packageTargets: Object.fromEntries(descriptors.map((package_) => [package_.name, package_.target])),
      runtimeProfile: 'flight-cpp',
      upstreamCommit: flightDependency.commit,
    },
    graph: {
      entries: [],
      moduleDependencies: [],
      packages: descriptors.map((package_) => ({
        dependencies: package_.dependencies,
        name: package_.name,
        root: package_.packageRoot,
      })),
      schema: 'flight-compiler-package-graph/1',
    },
    moduleResolution: createCompilerModuleResolutionPlan(inventory),
    sources,
  });

  const examplePackageNames = new Set(exampleDescriptors.map((example) => example.name));
  const exampleModules = compilation.report.modules.filter((module) => examplePackageNames.has(module.module.packageName));
  const frontierModules = exampleDescriptors.flatMap((example) => {
    const result = compileTypeScriptPackageGraph({
      backend: createCppCompilerBackend(),
      backendOptions: {
        externalBindings: mergeBindingProfiles(profiles),
        packageTargets: Object.fromEntries(descriptors.map((package_) => [package_.name, package_.target])),
        runtimeProfile: 'flight-cpp',
        upstreamCommit: flightDependency.commit,
      },
      graph: {
        entries: [],
        moduleDependencies: [],
        packages: [{ dependencies: [], name: example.name, root: example.packageRoot }],
        schema: 'flight-compiler-package-graph/1',
      },
      sources: sources.filter((source) => source.packageName === example.name),
    });
    return result.report.modules;
  });
  const exampleFiles = compilation.compilation.files.filter((file) => file.path.startsWith('flight/examples/'));
  for (const file of exampleFiles) {
    const target = path.join(outputRoot, 'include', file.path);
    mkdirSync(path.dirname(target), { recursive: true });
    writeFileSync(target, file.contents);
  }

  const packageResults = exampleDescriptors.map((example) => {
    const modules = exampleModules.filter((module) => module.module.packageName === example.name);
    const frontier = frontierModules.filter((module) => module.module.packageName === example.name);
    return {
      cppIncludePrefix: example.target.includePrefix,
      cppNamespace: example.target.namespace,
      emittedModules: modules.filter((module) => module.status === 'emitted').length,
      frontierEmittedModules: frontier.filter((module) => module.status === 'emitted').length,
      frontierRefusedModules: frontier.filter((module) => module.status === 'refused').length,
      package: example.name,
      refusedModules: modules.filter((module) => module.status === 'refused').length,
      renderer: example.renderer,
      selectorRemap: example.selectorRemap,
      sourceModules: modules.length,
      upstreamPackage: example.upstreamName,
      upstreamSourceModules: example.upstreamSourceModules,
    };
  });
  const refusals = exampleModules.flatMap((module) =>
    module.refusals.map((refusal) => ({
      code: refusal.code,
      ...(refusal.column === undefined ? {} : { column: refusal.column }),
      ...(refusal.line === undefined ? {} : { line: refusal.line }),
      module: module.module.source,
      package: module.module.packageName,
      reason: refusal.message,
      stage: refusal.stage,
    })),
  );
  const frontierRefusals = frontierModules.flatMap((module) =>
    module.refusals.map((refusal) => ({
      code: refusal.code,
      ...(refusal.column === undefined ? {} : { column: refusal.column }),
      ...(refusal.line === undefined ? {} : { line: refusal.line }),
      module: module.module.source,
      package: module.module.packageName,
      reason: refusal.message,
      stage: refusal.stage,
    })),
  );
  const totals = packageResults.reduce(
    (result, item) => ({
      emittedModules: result.emittedModules + item.emittedModules,
      refusedModules: result.refusedModules + item.refusedModules,
      sourceModules: result.sourceModules + item.sourceModules,
    }),
    { emittedModules: 0, refusedModules: 0, sourceModules: 0 },
  );
  const manifest = {
    schema: 'flight-generated-upstream-examples/1',
    compiler: {
      repository: compilerDependency.repository,
      revision: compilerDependency.commit,
      runtimeProfile: 'flight-cpp',
      target: 'cpp',
    },
    bindingProfiles: profiles.map((profile) => ({
      digest: profile.digest,
      identity: profile.identity,
      path: profile.path,
      profile: profile.profile,
      schema: profile.schema,
    })),
    generatedSdk: {
      location: '../../../generated',
      ownership: 'shared',
    },
    packages: packageResults,
    source: {
      directory: 'examples/packages',
      repository: flightDependency.repository,
      revision: flightDependency.commit,
    },
    summary: {
      packages: packageResults.length,
      frontierEmittedModules: frontierModules.filter((module) => module.status === 'emitted').length,
      frontierRefusedModules: frontierModules.filter((module) => module.status === 'refused').length,
      upstreamSourceModules: packageResults.reduce((total, package_) => total + package_.upstreamSourceModules, 0),
      ...totals,
    },
  };
  const refusalLedger = {
    schema: 'flight-generated-upstream-example-refusals/1',
    compilerRevision: compilerDependency.commit,
    sourceRevision: flightDependency.commit,
    refusals,
  };
  const frontierRefusalLedger = {
    schema: 'flight-generated-upstream-example-frontier-refusals/1',
    compilerRevision: compilerDependency.commit,
    sourceRevision: flightDependency.commit,
    refusals: frontierRefusals,
  };
  const initialization = filterInitialization(compilation.report.initialization, examplePackageNames);

  mkdirSync(outputRoot, { recursive: true });
  writeFileSync(path.join(outputRoot, 'manifest.json'), `${JSON.stringify(manifest, undefined, 2)}\n`);
  writeFileSync(path.join(outputRoot, 'refusals.json'), `${JSON.stringify(refusalLedger, undefined, 2)}\n`);
  writeFileSync(
    path.join(outputRoot, 'frontier-refusals.json'),
    `${JSON.stringify(frontierRefusalLedger, undefined, 2)}\n`,
  );
  writeFileSync(path.join(outputRoot, 'initialization.json'), `${JSON.stringify(initialization, undefined, 2)}\n`);
  writeFileSync(path.join(outputRoot, 'README.md'), generatedReadme(manifest));
  return manifest.summary;
}

function readExamplePackages(flightDirectory) {
  const packagesDirectory = path.join(flightDirectory, 'examples', 'packages');
  return readdirSync(packagesDirectory)
    .sort(compareText)
    .flatMap((name) => {
      const directory = path.join(packagesDirectory, name);
      const manifestFile = path.join(directory, 'package.json');
      if (!lstatSync(directory).isDirectory() || !existsSync(manifestFile)) return [];
      const manifest = readJson(manifestFile);
      const upstreamName = String(manifest.name ?? '');
      if (upstreamName.length === 0) throw new TypeError(`${manifestFile} has no package name`);
      const dependencies = Object.keys(manifest.dependencies ?? {}).sort(compareText);
      return [{
        cppName: safeCppName(upstreamName),
        dependencies,
        directory,
        name: upstreamName,
        packageName: `@flighthq/example-${upstreamName}`,
      }];
    });
}

function selectNativeExampleSources(exampleDirectory) {
  const allSources = listSourceFiles(path.join(exampleDirectory, 'src'));
  const webGl = allSources.find((source) => path.basename(source.sourcePath) === 'render.webgl.ts');
  const dom = allSources.find((source) => path.basename(source.sourcePath) === 'render.dom.ts');
  const rendererSource = webGl ?? dom;
  const renderer = webGl ? 'webgl' : dom ? 'dom-fallback' : 'unavailable';
  const selected = allSources.filter((source) => {
    const basename = path.basename(source.sourcePath);
    return !/^render\.(?:canvas|dom|webgl|webgpu)\.ts$/u.test(basename) || source === rendererSource;
  });
  const selector = selected.find((source) => path.basename(source.sourcePath) === 'render.ts');
  if (!selector || !rendererSource) {
    return {
      renderer,
      selectorRemap: null,
      sources: selected,
      upstreamSourceModules: allSources.length,
    };
  }
  // The compiler currently treats dotted renderer basenames as an unresolved evaluation edge.
  // Give the selected implementation a virtual, portable module name as part of the same explicit
  // source remap used for Flight's build-time RENDER alias.
  const virtualRendererPath = path.join(path.dirname(rendererSource.sourcePath), 'renderNative.ts');
  const target = './renderNative';
  return {
    renderer,
    selectorRemap: {
      implementation: {
        source: portable(path.relative(exampleDirectory, rendererSource.sourcePath)),
        target: portable(path.relative(exampleDirectory, virtualRendererPath)),
      },
      selector: { source: 'src/render.ts', target },
    },
    sources: selected.map((source) => {
      if (source === selector) return { ...source, contents: `export * from '${target}';\n` };
      if (source === rendererSource) return { ...source, sourcePath: virtualRendererPath };
      return source;
    }),
    upstreamSourceModules: allSources.length,
  };
}

function filterInitialization(initialization, packageNames) {
  const hasPackage = (module) => module && packageNames.has(module.packageName);
  return {
    entries: initialization.entries.filter(hasPackage),
    groups: initialization.groups.filter((group) => group.modules.some(hasPackage)),
    modules: initialization.modules.filter((module) => hasPackage(module.module)),
    schema: initialization.schema,
    semantics: initialization.semantics,
  };
}

function loadBindingProfiles(filenames) {
  return filenames.map((filename) => {
    const absolute = path.resolve(root, filename);
    const contents = readFileSync(absolute, 'utf8');
    const profile = JSON.parse(contents);
    if (
      profile?.schema !== 'flight-cpp-external-bindings/1' ||
      typeof profile.identity !== 'string' ||
      !Array.isArray(profile.bindings)
    ) {
      throw new TypeError(`${filename} is not a flight-cpp-external-bindings/1 profile`);
    }
    return {
      bindings: profile.bindings,
      digest: `sha256:${createHash('sha256').update(contents).digest('hex')}`,
      identity: profile.identity,
      path: portable(path.relative(root, absolute)),
      profile: String(profile.profile),
      schema: profile.schema,
    };
  });
}

function mergeBindingProfiles(profiles) {
  const symbols = new Set();
  const bindings = [];
  for (const profile of profiles) {
    for (const binding of profile.bindings) {
      const symbol = `${String(binding.sourceName)}:${String(binding.space)}`;
      if (symbols.has(symbol)) throw new TypeError(`binding profiles provide ${symbol} more than once`);
      symbols.add(symbol);
      bindings.push(binding);
    }
  }
  return { bindings, schema: 'flight-cpp-external-bindings/1' };
}

function sdkTarget(packageName) {
  const name = packageName.slice('@flighthq/'.length).replaceAll('-', '_');
  return { includePrefix: `flight/${name}`, namespace: `flight::${name}` };
}

function safeCppName(name) {
  return name.replace(/[^A-Za-z0-9]+/gu, '_').replace(/^_+|_+$/gu, '').toLowerCase();
}

function readJson(filename) {
  return JSON.parse(readFileSync(filename, 'utf8'));
}

function filesUnder(directory) {
  if (!existsSync(directory)) return [];
  const files = [];
  for (const entry of readdirSync(directory).sort(compareText)) {
    const filename = path.join(directory, entry);
    const status = lstatSync(filename);
    if (status.isSymbolicLink()) continue;
    if (status.isDirectory()) files.push(...filesUnder(filename));
    else files.push(filename);
  }
  return files;
}

function listSourceFiles(directory) {
  return filesUnder(directory)
    .filter((filename) => filename.endsWith('.ts') && !filename.endsWith('.d.ts') && !filename.endsWith('.test.ts'))
    .map((sourcePath) => ({ contents: readFileSync(sourcePath, 'utf8'), sourcePath }));
}

function compareTrees(expectedRoot, actualRoot) {
  if (!existsSync(actualRoot)) {
    return filesUnder(expectedRoot).map((filename) => portable(path.relative(expectedRoot, filename)));
  }
  const expected = new Map(
    filesUnder(expectedRoot).map((filename) => [portable(path.relative(expectedRoot, filename)), readFileSync(filename)]),
  );
  const actual = new Map(
    filesUnder(actualRoot).map((filename) => [portable(path.relative(actualRoot, filename)), readFileSync(filename)]),
  );
  const paths = [...new Set([...expected.keys(), ...actual.keys()])].sort(compareText);
  return paths.filter(
    (filename) =>
      !expected.has(filename) || !actual.has(filename) || !expected.get(filename).equals(actual.get(filename)),
  );
}

function validateInput(flightDependency, compilerDependency) {
  for (const dependency of [flightDependency, compilerDependency]) {
    if (!existsSync(path.join(dependency.directory, '.git'))) return `${dependency.name} is not rehydrated.`;
    const head = spawnSync('git', ['rev-parse', 'HEAD'], { cwd: dependency.directory, encoding: 'utf8' });
    if (head.status !== 0 || head.stdout.trim() !== dependency.commit) {
      return `${dependency.name} is not at pinned revision ${dependency.commit.slice(0, 7)}.`;
    }
    const status = spawnSync('git', ['status', '--porcelain'], { cwd: dependency.directory, encoding: 'utf8' });
    if (status.status !== 0 || status.stdout.length > 0) return `${dependency.name} has uncommitted changes.`;
  }
  if (!existsSync(path.join(compilerDependency.directory, 'node_modules', 'typescript'))) {
    return 'flight-compiler dependencies are not installed.';
  }
  if (!existsSync(path.join(flightDependency.directory, 'examples', 'packages'))) {
    return 'the pinned Flight checkout has no example packages.';
  }
  return undefined;
}

function runCompilerBuild(directory) {
  const npm = process.platform === 'win32' ? 'npm.cmd' : 'npm';
  const result = spawnSync(npm, ['run', 'build', '--silent'], { cwd: directory, encoding: 'utf8' });
  if (result.status !== 0) {
    process.stderr.write(result.stdout ?? '');
    process.stderr.write(result.stderr ?? '');
    process.stderr.write('Pinned flight-compiler build failed.\n');
    process.exit(1);
  }
}

function generatedReadme(manifest) {
  return `# Generated upstream Flight examples

This directory is generated from every package under Flight \`${manifest.source.directory}\` at
\`${manifest.source.revision}\` by \`flight-compiler\` at \`${manifest.compiler.revision}\`. Do not edit it by hand.

The SDL/GL native profile selected ${manifest.summary.sourceModules} of ${manifest.summary.upstreamSourceModules}
upstream modules across ${manifest.summary.packages} example packages and emitted ${manifest.summary.emittedModules}.
The selection mirrors Flight's \`RENDER=webgl\` alias by remapping each \`render.ts\` selector to its WebGL source;
the DOM-only cross-backend-embed example records its fallback explicitly. Every dependency-closed refusal is retained
in \`refusals.json\`.
\`frontier-refusals.json\` compiles each example without its package dependencies to expose the next direct source,
compiler, or host boundary hidden by dependency propagation. \`initialization.json\` retains any module-evaluation
plans available for emitted examples. Generated headers, as they become available,
live under \`include/flight/examples/\` and include the one shared SDK tree at the repository's top-level
\`generated/include/\`; this directory never contains another SDK copy.

Regenerate or verify this inventory from the repository root:

\`\`\`sh
npm run rehydrate
npm ci --prefix .dependencies/flight-compiler
npm run examples:generate
npm run examples:generate:check
\`\`\`
`;
}

function portable(filename) {
  return filename.split(path.sep).join('/');
}

function compareText(left, right) {
  return left < right ? -1 : left > right ? 1 : 0;
}

function summary(prefix, result) {
  return `${prefix}: ${String(result.emittedModules)}/${String(result.sourceModules)} modules emitted across ${String(result.packages)} examples; ${String(result.refusedModules)} refusals recorded.\n`;
}
