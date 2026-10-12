# A local holding a bound host call must be declared as the binding's result carrier

A local with no annotation takes its type from the call. For a host function bound with a
`callResultType`, that type is the library's, not the target's: `const id = setInterval(tick, ms)`
emits `const double id = host::set_interval(tick, ms)`, placing the handle in a double.

## Minimal TypeScript

```ts
export function every(tick: () => void, ms: number): void {
  const id = setInterval(tick, ms);
  clearInterval(id);
}
```

with the DOM declarations (`setInterval(handler, timeout?): number`, `clearInterval(id)`), and the
target binding `setInterval` → `host::set_interval` with `callResultType: host::TimerHandle`.

## Expected C++

```cpp
const host::TimerHandle id = host::set_interval(tick, ms);
host::clear_interval(id);
```

## Observed at `2b687ea`, flight `a627849`

```cpp
const double id = host::set_interval(tick, ms);
host::clear_interval(id);
```

`@flighthq/power` fails all three of its headers on the real shape
(`generated/include/flight/power/power.hpp:218`):

```
no viable conversion from 'TimerHandle' to 'const double'
```

## Affected modules

`@flighthq/power` today, at its `_idlePollingIntervalMs` poll loop
(`const idleIntervalId = setInterval(() => { … }, _idlePollingIntervalMs)`). Any package storing a
bound host call's result in a local has the same shape; the sibling
[`return-type-of-bound-external-in-union`](return-type-of-bound-external-in-union.md) covers the
*declared* positions (`ReturnType<typeof setInterval>` in a member or return) and bound-call
argument padding, which is why those do not appear here.

## Root cause

The two are separate: `ReturnType<typeof setInterval>` is an authored type reference the semantic
layer can keep, while `const id = setInterval(…)` has no type reference at all — the checker infers
`number` from the library declaration and that is the only evidence the backend receives. The
storage-target collector admits a binding only when the declared type is unresolved (`unknown`) or
carries a recorded absence, so a resolved `number` never reaches the binding table, and the storage
type emitter refuses any resolved non-union declared type rather than accept one.

## Carrying workaround

[`compiler-host-timer-local-storage-carrier.patch`](../../source-patches/compiler-host-timer-local-storage-carrier.patch)
admits a bare primitive declared type (`number`, `string`, `boolean`) for a call the emitter has
already resolved to a bound external call, and returns the binding's `callResultType` as the storage
type. A declared union keeps the path it had, and any other declared type still refuses as
`cpp-external-binding-missing-source-type-evidence`, so the patch does not paper over a real
disagreement; a binding that names no `callResultType` is untouched and keeps its declared domain.
It expires when upstream declares such a local as the binding's result carrier.
