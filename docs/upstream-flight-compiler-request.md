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
- `TextEncoder` scalar UTF-8 and unpaired-surrogate replacement, whose newly admitted SWF helper compiles after
  shared runtime containers gained JavaScript-compatible logical constness;
- numeric conversion and prefix parsing, safe-integer checks, object keys/values, symbols, URL protocol parsing,
  regular expressions, and a deterministic Intl baseline;
- idempotent `Ref<T>` projection, generated structural-row member access, writable entity construction, and
  structural reference casts;
- weak identity maps and sets for Flight references, closed reference variants, and weakly recoverable Flight arrays,
  including erased values and checked typed map views;
- the versioned callable signature/binding ABI used by Signals;
- an explicit JSON value model plus parsing and stringification for every JSON value domain;
- conditional capability facet references with required nested-member checks;
- source-compatible array `length`, resize, insertion splice, and iterable `Array.from` operations, plus variadic
  `Math.min`/`Math.max`;
- typed-array `from` with JavaScript integer conversion after mapping, and `ArrayBuffer.isView` for represented
  buffers and views;
- `RangeError` and `TypeError` classes that accept `flight::String`, preserve the JavaScript name/message surface,
  and share the existing `flight::Error` exception base.

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

Several direct ambient-member refusals now have exact downstream targets and need only compiler election:

- `Number.parseInt` → `flight::parse_int`, `Number.parseFloat` → `flight::parse_float`, and
  `Number.isSafeInteger` → `flight::is_safe_integer` from `flight/number.hpp`;
- `Object.values` → `flight::object_values` from `flight/object.hpp`;
- iterable `Array.from` → `flight::array_from` from `flight/array.hpp`;
- typed-array `from` → the selected concrete alias's static `from`, such as `flight::Uint32Array::from`, from
  `flight/typed_array.hpp`;
- `ArrayBuffer.isView` → `flight::is_array_buffer_view` from `flight/array_buffer_view.hpp` when the argument has a
  represented closed buffer/view domain.

Length-only `Array.from({ length }, mapper)` still requires dedicated compiler lowering for its implicit
`undefined` elements. Open `object` arguments to `ArrayBuffer.isView`, `Object.prototype.hasOwnProperty.call`,
generic structured cloning, and `Promise.allSettled` results still need represented compiler contracts; the runtime
does not provide a permissive erasure for them.

A disposable exact-pin compiler diagnostic added only the seven member families above. It removed all 19 direct
member refusals and expanded the runtime-profile inventory from 1,077 to 1,082 headers. The five added headers all
reach existing compiler-owned representation defects: `bitmap_fingerprint.hpp`, `capture_comparison.hpp`, and
`texture_atlas_page_meta.hpp` apply concrete typed-array aliases as templates, while the two physics ABI buffer
headers refer to `flight::types::SpatialObjectId` with a type spelling the generated dependency does not provide.
The resulting independent compile is 713 passing and 369 failing, so these mappings expose useful work but do not
yet increase the buildable header count.

The official runtime-profile compile has nine headers whose first error is construction of `std::range_error` from
`flight::String`. Map both the type and value spaces for `RangeError` to `flight::RangeError` and `TypeError` to
`flight::TypeError`, with `flight/error.hpp`; the native types now implement that contract. A disposable exact-pin
mapping removes those nine constructor errors, after which the same headers reach existing optional-unwrapping or
missing-symbol emission defects. This mapping is still the correct ABI fix, but it does not raise the present 713
passing-header count by itself.

The runtime now also provides `flight::all_settled_tasks(Array<Task<T>>)` and
`Task<T>::all_settled(std::vector<Task<T>>)`, returning ordered `TaskSettlement<T>` values without rejecting the
aggregate. The compiler can bind `Promise.allSettled` to the free function after it maps
`PromiseSettledResult<T>` to that carrier and lowers the source union's `status`, `value`, and `reason` access. A
disposable exact-pin member mapping already removes the only direct all-settled refusal; the scene-resource module
then reaches its existing `resolveScene3DResources` dependency refusal, so this change does not alter the current
1,077-header total by itself.

## Complete example sweep

`npm run examples:generate:check` now gives flight-compiler a stable downstream example gate. It discovers all 33
packages under Flight's `examples/packages`, inventories all 181 source modules, and selects the 100-module native
SDL/GL lane: each `render.ts` selector and chosen `render.webgl.ts` implementation receive an explicit recorded
`renderNative` source remap, with the DOM-only cross-backend example recording its fallback. Example output has its
own `examples/upstream/generated/include/flight/examples` tree and references the single canonical SDK tree rather
than copying SDK headers per example.

