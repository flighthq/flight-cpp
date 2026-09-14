import { spawnSync } from 'node:child_process';
import { existsSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

import { resolveDependency } from './dependencyLock.mjs';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const compiler = resolveDependency(root, 'flight-compiler');
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
if (!existsSync(compiler.directory)) {
  process.stdout.write('flight-compiler is not rehydrated (headless profile oracle skipped).\n');
  process.exit(0);
}

const cppCompiler = process.env.CXX || 'c++';
if (spawnSync(cppCompiler, ['--version'], { encoding: 'utf8' }).status !== 0) {
  process.stdout.write(`No C++ compiler available as ${cppCompiler} (headless profile oracle skipped).\n`);
  process.exit(0);
}

if (!existsSync(compilerEntry)) {
  const build = spawnSync('npm', ['run', 'build', '--silent'], { cwd: compiler.directory, encoding: 'utf8' });
  if (build.status !== 0) {
    process.stderr.write(`${build.stdout}${build.stderr}`);
    process.exit(1);
  }
}

const api = await import(pathToFileURL(compilerEntry));
const bindings = JSON.parse(readFileSync(path.join(root, 'bindings', 'headless.json'), 'utf8'));
const source = api.parseTypeScriptSource(
  '/flight/packages/host-test/src/headless.ts',
  `export function exercise(message: string, callback: () => void): number {
     console.debug(message);
     const interval = setInterval(callback, 0);
     clearInterval(interval);
     const timeout = setTimeout(callback, 0);
     clearTimeout(timeout);
     const entries = performance.getEntriesByType('navigation') as PerformanceNavigationTiming[];
     if (entries.length !== 0) throw new Error('native process unexpectedly has browser navigation history');
     return performance.now();
   }`,
);
const lowered = api.lowerTypeScriptSource(source, {
  packageName: '@flighthq/host-test',
  upstreamDirectory: '/flight',
});
if (lowered.diagnostics.length > 0) {
  process.stderr.write(`${JSON.stringify(lowered.diagnostics, undefined, 2)}\n`);
  process.exit(1);
}
const emitted = api.emitIrModuleCpp(lowered.module, {
  externalBindings: bindings,
  runtimeProfile: 'flight-cpp',
}).contents;
for (const header of ['console', 'performance', 'timers']) {
  if (!emitted.includes(`#include <flight/host/${header}.hpp>`)) {
    process.stderr.write(`Headless binding fixture did not emit flight/host/${header}.hpp.\n`);
    process.exit(1);
  }
}
if (!emitted.includes('flight::host::performance_get_entries_by_type')) {
  process.stderr.write('Headless binding fixture did not emit performance navigation lookup.\n');
  process.exit(1);
}

const temporary = mkdtempSync(path.join(tmpdir(), 'flight-cpp-headless-profile-'));
try {
  writeFileSync(path.join(temporary, 'headless.hpp'), emitted);
  writeFileSync(
    path.join(temporary, 'consumer.cpp'),
    `#include "headless.hpp"

int main() {
  int calls = 0;
  const double timestamp = flighthq_host_test::exercise(
      flight::String("headless profile oracle"), [&] { ++calls; });
  static_cast<void>(flight::host::pump_timers());
  return timestamp >= 0.0 && calls == 0 ? 0 : 1;
}
`,
  );
  const executable = path.join(temporary, process.platform === 'win32' ? 'oracle.exe' : 'oracle');
  const compilation = spawnSync(
    cppCompiler,
    [
      '-std=c++20',
      '-pthread',
      '-I',
      path.join(root, 'include'),
      '-I',
      temporary,
      path.join(temporary, 'consumer.cpp'),
      '-o',
      executable,
    ],
    { cwd: root, encoding: 'utf8' },
  );
  if (compilation.status !== 0) {
    process.stderr.write(`${compilation.stdout}${compilation.stderr}`);
    process.exit(1);
  }
  const native = spawnSync(executable, [], { cwd: root, encoding: 'utf8' });
  if (native.status !== 0) {
    process.stderr.write(`${native.stdout}${native.stderr}`);
    process.exit(1);
  }
} finally {
  rmSync(temporary, { force: true, recursive: true });
}

process.stdout.write(
  `Headless binding profile emits, compiles, and runs console/timer/performance use (${compiler.commit.slice(0, 7)}, ${cppCompiler}).\n`,
);
