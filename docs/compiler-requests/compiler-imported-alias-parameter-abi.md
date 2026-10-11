# Exact imported alias at a parameter boundary

An imported exact ABI alias is emitted as a reference wrapper and reopened into its source union when
passed to an API whose parameter is that identical union.

## Minimal TypeScript

```ts
import type { HostImageSource } from './provider.js';
export function draw(context: CanvasRenderingContext2D, image: HostImageSource): void {
  context.drawImage(image, 0, 0);
}
```

`HostImageSource` aliases `CanvasImageSource`, whose browser arms all map to one native image handle.

## Expected C++

```cpp
void draw(host::CanvasContext context, provider::HostImageSource image) {
  context.draw_image(image, 0.0, 0.0);
}
```

The parameter is a value carrier, not `Ref<HostImageSource>`, and the call must not construct a
variant.

## Affected modules

Canvas atlas, texture-view, texture-window, sprite, particle, quad, tilemap, and pattern draw paths.

## Carrying workaround

[`compiler-imported-alias-parameter-abi.patch`](../../source-patches/compiler-imported-alias-parameter-abi.patch)
emits the imported carrier by value and suppresses reconstruction only when both sides expand to the
same source union. It expires when upstream preserves exact aliases at parameter boundaries.
