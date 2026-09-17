# flight-compiler package graph review

The maintained downstream checklist now lives in [flight-compiler adoption status](flight-compiler-adoption.md).

The current checkout pins Flight `903f328` and flight-compiler `fbfcc11`. Both revisions are recorded in
[`dependencies.lock.json`](../dependencies.lock.json), and the maintained status document records the active counts
and remaining ownership. The section immediately below is the current round; everything after it is the historical
record of the earlier `993c280` and `9f6ce1c` handoffs.

## Round of 2026-09-17: the four downstream asks are implemented

`agents/flight-cpp-adoption.md` at `fbfcc11` named four items that could only land here. All four are in this
checkout, with native tests and, where the compiler already spells them, a measured effect on the ledger.

### Landed and already elected through a binding profile

- **`CanvasRenderingContext2D`, `CanvasGradient`, `CanvasPattern`, `DOMMatrix`.** `flight/canvas_2d.hpp` implements
  the half of the Canvas 2D contract that is device-independent -- the drawing-state stack, the current
  transformation matrix, path construction, dash lists, gradient stops, pattern parameters, and the settings a
  context was created with -- and delegates pixels to a `Canvas2DRasterizer` a host supplies. There is no rasterizer
  in flight-cpp and no default no-op one: a context created without a rasterizer reports `isContextLost()` and
  throws from every operation that would have produced or read pixels, while continuing to maintain the state the
  runtime owns. Path segments retain the transform in force when each was added, so a transformed arc stays an exact
  arc rather than being flattened at a tolerance the runtime cannot choose. `bindings/web-types.json` elects all four
  types. **Measured: the `CanvasRenderingContext2D` direct refusal goes from 58 modules to 0.**
- **`structuredClone`.** `flight/structured_clone.hpp` deep-copies the runtime's own value domain, preserving shared
  references and cycles, and refusing symbols and callables with `DataCloneError` as JavaScript does. A type with no
  clone definition is a compile-time refusal naming `structured_clone_traits`, not a shallow copy: C++20 cannot
  enumerate an aggregate's members, so a permissive default would be a shallow copy wearing a deep copy's name.
  Elected in `bindings/runtime.json`. **Measured: all three `@flighthq/snapshot` refusals clear.**
- **Per-arm settled results.** `flight::TaskFulfillment<T>` and `flight::TaskRejectionResult` are the two arms
  separately, with `status` spelled as the string the source narrows on and `as_fulfilled`/`as_rejected` to move from
  the union without inventing the member the other arm has. Elected in `bindings/runtime.json` as
  `PromiseFulfilledResult` and `PromiseRejectedResult`. **Measured: both modules clear.**

The whole `runtime external symbol binding plan is incomplete` family falls from **96 modules to 36** on the complete
SDL profile. Sixteen modules emit outright; the rest advance to their next blocker, which is the lower-bound property
the coverage plan describes rather than a disappointment. The profile moves from 1,145 to **1,161 of 2,900** emitted
modules, with refusals from 1,755 to 1,739 and direct refusals from 979 to 960. No module newly refuses.

### Landed and waiting on compiler election

