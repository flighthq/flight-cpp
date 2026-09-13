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

// Materializes the part of the pinned Flight SDK the pinned compiler can emit today. Refusals are
// output too: this tree is an honest compiler bring-up inventory, not a claim that Flight::Sdk can
// already be built. Each generated header has one canonical home shared by examples and tests.

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const options = process.argv.slice(2);
const check = options.includes('--check');
const bindingProfileOptions = options
  .filter((option) => option.startsWith('--binding-profile='))
  .map((option) => option.slice('--binding-profile='.length));
const outputOption = options.find((option) => option.startsWith('--output='));
const unknown = options.filter(
  (option) =>
    option !== '--check' &&
    !option.startsWith('--binding-profile=') &&
    !option.startsWith('--output='),
);
if (unknown.length > 0) {
  process.stderr.write(`Unknown SDK generation option(s): ${unknown.join(', ')}\n`);
  process.exit(1);
}

const flight = resolveDependency(root, 'flight');
const compiler = resolveDependency(root, 'flight-compiler');
const generatedRoot = outputOption
  ? path.resolve(root, outputOption.slice('--output='.length))
  : path.join(root, 'generated');
const bindingProfiles = loadBindingProfiles(bindingProfileOptions);
const inputFailure = validateInput(flight, compiler);
if (inputFailure !== undefined) {
  const message = `${inputFailure} Run \`npm run rehydrate\` and \`npm ci --prefix .dependencies/flight-compiler\`.\n`;
  if (check) {
    process.stdout.write(`SDK generation check skipped: ${message}`);
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

const temporaryRoot = mkdtempSync(path.join(tmpdir(), 'flight-cpp-sdk-generation-'));
const candidateRoot = path.join(temporaryRoot, 'generated');

try {
  const result = await generateSdk(candidateRoot, flight, compiler, compilerEntry, bindingProfiles);
  if (check) {
    const drift = compareTrees(candidateRoot, generatedRoot);
    if (drift.length > 0) {
      process.stderr.write(`Generated SDK inventory differs in ${String(drift.length)} path(s):\n`);
      for (const filename of drift.slice(0, 20)) process.stderr.write(`- ${filename}\n`);
      if (drift.length > 20) process.stderr.write(`- … and ${String(drift.length - 20)} more\n`);
      process.stderr.write('Run `npm run sdk:generate` and commit the resulting generated/ tree.\n');
      process.exitCode = 1;
    } else {
      process.stdout.write(summary('Generated SDK inventory is current', result));
    }
  } else {
    rmSync(generatedRoot, { force: true, recursive: true });
    cpSync(candidateRoot, generatedRoot, { recursive: true });
    process.stdout.write(summary('Generated SDK inventory updated', result));
  }
} finally {
  rmSync(temporaryRoot, { force: true, recursive: true });
}

async function generateSdk(outputRoot, flightDependency, compilerDependency, compilerModule, profiles) {
  const {
    analyzeFlightWorkspace,
    compileTypeScriptPackageGraph,
    createCompilerModuleResolutionPlan,
    createCppCompilerBackend,
    parseTypeScriptSource,
  } = await import(pathToFileURL(compilerModule));
  const sdkPackageFile = path.join(flightDependency.directory, 'packages', 'sdk', 'package.json');
  const sdkPackage = JSON.parse(readFileSync(sdkPackageFile, 'utf8'));
  const packageNames = Object.keys(sdkPackage.dependencies ?? {})
    .filter((name) => name.startsWith('@flighthq/'))
    .sort();
  const inventory = analyzeFlightWorkspace({
    targetPackageNames: packageNames,
    upstreamDirectory: flightDependency.directory,
  });
  const includedPackages = new Set(packageNames);
  const packageDescriptors = inventory.packages.map((package_) => {
    const root = path.join(flightDependency.directory, package_.directory);
    return {
      dependencies: package_.dependencies.filter((dependency) => includedPackages.has(dependency)).sort(compareText),
      name: package_.name,
      root,
      sources: listSourceFiles(path.join(root, 'src')),
      target: {
        includePrefix: `flight/${cppPackageName(package_.name)}`,
        namespace: `flight::${cppPackageName(package_.name)}`,
      },
    };
  });
  const sources = packageDescriptors.flatMap((package_) =>
    package_.sources.map((source) => ({
      packageName: package_.name,
      packageRoot: package_.root,
      sourceFile: parseTypeScriptSource(source.sourcePath, source.contents),
      upstreamDirectory: flightDependency.directory,
    })),
  );
  const compilation = compileTypeScriptPackageGraph({
    backend: createCppCompilerBackend(),
    backendOptions: {
      ...(profiles.length > 0
        ? {
            externalBindings: mergeBindingProfiles(profiles),
          }
        : {}),
      packageTargets: Object.fromEntries(packageDescriptors.map((package_) => [package_.name, package_.target])),
      runtimeProfile: 'flight-cpp',
      upstreamCommit: flightDependency.commit,
    },
    graph: {
      entries: [],
      moduleDependencies: [],
      packages: packageDescriptors.map((package_) => ({
        dependencies: package_.dependencies,
        name: package_.name,
        root: package_.root,
      })),
      schema: 'flight-compiler-package-graph/1',
    },
    moduleResolution: createCompilerModuleResolutionPlan(inventory),
    sources,
  });
  for (const file of compilation.compilation.files) {
    const target = path.join(outputRoot, 'include', file.path);
    mkdirSync(path.dirname(target), { recursive: true });
    writeFileSync(target, file.contents);
  }
  writeStructuralMemberTable(outputRoot, compilation.compilation.files);
  writeSdkBuildMetadata(outputRoot, compilation.compilation.files);
  const descriptorByName = new Map(packageDescriptors.map((package_) => [package_.name, package_]));
  const packageResults = compilation.report.packages.map((package_) => {
    const descriptor = descriptorByName.get(package_.name);
    if (!descriptor) throw new Error(`Compiler reported unknown package ${package_.name}`);
    return {
      cppIncludePrefix: descriptor.target.includePrefix,
      cppNamespace: descriptor.target.namespace,
      emittedModules: package_.modules.filter((module) => module.status === 'emitted').length,
      package: package_.name,
      refusedModules: package_.modules.filter((module) => module.status === 'refused').length,
      sourceModules: package_.modules.length,
    };
  });
  const refusals = compilation.report.modules.flatMap((module) =>
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
    schema: 'flight-generated-sdk/1',
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
    namespaceMapping: {
      desiredRoot: 'flight',
      status: 'applied',
    },
    packages: packageResults,
    source: {
      package: String(sdkPackage.name),
      repository: flightDependency.repository,
      revision: flightDependency.commit,
      version: String(sdkPackage.version),
    },
    summary: {
      packages: packageResults.length,
      ...totals,
    },
  };
  const refusalLedger = {
    schema: 'flight-generated-sdk-refusals/2',
    compilerRevision: compilerDependency.commit,
    sourceRevision: flightDependency.commit,
    refusals,
  };

  mkdirSync(outputRoot, { recursive: true });
  writeFileSync(path.join(outputRoot, 'manifest.json'), `${JSON.stringify(manifest, undefined, 2)}\n`);
  writeFileSync(path.join(outputRoot, 'refusals.json'), `${JSON.stringify(refusalLedger, undefined, 2)}\n`);
  writeFileSync(
    path.join(outputRoot, 'initialization.json'),
    `${JSON.stringify(compilation.report.initialization, undefined, 2)}\n`,
  );
  writeFileSync(path.join(outputRoot, 'README.md'), generatedReadme(manifest));
  return manifest.summary;
}

function loadBindingProfiles(filenames) {
  const profiles = filenames.map((filename) => {
    const absolute = path.resolve(root, filename);
    const contents = readFileSync(absolute, 'utf8');
    const profile = JSON.parse(contents);
    if (
      profile?.schema !== 'flight-cpp-external-bindings/1' ||
      typeof profile.identity !== 'string' ||
      profile.identity.length === 0 ||
      typeof profile.profile !== 'string' ||
      profile.profile.length === 0 ||
      !Array.isArray(profile.bindings)
    ) {
      throw new TypeError(`${filename} is not an identified flight-cpp-external-bindings/1 profile`);
    }
    return {
      bindings: profile.bindings,
      digest: `sha256:${createHash('sha256').update(contents).digest('hex')}`,
      identity: profile.identity,
      path: portable(path.relative(root, absolute)),
      profile: profile.profile,
      schema: profile.schema,
    };
  });
  const identities = new Set();
  const symbols = new Set();
  for (const profile of profiles) {
    if (identities.has(profile.identity)) throw new TypeError(`duplicate binding profile ${profile.identity}`);
    identities.add(profile.identity);
    for (const binding of profile.bindings) {
      const symbol = `${String(binding.sourceName)}:${String(binding.space)}`;
      if (symbols.has(symbol)) throw new TypeError(`binding profiles provide ${symbol} more than once`);
      symbols.add(symbol);
    }
  }
  return profiles;
}

function mergeBindingProfiles(profiles) {
  return {
    bindings: profiles.flatMap((profile) => profile.bindings),
    schema: 'flight-cpp-external-bindings/1',
  };
}

function writeSdkBuildMetadata(outputRoot, files) {
  const headers = files.map((file) => file.path).sort(compareText);
  headers.push('flight/sdk/structural_members.hpp');
  const cmakeHeaders = headers
    .map((header) => `  "\${CMAKE_CURRENT_LIST_DIR}/../include/${header}"`)
    .join('\n');
  const cmakeTarget = path.join(outputRoot, 'cmake', 'FlightSdkHeaders.cmake');
  mkdirSync(path.dirname(cmakeTarget), { recursive: true });
  writeFileSync(
    cmakeTarget,
    `# Generated from the Flight SDK package graph. Do not edit.\nset(FLIGHT_SDK_GENERATED_HEADERS\n${cmakeHeaders}\n)\n`,
  );

  const bazelHeaders = headers.map((header) => `        "include/${header}",`).join('\n');
  writeFileSync(
    path.join(outputRoot, 'BUILD.bazel'),
    `# Generated from the Flight SDK package graph. Do not edit.\nload("@rules_cc//cc:defs.bzl", "cc_library")\n\npackage(default_visibility = ["//visibility:public"])\n\ncc_library(\n    name = "sdk_preview",\n    hdrs = [\n${bazelHeaders}\n    ],\n    strip_include_prefix = "include",\n    deps = ["//:cpp"],\n)\n`,
  );
}

function writeStructuralMemberTable(outputRoot, files) {
  const names = new Set();
  const rowKey = /flight::RowKey<"(?<name>(?:[^"\\]|\\.)*)">/gu;
  for (const file of files) {
    for (const match of file.contents.matchAll(rowKey)) {
      if (match.groups?.name !== undefined) names.add(JSON.parse(`"${match.groups.name}"`));
    }
  }
  const sortedNames = [...names].sort(compareText);
  const cases = sortedNames
    .map((name) => {
      const member = safeCppMemberName(name);
      return `  else if constexpr (Key::name.view() == std::string_view(${JSON.stringify(name)}) && requires { object.${member}; }) return (object.${member});`;
    });
  const typeCases = sortedNames.map((name) => {
    const member = safeCppMemberName(name);
    return `  else if constexpr (Key::name.view() == std::string_view(${JSON.stringify(name)}) && requires(Object& object) { object.${member}; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().${member})>>{};`;
  });
  const bindings = sortedNames.map((name) => {
    const member = safeCppMemberName(name);
    return `  if constexpr (requires { object->${member}; }) owner.bind_named(${JSON.stringify(name)}, [object]() -> decltype(auto) { return (object->${member}); });`;
  });
  if (cases.length === 0) {
    throw new Error('compiler output uses no structural row keys');
  }
  cases[0] = cases[0].replace('  else if', '  if');
  typeCases[0] = typeCases[0].replace('  else if', '  if');
  const target = path.join(outputRoot, 'include', 'flight', 'sdk', 'structural_members.hpp');
  mkdirSync(path.dirname(target), { recursive: true });
  writeFileSync(
    target,
    `// Generated from the structural keys used by the emitted Flight SDK. Do not edit.\n#pragma once\n\n#include <flight/structural_ref.hpp>\n\n#include <memory>\n#include <string_view>\n#include <type_traits>\n#include <utility>\n\nnamespace flight::detail {\n\ntemplate <typename Key, typename Object>\ndecltype(auto) generated_row_member(Object& object) {\n${cases.join('\n')}\n  else static_assert(dependent_false<Key>, "Flight SDK row key has no compatible generated C++ member");\n}\n\ntemplate <typename Key, typename Object>\nconsteval auto generated_row_member_type_identity() {\n${typeCases.join('\n')}\n  else return std::type_identity<void>{};\n}\n\ntemplate <typename Key, typename Object>\nusing generated_row_member_t = typename decltype(generated_row_member_type_identity<Key, Object>())::type;\n\ntemplate <typename Object>\nvoid bind_generated_row_members(RowOwner& owner, const std::shared_ptr<Object>& object) {\n${bindings.join('\n')}\n}\n\n} // namespace flight::detail\n`,
  );
}

