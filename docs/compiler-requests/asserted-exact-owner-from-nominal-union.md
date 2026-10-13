# An assertion from a nominal union must recover its exact owner alternative

A call returning a represented nominal-reference union is asserted to one concrete reference type.
The destination is an exact alternative of the union, but the emitter treats it only as a structural
facet and refuses because that facet carries no separate nominal reference.

## Minimal TypeScript

```ts
interface Texture2D { readonly dimension: '2d'; version: number }
interface Texture3D { readonly dimension: '3d'; version: number }
type Texture = Texture2D | Texture3D;

declare function cloneTexture(value: Readonly<Texture>): Texture;
export function clone2D(value: Readonly<Texture2D>): Texture2D {
  return cloneTexture(value) as Texture2D;
}
```

## Observed

At Flight `a627849` and flight-compiler `2b687ea`,
`@flighthq/textureatlas` refuses `getTextureAtlasRegionTexture` with
`cpp-structural-variant-assertion-without-nominal-storage`:

```text
target flight::Ref<flight::types::Texture2D> is a structural facet of variant alternative
flight::Ref<Texture2D>, but that alternative retains only its anonymous referent and carries no
nominal target reference to recover
```

The runtime `Texture` carrier is a `std::variant` whose alternatives are the exact nominal
`Ref<Texture2D>`, `Ref<Texture2DArray>`, `Ref<Texture3D>`, and `Ref<TextureCube>` owners. No new owner
or copied row is required for this assertion.

## Expected lowering

Select the destination's exact nominal alternative and retain that same reference:

```cpp
auto cloned = clone_texture(source);
return std::get<flight::Ref<Texture2D>>(cloned);
```

Selection must be exact. A sibling alternative with the same members must not be accepted, and the
lowering must not materialize a new Texture2D or recover mutable authority from a merely compatible
structural row. Where the active alternative is not the asserted owner, the runtime's existing checked
variant failure is preferable to an unchecked cast.

## Affected module and carrying workaround

`packages/textureatlas/src/textureAtlasRegion.ts`, function `getTextureAtlasRegionTexture`. Its input
page is already the exact `Texture2D` owner stored by `TextureAtlas`, and `cloneTexture` preserves that
dimension. The package override constructs the exact input union arm and extracts the exact output arm,
preserving the cloned owner and the source's cache identity. It expires when the emitter selects the
nominal alternative directly.

This differs from `readonly-structural-owner-to-nominal-union.md`, which constructs a union from a
readonly row. This request is the read side after the nominal union already exists.
