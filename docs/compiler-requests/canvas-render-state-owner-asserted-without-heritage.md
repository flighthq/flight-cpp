# A derived render owner asserted from its base has no heritage to cast along

`@flighthq/scene2d-canvas`'s `canvasRenderState.ts` does not compile. Its two constructors build the
**base** carrier the shared render factory returns and then assert it to the **derived** interface the
package declares:

```ts
const state = _createRenderState({ /* ... */ }) as CanvasRenderState;
const runtime = createRenderStateRuntime() as CanvasRenderStateRuntime;
```

Because the emitter flattens interface heritage into independent owners, `CanvasRenderState` and
`CanvasRenderStateRuntime` share no emitted C++ class-heritage path with `RenderState` and
`RenderStateRuntime`, so there is nothing to cast along. The package's own runtime accessor then compounds
it, reading the derived owner back out of a computed symbol:

```ts
return state[EntityRuntimeKey] as CanvasRenderStateRuntime;
```

## Upstream shape

`packages/scene2d-canvas/src/canvasRenderState.ts`, at the pin:

```ts
export function createCanvasRenderState(
  registries: Readonly<CanvasRenderRegistries>,
  canvasTextureResolvers: CanvasTextureResolvers,
  options: Partial<CanvasRenderOptions> = {},
): CanvasRenderState {
  const state = _createRenderState({
    pixelRatio: options.pixelRatio ?? 1,
    renderTransform2D: options.renderTransform ?? createMatrix(),
    roundPixels: options.roundPixels ?? false,
    sceneGraphSyncPolicy: options.sceneGraphSyncPolicy,
  }) as CanvasRenderState;
  // ...writes state.registries through a structural assert...
  const runtime = createCanvasRenderStateRuntime(registries, canvasTextureResolvers);
  state[EntityRuntimeKey] = runtime;
  // ...
}
```

The rewrite is already intended by the source: `CanvasRenderState extends RenderState`, and the runtime
slot is `EntityRuntimeKey`, which upstream accesses through the same symbol. Nothing here is a
target-specific idea; the derived owner is the owner the source means.

## Emitted (flight-compiler `2b687ea` with the carried patches, flight `a627849`)

`generated/include/flight/scene2d_canvas/canvas_render_state.hpp` refuses four declarations; two of them
are this defect, both as `cpp-reference-assertion-without-heritage [target-runtime]`. The emitted header
carries the refusal verbatim:

```
// NOT GENERATED: function createCanvasRenderStateRuntime -- source line 54
// refusal: cpp-reference-assertion-without-heritage [target-runtime]
...
// cpp emission failed for .../canvasRenderState.ts: a reference
// assertion from flight::Ref<flight::types::RenderStateRuntime> to
// flight::Ref<flight::types::CanvasRenderStateRuntime> has no heritage to cast along: the declarations share
// no emitted C++ class-heritage path (source interface and intersection relationships flatten into independent
// owners), and the source carrier does not retain a checked dynamic owner that can recover the target. Keep the
// exact declared owner at the API boundary, or add a runtime contract that validates and recovers the target
// owner; the compiler will not use a native pointer cast, materialize a replacement row, or invent side storage
```

The `EntityRuntimeKey` read adds a second, differently worded refusal of the same shape: the asserted row
reads `canvasBlendEffectBackdrops`, `canvasHost`, `canvasTextureResolvers`, `currentAlpha`,
`currentBlendMode`, `currentRenderTarget`, `imageSmoothingEnabled`, `imageSmoothingQuality`, `passStack`,
`registries` and `teardowns`, which the **source** type does not declare, so the read has no compatible
cell to answer and no retained storage can recover the wider owner.

The remaining two refusals in the header are a different defect and are not what this request is about
(`cpp-member-projection-without-present-storage`, `cpp-structural-assertion-owner-unproven`).

## Required C++ shape

