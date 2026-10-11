import { spawnSync } from 'node:child_process';
import { existsSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

import {
  ctestLabelOf,
  testTargetOf,
  packageOutputDirectory,
  parseJobs,
  parsePackageArguments,
  portable,
} from './packageScope.mjs';

// `npm run pkg:compile -- <package>` compiles ONE package's generated headers, each on its own, at -j2.
//
// It is scripts/sdkHeaderCompile.mjs with `--package`, so the verdict is the same one the folded-tree
// sweep gives, judged only for this package: another package's drifted override does not fail it. The
// report lands in out/pkg/<package>/ where `pkg:tier` reads it.
//
// `--ctest` also builds and runs the CTest tests labelled `pkg:<package>` in the build directory --
// configuring it first if it has never been -- building only the targets those tests run.
//
//   -j N, --jobs=N       parallel compiler processes (default 2; FLIGHT_CPP_COMPILE_JOBS is ignored here)
//   --generated=DIR      the inventory to compile (default: generated)
//   --resume, --retry-killed, --deadline-seconds=N   passed through to sdkHeaderCompile.mjs
//   --ctest              also build and run the package's labelled CTest tests
//   --build-dir=DIR      the CMake build directory for --ctest (default: build)

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
let parsed;
let jobs;
try {
  parsed = parsePackageArguments(process.argv.slice(2), {
    flags: ['--resume', '--retry-killed', '--ctest'],
    valued: ['--generated', '--deadline-seconds', '--build-dir'],
  });
  jobs = parseJobs(parsed.values.get('--jobs'));
} catch (error) {
  process.stderr.write(`pkg:compile: ${error.message}\n`);
  process.exit(1);
}
const packageName = parsed.package;
const output = packageOutputDirectory(root, packageName);
const report = path.join(output, 'header-compilation.json');

const compile = spawnSync(
  process.execPath,
  [
    path.join(root, 'scripts', 'sdkHeaderCompile.mjs'),
    `--package=${packageName}`,
    `--report=${report}`,
    ...(parsed.values.has('--generated') ? [`--generated=${parsed.values.get('--generated')}`] : []),
    ...(parsed.values.has('--deadline-seconds') ? [`--deadline-seconds=${parsed.values.get('--deadline-seconds')}`] : []),
    ...['--resume', '--retry-killed'].filter((flag) => parsed.flags.has(flag)),
  ],
  { cwd: root, env: { ...process.env, FLIGHT_CPP_COMPILE_JOBS: String(jobs) }, stdio: 'inherit' },
);
let status = compile.status ?? 1;

if (parsed.flags.has('--ctest')) status = Math.max(status, runLabelledTests());
process.exit(status);

function runLabelledTests() {
  const label = ctestLabelOf(packageName);
  const labelPattern = `^${label.replace(/[.*+?^${}()|[\]\\]/gu, '\\$&')}$`;
  const buildDirectory = path.resolve(root, parsed.values.get('--build-dir') ?? 'build');
  const shown = portable(path.relative(root, buildDirectory));
  if (!existsSync(path.join(buildDirectory, 'CTestTestfile.cmake'))) {
    process.stdout.write(`Configuring ${shown} with tests enabled...\n`);
    if (!run('cmake', ['-S', root, '-B', buildDirectory, '-DBUILD_TESTING=ON'])) return 1;
  }
  const listing = spawnSync('ctest', ['--test-dir', buildDirectory, '-L', labelPattern, '--show-only=json-v1'], {
    encoding: 'utf8',
  });
  if (listing.status !== 0) {
    process.stderr.write(listing.stderr ?? '');
    return 1;
  }
  const tests = JSON.parse(listing.stdout).tests ?? [];
  if (tests.length === 0) {
    process.stdout.write(`No CTest test is labelled ${label}; nothing to build or run.\n`);
    return 0;
  }
  // ctest reports no command for an executable that has not been built yet, which is the usual case
  // here, so the target comes from the test name; scripts/packageScope.test.mjs pins that convention.
  const targets = [...new Set(tests.map((test) => testTargetOf(test.name)))].sort();
  process.stdout.write(`Building ${targets.join(', ')} for ${label}...\n`);
  if (!run('cmake', ['--build', buildDirectory, '--parallel', String(jobs), '--target', ...targets])) return 1;
  return run('ctest', [
    '--test-dir',
    buildDirectory,
    '-L',
    labelPattern,
    '--parallel',
    String(jobs),
    '--output-on-failure',
  ])
    ? 0
    : 1;
}

function run(command, args) {
  const result = spawnSync(command, args, { cwd: root, stdio: 'inherit' });
  if (result.error !== undefined) {
    process.stderr.write(`pkg:compile: could not run ${command}: ${result.error.message}\n`);
    return false;
  }
  return result.status === 0;
}

