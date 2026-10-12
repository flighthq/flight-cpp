# A generic key-remapping mapped type must not erase to an opaque value

A mapped type whose keys are computed from a *generic* parameter lowers to `flight::Any`. The
declaration then accepts the operations the erased parameter's type position allows, and the runtime
fails on the first one that needs a key set — which the erasure discarded.

## Minimal TypeScript

```ts
// packages/types/src/Tween.ts
export type NumericProps<T> = { [K in keyof T as T[K] extends number ? K : never]?: number };

// packages/tween/src/internal.ts
export function makeTween<T extends object>(
  target: T,
  duration: number,
  propertyMap: Readonly<NumericProps<T>>,
  options: Readonly<TweenOptions> | undefined,
  defaultEase: EasingFunction,
): Tween<T> {
  const keys = Object.keys(propertyMap);   // needs the mapped key set
  …
}
```

## Expected behavior

`NumericProps<T>` keeps a key set derived from `T`. At a use site the keys are statically known — a
concrete `T` fixes them — so `Object.keys` is a closed-record enumeration, which the profile admits,
and `propertyMap[key]` is an ordinary numeric read. Where the parameter is genuinely open, the
expression is outside the portable profile and belongs in a structured refusal rather than in emitted
code that cannot compile.

## Observed at `2b687ea` / `a627849`

```cpp
// generated/include/flight/types/tween.hpp:20
using NumericProps = flight::Any;
// generated/include/flight/types/tween.hpp:46
NumericProps<T> property_map;

// generated/include/flight/tween/_internal_internal.hpp:61
flight::Array<flight::String> keys = flight::object_keys(property_map);   // property_map : flight::Any
```

which selects the generic `object_keys<Entries>` overload in `include/flight/object.hpp:35` and fails
on `entries.begin()`:

```
include/flight/object.hpp:37:52: error: no member named 'begin' in 'flight::Any'
generated/include/flight/tween/_internal_internal.hpp:61:48: note: in instantiation of function
  template specialization 'flight::object_keys<flight::Any>' requested here
```

The three sibling `= flight::Any` aliases in the generated tree — `NativeWindowHandle`,
`NativeSurfaceHandle`, `ElectronShellCapabilitiesFor` — are opaque external handles and are correctly
opaque. `NumericProps<T>` is not: it is an ordinary mapped type whose keys the source asserts, and the
erasure is what makes the emitted module uncompilable instead of merely unsupported.

## Affected modules

`packages/types/src/Tween.ts` (the declaration) and its uses in `packages/tween/src/internal.ts`,
`tween.ts`, `tweenProgress.ts` and `updateTweens.ts`.

## Carrying workaround

[`tween-record-membership`](../../source-patches/tween-record-membership.patch) views the carrier as
`Record<string, number>` at both uses — the membership test and the key list — which keeps the same
keys and values and is ordinary target-neutral TypeScript. It corrects the uses, not the type, so any
new use of `NumericProps` will meet the same wall.

A second, independent defect governs how that view then lowers: an assertion becomes an unchecked
`static_cast`. See [a type assertion must not lower to an unchecked
`static_cast`](assertion-lowered-to-unchecked-static-cast.md); representing this type without fixing
that lowering still leaves the module uncompilable.
