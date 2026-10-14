# Shadow traversal must retain checked mesh capabilities through branch flow

`@flighthq/scene3d-gl` walks a heterogeneous `Node3D` tree and then selects rigid, skinned or
instanced shadow drawing. At Flight `a6278492` and flight-compiler `2b687eaf`,
`packages/scene3d-gl/src/glShadowMap.ts` crosses both capability boundaries with assertions:

```ts
const mesh = node as unknown as Mesh;
if (mesh.geometry == null) return;

const instanced = isShadowInstancedMesh(mesh);
if (instanced && (mesh as unknown as InstancedMesh).instanceCount === 0) return;
// ...
if (instanced) {
  const instancedMesh = mesh as unknown as InstancedMesh;
  const count = instancedMesh.instanceCount;
  bindGlInstancePalette(state, program, flattenShadowInstanceMatrices(instancedMesh), count);
}
```

The generated inventory refuses the module as `cpp-structural-assertion-owner-unproven`
(`source-portability`). Its exact current-pin diagnostic says the retained `Mesh` owner does not declare
`instanceColors`, `instanceCount`, `instanceMatrices` or `version`, and carries an incompatible
`EntityRuntimeKey`; the emitter will not invent a cast, copy, materialized row or side storage to recover
`InstancedMesh`.

The two old workarounds, `scene3d-gl-shadow-node-capabilities` and
`scene3d-gl-shadow-instanced-flow`, are not independent fixes. The second is the branch-flow form required
after the first introduces a typed instance predicate. They must be re-derived as one current-pin source
change so that no intermediate nullable `Mesh | InstancedMesh` carrier is introduced.

## Current-pin source shape

Keep both checks typed before erasure and let the predicate evidence narrow the original owner:

```ts
function isShadowMesh(node: NodeOf<Node3DTraits>): node is Mesh {
  const candidate: { readonly geometry?: MeshGeometry | null } = node;
  return candidate.geometry != null;
}

function isShadowInstancedMesh(mesh: Readonly<Mesh>): mesh is Readonly<InstancedMesh> {
  return 'instanceMatrices' in mesh;
}

forEachNodeDescendant<Node3DTraits>(scene, (node) => {
  if (!isShadowMesh(node)) return;
  const mesh = node;

  const instanced = isShadowInstancedMesh(mesh);
  if (instanced && mesh.instanceCount === 0) return;
  // ...
  if (instanced) {
    const count = mesh.instanceCount;
    bindGlInstancePalette(state, program, flattenShadowInstanceMatrices(mesh), count);
    // instanced draw with count
  }
});
```

The boolean `instanced` must remain the flow evidence. The older intermediate spelling
`const instancedMesh = isShadowInstancedMesh(mesh) ? mesh : null` asks C++ to represent a redundant
nullable union between flattened interface owners and previously failed as
`cpp-contextual-union-value-type-unrepresented`. It adds no runtime check beyond the predicate and is not
part of the replacement.

Three exact-carrier spellings are also required by this module at the current pin:

- type the shadow matrix coalescing destination as `Matrix4`;
- replace the nested `??=` program-selection expression with explicit null-checked branches assigning one
  `GlMeshProgram` local; and
- type the `program.locJointTexture ?? null` destination as `WebGLUniformLocation | null` before calling
  `uniform1i`.

The existing `scene3d-gl-uniform-location-locals` patch separately supplies exact locals for the program
factory return objects and was present in the composed measurement. It need not be duplicated by this
replacement.

## Emission evidence

The unmodified current pin records `glShadowMap.ts` as a refused placeholder whose first diagnostic is the
`Mesh` to `InstancedMesh` owner assertion above. A focused generation with the two typed predicates and
boolean flow, but before the explicit program branches, advanced to the distinct
`cpp-optional-member-access-unproven` rule: the nested `??=` expression left `.program` on
absence-carrying storage without a present proof.

With both capability paths and the three exact-carrier spellings applied together over the composed active
Flight patch stack, the focused package graph produced:

