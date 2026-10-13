# A merged structural row can be read but not constructed

When a source parameter is an INTERSECTION of a capability and an inline object, the emitter spells it as a merged
row. `@flighthq/app`'s window functions are full of this shape:

```cpp
inline void center_window(
    flight::StructuralRef<flight::RowReadonly<flight::RowMerge<
        flight::RowOf<flight::Ref<flight::types::HostWindowGeometryCapability>>,
        flight::RowRequired<flight::RowOf<flight::Ref<center_90209bd6a085864a>>>>>> host_window_geometry,
    flight::Ref<flight::types::AppWindow> win);
```

**Reading** such a row works. `flight::detail::schema_member_t` finds the members of BOTH halves on the merged
schema -- measured directly: `schema_member_t<RowKey<"getBounds">, Merged>` and `schema_member_t<RowKey<"center">,
Merged>` both resolve to their cell types, and the unmerged halves resolve the same members. The schema half of the
feature is sound.

**Constructing** one does not, by either route a caller has:

```cpp
// (1) allocate as the merged schema, then write the members it declares
auto construction = flight::entity::allocate_entity<flight::StructuralRef<Merged>>();
flight::row_set<flight::RowKey<"getBounds">>(construction, host->get_bounds);
// error: static assertion failed: structural row schema does not contain the requested property
```

Every write fails that way, for members of the capability half and the companion half alike, whether the companion's
required member is passed as the optional the type declares or as its unwrapped callable, and whether or not the
`RowRequired` wrapper is present. So the write site resolves the merged schema to `void` while the read-side lookup
above resolves the same key on the same schema -- the two disagree.

```cpp
// (2) project an existing capability row into the merged schema
auto as_row = flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<HostWindowGeometryCapability>>>(host);
auto merged = flight::structural_ref_cast<flight::StructuralRef<flight::RowReadonly<Merged>>>(as_row);
center_window(merged, win);
// compiles; throws std::bad_cast at run time
```

The cast is admitted by the type system and then refuses at run time, which is the projection declining to hand over
a member the source row's own schema does not name.

The emitter's own two merged-row idioms do not give a caller a third route. `node.hpp` emits the allocate-as-merged
form (`allocate_entity<StructuralRef<RowMerge<...>>>()` then writes), which needs route (1). `command.hpp` emits the
cast-a-wider-row form (`structural_ref_cast` from a `RowOf<NodeAny>`), which needs ONE existing row that already
carries both halves -- `NodeAny` genuinely contains all of that merge's members, whereas here the merged member
`center` lives on a companion type that nothing wider combines with the geometry row.

## Minimal TypeScript

```ts
interface Capability {
  center?(): void;
  getBounds(): Bounds;
}
export function center(host: Capability & { center: () => void }): void {
  host.center();
}
```

## Expected

Either route should work, and either would be sufficient:

- `row_set` reaches a member the read-side `schema_member_t` already resolves for the merged schema, so a row
  allocated as that schema can be filled; or
- `structural_ref_cast` projects a row whose subject carries the members into the merged schema without throwing,
  which is what the emitter already asks of it in `command.hpp`.

Until one of them does, a package whose generated API takes a merged row is reachable only from code the emitter
itself writes.

## Affected modules

`@flighthq/app` `appWindow.ts`: `centerWindow`, `setWindowOpacity`, `setWindowResizable`, `setWindowIcon`,
`setWindowMenuBarVisible`, `setWindowSkipTaskbar`, `flashWindowFrame`, and the `attachWindowResize`,
`attachWindowDropFile`, `attachWindowFullscreen`, `attachWindowOrientation`, `attachWindowRenderContext` and
`attachWindowRenderState` families. The shape is not app-specific: it appears wherever a source parameter intersects
a capability with an inline object.

## Carrying workaround

None. This repository carries no patch for it, does not broaden `structuralMemberTable` to reach the members, and
does not invent a wider row type. `tests/generated_app_window_test.cpp` measures the boundary at the point of use and
covers the single-capability functions around it; the merged-row families are left uncovered rather than covered
through a row the runtime cannot build.
