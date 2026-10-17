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

## G2 SWF video handler: the callable is represented, its signature is not adapted

At Flight `a627849` / compiler `806d257`, the refusal for `swfVideoHandler` (`swfVideoHandler.ts:18`,
`cpp-contextual-union-value-type-unrepresented`) has moved past the arm question. It now says "A callable is
a represented domain", and the obstacle is that the method's own signature is not the slot's:

- The value is `createPlacementNode(parsed, characterId, bounds)` returning `std::optional<Ref<Sprite>>`.
- The slot is `SwfTagHandler.createPlacementNode(parsed, characterId, bounds, diagnostics: ImportDiagnostic[] | undefined): Node2D | null`.

TypeScript accepts both differences: a function may declare fewer parameters than its slot passes, and a
`Sprite | null` result widens to `Node2D | null`. The lowering needs a callable that takes the slot's four
parameters, ignores the trailing one, and returns the same `Sprite` reference as a `Node2D`. No copy and no
replacement node: the identity must survive, as in
[interface-heritage-widening](interface-heritage-widening.md).

The binding is refused, but `swfVideoTagFamily.ts` still names it. So
`flight/swf/swf_video_tag_family.hpp:15` fails with `'swf_video_handler' is not a member of 'flight::swf'`
(GCC 15.2, one worker). This is the retained-binding class of
[custom-shader-effect-runner-owner](custom-shader-effect-runner-owner.md). `swf_video_handler.hpp` itself
compiles. Nothing is carried:
- The declaration repair declares functions, not a `Ref<SwfTagHandler>` variable.
- A runtime handler would fake video placement.
- A source patch was outside this unit's approved routes.
