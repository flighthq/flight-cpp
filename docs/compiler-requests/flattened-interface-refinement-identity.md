# Identity-preserving flattened interface refinements

TypeScript interface extension and intersection relationships are emitted as independent C++ owner
structs. An assertion that refines a base interface to a derived interface therefore has no C++ heritage
path, even when the same object satisfies the refined fields.

## Minimal TypeScript

```ts
interface Source { kind: string }
interface RenderTarget extends Source { kind: 'render-target'; width: number }
interface Texture2D { readonly dimension: '2d'; source: Source | null }
interface RenderTexture extends Texture2D { source: RenderTarget }

declare function createTexture(source: Source): Texture2D;
export function createRenderTexture(target: RenderTarget): RenderTexture {
  return createTexture(target) as RenderTexture;
}
```

## Expected C++

The emitted ownership model must make the refinement a checked view of the existing object, or preserve
real interface heritage. Conceptually:

```cpp
auto texture = create_texture(target);
return flight::structural_ref_cast<RenderTextureView>(texture);
```

The returned value must share the exact reference identity and lifetime of `texture`. A native pointer
cast between unrelated structs, a copied `RenderTexture`, or an auxiliary identity table is not an
equivalent lowering.

## Measured at a627849: why there is no bounded lowering here

Taken with every carried patch applied, on the concrete pair the loader is refused for --
`flight::types::ResourceLoader` and `ResourceLoaderInternal`:

- **They are independent flattened structs.** Both derive `flight::ReferenceEnabled`, neither derives the
  other, and the derived one **re-declares every base field** before its own. The emitter's field order is
  base-fields-then-derived-fields, so the relationship is known where the struct is written; it is not
  emitted as a base relation. A C++ base would therefore have to also *remove* the duplicated fields, or a
  read through one view and a write through the other would touch two different cells -- which is exactly
  the aliasing this request needs and the one thing a duplication cannot give.
- **The refusal is the model's own boundary.** `cpp-reference-assertion-without-heritage` fires on
  `emitType(source) !== emitType(target) && !isCppClassHeritageRelatedCpp(source, target)`, and its remedy
  text is to declare the value as the target where the concrete type is known, or to construct it
  explicitly. That is the whole model: a class relation is castable, an interface relation is not, and
  nothing between the two exists.
- **The runtime's owner-preserving cast is for structural rows, not for these.** `structural_ref_cast`
  keeps one live owner (`from_owner`) and its own comment says it "grants no reinterpret-cast of the native
  object". These owners have no row cells -- their fields are plain members -- so a view of one as the other
  would be a native reinterpretation, which this repository forbids, unless the storage itself becomes a row
  (a representation change, below).
- **A checked identity-preserving recovery exists and has no carrier to use.** `flight::ErasedRef` records
  the exact type a reference was erased from and `erased_ref_as<T>` recovers it, checked, with the same
  object. An entity is the natural carrier -- `EntityRuntime::binding` is exactly such a slot -- but the
  emitter's `allocate_entity` sets `entity_runtime_key` to `std::nullopt`, and no generated header ever
  assigns it. So there is no runtime owner to recover from today. Populating it means every entity
  allocation carries its concrete owner erased: a representation decision for every entity type in the SDK,
  not a lowering for this assertion.
- **A per-site variant does not exist.** The representation is per *type*, fixed where the struct is
  emitted. "Inherit only where a refinement is asserted" would emit the same interface differently in
  different modules, so the same object could satisfy it in one and not another.

The three repairs the filed text rules out (a native cast, a copy, a row carrier) are ruled out by the same
evidence, and so are the two this measurement adds: a base relation without removing the duplicates, and a
recovery with no carrier behind it.

## What a faithful fix would actually be, as a decision rather than a lowering

Two designs are faithful, and both are representation changes rather than a bounded patch:

1. **Real heritage for interface extension.** Emit `interface B extends A` as C++ inheritance, with `A`'s
   fields declared **once** (in `A`) rather than duplicated into `B`. Then `Ref<B>` converts to `Ref<A>`
   implicitly with the same object, and a base-to-derived assertion becomes a checked `dynamic_cast` over
   one object whose cells are shared. This changes the emitted layout of every interface that extends
   another, across every package.
