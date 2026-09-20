import { spawnSync } from 'node:child_process';
import { existsSync, mkdirSync, mkdtempSync, writeFileSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

import { resolveDependency } from './dependencyLock.mjs';

// The plain-object host capability contract, end to end.
//
// A host capability such as `HostGlCapability` is a plain interface of methods, provided by an
// object literal and passed explicitly to a consumer. Nothing about it is an Entity: it has no
// runtime key, and no identity-keyed side table stands behind it. This oracle proves that the
// runtime already carries that contract, by compiling and RUNNING the pinned compiler's own output
// for the shape rather than a hand-written approximation of it:
//
//   * a method-bearing plain interface reached as
//     `StructuralRef<RowReadonly<RowOf<Ref<Capability>>>>`, its members invoked through
//     `row_get<RowKey<"name">>(row)(args...)`;
//   * an OPTIONAL object capability -- `readonly context?: HostGlCapability` -- represented as
//     `std::optional<Ref<Capability>>`, so absence and a present reference stay distinct and no
//     generated code ever compares a reference against `flight::undefined`.
//
// The member table is generated here for exactly the keys this fixture uses, the same way
// `sdkGeneration` writes one for the emitted SDK. The committed table carries only the keys the
// portable inventory reaches, and the capability modules are not in it yet.

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
  process.stdout.write('flight-compiler is not rehydrated (host capability oracle skipped).\n');
  process.exit(0);
}

