# flight-compiler package graph review

The maintained downstream checklist now lives in [flight-compiler adoption status](flight-compiler-adoption.md).

This review covers Flight `1274ec5` and flight-compiler `993c280`. Both revisions are pinned in
[`dependencies.lock.json`](../dependencies.lock.json).

## Complete report sweep

The full graph now finishes locally in about two and a half minutes. It processes all 154 SDK packages and 2,851
source modules, emits 959 dependency-closed headers, and records 1,892 refused modules: 1,089 direct emission
refusals and 803 propagated dependency refusals. The exact headers,
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
  profile and accepted by typed-array/DataView zero-copy constructors, plus the concrete `SharedArrayBuffer` type and
  constructor mapping;
- shared live `MapIterator<T>` cursors for `Map.keys`, `Map.values`, and `Map.entries`, including insertion-order,
  deletion, post-creation insertion, copy identity, and sticky-exhaustion behavior;
- shared `AbortController`/`AbortSignal` state with first-reason retention, listener dispatch/removal, and
  `throw_if_aborted`, covered by a live compiler-versus-Node oracle;
- immutable shared `Blob` bytes with typed-array construction, slicing, MIME normalization, text decoding, and
  `ArrayBuffer` conversion, selected through the runtime profile and covered by a live compiler-versus-Node oracle;
- URI component and browser base64 globals with UTF-8, binary-string, malformed-input, padding, and whitespace
  semantics covered by live compiler-versus-Node oracles;
- shared readable/writable stream and async-iterable carriers, including compiler-emitted writer operations and
  native task callbacks; these admit three more independently compiling SDK headers;
- shared decoded-PCM `AudioBuffer` storage with live channel views and bounded channel copies, selected through the
  runtime profile and exercised by compiler-emitted native code;
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
C++ well-formedness. With GCC 15.2, 703 of the 959 headers compile independently and 256 fail. The composed SDL
profile emits 1,098 modules, of which 755 compile and 343 fail. Compared with compiler `5649642`, the portable pass
count changes from 704 to 703 while 72 malformed headers move behind explicit refusals; the SDL profile retains all
755 previously compiling headers while moving 31 malformed headers behind refusals. The current native
report is dominated by emission defects that cannot be repaired by adding a runtime symbol:

- source module-private helpers are emitted into one package namespace, so package barrels encounter C++
  redefinitions;
- several ambient aliases are applied as templates even though their emitted C++ target is a concrete type;
- some imported types are referenced by snake-case value spelling instead of their emitted PascalCase type name;
- optional values are passed or assigned where their contained value is required;
- discriminated unions represented by `std::variant` still receive direct member access;
- several structurally equivalent anonymous records are emitted as distinct, non-convertible C++ structs;
- a few emitted tokens and type queries remain malformed, including `typeidel`.

The stricter compiler newly refuses 16 direct roots that the previous portable sweep emitted. Five need equivalent
source-union evidence, five expose generic typed-array backing domains that are not represented by the concrete C++
aliases, three contain unresolved callable result types, one needs named structural-row construction, and two retain
an unresolved alternative in a scene-light-selection union. Their dependency refusals account for the rest of the
35 previously emitted modules absent from the full SDL profile. Returning `MapIterator<T>.next().value` as
`T | undefined` also reaches `contextual optionalSingle construction requires expression type evidence`; the exact
iterator binding and its `done` path compile in the live runtime oracle.

WebGPU descriptor bindings cannot yet be enabled faithfully. A focused `GPUBlendState` fixture emits nested
`GPUBlendComponent` literals as new anonymous `Ref<T>` records instead of contextual target values. Direct external
descriptor literals preserve source property order in C++ designators, so valid TypeScript objects whose properties
are written in a different order than the target struct fail C++'s ordered-designator rule. The compiler must
contextualize nested object literals and emit target declaration order before the host can bind `GPUBlendState`,
`GPUStencilFaceState`, `GPUSamplerDescriptor`, and the related bind-group layouts to exact value carriers.

These diagnostics arise after the runtime includes resolve, and many occur in a header before later errors in that
header can be observed. The JSON report from `npm run sdk:compile` is the compact handoff surface for fixing them in
flight-compiler. Downstream source rewriting would obscure compiler provenance and produce a second, unversioned
transpiler, so the checked-in SDK remains the compiler's exact output plus its generated build/member inventories.