2. **A carried owner with a checked recovery.** Every entity allocation records its concrete owner in the
   runtime it already has a slot for (`EntityRuntime::binding`), and a refinement lowers to
   `erased_ref_as<T>` -- same object, checked, no side table, no reinterpretation. This is
   [`generic-owner-into-widest-container.md`](generic-owner-into-widest-container.md)'s architecture applied
   to entity owners, and it was already rejected once for Signals.

Either is a deliberate, repository-wide decision. Neither is a per-assertion lowering, and approximating one
with a native cast, a materialized owner, or side storage is what the refusal exists to prevent. This is
recorded in [`compiler-interface-heritage-owner-widening.md`](compiler-interface-heritage-owner-widening.md),
which reaches the same boundary from the assignment direction.

## Affected modules

`createRenderTexture` in `@flighthq/texture` is refused at the `Texture2D` → `RenderTexture` assertion.
The same representation boundary also omits the `Texture` → `CubeTexture` and `TextureCube` →
`CubeTexture` assertions in the package's cube-texture placeholder.

## Carrying workaround

There is no downstream workaround: materializing the refined struct would replace the source object and
break reference equality. The generated placeholders deliberately retain the refusal until the compiler
and runtime agree on an owner-preserving interface-refinement carrier.

Also blocks `@flighthq/swf` at a627849: `swf_appearance.hpp` builds `colorAdjustments: Adjustment[]` from
`createColorScaleBiasAdjustment(...)`. `ColorScaleBiasAdjustment` extends `ColorMatrixAdjustment` extends
`Adjustment` in TypeScript, but the emitted structs are independent, so a `Ref<ColorScaleBiasAdjustment>`
cannot enter `flight::Array<flight::Ref<flight::types::Adjustment>>` without a copy that would split identity.
No workaround is carried.

Also blocks `swf_sound_handler.hpp` at a627849: `cues: TimelineCue[]` receives
`TimelineStreamAudioCue`, which extends `TimelineCue` in TypeScript but not in the emitted C++ (3 sites).

Also blocks `@flighthq/movieclip` at a627849, and it is the package's own root refusal: `movieClip.ts` fails on a
reference assertion from `flight::Ref<flight::types::Node2DRuntime>` to `flight::Ref<flight::types::MovieClipRuntime>`.
The pattern is the one this file already describes, at the package's runtime boundary rather than a texture -- a base
runtime is asserted to the derived runtime that a `MovieClip` carries (`createMovieClipRuntime`'s
`createNode2DRuntime() as MovieClipRuntime`, and every `clip[EntityRuntimeKey] as MovieClipRuntime` read) -- and the
emitter, correctly, will not use a native pointer cast, materialize a replacement row, or invent side storage. Nothing
in the module is supplied and no workaround is carried, which is why 4 of that package's 5 modules refuse: `contract.ts`
and `index.ts` behind it, and the two remaining modules behind dependencies (below).

Also blocks `@flighthq/swf` at a627849, in two modules that emit rather than refuse: `swf_timeline_parse.hpp`
and the tag families call a handler's `parse` with the reader they hold, and the handler's signature expects the
`SwfTagReader` interface -- `no known conversion from 'flight::Ref<flight::swf::SwfReader>' to
'std::shared_ptr<flight::types::SwfTagReader>'`. A `SwfReader` is a `SwfTagReader` in TypeScript; in the emitted C++ the
two are independent owners, so the call has no conversion, exactly as the texture and scene2d sites have none. The same
package's `swf_frame_action.hpp` fails the same way one step further out, converting a row view of a `MovieClip` into
the `Ref<MovieClip>` the timeline API declares.

Also blocks `@flighthq/scene2d` at a627849, where it is the larger cluster: five modules refuse on the same family --
`displayObject.ts`, `sprite.ts`, `scene2d.ts`, `scale9Sprite.ts` and `htmlView.ts`, the first four on reference
assertions between a base and a derived runtime or node type, the fifth on a structural assertion from a readonly row.
Because `@flighthq/movieclip` reaches scene2d, this is also the dependency blocker behind that package's
`spritesheetTimelineSource.ts`. The engine `@flighthq/movieclip` drives -- `@flighthq/timeline` -- is unaffected: all
three of its modules emit.
