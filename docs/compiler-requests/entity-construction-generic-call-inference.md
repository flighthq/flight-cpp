# Entity-construction generic calls must retain the finished owner argument

`EntityConstruction<T>` is a source assertion view of the same JavaScript object as `T`. A generic
helper called with that construction view and a `Readonly<T>` source must not specialize its one type
parameter as the construction wrapper alone:

```ts
function copyFields<T extends Material>(out: T, source: Readonly<T>): void {}

export function clone<T extends Material>(source: Readonly<T>): T {
  const out = allocateEntity<T>();
  copyFields(out, source);
  return finishEntity(out);
}
```

At Flight revision `a627849`, the materials exact-owner rewrite exposes this generated call:

```cpp
copy_material_fields<EntityConstruction<T>>(out, source, source_kind);
```

The helper's source parameter consequently expects `RowOf<EntityConstruction<T>>`, while `source` is
the required `RowOf<T>`, and every concrete instantiation fails. TypeScript accepted the call because
the construction carrier is the writable pre-finish view of the same `T`, not a different material
type.

The carried package-local rewrite calls `finishEntity` before the private field copy and stores that
identity result as `T`. Upstream `finishEntity` is exactly `return out as Type`; moving that assertion
before the remaining writes does not execute code, publish the object, invoke a callback, allocate,
copy, or alter field order observable to JavaScript. Native C++ then gives the helper one exact owner
for both arguments.

The request expires when generic call substitution reconciles `EntityConstruction<T>` with the same
finished `T` proven by the other arguments, or when the upstream clone adopts the exact local. Native
materials tests instantiate Standard-PBR clone and copy and assert distinct result identity, scalars,
name, equality, and alias safety.