At the current pins, dependency closure emits 0 of those 100 modules. The linked ledger contains 94 dependency, six
lowering, and one emission refusal entries. Forty example modules are held by the refused `@flighthq/sdk` barrel and
20 renderer modules are held by the refused `@flighthq/host-web/contract` path through `webGraphicsHost`. The exact
per-module evidence is committed in `examples/upstream/generated/refusals.json`.

The separate frontier ledger deliberately compiles each selected example without its package dependencies, so its
28 missing package-evaluation entries are boundary markers rather than claims that the full graph omitted those
packages. It exposes the direct example-source work hidden behind propagation: 29 captured referent mutations need
the compiler's shared C++ reference representation, four WebGL renderers need contextual typing for empty arrays,
two array binding patterns need statically recoverable element or tuple types, one spread needs finite/fold
lowering, one nested interface statement needs lowering, and one target-name candidate reaches an internal compiler
failure. The SDL application profile resolves every direct keyboard, pointer, wheel, DOM attachment, window, and
animation-frame ambient in the chosen lane. Only the sound example still reports `AudioContext[value]`; that belongs
with an eventual SDL audio adapter rather than the GL host.

The remaining integration request is a first-class source/package remap in the programmatic graph API. Flight uses
build-time renderer aliases and its examples import Web host providers. `flight-cpp` can name selected native
renderer and host modules, but it should not edit upstream import strings or maintain a second TypeScript transform
to do so. A remap must be importer-specific, recorded in provenance, participate in both type and module-evaluation
resolution, and continue to verify that the replacement exports every requested name. With that contract, the
handwritten SDL package can replace Web lifecycle/input/context providers while generated Flight GL modules retain
renderer ownership.

## Host boundary

The manifest-free generation remains the portable floor. Browser, media, Node, and graphics handles require explicit
binding profiles. SDL owns lifecycle and GL, Vulkan, or WebGPU surface acquisition; generated Flight renderer
packages own rendering behavior. These host bindings will increase the emitted module set, while the portable
1,032-header compile gate remains useful and independent of platform SDKs.

The downstream `flighthq/flight-cpp/sdl-gl/1` profile now names concrete SDL-owned canvas/context types, shared GL
object handles, context attributes, a weakly recoverable image-source carrier, and
`EXT_texture_filter_anisotropic`. Composed with the current runtime profile it emits 1,106 modules; 21 of its 29
newly admitted headers compile. The extension binding removes its direct ambient refusal; `GlContextRuntime` then
reaches `flight-cpp WeakMap value requires a proven C++ representation`. The next renderer-wide compiler blocker is exact:
`packages/types/src/GlContext.ts` retains
`auto viewport;` after its closed `Pick<WebGL2RenderingContext, GlContextMember>` is materialized. The fail-closed
placeholder gate correctly refuses it. The materialized surface also has six C++ target-name collisions because the
current snake-case transform maps `ACTIVE_TEXTURE`/`activeTexture`, `CULL_FACE`/`cullFace`,
`DEPTH_FUNC`/`depthFunc`, `FRONT_FACE`/`frontFace`, `STENCIL_FUNC`/`stencilFunc`, and `VIEWPORT`/`viewport` to the
same six names. The mapped-interface fix must assign distinct stable names before the record can be well formed.
Once all picked constants and methods receive their concrete callable types,
flight-cpp can populate the generated `flight::types::GlContext` record from its working `WebGl2Context`; the host
adapter already forwards shared GL object lifetime and every non-polymorphic command selected by Flight: buffer and
texture upload, compressed texture upload, framebuffer clear/blit/readback, shader/program compilation, render
state, vertex attributes, draw calls, and uniforms. A compiler fixture emits representative calls for that complete
surface directly through the external `WebGL2RenderingContext` binding and compiles them. Its closed `getParameter`
result carrier converts to every numeric, boolean, array, and identity-preserving GL handle domain used by Flight;
`getExtension` supplies feature presence and anisotropy enums. The SDL tween also executes the ordinary texture,
framebuffer, readback, state-query, shader, draw, and presentation paths against an offscreen GLES 3 context. Dynamic
compressed-extension enum lookup still depends on the compiler selecting flight-cpp's ordered
`Record<String, double>` representation and lowering optional indexed access. This proceeds into `render-gl` without
adding an SDL renderer.

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
