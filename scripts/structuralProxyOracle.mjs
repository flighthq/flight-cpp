import { spawnSync } from 'node:child_process';
import { existsSync, mkdirSync, mkdtempSync, rmSync, writeFileSync } from 'node:fs';
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
  process.stdout.write('flight-compiler is not rehydrated (structural proxy oracle skipped).\n');
  process.exit(0);
}

const cppCompiler = process.env.CXX || 'c++';
if (spawnSync(cppCompiler, ['--version'], { encoding: 'utf8' }).status !== 0) {
  process.stdout.write(`No C++ compiler available as ${cppCompiler} (structural proxy oracle skipped).\n`);
  process.exit(0);
}

if (!existsSync(compilerEntry)) {
  const build = spawnSync('npm', ['run', 'build', '--silent'], {
    cwd: compiler.directory,
    encoding: 'utf8',
  });
  if (build.status !== 0) {
    process.stderr.write(`${build.stdout}${build.stderr}`);
    process.exit(1);
  }
}

const api = await import(pathToFileURL(compilerEntry));
const types = api.parseTypeScriptSource(
  '/flight/packages/types/src/contract.ts',
  `export interface Entity { [EntityRuntimeKey]: EntityRuntime | undefined; }
   export interface EntityRuntime { binding: object | null; }
   export type EntityRuntimeWriteGuard = (slot: 'runtime-slot') => void;
   export const EntityRuntimeKey = Symbol.for('EntityRuntime');`,
);
const guards = api.parseTypeScriptSource(
  '/flight/packages/entity/src/guards.ts',
  `import type { Entity, EntityRuntimeWriteGuard } from '@flighthq/types/contract';
   import { EntityRuntimeKey } from '@flighthq/types/contract';
   export function createGuardedEntity<Type extends object>(entity: Type & Entity): Type & Entity {
     if (!_guardsEnabled || typeof Proxy === 'undefined') return entity;
     return new Proxy(entity, {
       set(target, prop, value) {
         if (prop === EntityRuntimeKey && _guardsEnabled) {
           _writeGuard?.('runtime-slot');
         }
         (target as unknown as Record<PropertyKey, unknown>)[prop] = value;
         return true;
       },
     });
   }
   let _guardsEnabled = false;
   let _writeGuard: EntityRuntimeWriteGuard | null = null;`,
);
const moduleResolution = {
  edges: [
    {
      specifier: '@flighthq/types/contract',
      target: { packageName: '@flighthq/types', source: 'packages/types/src/contract.ts' },
    },
  ],
  schema: 'flight-compiler-module-resolution/1',
};
const modules = api
  .lowerTypeScriptSources(
    [
      { packageName: '@flighthq/types', sourceFile: types, upstreamDirectory: '/flight' },
      { packageName: '@flighthq/entity', sourceFile: guards, upstreamDirectory: '/flight' },
    ],
    moduleResolution,
  )
  .map((result) => result.module);
const emission = api.createCppCompilerBackend().createEmissionSession({
  moduleResolution,
  modules,
  options: { runtimeProfile: 'flight-cpp' },
});
const emittedTypes = emission.emitModule(modules[0])[0].contents;
const emittedGuards = emission.emitModule(modules[1])[0].contents;
if (!emittedGuards.includes('flight::make_structural_write_proxy<')) {
  process.stderr.write('Pinned compiler did not emit the structural write proxy ABI.\n');
  process.exit(1);
}

const temporary = mkdtempSync(path.join(tmpdir(), 'flight-cpp-structural-proxy-'));
try {
  writeFileSync(path.join(temporary, 'contract.hpp'), emittedTypes);
  writeFileSync(path.join(temporary, 'guards.hpp'), emittedGuards);
  writeFileSync(
    path.join(temporary, 'consumer.cpp'),
    `#include "guards.hpp"

struct Extra : public flight::ReferenceEnabled {};

int main() {
  using ExtraRef = flight::Ref<Extra>;
  using Schema = flight::RowMerge<
      flight::RowOf<ExtraRef>,
      flight::RowOf<flight::Ref<flighthq_types::Entity>>>;
  auto target = flight::StructuralRef<Schema>::from_owner(std::make_shared<flight::RowOwner>());
  int calls = 0;
  flighthq_entity::guards_enabled = true;
  flighthq_entity::write_guard = [&](flight::String slot) {
    if (slot == flight::String("runtime-slot")) ++calls;
  };
  auto guarded = flighthq_entity::create_guarded_entity<ExtraRef>(target);
  auto runtime = flight::make_ref<flighthq_types::EntityRuntime>();
  const auto value = std::optional<flight::Ref<flighthq_types::EntityRuntime>>(runtime);
  flight::row_set(guarded, flighthq_types::entity_runtime_key, value);
  const auto observed = flight::row_get<
      std::optional<flight::Ref<flighthq_types::EntityRuntime>>>(
      target, flighthq_types::entity_runtime_key);
  return guarded != target && calls == 1 && observed.has_value() && observed.value() == runtime ? 0 : 1;
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
  `Compiler-emitted structural proxy compiles and preserves write behavior (${compiler.commit.slice(0, 7)}, ${cppCompiler}).\n`,
);
