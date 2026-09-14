# flight-compiler adoption status

This is flight-cpp's maintained view of the downstream work requested by
`flight-compiler/agents/flight-cpp-adoption.md`. A header or symbol being present is not enough to call a capability
adopted. Adoption also requires build packaging, compiler-emitted compilation, source differential behavior where
observable, and a complete regenerated SDK closure.

The current pins are Flight `1274ec5` and flight-compiler `993c280`. The portable sweep processes all 154 packages
and 2,851 modules. It emits 959 dependency-closed headers and records 1,892 refusals. All emitted includes resolve;
703 headers compile independently with GCC 15.2 and 256 stop at compiler-emitted C++ errors. Compared with compiler
`5649642`, the stricter semantic pass refuses 73 more modules while removing 72 malformed headers and retaining all
but one of the portable headers that compiled independently.

## Downstream implementation

| Contract | State | Evidence and remaining work |
| --- | --- | --- |
| ArrayBuffer, typed arrays, DataView, text encoding, `String.fromCodePoint`, insertion `splice` | Implemented | Native tests cover shared backing, views, byte order, numeric edge cases, UTF-8 replacement, astral scalars, and insertion order. `TextEncoder` handles scalar UTF-8 and unpaired-surrogate replacement through a live compiler/Node differential. `TypedArray::from` applies JavaScript modulo or clamped element conversion after its optional mapper, and `is_array_buffer_view` recognizes typed and data views without treating their backing buffers as views. Array and Map `keys`, `values`, and `entries` expose shared live cursors that retain insertions until exhaustion and preserve the exhausted state. The runtime source differential exercises these behaviors against Node. Arrays, typed arrays, DataView, maps, sets, records, weak maps, and weak sets treat C++ `const` as constness of the shared handle; referent mutation remains available inside compiler-emitted value-capturing lambdas, matching JavaScript object bindings. |
| `ArrayLike<T>` and `ArrayBufferView` carriers | Implemented downstream ABI | `SequenceView<T>` retains source ownership and identity while adapting `Array<T>`, typed arrays, and structurally compatible shared sources without copying. `ArrayBufferView` retains backing storage, byte range, source-view identity, and dynamic view kind. Native aliasing/lifetime tests and a live compiler-emitted binding oracle cover both. |
| `Iterable<T>` carrier | Implemented downstream ABI | `Iterable<T>` type-erases synchronous traversal while retaining Flight Array, Map, or Set shared storage. Each traversal gets the source collection's iterator semantics; active Array, Map, and Set traversals observe additions before exhaustion. The runtime profile and live compiler fixture cover the generic parameter and all three collection paths. This removes `Iterable[type]` from 14 full-SDK roots and both selected example roots. Ten full-SDK roots advance directly to existing compiler emission failures, while the example snapshot reaches the compiler-owned `Array.from` member mapping. The dependency-closed totals do not change at this pin. |
| `ArrayBufferLike` common backing | Implemented downstream ABI | `ArrayBufferLike` carries ordinary, shared, or host-owned storage with stable identity, lifetime, byte length, mutability, and concurrency policy. Its property view supports emitted `byte_length` access while retaining the existing callable spelling. Typed arrays and `DataView` construct zero-copy views over it; native tests cover shared and external lifetimes plus read-only rejection. The runtime profile binds both `ArrayBufferLike` and `SharedArrayBuffer`, and its live compiler fixture constructs and reads the shared carrier. Full atomic JavaScript `SharedArrayBuffer` operations remain outside this carrier contract. |
| Number conversion, RegExp, URL | Implemented baseline | The emitted APIs compile and the source differential covers the Flight paths in the current SDK. RegExp exec records unmatched capture absence, global `String.match` returns complete matches, and replacement covers JavaScript substitution tokens, callback offsets/input, global iteration, and empty matches. Fixed-signature callbacks can receive optional captures; the compiler still projects its declared `string` callback parameters to empty strings because TypeScript's library signature does not expose their runtime `undefined` value. RegExp remains an ECMAScript syntax and ASCII/UTF-8 matching subset rather than a claim that `std::regex` implements every JavaScript expression. |
| Object helpers | Implemented for represented map records | `object_keys`, `object_entries`, and identity-preserving `object_assign` cover the compiler's current represented map inputs. |
| `Record<K,V>` storage | Prepared runtime ABI | `Record` shares object identity, uses non-inserting presence-bearing reads, canonicalizes numeric and string property identity, and enumerates integer, string, and symbol keys in JavaScript order. `Object.keys`, `Object.entries`, and `Object.assign` have Record-aware paths, and the Node/native oracle covers ordering, collisions, symbol exclusion, and missing reads. The compiler still maps `Record` to `std::unordered_map`; it must elect this spelling, lower reads/writes to the presence-aware API, and lower ordered spreads before adoption is complete. |
| Portable ambient helpers and `WeakSet` | Implemented downstream ABI | `parse_int`, `parse_float`, `is_safe_integer`, `fround`, `object_values`, iterable `array_from`, and callable `to_boolean` implement the numeric, truthiness, and represented collection behavior requested by the current corpus. Typed arrays expose their exact `bytes_per_element` constructor constant. `to_boolean` preserves number/string/nullish truthiness, recurses through optional and variant carriers, and keeps empty represented objects truthy; logical negation of a closed `std::variant` union uses the same conversion. A Node/native differential and compiler-emitted `filter(Boolean)` and closed-union negation fixture cover the contract. `WeakSet<K>` shares identity across copies, never enumerates or retains keys, supports Flight references and weakly recoverable `Array<T>` values, and accepts the same external policy contract as `WeakMap`. The runtime profile maps bare `parseFloat`, bare `isFinite`, `Boolean`, and both `WeakSet` spaces. Compiler built-ins still need to elect `Number.parseInt`, `Number.parseFloat`, `Number.isSafeInteger`, `Math.fround`, `Object.values`, `Array.from`, typed-array `from`/`BYTES_PER_ELEMENT`, and `ArrayBuffer.isView`. Length-only `Array.from({ length }, mapper)` needs compiler lowering because its source elements are `undefined` rather than an iterable C++ element type. |
| Abort signals | Implemented baseline | `AbortController` and `AbortSignal` share cancellation state, retain the first reason, dispatch listeners once, support listener removal, and throw `AbortError` from the emitted `throw_if_aborted` call. A live compiler/Node differential covers those operations. They also accept `flight::Function` listeners and remove copies by their shared identity; compiler election remains before generated callbacks use that path. |
| Blob | Implemented baseline | `Blob` owns immutable bytes and shared identity, concatenates string, binary-view, buffer, and Blob parts, normalizes MIME types, slices by byte index, and returns tasks from `text` and `array_buffer`. The runtime profile maps its type and constructor spaces plus the standard `BlobPart` and `BlobPropertyBag` names, and a live compiler/Node differential covers those emitted signatures and operations. The added aliases remove their remaining direct refusals from the image and image-codec modules; those modules retain their explicit image-element and decoder-provider requirements, so the dependency-closed header total does not increase at this pin. |
| Decoded PCM `AudioBuffer` | Implemented baseline | `AudioBuffer` preserves shared object identity, owns distinct live `Float32Array` views per channel, validates Web Audio construction limits, and implements bounded `copyToChannel`/`copyFromChannel`. The runtime profile maps its type, constructor, and named `AudioBufferOptions` space, and a live compiler-emitted fixture constructs, mutates, and reads the carrier through both inferred and named options. This admits `types/AudioResource.ts` and `types/AudioResourceReference.ts`; the first compiles independently and the second exposes a compiler type/value spelling error. The audio and media modules now retain only their explicit `AudioContext`/node provider requirements. Encoded-byte decoding through `AudioContext` remains an explicit host/codec boundary. |
| Portable Web values and DOM exceptions | Implemented portable value ABI | `ImageData` allocates transparent RGBA storage or retains the exact supplied `Uint8ClampedArray`, derives omitted heights, preserves shared object identity, applies Web IDL unsigned-long conversion, and reports the specified `IndexSizeError`, `InvalidStateError`, `TypeError`, and allocation `RangeError` paths. `DOMException` exposes message, name, and legacy code values. Provider-neutral `TextMetrics`, `ImageEncodeOptions`, `PermissionDescriptor`, and `PositionOptions` carriers preserve the full pinned Web IDL field surfaces and optional presence. The Web-types profile maps these names plus `ImageDataArray`, `ImageDataSettings`, and both `ImageData` spaces; live compiler fixtures compile and run constructor, property, identity, and dictionary access. The ambient `DOMException` value remains unbound because current `typeof`/`instanceof` emission produces invalid C++ rather than calling a runtime type-test contract. |
| URI and base64 globals | Implemented | `encode_uri_component` and `decode_uri_component` preserve the JavaScript unescaped set, UTF-8 scalar rules, and malformed-input errors. `atob` and `btoa` implement the browser binary-string byte domain and forgiving base64 decoding. Live compiler/Node differentials cover both pairs. Their runtime-profile mappings remove every direct `encodeURIComponent`, `decodeURIComponent`, `atob`, and `btoa` refusal. Protocol/form code now reaches unresolved tuple-element `auto`; audio base64 loading reaches its explicit audio-host bindings. The emitted closure count is unchanged at this pin. |
| Readable, writable, and async-iterable streams | Implemented baseline | Shared stream carriers preserve identity, enforce one active reader or writer, route writes, close, abort, reads, and cancellation through task-returning native callbacks, and expose an asynchronous `next` operation. A live compiler/Node oracle compiles the generic external bindings and executes compiler-emitted writer operations. The profile admits `types/FileSystem.ts`, `types/Socket.ts`, and `socket/explainSocketSendFailure.ts`; all three new headers compile independently. Full Web Streams queuing, backpressure, piping, and `for await` emission remain future contract work. |
| JSON | Partial | `JsonValue` preserves null, boolean, number, string, array, and object domains; `Json::parse` and `Json::stringify` round-trip them. Generated ordinary structs still need compiler-provided member reflection, and replacer semantics remain open. |
| Intl | Partial | The locale-neutral baseline and emitted names exist and are source-compared for deterministic English cases. Locale selection, option validation, and a pinned ICU-style provider remain open. |
| Callable signature ABI | Prepared runtime ABI | `flight::Function<Signature>` type-erases a callable while giving all copies one weakly recoverable identity, and the existing exact argument-pack metadata and `bind_callable_v1` support both it and `std::function`. Native tests cover distinct construction, copy, invocation, weak recovery, and expiry. The compiler still emits `std::function`; it must elect `flight::Function` for JavaScript function values before callback identity is adopted end to end. Optional/rest role metadata and their complete oracle matrix remain open. |
| Symbol and closed Entity construction | Implemented for current output | Symbol interning, named structural access, the Entity runtime symbol slot, and emitted AmbientLight construction compile and run. |
| Open structural rows and proxies | Partial | Structural views reuse one `RowOwner` per source object. The generated member table installs one typed, presence-bearing cell for each reachable named field, while computed symbols retain distinct canonical identities. Writable, readonly, partial, and compatible merged projections share those cells. `make_structural_write_proxy` provides distinct identity, forwarding, pre-write interception, exception ordering, nested composition, and projection identity; a live compiler-emitted generic Entity proxy compiles and runs. Construction still needs an all-required-fields check, casts need whole-schema compatibility constraints, and `RowMerge` must validate every collision at schema instantiation rather than when a field is accessed. |
| Conditional capability facets | Implemented | `FacetRef`, lambda-based required `MemberPath`, `RequiredMemberFacet`, `ConditionalFacetRef`, and explicit `assume_conditional_facets` preserve one base reference and statically reject absent or optional paths. A live compiler-emitted generic Tray fixture proves capable, incapable, and optional host paths plus referent identity. |
| WeakMap, WeakSet, and erased typed views | Implemented runtime ABI | Default Flight-reference, closed-variant, and weakly recoverable Flight value policies are weak and identity-based. External policies use the specified weaken/lock/identity/hash/equal contract. Tests cover expiry, shared views, wrong tags, overwrite, and deletion. Host weak-key policies remain host-profile work. |
| SDL host mechanics | Implemented | SDL lifecycle, events, monotonic clock, windows, GL contexts, and callback-owned WebGPU surfaces are packaged by CMake and Bazel; Vulkan remains CMake-only. Bazel pins and builds SDL 3.4.10 and keeps the manual host targets out of the dependency-free core sweep. `InputDispatcher` translates keyboard, text/IME, pointer, wheel, and standard-layout gamepad events into records shaped for Flight's synchronous input-ingress seam; windows expose text-input and relative-pointer controls. `GlCanvas` and `WebGl2Context` share context/window lifetime and supply native procedure lookup and presentation. Connecting those records to generated `InputIngressSink` remains a thin adapter after its refused pointer dependency emits. |
| SDL/OpenGL binding profile | Implemented host ABI | The versioned profile maps the canvas, WebGL2 context, GL objects, context options, active-uniform metadata, image sources, and the anisotropy, float-buffer/filtering, and compressed-texture extension domains to concrete `host_sdl` types. GL object and image carriers preserve shared identity; image sources supply the weak-key policy used by Flight texture caches. The context forwards Flight's buffer, texture, compressed-texture, framebuffer, readback, shader/program, state, query, draw, and uniform command surface. Its closed `getParameter` carrier preserves queried object identity, and its extension result handles feature detection without `any`. The live compiler fixture compiles exact calls in each category, and the SDL tween exercises resource allocation, texture upload, framebuffer readback, state queries, shader drawing, and presentation through the same canvas/context seam. Populating the generated Flight `GlContext` callable record remains blocked on compiler method-type emission. |
| SDL native image-source profile | Implemented host ABI | `Flight::HostSdlImage` and `flighthq/flight-cpp/sdl-image/1` give GL and WebGPU one decoded-RGBA owner, source-kind metadata, and a cohesive weak-key policy. The DOM image/video/bitmap/offscreen/SVG/frame type names map to that carrier without adding a Canvas renderer. Browser constructor values and external `instanceof` lowering remain explicit compiler/provider boundaries. |
| SDL/WebGPU binding profile | Implemented handle ABI | The versioned profile maps 18 WebGPU object domains to typed, provider-owned handles and consumes the shared SDL image-source domain for external copies. Handle copies preserve native identity, provider release callbacks run exactly once, and weak policies support generated caches. Adapter feature/limit metadata, device/origin/vertex/external-image/sampler/render-pipeline descriptors, bind-group values, buffer and texture transfer descriptors, iterable extents, buffer sources, and the standard buffer, texture, shader, color-write, and map-mode flags are represented exactly. Dawn or wgpu-native still supplies device operations; this package does not select an implementation. |
| SDL application-shell profile | Implemented host ABI | The profile binds `window`, `document`, animation-frame request/cancellation, base/keyboard/text/pointer/wheel/custom events, listener options, and value-owned standard-layout gamepad snapshots to the SDL host. Listener registrations preserve order, remove `once` callbacks before reentrant dispatch, and remove signal-bound callbacks on abort. The shell also deduplicates and removes `flight::Function` listeners by copy-stable identity and capture flag; the external removal binding remains disabled until the compiler emits that carrier. `WebPlatformInput` preserves `InputDispatcher` state, routes input, and projects SDL visibility/focus changes through `document.hidden`, `document.hasFocus()`, and lifecycle listeners; frame callbacks use a one-turn queue. The headless profile reports an empty browser-navigation history so Flight selects its cold-launch path. Compiler fixtures compile the current application shell and performance lookup, while native tests cover frame ordering, cancellation, document state/listeners, copied callback removal, event narrowing, custom-detail identity, keyboard delivery, and gamepad polling. Rich HTML controls remain a deliberately narrow compatibility surface for examples rather than a browser DOM implementation. |
| SDL cursor backend | Implemented host ABI | `SdlCursorBackend` maps Flight's CSS-compatible cursor identifiers onto SDL's system cursor set, hides for `none`, restores the host default for null, and preserves the requested value when a headless driver exposes no cursor. `Flight::HostSdlSdkCursor` and Bazel `//:host_sdl_sdk_cursor` populate and execute the exact emitted `flight::types::CursorBackend` record; copies share state and native cursor ownership. Compiler package/source remapping still has to select this provider in place of `createWebCursorBackend` in the upstream interaction and sound examples. |
| SDL text clipboard backend | Implemented dependency-closed SDK record | `Flight::HostSdlSdkClipboard` and Bazel `//:host_sdl_sdk_clipboard` populate the exact emitted `ClipboardTextBackend`. Clear, presence, UTF-8 read, and UTF-8 write preserve Flight's task-returning boolean/empty-string contract over SDL's main-thread clipboard API. Rich formats, images, bookmarks, and change events remain absent capabilities. |
| SDL device backend | Implemented dependency-closed SDK record | `Flight::HostSdlSdkDevice` and Bazel `//:host_sdl_sdk_device` populate the exact emitted `DeviceBackend`. SDL reports attached keyboard/mouse presence, the window's current desktop display dimensions and pixel ratio, window-safe-area insets, CPU count, configured memory, and canonical native platform identity. Durable install id, available memory, physical DPI, OS version, and hardware/product identity retain Flight's specified sentinels. Retained display and safe-area readers fail closed after window destruction. |
| SDL platform backend | Implemented dependency-closed SDK record | `Flight::HostSdlSdkPlatform` and Bazel `//:host_sdl_sdk_platform` populate the exact emitted `PlatformBackend` into the caller-owned `PlatformInfo`. SDL supplies the closed OS name, preferred BCP-47 locale, and touch presence; the native adapter supplies architecture, byte order, and pointer width and retains Flight's empty/unknown sentinels for facts SDL cannot report. |
| SDL application-window backends | Implemented dependency-closed SDK records | `Flight::HostSdlSdkWindow` and Bazel `//:host_sdl_sdk_window` populate the emitted visibility, fullscreen command, input-target preparation, input-focus, file-drop, and pointer-lock records. Target handles are weakly registered by generated reference identity; focus/drop subscriptions return exact release closures; SDL relative mouse mode reports the closed pointer-lock outcome domain; and retained records fail closed after native destruction. Fullscreen subscriptions and `ApplicationExitBackend` remain disabled while generated callbacks use identity-less `std::function`; their exact unsubscribe contracts require compiler adoption of `flight::Function`. |
| SDL audio-device backend | Implemented host ABI | `SdlAudioDeviceBackend` implements Flight's device, buffer, and source lifecycle over an SDL device callback. It mixes mono/stereo Float32 PCM with live gain, equal-power pan, playback rate and bounded regions, preserves acquired buffers, suppresses callbacks on teardown, and queues completions for explicit delivery on the application thread. `Flight::HostSdlSdkAudio` and Bazel `//:host_sdl_sdk_audio` populate and execute the exact emitted `flight::types::AudioDeviceBackend` record as a build-tree preview. The native sound example runs compiler-generated copies of Flight's procedural PCM calculations through this path under both CMake and Bazel. Compiler source remapping and the encoded-audio decoding boundary still gate the full upstream sound module. |

