# A local typed by a constrained type parameter is stored as the constraint

Inside a generic function, a local whose type mentions a type parameter with a constraint is declared
with the constraint's lowering instead of the parameter. Function parameters keep `T`, so the first copy
from a parameter-typed value into such a local does not convert. Because the gate compiles headers without
instantiating templates, the header passes. Every instantiation fails.

## Minimal TypeScript

```ts
export interface Box<T> { items: (T | null)[] }

export function count<T extends (...args: any[]) => void>(box: Box<T>): number {
  const items = box.items.slice();
  return items.length;
}
```

At flight-compiler `2b687ea` with all 38 of this repository's compiler source patches applied:

```cpp
template <typename T>
inline double count(flight::Ref<Box<T>> box) {
  flight::Array<std::optional<std::function<void(flight::Array<flight::Any>)>>> items = box->items.slice();
  // no viable conversion from Array<optional<T>> once T is anything but that exact std::function
  ...
}
```

## Pinned boundary

| type parameter | local | emitted local type |
| --- | --- | --- |
| `T extends (...args: any[]) => void` | `const items = box.items.slice()` | `Array<optional<std::function<void(Array<Any>)>>>` |
| same | `const items: (T \| null)[] = box.items.slice()` (annotated) | the same: the annotation does not help |
| same | `const items = box.items` (no call) | the same |
| same | `const item = box.items[0]` | `optional<optional<std::function<void(Array<Any>)>>>` |
| `T extends object` | `const items = box.items.slice()` | `Array<optional<flight::ErasedRef>>`, also wrong |
| `T` (unconstrained) | `const items = box.items.slice()` | `Array<optional<T>>`, correct |

The required shape spells the local with `T`, as the unconstrained case already does.

## Affected Flight modules

`@flighthq/signals` `safe.ts` at flight `a627849`, `emitSignalSafe`: its snapshot
`const slots = data.slots.slice()` emits as `Array<optional<std::function<void(Array<Any>)>>>`
(`generated/include/flight/signals/safe.hpp:83`). The same mistyped slot is then passed to
`tombstone_once_slot<T>`. `emitSignalSafe` cannot be instantiated for any signal whose slot type differs from that exact
`std::function`. Instantiations for `void()` and `void(double)` were measured, and both fail with
`no viable conversion`. All seven `safe.test.ts` cases are excluded from
`tests/generated_signals_test.cpp` on this, and so are the safe-dispatch connection cases (which also need
the refused `connectSignalTracked`).

## Workaround carried

None.
