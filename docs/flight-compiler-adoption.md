# flight-compiler adoption status

This is flight-cpp's maintained view of the downstream work requested by
`flight-compiler/agents/flight-cpp-adoption.md`. A header or symbol being present is not enough to call a capability
adopted. Adoption also requires build packaging, compiler-emitted compilation, source differential behavior where
observable, and a complete regenerated SDK closure.

The current pins are Flight `903f328` and flight-compiler `fbfcc11`. The corpus grew with them, to 155 packages and
2,900 modules. The portable floor -- no binding profile applied -- emits 985 dependency-closed headers and records
1,915 refusals. The complete SDL profile emits **1,161** headers with 1,739 refusals: 960 direct emission refusals
and 779 propagated dependency refusals. The upstream example inventory now covers 34 examples and 103 selected
modules, none of them dependency-closed.

Against the same pins before this round's downstream work, the SDL profile emitted 1,145 modules with 1,755
refusals. The Canvas 2D contract, `structuredClone`, and the per-arm settled results took the
`runtime external symbol binding plan is incomplete` family from 96 modules to 36; sixteen modules emit outright and
the rest advanced to their next blocker, and none newly refuses. Independent-header compilation was not re-measured
this round and its last recorded figures belong to the previous pin, so they are not restated here.

## Downstream implementation