## Integration and release gates

- The exact generated inventory, refusal ledger, initialization plan, CMake header list, Bazel target, and structural
  member table are committed under `generated/` and reproduced by `npm run sdk:check`.
- CMake `Flight::SdkPreview` and Bazel `//:sdk_preview` expose every emitted header without claiming a finished SDK.
  The preview is intentionally not installed.
- `npm run sdk:compile` compiles every emitted header independently and writes the compiler-facing report to
  `out/sdk-header-compilation.json`.
- `npm run sdk:compile:headless` applies the same audit to the combined portable-runtime/headless inventory. At the
  current pin, the composed profile emits 1,049 modules: 90 more than the manifest-free floor. Of those additional
  headers, 12 compile and 78 advance to existing tuple, union, reference-conversion, aggregate-construction,
  typed-array-template, spatial-type, and type-spelling defects. Because every shared header is byte-identical to the
  full SDL inventory, its completed audit gives 715 passing and 334 failing headers.
- `npm run sdk:generate:sdl-gl` adds the exact Web string-alias and maintained SDL/OpenGL binding profiles. It emits
  1,081 modules, 32 more than the runtime/headless inventory. Of those additions, 23 compile independently and nine
  expose existing compiler defects. Its byte-identical subset of the full SDL audit is 738 passing and 343 failing.
