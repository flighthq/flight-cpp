# flight-compiler requests

Each file in this directory describes one compiler defect: a minimal TypeScript reproduction, the
required C++ shape, the affected Flight modules, and the workaround carried by this repository. The
patch manifest remains authoritative for pinning, equivalence arguments, verification, and expiry;
these request files are the concise upstream-facing reproductions.

The former monolithic request correspondence mixed multiple revisions, resolved integration
discussion, and unrelated runtime work. Its still-live compiler defects are represented here;
historical measurements remain available in git history and the maintained corpus record is
[generation-state.md](../generation-state.md).

## Patch-to-request index

Every `source-patches/compiler-*.patch` entry is mapped exactly once below.

| Carried patch | Compiler request |
| --- | --- |
| [`compiler-canvas-context-overload-result.patch`](../../source-patches/compiler-canvas-context-overload-result.patch) | [Literal-selected Canvas context result](compiler-canvas-context-overload-result.md) |
| [`compiler-closed-key-external-member-call.patch`](../../source-patches/compiler-closed-key-external-member-call.patch) | [Finite-key calls on a bound host object](closed-key-bound-member-call.md) |
| [`compiler-concrete-owner-union-row-assertions.patch`](../../source-patches/compiler-concrete-owner-union-row-assertions.patch) | [Concrete owner assertion from an open union](compiler-concrete-owner-union-row-assertions.md) |
| [`compiler-concrete-structural-union-view-selection.patch`](../../source-patches/compiler-concrete-structural-union-view-selection.patch) | [Concrete owner entering a structural-view union](compiler-concrete-structural-union-view-selection.md) |
| [`compiler-constructor-parameter-property-assignment.patch`](../../source-patches/compiler-constructor-parameter-property-assignment.patch) | [Constructor parameter properties are declared but never assigned](constructor-parameter-properties-dropped.md) |
| [`compiler-contextual-imported-union-owner.patch`](../../source-patches/compiler-contextual-imported-union-owner.patch) | [Contextual construction of an imported union](compiler-contextual-imported-union-owner.md) |
| [`compiler-erased-ref-weak-map-view.patch`](../../source-patches/compiler-erased-ref-weak-map-view.patch) | [Checked typed view of an erased WeakMap](compiler-erased-ref-weak-map-view.md) |
| [`compiler-error-stack-cause-presence.patch`](../../source-patches/compiler-error-stack-cause-presence.patch) | [Runtime-declared Error members and their presence tests](compiler-error-stack-cause-presence.md) |
| [`compiler-exact-variant-owner-priority.patch`](../../source-patches/compiler-exact-variant-owner-priority.patch) | [Exact variant owners must outrank shape matches](compiler-exact-variant-owner-priority.md) |
| [`compiler-external-optional-result-widening.patch`](../../source-patches/compiler-external-optional-result-widening.patch) | [External optional result widening](compiler-external-optional-result-widening.md) |
| [`compiler-flattened-optional-chain-union-storage.patch`](../../source-patches/compiler-flattened-optional-chain-union-storage.patch) | [Optional-chain payload union storage](compiler-flattened-optional-chain-union-storage.md) |
| [`compiler-generic-erased-map-reference-recovery.patch`](../../source-patches/compiler-generic-erased-map-reference-recovery.patch) | [Reference recovery from an erased Map](compiler-generic-erased-map-reference-recovery.md) |
| [`compiler-host-object-dynamic-index-refusal.patch`](../../source-patches/compiler-host-object-dynamic-index-refusal.patch) | [Dynamic index on a bound host object must refuse](host-object-dynamic-index.md) |
| [`compiler-host-timer-local-storage-carrier.patch`](../../source-patches/compiler-host-timer-local-storage-carrier.patch) | [A local holding a bound host call must be declared as the binding's result carrier](bound-host-timer-local-storage-carrier.md) |
| [`compiler-host-timer-return-type-handle.patch`](../../source-patches/compiler-host-timer-return-type-handle.patch) | [ReturnType of a bound host timer keeps its handle carrier](return-type-of-bound-external-in-union.md) |
| [`compiler-imported-alias-call-declaration-recovery.patch`](../../source-patches/compiler-imported-alias-call-declaration-recovery.patch) | [Recovering an exact call-result alias](compiler-imported-alias-call-declaration-recovery.md) |
| [`compiler-imported-alias-local-flow-abi.patch`](../../source-patches/compiler-imported-alias-local-flow-abi.patch) | [Exact imported alias through local flow](compiler-imported-alias-local-flow-abi.md) |
| [`compiler-imported-alias-outer-union-abi.patch`](../../source-patches/compiler-imported-alias-outer-union-abi.patch) | [Exact imported alias inside an outer union](compiler-imported-alias-outer-union-abi.md) |
| [`compiler-imported-alias-parameter-abi.patch`](../../source-patches/compiler-imported-alias-parameter-abi.patch) | [Exact imported alias at a parameter boundary](compiler-imported-alias-parameter-abi.md) |
| [`compiler-imported-readonly-view-owner-recovery.patch`](../../source-patches/compiler-imported-readonly-view-owner-recovery.patch) | [Imported owner recovery from readonly views](compiler-imported-readonly-view-owner-recovery.md) |
| [`compiler-imported-variant-owner-qualification.patch`](../../source-patches/compiler-imported-variant-owner-qualification.patch) | [Imported variant owner qualification](compiler-imported-variant-owner-qualification.md) |
| [`compiler-nested-import-forward-closure.patch`](../../source-patches/compiler-nested-import-forward-closure.patch) | [Forward declarations for nested imported aliases](compiler-nested-import-forward-closure.md) |
| [`compiler-nested-nullable-property-narrowed-null-test.patch`](../../source-patches/compiler-nested-nullable-property-narrowed-null-test.patch) | [A null test on a narrowed optional-nullable member must read its storage](nested-nullable-property-narrowed-null-test.md) |
| [`compiler-nullable-optional-parameter-narrowing.patch`](../../source-patches/compiler-nullable-optional-parameter-narrowing.patch) | [Nullable and optional storage needs two absence channels (parameter section)](nullable-optional-presence.md) |
| [`compiler-nullable-structural-row-widening.patch`](../../source-patches/compiler-nullable-structural-row-widening.patch) | [Nullable structural-row widening](compiler-nullable-structural-row-widening.md) |
| [`compiler-owner-preserving-named-write.patch`](../../source-patches/compiler-owner-preserving-named-write.patch) | [Owner-preserving dynamic named write](compiler-owner-preserving-named-write.md) |
| [`compiler-placeholder-graph-qualification.patch`](../../source-patches/compiler-placeholder-graph-qualification.patch) | [Partial-module placeholder qualification](partial-module-placeholder-qualification.md) |
| [`compiler-present-dual-sentinel-multivariant.patch`](../../source-patches/compiler-present-dual-sentinel-multivariant.patch) | [Present projection of a dual-sentinel multivariant](compiler-present-dual-sentinel-multivariant.md) |
| [`compiler-readonly-union-view-conversion.patch`](../../source-patches/compiler-readonly-union-view-conversion.patch) | [Nominal union to readonly-view union](compiler-readonly-union-view-conversion.md) |
| [`compiler-readonly-view-argument-substitution.patch`](../../source-patches/compiler-readonly-view-argument-substitution.patch) | [A callee's type parameter must not survive into an emitted structural row](compiler-generic-signal-row-projection.md) |
| [`compiler-scoped-emission-semantic-context.patch`](../../source-patches/compiler-scoped-emission-semantic-context.patch) | [Scoped emission must retain semantic context](compiler-scoped-emission-semantic-context.md) |
| [`compiler-sdk-lane-local-reexports.patch`](../../source-patches/compiler-sdk-lane-local-reexports.patch) | [SDK-local re-exports in an SDK export lane](compiler-sdk-lane-local-reexports.md) |
| [`compiler-structural-row-weak-keys.patch`](../../source-patches/compiler-structural-row-weak-keys.patch) | [Structural rows as WeakMap keys](compiler-structural-row-weak-keys.md) |
| [`compiler-structural-union-exact-native-owner.patch`](../../source-patches/compiler-structural-union-exact-native-owner.patch) | [Exact native owner recovery from a structural union](compiler-structural-union-exact-native-owner.md) |
| [`compiler-union-import-runtime-identity.patch`](../../source-patches/compiler-union-import-runtime-identity.patch) | [Canonical runtime identity across union imports](compiler-union-import-runtime-identity.md) |
| [`compiler-error-name-and-erased-error-reads.patch`](../../source-patches/compiler-error-name-and-erased-error-reads.patch) | [Error.name must be the runtime's instance accessor](compiler-error-name-instance-accessor.md), [An erased Error read needs a checked projection, and its `instanceof` guard needs lowering](compiler-erased-error-view-and-instanceof-guard.md) |
| [`compiler-unrepresented-error-member.patch`](../../source-patches/compiler-unrepresented-error-member.patch) | [An Error member the runtime does not carry must be refused, not emitted](compiler-unrepresented-error-member.md) |

## Active requests without a compiler patch

- [Variadic console calls with non-string arguments](console-variadic-arguments.md) —
  needs a declared console-formatting contract; blocks log.ts `_writeConsoleCaptureEntry`.
- [An object literal passed through an optional call must build the parameter's reference owner](optional-call-object-literal-into-reference-owner.md) —
  carried for socket by the override `socket-complete-lifecycle`.
- [Member reads on a collapsed union record must use one access form](collapsed-union-record-member-access.md) —
  carried for socket by the override `socket-guard-reporting`.
- [A catch body that awaits and reads its binding needs an owned caught value](catch-binding-across-await.md) —
  refuses `@flighthq/loader` `resourceLoader.ts`; no workaround.
- [Asserting a `Promise<unknown>` to `Promise<T>` needs a typed task view](generic-promise-assertion-to-type-parameter.md) —
  first refusal in `@flighthq/assets` `assetLibrary.ts`; carried by the override `assets-reject-task-template-parameter`.
- [`Promise.allSettled` needs a binding and a settled-result lowering](promise-all-settled-binding.md) —
  `@flighthq/assets` `loadAssetGroup`; no workaround.
- [`registerAssetLoader` stores a readonly adapter as a writable one (Flight source)](readonly-adapter-stored-as-writable.md) —
  `@flighthq/assets` `registerAssetLoader`; an upstream source question, no workaround.
- [A fresh array of `null` assigned to a nullable element union](fresh-null-array-into-nullable-element-union.md) —
  `@flighthq/registry` `createOrdinalTable`; a type-only source patch would carry it.
- [A numeric enum value is widened to a `number` union in flow](numeric-enum-flow-widened-to-number-union.md) —
  refuses `@flighthq/debug` `debug.ts` (`enableDebug`); no workaround.
- [Nullable storage of an imported alias is spelled by its expansion](nullable-alias-storage-spelled-by-expansion.md) —
  `@flighthq/debug` `install_debug_sink` under the LogSink override; no workaround yet.
- [`@flighthq/log` native-compile defect inventory](log-native-compile-defects.md) — fifteen emitter
  defects make log.hpp fail clang once the console refusal is set aside; the log override stubs twelve
  functions with throws, so log parity is not honest.
- [A binding initialized from a non-null-asserted collection get is present](non-null-asserted-collection-get-binding.md) —
  carried for log by the source patch `log-buffered-sink-state-explicit-guard`.
- [Emit declared interface heritage so derived references widen to their base](interface-heritage-widening.md) —
  carried by the emission repairs `adjustments-color-matrix-interface-heritage` and `adjustments-tint-interface-heritage`.
- [Narrow an optional local in the else-branch of a negated truthiness test](negated-truthiness-else-narrowing.md) —
  carried by the source patch `node-bounds-rectangle-explicit-narrowing`.
- [A reference-backed outcome variant is narrowed as if it were a record](reference-backed-outcome-variant-narrowing.md) —
  a bound capability's `keys()` result is read as `result->reason` on a `std::variant`, so
  `@flighthq/preferences` refuses at emission and its header compiles nowhere; carried by the
  hand-written `preferences-storage-outcome-dispatch` override, which declares its own JSON boundary.
- [`Object.assign` does not lower onto an ordinary structural target](object-assign-onto-structural-record.md) —
  `Record` and complete callable-object construction lower, but an ordinary closed interface or entity
  target does not; blocks Scene3D materialization and notification subscription construction.
- [Typed-array static `from` needs a flight-cpp binding](typed-array-static-from-binding.md) —
  five direct refusals reach the missing ambient member, including `scene3d-formats/gltfSkins.ts`; that
  length-only array-like source also needs lowering beyond the represented-range runtime operation,
  while OBJ's two calls expose a following exact readonly-union insertion boundary.
- [An `undefined` logical-OR fallback loses the present value domain](logical-or-undefined-fallback-evidence.md) —
  seven P-3D2 name/group sites refuse on `string || undefined`, while the same pinned fixtures with
  `null` or a concrete string fallback emit.
- [A Math collection spread with fixed operands needs a seeded fold](math-spread-fold-with-fixed-seed.md) —
  the pure range fold and ordinary array/Set spreads emit, but two P-3D2 COLLADA stride calculations
  lose the Math specialization when a fixed lower bound precedes the spread.
- [A length array needs proof before it becomes a dense carrier](dense-array-length-initialization-proof.md) —
  bare holes must remain refused; two of three P-3D2 length arrays fill every index before observation,
  while the COLLADA world-matrix array is honestly sparse.
- [Optional index/member flow must preserve the present payload](optional-index-member-present-flow.md) —
  an indexed string member call refuses for arrays and emits an invalid pre-unwrap for Records, while
  direct optional members, guarded locals, and indexed object-property reads provide passing controls.
- [A corpus-context structural row construction loses an exact Record spread](corpus-context-structural-row-record-spread.md) —
  `threeDsParse.ts` is a committed P-3D2 refusal, but its closest minimal form reaches an adjacent
  Record-spread rule and the split-module form emits; the page records the non-reproduction without
  inventing a narrower root cause.
- [An optional member must construct its declared Record value carrier](contextual-record-value-carrier-projection.md) —
  P-3D2 glTF loading refuses when an existing `Record<string, Uint8Array>` fills an optional
  `Record<string, ArrayLike<number>>` member; direct widening instead emits invalid invariant C++.
- [A WeakMap value may be a nullable reference carrier](nullable-reference-weak-map-value.md) —
  P-3D2 resource guards refuse `ImageResourceFailure | null` even though the runtime faithfully stores
  and distinguishes the nested optional carrier.
- [`Promise.allSettled` needs a static binding and settled-result lowering](promise-all-settled-binding-and-result-lowering.md) —
  the runtime aggregate and per-arm carriers already exist, but the compiler-owned `Promise` table
  omits the member; Scene3D's two discard-only calls need the binding, while assets also needs checked
  `status`/`reason` lowering.
- [A readonly structural row cannot yet enter `unknown`](structural-row-erasure-to-unknown.md) —
  P-3D2 calls the `unknown`-accepting `isMesh` predicate with `Readonly<Node3D>`; exact references can
  enter `Any`, but a row needs an identity- and native-owner-preserving structural alternative.
- [A readonly `Mesh` cannot assert writable `NodeAny` authority](readonly-mesh-to-writable-node-authority.md) —
  P-3D2 picking asserts a readonly mesh to a wider writable node only to read runtime state; readonly
  respelling removes the authority claim but still lacks owner-cell proof, so no permissive cast is a
  valid compiler fix.
- [An entity intersection arm and the value that fills it are sibling C++ carriers](entity-intersection-arm-sibling-carriers.md) —
  `@flighthq/tray` refuses at emission and none of its four headers compile; carried by the source patch
  `tray-create-result-and-runtime-owner`, which names the create-result arms once and narrows the
  entity's runtime slot, advancing the refusal to a member-presence narrow (`'error' in result`) that is
  still refused.
- [A member-presence narrow must select across every alternative that carries the member](narrowed-member-multi-alternative-selection.md) —
  `'error' in result` over the four failure arms of `TrayCreateCapabilityResult` narrows to three, and
  the emitter requires exactly one C++ alternative, so `@flighthq/tray` refuses at emission. No
  workaround carried: the discriminant rewrite answers a different question and would be a source change
  made solely for the emitter.
- [An async closure needs the coroutine lowering a module-level function gets](async-closure-coroutine-lowering.md) —
  an `async release()` inside an object literal and an `async () =>` handed to `then` are both refused;
  three functions in `@flighthq/tray`. No workaround carried.
- [A union-member test needs the binding's carrier to still be a variant](union-member-test-without-variant-binding.md) —
  fires on the *shared* helper of two families, so one rule holds ten notification functions and
  `startTrayIconAnimation`; reduction attempts are recorded because four of them reached a neighbouring
  rule instead.
- [An assertion to a type parameter has no owner to cast to](generic-outcome-assertion-without-heritage.md) —
  `invokeUpdate<Provider, Result extends { readonly outcome: string }>` builds an arm and asserts it to
  `Result`; distinguished from the three declared-heritage requests, because here the destination is a
  parameter and no identity is at stake.
- [An object literal returning into an outcome union is emitted as a record initializer](object-literal-into-outcome-variant.md) —
  the mirror of the reference-backed read-side request; four of `@flighthq/midi`'s seven headers fail on
  one line, and `@flighthq/log` records the same construct already.
- [A captured referent mutated after suspension needs a shared reference representation](captured-referent-mutation.md) —
  `@flighthq/midi`'s `midiPort.ts` mutates a captured state record from a settlement callback; copying
  the record would compile and be wrong.
- [An optional property call needs its callable's result evidence](optional-property-call-result-evidence.md) —
  `runtime.ownerSubscriptions?.delete(subscription)`; the chain already carries the presence proof the
  refusal asks for.
- [Distinct source domains mapped to one carrier need their discriminator declared](image-source-union-domain-discriminator.md) —
  `@flighthq/video` refuses because six element domains map to `flight::host_sdl::ImageSource`, whose
  runtime carrier **already carries `ImageSourceKind`** for exactly this; the binding schema simply has
  no field to declare it. Same profile family as `render-wgpu`'s image-source refusals.
- [TypeScript `instanceof` is emitted verbatim as C++ text](instanceof-lowered-verbatim.md) — `@flighthq/audio`
  cannot parse `audio_resource_from.hpp:102`; surfaced once the module emitted further.
- [A lambda whose fallback is absence is declared to return the present type](logical-or-lambda-return-type.md) —
  `blob.type || undefined` becomes a `String`-returning lambda with `return std::nullopt`; the audio
  override has hand-fixed this shape since the pin before last.
- [Equivalent structural payloads must share one generic owner](media-audio-mixer-runtime-set-union-conversion.md) —
  media's `Set` and Scene3D resources' `Signal` become distinct C++ owners because equivalent structural
  payloads retain different emitter-chosen or nominal row names.
- [A WeakMap value needs a representable C++ type, and naming the literal is not enough](media-weakmap-value-representation.md) —
  `@flighthq/media`'s `videoChannel.ts`; records the measured negative result that naming the literal with
  its declared interface does not clear the refusal.
- [A keyed `typeof` over a closed capability could enumerate its key set](keyed-typeof-capability-presence.md) —
  `@flighthq/lifecycle`'s `typeof hostLifecycle[operation] === 'function'`, where the key set is declared
  and enumerable; recorded from the module override's measurement, not re-measured here, and the
  provenance is stated in the request.
- [A timer handle assigned to a source-declared `number` slot](timer-handle-in-a-declared-number-slot.md) —
  the third position of the host-timer family; `@flighthq/input` emits four invalid sites, and the
  override that shadows it drops the repeat behavior the source documents.
- [Object key iteration needs the key evidence the type already carries](layout-object-key-iteration-closed-keys.md) —
  `@flighthq/layout`'s `anchorLayout.ts` refuses a `for…in` whose operand is a closed declared shape.
- [An assertion to `T | null` must name which alternative it means](assertion-to-nullable-variant-alternative.md) —
  `flexLayout.ts` and `gridLayout.ts` refuse the same construct; the message's own target spelling suggests
  the count is the emitter's reading of one nullable alternative as several.
- [An assertion from the erased runtime slot to a module-local runtime reads members it does not declare](asserted-erased-runtime-owner.md) —
  `@flighthq/app`'s `appRenderView.ts`; the tray lane's fix narrows the slot in the declaration, which is
  the shape this request points at.
- [Union carriers need canonical alternative order](canonical-union-carrier-order.md) — identical
  named unions currently become incompatible `std::variant` types at array callback boundaries.
- [Nullable and optional storage needs two absence channels](nullable-optional-presence.md) — one
  `std::optional` cannot distinguish source `null` from absent `undefined`.
- [Move-only structural assertions should not require derived cells](move-only-structural-assertions.md) —
  a row used only for identity and transport never reads the cells the current proof rejects.
- [Recursive type-evidence lowering must terminate](recursive-type-evidence-nontermination.md) — scoped
  package runs are the current operational workaround for the full-graph recursion/scaling failure.
- [Computed `Symbol` cells need generated bindings](computed-symbol-cell-bindings.md) — generation
  fails closed for widening, but symbol reads still need an upstream contract.
- [Guarded external optionals must unwrap before member lowering](guarded-external-optional-member-view.md) —
  the SDL/GL oracle intentionally carries the failing probe.
- [Guarded readonly optional rows must stay readonly across helper calls](guarded-readonly-optional-helper-call.md) —
  a proven-present readonly row is currently reconstructed through an invalid writable view.
- [Animation track: number-carrier discrimination and optional-easing
  lowering](animation-track-number-carrier-and-optional-easing.md) — `animationTrack.ts` emits a
  PARTIAL header missing five declarations, and its eight dependents are refused with it.
- [A generic key-remapping mapped type must not erase to an opaque
  value](generic-mapped-type-erased-to-any.md) — `NumericProps<T>` becomes `flight::Any`, so every use
  of its key set compiles against a carrier with no keys.
- [A type assertion must not lower to an unchecked
  `static_cast`](assertion-lowered-to-unchecked-static-cast.md) — `x as T` becomes
  `static_cast<T>(x)`, which an erased carrier cannot serve and which the runtime refuses to coerce.
- [Readonly structural owners must enter their exact nominal union arm](readonly-structural-owner-to-nominal-union.md) —
  a retained concrete owner is not recovered when its readonly row enters `TextureLike`.
- [Flattened interface refinements need an identity-preserving carrier](flattened-interface-refinement-identity.md) —
  independent emitted structs cannot represent a `Texture2D` → `RenderTexture` refinement.
- [Named union shapes must survive `Partial<T>`](named-union-partial-shape.md) — the emitter
  refuses a mapped partial even though every concrete union arm has a closed object shape.
- [Named interface shapes must survive `Partial<T>`](named-interface-partial-shape.md) — the emitter
  refuses a mapped partial even though the named interface has one closed generated object row.
- [A callable's parameter pack needs a typed buffer](callable-pack-argument-buffer.md) — debounce and
  throttle connectors cannot buffer a slot's arguments; carried by the signals throttle override.
- [Compound `%=` on a number must lower to `fmod`](compound-remainder-on-numbers.md) — the compound
  form is emitted as a C++ `double` remainder; carried by the same override.
- [A locally declared handler needs callable identity](local-handler-callable-identity.md) — an erased
  local lambda cannot be disconnected; carried by the same override.
- [A generic owner entering a widest-signature container must keep its
  identity](generic-owner-into-widest-container.md) — `connectSignalTracked` stays refused; a copy in
  `SignalScope.connections` would split identity and state, so no workaround is carried.
- [Partial row reads must materialize three-state returns](partial-row-three-state-return.md) — an
  `optional<Value>` row read cannot be returned directly as `variant<Value, Null, Undefined>`.
- [The runtime binding plan has no iterator-result type](external-binding-iterator-result-type-plan.md)
  — `IteratorResult` / `IteratorYieldResult` are unbound, so any direct `keys().next()` is refused at
  the binding stage; carried by the rewritten `glyphatlas-iterator-result-binding` source patch.
- [Interface heritage is not a C++ owner relation](compiler-interface-heritage-owner-widening.md) —
  a sub-interface value cannot widen into a super-interface slot across a nullable union; no faithful
  source spelling exists, so `glyphSource.ts`, `bitmapFontGlyphSource.ts`, and Scene3D's punctual-light
  factory stay refused; image resource resolution now records the same Bitmap/ImageResource-to-
  TextureSource identity boundary.
- [Browser URL and location values need runtime bindings](browser-url-location-runtime-bindings.md) —
  `@flighthq/image` cannot emit `imageResourceFrom.ts` because the runtime profile has `Blob` but no
  `location` value or URL construction/object-URL lifetime contract.
- [String conversion of an erased value needs a runtime contract](erased-value-stringification.md) —
  `String(cause)` over `unknown` emits an invalid `to_string(Any)` call in image resource failure
  handling; object conversion cannot be approximated without changing diagnostics.
- [intl is refused for carried union and absence evidence](compiler-intl-optional-and-union-carrier-evidence.md)
  — 7 of 9 `@flighthq/intl` modules; one is supplied by the intl-formatter-cache override, the rest
  are retained refusals.
- [An argument is passed without constructing its declared parameter type](compiler-union-and-literal-argument-construction.md)
  — a union argument to an overloaded call, and an object literal into an `std::optional` parameter;
  recorded so they are not confused with intl's own refusals.
- [`typeof` on a recursively union-typed value lowers to a constant comparison](compiler-typeof-narrowing-on-recursive-union.md)
  — `@flighthq/tokens`; the existing tokens-string-narrowing-binding patch no longer helps at a627849
  because it reaches the same fold.
- [Readonly structural sequences stored as owning arrays](readonly-sequence-owning-array.md) — swf
  timeline frames; font-formats CFF charstrings (supplied by override).
- [Callables as arms of a contextual union](callable-arm-in-contextual-union.md) — four swf tag handlers.
- [Narrowing must prove the payload present](narrowed-union-presence-proof.md) — swf parse-option merge;
  font format inference (supplied by override).
- [Intersections with a shapeless conjunct](intersection-with-shapeless-conjunct.md) — swf morph shapes.
- [Row assertions to undeclared members](assertion-to-undeclared-row-member.md) — swf authored bounds.
- [Unbounded spread into a rest call](unbounded-spread-into-rest-call.md) — swf tag-stream test helper.
- [Distinct union domains on one carrier](distinct-union-domains-one-carrier.md) — font-formats test helper.
- [`for...of` over a string needs a code-point iteration runtime contract](compiler-string-for-of-codepoint-iteration.md)
  — blocks `@flighthq/textinput`'s editing module and two `@flighthq/bitmaptext` modules; no workaround is
  carried, because a hand-rewritten index loop is not equivalent for surrogate pairs.
- [A class instance passed as the interface it implements must keep its
  identity](class-instance-as-implemented-interface.md) — swf hands its `SwfReader` to every tag handler.
- [A partially refused module's retained declarations must keep their names in
  scope](partial-module-placeholder-qualification.md) — the swf video handler placeholder.
- [Calls to overloaded functions lower through the selected overload](source-overload-call-through-selected-signature.md) —
  tween's timer and swf's texture construction.
