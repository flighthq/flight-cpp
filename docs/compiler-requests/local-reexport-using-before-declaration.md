# An obsolete repair inserts a same-namespace `using` before its definition

G3 corrects this request's original attribution. At Flight `a627849` and compiler `806d257`,
`canvasRenderState.ts` directly declares `export function getCanvasRenderStateRuntime`; it has no
local re-export. The compiler represents that export as an IR local binding and emits the function
directly. Both a direct-export fixture and an authored `export { getCanvasRenderStateRuntime }`
fixture produce no same-namespace using-declaration.

The failing line below comes from the repository's
`scene2d-canvas-unqualified-scene2d-canvas-names` emission repair. Its package-wide symbol match also
matches the defining function, inserts a self-include, and places a redundant using-declaration before
the definition. Applying that repair alone to either actual compiler fixture reproduces both insertions.

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

The using-declaration looks up the current namespace's own function before it has been declared.
It does not represent a missing source export or a separate callable owner.

## Required shape

Retain the compiler's local definition and ordinary dependency includes. At the current pin,
`emitReexportsCpp` already emits no declaration for a local export or an unrenamed same-package
re-export. Imported calls use the exact declaring namespace and allocated function name.

## Affected

34 headers at the G1 sweep, all of them importers of `scene2d-canvas`: the canvas render-state family itself
plus `scene2d-canvas` consumers in 2D, 3D, PLAT, TEXT and WGPU. G1 cluster `missing-member-b1a6d5cf`.

## G3 repair retirement

The obsolete repair is removed from `repairs/emission-repairs.json`; no compiler patch is needed for
this cause. Its recorded generated footprint is exactly `canvas_render_state.hpp` and
`canvas_render_surface.hpp`. The latter already has the compiler's real owner-header include, exact
function prototype and qualified calls. Focused two-module compiler graphs verify the dependency
include and original callable through both direct/local exports and an aliased source import.

The declared-repair regression in `scripts/emissionRepairs.test.mjs` applies the complete current
repair configuration to the definition and its dependent header. It preserves the definition, required
include and qualified call byte for byte, including a repeated repair pass. Removing the obsolete
entry changes no source declaration, initialization, import order, storage, function body or recursion;
it introduces no forwarding function, copied owner, stub, or declaration for a refused body.

A diagnostic-only copy with just the obsolete using/self-include removed gets past that lookup but
still fails in the transitive `canvas_render_surface.hpp`: `get_canvas_host` constructs a writable
`HostCanvasCapability` row from an existing readonly row before returning a readonly row. The runtime
correctly rejects adding writes. The original Canvas header includes its texture resolver, which
includes that surface header, so the actual public getter/helper consumer remains blocked there.
No helper execution or automatic promotion of the historical 40-header family follows from retirement.
The adjacent return correction is now carried separately as
[`compiler-present-readonly-row-context.patch`](../../source-patches/compiler-present-readonly-row-context.patch),
with its exact optional-storage guards and focused evidence in the existing
[readonly-view request](readonly-view-to-writable.md#g3-matching-present-readonly-returns).
Normal scoped regeneration and original-header qualification remain queued for a generation slot.

With the predicted readonly return also corrected in the diagnostic full-header copy, the actual
whole-header consumer next fails in `destroy_canvas_render_state`: the original `CanvasRenderState`
cannot enter `destroy_render_state(Ref<RenderState>)`. The flattened states have different refined
runtime-owner cells, so a copied base prefix or a second runtime would break live identity. This is
the existing [render-state teardown heritage boundary](interface-heritage-widening.md#g2-gl-view-destruction-instantiates-the-deferred-state-teardown),
now reached through Canvas. It remains outside the compatible-storage heritage subset. No teardown
body is suppressed and no fake conversion is supplied; the existing passing types/render owner test
is unchanged. This diagnostic run executes no public Canvas helper and is not a final regenerated
header qualification or a resolved-cause claim.
