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
  process.stdout.write('flight-compiler is not rehydrated (stream oracle skipped).\n');
  process.exit(0);
}

const cppCompiler = process.env.CXX || 'c++';
if (spawnSync(cppCompiler, ['--version'], { encoding: 'utf8' }).status !== 0) {
  process.stdout.write(`No C++ compiler available as ${cppCompiler} (stream oracle skipped).\n`);
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
  '/flight/packages/runtime-test/src/stream.ts',
  `export async function writeAll(
     stream: WritableStream<Uint8Array>,
     first: Uint8Array,
     second: Uint8Array,
   ): Promise<void> {
     const writer = stream.getWriter();
     await writer.write(first);
     await writer.write(second);
     await writer.close();
   }
   export function keepReadable(stream: ReadableStream<Uint8Array>): ReadableStream<Uint8Array> {
     return stream;
   }
   export function keepChunks(chunks: AsyncIterable<Uint8Array>): AsyncIterable<Uint8Array> {
     return chunks;
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
  '#include <flight/stream.hpp>',
  'flight::WritableStream<flight::Uint8Array>',
  'flight::ReadableStream<flight::Uint8Array>',
  'flight::AsyncIterable<flight::Uint8Array>',
  '.get_writer()',
  'co_await writer.write(first)',
  'co_await writer.close()',
]) {
  if (!emitted.includes(expected)) {
    process.stderr.write(`Stream binding fixture did not emit ${expected}.\n${emitted}\n`);
    process.exit(1);
  }
}

const temporary = mkdtempSync(path.join(tmpdir(), 'flight-cpp-stream-'));
try {
  writeFileSync(path.join(temporary, 'stream.hpp'), emitted);
  writeFileSync(
    path.join(temporary, 'consumer.cpp'),
    `#include "stream.hpp"

#include <iostream>

int main() {
  flight::Array<double> bytes;
  bool closed = false;
  flight::WritableStream<flight::Uint8Array> stream({
      .write = [&](const flight::Uint8Array& chunk) {
        for (const auto byte : chunk) bytes.push(byte);
        return flight::Task<void>::resolve();
      },
      .close = [&] {
        closed = true;
        return flight::Task<void>::resolve();
      },
  });
  flighthq_runtime_test::write_all(
      stream, flight::Uint8Array{1, 2}, flight::Uint8Array{3}).get();
  const auto readable = flighthq_runtime_test::keep_readable(
      flight::ReadableStream<flight::Uint8Array>{});
  const auto iterable = flighthq_runtime_test::keep_chunks(
      flight::AsyncIterable<flight::Uint8Array>{});
  std::cout << bytes.join(flight::String(",")).to_utf8() << '|' << std::boolalpha << closed << '|'
            << (readable.identity() != nullptr) << '|' << (iterable.identity() != nullptr);
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
    process.stderr.write(`${compilation.stdout}${compilation.stderr}\nEmitted stream header:\n${emitted}\n`);
    process.exit(1);
  }
  const native = spawnSync(executable, [], { cwd: root, encoding: 'utf8' });
  const node = spawnSync(
    process.execPath,
    [
      '--input-type=module',
      '--eval',
      `const bytes = [];
let closed = false;
const stream = new WritableStream({
  write(chunk) { bytes.push(...chunk); },
  close() { closed = true; },
});
const writer = stream.getWriter();
await writer.write(new Uint8Array([1, 2]));
await writer.write(new Uint8Array([3]));
await writer.close();
process.stdout.write(bytes.join(',') + '|' + closed + '|true|true');`,
    ],
    { cwd: root, encoding: 'utf8' },
  );
  if (native.status !== 0 || node.status !== 0 || native.stdout !== node.stdout) {
    process.stderr.write(
      `${native.stderr}${node.stderr}Stream source differential failed.\nExpected: ${node.stdout}\nActual:   ${native.stdout}\n`,
    );
    process.exit(1);
  }
} finally {
  rmSync(temporary, { force: true, recursive: true });
}

process.stdout.write(
  `Compiler-emitted stream carriers match native stream behavior (${compiler.commit.slice(0, 7)}, ${cppCompiler}).\n`,
);
