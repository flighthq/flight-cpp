# Recovering an exact call-result alias

Cross-package call evidence has two legitimate lanes: the callable return type and the
checker-selected call result. Either lane may expand a published exact ABI alias. Consulting only the
callable lane loses the carrier even when the result lane still names it.

## Minimal TypeScript

```ts
// provider.ts
export type HostImageSource = CanvasImageSource;
export type Resolver = () => HostImageSource | null;

// consumer.ts
import type { HostImageSource, Resolver } from './provider.js';
export function resolve(fn: Resolver): HostImageSource | null { return fn(); }
```

The binding profile maps every `CanvasImageSource` arm to one host `ImageSource` carrier.

## Expected C++

```cpp
std::optional<provider::HostImageSource> resolve(provider::Resolver fn) {
  return fn();
}
```

No DOM `std::variant`, `Any`, `Ref<HostImageSource>`, or cast may be introduced.

## Affected modules

Cross-package calls to `resolveCanvasTexture` and `resolveCanvasTextureWindowSource` from
`@flighthq/scene2d-canvas` consumers.

## Carrying workaround

[`compiler-imported-alias-call-declaration-recovery.patch`](../../source-patches/compiler-imported-alias-call-declaration-recovery.patch)
searches both result-evidence lanes for the same exact imported alias and sentinel. It expires when
upstream canonicalizes published exact result aliases before either lane expands them.
