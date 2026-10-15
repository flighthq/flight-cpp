import { spawnSync } from 'node:child_process';
import { existsSync, mkdtempSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

import { resolveDependency } from './dependencyLock.mjs';
import { applySourcePatches, loadSourcePatches, revertSourcePatches } from './sourcePatches.mjs';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const compiler = resolveDependency(root, 'flight-compiler');
const cxx = process.env.CXX || 'c++';
if (!existsSync(path.join(compiler.directory, '.git')) || spawnSync(cxx, ['--version'], { encoding: 'utf8' }).status !== 0) {
  process.stdout.write('Named re-export oracle skipped: rehydrate the compiler and set CXX to a C++20 compiler.\n');
  process.exit(0);
}
for (const args of [['rev-parse', 'HEAD'], ['status', '--porcelain']]) {
  const result = spawnSync('git', args, { cwd: compiler.directory, encoding: 'utf8' });
  if (result.status !== 0 || result.stdout.trim() !== (args[0] === 'rev-parse' ? compiler.commit : '')) {
    throw new Error('Named re-export oracle requires the clean exact pinned compiler checkout');
  }
}
function run(command, args, options = {}) {
  const result = spawnSync(command, args, { cwd: root, encoding: 'utf8', ...options });
  if (result.status !== 0) {
    process.stderr.write(`${result.stdout ?? ''}${result.stderr ?? ''}`);
    throw new Error(`${command} failed (${result.error?.message ?? result.status})`);
  }
  return result;
}
function build() {
  run(process.platform === 'win32' ? 'npm.cmd' : 'npm', ['run', 'build', '--silent'], { cwd: compiler.directory });
}
const patches = loadSourcePatches(root).filter((entry) => entry.dependency === 'flight-compiler');
const temporary = mkdtempSync(path.join(tmpdir(), 'flight-named-reexport-'));
try {
  applySourcePatches(patches, compiler);
  build();
  const cases = path.join(root, 'scripts', 'fixtures', 'named-reexport', 'cases.mjs');
  const entry = path.join(compiler.directory, 'packages', 'tool-compiler', 'dist', 'packages', 'tool-compiler', 'src', 'index.js');
  const controls = run(process.execPath, [cases], {
    env: { ...process.env, FLIGHT_CPP_COMPILER_ENTRY: entry, FLIGHT_CPP_REEXPORT_NATIVE_OUTPUT: temporary },
  });
  process.stdout.write(controls.stdout);
  const executable = path.join(temporary, process.platform === 'win32' ? 'consumer.exe' : 'consumer');
  run(cxx, [
    '-std=c++20', '-ffp-contract=off',
    '-I', path.join(root, 'include'), '-I', path.join(temporary, 'types'), '-I', temporary,
    path.join(temporary, 'consumer.cpp'), '-o', executable,
  ]);
  run(executable, []);
  process.stdout.write('Generated named re-exports retain constant address and mutable variable storage identity.\n');
} finally {
  revertSourcePatches(patches, compiler);
  build();
  rmSync(temporary, { recursive: true, force: true });
}