| Contract | State | Evidence and remaining work |
| --- | --- | --- |
| Erased dynamic value for `any` and `unknown` | Implemented runtime ABI, awaiting compiler election | `flight::Any` is a closed variant over `undefined`, `null`, boolean, number, string, symbol, object reference, callable, and opaque host value, so an unconstrained position holding a number is not misstated as `flight::Ref<void>`. Object references retain their concrete type and are recovered only at it. `typeof`, `===`, SameValueZero, `Object.is`, `ToBoolean`, and the abstract relational comparison are implemented exactly; ordering a symbol or a non-primitive throws `TypeError` rather than fabricating a `ToPrimitive` result. `AnySlot` keeps a missing entry distinct from an entry whose value is `undefined`, proved through `Record::get`. No `bigint` alternative exists and that gap is documented rather than approximated. Election is compiler-side: `emitTypeCpp` maps `kind: 'unknown'` to `auto` and no `sourceName` exists for `any`/`unknown`, so no binding profile can reach it. |
| Symbol-keyed attached properties over an erased object | Implemented runtime ABI, awaiting compiler election | `flight::AttachedProperties` and `flight::attached_properties` project a `Ref<Object>`, a `Ref<void>`, or a `StructuralRef` onto one symbol-keyed store. Two defects behind the ask are fixed: the owner registry was keyed per object *type*, so typed and erased projections of one object saw different entries, and it held owners weakly, so attachments died with the last transient row. It is now one address-keyed table holding owners strongly and objects weakly, with `StructuralRef` retaining its object so existing lifetimes are unchanged. `get` returns `AnySlot`, so a missing entry and a present `undefined` stay distinct, and a reused address never inherits the previous object's entries. Nothing copies the object or its properties. `flight/particles/particle_emitter_signals.hpp` is the header this exists for; flight-compiler elects the final spelling. |
| Canvas 2D contract | Implemented runtime ABI | `flight/canvas_2d.hpp` owns the drawing-state stack, the current transformation matrix, path construction, dash lists, gradient stops, pattern parameters, and context settings; a host supplies pixels through `Canvas2DRasterizer`. No rasterizer ships and none is faked: a context without one reports `isContextLost()` and throws from every pixel-producing or pixel-reading call while still maintaining the state the runtime owns. Path segments retain the transform in force when each was added, so a transformed arc stays exact instead of being flattened at a runtime-chosen tolerance. `Canvas2DImageSource` carries any host source type with its type intact. `bindings/web-types.json` elects `CanvasRenderingContext2D`, `CanvasGradient`, `CanvasPattern`, and `DOMMatrix`; the direct `CanvasRenderingContext2D` refusal goes from 58 modules to 0. Native tests and a compiler-emitted oracle that runs against a recording rasterizer cover state, transform, path, style, and drawing dispatch. `OffscreenCanvasRenderingContext2D` and the browser image constructor values remain explicit provider boundaries. |
| `structuredClone` | Implemented runtime ABI | `flight::structured_clone` deep-copies the runtime's value domain, preserves shared references, and clones a cyclic reference graph into an equally cyclic one by recording each clone before copying its contents. Symbols and callables are refused with `DataCloneError`, as in JavaScript; a type with no clone definition is a compile-time refusal naming `structured_clone_traits`, because C++20 cannot enumerate an aggregate's members and a permissive default would be a shallow copy wearing a deep copy's name. Elected in `bindings/runtime.json`; all three `@flighthq/snapshot` refusals clear. |
| Per-arm settled task results | Implemented runtime ABI | `flight::TaskFulfillment<T>` and `flight::TaskRejectionResult` carry only the member their arm has, with `status` spelled as the string the source narrows on, and `as_fulfilled`/`as_rejected` move from the union without inventing the other arm's member. Elected in `bindings/runtime.json` as `PromiseFulfilledResult` and `PromiseRejectedResult`; both affected modules clear. |
| ArrayBuffer, typed arrays, DataView, text encoding, `String.fromCodePoint`, insertion `splice` | Implemented | Native tests cover shared backing, views, byte order, numeric edge cases, UTF-8 replacement, astral scalars, and insertion order. `TextEncoder` handles scalar UTF-8 and unpaired-surrogate replacement through a live compiler/Node differential. `TypedArray::from` applies JavaScript modulo or clamped element conversion after its optional mapper, and `is_array_buffer_view` recognizes typed and data views without treating their backing buffers as views. Array and Map `keys`, `values`, and `entries` expose shared live cursors that retain insertions until exhaustion and preserve the exhausted state. The runtime source differential exercises these behaviors against Node. Arrays, typed arrays, DataView, maps, sets, records, weak maps, and weak sets treat C++ `const` as constness of the shared handle; referent mutation remains available inside compiler-emitted value-capturing lambdas, matching JavaScript object bindings. |
| `ArrayLike<T>` and `ArrayBufferView` carriers | Implemented downstream ABI | `SequenceView<T>` retains source ownership and identity while adapting `Array<T>`, typed arrays, and structurally compatible shared sources without copying. `ArrayBufferView` retains backing storage, byte range, source-view identity, and dynamic view kind. Native aliasing/lifetime tests and a live compiler-emitted binding oracle cover both. |
| `Iterable<T>` carrier | Implemented downstream ABI | `Iterable<T>` type-erases synchronous traversal while retaining Flight Array, Map, or Set shared storage. Each traversal gets the source collection's iterator semantics; active Array, Map, and Set traversals observe additions before exhaustion. The runtime profile and live compiler fixture cover the generic parameter and all three collection paths. This removes `Iterable[type]` from 14 full-SDK roots and both selected example roots. Ten full-SDK roots advance directly to existing compiler emission failures, while the example snapshot reaches the compiler-owned `Array.from` member mapping. The dependency-closed totals do not change at this pin. |
| `ArrayBufferLike` common backing | Implemented downstream ABI | `ArrayBufferLike` carries ordinary, shared, or host-owned storage with stable identity, lifetime, byte length, mutability, and concurrency policy. Its property view supports emitted `byte_length` access while retaining the existing callable spelling. Typed arrays and `DataView` construct zero-copy views over it; native tests cover shared and external lifetimes plus read-only rejection. The runtime profile binds both `ArrayBufferLike` and `SharedArrayBuffer`, and its live compiler fixture constructs and reads the shared carrier. Full atomic JavaScript `SharedArrayBuffer` operations remain outside this carrier contract. |
| Number and string conversion, RegExp, URL | Implemented baseline | `number_to_string(value, radix)` implements ECMAScript radix validation, binary-precision digit termination, large-number zero filling, and round-to-even carry behavior; `String::pad_end` repeats and truncates its fill in UTF-16 code units. Native coverage and the Node differential exercise integer, fraction, large-number, invalid-radix, and padding paths, and the five color/tilemap headers that required radix conversion now compile. RegExp exec records unmatched capture absence, global `String.match` returns complete matches, and replacement covers JavaScript substitution tokens, callback offsets/input, global iteration, and empty matches. Fixed-signature callbacks can receive optional captures; the compiler still projects its declared `string` callback parameters to empty strings because TypeScript's library signature does not expose their runtime `undefined` value. RegExp remains an ECMAScript syntax and ASCII/UTF-8 matching subset rather than a claim that `std::regex` implements every JavaScript expression. |
| Object helpers | Implemented for represented map records | `object_keys`, `object_entries`, and identity-preserving `object_assign` cover the compiler's current represented map inputs. |
| `Record<K,V>` storage | Implemented represented ABI | `Record` shares object identity, uses non-inserting presence-bearing reads, canonicalizes numeric and string property identity, and enumerates integer, string, and symbol keys in JavaScript order. The compiler now elects `flight::Record`; `Object.keys`, `Object.entries`, and `Object.assign` have Record-aware paths, and the Node/native oracle covers ordering, collisions, symbol exclusion, and missing reads. Conversions between distinct structural value schemas remain compiler-owned, as shown by `particle_emitter_signals.hpp`. |
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
| Symbol and closed Entity construction | Implemented for current output | `Symbol::for_key` provides global interning while `Symbol(String)` creates a distinct non-global identity with the supplied description. Native and Node differential coverage verifies those identity domains. Named structural access, the Entity runtime symbol slot, and emitted AmbientLight construction compile and run. `particle_emitter_signals.hpp` now advances past its emitted `Symbol(String)` call to a separate compiler-owned `Record` conversion. |
| Open structural rows and proxies | Partial | Structural views reuse one `RowOwner` per source object. The generated member table installs one typed, presence-bearing cell for each reachable named field, while computed symbols retain distinct canonical identities. Writable, readonly, partial, and compatible merged projections share those cells. `make_structural_write_proxy` provides distinct identity, forwarding, pre-write interception, exception ordering, nested composition, and projection identity; a live compiler-emitted generic Entity proxy compiles and runs. Construction still needs an all-required-fields check, casts need whole-schema compatibility constraints, and `RowMerge` must validate every collision at schema instantiation rather than when a field is accessed. |
| Conditional capability facets | Runtime implemented; compiler regression | `FacetRef`, lambda-based required `MemberPath`, `RequiredMemberFacet`, `ConditionalFacetRef`, and explicit `assume_conditional_facets` preserve one base reference and statically reject absent or optional paths. Native coverage passes, but the live compiler fixture currently forward-declares a generated alias as a struct before its `using` declaration. |
| WeakMap, WeakSet, and erased typed views | Implemented runtime ABI | Default Flight-reference, closed-variant, and weakly recoverable Flight value policies are weak and identity-based. External policies use the specified weaken/lock/identity/hash/equal contract. Tests cover expiry, shared views, wrong tags, overwrite, and deletion. Host weak-key policies remain host-profile work. |
| SDL host mechanics | Implemented | SDL lifecycle, events, monotonic clock, windows, GL contexts, and callback-owned WebGPU surfaces are packaged by CMake and Bazel; Vulkan remains CMake-only. Bazel pins and builds SDL 3.4.10 and keeps the manual host targets out of the dependency-free core sweep. `InputDispatcher` translates keyboard, text/IME, pointer, wheel, and standard-layout gamepad events into records shaped for Flight's synchronous input-ingress seam; windows expose text-input and relative-pointer controls. `GlCanvas` and `WebGl2Context` share context/window lifetime and supply native procedure lookup and presentation. Connecting those records to generated `InputIngressSink` remains a thin adapter after its refused pointer dependency emits. |
| SDL/OpenGL binding profile | Implemented host ABI | The versioned profile maps the canvas, WebGL2 context, GL objects, context options, active-uniform metadata, image sources, and the anisotropy, float-buffer/filtering, and compressed-texture extension domains to concrete `host_sdl` types. GL object and image carriers preserve shared identity; image sources supply the weak-key policy used by Flight texture caches. The context forwards Flight's buffer, texture, compressed-texture, framebuffer, readback, shader/program, state, query, draw, and uniform command surface. Its closed `getParameter` carrier preserves queried object identity, and its extension result handles feature detection without `any`. The live compiler fixture compiles exact calls in each category, and the SDL tween exercises resource allocation, texture upload, framebuffer readback, state queries, shader drawing, and presentation through the same canvas/context seam. Populating the generated Flight `GlContext` callable record remains blocked on compiler method-type emission. |
| SDL native image-source profile | Implemented host ABI | `Flight::HostSdlImage` and `flighthq/flight-cpp/sdl-image/1` give GL and WebGPU one decoded-RGBA owner, source-kind metadata, and a cohesive weak-key policy. The DOM image/video/bitmap/offscreen/SVG/frame type names map to that carrier without adding a Canvas renderer. Browser constructor values and external `instanceof` lowering remain explicit compiler/provider boundaries. |
| SDL/WebGPU binding profile | Implemented handle ABI | The versioned profile maps 18 WebGPU object domains to typed, provider-owned handles and consumes the shared SDL image-source domain for external copies. Handle copies preserve native identity, provider release callbacks run exactly once, and weak policies support generated caches. Adapter feature/limit metadata, device/origin/vertex/external-image/sampler/render-pipeline descriptors, bind-group values, buffer and texture transfer descriptors, iterable extents, buffer sources, and the standard buffer, texture, shader, color-write, and map-mode flags are represented exactly. Dawn or wgpu-native still supplies device operations; this package does not select an implementation. |
| SDL application-shell profile | Implemented host ABI | The profile binds `window`, `document`, animation-frame request/cancellation, base/keyboard/text/pointer/wheel/custom events, listener options, and value-owned standard-layout gamepad snapshots to the SDL host. Listener registrations preserve order, remove `once` callbacks before reentrant dispatch, and remove signal-bound callbacks on abort. The shell also deduplicates and removes `flight::Function` listeners by copy-stable identity and capture flag; the external removal binding remains disabled until the compiler emits that carrier. `WebPlatformInput` preserves `InputDispatcher` state, routes input, and projects SDL visibility/focus changes through `document.hidden`, `document.hasFocus()`, and lifecycle listeners; frame callbacks use a one-turn queue. The headless profile reports an empty browser-navigation history so Flight selects its cold-launch path. Compiler fixtures compile the current application shell and performance lookup, while native tests cover frame ordering, cancellation, document state/listeners, copied callback removal, event narrowing, custom-detail identity, keyboard delivery, and gamepad polling. Rich HTML controls remain a deliberately narrow compatibility surface for examples rather than a browser DOM implementation. |
| SDL cursor backend | Implemented host ABI | `SdlCursorBackend` maps Flight's CSS-compatible cursor identifiers onto SDL's system cursor set, hides for `none`, restores the host default for null, and preserves the requested value when a headless driver exposes no cursor. `Flight::HostSdlSdkCursor` and Bazel `//:host_sdl_sdk_cursor` populate and execute the exact emitted `flight::types::CursorBackend` record; copies share state and native cursor ownership. Compiler package/source remapping still has to select this provider in place of `createWebCursorBackend` in the upstream interaction and sound examples. |
| SDL text clipboard backend | Implemented dependency-closed SDK record | `Flight::HostSdlSdkClipboard` and Bazel `//:host_sdl_sdk_clipboard` populate the exact emitted `ClipboardTextBackend`. Clear, presence, UTF-8 read, and UTF-8 write preserve Flight's task-returning boolean/empty-string contract over SDL's main-thread clipboard API. Rich formats, images, bookmarks, and change events remain absent capabilities. |
| SDL device backend | Implemented dependency-closed SDK record | `Flight::HostSdlSdkDevice` and Bazel `//:host_sdl_sdk_device` populate the exact emitted `DeviceBackend`. SDL reports attached keyboard/mouse presence, the window's current desktop display dimensions and pixel ratio, window-safe-area insets, CPU count, configured memory, and canonical native platform identity. Durable install id, available memory, physical DPI, OS version, and hardware/product identity retain Flight's specified sentinels. Retained display and safe-area readers fail closed after window destruction. |
| SDL haptics backend | Implemented dependency-closed SDK record | `Flight::HostSdlSdkHaptics` and Bazel `//:host_sdl_sdk_haptics` populate the exact emitted `HapticsBackend` over the first connected SDL gamepad with rumble support. It dynamically discovers replacements, reports amplitude/intensity capabilities, and implements continuous vibration, optional-intensity impact, notification, selection, prepare, and cancel while shared adapter copies retain one owned gamepad reference. SDL has no timed multi-step rumble primitive, so pattern and waveform support are explicitly absent rather than approximated. |
| SDL platform backend | Implemented dependency-closed SDK record | `Flight::HostSdlSdkPlatform` and Bazel `//:host_sdl_sdk_platform` populate the exact emitted `PlatformBackend` into the caller-owned `PlatformInfo`. SDL supplies the closed OS name, preferred BCP-47 locale, and touch presence; the native adapter supplies architecture, byte order, and pointer width and retains Flight's empty/unknown sentinels for facts SDL cannot report. |
| SDL screen backends | Implemented dependency-closed SDK records | `Flight::HostSdlSdkScreen` and Bazel `//:host_sdl_sdk_screen` populate the emitted `ScreenQueryBackend`, `ScreenDetailsBackend`, and `ScreenChangeBackend`. SDL enumerates stable display ids, virtual-desktop and work-area geometry, density, physical dimensions, refresh rate, pixel depth, orientation, HDR state, labels, the primary display, and global pointer location into caller-owned records. Native display events produce exact add/remove/metrics records from a last-known snapshot, and returned release closures are idempotent. Native enumeration requires no Web permission, so the details record resolves `granted`/`true`. Physical DPI, gamut, luminance, and internal/touch classification remain unavailable. |
| SDL soft-keyboard backends | Implemented dependency-closed SDK records | `Flight::HostSdlSdkKeyboard` and Bazel `//:host_sdl_sdk_keyboard` populate the emitted `SoftKeyboardInfoBackend`, `SoftKeyboardVisibilityBackend`, and `SoftKeyboardChangeBackend`. SDL supplies a real screen-keyboard capability probe, per-window shown state, native start/stop operations, and shown/hidden events. Unsupported drivers return Flight's `operation-failed` or `acquisition-failed` outcomes; unavailable keyboard geometry stays zero. Returned unsubscribe closures are idempotent, and retained records fail closed after window destruction. Platform-specific style, resize, accessory-bar, and scroll-assist setters remain absent. |
| SDL application-window backends | Implemented dependency-closed SDK records | `Flight::HostSdlSdkWindow` and Bazel `//:host_sdl_sdk_window` populate the emitted visibility, fullscreen command, input-target preparation, input-focus, file-drop, and pointer-lock records. Target handles are weakly registered by generated reference identity; focus/drop subscriptions return exact release closures; SDL relative mouse mode reports the closed pointer-lock outcome domain; and retained records fail closed after native destruction. Fullscreen subscriptions and `ApplicationExitBackend` remain disabled while generated callbacks use identity-less `std::function`; their exact unsubscribe contracts require compiler adoption of `flight::Function`. |
| SDL audio-device backend | Implemented host ABI | `SdlAudioDeviceBackend` implements Flight's device, buffer, and source lifecycle over an SDL device callback. It mixes mono/stereo Float32 PCM with live gain, equal-power pan, playback rate and bounded regions, preserves acquired buffers, suppresses callbacks on teardown, and queues completions for explicit delivery on the application thread. `Flight::HostSdlSdkAudio` and Bazel `//:host_sdl_sdk_audio` populate and execute the exact emitted `flight::types::AudioDeviceBackend` record as a build-tree preview. The native sound example runs compiler-generated copies of Flight's procedural PCM calculations through this path under both CMake and Bazel. Compiler source remapping and the encoded-audio decoding boundary still gate the full upstream sound module. |

