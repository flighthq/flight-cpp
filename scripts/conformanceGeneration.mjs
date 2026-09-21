import { spawnSync } from 'node:child_process';
import { existsSync, mkdtempSync, mkdirSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

import { resolveDependency } from './dependencyLock.mjs';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const options = process.argv.slice(2);
const check = options.includes('--check');
const unknown = options.filter((option) => option !== '--check');
if (unknown.length > 0) {
  process.stderr.write(`Unknown conformance generation option(s): ${unknown.join(', ')}\n`);
  process.exit(1);
}

const compiler = resolveDependency(root, 'flight-compiler');
if (!existsSync(compiler.directory)) {
  process.stdout.write('flight-compiler is not rehydrated (native conformance generation skipped).\n');
  process.exit(0);
}

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
  const build = spawnSync('npm', ['run', 'build', '--silent'], {
    cwd: compiler.directory,
    encoding: 'utf8',
  });
  if (build.status !== 0) {
    process.stderr.write(`${build.stdout ?? ''}${build.stderr ?? ''}`);
    process.exit(1);
  }
  if (!existsSync(compilerEntry)) {
    process.stderr.write(`Pinned compiler build did not produce ${compilerEntry}\n`);
    process.exit(1);
  }
}

const sourcePath = path.join(root, 'tests', 'generated', 'semantic_runtime.ts');
const headerPath = path.join(root, 'tests', 'generated', 'semantic_runtime.hpp');
const api = await import(pathToFileURL(compilerEntry));
const source = api.parseTypeScriptSource(
  '/flight/packages/cpp-conformance/src/semantic-runtime.ts',
  readFileSync(sourcePath, 'utf8'),
);
const lowered = api.lowerTypeScriptSource(source, {
  packageName: '@flighthq/cpp-conformance',
  upstreamDirectory: '/flight',
});
if (lowered.diagnostics.length > 0) {
  process.stderr.write(`${JSON.stringify(lowered.diagnostics, undefined, 2)}\n`);
  process.exit(1);
}
const emitted = api.emitIrModuleCpp(lowered.module, { runtimeProfile: 'flight-cpp' }).contents;

if (!check) {
  mkdirSync(path.dirname(headerPath), { recursive: true });
  writeFileSync(headerPath, emitted);
  process.stdout.write(
    `Native conformance header regenerated with flight-compiler ${compiler.commit.slice(0, 7)}.\n`,
  );
  process.exit(0);
}

const temporary = mkdtempSync(path.join(tmpdir(), 'flight-cpp-native-conformance-'));
try {
  const candidatePath = path.join(temporary, 'semantic_runtime.hpp');
  writeFileSync(candidatePath, emitted);
  if (!existsSync(headerPath) || readFileSync(headerPath, 'utf8') !== emitted) {
    if (!existsSync(headerPath)) {
      process.stderr.write(`Native conformance header is missing: ${headerPath}\n`);
    } else {
      const difference = spawnSync(
        'git',
        ['diff', '--no-index', '--no-ext-diff', '--', headerPath, candidatePath],
        { cwd: root, encoding: 'utf8' },
      );
      process.stderr.write(`${difference.stdout ?? ''}${difference.stderr ?? ''}`);
    }
    process.stderr.write('Run `npm run conformance:generate` and commit the refreshed header.\n');
    process.exitCode = 1;
  } else {
    process.stdout.write(
      `Native conformance header matches flight-compiler ${compiler.commit.slice(0, 7)} output.\n`,
    );
  }
} finally {
  rmSync(temporary, { force: true, recursive: true });
}
