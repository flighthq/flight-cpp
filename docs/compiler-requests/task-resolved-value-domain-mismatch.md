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
## Affected

`@flighthq/loader` `resourceLoader.ts` (`_noopLoad`, and the pending-entry pool that stores it as
`wrappedLoad`). With the module emitted, its header no longer compiled -- and it *did* compile as a
placeholder, so the package's compile gate regressed until this was fixed.

## Carried by a compiler patch

`source-patches/compiler-promise-resolve-domain.patch` gives the settlement the declared domain explicitly,
which is the form the emitter already produces for a source-written `Promise.resolve<T>(x)`:

```cpp
inline flight::Task<flight::Any> noop_load(flight::AbortSignal signal) {
  return flight::resolve_task<flight::Any>(flight::undefined);
}
```

The rule fires only where deduction would answer with the literal's own domain -- a literal, `null`, or the
`undefined` the semantic layer keeps as the ambient name it is. A value already in the declared domain, an
adopted task, a reference the call boundary already erased, a source-written type argument, and a position
that states no promise domain are all left byte-identical, so nothing that already worked moves.

Only the CARRIER changes, never the value: the settlement holds the same `undefined`, and a boxed reference
keeps the same object (recovered by identity in `tests/task_resolve_domain_test.cpp`), with settlement
immediate and once in both forms and an adopted task keeping its value, its rejection, and its exception
rejection. Controls: `cppPromiseResolveDomain.test.ts`, 9 cases, three mutations with teeth (rule disabled 3
fails, the declared-domain guard 3, the source-type-argument guard 1), plus 6 native checks.