## Integration and release gates

- The exact generated inventory, refusal ledger, initialization plan, CMake header list, Bazel target, and structural
  member table are committed under `generated/` and reproduced by `npm run sdk:check`.
- CMake `Flight::SdkPreview` and Bazel `//:sdk_preview` expose every emitted header without claiming a finished SDK.
  The preview is intentionally not installed.
- Bazel automatically selects C++20 for its local Linux, macOS, BSD, and Windows host configuration; a default
  `bazel test //tests:runtime_test` no longer relies on a caller-supplied dialect flag. Cross and remote builds disable
  that host selection and provide C++20 through their registered toolchain.
- CMake `Flight::HostSdlSdkPreview` and Bazel `//:host_sdl_sdk_preview` collect every exact generated-record SDL
  adapter as one build-tree dependency without fabricating the compiler-refused aggregate `Host` record.
- `npm run sdk:compile` compiles every emitted header independently and writes the compiler-facing report to
  `out/sdk-header-compilation.json`.
- `npm run sdk:compile:headless` applies the same audit to the combined portable-runtime/headless inventory. At the
  previous `993c280` pin, the composed profile emitted 1,049 modules: 90 more than the manifest-free floor. Of those additional
  headers, 12 compile and 78 advance to existing tuple, union, reference-conversion, aggregate-construction,
  typed-array-template, spatial-type, and type-spelling defects. Because every shared header is byte-identical to the
  full SDL inventory, its completed audit gives 715 passing and 334 failing headers.
