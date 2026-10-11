# Readonly structural owner entering a nominal union

A readonly structural row retains its exact concrete owner, but contextual union construction rejects
the row because its emitted type is not literally one of the destination's nominal alternatives.

## Minimal TypeScript

```ts
interface Texture2D { readonly dimension: '2d'; version: number }
interface TextureCube { readonly dimension: 'cube'; version: number }
type Texture = Texture2D | TextureCube;
type TextureLike = Readonly<Texture2D> | Readonly<TextureCube>;

declare function cloneTexture(source: TextureLike): Texture;
export function cloneCube(source: Readonly<TextureCube>): Texture {
  return cloneTexture(source);
}
```

## Expected C++

The row's retained native owner identifies the one exact destination arm:

```cpp
return clone_texture(TextureLike{
    std::in_place_type<flight::Ref<TextureCube>>,
    flight::structural_ref_cast<flight::Ref<TextureCube>>(source)});
```

The recovery must check that the retained owner is exactly `TextureCube`; a layout-compatible sibling
must not enter that arm. No object copy, materialized replacement, or side storage is permitted.

## Affected modules

`cloneCubeTexture` in `@flighthq/texture` is the package-level refusal recorded for
`packages/texture/src/cubeTexture.ts`.

## Carrying workaround

There is no semantics-preserving source workaround in this repository. The dimension-specific clone
helpers retain exact owners internally, but the public readonly `CubeTexture` boundary still has to enter
the declared `TextureLike` union. The module remains an explicit refused placeholder until the compiler
uses the retained native owner to select a unique nominal arm.
