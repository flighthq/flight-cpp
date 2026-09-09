import { spawnSync } from 'node:child_process';
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
import { availableParallelism, tmpdir } from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { Worker } from 'node:worker_threads';

import { resolveDependency } from './dependencyLock.mjs';

// Materializes the part of the pinned Flight SDK the pinned compiler can emit today. Refusals are
// output too: this tree is an honest compiler bring-up inventory, not a claim that Flight::Sdk can
// already be built. Each generated header has one canonical home shared by examples and tests.

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const options = process.argv.slice(2);
const check = options.includes('--check');
const unknown = options.filter((option) => option !== '--check');
if (unknown.length > 0) {
  process.stderr.write(`Unknown SDK generation option(s): ${unknown.join(', ')}\n`);
  process.exit(1);
}

const flight = resolveDependency(root, 'flight');
const compiler = resolveDependency(root, 'flight-compiler');
const generatedRoot = path.join(root, 'generated');
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
  const result = await generateSdk(candidateRoot, flight, compiler, compilerEntry);
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

async function generateSdk(outputRoot, flightDependency, compilerDependency, compilerModule) {
  const sdkPackageFile = path.join(flightDependency.directory, 'packages', 'sdk', 'package.json');
  const sdkPackage = JSON.parse(readFileSync(sdkPackageFile, 'utf8'));
  const packageNames = Object.keys(sdkPackage.dependencies ?? {})
    .filter((name) => name.startsWith('@flighthq/'))
    .sort();
  const requestedJobs = Number.parseInt(process.env.FLIGHT_CPP_SDK_JOBS ?? '', 10);
  const jobs = Number.isInteger(requestedJobs) && requestedJobs > 0
    ? Math.min(requestedJobs, packageNames.length)
    : Math.min(availableParallelism(), 8, packageNames.length);
  const assignments = Array.from({ length: jobs }, () => []);
  packageNames.forEach((packageName, index) => assignments[index % jobs].push(packageName));
  const workerResults = await Promise.all(
    assignments.map((assignedPackages) =>
      runGenerationWorker({
        compilerEntry: compilerModule,
        flightDirectory: flightDependency.directory,
        outputRoot,
        packageNames: assignedPackages,
      }),
    ),
  );
  const packageResults = workerResults
    .flatMap((result) => result.packageResults)
    .sort((left, right) => (left.package < right.package ? -1 : 1));
  const refusals = workerResults
    .flatMap((result) => result.refusals)
    .sort(
      (left, right) =>
        compareText(left.package, right.package) || compareText(left.module, right.module),
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
    namespaceMapping: {
      desiredRoot: 'flight',
      status: 'pending flight-compiler namespace remap support',
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
    schema: 'flight-generated-sdk-refusals/1',
    compilerRevision: compilerDependency.commit,
    sourceRevision: flightDependency.commit,
    refusals,
  };

  mkdirSync(outputRoot, { recursive: true });
  writeFileSync(path.join(outputRoot, 'manifest.json'), `${JSON.stringify(manifest, undefined, 2)}\n`);
  writeFileSync(path.join(outputRoot, 'refusals.json'), `${JSON.stringify(refusalLedger, undefined, 2)}\n`);
  writeFileSync(path.join(outputRoot, 'README.md'), generatedReadme(manifest));
  return manifest.summary;
}

function runGenerationWorker(workerData) {
  return new Promise((resolve, reject) => {
    const worker = new Worker(new URL('./sdkGenerationWorker.mjs', import.meta.url), { workerData });
    let result;
    worker.once('message', (message) => {
      result = message;
    });
    worker.once('error', reject);
    worker.once('exit', (code) => {
      if (code !== 0) reject(new Error(`SDK generation worker exited with code ${String(code)}`));
      else if (result === undefined) reject(new Error('SDK generation worker returned no result'));
      else resolve(result);
    });
  });
}

function generatedReadme(manifest) {
  return `# Generated Flight SDK inventory

This directory is generated from \`${manifest.source.package}\` ${manifest.source.version} at
\`${manifest.source.revision}\` by \`flight-compiler\` at \`${manifest.compiler.revision}\`.
Do not edit it by hand.

The current compiler emitted ${manifest.summary.emittedModules} of ${manifest.summary.sourceModules} source modules from
${manifest.summary.packages} SDK packages and refused ${manifest.summary.refusedModules}. Emitted headers live under
\`include/flight/<package>/\`; every refusal and its owning module is recorded in \`refusals.json\`.

This is a bring-up inventory. It is intentionally committed before it forms a compilable SDK closure, and CMake and
Bazel do not publish it as \`Flight::Sdk\` yet. Current generated namespaces still reflect the npm publisher scope.
The manifest records the pending remap to the public C++ \`flight\` namespace.

Regenerate and verify the tree from the repository root:

\`\`\`sh
npm run rehydrate
npm ci --prefix .dependencies/flight-compiler
npm run sdk:generate
npm run sdk:check
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
