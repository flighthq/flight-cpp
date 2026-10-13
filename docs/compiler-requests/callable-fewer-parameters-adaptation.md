# A function with fewer parameters is assigned where a wider callable is expected

TypeScript lets a function take FEWER parameters than the callable type it is assigned to: the extra
arguments are simply not passed. The emitted C++ assigns the function itself, and `std::function` refuses a
callable whose signature has fewer parameters than the one it holds.

Measured at flight-compiler `2b687ea`, flight `a627849`, in `@flighthq/loader`'s `resourceLoader.ts` --
surfaced once the module stopped being a placeholder and started being emitted. It is the first compile error
in that module after the promise-domain fix, and it is not the module's remaining refusal
(`flattened-interface-refinement-identity`, which refuses different declarations).

## Minimal TypeScript

```ts
type Wrapped = (signal: AbortSignal, reportBytes: (loaded: number, total?: number) => void) => Promise<unknown>;
function noopLoad(_signal: AbortSignal): Promise<unknown> {
  return Promise.resolve(undefined);
}
export function use(): Wrapped {
  return noopLoad;
}
```

## Observed

`generated/include/flight/loader/resource_loader.hpp`:

```cpp
struct PendingEntry : public flight::ReferenceEnabled {
  std::function<flight::Task<flight::Any>(flight::AbortSignal, flight::types::ResourceLoadBytesReporter)> wrapped_load;
  …
};
…
  return flight::make_ref<PendingEntry>(PendingEntry{… .wrapped_load = noop_load});
```

```
error: no viable conversion from 'flight::Task<flight::Any> (flight::AbortSignal)' to
       'std::function<flight::Task<flight::Any> (flight::AbortSignal, flight::types::ResourceLoadBytesReporter)>'
```

## Expected behavior

The narrower function is adapted at the point where the wider callable is stored: a lambda that takes the
full parameter list and forwards only what the source passes, so the callable it becomes is the one the
declaration states and every argument it ignores is ignored rather than invented. The adaptation must not
change the call's settlement, its ordering, its rejection, or the value it resolves -- a callable that
resolves is still the same callable, just written with the arity the target's storage needs.

## Affected

`@flighthq/loader` `resourceLoader.ts`: `_noopLoad` (one parameter) stored into `PendingEntry.wrappedLoad` (two
parameters), in the pool's initialization and in `releasePendingEntry`. With the module emitted, its header
does not compile -- and it *did* compile as a placeholder, so the package's compile gate is held down by this
until it is fixed.

## Carrying workaround

None. The declaration refuses nothing, so there is nothing to defer; the header simply does not compile. A
loader-local override would be a copy of the module rather than a lowering of it.
