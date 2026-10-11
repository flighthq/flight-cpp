# Exact imported alias through local flow

An inferred local initialized from a nullable imported callable result discards the provider's exact
ABI alias, expands the source union for storage, and expands it again after a null guard.

## Minimal TypeScript

```ts
import type { HostImageSource, Resolver } from './provider.js';
declare function draw(image: CanvasImageSource): void;

export function resolveAndDraw(resolver: Resolver): void {
  const image = resolver();
  if (image === null) return;
  draw(image);
}
```

## Expected C++

```cpp
auto image = resolver(); // std::optional<provider::HostImageSource>
if (!image.has_value()) return;
draw_image(image.value());
```

The narrowed value remains the imported alias carrier rather than a reconstructed DOM union.

## Affected modules

Canvas resolver locals used by particle, quad, sprite, scale-9, tilemap, texture-window, and bitmap
pattern rendering.

## Carrying workaround

[`compiler-imported-alias-local-flow-abi.patch`](../../source-patches/compiler-imported-alias-local-flow-abi.patch)
preserves the foreign result alias in immutable local storage and returns its optional payload after a
presence proof. It expires when upstream local-flow lowering retains exact ABI aliases.
