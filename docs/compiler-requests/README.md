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
| [`compiler-catch-recorded-slot-carrier.patch`](../../source-patches/compiler-catch-recorded-slot-carrier.patch) | [A slot a catch records into needs the exception carrier, whatever its initializer](catch-recorded-slot-carrier.md) |
| [`compiler-catch-handler-breadth.patch`](../../source-patches/compiler-catch-handler-breadth.patch) | [A `catch (error)` whose body continues must catch every thrown value](catch-handler-breadth-attempt-all.md) |
| [`compiler-concrete-owner-union-row-assertions.patch`](../../source-patches/compiler-concrete-owner-union-row-assertions.patch) | [Concrete owner assertion from an open union](compiler-concrete-owner-union-row-assertions.md) |
| [`compiler-concrete-structural-union-view-selection.patch`](../../source-patches/compiler-concrete-structural-union-view-selection.patch) | [Concrete owner entering a structural-view union](compiler-concrete-structural-union-view-selection.md) |
| [`compiler-constructor-parameter-property-assignment.patch`](../../source-patches/compiler-constructor-parameter-property-assignment.patch) | [Constructor parameter properties are declared but never assigned](constructor-parameter-properties-dropped.md) |
| [`compiler-indexed-element-presence-test.patch`](../../source-patches/compiler-indexed-element-presence-test.patch) | [A null test on an element of a nullable-element array tests the wrong optional layer](nullable-array-element-null-test.md) |
| [`compiler-constrained-type-parameter-union-slot.patch`](../../source-patches/compiler-constrained-type-parameter-union-slot.patch) | [A local typed by a constrained type parameter is stored as the constraint](constrained-type-parameter-local-storage.md) |
| [`compiler-dependent-callable-pack-reuse.patch`](../../source-patches/compiler-dependent-callable-pack-reuse.patch) | [A forwarded parameter pack is consumed more than once](forwarded-parameter-pack-reused.md) |
| [`compiler-generic-receiver-null-write.patch`](../../source-patches/compiler-generic-receiver-null-write.patch) | [A `null` written through a generic receiver must produce the member's absent state](generic-receiver-null-write-engages-optional.md) |
| [`compiler-dependent-erased-object-recovery-template.patch`](../../source-patches/compiler-dependent-erased-object-recovery-template.patch) | [Dependent erased-object recovery needs a template disambiguator](dependent-erased-object-recovery-template.md) |
| [`compiler-dependent-indexed-access-reference-carrier.patch`](../../source-patches/compiler-dependent-indexed-access-reference-carrier.patch) | [Dependent indexed access must read through the reference carrier](dependent-indexed-access-reference-carrier.md) |
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
| [`compiler-nul-string-literal.patch`](../../source-patches/compiler-nul-string-literal.patch) | [A source string containing U+0000 is emitted truncated](nul-string-literal-truncated.md) |
| [`compiler-nullable-optional-parameter-narrowing.patch`](../../source-patches/compiler-nullable-optional-parameter-narrowing.patch) | [Nullable and optional storage needs two absence channels (parameter section)](nullable-optional-presence.md) |
| [`compiler-nullable-structural-row-widening.patch`](../../source-patches/compiler-nullable-structural-row-widening.patch) | [Nullable structural-row widening](compiler-nullable-structural-row-widening.md) |
| [`compiler-nullish-assignment-statement-in-catch.patch`](../../source-patches/compiler-nullish-assignment-statement-in-catch.patch) | [A nullish assignment as a statement in a catch body is dropped](nullish-assignment-dropped-in-catch.md) |
| [`compiler-owner-preserving-named-write.patch`](../../source-patches/compiler-owner-preserving-named-write.patch) | [Owner-preserving dynamic named write](compiler-owner-preserving-named-write.md) |
| [`compiler-placeholder-graph-qualification.patch`](../../source-patches/compiler-placeholder-graph-qualification.patch) | [Partial-module placeholder qualification](partial-module-placeholder-qualification.md) |
| [`compiler-present-dual-sentinel-multivariant.patch`](../../source-patches/compiler-present-dual-sentinel-multivariant.patch) | [Present projection of a dual-sentinel multivariant](compiler-present-dual-sentinel-multivariant.md) |
| [`compiler-readonly-union-view-conversion.patch`](../../source-patches/compiler-readonly-union-view-conversion.patch) | [Nominal union to readonly-view union](compiler-readonly-union-view-conversion.md) |
| [`compiler-readonly-view-argument-substitution.patch`](../../source-patches/compiler-readonly-view-argument-substitution.patch) | [A callee's type parameter must not survive into an emitted structural row](compiler-generic-signal-row-projection.md) |
| [`compiler-runtime-instanceof-lowering.patch`](../../source-patches/compiler-runtime-instanceof-lowering.patch) | [TypeScript `instanceof` is emitted verbatim as C++ text](instanceof-lowered-verbatim.md) |
| [`compiler-profile-type-name-evidence.patch`](../../source-patches/compiler-profile-type-name-evidence.patch) | [A property union loses the guard's evidence](property-union-evidence-lost.md) |
| [`compiler-guarded-path-narrowing.patch`](../../source-patches/compiler-guarded-path-narrowing.patch) | [A property union loses the guard's evidence](property-union-evidence-lost.md) |
| [`compiler-catch-await-owned-carrier.patch`](../../source-patches/compiler-catch-await-owned-carrier.patch) | [A catch body that awaits and reads its binding needs an owned caught value](catch-binding-across-await.md) |
| [`compiler-promise-resolve-domain.patch`](../../source-patches/compiler-promise-resolve-domain.patch) | [A resolved task carries the literal it was given](task-resolved-value-domain-mismatch.md) |
| [`compiler-callable-arity-adaptation.patch`](../../source-patches/compiler-callable-arity-adaptation.patch) | [A function with fewer parameters is assigned where a wider callable is expected](callable-fewer-parameters-adaptation.md) |
| [`compiler-scoped-emission-semantic-context.patch`](../../source-patches/compiler-scoped-emission-semantic-context.patch) | [Scoped emission must retain semantic context](compiler-scoped-emission-semantic-context.md) |
| [`compiler-sdk-lane-local-reexports.patch`](../../source-patches/compiler-sdk-lane-local-reexports.patch) | [SDK-local re-exports in an SDK export lane](compiler-sdk-lane-local-reexports.md) |
| [`compiler-structural-row-weak-keys.patch`](../../source-patches/compiler-structural-row-weak-keys.patch) | [Structural rows as WeakMap keys](compiler-structural-row-weak-keys.md) |
| [`compiler-structural-union-exact-native-owner.patch`](../../source-patches/compiler-structural-union-exact-native-owner.patch) | [Exact native owner recovery from a structural union](compiler-structural-union-exact-native-owner.md) |
| [`compiler-union-import-runtime-identity.patch`](../../source-patches/compiler-union-import-runtime-identity.patch) | [Canonical runtime identity across union imports](compiler-union-import-runtime-identity.md) |
| [`compiler-error-name-and-erased-error-reads.patch`](../../source-patches/compiler-error-name-and-erased-error-reads.patch) | [Error.name must be the runtime's instance accessor](compiler-error-name-instance-accessor.md), [An erased Error read needs a checked projection, and its `instanceof` guard needs lowering](compiler-erased-error-view-and-instanceof-guard.md) |
| [`compiler-unrepresented-error-member.patch`](../../source-patches/compiler-unrepresented-error-member.patch) | [An Error member the runtime does not carry must be refused, not emitted](compiler-unrepresented-error-member.md) |

## Active requests without a compiler patch

- [An indexed array write at length must grow the array](array-index-write-must-grow.md) —
  lighting's exact-owner forward-selection scratch arrays begin empty and grow a dense live prefix,
  but emitted `Array::element` writes throw at the first append; carried by explicit guarded pushes
  in `lighting-dense-forward-selection-scratch`.
- [A dynamic named read of a represented union must preserve its active arm](dynamic-named-read-of-represented-union.md) —
  `@flighthq/materials` clone/equality enumerates `Material.name`, but the dynamic row layer rejects
  its fully represented `String | null | undefined` carrier; carried by exact variant-arm `Any`
  conversion in the runtime.
- [Material clone and copy need the concrete generic owner](material-clone-needs-exact-generic-owner.md) —
  base-owner allocation cannot receive a concrete material's dynamically enumerated fields; carried
  by the materials exact-construction-owner source rewrite.
- [Entity-construction generic calls must retain the finished owner argument](entity-construction-generic-call-inference.md) —
  inference specializes the material clone helper as `EntityConstruction<T>` instead of its exact
  finished `T`; carried by finishing the identity view before the private copy.
- [`Array.isArray` must inspect an optional property's present payload](array-is-array-optional-property.md) —
  `@flighthq/adjustments` reads `Partial<ColorMatrixAdjustment>.colorMatrix` as an optional array,
  but the emitted predicate tests the optional carrier and always returns false; carried by the
  optional-aware `flight::is_array` runtime binding.
- [An imported `Record` alias loses its emitted runtime representation](imported-record-alias-loses-runtime-representation.md) —
  `@flighthq/capture`'s `CaptureBaseline` has an exact generated `flight::Record` carrier in the
  types package, but the consumer module classifies it as ambient and refuses all seven baseline
  operations; carried by `capture-baseline-imported-record-alias`.
- [A typed readonly callback needs to retain its derived owner](readonly-callback-derived-owner-recovery.md) —
  Scene2D's Sprite bounds hook loses both the exact Sprite node and its SpriteRuntime slot after the
  generic Node2D callback boundary; carried by the source patch `scene2d-sprite-exact-runtime-runner`.
- [Quadbatch needs one retained owner across specialized construction boundaries](quadbatch-owner-and-contextual-construction.md) —
  flattened Node2D/runtime/signal owners and contextual factories omit ten declarations; the native
  adapter preserves batch, runtime, buffer-view, atlas, material and signal identities explicitly.
- [Variadic console calls with non-string arguments](console-variadic-arguments.md) —
  needs a declared console-formatting contract; blocks log.ts `_writeConsoleCaptureEntry`.
- [An object literal passed through an optional call must build the parameter's reference owner](optional-call-object-literal-into-reference-owner.md) —
  carried for socket by the override `socket-complete-lifecycle`.
- [Member reads on a collapsed union record must use one access form](collapsed-union-record-member-access.md) —
  carried for socket by the override `socket-guard-reporting`.
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
  carried by the emission repairs `adjustments-color-matrix-interface-heritage` and `adjustments-tint-interface-heritage`,
  and by selection's runtime-type completion headers.
- [A callback member that names a constrained type parameter is emitted with the constraint](constrained-generic-member-callback-domain.md) —
  selection's generic active-change signal is fixed to `NodeAny`; its type completion retains `NodeType`.
- [A nullable generic value narrowed present must be unwrapped at calls](narrowed-optional-template-argument.md) —
  selection pointer-up passes `optional<NodeType>` to functions taking `NodeType`; the policy completion
  projects the present value after the unchanged guard.
- [Generic owners behind `Ref` must remain deducible at exported calls](reference-alias-template-deduction.md) —
  selection-state exports cannot infer `NodeType` through the runtime's conditional `Ref` alias; the
  completion retains explicit generic arguments internally and native callers currently do likewise.
- [A constrained generic argument must adapt to a structural helper parameter](constrained-generic-structural-helper-call.md) —
  selection's lasso and marquee candidate loops cannot instantiate for their declared `BoundsNodeAny`
  bound until the completion projects that exact owner to its bounds runtime.
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
- [A plain indexed array element must remain present for its member call](indexed-array-member-call-present-element.md) —
  `@flighthq/texture-formats` populates one inner array per format slot, then a plain
  `perSlotLevels[slot].push(...)` is refused after the emitter turns the element into optional storage;
  carried by the complete `texture-formats-parse-atf-nested-levels` override.
- [An indexed dense-array element keeps the presence established by lockstep append](indexed-dense-array-append-presence.md) —
  `@flighthq/path-boolean` creates adjacency-list indices only after appending their dense element, but
  the later `outgoing[from].push(edge)` loses that relational proof and withholds `DirectedGraph`.
- [A corpus-context structural row construction loses an exact Record spread](corpus-context-structural-row-record-spread.md) —
  `threeDsParse.ts` is a committed P-3D2 refusal, but its closest minimal form reaches an adjacent
  Record-spread rule and the split-module form emits; the page records the non-reproduction without
  inventing a narrower root cause.
- [An optional member must construct its declared Record value carrier](contextual-record-value-carrier-projection.md) —
  P-3D2 glTF loading refuses when an existing `Record<string, Uint8Array>` fills an optional
  `Record<string, ArrayLike<number>>` member; direct widening instead emits invalid invariant C++.
- [A reference-keyed WeakMap is emitted as a strong Map](reference-keyed-weak-map-as-strong-map.md) —
  `@flighthq/clipboard`'s `_watchSubscriptions` and `@flighthq/connectivity`'s `_subscriptions` are the
  same source shape and took different containers; only lifetime differs, so the clipboard audit
  records it as a bound rather than asserting against it.
- [A WeakMap value may be a nullable reference carrier](nullable-reference-weak-map-value.md) —
  P-3D2 resource guards refuse `ImageResourceFailure | null` even though the runtime faithfully stores
  and distinguishes the nested optional carrier.
- [`Promise.allSettled` needs a static binding and settled-result lowering](promise-all-settled-binding-and-result-lowering.md) —
  the runtime aggregate and per-arm carriers already exist, but the compiler-owned `Promise` table
  omits the member; Scene3D's two discard-only calls need the binding, while assets also needs checked
  `status`/`reason` lowering.
- [One frozen constant serving two outcome unions needs one carrier per union](shared-frozen-constant-across-two-outcome-unions.md) —
  `@flighthq/updater`'s `OPERATION_FAILED` is returned from both a variant-valued check function and a
  nominal install outcome; the emitter gives it the first union's readonly row, so the retained install
  body cannot compile while the banner says everything else did; carried for updater by the override
  `updater-complete-module`.
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
- [A readonly interface assertion should project from the retained owner](readonly-interface-assertion-retained-owner.md) —
  five `@flighthq/shading` built-in definitions are refused even though their signature callbacks
  only need checked readonly partial views of the retained modifier owner.
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
- [Strict identity must compare a readonly structural view with its nominal owner](readonly-structural-nominal-identity-comparison.md) —
  spritesheet emits `optional<StructuralRef<...>> == optional<Ref<...>>` without a refusal; the carried
  source local preserves the common readonly carrier and the same object identity.
- [An iterated element's property must retain its type at a contextual optional field](iterated-element-property-contextual-optional.md) —
  spritesheet refuses hydration and data validation after typed callback/iterator elements reach C++
  storage erased at optional destination fields; distinct from absence and presence-test defects.
- [A contextual empty-array fallback must inherit the iterable element type](contextual-empty-array-fallback-element-evidence.md) —
  spritesheet and texture-atlas format registries lose their typed descriptor arm at `options.formats ?? []`;
  explicit absence guards preserve the same zero-iteration behavior.
- [Structurally assignable schema unions need identity-preserving call conversion](parallel-schema-union-structural-conversion.md) —
  Aseprite and TexturePacker spritesheet documents cannot enter their parallel atlas-schema union arms
  even though TypeScript accepts each source arm structurally.
- [A contextual object return must use its declared nominal owner](contextual-object-return-nominal-owner.md) —
  two spritesheet serializer helpers silently return anonymous metadata owners from functions declared
  to return `AsepriteMeta` or `TexturePackerMeta`.
- [An object spread of a call result must copy the returned fields](object-spread-of-call-result-dropped.md) —
  Aseprite and TexturePacker array serializers silently drop every frame field supplied by a freshly
  returned nominal entry, leaving only `filename` in the emitted objects.
- [`JSON.stringify` must enumerate a nominal interface reference](json-stringify-nominal-ref-empty-object.md) —
  both spritesheet JSON serializers compile without diagnostics but stringify complete nominal documents
  as `{}` until the same owner is exposed through its readonly structural row.
- [A recursive value union needs incomplete-type-safe native storage](recursive-value-union-complete-storage.md) —
  `FlightDocumentValue` instantiates arrays and records of itself by value before its definition is
  complete, blocking six spritesheet-format parser and aggregate headers.
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
- [`@flighthq/tilemap-formats` native-compile defect inventory](tilemap-formats-native-compile-defects.md) —
  records the five current-pin refusals, seven emitted-C++ failure families, and frozen registry
  dependency before any package workaround is carried.
- [intl is refused for carried union and absence evidence](compiler-intl-optional-and-union-carrier-evidence.md)
  — 7 of 9 `@flighthq/intl` modules; one is supplied by the intl-formatter-cache override, the rest
  are retained refusals.
- [An argument is passed without constructing its declared parameter type](compiler-union-and-literal-argument-construction.md)
  — a union argument to an overloaded call, and an object literal into an `std::optional` parameter;
  recorded so they are not confused with intl's own refusals.
- [Contextual object literals for overloaded calls need an owned options carrier](contextual-object-literal-owner-materialization.md)
  — textureatlas passes a discriminated 2D options literal to `createTexture`, but the emitted call
  materializes none of the owned option carriers accepted by the native overload set.
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
- [An inferred `any` type argument leaks its parameter name into a readonly conversion](node-any-generic-traits-leak.md) —
  `@flighthq/command` captures child indices of `NodeAny` nodes; carried by the `command-undeclared-node-helpers` override.
- [A wider owner passed as a writable structural out parameter must keep its identity](writable-structural-out-of-a-wider-owner.md) —
  `@flighthq/geometry`'s aabb writers cannot take a `createAabb` result; scene3d's culling and bounds scratches depend on it (masked). No workaround is carried.
- [Type-argument deduction must follow interface heritage](heritage-argument-generic-deduction.md) —
  `@flighthq/movieclip`'s spritesheet source adds and invalidates a `Sprite` under a `Node2D`; the third leaked-`Traits` cause, cross-linked with the `any` and readonly-view requests. No workaround is carried.
- [Recovering an erased value as a readonly row must take the checked recovery a writable assertion takes](erased-value-readonly-row-recovery.md) —
  skeleton2d's three animation binders read `unknown` targets as readonly rows; `as T` already emits `object_if<T>`.
- [An inferred spread copy must keep its named type when it enters an optional, nullable array](spread-copy-into-optional-nullable-array.md) —
  skeleton2d's `cloneSkeleton2D` slot copy; carried by the `skeleton2d-clone-slot-copy-declared-type` source patch.
- [A default value on an optional, nullable parameter must lower without a presence proof](default-on-optional-nullable-parameter.md) —
  skeleton2d's `createSkeleton2D`; carried by the `skeleton2d-create-two-state-slots-default` source patch.
- [Strict equality between a nullable, optional value and a present value must not lower to a raw `==`](nullish-variant-strict-equality.md) —
  skeleton2d's `getSkeleton2DBoneIndexByName`; carried by the `skeleton2d-bone-name-present-comparison` source patch.
- [Members of a `satisfies Record<string, T>` table must carry `T`, or the emitter must convert them](satisfies-record-member-carrier.md) —
  `TransformMode2D` presets stored into `Bone2D.transformMode` emit as an unrelated anonymous struct; silent until C++ compile.
- [Reading the elements of an erased array must use the runtime's exact array recovery](erased-array-element-reading.md) —
  14 skeleton2d-formats JSON parser modules; the runtime's `AnyArray` and `external_if<Array<T>>` already exist.
- [An object literal passed to an optional `Record` parameter must be constructed before the optional](object-literal-into-optional-record-argument.md) —
  diagnostic details in five format packages; carried per package by detail-local source patches.
- [Owning arrays, readonly-row arrays and number buffers need conversions where they meet](animation-readonly-row-array-carriers.md) —
  animation blend tree and crossfade; carried by their two overrides.
- [A named function with fewer parameters must be adapted where a wider callable is expected](named-function-parameter-count-adapter.md) —
  timeline's no-op constructFrame fallback; carried by the `timeline-noop-construct-frame-arity` source patch.
- [A null test on an element of a nullable-element array tests the wrong optional layer](nullable-array-element-null-test.md) —
  silent miscompilation: `slots[i] !== null` emitted `get(i).has_value()`, so signals' tombstoned slots counted as live and were never compacted. Carried by the `compiler-indexed-element-presence-test` patch.
- [A local typed by a constrained type parameter is stored as the constraint](constrained-type-parameter-local-storage.md) —
  `@flighthq/signals`' `emitSignalSafe` could not be instantiated for any signal. Carried by the `compiler-constrained-type-parameter-union-slot` patch.
- [A forwarded parameter pack is consumed more than once](forwarded-parameter-pack-reused.md) —
  silent miscompilation: signal dispatch forwarded an rvalue argument into every slot, so slots after the first received moved-from values. Carried by the `compiler-dependent-callable-pack-reuse` patch.
- [Optional row reads must accept absent sparse-owner storage](optional-row-read-from-sparse-owner.md) —
  a `Partial<T>` options owner widened to an interface of optional properties threw on omitted fields;
  pinned by structural-row and native materials option propagation tests.
- [Dependent indexed access must read through the reference carrier](dependent-indexed-access-reference-carrier.md) —
  `T['kind']` emitted a member read directly on `flight::Ref<T>`, preventing materials'
  discriminant helper from being instantiated with any concrete material owner.
- [A `for...of` over a Map must iterate it live, because its body may delete from it](map-for-of-live-iteration.md) —
  silent miscompilation: the loop lowers to a range-for over the map's storage, which a `delete` in the body invalidates; tween's `updateTweens` crashed on the pass after a target's last tween finished. Carried by the `tween-update-erased-target-writes` override.
- [A generic argument that widens into an intersection parameter needs its conversion and template argument](generic-intersection-argument-widening.md) —
  silent until instantiation: velocity's transform traversal passes `NodeOf<Traits>` to node's `Transform2DNode<Traits>` queries bare. No workaround.
- [A generic node row passed to an `object` parameter must pass its object](generic-node-row-into-object-parameter.md) —
  silent until instantiation: velocity keys its samples by `node.shared_object()`, which a merged row does not offer. No workaround.
- [A structural runtime owner parameter must accept the node rows that carry the runtime](node-transform-runtime-owner-argument.md) —
  silent until instantiation: every `@flighthq/node` world-matrix query fails, which blocks velocity's transform baseline natively. No workaround.
- [An optional boolean operand of `&&` must be tested for its value, not its presence](logical-and-optional-operand-truthiness.md) —
  silent miscompilation: `a && b.flag` with `flag?: boolean` treats an explicit `false` as true; flow's overlay walks ticked and drew through `updateBelow: false` / `renderBelow: false`. Carried by the `flow-optional-overlay-flags-explicit` patch.
- [`Object.values` of a const object table must enumerate its members](object-values-of-const-table-unbound-keys.md) —
  silent miscompilation: the values come from a dynamic view whose keys the member table never binds, so the array is empty; statechart rejected every conditional transition as an invalid comparison. Carried by the `statechart-comparison-closed-set` patch.
- [Resolving an imported type's declaring owner must not re-walk every star re-export](exported-type-owner-lookup-uncached.md) —
  performance only: an uncached owner lookup over the `@flighthq/types/contract` barrel makes the registry migration group's generation take 3h40m. No workaround is carried.
- [A `?? null` fallback written into nullable structural storage must produce that storage's absent state](nullish-null-fallback-into-nullable-structural-storage.md) —
  `@flighthq/render`'s `initializeRenderState` (`canvasHost`, `imageHost`); one statement failed 227 renderer header compilations. Carried by the `render-state-nullable-structural-*-fallback` emission repairs.
- [`new Map(existingMap)` must copy, not alias](new-map-copy-lowered-to-alias.md) —
  silent miscompilation: `flight::Map(map)` selects the sharing copy constructor, so registry's `withKindMapEntry`, `withoutKindMapEntry` and `concatKindMap` mutated their input. Carried by the `registry-new-map-copy-*` emission repairs; seven other packages carry candidate sites.
- [A `null` written through a generic receiver must produce the member's absent state, not an engaged null](generic-receiver-null-write-engages-optional.md) —
  silent miscompilation and a crash: `initializeRenderStateRuntime<Runtime, …>` writes `registry_miss = nullptr`, so `createRenderState()` then `destroyRenderState()` dereferences null. Carried by the `render-state-generic-null-*` emission repairs.
