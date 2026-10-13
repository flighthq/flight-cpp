# Cross-package anonymous object shapes lose their common owner

An anonymous object value accepted structurally by an imported function becomes two unrelated C++
owners when the caller and callee are in different packages. The source has one exact four-field
contract at the call boundary, but the emitter declares the same byte-identical struct once in each
package namespace:

```cpp
namespace flight::mesh {
struct w_x_y_z_5137f2b28145ccce : public flight::ReferenceEnabled {
  double w;
  double x;
  double y;
  double z;
};
bool get_mesh_geometry_vertex_joints0(
    flight::Ref<w_x_y_z_5137f2b28145ccce> out, ...);
}

namespace flight::skeleton3d {
struct w_x_y_z_5137f2b28145ccce : public flight::ReferenceEnabled {
  double w;
  double x;
  double y;
  double z;
};

auto joint = flight::make_ref<w_x_y_z_5137f2b28145ccce>(...);
flight::mesh::get_mesh_geometry_vertex_joints0(joint, ...); // does not compile
}
```

The matching structural hash is not enough in C++: `flight::mesh::...` and
`flight::skeleton3d::...` are different nominal types, so Clang reports:

```
no viable conversion from
  Ref<flight::skeleton3d::w_x_y_z_5137f2b28145ccce>
to
  Ref<flight::mesh::w_x_y_z_5137f2b28145ccce>
```

## Minimal TypeScript

```ts
// package a
export function read4(out: { w: number; x: number; y: number; z: number }): void {
  out.x = 1;
}

// package b
import { read4 } from '@example/a';

export function useRead4(): number {
  const value = { w: 0, x: 0, y: 0, z: 0 };
  read4(value);
  return value.x;
}
```

TypeScript passes the same object reference to `read4`; no conversion, copy, or second object is
created. C++ needs the same identity-preserving relationship.

## Expected C++

Either emit one canonical structural owner for an identical anonymous shape used across a call edge:

```cpp
using w_x_y_z_5137f2b28145ccce =
    flight::mesh::w_x_y_z_5137f2b28145ccce;
```

or lower the parameter as a writable structural view which retains the caller's owner. In either
case, writes made by the callee must be visible through the original object and its reference identity
must not change.

Canonicalization must be based on the resolved source call boundary, not merely on coincidentally
equal field lists. Unrelated `{x, y}` objects are structurally assignable in TypeScript but choosing an
arbitrary domain name for all of them would produce misleading and potentially cyclic C++ ownership.

## Focused evidence

At Flight `a62784923f3be814463286c9fb28edfe6ab15789`,
`packages/skeleton3d/src/skinMeshGeometry.ts` creates `joint` and `weight` as
`{ w, x, y, z }` and passes them without transformation to
`getMeshGeometryVertexJoints0` and `getMeshGeometryVertexWeights0` from
`packages/mesh/src/meshGeometryAttributes.ts`. Both callee parameters declare those same four required
numeric fields. The emitted definitions have identical member order, types, structural suffix, and
body bytes; only their package namespaces differ. The failure occurs at both calls and disappears when
the caller declaration is made an alias of the callee declaration.

This differs from the existing tree-wide duplicate-struct repair: that repair deliberately requires a
`flight::types` copy as the canonical home. This shape exists only in `mesh` and `skeleton3d`, so it has
no `types` definition and receives no automatic alias.

## Affected modules

The measured failure is `@flighthq/skeleton3d`'s `skinMeshGeometry.ts`. The rule is shared by any
cross-package function whose parameter is an anonymous object type and whose caller passes an
independently inferred object literal of the same shape.

The same failure is independently reachable at Flight `a62784923f3be814463286c9fb28edfe6ab15789`
through `@flighthq/spritesheet-formats`. Its Cocos, libGDX, Starling, Aseprite and TexturePacker
modules pass fresh option objects to `createSpritesheetFrameData`, `createSpritesheetAnimationData`
and `createSpritesheetData` from `@flighthq/spritesheet`. The caller and callee declarations have the
same structural suffixes and byte-identical fields, but the caller owners live in
`flight::spritesheet_formats` and the parameters require the corresponding owners in
`flight::spritesheet`. Clang consequently rejects those calls before any parser can run. This reach
needs the same identity-preserving canonical-owner behavior as the four-component mesh example.

`@flighthq/textureatlas-formats` supplies a third direct reach. Its Aseprite, libGDX, Starling, and
TexturePacker parsers pass fresh region option objects to `createTextureAtlasRegion` from
`@flighthq/textureatlas`. The caller and callee declarations are byte-identical and share the same
structural suffix, but their package namespaces make them unrelated nominal C++ owners. The parser
calls therefore require the same canonical, identity-preserving owner rule.

## Carrying workaround

The emission repair `skeleton3d-mesh-attribute-four-component-owner` aliases only the caller's
`w_x_y_z_5137f2b28145ccce` declaration in
`flight/skeleton3d/skin_mesh_geometry.hpp` to the declaration in
`flight/mesh/mesh_geometry_attributes.hpp`. It introduces no operation or storage, preserves all four
field values and the exact object identity passed to both calls, and self-expires when the emitted
caller declaration no longer exists. Two companion exact-owner respellings remove the emitter's
intermediate readonly `MeshGeometry` row at those same calls because the canonical mesh completion
accepts the exact retained owner. `structural_ref_cast<Ref<MeshGeometry>>` recovers the shared object
already held by the row; it neither materializes nor copies geometry.

## Dependency-edge alias (carried)

`aliasDuplicateStructuralStructs` in `scripts/emissionRepairs.mjs` aliases a duplicated anonymous parameter
shape to the package that declares it, but only along a declared dependency edge and only when nothing is
ambiguous (see AGENTS.md, "The duplicate structural struct alias"). Its first case is
`@flighthq/particles`' `createParticleEmitterConfig(config?: Partial<ParticleEmitterConfig>)`. The libgdx,
Pixi, Starling PEX and Spine parsers in `@flighthq/particles-formats` each re-declare that partial-input
struct, and none converted to the owner's copy. The repair is computed from the tree, so it expires
when the emitter canonicalizes the owner itself.
