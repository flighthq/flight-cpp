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

## G2 scene-document value validation

At Flight `a627849` / compiler `806d257`, `packages/scene-document/src/sceneDocumentLayoutBindings.ts`
`isFlightDocumentValue(value: unknown, ancestors: Set<object>)` has the minimal shape above. After
`typeof value !== 'object'` returns false, it calls `ancestors.has(value)`, `ancestors.add(value)` and
`ancestors.delete(value)` for cycle detection. The emitted header passes the `flight::Any` unchanged:
`scene_document/scene_document_layout_bindings.hpp:138` fails with `cannot convert 'flight::Any' to 'const
flight::ErasedRef&'`, and the same happens at `:141` (GCC 15.2, one worker: 1 attempted, 0 pass, 1 fail).

The runtime half already exists. `flight::Any::erased_ref()` returns the held object as an `ErasedRef` with
the same identity and no copy. The emitter must apply it on the narrowed path, and must decide the array
alternative this request describes, because the function recurses into arrays as well.

The next error is a separate one: line 142 lowers `Array.isArray(value) ? value.every(...)` to a dynamic
`named_properties(value).get("every")(lambda)` call, an erased array read
([erased-array-element-reading](erased-array-element-reading.md)). Both must be resolved. Nothing is
carried: no value-keyed set, no fresh wrapper identity, and no narrowed document domain.