- `npm run sdk:generate:sdl-gl` adds the exact Web string-alias and maintained SDL/OpenGL binding profiles. It emits
  1,081 modules, 32 more than the runtime/headless inventory. Of those additions, 23 compile independently and nine
  expose existing compiler defects. Its byte-identical subset of the full SDL audit is 738 passing and 343 failing.
- `npm run sdk:generate:sdl-wgpu` applies the Web string aliases and provider-owned WebGPU handle profile. It emits
  1,067 modules, 18 more than runtime/headless, and every added header compiles independently: 733 pass and 334 fail
  overall.
- `npm run sdk:generate:sdl` composes the GL, WebGPU, Canvas 2D, and SDL application profiles. At `fbfcc11` it emits
  1,161 modules and records 1,739 refusals: 960 direct emission and 779 propagated dependency refusals. Independent
  header compilation was not re-measured at this pin. Window, document, `HTMLElement`, animation-frame
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
  The portable dictionary mappings similarly clear those ambient names, and `bindings/web-types.json` now also
  elects the Canvas 2D contract itself, so the Canvas render-state modules no longer stop at
  `CanvasRenderingContext2D` at all. They advance to variant-alternative, union-evidence, and optional-construction
  failures that were invisible behind it. `OffscreenCanvasRenderingContext2D` and the browser image constructor
  values remain explicit provider boundaries.
