# A double assertion up a declared `extends` chain is refused

`ParticleEmitter2D extends Node2D` (and `ParticleEmitter3D extends Node3D`) in `@flighthq/types`. The upstream
update functions still write `getNodeWorldMatrix(emitter as unknown as Node2D)` and
`getNodeWorldMatrix4(emitter as unknown as Node3D)`. The double assertion only restates a widening that the
declared heritage already allows. The emitter refuses the whole function:

```
cpp emission failed for @flighthq/particleemitter/packages/particleemitter/src/updateParticleEmitter2D.ts: an
assertion through an erased type hides the represented source ParticleEmitter2D before naming structural row
Node2D. ParticleEmitter2D declares data, but that member uses optionality or value representations that are not
representation-equivalent ...
cpp emission failed for @flighthq/particleemitter/packages/particleemitter/src/updateParticleEmitter3D.ts: the
asserted row from flight::Ref<flight::types::ParticleEmitter3D> to flight::Ref<flight::types::Node3D> reads data,
whose optionality or value representation is not representation-equivalent to the source type's cells ...
```

Measured in the emitted `@flighthq/particleemitter` ledger at flight `a627849` and flight-compiler `2b687ea`,
with this repository's compiler patches applied (`generated/refusals.json`). The reduction needs the node
package's `Transform2DNode` and `Transform3DNode` and cannot be reproduced in a single module.

## Expected behavior

`x as unknown as Base` where `x`'s declared type already extends `Base` is the identity widening. It should
lower exactly as `x` passed where `Base` is expected. In particular, the subtype's narrower `data` member must not
be compared cell by cell against the base's.

## Affected modules

`@flighthq/particleemitter`: `updateParticleEmitter2D.ts` and `updateParticleEmitter3D.ts`, which are refused
whole. Through them, `prewarmParticleEmitter2D/3D` and `stepParticleEmitter2D` are refused too.

## Carrying workaround

The `particleemitter-node-matrix-direct-emitter` source patch passes `emitter` directly. The assertion is erased
by TypeScript, and the argument is the same object with the same declared heritage.

## The direct call then loses the generic argument (measured)

With the assertion removed, `getNodeWorldMatrix(emitter)` emits, but as
`get_node_world_matrix(flight::structural_ref_cast<flight::types::Transform2DNode<Traits>>(...))`. The callee's
`Traits` was never inferred from `ParticleEmitter2D`'s heritage (`Node2D = Node<Node2DTraits> & Node2DTraits`),
so the header fails with `use of undeclared identifier 'Traits'`. The 3D calls fail the same way. TypeScript
infers `Node2DTraits` and `Node3DTraits` there, and the emitter should substitute them.

The carried patch therefore writes the inferred argument explicitly:
`getNodeWorldMatrix<Node2DTraits>(emitter)`, and `getNodeWorldMatrix4<Node3DTraits>` /
`getNodeLocalMatrix4<Node3DTraits>`. An explicit type argument equal to the inferred one is erased and selects the
same call.