- `npm run sdk:generate:sdl-wgpu` applies the Web string aliases and provider-owned WebGPU handle profile. It emits
  1,067 modules, 18 more than runtime/headless, and every added header compiles independently: 733 pass and 334 fail
  overall.
- `npm run sdk:generate:sdl` composes the GL, WebGPU, and SDL application profiles. It emits 1,098 modules; all 17
  headers beyond the SDL/GL inventory compile, for 756 passing and 342 failing headers. Direct external-binding
  refusals fall from the SDL/GL profile's original 242 to 111. Window, document, `HTMLElement`, animation-frame
  cancellation, and the represented input event types advance to their next compiler or dependency boundary. The
  runtime profile also maps the compiler's existing `PromiseLike<T>` task domain to `flight::Task<T>`; `dialog.ts`
  now reaches the compiler-owned async-closure coroutine blocker instead of stopping at that ambient type.
  `inputManager.ts` now has exact native `Gamepad`, `GamepadButton`, `navigator.getGamepads()`, and
  `AddEventListenerOptions` carriers and lacks only `EventTarget[type]`. Binding that target nominally exposes the
  compiler's unresolved `std::function<auto` event-listener parameter, so the host leaves it absent until callable
  listener types and removal identity have a concrete compiler contract.
  The lifecycle module now passes `CustomEvent<T>`, `PerformanceEntryList`, `PerformanceNavigationTiming`,
  `document.hidden`, and `document.hasFocus()` and stops at `document.removeEventListener`. A diagnostic mapping for
  removal advances it to its separate closed-`WeakMap` value proof, but it cannot be maintained while copied
  `std::function` values lose JavaScript callback identity in the returned unsubscribe closure. The downstream
  `flight::Function` carrier and SDL removal operation are ready; the compiler must elect that carrier for function
  values before the binding becomes sound. Object-valued custom event detail also needs the compiler to emit
  reference member access instead of `Ref<T>.member`; the native carrier retains the exact `Ref<T>`.
  `bindings/web-types.json` separately elects 22 standard string-literal domains, the exact
  `DOMHighResTimeStamp` number alias, portable `DOMPointInit` and `CanvasRenderingContext2DSettings`
  dictionaries, and CPU-backed `ImageData` values without selecting a Canvas or WebGPU implementation. Optional dictionary fields retain the
  difference between an omitted member and an explicit false or zero. The SDL application
  profile maps `DOMRect` to the complete eight-field logical rectangle already returned by the GL canvas. Together
  these contracts add compiling `CanvasMaterialState` and `CanvasMaterialRenderer` headers and advance the other
  affected modules to their concrete handle, lowering, or dependency boundaries.
  The application profile also binds the concrete HTML element families used by upstream controls. Its universal
  `CreatedElement` facade is required because emitted `document.createElement()` calls retain `auto` despite their
  tag-dependent TypeScript types; DOM members compile on that facade, while canvas-only calls lazily materialize the
  existing SDL GL surface.
  The GL profile also removes all direct standard extension-type refusals. `glCompressedTexture.ts` now stops at
  dual-sentinel optional-chain lowering, `glRenderTarget.ts` at nullish-coalescing presence lowering, and
  `glEnvironmentIblBake.ts` at an unrelated contextual `flight::Map` union conversion.
  The portable dictionary mappings similarly clear those ambient names without admitting a fake 2D renderer:
  Canvas render-state modules now stop solely at `CanvasRenderingContext2D`, and `canvasShapeCommands.ts` retains
  only its concrete Canvas and media handle requirements.