function safeCppMemberName(name) {
  const snake = name
    .replace(/([a-z0-9])([A-Z])/gu, '$1_$2')
    .replace(/[^A-Za-z0-9]+/gu, '_')
    .replace(/^_+|_+$/gu, '')
    .toLowerCase();
  return isCppKeyword(snake) ? `${snake}_` : snake;
}

function isCppKeyword(value) {
  return new Set([
  'alignas', 'alignof', 'and', 'and_eq', 'asm', 'atomic_cancel', 'atomic_commit',
  'atomic_noexcept', 'auto', 'bitand', 'bitor', 'bool', 'break', 'case', 'catch',
  'char', 'char8_t', 'char16_t', 'char32_t', 'class', 'compl', 'concept', 'const',
  'consteval', 'constexpr', 'constinit', 'const_cast', 'continue', 'co_await',
  'co_return', 'co_yield', 'decltype', 'default', 'delete', 'do', 'double',
  'dynamic_cast', 'else', 'enum', 'explicit', 'export', 'extern', 'false', 'float',
  'for', 'friend', 'goto', 'if', 'inline', 'int', 'long', 'mutable', 'namespace',
  'new', 'noexcept', 'not', 'not_eq', 'nullptr', 'operator', 'or', 'or_eq',
  'private', 'protected', 'public', 'reflexpr', 'register', 'reinterpret_cast',
  'requires', 'return', 'short', 'signed', 'sizeof', 'static', 'static_assert',
  'static_cast', 'struct', 'switch', 'synchronized', 'template', 'this',
  'thread_local', 'throw', 'true', 'try', 'typedef', 'typeid', 'typename', 'union',
  'unsigned', 'using', 'virtual', 'void', 'volatile', 'wchar_t', 'while', 'xor',
  'xor_eq',
  ]).has(value);
}

