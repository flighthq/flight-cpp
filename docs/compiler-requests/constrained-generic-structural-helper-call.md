# A constrained generic argument must adapt to a structural helper parameter

A generic function accepts `T extends SomeStructuralInterface` and passes a `T` value to a helper whose
parameter is that interface. TypeScript permits the call through the constraint. The C++ emitter leaves
the caller parameter as unconstrained `T` and calls a function template whose parameter is a
`StructuralRef`; deduction cannot use the conversion from the concrete owner to that view.

## Minimal TypeScript

```ts
interface Bounded { bounds: { x: number; y: number } }
declare function readBounds(value: Bounded): void;
export function visit<T extends Bounded>(values: readonly T[]): void {
  for (const value of values) readBounds(value);
}
```

The emitted call must either project `value` through the known `Bounded` constraint or constrain the C++
template parameter so the helper accepts its carrier. A raw `read_bounds(value)` cannot deduce a
`StructuralRef<...>` parameter from an arbitrary `T`.

## Affected Flight modules

At Flight `a627849` / flight-compiler `2b687ea`, both `lassoSelection.ts` and
`marqueeSelection.ts` declare `NodeType extends BoundsNodeAny` and call
`getNodeLocalBoundsRectangle(candidate)`. Their headers compile uninstantiated, but the source's ordinary
`BoundsNodeAny[]` call fails natively:

```
no matching function for call to 'get_node_local_bounds_rectangle'
candidate template ignored: could not match 'flight::StructuralRef' against 'std::shared_ptr'
```

This is distinct from generic argument inference: `NodeType` is already known exactly. What is lost is
the parameter constraint that supplies the structural conversion at the nested call.

## Carrying workaround

The selection lasso and marquee completions adapt the exact generated `Ref<BoundsNodeAny>` carrier to its
bounds runtime and expose the same readonly rectangle view; other already-structural node carriers retain
the generated node helper. The adapter neither copies the candidate nor its rectangle. It expires when a
constrained generic call projects through its declared structural bound.
