# An intersection with a shapeless conjunct must lower to the shaped conjunct

`ReturnType<typeof f> & object` is used to state "a non-primitive result of `f`". The emitter tries to
lower the intersection by multiple inheritance and refuses because `object` (and an `unknown`-shaped
return) has no shape.

## Minimal TypeScript

```ts
declare function createPathMorph(): { start: number; end: number };
interface MorphShape { morph: ReturnType<typeof createPathMorph> & object }
```

## Required C++ shape

`T & object`, where `T` is already an object type, is `T`; an intersection with a shapeless conjunct
contributes no members and should reduce to the shaped side before any inheritance lowering:

```cpp
struct MorphShape { flight::Ref<PathMorph> morph; };
```

## Affected modules

`swfMorphShape.ts` in `@flighthq/swf`: refused, not supplied.
