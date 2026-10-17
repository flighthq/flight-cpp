# A row assertion to a narrower data type must check the member it reads

`(node.data as SwfAuthoredBoundsData).authoredBounds` asserts that a node's data row carries a member the
declared data type does not. The emitter refuses, because the asserted row reads `authoredBounds`, which
the source row type `Node2DData` does not declare, and nothing proves the runtime value has it.

## Minimal TypeScript

```ts
interface NodeData { kind: string }
interface AuthoredBoundsData extends NodeData { authoredBounds: Rectangle }

export function boundsOf(data: NodeData): Rectangle {
  return (data as AuthoredBoundsData).authoredBounds;
}
```

## Required C++ shape

A checked downcast of the retained owner to the asserted nominal type (the value was created as
`AuthoredBoundsData`), failing explicitly when it is not one, rather than a row read of an undeclared member:

```cpp
// illustrative: a checked owner cast the runtime would need to provide
auto owner = checked_owner_cast<AuthoredBoundsData>(data);  // throws TypeError when it is not one
return owner->authored_bounds;
```

Related: [assertion lowered to an unchecked static_cast](assertion-lowered-to-unchecked-static-cast.md).

## Affected modules

`computeSwfLocalBoundsRectangle` in `@flighthq/swf` (`swfNode.ts`): refused, not supplied.

Also: a nominal downcast assertion lowered to a structural view. In `@flighthq/swf`
(`swfFrameAction.ts`), `const clip = target as MovieClip` (a `Node2D` asserted to `MovieClip`) becomes
`structural_ref_cast<StructuralRef<RowWritable<RowOf<Ref<MovieClip>>>>>(target)`. Every movieclip call
then needs the nominal `Ref<MovieClip>`: `swf_frame_action.hpp` fails at 6 sites. The same checked owner
recovery described above is the fix. No workaround is carried.

`@flighthq/skeleton2d` at flight `a627849` refuses three whole modules on the same downcast, each a
solver or accessor dispatched by the value's `kind` before it asserts the concrete row:
- `ikConstraint2D.ts`: `constraint as Readonly<Skeleton2DIkConstraint>` from `Readonly<Skeleton2DConstraint>`
  (reads `bendPositive`, `boneIndices`, `compress`, `stretch`, `targetBoneIndex`);
- `transformConstraint2D.ts`: `constraint as Readonly<Skeleton2DTransformConstraint>` (reads the `mix*` and
  `offset*` members and `boneIndices`);
- `pathConstraint2D.ts`: `attachment.kind === PathAttachment2DKind ? (attachment as Readonly<PathAttachment2D>) : null`
  from `Attachment2D` (reads `commands`, `pointCount`, `skin`, `vertices`, `winding`).

The checked owner recovery above serves all three; the readonly target only removes write capability. No
workaround is carried.

## Same downcast in physics2d (a627849)

`@flighthq/physics2d` stores every joint as `Physics2DJoint` and dispatches by `kind` to a solver that asserts
the concrete joint. The emitter flattens each joint interface into an independent owner, so no assertion can
recover the derived members:
- `joints.ts`: all nine solver objects (`physics2DDistanceJointSolver` through `physics2DGearJointSolver`, first
  reported at line 141) are refused as missing bindings. Each asserts `joint` to its concrete type, as in
  `Physics2DJoint` to `Physics2DGearJoint`.
- `debugGeometry.ts` `writeJoint` (line 228): `Readonly<Physics2DJoint>` asserted to the writable
  `Physics2DMouseJoint`.
- `stepValidation.ts` `isPhysics2DJointValid` (line 185): `for (const key in joint)` over a joint whose derived
  keys the flattened row does not carry. That is the same boundary, reported as missing closed key evidence.
- `@flighthq/physics2d-abi` `encodeJointFlags` and `getJointKindValues`: the same assertions on the ABI side.

Natively a world can hold `Physics2DJoint` rows, but no joint can be solved, drawn or validated by kind. The
checked owner recovery above is the fix. The ABI's two functions are declared without bodies, so a call fails to
link (overrides/manifest.json, physics2d-abi-variant-shape-encoding). The joint validator throws, as recorded in
physics2d-step-validation-refusals. Nothing substitutes zeros or common-field-only behavior.

