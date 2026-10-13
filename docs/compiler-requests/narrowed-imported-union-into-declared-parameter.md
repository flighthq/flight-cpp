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
