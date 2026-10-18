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

Every `source-patches/compiler-*.patch` entry is mapped exactly once below, and the entries whose
patch has retired because the fix landed upstream keep their row, marked, as the request reference.

| Carried patch | Compiler request |
| --- | --- |
| [`compiler-erased-host-result-evidence.patch`](../../source-patches/compiler-erased-host-result-evidence.patch) | [Results naming an unresolved external host type have no type evidence](erased-host-result-evidence.md) |
| [`compiler-lazy-nullish-fallback.patch`](../../source-patches/compiler-lazy-nullish-fallback.patch) | [The generic `??` fallback evaluates its right operand even when the left is present](eager-nullish-fallback.md) |
| [`compiler-structural-value-carrier-host-handle-read.patch`](../../source-patches/compiler-structural-value-carrier-host-handle-read.patch) | [A predicate-narrowed value escapes as the derived nominal owner (host-handle member reads)](narrowed-value-escape.md) |
| [`compiler-checked-derived-member-read.patch`](../../source-patches/compiler-checked-derived-member-read.patch) | [A member read through a predicate-narrowed base value](checked-derived-member-read.md) |
| [`compiler-erased-external-member-presence.patch`](../../source-patches/compiler-erased-external-member-presence.patch) | [A null guard on a member of an erased external type](erased-external-member-presence.md) |
| [`compiler-interface-absence-refinement-heritage.patch`](../../source-patches/compiler-interface-absence-refinement-heritage.patch) | [Absence-only interface refinements keep native heritage](interface-absence-refinement-heritage.md) |
| [`compiler-structural-value-carrier.patch`](../../source-patches/compiler-structural-value-carrier.patch) | [A predicate-narrowed value escapes as the derived nominal owner](narrowed-value-escape.md) |
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
| [`compiler-named-reexport-provenance.patch`](../../source-patches/compiler-named-reexport-provenance.patch) | [Named re-export declaration provenance](named-reexport-declaration-provenance.md) |
| [`compiler-new-map-copy.patch`](../../source-patches/compiler-new-map-copy.patch) | [`new Map(existingMap)` must copy, not alias](new-map-copy-lowered-to-alias.md) |
| [`compiler-new-set-copy.patch`](../../source-patches/compiler-new-set-copy.patch) | [`new Set(existingSet)` must copy, not alias](new-set-copy-lowered-to-alias.md) |
| [`compiler-new-set-from-iterator.patch`](../../source-patches/compiler-new-set-from-iterator.patch) | [`new Set(collection.values())` must lower without a contextual optional construction](new-set-from-collection-iterator.md) |
| [`compiler-nullish-conditional-absent-fallback.patch`](../../source-patches/compiler-nullish-conditional-absent-fallback.patch) | [A nullish-coalesce fallback of `null` is wrapped into an engaged optional](nullish-coalesce-null-into-optional-is-never-absent.md) |
| [`compiler-declared-present-reference-chain.patch`](../../source-patches/compiler-declared-present-reference-chain.patch) | [An optional chain through a reference declared present must still test it](host-group-optional-chain-dropped.md) |
| [`compiler-asserted-empty-object-fallback.patch`](../../source-patches/compiler-asserted-empty-object-fallback.patch) | [An asserted `?? {}` fallback must test the storage it reads and build the asserted type](host-initialize-optional-fallback-on-a-required-read.md) |
| [`compiler-coalesced-union-member-narrowing.patch`](../../source-patches/compiler-coalesced-union-member-narrowing.patch) | [A member-presence narrow must select across every alternative that carries the member](narrowed-member-multi-alternative-selection.md) |
| [`compiler-narrowed-coalesced-arm-slot.patch`](../../source-patches/compiler-narrowed-coalesced-arm-slot.patch) | [A binding narrowed to one coalesced arm must enter a wider union as the slot it is stored in](narrowed-coalesced-arm-into-wider-union.md) |
| [`compiler-dependent-indexed-access-carrier.patch`](../../source-patches/compiler-dependent-indexed-access-carrier.patch) | [`T['key']` on a type parameter must read the member through the type argument's carrier](dependent-indexed-access-through-references.md) |
| [`compiler-async-closure-coroutine.patch`](../../source-patches/compiler-async-closure-coroutine.patch) | [An async closure needs the coroutine lowering a module-level function gets](async-closure-coroutine-lowering.md) |
| [`compiler-exported-type-owner-cache.patch`](../../source-patches/compiler-exported-type-owner-cache.patch) | [Resolving an imported type's declaring owner must not re-walk every star re-export](exported-type-owner-lookup-uncached.md) |
| [`compiler-dependent-erased-object-recovery-template.patch`](../../source-patches/compiler-dependent-erased-object-recovery-template.patch) | [Dependent erased-object recovery needs a template disambiguator](dependent-erased-object-recovery-template.md) |
| [`compiler-dependent-indexed-access-reference-carrier.patch`](../../source-patches/compiler-dependent-indexed-access-reference-carrier.patch) | [Dependent indexed access must read through the reference carrier](dependent-indexed-access-reference-carrier.md) |
| [`compiler-contextual-imported-union-owner.patch`](../../source-patches/compiler-contextual-imported-union-owner.patch) | [Contextual construction of an imported union](compiler-contextual-imported-union-owner.md) |
| [`compiler-erased-ref-weak-map-view.patch`](../../source-patches/compiler-erased-ref-weak-map-view.patch) | [Checked typed view of an erased WeakMap](compiler-erased-ref-weak-map-view.md) |
| [`compiler-typed-record-missing-key.patch`](../../source-patches/compiler-typed-record-missing-key.patch) | [A typed Record read of a missing key yields undefined](typed-record-missing-key-read.md) |
| [`compiler-nullable-element-dual-sentinel.patch`](../../source-patches/compiler-nullable-element-dual-sentinel.patch) | [A nullable array element written into a dual-sentinel slot](nullable-array-element-into-dual-sentinel-slot.md) |
| [`compiler-structural-member-view-assertion.patch`](../../source-patches/compiler-structural-member-view-assertion.patch) | [An interface assertion between flattened owners must read the same owner's recorded cells](structural-member-view-interface-assertion.md) |
| [`compiler-finish-entity-construction-owner.patch`](../../source-patches/compiler-finish-entity-construction-owner.patch) | [finishEntity must specialize from its construction, not from the expected return type](finish-entity-construction-owner.md) |
| [`compiler-inferred-member-written-union.patch`](../../source-patches/compiler-inferred-member-written-union.patch) | [A member's written union must survive an inferred receiver](inferred-member-written-union.md) |
| [`compiler-loop-bound-indexed-element-member.patch`](../../source-patches/compiler-loop-bound-indexed-element-member.patch) | [A member use on an indexed element the loop proves in bounds must emit](loop-bound-indexed-element-member.md) |
| [`compiler-self-returning-array-iterable-lifetime.patch`](../../source-patches/compiler-self-returning-array-iterable-lifetime.patch) | [A for-of over a self-returning array call must keep the receiver alive](self-returning-array-iterable-lifetime.md) |
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
| `compiler-nested-import-forward-closure.patch` — retired at 806d257, the fix landed upstream | [Forward declarations for nested imported aliases](compiler-nested-import-forward-closure.md) |
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
| [`compiler-present-readonly-row-context.patch`](../../source-patches/compiler-present-readonly-row-context.patch) | [A present optional readonly return retains its stored view](readonly-view-to-writable.md#g3-matching-present-readonly-returns) |
| [`compiler-readonly-contextual-source-projection.patch`](../../source-patches/compiler-readonly-contextual-source-projection.patch) | [Readonly helper calls retain the source read view](guarded-readonly-optional-helper-call.md#g3-readonly-argument-source-projection) |
| [`compiler-runtime-instanceof-lowering.patch`](../../source-patches/compiler-runtime-instanceof-lowering.patch) | [TypeScript `instanceof` is emitted verbatim as C++ text](instanceof-lowered-verbatim.md) |
| [`compiler-profile-type-name-evidence.patch`](../../source-patches/compiler-profile-type-name-evidence.patch) | [A property union loses the guard's evidence](property-union-evidence-lost.md) |
| [`compiler-guarded-path-narrowing.patch`](../../source-patches/compiler-guarded-path-narrowing.patch) | [A property union loses the guard's evidence](property-union-evidence-lost.md) |
| [`compiler-catch-await-owned-carrier.patch`](../../source-patches/compiler-catch-await-owned-carrier.patch) | [A catch body that awaits and reads its binding needs an owned caught value](catch-binding-across-await.md) |
| [`compiler-promise-resolve-domain.patch`](../../source-patches/compiler-promise-resolve-domain.patch) | [A resolved task carries the literal it was given](task-resolved-value-domain-mismatch.md) |
| [`compiler-callable-arity-adaptation.patch`](../../source-patches/compiler-callable-arity-adaptation.patch) | [A function with fewer parameters is assigned where a wider callable is expected](callable-fewer-parameters-adaptation.md) |
| [`compiler-callable-value-identity.patch`](../../source-patches/compiler-callable-value-identity.patch) | [A locally declared handler needs callable identity](local-handler-callable-identity.md) |
| `compiler-scoped-emission-semantic-context.patch` — retired at 806d257, the fix landed upstream | [Scoped emission must retain semantic context](compiler-scoped-emission-semantic-context.md) |
| `compiler-sdk-lane-local-reexports.patch` — retired at 806d257, the fix landed upstream | [SDK-local re-exports in an SDK export lane](compiler-sdk-lane-local-reexports.md) |
| [`compiler-structural-row-weak-keys.patch`](../../source-patches/compiler-structural-row-weak-keys.patch) | [Structural rows as WeakMap keys](compiler-structural-row-weak-keys.md) |
| [`compiler-structural-union-exact-native-owner.patch`](../../source-patches/compiler-structural-union-exact-native-owner.patch) | [Exact native owner recovery from a structural union](compiler-structural-union-exact-native-owner.md) |
| [`compiler-union-import-runtime-identity.patch`](../../source-patches/compiler-union-import-runtime-identity.patch) | [Canonical runtime identity across union imports](compiler-union-import-runtime-identity.md) |
| [`compiler-error-name-and-erased-error-reads.patch`](../../source-patches/compiler-error-name-and-erased-error-reads.patch) | [Error.name must be the runtime's instance accessor](compiler-error-name-instance-accessor.md), [An erased Error read needs a checked projection, and its `instanceof` guard needs lowering](compiler-erased-error-view-and-instanceof-guard.md) |
| [`compiler-unrepresented-error-member.patch`](../../source-patches/compiler-unrepresented-error-member.patch) | [An Error member the runtime does not carry must be refused, not emitted](compiler-unrepresented-error-member.md) |

## Active requests without a compiler patch

- [Readonly map key enumeration must retain the map value carrier](readonly-map-key-enumeration-value-carrier.md) —
  a constrained whole-map helper and shared-storage self-view support typed key calls; source-domain,
  key/header/runtime checks pass while pipeline graph-owner behavior remains separate.
- [Readonly must retain a bound external value carrier and its member policy](readonly-bound-external-value-carrier.md) —
  Scene3D custom uniform guards emit a structural row for Readonly<GlContext> while callers retain
  the native shared context value; an annotation-only private helper source patch carries the fix.
- [A named interface value must keep its row carrier, not be recast as an anonymous struct](named-interface-value-recast-as-anonymous-row.md) —
  `@flighthq/scene3d-formats` re-derives `ObjMaterial` as an anonymous shape and cannot construct that row from the
  `Ref<ObjMaterial>` the map already yields; carried by an alias repair.
- [A nullish union stored into an optional field must use the optional carrier](nullish-union-into-optional-field.md) —
  `@flighthq/scene3d` passes a `Light | null` local's three-state variant into a `optional<StructuralRef<...>>`
  row field; the sibling array fields already emit the destination's optional carrier.
- [Native reference recovery from a merged row must project the row first](merged-row-native-recovery-must-project.md) —
  `@flighthq/node` `disposeNode` recovers `Node<Traits>` directly from the merged `Node<Traits> & Traits` row and
  trips the runtime's merged-row assertion.
- [A call to a user-defined type predicate must narrow the guarded value](type-predicate-narrowing-not-applied.md) —
  `@flighthq/scene3d` reads `geometry` through an `isMesh(node)` guard from the un-narrowed `Node3D`
  row, so the runtime's row-schema assertion fires; the predicate itself already builds the narrowed projection.
- [An assertion on an optional value must cast the present value, not the optional](optional-assertion-cast-must-unwrap.md) —
  `@flighthq/scene2d` `createScale9Sprite` emits `structural_ref_cast<T>(obj)` with `obj` still an
  `std::optional`; the asserted value is type-level only and the callee's optional parameter carries the absence.
- [A readonly row returned as its declared nominal owner keeps the owner recovery](readonly-row-return-owner-recovery.md) —
  `@flighthq/render` `resolveInheritedColorScaleBias` emits the `return parent` branch as a bare
  `optional<Ref<ColorScaleBias>>` construction while the sibling `return local` branch gets
  `structural_ref_cast`. No workaround carried: the recovery is an operation, and the header still
  fails on later families, so no legal carrier supplies it.
- [A numeric builtin used as a callback needs a callable value](builtin-numeric-predicate-callback.md) —
  COLLADA passes the overloaded `std::isfinite` name to `filter`; the preceding `map(Number)`
  seam is resolved by the runtime's first-class Number conversion object.
- [A fresh object literal asserted to a type parameter has no C++ owner to become](fresh-literal-asserted-to-type-parameter.md) —
  `@flighthq/tray` `invokeUpdate` (`cpp-reference-assertion-without-heritage`): the asserted value is a new literal, so no heritage proof applies. The unrefused `{ outcome } as Result` branch emits `make_ref<Result>(Result{...})`, which cannot instantiate for a variant. Sound lowering needs exact-shape alternative selection at instantiation.
- [A caught value written into an `unknown` member needs an `Any` that can hold it](caught-value-into-unknown-member.md) —
  `@flighthq/tray` `destroyTrayRuntime` (`cpp-contextual-union-missing-expression-type:optionalSingle`): `flight::Any` has no caught-exception form, so the refusal is correct. This needs a runtime capability, not evidence. A second blocker sits behind it: a narrowed single-arm struct against a coalesced arm.
- [A generic union alias must not template an arm that does not use its type parameter](generic-alias-non-dependent-anonymous-arms.md) —
  `@flighthq/tray` `createTrayIcon` (`cpp-contextual-union-inequivalent`): `TrayCreateResult<Tray>` re-emits the provider-failure arms as templated twins, so the value's own struct is no alternative of the declared return. No workaround carried: no identity-preserving conversion exists, and the emission fix renames 91 structs in 23 headers across 9 packages.

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
- [A derived render owner asserted from its base has no heritage to cast along](canvas-render-state-owner-asserted-without-heritage.md) —
  `@flighthq/scene2d-canvas` `canvasRenderState.ts`, both constructors and the runtime accessor
  (`cpp-reference-assertion-without-heritage`): `createRenderState(...) as CanvasRenderState` and
  `createRenderStateRuntime() as CanvasRenderStateRuntime` have no heritage to cast along because interface
  relationships flatten into independent owners, and the `state[EntityRuntimeKey]` read then asserts a wider
  owner back out of retained storage. Carried by the `canvas-render-state-runtime-owner-and-initialization`
  patch, which declares the exact owner beside the type and reaches it through declared accessors; that patch
  carries a `pendingRebase` marker on the 2D lane, so the module still refuses at the live base, and its
  `createDerivedRenderStateConstruction` / `initializeRenderStateRuntime` helpers come from the applied
  sibling `render-state-runtime-owner-and-initialization` patch — the two are read together.
- [An open 3D material path must retain each concrete material owner](material3d-concrete-value-path.md) —
  the current `Material3D` mesh, proxy, draw-list and renderer boundaries erase sixteen built-in owners before
  their GL binders assert them back. The current-pin replacement is an open `Material3DValue` union propagated
  through the renamed `gl*MeshMaterialRenderer.ts` files; `StandardMaterial` is now 2D and is deliberately not
  an arm. This is the request-first specification for the pending `material-concrete-value-path` patch.
- [Open modifier stacks and registries must retain concrete value owners](modifier-concrete-value-path.md) —
  `Modifier` stacks erase eight built-in descriptor owners, while `ModifierDefinition` registries erase the GL
  and WGPU snippet callbacks stored in them. The exact replacement is `ModifierValue` plus
  `ModifierDefinitionValue` across types, shading and the renamed `glShadedMeshMaterialRenderer.ts`, with a
  branch for the independent vertex-direction fallback. The current-pin source patch is applied; the shaded
  prelude now retains all owners, with its remaining Map guard declared only.
- [A readonly Map arm must retain its Map constructor evidence](readonly-map-instanceof-member-evidence.md) —
  the shaded prelude's union already emits a concrete Map arm, but its `instanceof Map` guard refuses through
  the `Readonly<ReadonlyMap<...>>` spelling. The exact guard declaration supplies no shader or type-test body.
- [PBR define-key construction needs only the alpha-mode scalar](scene3d-gl-pbr-alpha-mode-boundary.md) —
  the three renamed PBR renderers cannot widen their concrete flattened owners to `Readonly<Material3D>` merely
  so `glPbrStandardBlock.ts` can read `alphaMode`. A scalar entry point keeps the existing object-taking helper,
  shares one implementation and preserves every define-key flag. The carrying GL patch is still pending.
- [Shadow traversal must retain checked mesh capabilities through branch flow](scene3d-gl-shadow-checked-capability-flow.md) —
  `glShadowMap.ts` must narrow heterogeneous nodes to meshes and meshes to instanced meshes without assertions
  or a redundant nullable owner carrier. One current-pin rewrite supersedes the two pending shadow patches,
  preserves rigid, skinned and instanced drawing, and emits the complete header with zero focused refusals.
- [The Scene3D GL upload cache must state its erasure at the storage boundary](scene3d-gl-erased-upload-cache.md) —
  the source half of the applied `compiler-erased-ref-weak-map-view` patch. At `a627849`, `glScene3DRuntime.ts`
  still asserts the typed map through a mutable nullable local, so the `checked_weak_map_view` refusal persists.
  The re-derived rewrite clears it (measured), with the module then stopping on a `??=` result. The pending GL
  patch also regenerates `@flighthq/types`.
- [PBR extension storage must retain each built-in extension owner](pbr-extension-concrete-value-arms.md) —
  the seven built-in GL extension registrations assert `PbrExtension` back to their concrete interface. The
  open `PbrExtensionValue` union clears all seven refusals at the pin (measured: 39 → 46 emitted modules with
  the upload-cache rewrite). Re-landing it also requires refreshing one context line of the active
  `scene3d-gl-erased-program-cache` patch.
- [Lazy uniform-location caches must keep their three states through a mutable local](scene3d-gl-lazy-uniform-location-boundaries.md) —
  `glMeshProgram.ts`'s seven `let loc = program.locX; if (loc === undefined)` caches refuse with `external
  binding storage … requires one unresolved optional value domain`, behind `material-concrete-value-path`.
  Field-specific resolvers clear each measured site; the pending GL patch must be re-derived site by site.
- [The single default material is already upstream](scene3d-gl-single-default-material.md) —
  `renderGlScene3D.ts` now has one full-trailer `Readonly<Material3D>` placeholder, so the pending
  `scene3d-gl-single-default-material` patch's same-carrier refusal cannot arise. Recommended for the
  source-patch lane's obsolete list.
- [The result of `??=` is present](nullish-assignment-result-presence.md) — a local bound from
  `slot ??= value` keeps the slot's optional carrier, so the first member read refuses for lack of a presence
  proof. Carried per site in scene2d-canvas; uncarried in `glScene3DRuntime.ts` `addGlScene3DResourceCleanup`.
- [An initialized optional typed-array member needs a present indexed write](initialized-optional-typed-array-write.md) —
  instanced-mesh color writes lose presence and typed-array semantics; an erased assertion emits the
  checked write while existing writable bounds-owner calls keep the header failed.
- [A readonly preset tuple must retain its shared-owner element carrier](readonly-preset-tuple-element-owner.md) —
  Spine binary mode selection uses a type-only element-union annotation and the canonical preset alias;
  the header still fails on diagnostic Record construction and owes the separate Bone2D owner contract.
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
  `@flighthq/registry` `createOrdinalTable`; carried by the type-only source patch `registry-ordinal-table-nullable-entries` in the registry renderer group regeneration (registry 3/3 emitted).
- [Null literals in an array of nullable variants must use the element's absence carrier](null-literal-array-optional-variant.md) —
  cube Texture already has the correct optional-variant element type but emits six ambiguous
  `nullptr` initializers instead of six empty elements.
- [Cube texture property reads and nullable face completeness](cube-texture-width-and-completeness.md) —
  the cube header reads width on a variant and tests bounds instead of nullable face elements;
  a derived partial override restores selected-owner reads and the original every semantics.
- [Cube face writes require the full numeric array-property domain](cube-face-index-write-domain.md) —
  the unrestricted setter can append, create holes or write named properties; the dense-profile
  decision retains that explicit gap while independent cube clone/copy/equals are supplied.
- [A numeric enum value is widened to a `number` union in flow](numeric-enum-flow-widened-to-number-union.md) —
  refuses `@flighthq/debug` `debug.ts` (`enableDebug`); no workaround.
- [Nullable storage of an imported alias is spelled by its expansion](nullable-alias-storage-spelled-by-expansion.md) —
  `@flighthq/debug` `install_debug_sink` under the LogSink override; no workaround yet.
- [Nullable fields read through `Partial<T>` use the wrong storage domain](partial-row-nullable-read-carrier.md) —
  `??` picks its lowering from the RESULT type, so a `T | null` property read from a partial row emits
  variant operations against an `std::optional`; `set_texture_atlas_region` fails clang at
  `texture_atlas_region.hpp:290`. `@flighthq/textureatlas` is carried by the
  `textureatlas-region-nullable-partial-read` override; the emitter defect itself is unpatched.
- [Package-scoped pin moves must reject consumers of removed headers](missing-material-value-generated-header.md) —
  the current-pin types row removed `material_value.hpp` while old-pin Scene3D consumers remain;
  mixed-inventory verification must reject that unresolved include closure.
- [`@flighthq/log` native-compile defect inventory](log-native-compile-defects.md) — fifteen emitter
  defects make log.hpp fail clang once the console refusal is set aside; the log override stubs twelve
  functions with throws, so log parity is not honest.
- [A binding initialized from a non-null-asserted collection get is present](non-null-asserted-collection-get-binding.md) —
  carried for log by the source patch `log-buffered-sink-state-explicit-guard`.
- [A non-null assertion must not turn an absent property into `optional::value()`](non-null-asserted-optional-property-return.md) —
  `@flighthq/entity`'s `getEntityRuntime` throws `std::bad_optional_access` for a fresh entity even
  though the asserted property read returns `undefined` in JavaScript; native callers currently
  guard the present-value path with `hasEntityRuntime`.
- [Finishing a structural construction bag must preserve object identity](structural-construction-finish-identity.md) —
  `@flighthq/entity`'s anonymous allocation row is materialized as a second nominal owner at
  `finishEntity`, while the source assertion returns the same object.
- [A Proxy returned as its nominal target must retain its traps](proxy-return-loses-nominal-traps.md) —
  `createGuardedEntityRuntime` constructs a structural write proxy and then loses it while recovering
  the original nominal pointer, so direct binding writes cannot reach the reporter.
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
  six direct refusals reach the missing ambient member, including `scene3d-formats/gltfSkins.ts`; that
  length-only array-like source also needs lowering beyond the represented-range runtime operation,
  while OBJ's two calls expose a following exact readonly-union insertion boundary.
- [A typed-array alternative must reach the SequenceView alternative of a contextual union](typed-array-into-sequence-view-union.md) —
  `awd2GeometryHandler.ts`'s bounded index array has no exact carrier for the destination
  `SequenceView<double>` alternative (`cpp-contextual-union-inequivalent`), so the faithful
  `new Uint32Array(indices)` rewrite advances the refusal instead of emitting; the runtime already
  constructs the view from the typed array.
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
- [Readonly Record construction and entry enumeration need one retained owner](readonly-record-entry-enumeration.md) —
  scene-document's exact readonly Record input reaches an iterator-only Object.entries helper;
  scratch typed-owner delegation clears it but exposes a missing Record-to-row constructor.
- [A Flight interface bound to an external key type never reaches its weak-key policy](bound-interface-weak-map-key-policy.md) —
  `WeakMap<GlContext, V>` is refused because the policy is read only for ambient key names, though the
  binding, the `using GlContext` alias and `WebGl2ContextWeakPolicy` all exist; effects-gl's three blit
  caches are supplied meanwhile by a partial override.
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
- [An assertion from a nominal union must recover its exact owner alternative](asserted-exact-owner-from-nominal-union.md) —
  TextureAtlas clones an exact 2D page through the declared `Texture` union, but the emitter refuses
  to select the union's existing `Ref<Texture2D>` alternative.
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
- [DOM style writes require a shared element and style boundary](dom-style-shared-element-boundary.md) —
  three scene2d-dom headers target a value-owned SDL shell with missing style fields; the package explicitly declares a web environment. No copied/default-only style workaround is carried.
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
- [A readonly nominal array element must keep its exact owner when indexed](readonly-nominal-array-index-owner.md)
  — scene-document's YAML-subset parser stores `Line[]` but reads an unrelated anonymous row.
- [An indexed fallback must keep the statically present element carrier](array-index-nullish-fallback-present-carrier.md)
  — scene-document's YAML-subset parser returns an optional index-miss wrapper from a lambda whose
  declared result is the present token reference.
- [A structural subtype argument must retain an identity-preserving view](structural-subtype-reference-argument-owner.md)
  — scene-document's token owner cannot enter the smaller readonly position parameter whose fields it
  already carries.
- [Reassigning a narrowed union must replace the whole variant](narrowed-union-reassignment-must-replace-variant.md)
  — scene-document assigns a fresh number-or-failure result after narrowing the same local to number,
  but the emitter writes the entire variant into its `double` alternative.
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
- [An inferred Map-values spread local must retain its typed elements](map-values-spread-local-element-carrier.md) —
  SWF Scale9 placement collection emits a typed spread result into `Array<Any>`; observed after the readonly Shape copy binding repair.
- [An inferred spread copy must keep its named type when it enters an optional, nullable array](spread-copy-into-optional-nullable-array.md) —
  skeleton2d's `cloneSkeleton2D` slot copy; carried by the `skeleton2d-clone-slot-copy-declared-type` source patch.
- [A default value on an optional, nullable parameter must lower without a presence proof](default-on-optional-nullable-parameter.md) —
  skeleton2d's `createSkeleton2D`; carried by the `skeleton2d-create-two-state-slots-default` source patch.
- [A strict literal test on a number/string member must inspect the active alternative](mixed-value-union-literal-comparison.md) —
  flex basis validation compares an optional numeric/string member with `auto` and then projects an untrusted
  string as numeric; carried by source guards that retain every original property read.
- [Strict equality between a nullable, optional value and a present value must not lower to a raw `==`](nullish-variant-strict-equality.md) —
  skeleton2d's `getSkeleton2DBoneIndexByName`; carried by the `skeleton2d-bone-name-present-comparison` source patch.
- [Members of a `satisfies Record<string, T>` table must carry `T`, or the emitter must convert them](satisfies-record-member-carrier.md) —
  `TransformMode2D` presets stored into `Bone2D.transformMode` emit as an unrelated anonymous struct; silent until C++ compile.
- [Reading the elements of an erased array must use the runtime's exact array recovery](erased-array-element-reading.md) —
  14 skeleton2d-formats JSON parser modules; the runtime's `AnyArray` and `external_if<Array<T>>` already exist.
- [A nullable array element written into a slot that also admits undefined must lift through the array's outer/inner read](nullable-array-element-into-dual-sentinel-slot.md) —
  skeleton2d's slot attachment binder and the Spine/DragonBones handlers that reach it; carried by the `compiler-nullable-element-dual-sentinel` patch, which keeps a stored null distinct from an index JavaScript reads as undefined.
- [An interface assertion between flattened owners must read the same owner's recorded cells](structural-member-view-interface-assertion.md) —
  loader's progress body; the runtime's StructuralMemberView/MemberValue read the object's own recorded cells, with absence distinct from stored false/0.
- [finishEntity must specialize from its construction, not from the expected return type](finish-entity-construction-owner.md) —
  loader's createResourceLoader finished the public base and aborted on exact-owner recovery; the finish now names the construction owner and the return widens the same reference.
- [A member's written union must survive an inferred receiver](inferred-member-written-union.md) —
  loader's arraybuffer branch: an awaited receiver arrives as a checker-collapsed shape where `T | unknown` is unknown; the declaration's written union is recovered member-wise.
- [A member use on an indexed element the loop proves in bounds must emit](loop-bound-indexed-element-member.md) —
  md5/obj feature scanners' `lines[i].trim()`; the loop condition is the bounds proof and the required accessor keeps the check at the use.
- [A for-of over a self-returning array call must keep the receiver alive](self-returning-array-iterable-lifetime.md) —
  `for (x of [...s].sort())` iterated zero elements because Array::sort returns a reference into a dead temporary; the call result is bound first.
- [An object literal passed to an optional `Record` parameter must be constructed before the optional](object-literal-into-optional-record-argument.md) —
  diagnostic details in five format packages; shared compiler materialization and original 3DS consumer qualified; existing detail-local patches retained for expiry audit.
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
- [A node-transform call needs a traits argument its enclosing function never declares](node-call-trait-absent-from-scope.md) —
  `@flighthq/render` `renderTarget.ts` and `renderTransform2d.ts`, both regenerated by the registry renderer-group landing: **three sites** over two symbols — `getNodeLocalMatrix` twice and `getNodeLocalTransformRevision` once. One site passes an unnarrowed `Node2D` row where a `Transform2DNode<Traits>` is required so deduction fails; the other two name `Traits` inside functions that are not templates. The same call compiles clean in `node/bounds_rectangle.hpp` where `Traits` IS declared, and this family — unlike `node-transform-runtime-owner-argument` — does have a small source repair, demonstrated by the `particleemitter-node-matrix-direct-emitter` precedent. The compiler fixture harness cannot model this external binding (`cpp-runtime-external-symbol-binding-incomplete`), so scratch-emitted C++ plus package tests are the verification. Carried by the `render-node-call-explicit-traits` patch, which rewrites all three sites; `pkg:compile -- @flighthq/render` is the gate obligation.
- [An optional boolean operand of `&&` must be tested for its value, not its presence](logical-and-optional-operand-truthiness.md) —
  silent miscompilation: `a && b.flag` with `flag?: boolean` treats an explicit `false` as true; flow's overlay walks ticked and drew through `updateBelow: false` / `renderBelow: false`. Carried by the `flow-optional-overlay-flags-explicit` patch.
- [`Object.values` of a const object table must enumerate its members](object-values-of-const-table-unbound-keys.md) —
  silent miscompilation: the values come from a dynamic view whose keys the member table never binds, so the array is empty; statechart rejected every conditional transition as an invalid comparison. Carried by the `statechart-comparison-closed-set` patch.
- [Resolving an imported type's declaring owner must not re-walk every star re-export](exported-type-owner-lookup-uncached.md) —
  performance only: a scoped `@flighthq/scene2d-gl` generation drops from 8172 s to 830 s with byte-identical output. Carried by the `compiler-exported-type-owner-cache` patch (`answers: performance`).
- [A `?? null` fallback written into nullable structural storage must produce that storage's absent state](nullish-null-fallback-into-nullable-structural-storage.md) —
  `@flighthq/render`'s `initializeRenderState` (`canvasHost`, `imageHost`); one statement failed 227 renderer header compilations. Carried by the `render-state-nullable-structural-*-fallback` emission repairs.
- [`new Map(existingMap)` must copy, not alias](new-map-copy-lowered-to-alias.md) —
  silent miscompilation: `flight::Map(map)` selects the sharing copy constructor, so registry's `withKindMapEntry`, `withoutKindMapEntry` and `concatKindMap` mutated their input. Carried by the `compiler-new-map-copy` patch (which retired the registry repairs); seven other packages carry candidate sites that change on their next regeneration.
- [A `null` written through a generic receiver must produce the member's absent state, not an engaged null](generic-receiver-null-write-engages-optional.md) —
  silent miscompilation and a crash: `initializeRenderStateRuntime<Runtime, …>` writes `registry_miss = nullptr`, so `createRenderState()` then `destroyRenderState()` dereferences null. Carried by the `compiler-generic-receiver-null-write` patch; two `render-state-generic-null-clip-hooks-*` repairs remain for the mapped-type receiver.
- [`new Set(existingSet)` must copy, not alias](new-set-copy-lowered-to-alias.md) —
  silent miscompilation, the Set twin of the Map request: `flight::Set<T>(set)` selects the sharing copy constructor. One SDK site (`scene3d-formats` `objRequirements.ts`).
- [`new Set(collection.values())` must lower without a contextual optional construction](new-set-from-collection-iterator.md) —
  a refusal (`contextual optionalSingle construction requires expression type evidence`) for `values()`/`keys()` iterator arguments; preventive, no SDK site is refused today. `entries()` and stored iterators stay refused.
- [An optional chain through a reference declared present must still test it](host-group-optional-chain-dropped.md) —
  `@flighthq/host` `hostQuery.ts`: `host.group?.slot ?? null` dereferenced a missing group's null `Ref`. Carried by the `compiler-declared-present-reference-chain` patch.
- [An asserted `?? {}` fallback must test the storage it reads and build the asserted type](host-initialize-optional-fallback-on-a-required-read.md) —
  `@flighthq/host` `initializeHost`: `.value_or` on a required read plus an empty anonymous fallback. Carried by the `compiler-asserted-empty-object-fallback` patch and `flight::nullish_or`.
- [A nullish-coalesce fallback of `null` is wrapped into an *engaged* optional, so its truthiness test dereferences null](nullish-coalesce-null-into-optional-is-never-absent.md) —
  silent miscompilation: `@flighthq/textlayout`'s wrapping path panics on the first-segment overrun that upstream's own fixture exercises. No workaround is carried.
- [A structural view of an entity-allocated carrier is invisible when the carrier derives from the row's subject](structural-view-of-a-derived-carrier-is-invisible.md) —
  silent miscompilation: `@flighthq/textshaper` cannot recover the derived run carrier through its structural row view. No workaround is carried.
- [A named input carrier and a structurally identical anonymous output carrier cannot alias](named-input-and-anonymous-output-carriers-cannot-alias.md) —
  `@flighthq/screen` loses the aliasing half of all four `dipToScreen*`/`screenToDip*` conversions: the input is a row over the named `Vector2Like`/`RectangleLike` while the out is the anonymous `x_y` shape, so a single object cannot be passed as both. Measured as a hard compile error. No workaround is carried.
- [A readonly structural member must widen the original cell value](readonly-structural-member-carrier-widening.md) —
  runtime gap measured by a rejected Scene3D native-present candidate: a concrete framebuffer cell
  cannot be read as the nullable member of its compatible readonly base view. No compiler caller
  change or native candidate is carried; original 0/1 remains.
- [A merged row must be constructible and readable as the one object it describes](merged-row-not-constructible.md) —
  `@flighthq/host` construction and `@flighthq/app` narrowed reads exposed a runtime gap. Resolved in
  `structural_ref.hpp`; no compiler change is requested.
- [An optional chain on a host capability group is lowered as an unconditional dereference](host-group-optional-chain-dropped.md) —
  `@flighthq/host` accessors panic when an optional capability group is absent. No patch is carried.
- [A nullish fallback on a required read lowers to `value_or` on a non-optional](host-initialize-optional-fallback-on-a-required-read.md) —
  `@flighthq/host`'s `createHost` and `initializeHost` do not compile. No patch is carried.
- [A binding narrowed to one coalesced arm must enter a wider union as the slot it is stored in](narrowed-coalesced-arm-into-wider-union.md) —
  `@flighthq/tray` `destroyTrayRuntime` `return result`, and an upstream backend test failing at the pin. Carried by the `compiler-narrowed-coalesced-arm-slot` patch; `destroyTrayRuntime` stays blocked by the caught-value capability.
- [`T['key']` on a type parameter must read the member through the type argument's carrier](dependent-indexed-access-through-references.md) —
  `@flighthq/tray` `invokeRead` (and `invokeUpdate`), and `@flighthq/materials` `getMaterialOfKind`: `decltype(std::declval<T&>().key)` could not instantiate for any `Ref` or variant. Carried by the `compiler-dependent-indexed-access-carrier` patch and `flight::indexed_member_t`.
- [An async closure needs the coroutine lowering a module-level function gets](async-closure-coroutine-lowering.md) —
  `@flighthq/tray` `queueAnimationWrite` and `attachTrayEvent`'s async release. Carried by the `compiler-async-closure-coroutine` patch and `flight::retain_closure`; `attachTrayEvent` stays blocked by the caught-value capability.
- [An out-of-range indexed array read must widen to `null`](out-of-range-array-index-widened-to-null.md) —
  `@flighthq/input` array indexing throws where TypeScript returns `null`; no workaround is carried.
- [A generic structural parameter is passed an owner whose emitted row is narrower than the parameter's](generic-structural-parameter-given-a-narrower-row.md) —
  `@flighthq/effects` `computeEffectCaptureGeometry` passes its `Node2D` source to
  `computeNodeRootLocalBoundsRectangle<Traits>` and no overload matches
  (`no matching function for call to 'compute_node_root_local_bounds_rectangle'`). TypeScript accepts the
  argument because `Node2D` supplies every constituent of `Spatial2DNode<Node2DTraits>`; the emitter
  renders `Node2D` as a two-component row and `Spatial2DNode<Traits>` as a five-component one. Measured:
  the runtime's `structural_ref_cast` expression compiles, but its result does not match the callee's
  row parameter; no working call-site projection is established. No patch is carried.
- [A membership test must lower to the runtime's presence question](membership-test-lowered-literally.md) —
  `@flighthq/effects` `copyEffectDefaults`, reported as `unknown type name 'key_2'`, plus 28 more sites
  across 14 other headers (render, skeleton2d-formats, interaction, lighting, log, scene3d, scene3d-gl,
  scene3d-wgpu, scene2d-formats, physics3d-abi). Carried by the `respell-membership-test` emission repair,
  declared per receiver; no patch is carried.

- [Video constructor readonly host and partial-row boundary](video-constructor-readonly-partial-boundary.md).

- [Readonly video helper carriers](video-readonly-helper-carriers.md).
- [Video frame advance asserted source and undefined callback argument](video-advance-asserted-host-source.md).
- [An object literal passed to an `optional<Record<...>>` parameter is emitted as a bare braced pair list](optional-record-literal-not-materialized.md) —
  17 historical assigned headers across scene2d-formats, scene3d-formats, skeleton2d-formats and swf (cause cluster
  `other-40da43f7`); shared `compiler-optional-record-literal-materialization` names the existing Record.
  Normal generation and the original 3DS public consumer pass; nine of the 17 historical headers compile, eight have later failures, and SVG results are counted separately.
- [A joint initializer is called with the construction view instead of the owner it wraps](entity-construction-view-passed-to-owner-parameter.md) —
  8 headers across physics2d, physics2d-abi and physics3d; a CONCRETE call, not the generic substitution case in
  `entity-construction-generic-call-inference.md`. No patch is carried.

- [A bound external constant must keep its member target name](external-property-method-name-collision.md) —
  the G2 GL velocity declaration-only candidate fails on `gl.VIEWPORT` emitted as the native
  `viewport` method instead of `viewport_constant`; typed query-result assertions remain a separate inspected seam.

- [A texture-cache local must retain the effective key carrier and shared map](native-texture-source-cache-local-carrier.md) —
  the G2 Scale9Shape getter-declaration candidate reaches a variant-key native cache copied into a
  nominal TextureSource-key local; preserve the same map and source owners, with no cross-key cast or copied cache.

- [A bound WebGPU texture must expose its actual size properties](webgpu-texture-size-properties.md) —
  WGPU screen-target resize reads GPUTexture.width/height, absent from the native shared handle.
  Original1 stays0/1; exact initializer declaration fails unused-header compilation before link.
  Actual dimensions, shared resource identity, live surface and resize/lifecycle order must survive;
  no target/provider/repair or GPU behavior is supplied.

- [A bound WebGPU buffer needs real mapped readback and its lifetime](webgpu-buffer-mapped-readback.md) —
  screen-capture original1 stays0/1; exact three-declaration unused-header candidate fails absent
  get_mapped_range before link. Preserve real mapped bytes, shared buffer/map lifetime, frame-copy
  handshake, padded rows, BGRA/premultiplied pixels and timeout/late-settlement semantics; no provider
  or capture implementation is supplied.
- [Object literals passed to bound WebGPU methods need the binding's field contract](external-descriptor-literal-construction.md) —
  An object literal passed to a bound WebGPU method (the `call-has-no-matching` cluster led by
  `begin_render_pass`) is emitted as a bare braced initializer. The constrained variadic host template
  cannot deduce from it, even where `bindings/sdl-wgpu.json` declares the descriptor's `objectConstruction`.
- [A `typeof` guard on a record member does not narrow the member's next read](typeof-narrowed-record-member-read.md) —
  `@flighthq/scene2d-formats` Lottie detection: `typeof record.fr === 'number' && record.fr > 0` compares an
  erased `flight::Any`. The narrowed read must be `as_number()`, as it already is for locals.
- [A call to a generic function converts its argument to the callee's unsubstituted parameter type](generic-call-argument-uses-unsubstituted-parameter-type.md) —
  `@flighthq/shape` `registerShapeBoundsCommand` and the `Traits`-generic node helpers: a non-generic caller
  names the callee's own type parameter (`K`, `Traits`), which is not in scope at the call site.
- [A module-local interface that extends a lib.dom interface refuses its whole module](local-interface-extending-lib-dom-interface.md) —
  `@flighthq/scene2d-dom` `domClipRectangle.ts`: `interface DomClipStyle extends CSSStyleDeclaration` stops
  the interface-inheritance pass, so none of the module's eleven declarations is emitted.
- [A downcast assertion kept as a structural view must be recovered before a nominal parameter](asserted-row-view-into-nominal-parameter.md) —
  `@flighthq/swf` `swfFrameAction.ts`: `target as MovieClip` stays a view over the same owner but reaches
  `Ref<MovieClip>` parameters without `structural_ref_cast<Ref<T>>`. The recovery compiles in scratch but is
  checked where the assertion is not, so nothing is carried.
- [A numeric optional-chain result must retain its numeric payload](numeric-optional-chain-result-erased.md) —
  explicit guards and shared dictionary/presence mechanisms now emit the original full style/color
  closure: actual gradient/cascade/color+lookup consumers pass2/2CTest; the separate collector remains
  refused. Four SVG roots compile; older geometry/Matrix callers remain independently scoped.
- [A typed array passed to a `readonly unknown[]` parameter needs a readonly element view](typed-array-into-readonly-unknown-array-parameter.md) —
  `@flighthq/scene3d-gl` fake GL error audit: `number[]` into `setError(..., readonly unknown[])` lowers as
  `Array<double>` against `Array<Any>`. Nothing is carried.
- [A numeric enum interpolated into a template literal has no string conversion](numeric-enum-template-literal-interpolation.md) —
  `@flighthq/render` `renderRegistryGuards.ts`: `${registry}` over a numeric enum lowers to
  `flight::to_string(enum class)`; prepared explicit numeric conversion passes source/native controls.
  The original registry consumer and eight rows still await independent membership owner support.
- [`typeof` on a member read through an assertion is folded to a constant](asserted-member-typeof-constant-fold.md) —
  `@flighthq/render` `isRenderCacheAdapter`: `typeof (value as RenderCacheAdapter).adapt === 'function'` emits
  `"function" == "function"`. It should read the erased value's own member, or refuse. Nothing is carried.

- [Computed Record deletion must remove the property](record-computed-delete.md) —
  shared mutable dictionary deletion, primitive missing-key storage and erased local
  presence assertions enable the actual SVG style/color parser closure. Code-first
  whole-body consumers and normal generation are qualified: actual native consumers2/2PASS,
  all22style fields/88colors retained, no source body replacement or full-import claim.

- [Readonly automatic particle parser forwarding entrypoints](particle-readonly-forwarding-proposal.md) —
  approved bounded additional readonly helpers preserve the original native parser ABI and
  forward the original options owner into declared readonly codec slots.