function generatedReadme(manifest) {
  const profiles = manifest.bindingProfiles.length === 0
    ? 'No external binding profile is applied; this is the portable floor.'
    : `Applied binding profiles: ${manifest.bindingProfiles.map((profile) => `\`${profile.identity}\``).join(', ')}.`;
  return `# Generated Flight SDK inventory

This directory is generated from \`${manifest.source.package}\` ${manifest.source.version} at
\`${manifest.source.revision}\` by \`flight-compiler\` at \`${manifest.compiler.revision}\`.
Do not edit it by hand.

${profiles} Exact profile paths and SHA-256 digests are recorded in \`manifest.json\`.

The current compiler emitted ${manifest.summary.emittedModules} of ${manifest.summary.sourceModules} source modules from
${manifest.summary.packages} SDK packages and refused ${manifest.summary.refusedModules}. Emitted headers live under
\`include/flight/<package>/\`; every refusal and its owning module is recorded in \`refusals.json\`.

This is a bring-up inventory. It is intentionally committed before it forms a completely compilable SDK closure.
CMake exposes the full inventory as \`Flight::SdkPreview\`, and Bazel exposes \`//:sdk_preview\`; the preview name
keeps the remaining native compile failures visible. The package graph applies the public C++ \`flight\` namespaces
and installed include prefixes. \`initialization.json\` records the compiler's dependency and module-evaluation plan.

Regenerate and verify the tree from the repository root:

\`\`\`sh
npm run rehydrate
npm ci --prefix .dependencies/flight-compiler
npm run sdk:generate
npm run sdk:check
\`\`\`

Generate a profile-specific inventory outside the committed portable tree with:

\`\`\`sh
node scripts/sdkGeneration.mjs --binding-profile=bindings/runtime.json --binding-profile=bindings/headless.json --output=out/sdk-headless
\`\`\`
`;
}

