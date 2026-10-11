# Exact imported alias inside an outer union

An imported alias used as a nullable callable result is recursively flattened even when its defining
header and external bindings publish one exact native carrier. Distinct browser source types then
collide because the host has deliberately mapped them to the same handle.

## Minimal TypeScript

```ts
// provider.ts
export type HostImageSource = CanvasImageSource;
export type Resolver = () => HostImageSource | null;

// consumer.ts
import type { HostImageSource, Resolver } from './provider.js';
export function resolve(fn: Resolver): HostImageSource | null { return fn(); }
```

## Expected C++

```cpp
std::optional<provider::HostImageSource> resolve(provider::Resolver fn) {
  return fn();
}
```

The outer optional is preserved, while `HostImageSource` remains the exact carrier selected by its
declaring module. It must not reopen into browser alternatives.

## Affected modules

The Canvas texture resolver callback and cache result shared by `@flighthq/types` and
`@flighthq/scene2d-canvas`.

## Carrying workaround

[`compiler-imported-alias-outer-union-abi.patch`](../../source-patches/compiler-imported-alias-outer-union-abi.patch)
keeps an imported alias intact only when resolution reaches an exact external carrier or a target
collision. It expires when upstream union planning preserves exact imported ABI carriers.
