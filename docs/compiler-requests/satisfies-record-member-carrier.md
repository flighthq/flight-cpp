# Members of a `satisfies Record<string, T>` table must carry `T`, or the emitter must convert them

`TransformMode2D` is a preset table declared with `satisfies Record<string, TransformInherit2D>`. TypeScript
checks each member against `TransformInherit2D` but keeps its literal type, which is assignable wherever a
`TransformInherit2D` is expected. The emitter gives every member an anonymous struct carrier
(`reflection_rotation_scale_translation_…`), and when a member is then stored into a declared
`TransformInherit2D` slot it inserts no conversion. Emission is not refused; the header fails to compile.

Measured at flight-compiler `2b687ea` with all 42 of this repository's compiler source patches applied, one
module per case, then each emitted header compiled with clang 22.

## Minimal TypeScript (measured)

```ts
// types.ts
export interface Inherit { reflection: boolean; rotation: boolean; scale: boolean; translation: boolean }
export interface Bone { name: string; mode: Inherit }
export const ModeSatisfies = {
  Normal: { reflection: true, rotation: true, scale: true, translation: true },
  OnlyTranslation: { reflection: false, rotation: false, scale: false, translation: true },
} satisfies Record<string, Inherit>;
export const ModeTyped: { readonly Normal: Inherit; readonly OnlyTranslation: Inherit } = {
  Normal: { reflection: true, rotation: true, scale: true, translation: true },
  OnlyTranslation: { reflection: false, rotation: false, scale: false, translation: true },
};

// a_satisfies_preset.ts
import type { Bone } from './types.ts';
import { ModeSatisfies } from './types.ts';
export function boneSatisfies(name: string): Bone { return { name, mode: ModeSatisfies.Normal }; }
```

`ModeSatisfies` is emitted with `flight::Ref<reflection_rotation_scale_translation_9dff6858eecb0eb4>` members,
and `boneSatisfies` as `Bone{.name = name, .mode = mode_satisfies->normal}`:

```text
error: no viable conversion from 'flight::Ref<reflection_rotation_scale_translation_9dff6858eecb0eb4>' to 'flight::Ref<Inherit>'
```

## Pinned boundary

| case | preset member carrier | emitted header |
| --- | --- | --- |
| `satisfies` table, member stored into `Bone.mode` | anonymous struct | **fails to compile** |
| explicitly typed table (`readonly Normal: Inherit`), same store | `flight::Ref<Inherit>` | compiles |
| `satisfies` member bound first to `const mode: Inherit`, then stored | anonymous struct (the local is emitted as `auto`) | **fails to compile** |

The third row shows the local's declared type is not used either: the anonymous carrier flows through it.

## Required C++ shape

`satisfies T` (or `satisfies Record<K, T>`) gives the checker the evidence that each member is a `T`. The
emitter should either give the members the `T` carrier, as the explicitly typed table does, or construct
the declared `T` at the store. It must not emit a store between unrelated carriers.

## Affected Flight modules

`@flighthq/types` `TransformMode2D.ts` declares the table; every consumer that stores a preset into a
`Bone2D.transformMode` is affected:
- `@flighthq/skeleton2d-formats` `spineJsonBonesHandler.ts` and `spineBinaryBonesHandler.ts` (emitted
  `spine_json_bones_handler.hpp`, `spine_binary_bones_handler.hpp`);
- `@flighthq/scene2d-formats` `riveSkeleton.ts` (`rive_skeleton.hpp`).

Native code cannot pass a preset where `TransformInherit2D` is expected; `generated_skeleton2d_test.cpp`
builds the inherit flags directly for that reason.

## Carrying workaround

None. The faithful source fix is in `@flighthq/types` (declare the table's member type, as `ModeTyped`
does), which is outside the packages that consume it.