Either the emitter preserves `CanvasRenderState` and `CanvasRenderStateRuntime` heritage across their
`RenderState` bases — so an assertion from the base to the derived has a real heritage path and the
computed-symbol cell has an owner — or the runtime owner is reached through a **declared function** rather
than a symbol-indexed member, which the emitter can lower exactly. The second is what this repository
carries today.

## Carried workaround

`source-patches/canvas-render-state-runtime-owner-and-initialization.patch` (`dependent: flight`,
`baseCommit 7e2fc7df`, package `@flighthq/scene2d-canvas`). Target-neutral TypeScript; no public factory
signature changes. It replaces each asserting expression with a declared owner:

| asserting target expression | replacement |
| --- | --- |
| `_createRenderState({...}) as CanvasRenderState` | `createDerivedRenderStateConstruction<CanvasRenderState>({...})` |
| `createRenderStateRuntime() as CanvasRenderStateRuntime` | `initializeRenderStateRuntime<CanvasRenderStateRuntime, CanvasRenderRegistries>({} as CanvasRenderStateRuntime, runtimeRegistries)` |
| `state[EntityRuntimeKey] = runtime` | `writeCanvasRenderStateRuntimeOwner(state, runtime)` |
| `state[EntityRuntimeKey] as CanvasRenderStateRuntime` | `readCanvasRenderStateRuntimeOwner(state)` |
| `(state as { registries: ... }).registries = registries` | `state.registries = registries` |

It adds to `packages/types/src/CanvasRenderState.ts` the computed-symbol cell on the interface plus the two
accessor functions beside it, so the owner is declared rather than asserted. It also gives
`createCanvasRenderState` an explicit `CanvasRenderStateInitialization` overload instead of
`Partial<CanvasRenderOptions>`.

The two helpers it leans on — `createDerivedRenderStateConstruction` and `initializeRenderStateRuntime` —
are a **temporary generic render initializer** that exists only because the emitter flattens the heritage.
They are not a second owner; they are the same allocation constructed through a declared type argument.

### Equivalence

State and runtime each retain **one allocation**; the same field values are written in the same order; the
same `EntityRuntimeKey` attachment is made. Base fields are still initialized before Canvas fields. The
registry spread still produces one shallow derived registry. Nothing observable — absence, reference
identity, equality, ordering, exception shape or task settlement — changes.

### Expiry

Drop the patch when the emitter preserves `CanvasRenderState` and `CanvasRenderStateRuntime` heritage and
computed-symbol ownership across their `RenderState` bases, or when Flight adopts these exact-owner helpers
upstream.

### Status

The patch carries a `pendingRebase` marker: context drift since `a627849` on the 2D lane (10 of 14 removed
lines still present, surrounding source changed). It is **not currently applied**, which is why the module
still refuses at the live base. Rebasing the hunks is the source-patch lane's work, not this request's.

## Focused test

A compiler fixture that lowers both halves and distinguishes them:

1. **The refusal is real and is this class.** Lower a source module holding `createBase(...) as Derived`
   where `Derived` extends `Base`, and assert the diagnostic names
   `cpp-reference-assertion-without-heritage` and the `flight::Ref<Base>` →
   `flight::Ref<Derived>` pair. A fixture that only checks "emission failed" would pass for the wrong
   reason, since the same module refuses for two other classes.
2. **The replacement selects the exact owner.** Lower the rewritten form
   (`createDerivedRenderStateConstruction<Derived>(...)` and the declared accessors) and assert the emitted
   construction and accessor calls carry `Derived` — not the base — and that no `as`-assertion survives.
3. **The computed-symbol read is gone.** Assert the emitted header contains no symbol-indexed owner read,
   which is the shape that produced the second refusal.

A native test belongs on the runtime side rather than the compiler side: assert that a constructed state
and its runtime each keep **one** allocation, that `readCanvasRenderStateRuntimeOwner` returns the very
runtime written by `writeCanvasRenderStateRuntimeOwner`, and that the two accessors do not copy. That is
the invariant the equivalence argument claims, and it is the one an assertion-based rewrite would break.