- **The erased dynamic value is `flight::Any`** (`flight/any.hpp`). It is a closed variant over every ECMAScript
  language type this runtime has -- `undefined`, `null`, boolean, number, string, symbol, object reference, callable,
  and an opaque host value -- so a position written `unknown` that holds a number stays a number instead of being
  misstated as `flight::Ref<void>`. An object reference retains its concrete type and comes back only at that type.
  `typeof` reports the language's own answers, `===` never coerces, `SameValueZero` matches NaN with NaN so an erased
  value can key a `Map`, and `Object.is` separates the zeroes. The relational comparison is implemented exactly over
  the primitive domain and throws `TypeError` for a symbol, object, callable, or host operand rather than fabricating
  a `ToPrimitive` result the runtime has no prototype chain to obtain. The one gap is stated rather than faked: there
  is no `bigint` alternative, because the runtime has no arbitrary-precision integer.

  **Missing and present-but-absent stay distinct**, which was the explicit requirement. `Any` holds `undefined` as a
  present value; `flight::AnySlot` (`std::optional<Any>`) is the absence of an entry. `Record<K, Any>::get` returns
  the latter, so a key written with `undefined` and a key never written are different results from the same call, and
  `has` agrees. A native test asserts exactly that pair.

  **This cannot be elected from a binding profile, so it needs a compiler change.** `emitTypeCpp` maps
  `IrType { kind: 'unknown' }` to the literal `'auto'` for every `source` other than `this` and `object`
  (`packages/compiler-backend-cpp/src/cppCompilerBackend.ts`, the `case 'unknown':` arm), and
  `assertCppOutputHasNoUnresolvedTypePlaceholder` then refuses the module. There is no `sourceName` for `any` or
  `unknown`, so `flight-cpp-external-bindings/1` has no way to reach that arm.

  **The change is four lines and it works.** Verified here, not assumed: a local, never-committed patch to that arm --
  `if (type.source === 'any' || type.source === 'unknown') { context.includes.add('flight/any.hpp'); return 'flight::Any'; }`,
  left after the `this` and `object` cases so unresolved-alias residue still yields `auto` and stays refused --
  turns a fixture carrying the corpus's three named blockers into an emitted header that compiles and runs against
  this runtime:

  ```cpp
  struct AnimationChannel : public flight::ReferenceEnabled {
    flight::Any target_ref;
    flight::Array<double> values;
  };

  using NativeWindowHandle = flight::Any;

  struct NodeInteractiveStateBinding : public flight::ReferenceEnabled {
    flight::Any data;
    NativeWindowHandle handle;
  };
  ```

  The compiled fixture reads `42` back out of `targetRef` as a number with `typeof` `"number"`, reports a host window
  handle as `"object"`, and reports an unwritten `any` member as `undefined`. The patched compiler was reverted and
  rebuilt before this checkout's gates were run, so nothing here depends on it; what is recorded is that the
  downstream half is ready and the upstream half is small.

- **The erased-object symbol-keyed property view is `flight::AttachedProperties`**, with
  `flight::attached_properties(ref)` over a `Ref<Object>`, a `Ref<void>`, or a `StructuralRef`. It satisfies each
  part of the ask, and two of them were previously broken rather than merely absent:
  - *One owner per native object identity.* `detail::owner_for` kept a `static` registry **per object type**, so a
    typed projection and an erased `Ref<void>` projection of one object resolved two different owners and therefore
    two different sets of symbol properties. The registry is now one table keyed on the erased address.
  - *Attachments live for the object's lifetime.* The registry held owners weakly, so attachments died with the last
    transient row rather than with the object. It now holds the owner strongly and the object weakly; `NativeRowOwner`
    holds its object weakly in turn, and `StructuralRef` retains the object itself, so lifetimes are unchanged for
    existing callers while attachments now outlive the view that wrote them. An address reused by a later object
    never inherits the earlier object's entries, because expiry is checked on every lookup.
  - *A missing entry differs from a present `undefined`.* `get` returns `AnySlot`; `has` answers without reading.
  - *No copying.* Entries live in the owner, not in the object's storage, and nothing copies the object or its
    properties.

  **The spelling this compiler already emits works as-is.** `particle_emitter_signals.hpp` casts an erased
  `flight::Ref<void>` to `flight::Record<flight::Symbol, std::optional<flight::Ref<ParticleEmitterSignals>>>` and then
  uses presence-bearing `get`/`set`. That construction is now supported: it produces a view onto the object's
  attached properties rather than a copy, `get` returns `std::optional<Value>` so a missing entry and a present
  `std::optional` holding nothing are different answers, and the view shares one store with `AttachedProperties` and
  with a row's computed-symbol accessors because all three resolve the same attachment by object identity. **That
  header compiles at this pin**, so the compiler need not change anything unless it prefers a different spelling.

  Independent compilation of the complete SDL inventory is 1,088 of 1,161 headers. Of the 73 failures, 44 are only
  `SDL3/SDL_video.h: No such file or directory` -- SDL 3 development files are absent from the environment this audit
  ran in -- leaving 29 genuine generated-code defects, none of them in the contracts added this round.