- `npm run examples:generate` selects 100 native modules from all 181 sources in all 33 pinned upstream example
  packages, mirroring Flight's WebGL build selection with an explicit, recorded `renderNative.ts` remap. No example
  module is dependency-closed yet. The SDL application profile removes every direct `window`, `document`, animation
  frame, keyboard, pointer, wheel, gamepad-button, and `DOMRect` refusal. The rectangle binding clears that ambient
  name from eleven selected roots; collision, scene-picking, shapes, and spatial now expose their next compiler or
  dependency boundary. Remaining direct names describe real work: Canvas 2D, richer HTML controls, media, and the
  sound example's `AudioContext`. The selected ledger contains 55 emission and 45
  dependency refusals; its dependency-first frontier remains 39 compiler emission failures, 33 propagated dependency
  failures, and 28 external-package initialization edges. The inventory is committed under
  `examples/upstream/generated/` and contains no duplicate SDK sources.
- `npm run runtime:oracle` executes TypeScript-valid source behavior under Node and compares it with the native
  runtime. It currently covers 49 cross-runtime observations.
- `npm run structural:oracle` generates the exact generic Entity write proxy through the pinned compiler, compiles
  the emitted headers, and executes an intercepted write against the working runtime.
- `Flight::Sdk` remains blocked until every emitted header in the selected binding profile compiles. At that point it
  must be installed/exported through CMake and exposed through Bazel, then exercised as an installed consumer.
