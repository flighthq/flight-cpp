# An erased array needs an identity-preserving element view

`flight::Any` now records an exact Flight array alternative and can recover the identical typed
`Array<T>`, but the compiler still refuses an `unknown[]` assertion with a diagnostic that says no
array alternative exists. Exact recovery alone is not enough for this source shape: `unknown[]`
must read and write arrays whose concrete element type is not statically known while preserving the
same JavaScript array identity.

## Minimal TypeScript

```ts
export function resize(value: unknown, length: number): void {
  if (Array.isArray(value)) (value as unknown[]).length = length;
}
```

At flight-compiler `2b687ea` and Flight `a627849`, `@flighthq/snapshot` `equalsSnapshot.ts` is
refused with:

```text
an erased C++ value has no identity-preserving reading as flight::Array<flight::Any>:
flight::Any has no array alternative or checked exact-array extraction
```

The first clause is stale: `Any` has `detail::AnyArray`, retains the shared-vector identity, and
`external_if<Array<T>>()` performs exact recovery. The missing part is a heterogeneous view; copying
an `Array<number>` into an `Array<Any>` would change identity and make restore writes invisible.

## Required behavior

Represent a checked erased-array view with size, indexed read, indexed write, resize, and identity.
Box reads as `Any`, validate writes against the concrete element carrier, and mutate the original
shared array. `Array.isArray` narrows an erased value onto this view. A failed element write must be
explicit rather than coercing or replacing the array.

## Affected Flight modules

`equalsSnapshot.ts` needs size and indexed reads; `interpolateSnapshots.ts` and
`restoreSnapshot.ts` additionally need resize and writes. Their current retained C++ also attempts
`static_cast<Array<Any>>(Ref<void>)`, so even the modules whose public generic entry point is refused
leave uncompilable helper bodies. The native overrides use one recursive plain-data graph carrier
whose arrays already have erased elements and shared identity; they do not pretend the general
`Array<T>` view exists.
