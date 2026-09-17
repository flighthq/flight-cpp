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
  process.stdout.write('flight-compiler is not rehydrated (Boolean oracle skipped).\n');
  process.exit(0);
}

const cppCompiler = process.env.CXX || 'c++';
if (spawnSync(cppCompiler, ['--version'], { encoding: 'utf8' }).status !== 0) {
  process.stdout.write(`No C++ compiler available as ${cppCompiler} (Boolean oracle skipped).\n`);
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
  '/flight/packages/host-test/src/boolean.ts',
  `export function truthiness(
     text: string,
     number: number,
     nullable: string | null,
     values: string[],
   ): boolean[] {
     return [Boolean(text), Boolean(number), Boolean(nullable), Boolean(values)];
   }
   export function compact(value: string): string[] {
     return value.split(',').filter(Boolean);
   }
   export function negated(value: string | null | undefined): boolean {
     return !value;
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
for (const expected of [
  '#include <flight/boolean.hpp>',
  'flight::to_boolean(text)',
  'flight::to_boolean(nullable)',
  '.filter(flight::to_boolean)',
  // `value` is a union here, and a std::variant has no operator!, so negation has to go through the
  // truthiness conversion rather than through the variant itself.
  '!flight::to_boolean(value)',
]) {
  if (!emitted.includes(expected)) {
    process.stderr.write(`Boolean fixture did not emit ${expected}.\n${emitted}\n`);
    process.exit(1);
  }
}

const temporary = mkdtempSync(path.join(tmpdir(), 'flight-cpp-boolean-'));
try {
  writeFileSync(path.join(temporary, 'boolean.hpp'), emitted);
  writeFileSync(
    path.join(temporary, 'consumer.cpp'),
    `#include "boolean.hpp"

int main() {
  const auto values = flighthq_host_test::truthiness(
      flight::String(), 0.0, std::nullopt, flight::Array<flight::String>{});
  const auto compact = flighthq_host_test::compact(flight::String(",flight,,cpp"));
  using NullableString = std::variant<flight::String, flight::Null, flight::Undefined>;
  return values.size() == 4 && !values[0] && !values[1] && !values[2] && values[3] &&
                 compact.size() == 2 && compact[0] == flight::String("flight") &&
                 compact[1] == flight::String("cpp") &&
                 flighthq_host_test::negated(NullableString{flight::null}) &&
                 flighthq_host_test::negated(NullableString{flight::String()}) &&
                 !flighthq_host_test::negated(NullableString{flight::String("flight")})
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
    process.stderr.write(`${compilation.stdout}${compilation.stderr}\nEmitted Boolean header:\n${emitted}\n`);
    process.exit(1);
  }
  const native = spawnSync(executable, [], { cwd: root, encoding: 'utf8' });
  if (native.status !== 0) {
    process.stderr.write(
      `${native.stdout}${native.stderr}Compiler-emitted Boolean fixture failed with status ${String(native.status)}.\n`,
    );
    process.exit(1);
  }
} finally {
  rmSync(temporary, { force: true, recursive: true });
}

process.stdout.write(`Boolean binding preserves represented JavaScript truthiness (${compiler.commit.slice(0, 7)}, ${cppCompiler}).\n`);
