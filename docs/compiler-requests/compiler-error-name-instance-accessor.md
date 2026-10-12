# `Error.name` must be the runtime's instance accessor

The emitter refuses a `name` read on an Error-typed value, and it is right to: the runtime exposed only
one *static* `name()` per class, so reading `name` through an `Error` binding would answer the name of
the declared class. A `RangeError` held as an `Error` would say `"Error"`, which is the one answer the
source never asked for.

## Minimal TypeScript

```ts
export function name(error: Error): string { return error.name; }
```

## Expected behavior

The answer comes from the object. `catch` hands back the value that was thrown and
`e instanceof Error` is true for a `RangeError`, so `name` is a dynamic property and the runtime has to
expose it as an instance accessor the subclasses override. The runtime now does: `flight::Error::name()`
is `virtual`, `RangeError` and `TypeError` override it, and `Error` is reference-backed so a projection
hands back the object rather than a copy that would slice the subclass.

Observed at `2b687ea` / `a627849`, before this work:

```
cpp emission failed for @flighthq/log/packages/log/src/log.ts:
reading Error.name from an erased value requires both an instance flight::Error::name() const accessor …
rule: cpp-error-name-runtime-required
```

## Affected modules

`packages/log/src/log.ts` (`serializeLogError`), which reads `name`, `message`, `stack` and `cause` off
the caught value.

## Carrying workaround

[`compiler-error-name-and-erased-error-reads.patch`](../../source-patches/compiler-error-name-and-erased-error-reads.patch)
lowers a typed `Error.name` to `name()`. The runtime half landed in flight-cpp `48e4424`. It expires when
the emitter executes the read itself.
