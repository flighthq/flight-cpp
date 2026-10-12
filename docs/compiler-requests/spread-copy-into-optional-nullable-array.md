# An inferred spread copy must keep its named type when it enters an optional, nullable array

`slots.map((s) => ({ ...s }))` copies each element of a `Slot[]`. TypeScript types the copy as an
anonymous object with the same members, which is assignable to `Slot`. The emitter gives that anonymous
shape its own carrier. When the array then enters a destination that is both optional and nullable
(`slots?: Slot[] | null`, reached as `Skel['slots']`), the emitter refuses it as an unrepresented domain.
The same array entering a plain `Slot[] | null` result emits, as does the copy with its callback annotated
as returning `Slot`.

Measured at flight-compiler `2b687ea` with all 39 of this repository's compiler source patches applied,
one module per case.

## Minimal TypeScript (measured)

```ts
// types.ts
export interface Slot { boneIndex: number; name?: string | null; color?: number }
export interface Skel { slots?: Slot[] | null; count: number }
export function initSkel(out: Skel, slots: Skel['slots']): void { out.slots = slots; }

// d_conditional_inferred.ts
import type { Skel } from './types.ts';
import { initSkel } from './types.ts';
export function cloneSkel(out: Skel, s: Readonly<Skel>): void {
  initSkel(out, s.slots === null || s.slots === undefined ? s.slots : s.slots.map((x) => ({ ...x })));
}
```

```text
cpp emission failed for @local/source/d_conditional_inferred.ts: contextual union value type
flight::Array<flight::Ref<bone_index_name_color_129fde6e79c68962>> is not a represented runtime domain
```

## Pinned boundary

| case | destination | result |
| --- | --- | --- |
| inferred spread copy, passed in a conditional | `Skel['slots']` (optional and nullable) | **refused** |
| inferred spread copy, passed alone after a null/undefined branch | `Skel['slots']` | **refused** |
| `(x): Slot => ({ ...x })`, in the same conditional | `Skel['slots']` | emits |
| inferred spread copy | returned as `Slot[] \| null` | emits |
| inferred spread copy of an all-required interface | returned as `Bone[]` | emits |

The refusal needs both the anonymous carrier of an inferred spread and a destination with two absence
states. Neither alone refuses.

## Required C++ shape

The copy should be constructed as the element type the destination array declares, so the array is
`flight::Array<flight::Ref<Slot>>` and enters the three-state destination like any other `Slot[]`. A spread
whose inferred type is a member-for-member copy of a named interface can take that interface's carrier
directly; no conversion between anonymous and named carriers is needed.

## Affected Flight modules

`@flighthq/skeleton2d` `skeleton2d.ts` `cloneSkeleton2D` at flight `a627849`: `skeleton.slots.map((s) =>
({ ...s }))` passed to `initializeSkeleton2D(…, slots: Skeleton2D['slots'], …)`. Refusing it refuses the
whole core module, `skeleton2d.ts`.

## Carrying workaround

[`skeleton2d-clone-slot-copy-declared-type`](../../source-patches/skeleton2d-clone-slot-copy-declared-type.patch)
annotates the callback as returning `Slot2D`, the measured emitting control. The annotation erases.