### Checked against flight-cpp and deliberately not bound

- **`Function[value]`** (`@flighthq/effects-gl/glShaderTestHelper.ts`) is `new Function(source)()` -- evaluating
  JavaScript source text at runtime. flight-cpp has no evaluator and will not grow one to satisfy a test helper, so
  there is no target to name. This is a deliberate absence, not an oversight; the coverage plan's "check each against
  flight-cpp before adding a table entry" is answered "no target exists".
- **`globalThis[value]`** (five modules) is only ever reached as `globalThis.document` and `globalThis.navigator`.
  Those members are host-profile values -- `bindings/sdl-app.json` already binds `document` and `navigator` to
  `flight::host_sdl` objects -- so `globalThis` belongs in that profile as an object whose members forward to them,
  not in the portable runtime, which has no global scope to expose. It is not bound in this checkout because SDL 3
  development files are not available in this working environment, so the host header could not be compiled or
  tested here, and shipping an unverified host binding would be worse than naming the gap.

### Compiler-side defects this round surfaced

- **A local array literal over native-binding property reads loses its element type.**
  `const restored = [ctx.globalAlpha, ctx.lineWidth];` emits `flight::Array<auto` and refuses with
  `cpp-unresolved-type-placeholder`, although TypeScript types both properties `number` and the same expression
  emits correctly when returned directly. Reproduced against the Canvas 2D binding at `fbfcc11`; the Canvas 2D
  oracle works around it by returning the literal instead of binding it.
- **`Math.fround` and `Math.SQRT2` have no ambient member binding**, and five modules now stop there.
  `flight::fround` exists in `flight/math.hpp` and has since the numeric-conversion contract landed, so these are
  two missing rows in `cppFlightRuntimeAmbientMemberBindings`, not missing runtime work.
- **`captured referent mutation of ctx requires a shared C++ reference representation`** now blocks three
  Canvas 2D modules. A bound context is emitted as a by-value parameter, so a closure that captures it and writes an
  attribute cannot be represented. The runtime side is already shared -- copies of a context share one state stack,
  one rasterizer, and one identity -- so what is needed is the compiler's reference representation for a captured
  binding, not a different runtime carrier.
- **Upstream examples import `@flighthq/host-web/contract` without declaring it.** At Flight `903f328` the example
  package graph no longer validates: `examples/packages/awd2loading/src/render.webgl.ts` and its siblings import the
  Web host package while their manifests declare only `@flighthq/sdk`, and the package graph rejects a module edge to
  a package the owning package does not declare. `scripts/upstreamExampleGeneration.mjs` now records that one package
  name as an explicit addition, closes over its declared dependencies, and adds it to the dependency list of every
  example whose sources name it -- the same class of recorded remap as the existing `renderNative.ts` selector. The
  example inventory grows from 33 examples and 100 selected modules to 34 and 103. This is a Flight-side manifest
  gap rather than a compiler one; the downstream workaround is recorded so it can be removed when the manifests
  declare the dependency.


## Regression still present at 9f6ce1c

`npm run facets:oracle` still emits an invalid declaration pair for the existing conditional-facet fixture:

```cpp
struct TrayWithImage;
using TrayWithImage = flight::FacetRef<TrayIcon, tray_with_image_facet>;
```

The second declaration conflicts with the first, leaving the alias incomplete at every conversion and call site.
The runtime ABI and fixture passed at `993c280`; the failure appears in the compiler's module-reference
forward-declaration path. Alias targets must not receive record-style `struct` forward declarations. The focused
oracle reproduces the same failure at `9f6ce1c`; keep it red until the compiler emits a legal alias dependency order.

The same pin adds a second compiler-owned regression to the downstream aggregate gate. The `arrayBinding` golden
emits the nested tuple default `[1]` as:

```cpp
std::get<0>(array_pattern_value).value_or(std::make_tuple(1.0));
```

