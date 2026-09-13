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
  process.stdout.write('flight-compiler is not rehydrated (array-like oracle skipped).\n');
  process.exit(0);
}

const cppCompiler = process.env.CXX || 'c++';
if (spawnSync(cppCompiler, ['--version'], { encoding: 'utf8' }).status !== 0) {
  process.stdout.write(`No C++ compiler available as ${cppCompiler} (array-like oracle skipped).\n`);
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
  '/flight/packages/runtime-test/src/arrayLike.ts',
  `export function total(values: ArrayLike<number>): number {
     let result = 0;
     for (let index = 0; index < values.length; index++) result += values[index]!;
     return result;
   }
   export function byteEnd(view: ArrayBufferView): number {
     return view.byteOffset + view.byteLength;
   }
   export function bufferBytes(buffer: ArrayBufferLike): number {
     return buffer.byteLength;
   }
   export function weakArray(values: number[]): boolean {
     const seen = new WeakSet<readonly number[]>();
     seen.add(values);
     const present = seen.has(values);
     return present && seen.delete(values) && !seen.has(values);
   }
   export function parsePrefix(value: string): number {
     return parseFloat(value);
   }
   export function finite(value: number): boolean {
     return isFinite(value);
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
  '#include <flight/array_buffer_view.hpp>',
  '#include <flight/sequence_view.hpp>',
  '#include <flight/weak_set.hpp>',
  'flight::SequenceView<double>',
  'flight::ArrayBufferLike',
  'flight::ArrayBufferView',
  'flight::WeakSet<flight::Array<double>>',
  'flight::parse_float',
  'std::isfinite',
]) {
  if (!emitted.includes(expected)) {
    process.stderr.write(`Array-like binding fixture did not emit ${expected}.\n`);
    process.exit(1);
  }
}

const temporary = mkdtempSync(path.join(tmpdir(), 'flight-cpp-array-like-'));
try {
  writeFileSync(path.join(temporary, 'array_like.hpp'), emitted);
  writeFileSync(
    path.join(temporary, 'consumer.cpp'),
    `#include "array_like.hpp"

#include <memory>
#include <vector>

int main() {
  flight::Array<double> values{1.0, 2.0, 3.0};
  const auto array_total = flighthq_runtime_test::total(values);
  flight::Uint16Array words(4);
  const auto typed_total = flighthq_runtime_test::total(words);
  const auto custom = std::make_shared<std::vector<double>>(std::initializer_list<double>{4.0, 5.0});
  const auto custom_total = flighthq_runtime_test::total(custom);
  const auto byte_end = flighthq_runtime_test::byte_end(flight::ArrayBufferView(words));
  const auto buffer_bytes = flighthq_runtime_test::buffer_bytes(words.buffer);
  const auto weak_array = flighthq_runtime_test::weak_array(values);
  const auto parsed = flighthq_runtime_test::parse_prefix(flight::String(" -12.5tail"));
  const auto finite = flighthq_runtime_test::finite(parsed);
  return array_total == 6.0 && typed_total == 0.0 && custom_total == 9.0 && byte_end == 8.0 &&
                 buffer_bytes == 8.0 &&
                 weak_array && parsed == -12.5 && finite
             ? 0
             : 1;
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
  `Compiler-emitted portable runtime bindings compile and preserve native behavior (${compiler.commit.slice(0, 7)}, ${cppCompiler}).\n`,
);