The SDL lifecycle profile now supplies the exact `CustomEvent<T>`, navigation timing, document visibility, and focus
carriers. Two compiler contracts remain before that module can use them faithfully. Generated object-valued
`event.detail` access must dereference `Ref<T>` rather than emit `event.detail.member`, and copied callable values must
retain source identity so `removeEventListener(type, listener)` can remove the registration added before the listener
is captured by the returned unsubscribe closure. A diagnostic removal mapping moves `lifecycle.ts` to the separate
"WeakMap value requires a proven C++ representation" refusal, confirming that these are the remaining event-shell
edges rather than missing SDL behavior.

Several direct ambient-member refusals now have exact downstream targets and need only compiler election:

- `Number.parseInt` → `flight::parse_int`, `Number.parseFloat` → `flight::parse_float`, and
  `Number.isSafeInteger` → `flight::is_safe_integer` from `flight/number.hpp`;
- `Object.values` → `flight::object_values` from `flight/object.hpp`;
- iterable `Array.from` → `flight::array_from` from `flight/array.hpp`;
- typed-array `from` → the selected concrete alias's static `from`, such as `flight::Uint32Array::from`, from
  `flight/typed_array.hpp`;
- `ArrayBuffer.isView` → `flight::is_array_buffer_view` from `flight/array_buffer_view.hpp` when the argument has a
  represented closed buffer/view domain.
- `Math.fround` → `flight::fround` from `flight/math.hpp`, and each typed-array constructor's
  `BYTES_PER_ELEMENT` → its `bytes_per_element` constant from `flight/typed_array.hpp`.

Length-only `Array.from({ length }, mapper)` still requires dedicated compiler lowering for its implicit
`undefined` elements. Open `object` arguments to `ArrayBuffer.isView`, `Object.prototype.hasOwnProperty.call`,
generic structured cloning, and `Promise.allSettled` results still need represented compiler contracts; the runtime
does not provide a permissive erasure for them.

The latest compiler still records direct refusals for all seven member families above. Newly emitted generic typed
arrays also expose a distinct representation issue: TypeScript's current library models backing storage as
`Uint8Array<ArrayBufferLike>` and peers, but C++ emission applies the concrete aliases as templates. The runtime must
not redefine each concrete element alias as a generic backing template; the compiler must either erase a proven
backing parameter to the existing `ArrayBufferLike` member or emit a compatible generic carrier deliberately.

The runtime profile maps both spaces for `RangeError` and `TypeError` to the semantic runtime classes in
`flight/error.hpp`. Current generated headers no longer fail by trying to construct `std::range_error` from
`flight::String`; the remaining failures are later optional-unwrapping or missing-symbol emission defects.

The runtime now also provides `flight::all_settled_tasks(Array<Task<T>>)` and
`Task<T>::all_settled(std::vector<Task<T>>)`, returning ordered `TaskSettlement<T>` values without rejecting the
aggregate. The compiler can bind `Promise.allSettled` to the free function after it maps
`PromiseSettledResult<T>` to that carrier and lowers the source union's `status`, `value`, and `reason` access. A
disposable exact-pin member mapping already removes the only direct all-settled refusal; the scene-resource module
then reaches its existing `resolveScene3DResources` dependency refusal, so this change does not alter the current
emitted-header total by itself.

## Complete example sweep

`npm run examples:generate:check` now gives flight-compiler a stable downstream example gate. It discovers all 33
packages under Flight's `examples/packages`, inventories all 181 source modules, and selects the 100-module native
SDL/GL lane: each `render.ts` selector and chosen `render.webgl.ts` implementation receive an explicit recorded
`renderNative` source remap, with the DOM-only cross-backend example recording its fallback. Example output has its
own `examples/upstream/generated/include/flight/examples` tree and references the single canonical SDK tree rather
than copying SDK headers per example.

At the current pins, dependency closure emits 0 of those 100 modules. The linked ledger contains 44 dependency and
56 emission refusal entries. Forty example modules are held by the refused `@flighthq/sdk` barrel and
20 renderer modules are held by the refused `@flighthq/host-web/contract` path through `webGraphicsHost`. The exact
per-module evidence is committed in `examples/upstream/generated/refusals.json`.

