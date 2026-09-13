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
  process.stdout.write('flight-compiler is not rehydrated (Blob oracle skipped).\n');
  process.exit(0);
}

const cppCompiler = process.env.CXX || 'c++';
if (spawnSync(cppCompiler, ['--version'], { encoding: 'utf8' }).status !== 0) {
  process.stdout.write(`No C++ compiler available as ${cppCompiler} (Blob oracle skipped).\n`);
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
  '/flight/packages/runtime-test/src/blob.ts',
  `export function makeText(): Blob {
     return new Blob(['Flight'], { type: 'TEXT/PLAIN' });
   }
   export function sliceText(blob: Blob): Blob {
     return blob.slice(1, -1, 'APPLICATION/X-FLIGHT');
   }
   export async function readText(blob: Blob): Promise<string> {
     return await blob.text();
   }
   export async function byteLength(blob: Blob): Promise<number> {
     const bytes = await blob.arrayBuffer();
     return bytes.byteLength;
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
  '#include <flight/blob.hpp>',
  'flight::Blob(',
  '.slice(',
  'co_await blob.text()',
  'co_await blob.array_buffer()',
  '.byte_length',
]) {
  if (!emitted.includes(expected)) {
    process.stderr.write(`Blob binding fixture did not emit ${expected}.\n${emitted}\n`);
    process.exit(1);
  }
}

const temporary = mkdtempSync(path.join(tmpdir(), 'flight-cpp-blob-'));
try {
  writeFileSync(path.join(temporary, 'blob.hpp'), emitted);
  writeFileSync(
    path.join(temporary, 'consumer.cpp'),
    `#include "blob.hpp"

#include <iostream>

int main() {
  const auto blob = flighthq_runtime_test::make_text();
  const auto sliced = flighthq_runtime_test::slice_text(blob);
  std::cout << blob.type.to_utf8() << '|' << blob.size << '|'
            << flighthq_runtime_test::read_text(blob).get().to_utf8() << '|'
            << sliced.type.to_utf8() << '|' << sliced.size << '|'
            << flighthq_runtime_test::byte_length(blob).get();
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
    process.stderr.write(`${compilation.stdout}${compilation.stderr}\nEmitted Blob header:\n${emitted}\n`);
    process.exit(1);
  }
  const native = spawnSync(executable, [], { cwd: root, encoding: 'utf8' });
  const node = spawnSync(
    process.execPath,
    [
      '--input-type=module',
      '--eval',
      `const blob = new Blob(['Flight'], { type: 'TEXT/PLAIN' });
const sliced = blob.slice(1, -1, 'APPLICATION/X-FLIGHT');
process.stdout.write(blob.type + '|' + blob.size + '|' + await blob.text() + '|' +
  sliced.type + '|' + sliced.size + '|' + (await blob.arrayBuffer()).byteLength);`,
    ],
    { cwd: root, encoding: 'utf8' },
  );
  if (native.status !== 0 || node.status !== 0 || native.stdout !== node.stdout) {
    process.stderr.write(
      `${native.stderr}${node.stderr}Blob source differential failed.\nExpected: ${node.stdout}\nActual:   ${native.stdout}\n`,
    );
    process.exit(1);
  }
} finally {
  rmSync(temporary, { force: true, recursive: true });
}

process.stdout.write(
  `Compiler-emitted Blob operations match native Blob behavior (${compiler.commit.slice(0, 7)}, ${cppCompiler}).\n`,
);
