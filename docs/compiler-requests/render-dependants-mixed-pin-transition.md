# Render dependants require a coordinated mixed-pin transition

Status: package-provenance blocker, not a compiler or missing-header emission defect.

## Symptom

The committed inventory has no `generated/include/flight/types/renderer.hpp`, but exactly six headers
still include it:

- `flight/render/explain_scene2_dcoverage.hpp`
- `flight/render/render_cache.hpp`
- `flight/render/render_registry_signals.hpp`
- `flight/render/render_state.hpp`
- `flight/render/renderer.hpp`
- `flight/scene2d_canvas/canvas_texture_resolver.hpp`

That missing include blocks the focused Canvas texture-resolver target through
`canvas_texture_resolver.hpp` and the focused Dialog target through `render_state.hpp` before either
test translation unit can be compiled.

## Cause

This is an internally inconsistent mixed-pin inventory. The relevant committed manifest rows are:

| package | recorded revision | emitted/source | refused |
|---|---|---:|---:|
| `@flighthq/types` | `a627849` | 1029/1029 | 0 |
| `@flighthq/dialog` | `a627849` | 4/5 | 1 |
| `@flighthq/render` | `7e2fc7d` | 11/25 | 14 |
| `@flighthq/scene2d-canvas` | `7e2fc7d` | 40/51 | 11 |
| `@flighthq/effects-canvas` | `7e2fc7d` | 12/30 | 18 |

At `a627849`, `packages/types/src/Renderer.ts` was replaced by
`packages/types/src/NodeRenderer.ts`; the current `RenderProxy` and `RenderState` types use
`NodeRenderer`. The types slice therefore correctly stopped emitting `renderer.hpp`. The five Render
headers and one Canvas header above are older dependants, not evidence that the current types emitter
lost an output.

A compatibility header or test-only substitute would be unsound: it would preserve an API that the
current source intentionally removed while leaving the dependant packages at the wrong source
revision.

## Scoped generation result

The repository-owned coordinated command was run against the pinned sources:

```text
npm run pkg:generate -- @flighthq/render @flighthq/scene2d-canvas
```

The compiler successfully produced the current candidate closure (`1377/1447` modules emitted, 70
refusals), but the package merge guard rejected the requested two-package move and explicitly left
`generated/` untouched:

```text
@flighthq/scene2d-canvas cannot move alone: at the candidate revision it no longer has modules or
headers that other packages still use at their recorded revision: @flighthq/effects-canvas (uses
packages/scene2d-canvas/src/canvasRenderPass.ts,
packages/scene2d-canvas/src/canvasRenderState.ts,
packages/scene2d-canvas/src/canvasRenderSurface.ts,
packages/scene2d-canvas/src/canvasRenderTexture.ts,
packages/scene2d-canvas/src/canvasTextureRenderTarget.ts).
Move them together, in the same change, or leave @flighthq/scene2d-canvas where it is.
generated is untouched.
```

The package-scope negative and positive fixtures also pass:

```text
node --test --test-name-pattern='a coordinated group may drop a module|a moved package that includes a header no root provides' ./scripts/packageScope.test.mjs
# tests 2; pass 2; fail 0
```

They prove both sides of the guard: a package that references an unavailable header is reported, and a
provider may drop a module only when its importing partner moves in the same atomic group and the
result has no dangling includes.

## Required ownership and next step

A complete repair for both blocked targets is therefore not a narrow Render/Canvas compiler change.
It requires a coordinated provenance move containing at least `@flighthq/render`,
`@flighthq/scene2d-canvas`, and `@flighthq/effects-canvas`. Before authorizing that move, package
ownership must check whether moving `effects-canvas` exposes another reverse-dependant requirement.
Only after the package guard accepts the full atomic group should the focused Render include-order,
Canvas texture-resolver, Dialog, and Scene2D coverage targets be compiled against the real generated
headers.
