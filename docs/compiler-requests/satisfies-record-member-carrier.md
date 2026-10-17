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

## G2 Spine binary table selection and bone storage are distinct boundaries

At compiler `806d257` / Flight `a627849`, `spine_binary_bones_handler.hpp` first failed because
its readonly preset tuple became `Array<Any>`, mixing an erased element with an exact preset Ref in
the ordinal selection. The [local element-union source annotation](readonly-preset-tuple-element-owner.md)
retains those original owners. Normal generation includes the unchanged canonical TransformMode2D
header so the existing duplicate-struct alias engine preserves its exact shared flags carrier. Whole
source JavaScript and every emitted function body remain unchanged; no preset is reconstructed.

The independent store still expects `Bone2D.transform_mode: Ref<TransformInherit2D>`, while each
preset is `Ref<reflection_rotation_scale_translation_9dff6858eecb0eb4>`. The two declarations still
inherit ReferenceEnabled independently and share no native base relation. A source assertion,
unchecked pointer cast, copied four-flag object or changed Bone2D slot cannot supply the original
preset owner. The shared `satisfies` table itself is not rewritten by this one-header task.

The final scoped result is **0/1 pass** at the earlier optional Record diagnostic construction63;
this nominal-store mismatch is inspected, not claimed as a new winning compiler diagnostic or a
native parsing pass. Preserve the reader's exact cursor/field order, root parent-byte omission,
invalid-parent Drop/overrun behavior, original diagnostics/context arrays and all five preset identities
when the owner contract is fixed. Those source requirements are recorded in the linked request.


## G2 Spine JSON bones store the original shared preset into a nominal slot

At Flight `a62784923f3be814463286c9fb28edfe6ab15789` / compiler
`806d257d2f4100de745cc97340e61c2f301b0f24`, cluster `no-viable-conversion-f7267dc5` contains
`flight/skeleton2d_formats/spine_json_bones_handler.hpp`. Its recorded first failure at native36 is
`no viable conversion from 'Ref<reflection_rotation_scale_translation_9dff6858eecb0eb4>' to 'Ref<flight::types::TransformInherit2D>'`.
This is the exact module already listed above, at source spineJsonBonesHandler.ts69-82's
createPlaceholderBone2D return, not a failure to recognize a JSON schema:

```ts
function createPlaceholderBone2D(): Bone2D {
  return {
    length: 0, name: null, parentIndex: -1, rotation: 0,
    scaleX: 1, scaleY: 1, shearX: 0, shearY: 0,
    transformMode: TransformMode2D.Normal, x: 0, y: 0,
  };
}
```

The generated Bone2D initializer stores transform_mode_2d->normal directly into
Bone2D::transform_mode: Ref<TransformInherit2D>. The canonical TransformMode2D table's five members
all retain Ref<reflection_rotation_scale_translation_9dff6858eecb0eb4>, while TransformInherit2D is
an independent ReferenceEnabled struct. Identical reflection/rotation/scale/translation fields do
not create a shared_ptr conversion or C++ inheritance. The same retained spine_transform_mode helper
at native39-55 declares Ref<TransformInherit2D> and returns each of the five anonymous preset owners;
those are inspected additional stores/returns, not newly measured next compiler diagnostics.

A local mode:TransformInherit2D annotation was already measured above to retain the anonymous carrier.
A declaration repair cannot change an existing initializer or establish a relation between these
owners. Copying the four flags into a fresh nominal object, changing Bone2D storage to Any or just the
five preset arms, or casting unrelated pointers would lose preset identity or narrow the independent
four-axis domain. The existing canonical-table typed-member solution must retain the actual module
preset objects and be reconciled at their defining/generated owner boundary; this one-header disposition
leaves that shared table, other consumers and all C++ unchanged. No source/runtime/compiler prototype,
canonical-table rewrite, fresh generation or native rerun is claimed here.

### Full bone and JSON contract remains required

- Nonarray raw input yields a fresh empty bone list. For an array, preserve every input position and
  visit entries in order. Null or nonobject entries report Recover spine.malformed-bone-recovered,
  origin parseSpineSkeleton, detail {bones:1}, then append one real placeholder Bone2D and continue.
  Its name is null, parent=-1, length/rotation/shears/x/y=0, scales=1 and mode is the original Normal
  preset. This source-authored recovery is necessary for file-order weighted-mesh indices; it is not
  permission to replace the parser with placeholder-only output, drop malformed entries or return
  empty success. When diagnostics is absent, the same positional bones still exist.
