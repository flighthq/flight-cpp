# A resolved task carries the literal it was given, not the domain the function declared

An `async` function whose declared value domain is wider than the value it resolves emits a task carrying the
narrower domain, and the target refuses the conversion the `return` then needs.

Measured at flight-compiler `2b687ea`, flight `a627849`, in `@flighthq/loader`'s `resourceLoader.ts:79` --
surfaced once `catch-await-hoisting` stopped refusing the module, because until then the whole module was a
placeholder and this declaration was never emitted.

## Minimal TypeScript

```ts
function noop(_signal: AbortSignal): Promise<unknown> {
  return Promise.resolve(undefined);
}
```

## Observed

`generated/include/flight/loader/resource_loader.hpp:102`:

```cpp
inline flight::Task<flight::Any> noop_load(flight::AbortSignal signal) {
  return flight::resolve_task(flight::undefined);
}
```

```
error: no viable conversion from returned value of type 'Task<flight::Undefined>' to function return type
       'Task<flight::Any>'
```

`Promise.resolve(undefined)` is a `Promise<undefined>`, which the source assigns to a `Promise<unknown>`
return by covariance; the emitted `Task` carriers have no such conversion, and the literal's own domain is
what the resolver was given rather than the domain the declaration states.

## Expected behavior

The resolved value should carry the declared domain -- the `Any` an `unknown`-typed async function settles
with -- so `Task<flight::Any>` is what the resolver produces, and the settlement, its order, and the
subsequent rejection shape are unchanged. Whether the emitter wraps the settled value at the `return` or gives
the task carriers a faithful covariant conversion is the emitter's decision; inventing a conversion at the
call site is not available, and neither is narrowing the declared domain.

## Affected

`@flighthq/loader` `resourceLoader.ts` (`_noopLoad`, and the pending-entry pool that stores it as
`wrappedLoad`). With the module emitted, its header no longer compiles -- and it *did* compile as a
placeholder, so the package's compile gate regresses until this is fixed.

## Carrying workaround

None. The declaration refuses nothing, so there is nothing to defer; the header simply does not compile.
