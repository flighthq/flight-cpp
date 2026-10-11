# Union carriers need canonical alternative order

The same named union is independently expanded in an array element and its callback parameter, but the
emitter orders the alternatives differently. The resulting `std::variant` specializations are unrelated
C++ types, so an ordinary `Array.every` call cannot pass its element to the generated callback.

## Minimal TypeScript

```ts
interface Image { readonly kind: 'image'; width: number }
interface Voxel { readonly kind: 'voxel'; width: number }
type Source = Image | Voxel;
type Faces = readonly [Source | null, Source | null];

export function complete(faces: Faces): boolean {
  return faces.every((face) => face !== null);
}
```

## Expected C++

Every expansion of `Source` must reuse one canonical carrier, including the same alternative order:

```cpp
using Source = std::variant<flight::Ref<Image>, flight::Ref<Voxel>>;
using Face = std::optional<Source>;
return faces.every([](Face face) { return face.has_value(); });
```

The order itself is not observable in TypeScript, but it is part of `std::variant`'s C++ type identity.
Choosing it once per declared union also prevents index-based narrowing from drifting between use sites.

## Affected modules

`isCubeTextureComplete` in `@flighthq/texture` stores `TextureSourceValue` in
`TextureSourceCubeFaces`; its synthesized `every` callback contains the same seven owners in a different
order and fails at `std::invoke`.

## Carrying workaround

[`texture-direct-cube-face-storage.patch`](../../source-patches/texture-direct-cube-face-storage.patch)
spells the fixed six-element tuple check as ordered constant-index tests. It evaluates the face storage
once and short-circuits on the first null in the same order as `every`. The request closes when every
expansion of a named union reuses its canonical native carrier.
