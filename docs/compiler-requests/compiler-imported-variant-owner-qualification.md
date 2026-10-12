# Imported variant owner qualification

Exact variant insertion and assertion compare an imported owner's declaring-module spelling with a
consumer-local spelling. The same declaration therefore fails to match its existing union slot.

## Minimal TypeScript

```ts
// types.ts
export interface Texture2D { readonly dimension: '2d'; version: number }
export interface TextureCube { readonly dimension: 'cube'; sources: readonly number[] }
export type Texture = Texture2D | TextureCube;

// consumer.ts
import type { Texture, Texture2D } from './types.js';
export function insert(value: Texture2D): Texture { return value; }
export function cubeSize(value: Texture | null): number {
  return value?.dimension === 'cube' ? value.sources.length : 0;
}
```

## Expected C++

```cpp
types::Texture insert(flight::Ref<types::Texture2D> value) {
  return types::Texture{
      std::in_place_type<flight::Ref<types::Texture2D>>, value};
}
```

The cube branch must select the qualified `flight::Ref<types::TextureCube>` slot before reading
`sources`.

## Affected modules

The environment cube/IBL chain and particle-atlas texture insertion in `@flighthq/scene3d-gl`.

`@flighthq/movieclip` reaches a site the carrying compiler patch does not: assigning the
`Texture2D | null` result of `getTextureAtlasRegionTexture` into the nullable `Texture` member
`bitmap.data.texture` (`spritesheetTimelineSource.ts`) is refused as a nullable single-carrier conversion.
The `movieclip-spritesheet-texture-narrowed-insertion` source patch narrows the value first so it
takes the covered concrete-value insertion.

## Carrying workaround

[`compiler-imported-variant-owner-qualification.patch`](../../source-patches/compiler-imported-variant-owner-qualification.patch)
qualifies both runtime owners and destination slots through their declaring module. It expires when
upstream exact variant operations canonicalize imported names.