The same module also refuses `world.ts` `hydratePhysics2DWorld` (line 283), on a near relative of this form.
`(world as unknown as { version?: unknown }).version` asserts the world to a row whose `version` is optional and
`unknown`. `Physics2DWorld` does declare `version: number`, so the member exists; what the emitter refuses is that
the asserted row re-types it ("optionality or value representation is not representation-equivalent to the source
type's cells"). The assertion exists so the read also tolerates a serialized world that predates the field. It reads
the same property as `world.version`, and a native `Physics2DWorld` always stores that member as a number.
[`physics2d-hydrate-version-declared-read`](../../source-patches/manifest.json) carries it as
`const serializedVersion = world.version as number | undefined`, with the `(version as number)` identity casts
dropped. Two neighbouring spellings were measured in the same reduction, and each hits a separate lowering
defect:
- an `unknown`-typed local turns `version as number` into `Any::as_number()` on what it stored as a `double`;
- a `number | undefined`-annotated local initialized from the present member turns `=== undefined` into
  `serialized_version.value().has_value()`, a presence test on the payload.
Neither compiles. The `as number | undefined` form and an `unknown` local narrowed by `typeof` both emit correct
code.

Also refused at `a627849`: the seven built-in GL PBR extension registrations in `@flighthq/scene3d-gl`
(`anisotropy`, `clearcoat`, `iridescence`, `sheen`, `specular`, `transmissionVolume`, `wrappedDiffuse`). Each
asserts `Readonly<PbrExtension>` to its concrete interface. The pending `pbr-extension-concrete-value-arms`
patch retains the owners instead; see [its request](pbr-extension-concrete-value-arms.md).

## G2 Scene3D material texture listers retain only the base material row

At compiler `806d257` / Flight `a627849`, `sceneMaterialTextureRegistry.ts` refuses
`listStandardPbrMaterialTextures`, `listUnlitMaterialTextures` and
`registerExtendedPbrScene3DMaterialTextures` as `cpp-structural-assertion-owner-unproven`.
Each registry callback receives `Readonly<Material>`. The actual emitted Material contains only
`entity_runtime_key`, `kind` and `name`; its readonly structural view does not declare the concrete
standard, unlit or extended map fields. The concrete structs are separate flattened owners.

The standard lister asserts that row to StandardPbrMaterial, then visits the existing non-null
`baseColorMap`, `emissiveMap`, `metallicRoughnessMap`, `normalMap` and `occlusionMap` in that order,
appending the same Texture references to the caller's existing array. The unlit lister appends its
non-null `baseColorMap`. The extended lister first visits its original `standard` property block,
then traverses its extensions in order and looks each kind up in the registry's extension-lister map.
Unknown material and extension kinds append nothing through the normal lookup behavior; that does
not authorize an empty-list replacement for a registered concrete material.

Pinned `ExtendedPbrMaterial.extensions` is an open `readonly PbrExtension[]`. The current carried
type patch retains the open arm alongside redundant concrete built-in references in PbrExtensionValue;
neither representation permits replacing it with a closed built-in PBR family. Likewise, a registry
kind or callback selection does not reconstruct a concrete material owner or add the missing map cells
to a base-typed row. The required recovery must retain the exact material, its live cells, the same
texture owners and the existing accumulation/dispatch order. Copying a base or concrete owner, assuming
a cast from the kind string, or narrowing opaque vendor extensions would change that contract.

The bounded declaration repairs described in
[the retained-callback request](retained-declaration-calls-omitted-callee.md#g2-scene3d-material-texture-listers-are-retained-callback-values)
address only name visibility for the standard and unlit callbacks. They supply no material refinement,
texture list or extension registration body. All three owner-recovery refusals remain open here.

## G2 Interaction graph bounds need the original node's 2D capability

At compiler `806d257` / Flight `a627849`, `interaction/hit_tests.hpp:88` marks
`hitTestGraphLocalBounds` NOT GENERATED (`cpp-structural-assertion-owner-unproven`). The source is
generic over `Node<Traits>` but asserts that same node to `Node2D` for both world-transform and
local-bounds access. The refusal names `shared_ptr<Node<Traits>>` and an unresolved asserted row;
the generic declaration does not prove the specialized trait/runtime cells the 2D accessors need.

The real operation first inverse-transforms world `(x, y)` into the module's shared scratch point,
then reads the node's actual local bounds and calls `containsRectanglePointXY`. A repair must retain
that node, its world matrix and lazily computed bounds, including provider effects and call order.
Existing concrete Scene2D storage does not prove an arbitrary generic node carries those same cells.
Retyping the public helper or its `NodeAny` callers to a closed Node2D carrier narrows the source
domain; copying a graph, fabricating a runtime or returning false changes hit behavior.

This is the same unproved narrower-row assertion family, with generic trait/runtime recovery rather
than only a nominal data downcast. No Interaction-local source spelling can establish the missing
owner relation merely by changing a type argument. No shared Node, Scene2D or TextLabel code was
changed. The [retained-callee request](retained-declaration-calls-omitted-callee.md#g2-interaction-graph-local-bounds-calls-cross-the-refused-module)
records the resulting five-header compile failure and why the current declaration repair cannot match.
