# `new Set(existingSet)` must copy, not alias

Silent miscompilation, the Set twin of [`new Map(existingMap)` must copy, not alias](new-map-copy-lowered-to-alias.md).

In TypeScript, `new Set(set)` builds a new Set holding the same values in the same order. Adding to or deleting
from either set is invisible to the other.

The emitter lowers it to `flight::Set<T>(set)`. When the argument already has that exact type, C++ prefers
`Set`'s implicit copy constructor to its `explicit Set(const Range&)` constructor. The copy constructor shares
storage, because a JavaScript Set passed by value is the same object. So the "copy" is the caller's set.

## Minimal TypeScript

```ts
export function copy(set: ReadonlySet<string>): Set<string> {
  return new Set(set);
}

const s = new Set(['b', 'a']);
const c = copy(s);
c.add('z');
c.delete('b');
// node: s is ['b', 'a'], c is ['a', 'z']
```

Emitted at flight `a627849` (flight-compiler `2b687ea` with every carried patch, including `compiler-new-map-copy`):

```cpp
return flight::Set<flight::String>(set);   // copy constructor: shares set's storage
```

Measured natively, compiling and running the emitted header:
- `new Set(set)` and `new Set<string>(set)` both alias, so writes to the copy reach the original.
- Insertion order is kept, because it is the same storage.
- The controls behave correctly: `new Set(array)` (unique values in first-seen order) and live iteration (a value
  added during `for...of` is visited).

## Required C++ shape

`new Set(iterable)` must construct a new Set. When the argument is itself a `flight::Set`, `Set::clone()` copies
its values in insertion order into new storage:

```cpp
return flight::Set<flight::String>(set).clone();
```

Arrays, iterators and literals already reach the range constructor and are correct.

## Affected modules

At flight a627849, 17 SDK `new Set(<expression>)` sites were type-checked. Exactly one passes a Set:
`@flighthq/scene3d-formats` `objRequirements.ts:35`, `new Set(collectObjFeatures(source))` with a
`ReadonlySet<string>` argument.

The other 16 pass arrays or `MapIterator`s and are not affected:
- assets ×3, render ×3, requirement ×3, scene-document, scene2d-formats, scene3d-formats `awd2LightingHandler` ×2,
  scene3d-resources, snapshot, spritesheet.

Tool and host packages (`tool-*`, `host-*`, `vite-plugin-*`) are outside the SDK and were not checked.

## Carried compiler patch

[`compiler-new-set-copy.patch`](../../source-patches/compiler-new-set-copy.patch) spells a flight-cpp
`new Set(argument)` whose single argument is itself a `Set` or `ReadonlySet` as `flight::Set<T>(argument).clone()`,
exactly as `compiler-new-map-copy` does for Map. Values arrays, literals and empty construction are emitted as
before. The values-iterator form (`new Set(set.values())`) was already refused by an unrelated rule
(`contextual optionalSingle construction requires expression type evidence`) and still is. Fixtures are in
`cppNewSetCopy.test.ts`. No repair was carried, so none is retired. The `scene3d-formats` site changes when its
owner next regenerates.