The separate frontier ledger deliberately compiles each selected example without its package dependencies, so its
28 missing package-evaluation entries are boundary markers rather than claims that the full graph omitted those
packages. It records 33 dependency and 39 direct emission boundaries, led by contextual optional construction,
captured referent mutation, contextual typing for empty arrays, and incomplete HTML element profiles. The SDL
application profile resolves every direct keyboard, pointer, wheel, DOM attachment, window, and
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
packages own rendering behavior. These host bindings increase the emitted module set from 959 to 1,098, while the
portable compile gate remains useful and independent of platform SDKs.

The SDL host now also implements the complete emitted `AudioDeviceBackend` operation record over SDL's device
callback, including PCM buffer acquisition, concurrent source playback, live gain/pan/rate, bounded regions,
teardown semantics, and application-thread completion delivery. Its generated-record adapter compiles and runs at
this pin. A native example also executes the upstream sound example's compiler-generated procedural PCM through the
SDL backend under CMake and Bazel. The compiler still needs importer-specific module remapping to replace
`webAudioDeviceBackend` in the full sound example. The runtime now represents the `AudioBuffer` stored by
`AudioResource` and created by
`createAudioResourceFromSamples`; encoded-byte decoding still requires an explicit native codec/provider contract.
The host playback seam is no longer part of that blocker.

The downstream `flighthq/flight-cpp/sdl-gl/1` profile now names concrete SDL-owned canvas/context types, shared GL
object handles, context attributes, a weakly recoverable image-source carrier, and
`EXT_texture_filter_anisotropic`. Composed with the runtime and exact Web string-alias profiles it emits 1,081
modules, 32 beyond the runtime/headless inventory; 23 of those additions compile independently. The extension binding removes its direct
ambient refusal; `GlContextRuntime` then
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

Composing `bindings/sdl-app.json` with the maintained renderer profiles admits the represented window, document,
`HTMLElement`, keyboard, mouse, pointer, wheel, and animation-frame cancellation contracts; these now reach their
next compiler or package dependency failures in the full SDK graph. The runtime profile maps `PromiseLike<T>` to the
existing `flight::Task<T>` carrier; the dialog module consequently advances to the compiler's async-closure
coroutine-lowering refusal.

The same profile maps `DOMRect` to the host's complete eight-field `ClientRect`, which is populated from SDL logical
canvas dimensions. This removes the name from all eleven selected example roots that referenced it. Four of those
roots now stop directly at compiler-owned union, captured-reference, intersection, or SDK-dependency boundaries;
the others retain separate Canvas, listener-option, or HTML-control requirements. In the full SDK sweep, direct
ambient-binding refusals are now 150 modules, down from 242 with SDL/GL alone, while the dependency-closed total stays
at 1,098 modules.

The SDL profile now also supplies `Event`, `CompositionEvent`, `InputEvent`, `Gamepad`, `GamepadButton`,
`GamepadEvent`, and `navigator.getGamepads()` with compiler-checked native carriers. SDL's gamepad carrier exposes
the standard `pressed`, `touched`, and `value` button fields and preserves them in each polled snapshot.
`packages/input/src/inputManager.ts` consequently reports only `AddEventListenerOptions[type]` and
`EventTarget[type]`. A focused compiler probe with an otherwise valid `EventTarget` binding fails with `flight-cpp
type position retains unresolved auto placeholder: std::function<auto` for `addEventListener` and
`removeEventListener`. The compiler needs to materialize the listener's event parameter and elect a callable
identity representation that lets the host remove the same listener without comparing `std::function` targets.
`AddEventListenerOptions` also needs a concrete representation of capture, once, passive, and abort-signal behavior;
the current host listener surface does not claim the type while those semantics are incomplete.

The provider-neutral `flighthq/flight-cpp/sdl-wgpu/1` profile now supplies typed shared identity for 16 WebGPU
object domains, exact adapter capability metadata, standard usage flags, and weak-key policies. On its own it adds
18 dependency-closed headers over runtime/headless and all 18 compile. Composed with SDL/GL and the application
shell, it raises the inventory from 1,081 to 1,098 headers; all 17 additions compile and direct ambient-refused
modules fall from the SDL/GL profile's 242 to 166. The device, origin, vertex, and external-image descriptors now have compiler-checked
native representations. `wgpuHost.ts` reaches contextual optional construction, while `wgpuExternalImageSource.ts`
retains only browser constructor values (`DOMException`, image/video/canvas/bitmap/frame); both are compiler or
browser-adapter boundaries rather than missing native WGPU data contracts.

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
