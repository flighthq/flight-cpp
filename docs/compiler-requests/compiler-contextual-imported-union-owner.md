# Contextual construction of an imported union

Contextual construction enters an imported union alias's declaring module while still holding the
consumer-side import binding. The alias then degrades to a nominal reference instead of its declared
variant.

## Minimal TypeScript

```ts
// types.ts
export interface Source { kind: string }
export interface Bitmap extends Source { kind: 'bitmap'; bytes: Uint8Array }
export type SourceValue = Source | Bitmap;

// consumer.ts
import type { Source, SourceValue } from './types.js';
export function retain(source: Source | null): SourceValue | null { return source; }
```

## Expected C++

The result must be the declaring module's optional variant and construct its `Source` arm:

```cpp
std::optional<types::SourceValue> retain_source(std::optional<flight::Ref<types::Source>> source) {
  if (!source) return std::nullopt;
  return types::SourceValue{
      std::in_place_type<flight::Ref<types::Source>>, source.value()};
}
```

## Affected modules

Core texture queries and GL upload dispatch that return nullable imported `TextureSourceValue` in
`@flighthq/texture` and its consumers.

## Carrying workaround

[`compiler-contextual-imported-union-owner.patch`](../../source-patches/compiler-contextual-imported-union-owner.patch)
resolves the import in the consumer before reconstructing the alias in its declaring module. It
expires when contextual union construction preserves imported alias ownership upstream.
