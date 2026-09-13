# flight-compiler package graph review

The maintained downstream checklist now lives in [flight-compiler adoption status](flight-compiler-adoption.md).

This review covers Flight `1274ec5` and flight-compiler `5649642`. Both revisions are pinned in
[`dependencies.lock.json`](../dependencies.lock.json).

## Complete report sweep

The full graph now finishes locally in about two and a half minutes. It processes all 154 SDK packages and 2,851
source modules, emits 1,032 dependency-closed headers, and records 1,819 refused modules. The exact headers,
initialization order, package totals, and refusal diagnostics are committed under [`generated/`](../generated/).
`npm run sdk:check` reproduces the tree from the two pins.

The emitted inventory is exposed in the build tree as `Flight::SdkPreview` and as Bazel `//:sdk_preview`. The preview
contains every emitted header but deliberately is not installed as `Flight::Sdk`: independent native compilation is
still red. Run `npm run sdk:compile` to compile every compiler-emitted header and write the full toolchain-specific
diagnostic report to `out/sdk-header-compilation.json`.

## Runtime work adopted downstream

flight-cpp now supplies all runtime headers referenced by the emitted inventory:

- stable shared `ArrayBuffer`, typed-array views, complete numeric `DataView` access, UTF-8 `TextDecoder`, and
  `String::from_code_point`;
- a common `ArrayBufferLike` carrier for ordinary, shared, and host-owned storage, now selected by the runtime binding
  profile and accepted by typed-array/DataView zero-copy constructors;
- shared `AbortController`/`AbortSignal` state with first-reason retention, listener dispatch/removal, and
  `throw_if_aborted`, covered by a live compiler-versus-Node oracle;
- immutable shared `Blob` bytes with typed-array construction, slicing, MIME normalization, text decoding, and
  `ArrayBuffer` conversion, selected through the runtime profile and covered by a live compiler-versus-Node oracle;
- URI component and browser base64 globals with UTF-8, binary-string, malformed-input, padding, and whitespace
  semantics covered by live compiler-versus-Node oracles;
- shared readable/writable stream and async-iterable carriers, including compiler-emitted writer operations and
  native task callbacks; these admit three more independently compiling SDK headers;
- numeric conversion and prefix parsing, safe-integer checks, object keys/values, symbols, URL protocol parsing,
  regular expressions, and a deterministic Intl baseline;
- idempotent `Ref<T>` projection, generated structural-row member access, writable entity construction, and
  structural reference casts;
- weak identity maps and sets for Flight references, closed reference variants, and weakly recoverable Flight arrays,
  including erased values and checked typed map views;
- the versioned callable signature/binding ABI used by Signals;
- an explicit JSON value model plus parsing and stringification for every JSON value domain;
- conditional capability facet references with required nested-member checks;
- source-compatible array `length`, resize, and insertion splice operations, plus variadic `Math.min`/`Math.max`.

The native suite covers the new ownership and observable behavior. A generated SDK executable links
`Flight::SdkPreview`, calls the emitted interpolation and Entity construction functions, runs under CMake's
development preset, and is declared in the Bazel graph.

## Remaining compiler-owned native failures

The emitted set is dependency-closed in the TypeScript package graph, but dependency closure is not yet the same as
C++ well-formedness. With GCC 15.2, 704 of the 1,032 headers compile independently and 328 fail. The current native
report is dominated by emission defects that cannot be repaired by adding a runtime symbol:

- source module-private helpers are emitted into one package namespace, so package barrels encounter C++
  redefinitions;
- several ambient aliases are applied as templates even though their emitted C++ target is a concrete type;
- some imported types are referenced by snake-case value spelling instead of their emitted PascalCase type name;
- optional values are passed or assigned where their contained value is required;
- discriminated unions represented by `std::variant` still receive direct member access;
- several structurally equivalent anonymous records are emitted as distinct, non-convertible C++ structs;
- a few emitted tokens and type queries remain malformed, including `typeidel`.

These diagnostics arise after the runtime includes resolve, and many occur in a header before later errors in that
header can be observed. The JSON report from `npm run sdk:compile` is the compact handoff surface for fixing them in
flight-compiler. Downstream source rewriting would obscure compiler provenance and produce a second, unversioned
transpiler, so the checked-in SDK remains the compiler's exact output plus its generated build/member inventories.

## Host boundary

The manifest-free generation remains the portable floor. Browser, media, Node, and graphics handles require explicit
binding profiles. SDL owns lifecycle and GL, Vulkan, or WebGPU surface acquisition; generated Flight renderer
packages own rendering behavior. These host bindings will increase the emitted module set, while the portable
1,032-header compile gate remains useful and independent of platform SDKs.

The downstream `flighthq/flight-cpp/sdl-gl/1` profile now names concrete SDL-owned canvas/context types, shared GL
object handles, context attributes, and a weakly recoverable image-source carrier. Composed with the current runtime
profile it emits 1,105 modules; 21 of its 29 newly admitted headers compile. The next compiler blocker is exact:
`packages/types/src/GlContext.ts` retains
`auto viewport;` after its closed `Pick<WebGL2RenderingContext, GlContextMember>` is materialized. The fail-closed
placeholder gate correctly refuses it. Once all picked constants and methods receive their concrete callable types,
flight-cpp can populate the generated `flight::types::GlContext` record from its working `WebGl2Context` and proceed
into `render-gl` without adding an SDL renderer.

Regenerate and validate from the repository root:

```sh
npm run rehydrate
npm ci --prefix .dependencies/flight-compiler
npm run sdk:generate
npm run sdk:check
npm run sdk:compile
cmake --preset development
cmake --build --preset development
ctest --preset development
```
