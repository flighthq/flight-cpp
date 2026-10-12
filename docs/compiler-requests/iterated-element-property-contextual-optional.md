# An iterated element's property must retain its type at a contextual optional field

An array element has a concrete declared owner inside a `map` callback or `for...of` body, and a direct
property read has a checker-recorded primitive type. When that read fills a nullable or optional field
of a contextual object literal, the C++ emitter nevertheless reports that the receiver reached storage
erased and refuses the surrounding function.

Measured at Flight `a627849` / flight-compiler `2b687ea` in two functions of
`@flighthq/spritesheet`: `createSpritesheetFromData` and `validateSpritesheetData`.

## Minimal TypeScript

```ts
interface FrameData { offsetX: number }
interface FrameOptions { offsetX?: number }
interface AnimationData { name: string; frameNames: string[] }
interface Diagnostic { animationName: string | null; message: string }

declare function createFrame(options: FrameOptions): void;

export function hydrate(frames: readonly FrameData[]): void {
  frames.map((frame) => createFrame({ offsetX: frame.offsetX }));
}

export function validate(animations: readonly AnimationData[]): Diagnostic[] {
  const diagnostics: Diagnostic[] = [];
  for (const animation of animations) {
    if (animation.frameNames.length === 0) {
      diagnostics.push({ animationName: animation.name, message: animation.name });
    }
  }
  return diagnostics;
}
```

The complete package sources have more fields, but the first refusal in each function is the same
boundary: `fd.offsetX` enters `Partial<SpritesheetFrame>.offsetX`, and `ad.name` enters nullable
`SpritesheetValidationDiagnostic.animationName`.

## Observed

Both declarations are replaced by `NOT GENERATED` blocks. Their diagnostics differ only in the member:

```text
property offsetX has a checker-recorded result type, but its receiver reached C++ storage erased;
preserve the receiver's concrete type through destructuring or iteration before constructing the
contextual optionalSingle union

property name has a checker-recorded result type, but its receiver reached C++ storage erased;
preserve the receiver's concrete type through destructuring or iteration before constructing the
contextual optionalSingle union
```

The first omission keeps `spritesheet_from.hpp` partial and causes both package aggregators to be
dependency-refused. The second leaves only `validateSpritesheetData` missing; `validateSpritesheet`
itself emits at this pin.

## Required C++ shape

The iterated value must keep its exact declared owner until the property read has chosen its carrier:

```cpp
flight::Ref<FrameData> frame = /* callback or iteration element */;
auto options = flight::make_ref<FrameOptions>(FrameOptions{
    .offset_x = std::optional<double>{frame->offset_x},
});
```

The same rule applies to `animation->name` entering the diagnostic's optional string. No erased
`flight::Any` recovery, unchecked cast, copied owner, or default value is needed: the source collection
already declares the element type and the property result is already checker-recorded.

## Distinguish from optional presence defects

This is not [`nullable-optional-presence.md`](nullable-optional-presence.md). The source value being
inserted is present and non-null (`number` or `string`); only the *destination member* is optional or
nullable. It is also not an indexed-element absence proof: both sites have already obtained their
element through a typed array callback or iterator.

## Carrying workaround

No generated declaration is supplied. A source rewrite may bind each property immediately before the
same contextual object construction, preserving read count and evaluation order; that experiment must
clear the complete function, not merely move the first refusal, before it is carried. At this pin that
experiment moved `createSpritesheetFromData` from `offsetX` to `pivotX`, then moved
`validateSpritesheetData` to a later nullable-property presence test. Neither rewrite is carried because
neither restored its complete declaration.