The optional contains `flight::Array<double>`, so GCC correctly rejects the tuple argument. This must emit the
represented array value instead. Adding a general tuple-to-Array conversion in flight-cpp would accept unrelated
target-shape errors and is not a valid runtime fix. `npm run compile:check` reproduces this as its sole failing
compiler fixture.

## Complete report sweep

The full graph processes all 154 SDK packages and 2,851 source modules, emits 950 dependency-closed portable
headers, and records 1,901 refused modules: 1,105 direct emission refusals and 796 propagated dependency refusals. The exact headers,
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
- shared live `MapIterator<T>` and `ArrayIterator<T>` cursors for `keys`, `values`, and `entries`, including
  insertion-order, deletion, post-creation insertion, copy identity, and sticky-exhaustion behavior;
- shared `AbortController`/`AbortSignal` state with first-reason retention, listener dispatch/removal, and
  `throw_if_aborted`, covered by a live compiler-versus-Node oracle;
- immutable shared `Blob` bytes with typed-array construction, slicing, MIME normalization, text decoding, and
  `ArrayBuffer` conversion, plus exact `BlobPart` and `BlobPropertyBag` aliases selected through the runtime profile
  and covered by a live compiler-versus-Node oracle;
- URI component and browser base64 globals with UTF-8, binary-string, malformed-input, padding, and whitespace
  semantics covered by live compiler-versus-Node oracles;
- shared readable/writable stream and async-iterable carriers, including compiler-emitted writer operations and
  native task callbacks; these admit three more independently compiling SDK headers;
- shared decoded-PCM `AudioBuffer` storage with live channel views and bounded channel copies, plus its named
  `AudioBufferOptions` type, selected through the runtime profile and exercised by compiler-emitted native code;
- CPU-backed `ImageData` with exact live `Uint8ClampedArray` reuse, Web IDL dimensions, color-space values, named
  DOM exceptions, and shared identity, plus complete provider-neutral `TextMetrics`, `ImageEncodeOptions`,
  `PermissionDescriptor`, and `PositionOptions` values exercised by live compiler fixtures;
- `TextEncoder` scalar UTF-8 and unpaired-surrogate replacement, whose newly admitted SWF helper compiles after
  shared runtime containers gained JavaScript-compatible logical constness;
- numeric conversion and prefix parsing, ECMAScript radix number formatting, `String.padEnd`, safe-integer checks,
  object keys/values, global and non-global symbols, URL protocol parsing, regular expressions, and a deterministic
  Intl baseline;
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
C++ well-formedness. With GCC 15.2, 921 of the 950 portable headers compile independently and 29 fail. The composed
SDL profile emits 1,093 modules, of which 1,049 compile and 44 fail. The new downstream `number_to_string`,
`String::pad_end`, and `Symbol(String)` contracts remove every missing-runtime-symbol diagnostic. The current native
report is entirely emission defects that cannot be repaired by adding another runtime symbol:

- 16 incompatible structural assertions across adjustments, image-codec, and spatial;
- 21 nominal, structural, optional, and `Record` conversions across binpack, font-formats, materials, media, mesh,
  particles, physics3d, scene2d-formats, and skeleton2d;
- six recursive-alias declaration failures for `TiledLayer` and `FlightDocumentValue`; and
- one incorrectly optionalized XML replacement-callback parameter.

The stricter compiler newly refuses 16 direct roots that the previous portable sweep emitted. Five need equivalent
source-union evidence, five expose generic typed-array backing domains that are not represented by the concrete C++
aliases, three contain unresolved callable result types, one needs named structural-row construction, and two retain
an unresolved alternative in a scene-light-selection union. Their dependency refusals account for the rest of the
35 previously emitted modules absent from the full SDL profile. Returning `MapIterator<T>.next().value` as
`T | undefined` also reaches `contextual optionalSingle construction requires expression type evidence`; the exact
iterator binding and its `done` path compile in the live runtime oracle.

