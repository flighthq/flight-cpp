# Narrow an optional local in the else-branch of a negated truthiness test

An untyped or optional local that is assigned on some paths is emitted as `std::optional<T>`. The
emitter unwraps it inside `if (x) { … }`, but not in the `else` of `if (!x) { … }`, so the else
branch passes the still-optional value where `T` is required.

## Minimal TypeScript

```ts
export function pick(out: Rectangle, a: Readonly<Rectangle>, useA: boolean): void {
  let bounds;
  if (useA) bounds = a;
  if (!bounds) {
    out.x = 0;
  } else {
    copyRectangle(out, bounds); // bounds: Readonly<Rectangle>
  }
}
```

## Expected C++

The else branch reads `bounds.value()` (or binds a narrowed local), exactly as the then-branch of
`if (bounds)` already does.

Observed at `2b687ea`, at flight `a627849`, in `flight/node/bounds_rectangle.hpp`:

```
no viable conversion from 'std::optional<flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::Rectangle>>>>>'
to 'flight::StructuralRef<...>'
```

## Affected modules

`packages/node/src/boundsRectangle.ts` (`computeNodeBoundsRectangle`). The same header failed at
`7e2fc7d`, so this is not new with the repin.

## Carrying workaround

[`node-bounds-rectangle-explicit-narrowing.patch`](../../source-patches/node-bounds-rectangle-explicit-narrowing.patch)
annotates the local and swaps the branches to a positive test. It expires when the emitter narrows
the else-branch of a negated truthiness test.
