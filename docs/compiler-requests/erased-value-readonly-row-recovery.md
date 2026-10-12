# Recovering an erased value as a readonly row must take the checked recovery a writable assertion takes

An animation binder receives its target as `unknown` and reads it through `target as Readonly<T>`. The
emitter refuses the module. The same function written as `target as T` emits a checked recovery,
`flight::Any::object_if<T>`, and reads the fields from the recovered owner. A readonly view of an owner
that can already be recovered is strictly weaker than the writable owner itself, so it should take the
same path and view the result as `RowReadonly`.

This is the read side of [structural-row-erasure-to-unknown](structural-row-erasure-to-unknown.md). That
request covers constructing an `Any` from a readonly row. Here the `Any` already holds an exact nominal
owner, put there by ordinary writable code, and only the recovery is refused.

Measured at flight-compiler `2b687ea` with all 39 of this repository's compiler source patches applied,
one module per case.

## Minimal TypeScript (measured)

```ts
// types.ts
export interface BoneTarget { boneIndex: number; property: string }
export class ClassTarget { constructor(public boneIndex: number) {} }

// a_readonly_recovery.ts
import type { BoneTarget } from './types.ts';
export function readonlyRecovery(target: unknown): number {
  const bone = target as Readonly<BoneTarget>;
  return bone.boneIndex;
}
```

```text
cpp emission failed for @local/source/a_readonly_recovery.ts: an actually erased value cannot be asserted as
structural row Readonly: flight::Any has no structural-row alternative or checked row projection. Preserve the
represented row through the assertion, or keep the API typed to the structural source before erasure
```

## Pinned boundary

| module | recovery | result |
| --- | --- | --- |
| `a_readonly_recovery` | `target as Readonly<BoneTarget>` from `unknown` | **refused** |
| `b_writable_recovery` | `target as BoneTarget` from `unknown` | emits `target.object_if<BoneTarget>()`, throwing when absent, then `bone->bone_index` |
| `c_class_recovery` | `target as ClassTarget` from `unknown` | emits the same checked recovery for the class |
| `d_typed_target` | parameter typed `Readonly<BoneTarget>`, no erasure | emits |

## Required C++ shape

The checked recovery the writable row already emits, then a readonly view of the recovered owner:

```cpp
inline double readonly_recovery(flight::Any target) {
  auto bone = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<BoneTarget>>>>(
      /* the same checked recovery b_writable_recovery emits */ recover_object<BoneTarget>(target));
  return flight::row_get<flight::RowKey<"boneIndex">>(bone);
}
```

No new carrier is needed: the owner is exact and nominal, and the view only removes write capability.
Recovery failure keeps the writable form's existing behavior.

## Affected Flight modules

`@flighthq/skeleton2d` at flight `a627849`. Each binder reads an `unknown` channel target as the readonly
target row, and each module is refused at its binder:
- `deformAnimationTarget2D.ts` `bindSkeleton2DDeformChannel`: `target as Readonly<Skeleton2DDeformAnimationTarget>`
- `skeleton2dAnimationTarget.ts` `bindSkeleton2DBoneChannel` and the slot binder:
  `target as Readonly<Skeleton2DAnimationTarget>` and `Readonly<Skeleton2DSlotAnimationTarget>`
- `skeleton2dDrawOrderTarget.ts` `bindSkeleton2DDrawOrderChannel`: `target as Readonly<Skeleton2DDrawOrderAnimationTarget>`

The same diagnostic appears at `@flighthq/scene2d-formats` `lottieBezierPath.ts`
(`toLottieShapePath(value: unknown)`). There the erased value is parsed JSON narrowed by `'v' in path`, not a
nominal owner, so the recovery above would not serve it; it is a different question and is not claimed
here.

## Carrying workaround

None.
