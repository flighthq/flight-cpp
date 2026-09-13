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
  process.stdout.write('flight-compiler is not rehydrated (abort oracle skipped).\n');
  process.exit(0);
}

const cppCompiler = process.env.CXX || 'c++';
if (spawnSync(cppCompiler, ['--version'], { encoding: 'utf8' }).status !== 0) {
  process.stdout.write(`No C++ compiler available as ${cppCompiler} (abort oracle skipped).\n`);
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
const bindings = JSON.parse(readFileSync(path.join(root, 'bindings', 'runtime.json'), 'utf8'));
const source = api.parseTypeScriptSource(
  '/flight/packages/runtime-test/src/abort.ts',
  `export function listenerResult(): boolean {
     const controller = new AbortController();
     let calls = 0;
     const listener = (): void => { calls++; };
     controller.signal.addEventListener('abort', listener, { once: true });
     controller.signal.addEventListener('abort', listener, { once: true });
     controller.abort('first');
     controller.abort('second');
     return controller.signal.aborted && calls === 1;
   }
   export function removedResult(): boolean {
     const controller = new AbortController();
     let calls = 0;
     const listener = (): void => { calls++; };
     controller.signal.addEventListener('abort', listener);
     controller.signal.removeEventListener('abort', listener);
     controller.abort();
     return calls === 0;
   }
   export function makeAborted(): AbortSignal {
     const controller = new AbortController();
     controller.abort('cancelled');
     return controller.signal;
   }
   export function requireActive(signal: AbortSignal): void {
     signal.throwIfAborted();
   }`,
);
const lowered = api.lowerTypeScriptSource(source, {
  packageName: '@flighthq/runtime-test',
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
for (const expected of [
  '#include <flight/abort.hpp>',
  'flight::AbortController()',
  '.add_event_listener(',
  '.remove_event_listener(',
  '.throw_if_aborted()',
]) {
  if (!emitted.includes(expected)) {
    process.stderr.write(`Abort binding fixture did not emit ${expected}.\n`);
    process.exit(1);
  }
}

const temporary = mkdtempSync(path.join(tmpdir(), 'flight-cpp-abort-'));
try {
  writeFileSync(path.join(temporary, 'abort.hpp'), emitted);
  writeFileSync(
    path.join(temporary, 'consumer.cpp'),
    `#include "abort.hpp"

#include <iostream>

int main() {
  const bool listener = flighthq_runtime_test::listener_result();
  const bool removed = flighthq_runtime_test::removed_result();
  const auto signal = flighthq_runtime_test::make_aborted();
  bool threw = false;
  try {
    flighthq_runtime_test::require_active(signal);
  } catch (const flight::AbortError&) {
    threw = true;
  }
  std::cout << std::boolalpha << listener << '|' << removed << '|' << threw << '|'
            << signal.reason.get<flight::String>().to_utf8();
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

  const node = spawnSync(
    process.execPath,
    [
      '--input-type=module',
      '--eval',
      `const first = new AbortController();
let firstCalls = 0;
const firstListener = () => { firstCalls++; };
first.signal.addEventListener('abort', firstListener, { once: true });
first.signal.addEventListener('abort', firstListener, { once: true });
first.abort('first');
first.abort('second');
const second = new AbortController();
let secondCalls = 0;
const secondListener = () => { secondCalls++; };
second.signal.addEventListener('abort', secondListener);
second.signal.removeEventListener('abort', secondListener);
second.abort();
let threw = false;
try { first.signal.throwIfAborted(); } catch { threw = true; }
process.stdout.write(String(first.signal.aborted && firstCalls === 1) + '|' +
  String(secondCalls === 0) + '|' + String(threw) + '|cancelled');`,
    ],
    { cwd: root, encoding: 'utf8' },
  );
  if (node.status !== 0 || native.stdout !== node.stdout) {
    process.stderr.write(
      `${node.stdout}${node.stderr}Abort source differential failed.\nExpected: ${node.stdout}\nActual:   ${native.stdout}\n`,
    );
    process.exit(1);
  }
} finally {
  rmSync(temporary, { force: true, recursive: true });
}

process.stdout.write(
  `Compiler-emitted abort bindings match native AbortController behavior (${compiler.commit.slice(0, 7)}, ${cppCompiler}).\n`,
);
