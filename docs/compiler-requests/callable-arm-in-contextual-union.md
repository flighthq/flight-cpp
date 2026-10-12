# A callable must be a represented arm of a contextual union

An object literal is checked against a union-typed context whose arms include function-valued members,
such as a tag handler table, or a texture factory passed where `Texture | (() => Texture)` is accepted. The
emitter refuses because `std::function<...>` is not a represented runtime domain of that union carrier.

## Minimal TypeScript

```ts
type TextureSource = Texture2D | ((width: number, smooth: boolean, repeat: boolean) => Texture2D);

export function fill(source: TextureSource): void { /* ... */ }

fill((width, smooth, repeat) => createTexture(width, smooth, repeat));
```

## Required C++ shape

A union carrier that admits the callable as its own alternative, discriminated from the object arms:

```cpp
using TextureSource = std::variant<flight::Ref<Texture2D>, std::function<flight::Ref<Texture2D>(double, bool, bool)>>;
```

## Affected modules

In `@flighthq/swf`, none supplied:

- `swfDefineShapeHandler.ts` and `swfDefineMorphShapeHandler.ts`: `std::function<Ref<Texture2D>(double, bool, bool)>` arm.
- `swfJpegBitmapHandler.ts` and `swfVideoHandler.ts`: a callable placement-node factory arm returning
  `std::optional<Ref<Sprite>>`.

Related: [contextual union value domains](animation-track-number-carrier-and-optional-easing.md).