```text
module: packages/scene3d-gl/src/glShadowMap.ts
status: emitted
refusals: []
output: flight/scene3d_gl/gl_shadow_map.hpp (complete)
```

This is a source-portability rewrite. It needs no new runtime operation and emits no `Any`, unchecked
structural cast, copied row or replacement owner.

## Equivalence

The rewritten traversal visits the same descendants in the same preorder. The mesh guard performs the
same `geometry != null` read that followed the old `Node3D` assertion, so a node without geometry is still
skipped and a geometry-bearing node follows the same draw path. The instance guard performs the same
`'instanceMatrices' in mesh` test. That one result still controls the zero-instance exit, skin exclusion,
program variant, palette upload and instanced draw.

For every accepted caster the same object supplies its geometry, world matrix, skin, instance count and
instance matrices. Rigid, skinned and instanced programs keep the same keys and are compiled lazily on the
same first use; explicit null checks are the control-flow expansion of the former `??=` assignments. The
same view-projection/model uniforms, joint palette, instance palette, upload, draw API and draw count are
used in the same order. The `Matrix4` and `WebGLUniformLocation` annotations erase in JavaScript. No caster,
matrix, palette or GL owner is cast, copied, reallocated, materialized or moved to side storage.

## Focused tests

The replacement should prove the two capability paths separately and then retain the existing package
behavior:

1. **Node-capability compiler fixture.** Lower a heterogeneous `NodeOf<Node3DTraits>` callback containing a
   plain node and a geometry-bearing mesh. The assertion form must reproduce the owner-unproven refusal;
   the typed `geometry` predicate must emit the guarded mesh reads with no `flight::Any`,
   `structural_ref_cast`, reinterpretation or row materialization. The existing backend coverage for typed
   pre-erasure scene predicates is the passing control; this fixture pins the shadow callback shape.
2. **Instanced-flow compiler fixture.** From a `Mesh`, compare the old `as unknown as InstancedMesh` reads,
   the nullable `instancedMesh` alias and the boolean predicate flow. Assert that the first names the
   `instanceCount`/`instanceMatrices` owner mismatch, the second reproduces the unrepresented contextual
   union, and only the boolean guard emits. Its true branch must read the original narrowed mesh for the
   zero-count check and palette/draw count; its false branch must not expose instance members. Include the
   explicit program branches so the fixture cannot pass while failing on the later optional-member rule.
3. **Package node behavior.** Extend `glShadowMap.test.ts` with a non-mesh descendant before an ordinary
   rigid mesh. Assert the non-mesh node produces no upload or draw and the rigid mesh still produces exactly
   one non-instanced draw. This is the runtime witness for the first guard.
4. **Package instanced behavior.** Retain the existing three-instance case (one `drawElementsInstanced` with
   count `3` and no rigid draw), zero-live-instance case (no draw), and ordinary-rigid control. Also assert
   the live case selects the `shadow:depth:instanced` cache key and uploads the authored instance palette so
   the test covers the guarded data path, not only the final API name.

Compiler fixtures prove the native ownership and control-flow shape. The package Vitest cases prove the
observable traversal and GL calls; neither substitutes for the other.

## Carrying workaround and status

`source-patches/scene3d-gl-shadow-node-capabilities.patch` and
`source-patches/scene3d-gl-shadow-instanced-flow.patch` both target Flight `7e2fc7df` and both carry
`pendingRebase` markers for `a6278492`. The latter was layered on the former, so applying either alone is not
a valid current-pin state. The source-patch lane should supersede them with one combined current-pin patch,
suggested id `scene3d-gl-shadow-checked-capability-flow`, and one manifest equivalence argument matching the
section above.

This request deliberately changes neither old patch nor generated inventory. The replacement expires when
Flight adopts typed shadow mesh/instance predicates plus compiler-representable lazy program flow, or when
flight-compiler can preserve the same checked owners and presence evidence from the original source without
casts or materialization.