- `npm run examples:generate` selects 103 native modules from all 34 pinned upstream example packages, mirroring
  Flight's WebGL build selection with an explicit, recorded `renderNative.ts` remap. At Flight `903f328` the examples
  also import `@flighthq/host-web/contract` without declaring it in their manifests, so the generator records that
  one package name as a second explicit addition and closes over its declared dependencies; without it the package
  graph rejects the module edge and no example inventory can be produced at all. No example
  module is dependency-closed yet. The SDL application profile removes every direct `window`, `document`, animation
  frame, keyboard, pointer, wheel, gamepad-button, and `DOMRect` refusal. The rectangle binding clears that ambient
  name from eleven selected roots; collision, scene-picking, shapes, and spatial now expose their next compiler or
  dependency boundary. Remaining direct names describe real work: Canvas 2D, richer HTML controls, media, and the
  sound example's `AudioContext`. The selected ledger contains 54 emission and 46
  dependency refusals; its dependency-first frontier remains 38 compiler emission failures, 33 propagated dependency
  failures, and 29 external-package initialization edges. Nineteen native renderer roots currently reach the compiler's
  unrepresented optional `Raster2DSurfaceProvider` reference domain. Seven application roots also expose a new
  compiler-generated `R[type]` ambient request even though no such external source type exists. The inventory is committed under
  `examples/upstream/generated/` and contains no duplicate SDK sources.
