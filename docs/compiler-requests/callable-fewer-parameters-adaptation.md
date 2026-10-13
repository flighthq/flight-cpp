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
## Affected

`@flighthq/loader` `resourceLoader.ts`: `_noopLoad` (one parameter) stored into `PendingEntry.wrappedLoad` (two
parameters), in the pool's initialization and in `releasePendingEntry`. With the module emitted, its header
does not compile -- and it *did* compile as a placeholder, so the package's compile gate is held down by this
until it is fixed.

## Carried by a compiler patch

`source-patches/compiler-callable-arity-adaptation.patch`. The adaptation itself already existed in the
backend -- the wrapper it emits for a callable slot inside a union -- and what was missing was any plain
callable position reaching it, plus one shape of its own:

- a position declaring a callable directly (through an alias) now reaches that wrapper. A union or optional
  carrier is deliberately excluded, because that path wraps the adapted callable in the carrier and adapting
  earlier would drop it.
- a source **requiring** more parameters than the target promises is refused
  (`cpp-callable-arity-narrowing-unrepresented`) rather than wrapped: it would be called with fewer arguments
  than it declares, and no adapter can supply them.
- a source whose last parameter is a **rest** now receives every remaining argument in one array, which is
  the array the source language would have built. Before this it received only the first.

The wrapper's properties are the ones the union path already had: a declaration is invoked directly (a
stateless function address), any other callable is evaluated **once** where the callable is created and
retained by value, a parameter the source cannot receive is left out, a source-only optional parameter is
supplied `std::nullopt` so its default applies, and an equal signature is not wrapped at all.

Controls: `cppCallableArityAdaptation.test.ts`, 10 cases, with three mutations that bite (the entry point
disabled 6 fails, the rest collection 1, the required-parameter rule 1) -- and the plain-callable restriction
is guarded by three existing expectations that pin the union path's wrapping.
