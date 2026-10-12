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

[`compiler-constrained-type-parameter-union-slot.patch`](../../source-patches/compiler-constrained-type-parameter-union-slot.patch).

The defect is in union planning, not local declaration. A local of type `T` already keeps `T`, but a union
naming a constrained `T` opened `T` to its constraint's runtime domain, and that domain then also chose the
alternative's C++ spelling. The patch changes two things:
- **Planning:** a type parameter whose constraint is one non-union runtime domain stays its own
  alternative, spelled `T`. A constraint that is itself a union of several domains still opens.
- **Construction:** a value of such a parameter enters that alternative directly. A destination union
  spelled with the constraint itself still converts through the existing lanes.

Verified two ways:
- by `cppConstrainedTypeParameterStorage.test.ts`: 10 cases, 6 of which fail without the patch, and 4
  controls (unconstrained `T`, a scalar constrained local, a push into a constraint-spelled union, and a
  union constraint);
- by the unchanged set of pre-existing backend failures.

`@flighthq/signals` is regenerated with the patch, and `emitSignalSafe` instantiates for `void()`,
`void(double)` and `void(String)`. Signals' `signals-generic-slot-operations` emission repair, which
respelled exactly these slot operations to `T`, matched nothing after the patch and was removed.

Reaching the instantiation exposed two masked defects in `safe.hpp`, both the same shapes `slot.hpp`
already repairs. They are carried as sibling repairs:
- the nested length-bounded slot read (`signals-safe-flatten-generic-slot-read`);
- the nullable slot identity comparison (`signals-safe-tombstone-identity-comparison`).

It also exposed a third, which is not repaired: the dispatch loops forward the argument pack on every slot
call. See [forwarded-parameter-pack-reused](forwarded-parameter-pack-reused.md).
