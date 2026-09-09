# flight-compiler package graph review

This review covers Flight `1274ec5` and flight-compiler `a7596ab`. The compiler revision is pinned in
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
- `21b206e` lets workspace analysis target an explicit package closure. The downstream generator now asks for the
  154 packages in `@flighthq/sdk`, then consumes the inventory's 308 public export lanes through
  `createCompilerModuleResolutionPlan`; it no longer reconstructs `index` and `contract` lanes itself. Whole-workspace
  analysis still reports the unrelated `@flighthq/tool-registry` exclusion drift, which no longer blocks SDK work.
- `ef8da01` analyzes interface heritage across the module graph, and `12a637b` orders non-exported C++ helpers before
  callers and removes self-referential type re-exports.

The full graph completed in 76 seconds on the current development machine. The previous isolated sweep
emitted 1,322 headers that did not form a dependency closure. The graph report emits 314 dependency-closed modules
and refuses 2,537; that lower headline is more useful because no reported output depends on a refused module. It
now records 2,989 refusal causes: 2,190 propagated dependency failures, 703 lowering diagnostics, and 96 emission
failures. Graph-wide semantic lowering exposes direct failures in modules that older revisions reached only as
dependency refusals, so the direct diagnostic totals are not comparable to the old short-circuiting report.

The `0d3416c` semantic fix removes all 77 prior `FirstTypeNode` diagnostics. The `cc8e210` and `b5c9ee1` interface
changes reduce caught lowering-pass failures from 24 to 7 and allow nine inherited type modules to enter the emitted
closure. The nine new headers still fail against flight-cpp because their generated classes inherit the missing
`flight::ReferenceEnabled` runtime type. The latest graph-aware heritage pass removes all 90 former "requires an
interface reference" diagnostics. Eleven diagnostics in two source modules now reach the narrower unsupported case
where a heritage target cannot be reduced to an object-shaped declaration; the three caught interface-inheritance
failures remain.

The old `assetLibrary.ts` duplicate-binding invariant no longer occurs. Imported identity is now resolved across the
graph, apart from two remaining `indeterminateIdentity` cases and four `unsupportedReferenceForm` cases in
`@flighthq/types`. The earlier `@flighthq/adjustments` probe no longer has its two `FirstTypeNode` blockers; its direct
remaining compiler/runtime blocker is unsupported dense-array length construction. Its other refusals are dependency
propagation from that module or `@flighthq/types/contract`.

## Remaining upstream requests

### Keep the runtime contract executable

ABI alignment is fixed, but the C++ backend emits runtime APIs absent from the exact flight-cpp revision it pins.
Examples include `flight::ReferenceEnabled`, `flight::Ref<T>`, `flight::make_ref`, binding cells, bitwise/shift
helpers, and `flight::power`, `minimum`, and `maximum`. This repository now supplies `flight::power` so its generated
tween remains runnable. A compile probe over the 314 dependency-closed SDK headers passes 110 and fails 204. The
compiler's golden C++ compile probe reports 53 emitted files that do not compile against this runtime, led by the same
runtime-surface mismatch.

Please compile representative `runtimeProfile: "flight-cpp"` output against the pinned flight-cpp checkout as a
compiler gate. ABI number equality cannot catch a missing API surface. Either advance the runtime pin with these
semantics or keep the backend from claiming that runtime profile until its required capabilities exist.

Some emitted constructs fail independently of missing runtime names, including optional defaults that call
`value_or` with the optional itself, invalid `cmath.log` member syntax, and out parameters emitted by value. These
need target compile fixtures and, where observable, TypeScript/native behavior oracles. Helper declaration ordering
is fixed in `12a637b`, although the affected full-SDK headers still fail on other runtime gaps.

### Preserve package graph identity through the remaining type cases

The graph-wide direct refusal families are now clear enough to prioritize. Counts are diagnostic instances; repeated
computed members can produce more than one diagnostic in a module.

| Diagnostics | Modules | Direct lowering family |
| ---: | ---: | --- |
| 585 | 212 | computed property names |
| 40 | 25 | `unique` type operators |
| 28 | 17 | conditional types |
| 11 | 2 | interface heritage target is not object-shaped |
| 10 | 5 | mapped types |

There are seven caught lowering-pass failures, including three remaining interface-inheritance cases. Please retain
graph identity through those cases and finish the two remaining `indeterminateIdentity` and four
`unsupportedReferenceForm` cases.

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