The compiler-emitted RegExp fixture now verifies global `String.match`, substitution tokens, empty global matches,
and callback offsets against Node. One contextual callback edge remains: an unannotated second parameter for a
zero-capture `String.replace` callback currently emits as `flight::String`, although JavaScript supplies the numeric
match offset at that position. An explicit `offset: number` annotation emits the correct `double` and passes the
native oracle. Capture-aware callback typing must also preserve the runtime `undefined` value for unmatched groups;
the TypeScript library's broad callback signature does not expose that absence by itself.

The current compiler can now contextualize Flight's nested `GPUBlendComponent` literals, so flight-cpp binds
`GPUBlendComponent`, `GPUBlendState`, and `GPUStencilFaceState` to provider-neutral value carriers. Seven affected
modules advance to their next union-evidence, optional-callable, anonymous-property, or `Record`-spread failures.
The profile now also supplies buffer/texture descriptors, iterable extents, shared buffer/view sources, texel-copy
layouts, the complete bind-group resource and layout value family, and the render-pipeline descriptor family. Their
pass-through signatures and
declaration-ordered compiler fixtures compile. Three production scene3d-wgpu modules advance past their ambient
refusals: one reaches `typeOf` computation, one reaches dense-array length construction, and the inline `{ buffer }`
resource in `wgpuMeshPipeline.ts` reaches `anonymous object property buffer requires concrete C++ type evidence`.
`wgpuTestHelper.ts` no longer requests `GPURenderPipelineDescriptor`; it remains refused on its browser-image and
test-host names, so the full SDL profile still has 120 direct ambient-refusal records and emits 1,098 modules.
The shared SDL image-source profile now maps every image/video/bitmap/offscreen/SVG/frame constituent type to one
decoded RGBA owner with retained source kind and a cohesive weak policy. Direct ambient-refusal records fall again,
from 120 to 112, while the emitted closure remains 1,098. `glDraw.ts` now reaches contextual source-union evidence;
`wgpuDraw.ts` retains only its browser constructor values. Those values should select a compiler-lowered external
runtime-type test against the retained kind rather than require a browser DOM implementation.
Direct external descriptor literals also preserve source property order in C++ designators; arbitrary valid
TypeScript property order can therefore violate the target aggregate's declaration order. Contextual target types
and ordered-designator emission remain required for inline resource construction and property-order-independent
construction.

These diagnostics arise after the runtime includes resolve, and many occur in a header before later errors in that
header can be observed. The JSON report from `npm run sdk:compile` is the compact handoff surface for fixing them in
flight-compiler. Downstream source rewriting would obscure compiler provenance and produce a second, unversioned
transpiler, so the checked-in SDK remains the compiler's exact output plus its generated build/member inventories.

The SDL lifecycle profile now supplies the exact `CustomEvent<T>`, navigation timing, document visibility, and focus
carriers. The headless profile also maps `PerformanceEntry` and `PerformanceEntryList` to the standard value and
base-entry array while its native navigation query returns the narrower navigation-timing array used by Flight. Two
compiler contracts remain before that module can use them faithfully. Generated object-valued
`event.detail` access must dereference `Ref<T>` rather than emit `event.detail.member`, and copied callable values must
retain source identity so `removeEventListener(type, listener)` can remove the registration added before the listener
is captured by the returned unsubscribe closure. Downstream now provides `flight::Function<Signature>`, whose copies
share a weakly recoverable identity, and the SDL element/document/window stores deduplicate and remove it using the
DOM type/callback/capture key. flight-compiler must emit that carrier for JavaScript function values; the maintained
profile cannot bind removal while it still emits `std::function`. A diagnostic removal mapping moves `lifecycle.ts`
to the separate "WeakMap value requires a proven C++ representation" refusal, confirming the next boundary.

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

The runtime profile now binds `Iterable<T>` to an owner-preserving `flight::Iterable<T>` carrier. It removes this
ambient type from 14 full-SDK roots and from the snapshot and spritesheet example roots. Ten of the full-SDK roots
advance immediately to named compiler failures: anonymous-object evidence, target-name candidate identity,
source-union evidence, `typeOf` lowering, optional construction, captured reference representation, structural-union
representation, or optional-variant construction. In the selected examples, spritesheet advances to its Canvas
types and snapshot reaches `ambient value Array member from has no C++ binding`. `Array[value]` is already reserved
by the compiler runtime plan, so an external member mapping for `from` is rejected as a duplicate; the compiler must
elect the existing `flight::array_from` helper in that intrinsic plan.