function filesUnder(directory) {
  const files = [];
  for (const entry of readdirSync(directory).sort()) {
    const filename = path.join(directory, entry);
    const status = lstatSync(filename);
    if (status.isSymbolicLink()) continue;
    if (status.isDirectory()) files.push(...filesUnder(filename));
    else files.push(filename);
  }
  return files;
}

function listSourceFiles(directory) {
  if (!existsSync(directory)) return [];
  return filesUnder(directory)
    .filter((filename) => filename.endsWith('.ts') && !filename.endsWith('.d.ts') && !filename.endsWith('.test.ts'))
    .map((sourcePath) => ({ contents: readFileSync(sourcePath, 'utf8'), sourcePath }));
}

function cppPackageName(packageName) {
  return packageName.slice('@flighthq/'.length).replaceAll('-', '_');
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
  const paths = [...new Set([...expected.keys(), ...actual.keys()])].sort();
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
  if (!existsSync(path.join(flightDependency.directory, 'packages', 'sdk', 'package.json'))) {
    return 'the pinned Flight checkout has no @flighthq/sdk package.';
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

function portable(filename) {
  return filename.split(path.sep).join('/');
}

function compareText(left, right) {
  return left < right ? -1 : left > right ? 1 : 0;
}

function summary(prefix, result) {
  return `${prefix}: ${String(result.emittedModules)}/${String(result.sourceModules)} modules emitted across ${String(result.packages)} packages; ${String(result.refusedModules)} refusals recorded.\n`;
}
