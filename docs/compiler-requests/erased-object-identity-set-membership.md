# `Set<object>` membership must retain an erased object's identity

An `unknown` value narrowed by `typeof value === 'object'` is represented as `flight::Any`, while a
`Set<object>` is represented as `flight::Set<flight::ErasedRef>`. The compiler emits membership and
insertion with the `Any` argument unchanged, so the header cannot compile even though the narrowing
proved the value has object identity.

## Minimal TypeScript

```ts
export function repeated(value: unknown, seen: Set<object>): boolean {
  if (value === null || typeof value !== 'object') return false;
  if (seen.has(value)) return true;
  seen.add(value);
  return false;
}
```

At flight-compiler `2b687ea` and Flight `a627849`, `@flighthq/snapshot`
`enableSnapshotGuards.ts` emits `Set<ErasedRef>::has(Any)` and fails with:

```text
no viable conversion from 'flight::Any' to 'const flight::ErasedRef'
```

## Required behavior

The successful null/`typeof` guard must project the held object reference into an identity-only key
accepted by `Set<object>`. The projection must keep the original reference alive and compare its
existing identity; it must not allocate a wrapper object with a new identity. A value whose `Any`
alternative is primitive, external, or an array without a represented object projection must fail
the narrowing rather than enter the set.

This is observable in cycle detection: re-identifying the same node on each visit misses the cycle,
while comparing values instead of identities conflates distinct equal objects. The native snapshot
carrier stores graph nodes directly, so its guard can keep a set of their actual node addresses.