The latest compiler still records direct refusals for all seven member families above. Newly emitted generic typed
arrays also expose a distinct representation issue: TypeScript's current library models backing storage as
`Uint8Array<ArrayBufferLike>` and peers, but C++ emission applies the concrete aliases as templates. The runtime must
not redefine each concrete element alias as a generic backing template; the compiler must either erase a proven
backing parameter to the existing `ArrayBufferLike` member or emit a compatible generic carrier deliberately.

The runtime profile maps both spaces for `RangeError` and `TypeError` to the semantic runtime classes in
`flight/error.hpp`. Current generated headers no longer fail by trying to construct `std::range_error` from
`flight::String`; the remaining failures are later optional-unwrapping or missing-symbol emission defects.

The Web-types profile maps both `ImageData` spaces, `ImageDataArray`, and `ImageDataSettings` to the downstream
CPU-backed RGBA carrier. This removes `ImageData` from every direct refusal and moves bitmap/effects modules to
their `CanvasRenderingContext2D` provider boundary and image-codec modules to their bitmap/canvas provider boundary.
The composed SDL direct ambient frontier falls from 124 to 123 modules. The downstream `DOMException` carrier has
exact message, name, and legacy-code values, but its ambient constructor remains deliberately unbound: the current
`typeof DOMException !== 'undefined' && error instanceof DOMException` path emits C++ `instanceof` syntax and member
access on the erased catch value. flight-compiler needs a represented external runtime-type-test operation before
that global can be selected soundly.

The same profile removes `TextMetrics`, `ImageEncodeOptions`, `PermissionDescriptor`, and `PositionOptions` from
every direct refusal. Each affected root advances to its remaining Canvas, offscreen-canvas, geolocation,
permissions, media, or wake-lock provider contract. The emitted closure remains 1,098 because no root was otherwise
dependency-complete; these portable values are ready for those host adapters without choosing their implementation.

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

At the current pins, dependency closure emits 0 of those 100 modules. The linked ledger contains 46 dependency and
54 emission refusal entries. Forty example modules are held by the refused `@flighthq/sdk` barrel and
20 renderer modules are held by the refused `@flighthq/host-web/contract` path through `webGraphicsHost`. The exact
per-module evidence is committed in `examples/upstream/generated/refusals.json`.

The separate frontier ledger deliberately compiles each selected example without its package dependencies, so its
29 missing package-evaluation entries are boundary markers rather than claims that the full graph omitted those
packages. It records 33 dependency and 38 direct emission boundaries, led by contextual optional construction,
captured referent mutation, contextual typing for empty arrays, and package evaluation. Seven application roots now
report `runtime external symbol binding plan is incomplete (missing: R[type])`. No `R` external type exists in the
pinned Flight sources, so this is synthesized generic evidence escaping into the external-binding plan and cannot be
made sound by adding a downstream binding. The SDL
application profile resolves every direct keyboard, pointer, wheel, gamepad-button, rectangle, DOM attachment,
window, animation-frame, generic iterable, and concrete HTML control ambient in the chosen lane. Those HTML bindings
expose intersection, `typeOf`, contextual union, multi-variant, optional construction, and empty-array compiler
failures in eight UI-heavy examples. Remaining direct names describe Canvas 2D, media, and the sound example's
`AudioContext`; each stays explicit until
its compiler or host contract exists.

The remaining integration request is a first-class source/package remap in the programmatic graph API. Flight uses
build-time renderer aliases and its examples import Web host providers. `flight-cpp` can name selected native
renderer and host modules, but it should not edit upstream import strings or maintain a second TypeScript transform
to do so. A remap must be importer-specific, recorded in provenance, participate in both type and module-evaluation
resolution, and continue to verify that the replacement exports every requested name. With that contract, the
handwritten SDL package can replace Web lifecycle/input/context providers while generated Flight GL modules retain
renderer ownership.

