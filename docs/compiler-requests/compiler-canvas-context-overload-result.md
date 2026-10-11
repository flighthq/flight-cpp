# Literal-selected Canvas context result

`HTMLCanvasElement.getContext` loses its selected result when the overload set is lowered as one
ambient callable surface. A literal `"2d"` call must retain `CanvasRenderingContext2D | null`, just as
the existing `"webgl2"` special case retains `WebGL2RenderingContext | null`.

## Minimal TypeScript

```ts
export function context(canvas: HTMLCanvasElement): CanvasRenderingContext2D | null {
  return canvas.getContext('2d', { alpha: false });
}
```

## Expected C++

With the Canvas binding profile applied, the selected result remains the nullable bound carrier:

```cpp
std::optional<flight::host_sdl::CanvasRenderingContext2D> context(
    flight::host_sdl::HtmlCanvasElement canvas) {
  return canvas.get_context_2d(/* alpha: false */);
}
```

The important contract is the selected `CanvasRenderingContext2D | null` evidence; the exact bound
method spelling is profile-owned.

## Affected modules

`packages/scene2d-canvas/src/canvasRenderSurface.ts`, particularly `finishCanvasRenderSurface`, and
any consumer of the literal `"2d"` overload.

## Carrying workaround

[`compiler-canvas-context-overload-result.patch`](../../source-patches/compiler-canvas-context-overload-result.patch)
extends the semantic lowering rule from `"webgl2"` to `"2d"`. It expires when flight-compiler records
the literal-selected Canvas result upstream.