const cppCompiler = process.env.CXX || 'c++';
if (spawnSync(cppCompiler, ['--version'], { encoding: 'utf8' }).status !== 0) {
  process.stdout.write(`No C++ compiler available as ${cppCompiler} (host capability oracle skipped).\n`);
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
const types = api.parseTypeScriptSource(
  '/flight/packages/types/src/contract.ts',
  `export interface GlSurface { readonly id: number; }
   export interface GlContext { readonly generation: number; }
   export interface HostGlCapability {
     acquire(surface: Readonly<GlSurface>): GlContext | null;
     release(surface: Readonly<GlSurface>): void;
     subscribe(surface: Readonly<GlSurface>, onLost: () => void): () => void;
   }
   export interface HostGlCapabilities { readonly context?: HostGlCapability; }`,
);
const consumer = api.parseTypeScriptSource(
  '/flight/packages/surface/src/attach.ts',
  `import type { GlContext, GlSurface, HostGlCapabilities, HostGlCapability } from '@flighthq/types/contract';
   export function attachWindowRenderContext(
     hostGl: Readonly<HostGlCapability>,
     surface: Readonly<GlSurface>,
   ): GlContext | null {
     const context = hostGl.acquire(surface);
     if (context === null) return null;
     hostGl.release(surface);
     return context;
   }
   export function attachFromGroup(
     capabilities: Readonly<HostGlCapabilities>,
     surface: Readonly<GlSurface>,
   ): GlContext | null {
     const capability = capabilities.context;
     if (capability === undefined) return null;
     return attachWindowRenderContext(capability, surface);
   }`,
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
      { packageName: '@flighthq/surface', sourceFile: consumer, upstreamDirectory: '/flight' },
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
const emittedConsumer = emission.emitModule(modules[1])[0].contents;

// The representation claims this oracle exists to hold the compiler to.
const claims = [
  ['std::optional<flight::Ref<HostGlCapability>> context;', 'an optional object capability is an optional reference'],
  ['flight::row_get<flight::RowKey<"acquire">>', 'a plain interface method is reached through its row key'],
];
for (const [needle, description] of claims) {
  const haystack = `${emittedTypes}\n${emittedConsumer}`;
  if (!haystack.includes(needle)) {
    process.stderr.write(`Pinned compiler no longer emits ${description} (${needle}).\n`);
    process.exit(1);
  }
}
if (/flight::undefined/u.test(emittedConsumer)) {
  process.stderr.write('Emitted capability code compares a reference against flight::undefined.\n');
  process.exit(1);
}

const memberKeys = ['acquire', 'release', 'subscribe', 'context', 'id', 'generation'];
const temporary = mkdtempSync(path.join(tmpdir(), 'flight-cpp-host-capability-'));
try {
  writeFileSync(path.join(temporary, 'contract.hpp'), emittedTypes);
  writeFileSync(path.join(temporary, 'attach.hpp'), emittedConsumer);
  mkdirSync(path.join(temporary, 'flight', 'sdk'), { recursive: true });
  writeFileSync(path.join(temporary, 'flight', 'sdk', 'structural_members.hpp'), memberTable(memberKeys));
  writeFileSync(path.join(temporary, 'consumer.cpp'), fixture());

  const executable = path.join(temporary, process.platform === 'win32' ? 'oracle.exe' : 'oracle');
  const compilation = spawnSync(
    cppCompiler,
    [
      '-std=c++20',
      '-pthread',
      '-Wall',
      '-Wextra',
      '-Werror',
      '-I',
      temporary,
      '-I',
      path.join(root, 'include'),
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
    process.stderr.write('The compiler-emitted host capability fixture did not behave as specified.\n');
    process.exit(1);
  }
} finally {
  rmSync(temporary, { force: true, recursive: true });
}

process.stdout.write(
  `Compiler-emitted plain host capabilities pass and invoke across the row seam (${compiler.commit.slice(0, 7)}, ${cppCompiler}).\n`,
);

function memberTable(keys) {
  const member = (name) => name.replace(/([a-z0-9])([A-Z])/gu, '$1_$2').toLowerCase();
  const cases = keys.map(
    (name, index) =>
      `  ${index === 0 ? 'if' : 'else if'} constexpr (Key::name.view() == std::string_view("${name}") && requires { object.${member(name)}; }) return (object.${member(name)});`,
  );
  const typeCases = keys.map(
    (name, index) =>
      `  ${index === 0 ? 'if' : 'else if'} constexpr (Key::name.view() == std::string_view("${name}") && requires(Object& object) { object.${member(name)}; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().${member(name)})>>{};`,
  );
  const bindings = keys.map(
    (name) =>
      `  if constexpr (requires { object->${member(name)}; }) owner.bind_named("${name}", [object]() -> decltype(auto) { return (object->${member(name)}); });`,
  );
  // A member table has to answer the widening proof too: flight/structural_ref.hpp defines its own
  // "nothing is provable" fallback only when no table is present at all, so a table that omits this
  // leaves the name undeclared. sdkGeneration emits the real one, key by key; this fixture proves no
  // widening, which is the safe answer and all it needs.
  const widening =
    'template <typename Base, typename Derived>\nconsteval bool generated_row_widening_proven() {\n  return false;\n}\n';
  return `#pragma once\n\n#include <flight/structural_ref.hpp>\n\n#include <memory>\n#include <string_view>\n#include <type_traits>\n#include <utility>\n\nnamespace flight::detail {\n\n${widening}\n\ntemplate <typename Key, typename Object>\ndecltype(auto) generated_row_member(Object& object) {\n${cases.join('\n')}\n  else static_assert(dependent_false<Key>, "no generated member");\n}\n\ntemplate <typename Key, typename Object>\nconsteval auto generated_row_member_type_identity() {\n${typeCases.join('\n')}\n  else return std::type_identity<void>{};\n}\n\ntemplate <typename Key, typename Object>\nusing generated_row_member_t = typename decltype(generated_row_member_type_identity<Key, Object>())::type;\n\ntemplate <typename Object>\nvoid bind_generated_row_members(RowOwner& owner, const std::shared_ptr<Object>& object) {\n${bindings.join('\n')}\n}\n\n} // namespace flight::detail\n`;
}

function fixture() {
  return `#include "attach.hpp"

#include <iostream>

namespace {

int failures = 0;

void check(bool condition, const char* message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\\n';
  ++failures;
}

// Asked as a template so the answer is a constraint rather than a hard error on a known type.
template <typename Object>
concept has_entity_runtime_slot = requires(Object& object) { object.entity_runtime_key; };

} // namespace

int main() {
  using Capability = flighthq_types::HostGlCapability;
  using CapabilityRow = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<Capability>>>>;
  using GroupRow = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flighthq_types::HostGlCapabilities>>>>;
  using SurfaceRow = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flighthq_types::GlSurface>>>>;

  auto surface = flight::make_ref<flighthq_types::GlSurface>(flighthq_types::GlSurface{.id = 3.0});
  const SurfaceRow surface_row(surface);

  // A PLAIN capability object: an object literal with method members, exactly as webHostGl builds
  // one. No entity runtime key is written, and nothing is registered in a side table.
  int acquire_calls = 0;
  int release_calls = 0;
  int subscribe_calls = 0;
  auto context = flight::make_ref<flighthq_types::GlContext>(flighthq_types::GlContext{.generation = 7.0});
  auto capability = flight::make_ref<Capability>();
  capability->acquire = [&](SurfaceRow seen) -> std::optional<flight::Ref<flighthq_types::GlContext>> {
    ++acquire_calls;
    return flight::row_get<flight::RowKey<"id">>(seen) == 3.0
               ? std::optional<flight::Ref<flighthq_types::GlContext>>(context)
               : std::nullopt;
  };
  capability->release = [&](SurfaceRow) { ++release_calls; };
  capability->subscribe = [&](SurfaceRow, std::function<void()> on_lost) {
    ++subscribe_calls;
    return std::function<void()>([on_lost] { on_lost(); });
  };

  // Passed EXPLICITLY to the consumer, which reaches both methods through the row seam.
  const CapabilityRow capability_row(capability);
  const auto attached = flighthq_surface::attach_window_render_context(capability_row, surface_row);
  check(attached.has_value() && attached.value() == context,
        "a plain capability object crosses the row seam and returns the host's own context");
  check(acquire_calls == 1 && release_calls == 1,
        "both capability methods were invoked through the row, not around it");

  bool lost = false;
  const auto unsubscribe = flight::row_get<flight::RowKey<"subscribe">>(capability_row)(
      surface_row, std::function<void()>([&] { lost = true; }));
  unsubscribe();
  check(subscribe_calls == 1 && lost,
        "a capability method that returns a callable round-trips through the row");

  // PRESENT in an optional capability group.
  auto present_group = flight::make_ref<flighthq_types::HostGlCapabilities>();
  present_group->context = capability;
  const GroupRow present_row(present_group);
  const auto from_present = flighthq_surface::attach_from_group(present_row, surface_row);
  check(from_present.has_value() && from_present.value() == context,
        "a capability stored in an optional group is reached and invoked");
  check(acquire_calls == 2 && release_calls == 2, "the grouped capability ran the same two methods");

  // ABSENT: the group holds no capability, which is distinct from holding one.
  auto absent_group = flight::make_ref<flighthq_types::HostGlCapabilities>();
  const GroupRow absent_row(absent_group);
  const auto from_absent = flighthq_surface::attach_from_group(absent_row, surface_row);
  check(!from_absent.has_value(), "an absent optional capability yields no context");
  check(acquire_calls == 2, "an absent capability is not invoked");

  const auto slot = flight::row_get<flight::RowKey<"context">>(present_row);
  const auto empty_slot = flight::row_get<flight::RowKey<"context">>(absent_row);
  check(slot.has_value() && !empty_slot.has_value(),
        "presence and absence of an object capability are distinct states of one optional");

  // The capability gained no identity of its own along the way: no entity runtime slot on the
  // type, and the row reaches the very object the provider built rather than a stand-in for it.
  static_assert(!has_entity_runtime_slot<Capability>,
                "a plain host capability declares no entity runtime slot");
  check(capability_row.shared_object() == capability && present_row.shared_object() == present_group,
        "the rows reach the provider's own objects, so nothing was copied or re-identified");

  return failures == 0 ? 0 : 1;
}
`;
}