The SDL cursor side of that replacement is now concrete. `Flight::HostSdlSdkCursor` and Bazel
`//:host_sdl_sdk_cursor` populate the compiler-emitted `flight::types::CursorBackend` record and execute it against
SDL's main-thread system-cursor API. The interaction and sound examples still import `createWebCursorBackend`; the
remap must select a native provider with the same requested export before those entry points can consume this
adapter without source rewriting.

The SDL window side now also populates emitted `ApplicationVisibilityBackend` and the request/exit portion of
`FullscreenBackend` through `Flight::HostSdlSdkWindow`. It also populates the dependency-closed input-target,
focus, file-drop, and pointer-lock records; their returned release closures do not require callback comparison. The optional fullscreen listener fields and the required
`ApplicationExitBackend` listener pair are deliberately left unwired: the provider must remove the same JavaScript
function object passed at subscription time, which cannot be recovered from separately copied `std::function`
values. Emitting the prepared `flight::Function` carrier unlocks those exact adapters.

The dependency-closed `ClipboardTextBackend` is also populated by `Flight::HostSdlSdkClipboard`; SDL covers its
clear, presence, UTF-8 read, and UTF-8 write methods without a new compiler contract. Rich clipboard records remain
absent until a native provider implements those separate capabilities.

`Flight::HostSdlSdkPlatform` now populates the dependency-closed `PlatformBackend` and writes SDL/native facts into
the exact caller-owned `PlatformInfo`. It needs no additional compiler behavior and leaves unavailable version,
build, and distribution fields at their specified sentinels.

`Flight::HostSdlSdkDevice` also populates the dependency-closed `DeviceBackend`. SDL supplies input-device presence,
desktop-display dimensions and pixel ratio, per-window safe-area geometry, CPU count, configured memory, and native
platform identity; unavailable identity and environment fields retain the upstream sentinel contract. This backend
needs no further compiler work and resolves its SDL window id on each dynamic read.

`Flight::HostSdlSdkHaptics` populates the emitted `HapticsBackend` without compiler changes. It uses SDL gamepad
rumble for the continuous and named feedback operations, accepts the emitted optional impact intensity, dynamically
tracks a capable connected device, and leaves
multi-step pattern/waveform capability absent because SDL has no corresponding timed primitive.

`Flight::HostSdlSdkScreen` populates the dependency-closed `ScreenQueryBackend`, `ScreenDetailsBackend`, and
`ScreenChangeBackend` without compiler changes. SDL provides native multi-display enumeration, permission-free
access, and display events. The adapter caches the prior display snapshots, constructs the generated readonly
structural change-event view, and returns an idempotent release closure for each subscription.

`Flight::HostSdlSdkKeyboard` populates the emitted soft-keyboard info, visibility, and change records without
compiler changes. SDL's shown/hidden events and returned release closure satisfy the subscription contract directly;
unsupported desktop drivers return the existing failure-domain values. Platform-only style and layout setters remain
absent capabilities.

## Host boundary

The manifest-free generation remains the portable floor. Browser, media, Node, and graphics handles require explicit
binding profiles. SDL owns lifecycle and GL, Vulkan, or WebGPU surface acquisition; generated Flight renderer
packages own rendering behavior. These host bindings increase the emitted module set from 950 to 1,093, while the
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
object handles, context attributes, a weakly recoverable image-source carrier, and the standard anisotropy,
float-buffer/filtering, ASTC, BPTC, ETC, PVRTC, RGTC, S3TC, and S3TC-sRGB extension domains. Composed with the runtime
and exact Web string-alias profiles it emits 1,081
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

The added extension domains remove every direct refusal for those interfaces. `glCompressedTexture.ts` advances to
dual-sentinel optional-chain lowering, `glRenderTarget.ts` advances to nullish-coalescing presence lowering, and
`glEnvironmentIblBake.ts` advances to an unrelated contextual `flight::Map` union conversion. The dependency-closed
header count remains 1,098 until those compiler-owned boundaries move.

