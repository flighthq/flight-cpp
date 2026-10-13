# A named interface value passed to an anonymous object parameter has no C++ conversion

A parameter declared as an anonymous object shape, `out: { minX: number; minY: number; maxX: number; maxY: number }`,
is emitted as a fresh struct named by its members. A caller holding a named interface with exactly those
members, `SpatialAabb2D`, passes a `flight::Ref<flight::types::SpatialAabb2D>`. TypeScript accepts the call
structurally. C++ sees two unrelated owners, so the call does not compile.

This differs from [cross-package-anonymous-object-owner](cross-package-anonymous-object-owner.md), where two
packages each emit the same anonymous struct. Here one side is a named, declared interface and the other is
the anonymous shape, in the same package, so the structural-struct alias repair has nothing to join.

Measured at flight-compiler `2b687ea` with this repository's 44 compiler patches applied. The reduction
emits, and its header fails to compile; the control, whose parameter names the interface, emits and compiles.

## Minimal TypeScript (measured)

```ts
export interface Box { minX: number; minY: number; maxX: number; maxY: number; }

export function writeBounds(out: { minX: number; minY: number; maxX: number; maxY: number }): void {
  out.minX = 0; out.minY = 0; out.maxX = 1; out.maxY = 2;
}

export function fill(box: Box): number {
  writeBounds(box);   // bounds_param.hpp:37: no matching function for call to 'write_bounds'
  return box.maxY;
}
```

Control (measured: compiles): the same module with `writeBounds(out: Box)`.

## Expected behavior

When the anonymous parameter's members are exactly the argument's declared members, with the same types and
mutability, the argument is the same object and needs no conversion. Either the parameter lowers to the
argument's named carrier, or the call site gets a checked structural view of the named owner. Copying into a
fresh anonymous struct would be wrong: the callee writes through `out`, and the caller reads its own object
afterwards.

## Affected modules

`@flighthq/physics2d` `colliderTransform.ts` `writePhysics2DColliderBounds`, called with a `SpatialAabb2D` by
`broadphase.ts` (`scratch.bounds`) and `worldQueries.ts` (`scratch.colliderBounds` and `out`). At a627849 this is
the first error of 13 of physics2d's 26 headers, all through `broadphase.hpp`.

## Carrying workaround

[`physics2d-collider-bounds-spatial-aabb`](../../source-patches/manifest.json) declares the parameter as
`SpatialAabb2D`, whose four members are exactly the anonymous shape's.
