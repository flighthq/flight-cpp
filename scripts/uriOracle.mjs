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
  process.stdout.write('flight-compiler is not rehydrated (URI oracle skipped).\n');
  process.exit(0);
}

const cppCompiler = process.env.CXX || 'c++';
if (spawnSync(cppCompiler, ['--version'], { encoding: 'utf8' }).status !== 0) {
  process.stdout.write(`No C++ compiler available as ${cppCompiler} (URI oracle skipped).\n`);
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
  '/flight/packages/runtime-test/src/uri.ts',
  `export function encodeValue(value: string): string {
     return encodeURIComponent(value);
   }
   export function decodeValue(value: string): string {
     return decodeURIComponent(value);
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
  '#include <flight/uri.hpp>',
  'flight::encode_uri_component(value)',
  'flight::decode_uri_component(value)',
]) {
  if (!emitted.includes(expected)) {
    process.stderr.write(`URI binding fixture did not emit ${expected}.\n${emitted}\n`);
    process.exit(1);
  }
}

const temporary = mkdtempSync(path.join(tmpdir(), 'flight-cpp-uri-'));
try {
  writeFileSync(path.join(temporary, 'uri.hpp'), emitted);
  writeFileSync(
    path.join(temporary, 'consumer.cpp'),
    `#include "uri.hpp"

#include <iostream>

int main() {
  const auto input = flight::String::from_utf8("flight /?=&# \\xF0\\x9F\\x98\\x80");
  const auto encoded = flighthq_runtime_test::encode_value(input);
  bool malformed = false;
  try {
    static_cast<void>(flighthq_runtime_test::decode_value("%E0%A4%A"));
  } catch (const flight::UriError&) {
    malformed = true;
  }
  std::cout << encoded.to_utf8() << '|' <<
      flighthq_runtime_test::decode_value(encoded).to_utf8() << '|' << std::boolalpha << malformed;
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
    process.stderr.write(`${compilation.stdout}${compilation.stderr}\nEmitted URI header:\n${emitted}\n`);
    process.exit(1);
  }
  const native = spawnSync(executable, [], { cwd: root, encoding: 'utf8' });
  const node = spawnSync(
    process.execPath,
    [
      '--input-type=module',
      '--eval',
      `const input = 'flight /?=&# 😀';
const encoded = encodeURIComponent(input);
let malformed = false;
try { decodeURIComponent('%E0%A4%A'); } catch { malformed = true; }
process.stdout.write(encoded + '|' + decodeURIComponent(encoded) + '|' + malformed);`,
    ],
    { cwd: root, encoding: 'utf8' },
  );
  if (native.status !== 0 || node.status !== 0 || native.stdout !== node.stdout) {
    process.stderr.write(
      `${native.stderr}${node.stderr}URI source differential failed.\nExpected: ${node.stdout}\nActual:   ${native.stdout}\n`,
    );
    process.exit(1);
  }
} finally {
  rmSync(temporary, { force: true, recursive: true });
}

process.stdout.write(
  `Compiler-emitted URI component operations match native behavior (${compiler.commit.slice(0, 7)}, ${cppCompiler}).\n`,
);
