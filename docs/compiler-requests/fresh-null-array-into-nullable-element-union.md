# A fresh array of `null` assigned to a nullable element union

```ts
export interface Table<T> { entries: (T | null)[]; vocabulary: readonly string[] }
export function create<T>(vocabulary: readonly string[]): Table<T> {
  return { entries: vocabulary.map(() => null), vocabulary };
}
```

```text
an array of [null] cannot be passed where the element union is [T, null]: the target's container is
parameterized by its element type, so this would need a different container, and building one would
copy the array and change the identity the source passes.
```

TypeScript infers `null[]` for `vocabulary.map(() => null)`, and accepts it as `(T | null)[]` because
arrays are covariant. In C++ those are two container types, and the emitter correctly refuses to copy an
array whose identity the source might share.

Here the array is a fresh temporary: the `map` result has no other holder, so no array identity can be
observed. Building it at the target element type from the start, so that `map` produces
`flight::Array<std::optional<T>>`, changes nothing the program can see. The request is to elect the
contextual element type for an array created in the same expression it is assigned from. Arrays that
already have a holder stay refused.

Measured at flight-compiler `2b687ea`, with every carried patch applied, against flight `a627849`. The
repro above fails as shown. With the callback's return annotated, `(): T | null => null`, it lowers.

## Affected

`@flighthq/registry` `registryTable.ts` `createOrdinalTable` (line 11), the module's only refusal at
`a627849`; `contract` and `index` cascade from it.

## Carrying workaround

None yet. A type-only source patch, annotating the callback's return as `T | null`, would lower it with
no runtime difference. Registry cannot move alone at this pin anyway (see the registry move report), so the
patch belongs to that coordinated move.
