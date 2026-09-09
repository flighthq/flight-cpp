# flight-compiler package graph review

This review covers Flight `1274ec5` and flight-compiler `2faa5a7`. The compiler revision is pinned in
[`dependencies.lock.json`](../dependencies.lock.json), and the committed [SDK manifest](../generated/manifest.json),
[initialization plan](../generated/initialization.json), and [refusal ledger](../generated/refusals.json) are generated
from that pair.

## What the new compiler work unlocked

The three integration blockers reported against `8a4b1a0` now have real public contracts:

- `b53c171` aligns emitted C++ with the pinned ABI. Generated headers assert `cpp_abi == 1`, matching flight-cpp
  `c437f20`.
- `6f12141` accepts separate package namespace and installed-include mappings plus a versioned
  `flight-cpp-external-bindings/1` manifest. The SDK now emits directly into `flight::<package>` namespaces and
  `flight/<package>/` include paths without rewriting generated text.
- `b07db2c` adds `compileTypeScriptPackageGraph`. One request now carries all 2,851 SDK modules, 154 package roots,
  package dependencies, and public export resolution. It shares C++ reference analysis, records dependency-closed
  partial output, and returns a module-evaluation plan.

The full graph completes in about three minutes on the current development machine. The previous isolated sweep
emitted 1,322 headers that did not form a dependency closure. The graph report emits 305 dependency-closed modules
and refuses 2,546; that lower headline is more useful because no reported output depends on a refused module. It
records 2,726 refusal causes: 2,310 propagated dependency failures, 312 lowering diagnostics, and 104 emission
failures.

The old `assetLibrary.ts` duplicate-binding invariant no longer occurs. Imported identity is now resolved across the
graph, apart from two remaining `indeterminateIdentity` cases and four `unsupportedReferenceForm` cases in
`@flighthq/types`. The earlier `@flighthq/adjustments` probe now exposes three direct compiler/runtime blockers instead
of isolated-import noise: two `FirstTypeNode` diagnostics and unsupported dense-array length construction. Its other
refusals are dependency propagation from those modules or `@flighthq/types/contract`.

## Remaining upstream requests

### Keep the runtime contract executable

ABI alignment is fixed, but the C++ backend emits runtime APIs absent from the exact flight-cpp revision it pins.
Examples include `flight::ReferenceEnabled`, `flight::Ref<T>`, `flight::make_ref`, binding cells, bitwise/shift
helpers, and `flight::power`, `minimum`, and `maximum`. This repository now supplies `flight::power` so its generated
tween remains runnable. A compile probe over the 305 dependency-closed SDK headers passes 110 and fails 195. The
compiler's golden C++ compile probe fails for the same runtime-surface mismatch.

Please compile representative `runtimeProfile: "flight-cpp"` output against the pinned flight-cpp checkout as a
compiler gate. ABI number equality cannot catch a missing API surface. Either advance the runtime pin with these
semantics or keep the backend from claiming that runtime profile until its required capabilities exist.

Some emitted constructs fail independently of missing runtime names, including source-order calls without a prior
declaration, optional defaults that call `value_or` with the optional itself, invalid `cmath.log` member syntax, and
out parameters emitted by value. These need target compile fixtures and, where observable, TypeScript/native behavior
oracles.

### Preserve package graph identity through the remaining type cases

The direct refusal families are now clear enough to prioritize:

| Count | Direct lowering family |
| ---: | --- |
| 90 | interface heritage requires an interface reference |
| 77 | `FirstTypeNode` |
| 43 | computed property names |
| 37 | `unique` type operators |
| 28 | conditional types |
| 11 | mapped types |

There are also 24 caught lowering-pass failures, mostly interface inheritance unable to find local or imported base
interfaces. Please retain graph identity through interface-inheritance lowering and finish the two remaining
`indeterminateIdentity` and four `unsupportedReferenceForm` cases.

### Separate runtime ambient gaps from native host bindings

The external-binding manifest is the correct boundary for actual host and graphics types such as WebGL objects,
WebGPU objects, canvas image sources, and DOM-backed surfaces. flight-cpp or the compiler's standard-library plan
should own JavaScript built-ins such as `Number`, `Record`, `ArrayBuffer`, `DataView`, `RangeError`, `TextEncoder`,
and `TextDecoder`. TypeScript syntax such as `const` type parameters should be diagnosed as syntax/lowering rather
than reported as a missing ambient `const[type]` binding.

The SDL host will provide a downstream manifest only for types it can implement truthfully. It will not map browser
types to unrelated SDL handles merely to increase emission coverage.

### Emit the planned module initialization

`flight-compiler-module-evaluation/1` now gives downstream tooling deterministic dependency groups, live-binding
facts, and declaration initialization steps. This is enough to inspect and commit the plan, but it does not yet emit
the C++ translation units or initialization entry point needed to build executable top-level module behavior.

Please add C++ source/translation-unit emission once executable top-level statements exist in neutral IR. The report
should continue to own dependency order and one-definition-safe initialization.

## Ownership boundary

flight-compiler owns module graph semantics, lowering, target names, external-binding contracts, output layout, and
refusal reports. flight-cpp owns the semantic runtime and CMake packaging. `Flight::HostSdl`, `Flight::HostSdlGl`,
`Flight::HostSdlVulkan`, and `Flight::HostSdlWgpu` own SDL lifecycle and native context/surface acquisition. Generated
Flight renderer packages continue to own rendering.
