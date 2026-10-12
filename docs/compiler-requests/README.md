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
| [`compiler-concrete-owner-union-row-assertions.patch`](../../source-patches/compiler-concrete-owner-union-row-assertions.patch) | [Concrete owner assertion from an open union](compiler-concrete-owner-union-row-assertions.md) |
| [`compiler-concrete-structural-union-view-selection.patch`](../../source-patches/compiler-concrete-structural-union-view-selection.patch) | [Concrete owner entering a structural-view union](compiler-concrete-structural-union-view-selection.md) |
| [`compiler-contextual-imported-union-owner.patch`](../../source-patches/compiler-contextual-imported-union-owner.patch) | [Contextual construction of an imported union](compiler-contextual-imported-union-owner.md) |
| [`compiler-erased-ref-weak-map-view.patch`](../../source-patches/compiler-erased-ref-weak-map-view.patch) | [Checked typed view of an erased WeakMap](compiler-erased-ref-weak-map-view.md) |
| [`compiler-exact-variant-owner-priority.patch`](../../source-patches/compiler-exact-variant-owner-priority.patch) | [Exact variant owners must outrank shape matches](compiler-exact-variant-owner-priority.md) |
| [`compiler-external-optional-result-widening.patch`](../../source-patches/compiler-external-optional-result-widening.patch) | [External optional result widening](compiler-external-optional-result-widening.md) |
| [`compiler-flattened-optional-chain-union-storage.patch`](../../source-patches/compiler-flattened-optional-chain-union-storage.patch) | [Optional-chain payload union storage](compiler-flattened-optional-chain-union-storage.md) |
| [`compiler-generic-erased-map-reference-recovery.patch`](../../source-patches/compiler-generic-erased-map-reference-recovery.patch) | [Reference recovery from an erased Map](compiler-generic-erased-map-reference-recovery.md) |
| [`compiler-imported-alias-call-declaration-recovery.patch`](../../source-patches/compiler-imported-alias-call-declaration-recovery.patch) | [Recovering an exact call-result alias](compiler-imported-alias-call-declaration-recovery.md) |
| [`compiler-imported-alias-local-flow-abi.patch`](../../source-patches/compiler-imported-alias-local-flow-abi.patch) | [Exact imported alias through local flow](compiler-imported-alias-local-flow-abi.md) |
| [`compiler-imported-alias-outer-union-abi.patch`](../../source-patches/compiler-imported-alias-outer-union-abi.patch) | [Exact imported alias inside an outer union](compiler-imported-alias-outer-union-abi.md) |
| [`compiler-imported-alias-parameter-abi.patch`](../../source-patches/compiler-imported-alias-parameter-abi.patch) | [Exact imported alias at a parameter boundary](compiler-imported-alias-parameter-abi.md) |
| [`compiler-imported-readonly-view-owner-recovery.patch`](../../source-patches/compiler-imported-readonly-view-owner-recovery.patch) | [Imported owner recovery from readonly views](compiler-imported-readonly-view-owner-recovery.md) |
| [`compiler-imported-variant-owner-qualification.patch`](../../source-patches/compiler-imported-variant-owner-qualification.patch) | [Imported variant owner qualification](compiler-imported-variant-owner-qualification.md) |
| [`compiler-nested-import-forward-closure.patch`](../../source-patches/compiler-nested-import-forward-closure.patch) | [Forward declarations for nested imported aliases](compiler-nested-import-forward-closure.md) |
| [`compiler-nullable-structural-row-widening.patch`](../../source-patches/compiler-nullable-structural-row-widening.patch) | [Nullable structural-row widening](compiler-nullable-structural-row-widening.md) |
| [`compiler-owner-preserving-named-write.patch`](../../source-patches/compiler-owner-preserving-named-write.patch) | [Owner-preserving dynamic named write](compiler-owner-preserving-named-write.md) |
| [`compiler-present-dual-sentinel-multivariant.patch`](../../source-patches/compiler-present-dual-sentinel-multivariant.patch) | [Present projection of a dual-sentinel multivariant](compiler-present-dual-sentinel-multivariant.md) |
| [`compiler-readonly-union-view-conversion.patch`](../../source-patches/compiler-readonly-union-view-conversion.patch) | [Nominal union to readonly-view union](compiler-readonly-union-view-conversion.md) |
| [`compiler-scoped-emission-semantic-context.patch`](../../source-patches/compiler-scoped-emission-semantic-context.patch) | [Scoped emission must retain semantic context](compiler-scoped-emission-semantic-context.md) |
| [`compiler-sdk-lane-local-reexports.patch`](../../source-patches/compiler-sdk-lane-local-reexports.patch) | [SDK-local re-exports in an SDK export lane](compiler-sdk-lane-local-reexports.md) |
| [`compiler-structural-row-weak-keys.patch`](../../source-patches/compiler-structural-row-weak-keys.patch) | [Structural rows as WeakMap keys](compiler-structural-row-weak-keys.md) |
| [`compiler-structural-union-exact-native-owner.patch`](../../source-patches/compiler-structural-union-exact-native-owner.patch) | [Exact native owner recovery from a structural union](compiler-structural-union-exact-native-owner.md) |
| [`compiler-union-import-runtime-identity.patch`](../../source-patches/compiler-union-import-runtime-identity.patch) | [Canonical runtime identity across union imports](compiler-union-import-runtime-identity.md) |
| [`compiler-error-name-and-erased-error-reads.patch`](../../source-patches/compiler-error-name-and-erased-error-reads.patch) | [Error.name must be the runtime's instance accessor](compiler-error-name-instance-accessor.md), [An erased Error read needs a checked projection, and its `instanceof` guard needs lowering](compiler-erased-error-view-and-instanceof-guard.md) |

## Active requests without a compiler patch

- [Emit declared interface heritage so derived references widen to their base](interface-heritage-widening.md) —
  carried by the emission repairs `adjustments-color-matrix-interface-heritage` and `adjustments-tint-interface-heritage`.
- [Narrow an optional local in the else-branch of a negated truthiness test](negated-truthiness-else-narrowing.md) —
  carried by the source patch `node-bounds-rectangle-explicit-narrowing`.
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
- [A callable's parameter pack needs a typed buffer](callable-pack-argument-buffer.md) — debounce and
  throttle connectors cannot buffer a slot's arguments; carried by the signals throttle override.
- [Compound `%=` on a number must lower to `fmod`](compound-remainder-on-numbers.md) — the compound
  form is emitted as a C++ `double` remainder; carried by the same override.
- [A locally declared handler needs callable identity](local-handler-callable-identity.md) — an erased
  local lambda cannot be disconnected; carried by the same override.
- [A generic owner entering a widest-signature container must keep its
  identity](generic-owner-into-widest-container.md) — `connectSignalTracked` stays refused; a copy in
  `SignalScope.connections` would split identity and state, so no workaround is carried.
