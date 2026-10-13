# An imported generic mapped alias must retain its selected owner

An imported `Snapshot<T>` alias is refused even after `T` is fixed by a function parameter. The
defining module emits `Snapshot<T>` as the identity alias `DeepReadonly<T> = T`, but a consumer is
classified as having an indeterminate owner before that selected `T` reaches the target ABI.

## Minimal TypeScript

```ts
// types.ts
export type DeepReadonly<T> = T extends object
  ? { readonly [K in keyof T]: DeepReadonly<T[K]> }
  : T;
export type Snapshot<T> = DeepReadonly<T>;

// consumer.ts
import type { Snapshot } from './types.ts';
export function restore<T>(snapshot: Snapshot<T>, target: T): void {}
```

At flight-compiler `2b687ea` and Flight `a627849`, the types module emits:

```cpp
template <typename T> using DeepReadonly = T;
template <typename T> using Snapshot = DeepReadonly<T>;
```

but each consumer refuses under
`cpp-imported-type-unsupported:indeterminateIdentity` with:

```text
imported type Snapshot cannot be traced to one exact, terminating declaration application
(unconstrained-type-parameter), so the target cannot name the type it holds
```

## Required behavior

Resolve the imported alias application after substituting the consumer's `T`. On this C++ profile,
readonly is a view restriction rather than a second allocation, so `Snapshot<T>` must select the
same owner and storage identity as `T`; it must not add another `Ref`, materialize a copy, or erase
the owner. An actually unresolved or recursive alias must remain refused.

## Affected Flight modules

`@flighthq/snapshot` at the same pin refuses the public functions in `captureSnapshot.ts`,
`equalsSnapshot.ts`, `interpolateSnapshots.ts`, and `restoreSnapshot.ts` at their imported
`Snapshot<T>` parameter or result. The defining `flight/types/snapshot.hpp` already emits the exact
identity aliases, so this is distinct from an ambient type that lacks a native representation.

The carrying native overrides use one explicit plain-data graph carrier and never claim that an
arbitrary unrepresented C++ aggregate is snapshot-capable. They expire when the compiler preserves
the selected owner through the imported mapped alias.
