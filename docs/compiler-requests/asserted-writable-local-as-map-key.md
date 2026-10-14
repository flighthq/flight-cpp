# A readonly value asserted writable and bound to a local cannot be a WeakMap key

`const mutableThing = thing as Thing` takes a `Readonly<Thing>` parameter and asserts it writable. The emitter
binds the local as a writable structural view, `StructuralRef<RowWritable<RowOf<Ref<Thing>>>>`. That is right for
the member write that follows, but the same local is then passed to `WeakMap<Thing, …>.delete`, whose key is the
nominal `flight::Ref<Thing>`, and no conversion exists. The same assertion written inline at the call,
`owners.delete(thing as Thing)`, lowers to the nominal owner and compiles.

Measured at flight-compiler `2b687ea` with this repository's 44 compiler patches applied. Both modules emit. The
reduction's header fails at the `delete` call; the inline variant has no error there. The other diagnostics in
both are row reads and writes that need the SDK's generated structural member table, which a single-directory
reduction does not have.

## Minimal TypeScript (measured)

```ts
export interface Thing { index: number; }
const owners = new WeakMap<Thing, number>();
export function adopt(thing: Thing): void { owners.set(thing, 1); }
export function release(thing: Readonly<Thing>): boolean {
  const mutableThing = thing as Thing;
  mutableThing.index = -1;
  return owners.delete(mutableThing);   // owner_key.hpp:27: no viable conversion from
                                       // StructuralRef<RowWritable<RowOf<Ref<Thing>>>> to Ref<Thing>
}
```

Inline variant (measured: no error at the call): `return owners.delete(thing as Thing);`.

## Expected behavior

A writable view whose row is exactly one nominal owner's should convert to that owner wherever the owner is
expected: a map or set key, an argument, a comparison. It is the same object, so identity-keyed collections must
see the same key.

## Affected modules

`@flighthq/physics2d` `world.ts` `removePhysics2DBody` (line 469): `physics2DBodyOwners.delete(mutableBody)`. At
a627849 it is the first error of 11 of physics2d's 26 headers, all through `world.hpp`.

## Carrying workaround

[`physics2d-remove-body-owner-key`](../../source-patches/manifest.json) deletes with `body as RigidBody2D`,
which is the module's own form at its collider removal (`physics2DColliderOwners.delete(collider as
Physics2DCollider)`).
