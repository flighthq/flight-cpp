import { spawnSync } from 'node:child_process';
import { existsSync, mkdtempSync, rmSync, writeFileSync } from 'node:fs';
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
  process.stdout.write('flight-compiler is not rehydrated (RegExp oracle skipped).\n');
  process.exit(0);
}

const cppCompiler = process.env.CXX || 'c++';
if (spawnSync(cppCompiler, ['--version'], { encoding: 'utf8' }).status !== 0) {
  process.stdout.write(`No C++ compiler available as ${cppCompiler} (RegExp oracle skipped).\n`);
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
const source = api.parseTypeScriptSource(
  '/flight/packages/runtime-test/src/regexp.ts',
  `export function substitute(value: string): string {
     return value.replace(/(a)(b)?(z)?/, "$$|$&|$\`|$'|$1|$2|$3|$12");
   }
   export function empty(value: string): string {
     return value.replace(/(?:)/g, '_');
   }
   export function digits(value: string) {
     return value.match(/[0-9]/g);
   }
   export function offsets(value: string): string {
     return value.replace(/[0-9]/g, (_match: string, offset: number) => String(offset));
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
const emitted = api.emitIrModuleCpp(lowered.module, { runtimeProfile: 'flight-cpp' }).contents;
for (const expected of [
  '#include <flight/regexp.hpp>',
  'value.replace(flight::RegExp',
  'value.match(flight::RegExp',
  '[=](flight::String match, double offset)',
]) {
  if (!emitted.includes(expected)) {
    process.stderr.write(`RegExp fixture did not emit ${expected}.\n${emitted}\n`);
    process.exit(1);
  }
}

const temporary = mkdtempSync(path.join(tmpdir(), 'flight-cpp-regexp-'));
try {
  writeFileSync(path.join(temporary, 'regexp.hpp'), emitted);
  writeFileSync(
    path.join(temporary, 'consumer.cpp'),
    `#include "regexp.hpp"

#include <iostream>

int main() {
  flight::JsonArray observations;
  observations.push(flighthq_runtime_test::substitute("abc"));
  observations.push(flighthq_runtime_test::empty("-"));
  flight::JsonArray digits;
  const auto matched = flighthq_runtime_test::digits("a1b2");
  for (const auto& digit : *matched) digits.push(digit);
  observations.push(std::move(digits));
  observations.push(flighthq_runtime_test::offsets(
      flight::String::from_utf8("\\xC3\\xA9" "0x0")));
  std::cout << flight::Json::stringify(observations).to_utf8();
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
    process.stderr.write(`${compilation.stdout}${compilation.stderr}\nEmitted RegExp header:\n${emitted}\n`);
    process.exit(1);
  }
  const native = spawnSync(executable, [], { cwd: root, encoding: 'utf8' });
  const node = spawnSync(
    process.execPath,
    [
      '--input-type=module',
      '--eval',
      `const observations = [
  'abc'.replace(/(a)(b)?(z)?/, "$$|$&|$\\\`|$'|$1|$2|$3|$12"),
  '-'.replace(/(?:)/g, '_'),
  Array.from('a1b2'.match(/[0-9]/g) ?? []),
  'é0x0'.replace(/[0-9]/g, (_match, offset) => String(offset)),
];
process.stdout.write(JSON.stringify(observations));`,
    ],
    { cwd: root, encoding: 'utf8' },
  );
  if (native.status !== 0 || node.status !== 0 || native.stdout !== node.stdout) {
    process.stderr.write(
      `${native.stderr}${node.stderr}RegExp source differential failed.\nExpected: ${node.stdout}\nActual:   ${native.stdout}\n`,
    );
    process.exit(1);
  }
} finally {
  rmSync(temporary, { force: true, recursive: true });
}

process.stdout.write(
  `Compiler-emitted RegExp match and replacement operations match JavaScript (${compiler.commit.slice(0, 7)}, ${cppCompiler}).\n`,
);