- The reciprocal lock cannot be completed solely in this checkout: after these commits land, flight-compiler must pin
  the landed flight-cpp revision and run its exact-pin emitted compile. The unreleased C++ ABI number also requires a
  coordinated decision before either repository advertises a release ABI.

## Compiler and host work still gating the SDK

The current 255 portable and 342 SDL-profile native header failures begin with compiler-emitted optional/value
conversions, concrete typed-array aliases used as templates, value spelling used where a type name is required,
invalid union member access,
non-convertible duplicate anonymous records, package-scope helper collisions, and malformed type queries. These must
be corrected in flight-compiler rather than rewritten in the generated tree.

The stricter compiler newly refuses 16 direct roots that the previous portable sweep emitted. Five need equivalent
source-union evidence, five expose generic typed-array backing domains that are not represented by the concrete C++
aliases, three contain unresolved callable result types, one needs named structural-row construction, and two retain
an unresolved alternative in a scene-light-selection union. Their dependency refusals account for the rest of the
35 previously emitted modules absent from the full SDL profile. Returning `MapIterator<T>.next().value` as
`T | undefined` also reaches `contextual optionalSingle construction requires expression type evidence`; the exact
iterator binding and its `done` path compile in the live runtime oracle.

The SDL/WGPU profile now binds the `GPUBlendComponent`, `GPUBlendState`, `GPUStencilFaceState`, and
`GPUSamplerDescriptor` values used by Flight's current renderer literals. It also binds `GPUBufferDescriptor`,
`GPUTextureDescriptor`, `GPUExtent3D`, `GPUAllowSharedBufferSource`, and the texel-copy dictionaries used by the
renderer test backends. Optional fields preserve presence, iterable extents remain iterable, and shared buffers and
views retain their backing identity for the provider adapter. The two affected test-helper refusal records now move
past every one of these transport names, although their separate browser-image and test-host bindings keep the
dependency-closed count unchanged. `wgpuRenderState.ts` advances from its last ambient refusal to the compiler's
`dual-sentinel optional chaining requires presence projection lowering` boundary; seven other modules advance to
their actual union-evidence, optional-callable, anonymous-property, or `Record`-spread compiler failures.

