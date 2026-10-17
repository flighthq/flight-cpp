# A call to a user-defined type predicate must narrow the guarded value

`isMesh(source): source is Mesh` narrows its argument inside the guarded block. The emitter writes the
predicate body correctly -- it projects the argument to a readonly partial `Mesh` row and reads
`geometry` from that row. The **caller** keeps the argument's declared row and reads the narrowed-only
property from it, so the runtime's `row_get` static assertion fires: a row schema names exactly the
properties of its declared type, and `geometry` belongs to `Mesh`, not to the `Node3D` row.

## Minimal TypeScript

```ts
interface Geometry { version: number }
interface Mesh { geometry: Geometry }
interface Node3D { kind: string }
declare function isMesh(source: unknown): source is Mesh;
declare function consume(geometry: Geometry): void;

export function bounds(node: Readonly<Node3D>): void {
  if (isMesh(node)) consume(node.geometry);
}
```

## Observed C++

`@flighthq/scene3d` `sceneNodeBounds.ts` `_accumulateWorldBounds`:

```cpp
inline void accumulate_world_bounds(flight::Ref<flight::types::AabbLike> out,
    flight::StructuralRef<flight::RowReadonly<flight::RowMerge<
        flight::RowOf<std::shared_ptr<flight::types::Node<flight::Ref<flight::types::Node3DTraits>>>>,
        flight::RowOf<flight::Ref<flight::types::Node3DTraits>>>>> node) {
  if (is_scene_mesh_...(node)) {
    ... flight::mesh::ensure_mesh_geometry_bounds(
            flight::row_get<flight::RowKey<"geometry">>(node)) ...
```

The predicate itself is emitted faithfully:

```cpp
inline bool is_scene_mesh_...(merged-row node) {
  flight::StructuralRef<flight::RowReadonly<flight::RowPartial<flight::RowOf<flight::Ref<flight::types::Mesh>>>>> candidate = node;
  return flight::row_get<flight::RowKey<"geometry">>(candidate).has_value();
}
```

and the guarded read fails at the pin:

```
include/flight/structural_ref.hpp:1421: error: static assertion failed due to requirement
  '!std::is_void_v<void>': structural row schema does not contain the requested property
generated/include/flight/scene3d/scene_node_bounds.hpp:53: note: in instantiation of
  'flight::row_get<flight::RowKey<FixedString<9>{"geometry"}>, RowReadonly<RowMerge<...Node3D...>>>'
```

## Required C++ shape

Apply the predicate's narrowing to the guarded value: the narrowed carrier is the same readonly partial
row the predicate builds, over the **same retained owner** (no copy, no pointer cast, no widened
domain), and the narrowed-only reads go through it:

```cpp
if (is_scene_mesh_...(node)) {
  auto mesh = flight::structural_ref_cast<
      flight::StructuralRef<flight::RowReadonly<flight::RowPartial<flight::RowOf<flight::Ref<flight::types::Mesh>>>>>>(node);
  ... flight::row_get<flight::RowKey<"geometry">>(mesh) ...
```

Alternatively the emitter may refuse with a diagnostic naming the narrowed type it could not build;
what it must not do is read the property from the declared row.

## Affected modules

- `@flighthq/scene3d` `sceneNodeBounds.ts` (`_accumulateWorldBounds`, `scene_node_bounds.hpp:53`) and
  `sceneNodeCulling.ts` (`scene_node_culling.hpp:61`, the same emitted construct). These are cluster
  `static-assertion-00a0e75d`; the GL and WGPU forward-lights headers reach the bounds module through
  their includes and report the same first diagnostic.

Follow-on families measured in the same headers and **not** addressed by this request (each keeps its
header failing after this is fixed):

- `Ref<Aabb>` passed where `Ref<AabbLike>` is declared -- `scene_node_bounds.hpp:57`,
  `prepare_gl_scene_3d_forward_lights.hpp:155` (and the WGPU mirror);
- `row_get<RowKey<"m">>` on the emitted world-matrix row, which does not declare it --
  `prepare_gl_scene_3d_forward_lights.hpp:158`;
- a `Scene3DLights` row projected where the declared `Scene3DLightsLike` carrier is expected --
  `prepare_gl_scene_3d_forward_lights.hpp:191`.
