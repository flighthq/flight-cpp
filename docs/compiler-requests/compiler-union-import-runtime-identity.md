# Canonical runtime identity across union imports

One declared union owner can be reached through its defining module's imports and through a consumer's
direct imports. Planning keys those local bindings separately even though they resolve to the same
exported declaration and C++ reference carrier.

## Minimal TypeScript

```ts
// sources.ts
export interface Source { kind: string }
export interface Bitmap extends Source { kind: 'bitmap'; bytes: Uint8Array }
export interface Image extends Source { kind: 'image'; url: string }

// values.ts
import type { Source, Bitmap, Image } from './sources.js';
export type SourceValue = Source | Bitmap | Image;

// consumer.ts
import type { Bitmap, Image } from './sources.js';
import type { SourceValue } from './values.js';
export function size(value: SourceValue): number {
  if (value.kind === 'bitmap') return (value as Bitmap).bytes.length;
  if (value.kind === 'image') return (value as Image).url.length;
  return 0;
}
```

## Expected C++

The imported spellings share one slot per exported owner, while emitted types retain their declaring
namespace:

```cpp
std::get<flight::Ref<types::Bitmap>>(value);
std::get<flight::Ref<types::Image>>(value);
```

## Affected modules

Cube-face `TextureSourceValue` storage and Scene3D GL upload branches that also directly import
`Bitmap` and `ImageResource`.

## Carrying workaround

[`compiler-union-import-runtime-identity.patch`](../../source-patches/compiler-union-import-runtime-identity.patch)
keys uniquely resolved imports by their exported declaration owner without changing emitted type
spelling. It expires when upstream union planning canonicalizes imported runtime identities.
