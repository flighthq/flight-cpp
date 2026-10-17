# A local re-export is emitted as a `using` before the declaration it names

## Reproduction

`flight/scene2d_canvas/canvas_render_state.hpp`, emitted at `806d257` from
`packages/scene2d-canvas/src/canvasRenderState.ts`:

```cpp
namespace flight::scene2d_canvas {

using flight::scene2d_canvas::get_canvas_render_state_runtime;   // line 69

using flight::types::CanvasRenderState;
// ...

inline flight::Ref<flight::types::CanvasRenderStateRuntime> get_canvas_render_state_runtime(  // line 80
    flight::Ref<flight::types::CanvasRenderState> state) { ... }
```

A `using`-declaration is a name *lookup*: it requires the name to be declared already. Here the function it
names is defined eleven lines below it in the same namespace, so the declaration itself is the error, and
every header that includes this one fails with

```
error: no member named 'get_canvas_render_state_runtime' in namespace 'flight::scene2d_canvas'
```

The `using` comes from a **local re-export** in the TypeScript
(`export { getCanvasRenderStateRuntime }`), which is a no-op when the declaration is already in scope.

## Required shape

A same-namespace `using`-declaration introduces no name, so it is only ever a rename into scope. For a local
re-export the emitter should either emit it after the module's own declarations, or omit it — both leave the
emitted program identical, because the name is already visible in that namespace.

## Affected

34 headers at the G1 sweep, all of them importers of `scene2d-canvas`: the canvas render-state family itself
plus `scene2d-canvas` consumers in 2D, 3D, PLAT, TEXT and WGPU. G1 cluster `missing-member-b1a6d5cf`.

## What this repository carries meanwhile

Nothing yet: the cluster is open and every header in it fails. The fix is a one-line ordering change in the
emitter rather than a workaround here, which is why no override was taken.
