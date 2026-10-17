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

`packages/scene2d-canvas/src/canvasRenderState.ts` at the pin (`a627849`), the functions that refuse:

```ts
export function createCanvasRenderState(options: Readonly<CanvasRenderStateOptions> = {}): CanvasRenderState {
  const state = _createRenderState({
    pixelRatio: options.pixelRatio ?? 1,
    roundPixels: options.roundPixels ?? false,
    sceneGraphSyncPolicy: options.sceneGraphSyncPolicy,
  }) as CanvasRenderState;
  // ...applyBlendMode and the CSS-filter slot...
  const registries = buildCanvasRenderRegistries(options);
  const canvasTextureResolvers = options.canvasTextureResolvers ?? createCanvasTextureResolvers(options.canvasHost!);
  const runtime = createCanvasRenderStateRuntime(registries, canvasTextureResolvers);
  if (options.canvasHost !== undefined) runtime.canvasHost = options.canvasHost;
  state[EntityRuntimeKey] = runtime;
  (state as { registries: Readonly<CanvasRenderRegistries> }).registries = runtime.registries;
  // ...
  return state;
}

export function createCanvasRenderStateRuntime(
  registries: Readonly<CanvasRenderRegistries>,
  canvasTextureResolvers: CanvasTextureResolvers,
): CanvasRenderStateRuntime {
  const runtime = createRenderStateRuntime() as CanvasRenderStateRuntime;
  runtime.registries = cloneCanvasRenderRegistries(registries);
  // ...
}

export function getCanvasRenderStateRuntime(state: CanvasRenderState): CanvasRenderStateRuntime {
  return state[EntityRuntimeKey] as CanvasRenderStateRuntime;
}
```

Three details of this shape are load-bearing, and an earlier draft of this document got all three wrong
by taking its excerpt from the patch text instead of from the pin:

- `createCanvasRenderState` takes **one** parameter, `Readonly<CanvasRenderStateOptions>`. There is no
  `(registries, canvasTextureResolvers, options)` signature and no exported `CanvasRenderStateInitialization`
  interface; the registries are derived inside through the module-local `buildCanvasRenderRegistries`.
- There is no `renderTransform2D` field at this pin.
- The state's `registries` field is neither the caller's object nor a freshly built one: it is assigned
  **from the runtime aggregate** (`runtime.registries`). The source comments why — the field must be a read
  view of the same aggregate the runtime holds, or the offscreen factories that derive a pipeline from it
  would snapshot an empty policy.

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

`source-patches/canvas-render-state-runtime-owner-and-initialization.patch` (`dependency: flight`,
`baseCommit 7e2fc7df`, package `@flighthq/scene2d-canvas`). Target-neutral TypeScript; no public factory
signature changes. It replaces each asserting expression with a declared owner — five replacements, against
the pin shape above:

| asserting target expression | replacement |
| --- | --- |
| `_createRenderState({...}) as CanvasRenderState` | `createDerivedRenderStateConstruction<CanvasRenderState>({...})` |
| `createRenderStateRuntime() as CanvasRenderStateRuntime` | `initializeRenderStateRuntime<CanvasRenderStateRuntime, CanvasRenderRegistries>({} as CanvasRenderStateRuntime, runtimeRegistries)` |
| `state[EntityRuntimeKey] = runtime` | `writeCanvasRenderStateRuntimeOwner(state, runtime)` |
| `state[EntityRuntimeKey] as CanvasRenderStateRuntime` | `readCanvasRenderStateRuntimeOwner(state)` |
| `(state as { registries: Readonly<CanvasRenderRegistries> }).registries = runtime.registries` | `state.registries = runtime.registries` |

Note the fifth row carries `runtime.registries`, not `registries`: the assignment is already reading the
runtime aggregate at the pin, and the rewrite only removes the structural assert around it.

It adds to `packages/types/src/CanvasRenderState.ts` the computed-symbol cell on the interface plus the two
accessor functions beside it, so the owner is declared rather than asserted.

**The two helpers are not this patch's.** `createDerivedRenderStateConstruction` and
`initializeRenderStateRuntime` are defined by the **sibling** carried patch
`render-state-runtime-owner-and-initialization` (module `packages/render/src/renderState.ts`, package
`@flighthq/render-gl`, `baseCommit a627849`, `pendingRebase: none` — so it **is** applied at the live base).
Their signatures are:

```ts
export function createDerivedRenderStateConstruction<State extends RenderState>(
  obj: RenderStateInitialization | undefined,
): EntityConstruction<State>;

export function initializeRenderStateRuntime<
  Runtime extends RenderStateRuntime,
  Registries extends RenderRegistries,
>(runtime: Runtime, registries: Registries): Runtime;
```

They are a **temporary generic render initializer** that exists only because the emitter flattens the
heritage. They are not a second owner; they are the same allocation constructed through a declared type
argument. Both requests should be read together: this one cannot be applied without that one.

### Equivalence

State and runtime each retain **one allocation**; the same field values are written in the same order; the
same `EntityRuntimeKey` attachment is made. Base fields are still initialized before Canvas fields. The
runtime still holds one shallow derived registry copy (`cloneCanvasRenderRegistries` at the pin, the spread
in the rewrite — the same shallow derivation), and the state's `registries` field still points at **that
aggregate** rather than at the caller's object, which is the property the source comment protects.
Nothing observable — absence, reference identity, equality, ordering, exception shape or task settlement —
changes.

### Expiry

Drop the patch when the emitter preserves `CanvasRenderState` and `CanvasRenderStateRuntime` heritage and
computed-symbol ownership across their `RenderState` bases, or when Flight adopts these exact-owner helpers
upstream.

### Status

The patch was authored against `baseCommit 7e2fc7df` by the 2D lane (10 of 14 removed
lines still present, surrounding source changed). It is **not currently applied**, which is why the module
still refuses at the live base. Rebasing the hunks is the source-patch lane's work, not this request's.

That authoring revision is also why this document's first draft misstated the pin shape: it took its
excerpt from the patch's own context, which is `7e2fc7df`-era source, instead of reading the live checkout.
The excerpt above is the live `a627849` source, and the three differences from the earlier draft
(one `Readonly<CanvasRenderStateOptions>` parameter, no `renderTransform2D`, registries taken from the
runtime aggregate) are listed under **Upstream shape** so the correction is visible rather than silent.

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

## G2 DOM clip hooks assert the DOM render state from its base

`@flighthq/scene2d-dom`'s `domClip.ts` refuses its `domScene2DClipHooks: Scene2DClipHooks` binding with
`cpp-reference-assertion-without-heritage`, at Flight `a627849` / compiler `806d257`. Each hook receives the
shared `RenderState` and calls `getDomRenderStateRuntime(state as DomRenderState)`, which is the same
base-to-derived assertion as the Canvas constructors. `enable_dom_clip_support` still stores the binding,
so `flight/scene2d_dom/dom_clip.hpp:81` fails on `'dom_scene_2d_clip_hooks' was not declared` (GCC 15.2, one
worker). `canvas_clip.hpp:89` (`canvas_clip_hooks`) has the same shape.

The hooks need the exact DOM owner recovered through real heritage or a checked dispatch contract that
validates the state before entering the typed hook. They must never use a pointer cast or a replacement
state. Nothing is carried:
- The declaration repair declares only functions, not a `Ref<Scene2DClipHooks>` variable.
- A hook table would fake clipping.
- The next call, `set_dom_clip_hooks` at line 82, belongs to the wholly refused `domClipRectangle.ts`. See
  [local-interface-extending-lib-dom-interface](local-interface-extending-lib-dom-interface.md).

### G2 Canvas clip hooks

`canvasClip.ts:12` refuses `canvasClipHooks` with the same `cpp-reference-assertion-without-heritage`: every
hook starts with `const s = state as CanvasRenderState`. `enable_canvas_clip` still stores the binding, so
`flight/scene2d_canvas/canvas_clip.hpp:89` fails on `'canvas_clip_hooks' was not declared` (GCC 15.2, one
worker, all errors shown). That is the header's own first diagnostic, not a cascade from an included header.

Every callee the hooks need is already emitted in `canvas_clip_rectangle.hpp`, taking
`Ref<CanvasRenderState>`: `pop_canvas_clip_rectangle`, `push_canvas_clip_rectangle` and
`push_canvas_clip_contours`. So the only missing piece is recovering the exact Canvas owner from the shared
`RenderState` argument, through heritage or a checked dispatch contract, as above. Nothing is carried:
- The hooks are a variable, which the declaration repair cannot declare.
- A hook table would fake clipping.