Bind-group resources, entries, layouts, and their outer descriptors now also have exact provider value carriers.
`customShaderWgpuMeshMaterialRenderer.ts`, `wgpuMeshPipeline.ts`, and `wgpuShadedPrelude.ts` advance from ambient
refusals to `typeOf` computation, inline buffer-resource contextualization, and dense-array length construction,
respectively. That lowers the full SDL direct ambient frontier from 123 to 120 without changing the emitted closure.
The render-pipeline descriptor and its programmable, vertex, fragment, color-target, primitive, depth/stencil, and
multisample values now preserve optional presence and nullable iterable slots as well. This removes
`GPURenderPipelineDescriptor` from `wgpuTestHelper.ts`; its refusal record remains at the browser-image and test-host
bindings, so the direct-record and emitted counts stay at 120 and 1,098.
The separate SDL image profile then supplies one provider-owned decoded source and weak policy for
`HTMLImageElement`, `HTMLVideoElement`, `ImageBitmap`, `OffscreenCanvas`, `SVGImageElement`, `VideoFrame`,
`CanvasImageSource`, `TexImageSource`, and `GPUCopyExternalImageSource`. The six constituent type names disappear
from the full graph and direct ambient-refusal records fall from 120 to 112. `glDraw.ts` and several scene renderers
now expose contextual source-union evidence, while `wgpuDraw.ts` retains only its browser constructor values. The
emitted closure remains 1,098 because no newly advanced root is otherwise dependency-complete.
The headless profile now supplies the standard `PerformanceEntry` value and `PerformanceEntryList` base-entry array
used by the lifecycle module while retaining the narrower native navigation-timing result for
`getEntriesByType('navigation')`. This removes the last performance ambient from that root and lowers direct
ambient-refusal records from 112 to 111; the module then stops at the deliberately unbound listener-removal contract
described above.
Direct external descriptor construction still preserves source property order instead of the target aggregate's
declaration order. The compiler must supply contextual target types and ordered designators before arbitrary valid
TypeScript descriptor literals compile regardless of property order.

