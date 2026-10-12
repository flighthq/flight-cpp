# A callee's type parameter must not survive into an emitted structural row

Passing a `Signal<T>` through a generic helper leaves the helper's own type-parameter name in the
emitted C++, where it names nothing.

## Minimal TypeScript

The helper (`@flighthq/signals`, `packages/signals/src/slot.ts:64`):

```ts
export function hasSignalSlots<T extends (...args: any[]) => void>(signal: Readonly<Signal<T>>): boolean;
```

and a caller (`@flighthq/power`, `packages/power/src/power.ts`):

```ts
if (power.onIdleStateChange !== null && hasSignalSlots(power.onIdleStateChange)) { … }
```

## Expected behavior

The projection the emitter builds for the argument must name the type argument it was called with —
`std::function<void()>` here — because that is the type the value has. The callee's type-parameter
*identifier* is a source-level name and has no meaning at the call site.

## Observed at `2b687ea` / `a627849`

`generated/include/flight/power/power.hpp:220`, emitted in full:

```cpp
flight::structural_ref_cast<flight::StructuralRef<flight::RowReadonly<flight::RowOf<std::shared_ptr<flight::types::Signal<T>>>>>>(
    flight::StructuralRef<flight::RowWritable<flight::RowOf<std::shared_ptr<flight::types::Signal<std::function<void()>>>>>>(idle_signal.value()))
```

The **source** side of the cast spells the concrete `Signal<std::function<void()>>` correctly; the
**target** side spells `Signal<T>`, with `T` undeclared, so the whole translation unit fails:

```
flight/power/power.hpp:220: error: use of undeclared identifier 'T'
```

All three of `@flighthq/power`'s headers fail on it (`power.hpp`, `contract.hpp`, `_internal_index.hpp`),
and the package emits cleanly otherwise: **3/3 modules emitted, 0 refused, 0/3 headers compiling**.

## Affected modules

`packages/power/src/power.ts` today. `hasSignalSlots` has one other SDK caller —
`packages/preferences/src/storage.ts` — so `@flighthq/preferences` is expected to meet the same defect
and was not measured separately here.

## Status

**No patch is carried.** Filing only: the emission is observed directly at the pin, but a reduced
reproduction inside the compiler's own harness was not confirmed in the session that found it, and a
patch written against an unreduced report is a patch that fixes the spelling rather than the
substitution. The source call above is the whole reduction — one `hasSignalSlots` call on an optional
`Signal<() => void>` — so the owner can land a fixture in one step and then lower the target side from
the argument's type evidence.
