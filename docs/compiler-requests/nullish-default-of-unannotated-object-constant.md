# `value ?? DEFAULT` with an unannotated object constant is refused

When the right side of `??` is a module constant written as a bare object literal, the emitter gives the
constant its own anonymous struct. The left side is a member declared as a named interface. The result is then
a union of the nominal owner and the anonymous struct, which matches no destination carrier, so the module is
refused. In TypeScript the constant is structurally the interface, and the expression's type is the interface.

Measured at flight-compiler `2b687ea` with this repository's 44 compiler patches applied: the reduction is
refused; the control, which only annotates the constant with the interface, emits.

## Minimal TypeScript (measured)

```ts
// filterTypes.ts
export interface Filter { categoryBits: number; maskBits: number; groupIndex: number; }
export interface Holder { filter: Filter; }

// pair.ts — refused
import type { Holder } from './filterTypes.ts';
export function pairEnabled(a: Readonly<Holder>, b: Readonly<Holder>): boolean {
  const filterA = a.filter ?? defaultFilter;
  const filterB = b.filter ?? defaultFilter;
  return (filterA.maskBits & filterB.categoryBits) !== 0 && (filterB.maskBits & filterA.categoryBits) !== 0;
}
const defaultFilter = { categoryBits: 1, maskBits: 0xffffffff, groupIndex: 0 };
```

```text
contextual C++ union conversion requires equivalent source union evidence: target multiVariant carriers
[flight::Ref<flighthq_source::Filter>, flight::Any] from binary source multiVariant carriers
[flight::Ref<flighthq_source::Filter>, flight::Ref<category_bits_mask_bits_group_index_…>]
```

Control (measured: emits): `const defaultFilter: Readonly<Filter> = { … }`. The control's standalone header
needs the SDK's generated structural member table for its row reads, which a single-directory reduction does
not have; in the package build that table exists, and `step.hpp` is the compile check.

## Expected behavior

When an unannotated object constant's members are exactly a named interface's, and the `??` result is used as
that interface, the constant should lower as that interface, or the binary expression should take the left
operand's declared carrier. A union of a nominal and an anonymous owner for one structural type should not
arise.

## Affected modules

`@flighthq/physics2d` `step.ts` `isPhysics2DColliderPairEnabled` (line 182), using `defaultCollisionFilter`
(line 1377).

## Carrying workaround

[`physics2d-default-collision-filter-annotation`](../../source-patches/manifest.json) annotates the constant as
`Readonly<Physics2DCollisionFilter>`.
