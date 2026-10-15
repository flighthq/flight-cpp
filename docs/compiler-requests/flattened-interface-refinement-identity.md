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
No workaround is carried. Measured at `swf_appearance.hpp:123` (and through it `swf_place_object_handler.hpp`):
`flight::Array<flight::Ref<flight::types::Adjustment>>{create_color_scale_bias_adjustment(...)}` has no matching
constructor. The element is a fresh entity whose only reference is the array, and its `colorMatrix`/scale-bias
fields are what the consumer reads, so allocating it as an `Adjustment` would narrow the source domain. No emitted
package stores a concrete adjustment in an `Adjustment[]` today, so there is no existing carrier to reuse.

Also blocks `swf_sound_handler.hpp` at a627849: `cues: TimelineCue[]` receives
`TimelineStreamAudioCue`, which extends `TimelineCue` in TypeScript but not in the emitted C++ (3 sites).
The sites are `cues.push(cue)` into `flight::Array<flight::Ref<flight::types::TimelineCue>>`: a
`TimelineStreamAudioCue` in `appendSwfStreamSoundCue`, and a `TimelineAudioCue` in `readSwfStartSound` and
`readSwfStartSound2`. The carrier is a plain array on `SwfTagTimelineState`/`TimelineSource.cues`, with no union, and
the cue is shared by identity: the same object is also parked in `soundCuesAwaitingRate`/`soundCuesAwaitingClass` and
completed later, so neither a copy nor a base-typed allocation is faithful. The runtime has no bridge either:
`flight::StructuralRef<RowReadonly<RowOf<Ref<TimelineCue>>>>` built from a `Ref<TimelineStreamAudioCue>` fails
`structural_ref.hpp`'s "incompatible object type" assertion.

Measured with a scratch model, real C++ heritage would be sufficient for this flow with no runtime change:
- the derived reference pushes into `flight::Array<flight::Ref<Base>>` with identity kept;
- a base `StructuralRef` view reads the base fields, which is the argument `TimelineCueHandler` receives;
- `StructuralRef::shared_exact_object<Derived>()` recovers the same object.

That recovery succeeds only when the object first enters the owner registry as its exact type. The registry is keyed
by address and first entry wins, so the emitted shape should register the exact owner at construction. C++20
designated initializers cannot name base members, so heritage-carrying construction cannot keep the
`Derived{.field = ...}` form for inherited fields.

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
Also the physics2d joint solvers and debug draw at a627849 (measured 2026-10-14). `Physics2DWorld.joints` stores
`Ref<Physics2DJoint>`. Each of the nine built-in solvers in `joints.ts` (`physics2DDistanceJointSolver` through
`physics2DGearJointSolver`), and `debugGeometry.ts` `writeJoint` (for `Physics2DMouseJoint`'s target), refines the
stored joint to its concrete interface by `kind` before reading the derived members. That is this request's base
to derived direction. A scratch inventory that gave the nine joint structs their declared `Physics2DJoint` base
(the emission-repair shape of [interface-heritage-widening](interface-heritage-widening.md)) leaves `joints.ts`'s
solver bindings refused and `debug_geometry.hpp` failing exactly as before. Heritage supplies the widening, but a
refinement still needs a checked, owner-preserving view, and `flight::ReferenceEnabled` is an empty,
non-polymorphic aggregate base. Solver dispatch and joint drawing therefore wait on the runtime/compiler decision
this request describes. Nothing is carried.

## One user decision across loader, signals and Physics2D

This request and [generic-owner-into-widest-container](generic-owner-into-widest-container.md) are the same
missing capability seen from two sides. In both, one object has to be reachable through two distinct emitted C++
owner types without a copy, and recovered from the wider one with its identity intact. Here it is a declared
interface refinement (base to derived); there it is a generic handle stored as its widest instantiation.

The Manager has ruled that this is a user decision, not fleet work. Until it is made:
- Physics2D stays honestly partial: no solver dispatch, joint factories or joint drawing natively, and factories
  are not landed alone.
- signals keeps `connectSignalTracked` refused.
- The loader residual named in that decision waits on it too. Its measured doc is not on this base, so it is named
  here rather than linked.