- `npm run runtime:oracle` executes TypeScript-valid source behavior under Node and compares it with the native
  runtime. It currently covers 54 cross-runtime observations.
- `npm run structural:oracle` generates the exact generic Entity write proxy through the pinned compiler, compiles
  the emitted headers, and executes an intercepted write against the working runtime.
- `npm run facets:oracle` is green at `fbfcc11`. The alias forward-declaration collision that kept it red at
  `9f6ce1c` -- `TrayWithImage` declared as a struct before being defined as `flight::FacetRef` -- is fixed upstream,
  and the emitted facets compile and preserve their static capability gates.
- `npm run compile:check` is green at `fbfcc11`: 202 emitted files across 201 fixtures compile. The nested
  array-binding default that emitted `std::make_tuple(1.0)` into
  `std::optional<flight::Array<double>>::value_or` at `9f6ce1c` is also fixed upstream. No downstream tuple-to-array
  conversion was added, which was the point of keeping it red.
- `npm run boolean:oracle` asserts the negation of a union through the truthiness conversion rather than through the
  variant. At `fbfcc11` the compiler emits `!flight::to_boolean(value)` where it previously emitted `!value`; the
  fixture expectation moved to the new spelling because a `std::variant` has no `operator!` and the new emission is
  the one that can compile. Its compile-and-run half is unchanged and still passes.
