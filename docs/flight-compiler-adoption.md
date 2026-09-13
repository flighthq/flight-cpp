# flight-compiler adoption status

This is flight-cpp's maintained view of the downstream work requested by
`flight-compiler/agents/flight-cpp-adoption.md`. A header or symbol being present is not enough to call a capability
adopted. Adoption also requires build packaging, compiler-emitted compilation, source differential behavior where
observable, and a complete regenerated SDK closure.

The current pins are Flight `1274ec5` and flight-compiler `5649642`. The portable sweep processes all 154 packages
and 2,851 modules. It emits 1,032 dependency-closed headers and records 1,819 refusals. All emitted includes resolve;
704 headers compile independently with GCC 15.2 and 328 stop at compiler-emitted C++ errors.

## Downstream implementation

| Contract | State | Evidence and remaining work |
| --- | --- | --- |
| ArrayBuffer, typed arrays, DataView, TextDecoder, `String.fromCodePoint`, insertion `splice` | Implemented | Native tests cover shared backing, views, byte order, numeric edge cases, UTF-8 replacement, astral scalars, and insertion order. The runtime source differential exercises these behaviors against Node. |
| Number conversion, RegExp, URL | Implemented baseline | The emitted APIs compile and the source differential covers the Flight paths in the current SDK. RegExp remains a documented ECMAScript subset rather than a claim that `std::regex` implements every JavaScript expression. |
| Object helpers | Implemented for represented map records | `object_keys`, `object_entries`, and identity-preserving `object_assign` cover the compiler's current represented map inputs. The ordered `Record<K,V>` ABI remains open. |
| JSON | Partial | `JsonValue` preserves null, boolean, number, string, array, and object domains; `Json::parse` and `Json::stringify` round-trip them. Generated ordinary structs still need compiler-provided member reflection, and replacer semantics remain open. |
| Intl | Partial | The locale-neutral baseline and emitted names exist and are source-compared for deterministic English cases. Locale selection, option validation, and a pinned ICU-style provider remain open. |
| Callable signature ABI | Partial | Exact argument packs, signature metadata, binding, and current emitted Signals headers compile. Optional/rest role metadata, callable wrappers, and their complete oracle matrix remain open. |
| Symbol and closed Entity construction | Implemented for current output | Symbol interning, named structural access, the Entity runtime symbol slot, and emitted AmbientLight construction compile and run. |
| Open structural rows and proxies | Partial | Current emitted `RowOf`, readonly/writable projection, construction, and generated member access work. A layout-independent shared `RowOwner`, `RowMerge` validation, presence cells, arbitrary open fields, and `make_structural_write_proxy` remain open and must not be inferred from native member offsets. |
| Conditional capability facets | Implemented runtime ABI | `FacetRef`, lambda-based required `MemberPath`, `RequiredMemberFacet`, `ConditionalFacetRef`, and explicit `assume_conditional_facets` preserve one base reference and statically reject absent or optional paths. An emitted/native differential fixture is still needed before release adoption. |
| WeakMap and erased typed views | Implemented runtime ABI | Default Flight-reference and closed-variant policies are weak and identity-based. External policies use the specified weaken/lock/identity/hash/equal contract. Tests cover expiry, shared views, wrong tags, overwrite, and deletion. Host weak-key policies remain host-profile work. |
| SDL host mechanics | Implemented | SDL lifecycle, events, monotonic clock, windows, GL contexts, Vulkan surfaces, and callback-owned WebGPU surfaces are packaged by CMake. Generated renderer bindings remain open. |

## Integration and release gates

- The exact generated inventory, refusal ledger, initialization plan, CMake header list, Bazel target, and structural
  member table are committed under `generated/` and reproduced by `npm run sdk:check`.
- CMake `Flight::SdkPreview` and Bazel `//:sdk_preview` expose every emitted header without claiming a finished SDK.
  The preview is intentionally not installed.
- `npm run sdk:compile` compiles every emitted header independently and writes the compiler-facing report to
  `out/sdk-header-compilation.json`.
- `npm run runtime:oracle` executes TypeScript-valid source behavior under Node and compares it with the native
  runtime. It currently covers 18 cross-runtime observations.
- `Flight::Sdk` remains blocked until every emitted header in the selected binding profile compiles. At that point it
  must be installed/exported through CMake and exposed through Bazel, then exercised as an installed consumer.
- The reciprocal lock cannot be completed solely in this checkout: after these commits land, flight-compiler must pin
  the landed flight-cpp revision and run its exact-pin emitted compile. The unreleased C++ ABI number also requires a
  coordinated decision before either repository advertises a release ABI.

## Compiler and host work still gating the SDK

The current 328 native header failures begin with compiler-emitted optional/value conversions, concrete typed-array
aliases used as templates, value spelling used where a type name is required, invalid union member access,
non-convertible duplicate anonymous records, package-scope helper collisions, and malformed type queries. These must
be corrected in flight-compiler rather than rewritten in the generated tree.

The versioned `flighthq/flight-cpp/headless/1` binding profile currently supplies the monotonic `performance.now`
ambient and records its selected profile, identity, and digest in each profile-specific generated manifest. It
removes the direct performance refusal from input and log, but those modules remain refused on other headless or
browser symbols and the emitted count therefore remains unchanged. More headless bindings and maintained profiles
for Node/tooling, browser/media, SDL/GL, SDL/Vulkan, and SDL/WebGPU are still required. A profile may name only native
types and lifetimes its host package actually implements; opaque placeholders would make a larger report while
leaving the SDK unusable.
