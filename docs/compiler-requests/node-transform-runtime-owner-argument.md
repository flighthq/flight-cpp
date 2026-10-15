# A structural runtime owner parameter must accept the node rows that carry the runtime

`getTransform2DRuntime` in `@flighthq/node` takes a structural `Transform2DRuntimeOwner<Traits>` -- an interface
whose only member is the computed `[EntityRuntimeKey]` cell -- and every node that has a transform runtime
satisfies it. The emitter lowers that parameter to a nominal `flight::Ref<Transform2DRuntimeOwner<Traits>>`,
while its callers pass the `Transform2DNode<Traits>` row they were given. No conversion exists between the two,
so `ensureNodeWorldMatrix`, `getNodeWorldMatrix` and the other callers in `nodeTransform2d.ts` fail on every
instantiation. The module is emitted, and passes the uninstantiated compile gate.

Observed in the committed emission at flight-compiler `2b687ea`, flight `a627849`, instantiated with
`Traits = flight::Ref<flight::types::Node2DTraits>` from a native display object (clang 22):

```ts
interface Transform2DRuntimeOwner<Traits extends object> {
  [EntityRuntimeKey]: (NodeRuntime<Traits> & HasTransform2DRuntime) | undefined;
}
function getTransform2DRuntime<Traits extends object>(target: Transform2DRuntimeOwner<Traits>): NodeRuntime<Traits> & HasTransform2DRuntime {
  return target[EntityRuntimeKey]!;
}
export function ensureNodeWorldMatrix<Traits extends object>(target: Transform2DNode<Traits>): void {
  const runtime = getTransform2DRuntime(target);
  // ...
}
```

```text
generated/include/flight/node/node_transform2d.hpp:230:18: error: no matching function for call to 'get_transform2_druntime'
  note: candidate function template not viable: no known conversion from
        'flight::types::Transform2DNode<std::shared_ptr<flight::types::Node2DTraits>>' to
        'flight::Ref<Transform2DRuntimeOwner<...>>'
generated/include/flight/node/node_transform2d.hpp:248:3: error: no matching function for call to 'ensure_node_world_matrix'
```

The same call appears five times in the module (`node_transform2d.hpp:166, 175, 230, 237, 249`).

A reduction with a locally declared `unique symbol` key is refused instead ("computed-symbol members on generic
interface RuntimeOwner require a closed native object type"), so this path is specific to the entity runtime
key, which the compiler binds as a computed cell.

## Required C++ shape

The parameter as the structural view it is -- a row over the owner's computed cell -- so any row that carries the
cell converts, or the call site reading the cell from its own argument:

```cpp
auto runtime = flight::row_get(target, flight::types::entity_runtime_key).value();
```

## Affected Flight modules

`@flighthq/node` `nodeTransform2d.ts`: every world-matrix query, and through them `@flighthq/velocity`'s
`contributeTransformVelocity`, which cannot be exercised natively until these instantiate.

## Carrying workaround

None. `@flighthq/node` is not part of the velocity unit that found it.

## Further site: particleemitter update (measured)

`@flighthq/particleemitter`'s `updateParticleEmitter2D` and `updateParticleEmitter3D`, once their own refusals are patched
(`particleemitter-node-matrix-direct-emitter`), instantiate `getNodeWorldMatrix<Node2DTraits>`,
`invalidateNodeLocalBounds<Node2DTraits>`, `getNodeWorldMatrix4<Node3DTraits>` and `getNodeLocalMatrix4<Node3DTraits>`.
They fail here, in `node_transform2d.hpp:249` and `node_transform3d.hpp:113/122/199/218`. Through them the step and
prewarm modules fail too.

The particleemitter row regenerated at a627849 (scoped, measured on `367f203d`) still fails here in 2D. In 3D it
now fails one step earlier, at its own call site (`update_particle_emitter3_d.hpp:112`). The emitter casts the emitter to a
readonly merged row whose runtime-key member is a particleemitter-local concrete struct, where
`Transform3DNode<Traits>` names the generic `flight::types::entity_runtime_key_d09eb903fba65d57<Traits>`. That boundary
is [a generic structural parameter given a narrower row](generic-structural-parameter-given-a-narrower-row.md); this
request applies again only once it is past.

## Why there is no small source repair (measured 2026-10-13)

The helper that fails, `getTransform2DRuntime(target: Transform2DRuntimeOwner<Traits>)`, comes from this repository's
own `node-exact-runtime-helper-boundaries` patch. `Transform2DRuntimeOwner` is a nominal struct that no object
ever is, so `structural_ref_cast` can never produce one from a node view. That differs from
`getMutableNodeRuntime(source: Node<Traits>)`, whose callers recover the real `Node<Traits>` owner.

Measured candidate: retype the helper on the real owner, `getTransform2DRuntime(target: Node<Traits>)`, returning
`target[EntityRuntimeKey]! as NodeRuntime<Traits> & HasTransform2DRuntime` (and the same in 3D).
- A scratch regeneration of the `@flighthq/node` closure refuses both modules: "a reference assertion from
  flight::Ref<flight::types::NodeRuntime<Traits>> to the intersection NodeRuntime<Traits> & HasTransform2DRuntime
  has no heritage to cast along".
- That cascades to every node module, so `pkg:generate` rightly refused to move node.
- The patch was reverted.

The defect is the runtime representation, not the argument spelling:
- The node's slot is typed `std::shared_ptr<NodeRuntime<Traits>>`, while the transform cells (`local_matrix`,
  `world_matrix`, rotation) live only on the emitted intersection struct, which derives from `NodeRuntime<Traits>`.
- Reading them needs a checked downcast of the stored runtime.
- `flight::ReferenceEnabled` is an empty, non-polymorphic base, and emitted runtimes are aggregates, so a native
  checked downcast (`dynamic_pointer_cast`) is not available.
- The only checked recoveries that remain go through an erased typed owner.
- The scene2d override's `create_node2_d_runtime()` also allocates the base `NodeRuntime`, so a native Node2D has
  no transform-cell storage at all.

Unblocking needs a runtime decision:
- (a) a polymorphic or tagged runtime base, so the compiler can emit a checked downcast along the emitted heritage; or
- (b) a node runtime slot typed with its capability intersections, allocated as the intersection struct by every
  node constructor.

Either is a runtime-ABI and construction change across node and scene2d/scene3d, not a node-local repair.

## Further site: physics2d nodeSync (measured 2026-10-14)

`@flighthq/physics2d` `nodeSync.ts` `syncPhysics2DBodyToNode2D` writes `x`, `y` and `rotation`, then calls
`invalidateNodeLocalTransform(node)`. Node's helper cannot recover a native owner from the merged `Node2D` row
(a static assertion), so the physics2d-node-transform-invalidation override projects to the `Node<Node2DTraits>`
base row first. That projection compiles, but recovering the owner throws `std::bad_cast` at run time on the
only `Node2D` the SDK constructs, scene2d's `createDisplayObject`, which has no native `Node` owner. The override
is therefore recorded as `partial`, and its four upstream cases are written but not run.
