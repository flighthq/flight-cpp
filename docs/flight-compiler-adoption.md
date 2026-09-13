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
| Open structural rows and proxies | Partial | Structural views reuse one `RowOwner` per source object. The generated member table installs one typed, presence-bearing cell for each reachable named field, while computed symbols retain distinct canonical identities. Writable, readonly, partial, and compatible merged projections share those cells. `make_structural_write_proxy` provides distinct identity, forwarding, pre-write interception, exception ordering, nested composition, and projection identity; a live compiler-emitted generic Entity proxy compiles and runs. Construction still needs an all-required-fields check, casts need whole-schema compatibility constraints, and `RowMerge` must validate every collision at schema instantiation rather than when a field is accessed. |
| Conditional capability facets | Implemented | `FacetRef`, lambda-based required `MemberPath`, `RequiredMemberFacet`, `ConditionalFacetRef`, and explicit `assume_conditional_facets` preserve one base reference and statically reject absent or optional paths. A live compiler-emitted generic Tray fixture proves capable, incapable, and optional host paths plus referent identity. |
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
- `npm run structural:oracle` generates the exact generic Entity write proxy through the pinned compiler, compiles
  the emitted headers, and executes an intercepted write against the working runtime.
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

The versioned `flighthq/flight-cpp/headless/1` profile supplies monotonic `performance.now`, `console.debug`, and
single-threaded host-pumped timeout/interval functions. Its selected profile, identity, and digest are recorded in
each profile-specific generated manifest, and a live compiler fixture compiles and runs every binding. SDL's host
loop exposes the same timer pump. Log now reaches the compiler-owned ordered-`Record` spread refusal, and Signals
throttle reaches the dependent-callable-pack refusal; neither remains blocked on its headless ambient names. The
aggregate emitted count is still unchanged. More headless bindings and maintained profiles for Node/tooling,
browser/media, SDL/GL, SDL/Vulkan, and SDL/WebGPU are still required. A profile may name only native types and
lifetimes its host package actually implements; opaque placeholders would make a larger report while leaving the SDK
unusable.
