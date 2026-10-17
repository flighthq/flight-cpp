# A narrowed imported union passed back to a parameter of the declared union is refused

After `if (a.kind === 'point') return …`, TypeScript narrows `a` to the union's other arms. Passing that `a` to
a function whose parameter is the *whole* imported union is an ordinary widening back to the declared type,
and the object is unchanged. The emitter instead treats the narrowed value as a source union with fewer
alternatives, finds carriers it cannot pair with the destination's, and refuses the module.

Measured at flight-compiler `2b687ea` with this repository's 44 compiler patches applied: of four modules, the
reduction alone is refused; the control emits and compiles with no errors.

## Minimal TypeScript (measured)

```ts
// shapes.ts
export interface Circle { x: number; y: number; radius: number; }
export interface Point { x: number; y: number; }
export interface Square { x: number; y: number; side: number; }
export type Shape = (Circle & { kind: 'circle' }) | (Square & { kind: 'square' }) | (Point & { kind: 'point' });

// contains.ts
export function containsPoint(shape: Readonly<Shape>, x: number, y: number): boolean { /* … */ }

// overlap.ts — refused
export function overlapsPoint(a: Readonly<Shape>, b: Readonly<Shape>): boolean {
  if (a.kind === 'point') return containsPoint(b, a.x, a.y);
  if (b.kind === 'point') return containsPoint(a, b.x, b.y);   // `a` is narrowed to circle | square
  return false;
}
```

```text
contextual C++ union conversion requires equivalent source union evidence: target multiVariant carriers
[flight::Ref<x_y_kind_…>, flight::Ref<x_y_radius_kind_…>, flight::Ref<x_y_side_kind_…>] from identifier source
multiVariant carriers [flight::Ref<flighthq_source::x_y_radius_kind_…>, flight::Ref<flighthq_source::x_y_side_kind_…>]
```

Control (measured: emits and compiles): bind `const whole: Readonly<Shape> = a;` before the first test and pass
`whole`. It is the same object, held by a local whose declared type is the whole union.

A single-module reduction with an inline object union emits and compiles. The refusal needs the arms to be
interface-and-discriminant intersections declared in an imported module, as `CollisionBuiltInShape2D`'s are.

## Expected behavior

A narrowed reference passed where its own declared union is expected is the identity widening. It should lower
to the original variant, whose active alternative is already one of the destination's, with no conversion.

## Affected modules

`@flighthq/physics2d` `step.ts` `testPhysics2DAreaLessSensorOverlap` (line 147), which passes narrowed shapes to
`getCollisionShapeContainsPoint2D` and `testPhysics2DSegmentOverlap`.

## Carrying workaround

[`physics2d-area-less-overlap-whole-shapes`](../../source-patches/manifest.json) binds both shapes to locals of the
declared union before narrowing, as the control does, and passes those locals at the whole-union positions.


## Additional G2 site: 3D collider shapes to the convex collision union

Inspected at Flight `a62784923f3be814463286c9fb28edfe6ab15789` and compiler
`806d257d2f4100de745cc97340e61c2f301b0f24`, using the committed emitted tree.
This related-union boundary has the same imported intersection arms and qualification mismatch as the
reduction above; it also leaves calls to the refused function in other headers.

`packages/physics3d/src/colliderCollision.ts:19` declares
`collidePhysics3DColliderShapes(shapeA: Readonly<CollisionColliderShape3D>,
shapeB: Readonly<CollisionColliderShape3D>, out: CollisionContactManifold3D): boolean`.
The source unions in `packages/types/src/Collision.ts` are:

```ts
// Seven interface-and-kind intersection arms: sphere, aabb, box, capsule, cylinder, cone, convex.
type CollisionShape3D = CollisionBuiltInShape3D | CollisionVendorShape3D;
type CollisionColliderShape3D = CollisionBuiltInShape3D | CollisionStaticShape3D;
// CollisionStaticShape3D is triangle mesh | heightfield.
```