The runtime profile maps `RangeError` and `TypeError` to downstream classes that preserve semantic messages and their
common `flight::Error` base. Current generated headers no longer fail by constructing `std::range_error` from
`flight::String`; they advance to existing optional-unwrapping or missing-symbol emission defects.

The global `Boolean` value maps to the callable `flight::to_boolean` object. The same spelling works for both
`Boolean(value)` and `array.filter(Boolean)`. The SDL sweep's `scene2d-formats/svgDocument.ts` consequently advances
to the compiler-owned `ambient value Number member parseFloat has no C++ binding` boundary.

Closed unions represented by `std::variant` now use that same truthiness operation for logical negation. All 22 SDL
headers previously stopped at a missing variant `operator!` advance: `texture/sampler.ts` compiles independently,
while the other 21 expose the compiler's existing optional-number unwrapping error.

`ArrayIterator<T>` now provides shared live cursors for `Array.keys`, `Array.values`, and `Array.entries`, matching
the Map iterator's sticky exhaustion behavior. The compiler-emitted return types and native cursor behavior compile
in the runtime oracle. `tilemap-formats/tiledXmlParse.ts` advances from its ambient iterator refusal to
`contextual union value type flight::Uint32Array<flight::ArrayBuffer> is not a represented runtime domain`, the
known concrete-typed-array-as-template compiler defect.