Composing `bindings/sdl-app.json` with the maintained renderer profiles admits the represented window, document,
`HTMLElement`, keyboard, mouse, pointer, wheel, and animation-frame cancellation contracts; these now reach their
next compiler or package dependency failures in the full SDK graph. The runtime profile maps `PromiseLike<T>` to the
existing `flight::Task<T>` carrier; the dialog module consequently advances to the compiler's async-closure
coroutine-lowering refusal.

The same profile maps `DOMRect` to the host's complete eight-field `ClientRect`, which is populated from SDL logical
canvas dimensions. This removes the name from all eleven selected example roots that referenced it. Four of those
roots now stop directly at compiler-owned union, captured-reference, intersection, or SDK-dependency boundaries;
the others retain separate Canvas, listener-option, or HTML-control requirements. In the full SDK sweep, direct
ambient-binding refusals are now 111 modules, down from the original 242-module SDL/GL frontier, while the
dependency-closed total stays at 1,098 modules.

The runtime profile also binds the global `Boolean` value to a callable object with exact represented truthiness.
This supports both direct conversion and `filter(Boolean)` without overload erasure. `svgDocument.ts` now reaches
the existing `Number.parseFloat` intrinsic gap, whose downstream `flight::parse_float` operation is already present.
Closed `std::variant` unions use the same operation for logical negation. That removes the invalid `operator!`
diagnostic from all 22 affected SDL headers: `texture/sampler.ts` now compiles, while the other 21 expose the
existing optional-number unwrapping defect.

The runtime profile now maps `ArrayIterator<T>` to shared live cursors over `Array.keys`, `Array.values`, and
`Array.entries`. `tiledXmlParse.ts` moves immediately to the existing concrete typed-array backing error:
`flight::Uint32Array<flight::ArrayBuffer>` is emitted as though the concrete alias were a template.

The SDL profile now also supplies `Event`, `CompositionEvent`, `InputEvent`, `Gamepad`, `GamepadButton`,
`GamepadEvent`, `navigator.getGamepads()`, and `AddEventListenerOptions` with compiler-checked native carriers. SDL's
gamepad carrier exposes the standard `pressed`, `touched`, and `value` button fields and preserves them in each
polled snapshot. The listener option carrier represents optional capture, once, passive, and abort signal fields;
the shell removes once registrations before reentrant dispatch and removes signal-bound registrations on abort.
Filesystem and scene-resource roots now advance to awaited-union and source-union compiler failures, and four of the
five affected example apps advance to contextual-union or `typeOf` failures; tilemap retains only its Canvas 2D gap.
`packages/input/src/inputManager.ts` consequently reports only `EventTarget[type]`. A focused compiler probe with an
otherwise valid `EventTarget` binding fails with `flight-cpp type position retains unresolved auto placeholder:
std::function<auto` for `addEventListener` and `removeEventListener`. The compiler needs to materialize the listener's
event parameter and elect the downstream `flight::Function<Signature>` carrier. Its copies preserve identity, and
the SDL host already accepts it for add/remove operations with the DOM capture-key rule.

The provider-neutral `flighthq/flight-cpp/sdl-wgpu/1` profile now supplies typed shared identity for 18 WebGPU
object domains, exact adapter capability metadata, standard usage flags, and weak-key policies. The composed
`sdl-image` profile supplies its external-copy source. On its own the WebGPU profile adds
18 dependency-closed headers over runtime/headless and all 18 compile. Composed with SDL/GL and the application
shell, it raises the inventory from 1,081 to 1,098 headers; all 17 additions compile and direct ambient-refused
modules fall from the SDL/GL profile's 242 to 163. The device, origin, vertex, external-image, buffer, texture,
extent, shared-buffer-source, texel-copy, sampler, bind-group, and render-pipeline descriptors now have
compiler-checked native representations.
Optional source fields preserve their presence; binding the sampler advances
`wgpuRenderState.ts` to `dual-sentinel optional chaining requires presence projection lowering`. `wgpuHost.ts`
reaches contextual optional construction, while `wgpuExternalImageSource.ts`
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
