# An entity built through `allocateEntity` is allocated as its construction bag, not as the entity type

`allocateEntity<Type>()` returns `{} as EntityConstruction<Type>`: an empty object that the factory fills
and `finishEntity` asserts to `Type`. JavaScript has ONE object throughout, whose members are whatever the
factory wrote. The C++ emission allocates a different, anonymous construction struct and converts it at
`finishEntity` with `flight::structural_ref_cast<Type>`:

```cpp
// generated/include/flight/entity/entity.hpp
template <typename Type>
inline flight::types::EntityConstruction<Type> allocate_entity() {
  flight::types::EntityConstruction<Type> out = flight::structural_ref_cast<flight::types::EntityConstruction<Type>>(
      flight::StructuralRef<...>(flight::make_ref<anonymous_af57427cc9d99ff0<Type>>(anonymous_af57427cc9d99ff0<Type>{})));
  flight::row_set(out, flight::types::entity_runtime_key, std::nullopt);
  return out;
}
template <typename Type>
inline Type finish_entity(flight::types::EntityConstruction<Type> out) { return flight::structural_ref_cast<Type>(out); }
```

## Why it matters now

A by-name membership query on a finished entity (`'device' in state`, registry guards) must answer from
the owner of the type the object really is. The runtime can establish that only from the ALLOCATION:
`flight::make_ref` records an allocation's exact type, and a query with no record must refuse rather than
guess from the static type. With this emission the allocation `make_ref` saw is
`anonymous_af57427cc9d99ff0<Type>`, not `Type`, so a record (if any) names the construction bag. Whether
the finished `Type` is the same allocation or a new one materialized by `structural_ref_cast` decides which
record the query sees. Neither answer may be substituted by an `Object.assign` copy or a downcast guess.

## Minimal TypeScript

```ts
export interface State extends Entity { device: Device | null }
export function createState(): State {
  const out = allocateEntity<State>();
  out.device = null;
  return finishEntity(out);
}
// later, with a Ref<RenderState>-typed receiver: 'device' in state  -> true
```

## Expected

The finished entity's allocation is recorded as `Type` (or its construction provably IS a `Type`), so a
provenance-checked query answers exactly. Either lower `allocateEntity<Type>()` to an allocation of `Type`
itself with its members in their absent state, or have the materializing conversion `make_ref` the
`Type` and carry the bag's written cells onto it, never a copy that leaves two live objects.

## Open, to be proven before any lowering change

- Whether `structural_ref_cast<Type>` at `finish_entity` returns the bag's allocation or a new `Type`
  allocation (and so whether identity across `finishEntity` holds today).
- Which provenance record each path leaves under the pending exact-allocation decision (Foreman e1de5e33,
  Manager 29a9c11e).

## Affected

Every `create*` factory built on `allocateEntity`/`finishEntity`, notably `createRenderState` and
render-wgpu's `WgpuDeviceState`/`WgpuHostAcquisition`, and through them the registry-guard membership helper.
