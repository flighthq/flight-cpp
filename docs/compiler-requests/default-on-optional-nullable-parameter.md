# A default value on an optional, nullable parameter must lower without a presence proof

`createSkeleton2D(bones, slots: Skeleton2D['slots'] = null)` declares a parameter whose type is optional
and nullable (`slots?: Slot2D[] | null`) and gives it a default. The emitter refuses the function: its
lowering of the default narrows the three-state carrier, and the narrowing it writes still admits absence.
The same parameter without a default emits, and so does the defaulted parameter declared two-state.

Measured at flight-compiler `2b687ea` with all 39 of this repository's compiler source patches applied,
one module per case.

## Minimal TypeScript (measured)

```ts
// types.ts
export interface Slot { boneIndex: number; name?: string | null }
export interface Skel { slots?: Slot[] | null; count: number }
export function initSkel(out: Skel, slots: Skel['slots']): void { out.slots = slots; }

// a_three_state_default.ts
import type { Skel } from './types.ts';
import { initSkel } from './types.ts';
export function create(out: Skel, slots: Skel['slots'] = null): void { initSkel(out, slots); }
```

```text
cpp emission failed for @local/source/a_three_state_default.ts: narrowed C++ union projection requires
proven present payload: the narrowing still admits absence, so nothing proved the payload present and
reading the value alternative here would unpack storage that may still be holding the sentinel the test
left reachable. Test both at once with `x != null`, or narrow the value into a present local the same way
```

## Pinned boundary

| parameter | result |
| --- | --- |
| `slots: Skel['slots'] = null` (optional and nullable, defaulted) | **refused** |
| `slots: Slot[] \| null = null` (nullable, defaulted) | emits: `slots = slots.value_or(std::nullopt)`, then passes the present value |
| `slots?: Slot[] \| null`, passed as `slots ?? null` | emits |
| `slots: Skel['slots']` (optional and nullable, no default) | emits |

## Required C++ shape

A default replaces exactly the absent (`undefined`) state, so after it the parameter holds one of the
remaining states and needs no further proof. The lowering should assign the default when the argument is
absent and then treat the parameter as the declared type minus `undefined`, as it already does for the
two-state declaration.

## Affected Flight modules

`@flighthq/skeleton2d` `skeleton2d.ts` `createSkeleton2D` (source line 152) at flight `a627849`, the last
refused declaration of the core module once the spread copy in `cloneSkeleton2D` is carried
([spread-copy-into-optional-nullable-array](spread-copy-into-optional-nullable-array.md)).

## Carrying workaround

[`skeleton2d-create-two-state-slots-default`](../../source-patches/skeleton2d-create-two-state-slots-default.patch)
declares the parameter `Slot2D[] | null = null`, the measured emitting control. The default already
excludes `undefined`, so the value the body sees is unchanged.