- The object guard is only non-null typeof object, so arrays pass it too. Record<string,unknown> is
  a view of that original entry's members, not a demand for a declared native bone/anonymous schema.
  Missing fields read undefined and use their actual fallbacks. Preserve repeated member reads and
  exception order; do not add a plain-object test, require names/parents, copy a schema owner or
  coerce strings into numbers. The retained handler's doc.get("bones").value() exposes the existing
  [parsed-JSON/member-read boundary](parsed-json-object-shape-reads.md): missing bones must reach the
  nonarray path as undefined instead of throwing bad_optional_access.
- A string name, including empty string, is kept; other names become null. ParentIndex begins at-1.
  For a string parent, scan only the already-produced local bones backwards and take the first equal
  name. Thus the latest preceding duplicate wins. With no preceding name match, absent, forward or
  self parent references remain-1 without a new diagnostic. Do not topologically sort, search future entries, deduplicate
  names, repair an unresolved parent later or resolve against preexisting context.bones instead.
- Each accepted object creates one Bone2D with length/rotation/shearX/shearY/x/y default0 and
  scaleX/scaleY default1. numberOr tests typeof number only, retaining -0, negative, NaN and infinite
  numeric values rather than applying a finite-only validator. Bone angles stay in authored degrees.
  The same object references then append to the original context.bones array after parsing completes;
  do not replace that array, clone results or offset local parent indices by preexisting context length.
  The exported spineJsonBonesSectionReader remains the exact handler alias and registry member.
- spineTransformMode returns the original OnlyTranslation, NoRotationOrReflection, NoScale or
  NoScaleOrReflection singleton for its four matching strings, and the original Normal singleton for
  every other value, including absent/unknown transform. All five presets keep their own four booleans
  and shared identity; no copied flag struct or ordinal enum replaces them. Bone2D.transformMode still
  accepts the complete independent TransformInherit2D domain beyond those five authored presets.

parseSpineBones24 remains explicitly omitted with
`cpp-presence-test-without-absence-storage`. Its source iterates an unknown array and checks each
entry for null/object before reading it as a Record. The diagnostic itself names only an identifier;
no uniquely failing variable is inferred from the text. [Existing erased-array coverage](erased-array-element-reading.md)
already lists this exact module. AnyArray preserves original array identity and exact Array<T>
recovery, but an array guard does not establish T or provide a general erased-element view. Accepting
only Array<Any>/Array<Ref<Bone2D>>, copying input cells or dropping unsupported carriers narrows the JSON
input domain and does not implement this algorithm.

The retained section handler at native114-120 directly calls the missing parse_spine_bones with
Any from doc.bones and optional<Array<Ref<ImportDiagnostic>>> from its original context, then appends
returned bones. [Retained-callee coverage](retained-declaration-calls-omitted-callee.md) records that
separate declaration boundary. A declaration would not supply parsing or repair the earlier preset
initializer. No candidate declaration, unused-header compile/link/run or qualified negative-link check
was attempted or claimed. Fresh diagnostic detail literals must retain the real collector contract;
none was separately emitted or compiled in this disposition.

spineParse.test.ts specifies hierarchy/numeric setup fields, exact preset identity for all modes and
unknown fallback, and an aligned placeholder at index1 between two valid bones. The small handler test
checks function existence and exact reader alias; it alone establishes no parsing parity. These tests
were read as source requirements only, not ported or run here. No native bone/parser/weighted-mesh,
registry, animation or header-availability/link success is claimed.

**Recorded 0/1 pass, 1 fail is retained.** This append-only existing request note, worklist143 and two
compact owned product lines (one cluster/one failed row) link the actual preset carrier problem and
related erased-array/JSON/retained-callee requests. C++, shared owners/presets, runtime, source patches,
repairs, generation inventory and pins are unchanged; unchanged C++ is not rerun. Foreign compact
products, worklist entries, previous binary disposition and historical/G1 totals remain intact.
