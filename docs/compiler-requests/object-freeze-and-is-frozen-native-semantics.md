# `Object.freeze` and `Object.isFrozen` must share represented native state

The C++ backend erases `Object.freeze(value)` to `value` and refuses `Object.isFrozen(value)` as an
unbound ambient member. Together those choices make a source contract based on immutability
impossible to preserve: the write still succeeds, and the later query cannot observe any state.

## Minimal TypeScript

```ts
export function freezeAndRead(value: object): boolean {
  Object.freeze(value);
  return Object.isFrozen(value);
}
```

At flight-compiler `2b687ea`, the freeze call is emitted as its operand, while the query refuses:

```text
ambient value Object member isFrozen has no C++ binding
```

Erasing a freeze is only sound for a value whose target representation is already immutable. It is
not sound for a mutable record or array merely because C++ lacks JavaScript property descriptors.

## Required behavior

For represented plain-data objects and arrays, freezing must mark the same reference identity and
reject later writes through every alias. `Object.isFrozen` must read that mark. Deep freezing remains
the caller's recursive operation; the primitive only freezes one container, exactly as JavaScript
does. Unsupported target carriers must fail explicitly rather than treating every value as frozen.

## Affected Flight module

`@flighthq/snapshot` `captureSnapshot.ts` calls `Object.isFrozen` as the visited mark, then freezes
each cloned object before descending. The module is refused at `Object.isFrozen`; simply electing a
query that always returns true would terminate the walk while leaving the snapshot mutable, and
always returning false would recurse forever on cycles. The native override therefore carries an
explicit freeze bit on its plain-data graph nodes and checks it on every mutator.