- `Flight::Sdk` remains blocked until every emitted header in the selected binding profile compiles. At that point it
  must be installed/exported through CMake and exposed through Bazel, then exercised as an installed consumer.
- The reciprocal lock cannot be completed solely in this checkout: after these commits land, flight-compiler must pin
  the landed flight-cpp revision and run its exact-pin emitted compile. The unreleased C++ ABI number also requires a
  coordinated decision before either repository advertises a release ABI; this checkout cannot bump it while current
  compiler output still asserts ABI 1.

## Compiler and host work still gating the SDK

Independent-header compilation was last measured at `9f6ce1c` and has not been re-run at `fbfcc11`; the paragraph
below therefore describes the previous pin and its counts should not be read as this checkout's.

The 29 portable and 44 SDL-profile native header failures recorded at `9f6ce1c` contain no missing flight-cpp runtime symbol.
Across the SDL inventory they comprise 16 incompatible structural assertions in adjustments, image-codec, and
spatial; 21 nominal, structural, optional, and `Record` conversions in binpack, font-formats, materials, media, mesh,
particles, physics3d, scene2d-formats, and skeleton2d; six recursive-alias declaration failures for `TiledLayer` and
`FlightDocumentValue`; and one incorrectly optionalized XML replacement-callback parameter. These must be corrected
in flight-compiler rather than rewritten in the generated tree. `sfnt_assembly.hpp` and
`particle_emitter_signals.hpp` passed their new `pad_end` and `Symbol(String)` calls and exposed the next conversion
failures in this list.

The detailed profile-frontier notes below record the earlier binding bring-up. Their intermediate counts are
historical; the current aggregate figures above come from complete `fbfcc11` regeneration.

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