The callee first handles mesh/heightfield pairs through their dedicated routines, rejects a pair of static
concave descriptors, and reverses the manifold normal when the static shape was the first argument. After
these guards, it passes only the surviving built-in descriptors to the convex collision functions:

```ts
if (shapeB.kind === 'triangle-mesh') return collideCollisionTriangleMesh3D(shapeA, shapeB, out);
if (shapeB.kind === 'heightfield') return collideCollisionHeightfield3D(shapeA, shapeB, out);
return collideContactManifold3D(shapeA, shapeB, out);
```

`generated/include/flight/physics3d/collider_collision.hpp:187` explicitly marks the function
`NOT GENERATED: function collidePhysics3DColliderShapes -- source line 19`, with
`cpp-contextual-union-inequivalent [source-portability]`. The target list has eight unqualified
`flight::Ref<…>` owners (the seven built-in arms plus `CollisionVendorShape3D`); the identifier source
list has the seven corresponding `flight::Ref<flight::types::…>` built-in owners, in a different order.
Every surviving source arm already names one canonical built-in owner in the destination union.
The diagnostic's qualification/order difference identifies the failed match; it does not justify
retyping an owner, copying a shape, or treating a mesh as convex.

`sweepPhysics3DColliderShapes` at source line 88 is refused with the same lists and family. Its source
additionally preserves relative deltas, time-of-impact output, maximum fraction, and normal reversal.
The refusal ledger currently records that sweep site for the module; the header's individual NOT GENERATED
markers retain both refused functions. A module-level ledger row must not be mistaken for evidence that
the collide body exists.

The immediate compile failure is the retained-call half of
[retained declarations calling omitted callees](retained-declaration-calls-omitted-callee.md):

| Retained caller | Missing call site |
| --- | --- |
| `contact_intake.hpp` | line 284, `collide_physics_3d_collider_shapes` |
| `continuous.hpp` | line 375, `collide_physics_3d_collider_shapes`; line 480 also calls the refused sweep |
| `solver.hpp` | line 510, `collide_physics_3d_collider_shapes` |

The call carriers are visible in the emitter's own declarations: both `Physics3DCollider::world` members
are `flight::types::CollisionColliderShape3D`, and the output members are
`flight::Ref<flight::types::CollisionContactManifold3D>`. The retained sibling raycast uses the same
collider alias for its `Readonly<CollisionColliderShape3D>` parameter. No collide or sweep definition
exists in the generated, override, or runtime headers. These observations establish the omission; no
replacement declaration or body is supplied by this disposition.

A runtime method or external binding cannot restore a body omitted from this TypeScript module. The
whole-union-local workaround carried for the 2D site above cannot simply be bound to `CollisionShape3D`
before narrowing here: the collider union still admits triangle meshes and heightfields, which the convex
union deliberately excludes. The source must first prove the built-in subset, and that is precisely the
conversion this emitter refuses. No source patch was attempted or claimed successful for this 3D site.

The existing `declare-refused-function` repair tests the exact refusal marker, retained call, and absence
of a definition **within one header**. Here the marker is in `collider_collision.hpp` and the calls are
in sibling headers, so it cannot match. Extending that mechanism would require cross-header witnesses
and still leave the collider behavior absent; a naked forward declaration added through another repair
kind would bypass those guards. No such workaround is carried. A declaration-only compiler fix may make
these callers compile while leaving actual calls visibly failing to link. Supplying the real behavior
requires the owner-preserving subset conversion above and full emission of the original branches.

G2 cluster `missing-member-fd48c0bb` contains exactly seven headers:
`_internal_index.hpp`, `contact_intake.hpp`, `continuous.hpp`, `contract.hpp`,
`enable_physics_3d_guards.hpp`, `solver.hpp`, and `step.hpp`, all under `flight/physics3d/`.
**0/7 pass, 7 fail** is retained from G1. This disposition changes no C++ input, so it does not rerun an
unchanged compile or claim new native results. Each owned status row remains failing and links this
request; clearing the first missing name alone must not be reported as working collision behavior.