The Web-types profile now maps `ImageData`, `ImageDataArray`, and `ImageDataSettings` to a CPU-backed RGBA carrier.
Both source constructors compile and run through a live compiler fixture; supplied typed arrays remain the exact
live storage object, and invalid dimensions or lengths expose named DOM exceptions. The bitmap and effects modules
advance to their explicit `CanvasRenderingContext2D` boundary, while the image-codec modules retain only their
bitmap/canvas provider contracts. This removes `ImageData` from every direct refusal and lowers the SDL profile's
direct ambient-binding frontier from 124 to 123 modules without adding a Canvas 2D implementation.

The same profile now supplies complete provider-neutral `TextMetrics`, `ImageEncodeOptions`, `PermissionDescriptor`,
and `PositionOptions` values. Their compiler-emitted field spellings compile and run, and `TextMetrics` copies retain
the provider result's object identity. These names disappear from all direct refusals; the affected modules now name
only their Canvas, offscreen-canvas, geolocation, permissions, media, or wake-lock provider boundaries. Because each
root retains one of those explicit dependencies, this preparation does not change the 1,098-header closure.

`flight::all_settled_tasks` now returns an ordered, non-rejecting task of `TaskSettlement<T>` records and preserves
exact rejection values, including for `void` tasks. Mapping `Promise.allSettled` also requires the compiler to map
the source `PromiseSettledResult<T>` discriminated union onto this carrier and lower `status`, `value`, and `reason`
access without direct `std::variant` member access. A disposable exact-pin member mapping removes the current direct
refusal; the scene-resource caller then reaches an already-refused dependency, so the emitted header total is
unchanged.

The versioned `flighthq/flight-cpp/runtime-carriers/1` profile supplies the implemented `AbortController`,
`AbortSignal`, `ArrayBufferLike`, `SharedArrayBuffer`, `ArrayLike<T>`, `ArrayBufferView`, `Iterable<T>`, `Blob`, `BlobPart`,
`BlobPropertyBag`, `MapIterator`, `ArrayIterator`,
`WeakSet`, `atob`, `btoa`,
`encodeURIComponent`, `decodeURIComponent`, `ReadableStream<T>`, `WritableStream<T>`, `AsyncIterable<T>`,
`TextEncoder`, `AudioBuffer`, `AudioBufferOptions`, `Boolean`, bare `parseFloat`, and bare `isFinite`
bindings. Combined with the headless profile, it removes every direct refusal for those portable names and expands
the dependency-closed inventory from 959 to 1,049 emitted modules. The versioned
`flighthq/flight-cpp/headless/1` profile supplies monotonic `performance.now`, `console.debug`, and
single-threaded host-pumped timeout/interval functions. Its selected profile, identity, and digest are recorded in
each profile-specific generated manifest, and a live compiler fixture compiles and runs every binding. SDL's host
loop exposes the same timer pump. Log now reaches the compiler-owned ordered-`Record` spread refusal, and Signals
throttle reaches the dependent-callable-pack refusal; neither remains blocked on its headless ambient names. The
headless values do not change the aggregate count independently at this pin. The
`flighthq/flight-cpp/sdl-gl/1` profile supplies the concrete SDL surface, WebGL2 context, GL handle, image source, and
weak-cache policy types, as well as the `EXT_texture_filter_anisotropic` carrier. In composition with the current
runtime profile it admits 30 additional modules. The Flight `GlContext` module then reaches a compiler fail-closed
check because inherited `viewport` is emitted as unresolved `auto`; fixing that method typing is the next boundary
before `render-gl` can call the native adapter. Its materialized members also contain six constant/method pairs that
collapse to identical snake-case target names; these require deterministic disambiguation in the compiler.
`GlContextRuntime` has advanced past its extension binding and now
stops at the compiler's closed-value proof for a `WeakMap` field. Maintained
profiles for Node/tooling, browser/media, and SDL/Vulkan are still required; the provider-neutral SDL/WebGPU profile
is maintained here.
