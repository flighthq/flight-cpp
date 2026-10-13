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
