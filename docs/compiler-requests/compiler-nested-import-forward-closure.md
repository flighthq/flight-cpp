# Forward declarations for nested imported aliases

An early collection alias can expand an imported union that itself imports reference owners. The
generated alias names those nested owners before any include or forward declaration makes them visible.

## Minimal TypeScript

```ts
// sources.ts
export interface Source { readonly kind: string }
export interface Bitmap extends Source { readonly bytes: Uint8Array }
export interface Image extends Source { readonly url: string }

// source-value.ts
import type { Source, Bitmap, Image } from './sources.js';
export type SourceValue = Source | Bitmap | Image;

// faces.ts
import type { SourceValue } from './source-value.js';
export type Faces = readonly [SourceValue, SourceValue, SourceValue];
```

## Expected C++

```cpp
namespace flight::types {
struct Source;
struct Bitmap;
struct Image;
using Faces = std::array<SourceValue, 3>;
}
#include <flight/types/source_value.hpp>
```

All owner declarations used by the early alias must precede it or otherwise be visible before use.

## Affected modules

`TextureSourceCubeFaces` and downstream PBR material headers that use imported `TextureSourceValue`
owners.

## Carrying workaround

[`compiler-nested-import-forward-closure.patch`](../../source-patches/compiler-nested-import-forward-closure.patch)
extends early forward discovery across nested imported aliases. It expires when upstream computes the
full forward-declaration closure.
