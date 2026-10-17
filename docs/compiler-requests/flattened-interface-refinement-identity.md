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

The original SWF measurement at a627849 also failed in `swf_appearance.hpp:123`, where
`colorAdjustments: Adjustment[]` receives `createColorScaleBiasAdjustment(...)`. Its TypeScript
heritage is `ColorScaleBiasAdjustment extends ColorMatrixAdjustment extends Adjustment`, but the
flattened concrete reference could not enter `Array<Ref<Adjustment>>`.

At compiler `806d257`, the existing declaration repairs already preserve ColorMatrixAdjustment's
Adjustment base and TintAdjustment's ColorMatrixAdjustment base. G2 extends that same bounded repair
family to ColorScaleBiasAdjustment, removing only duplicated inherited fields and retaining the
original concrete allocation through ordinary C++ shared-pointer widening. See
[the current widening repair](interface-heritage-widening.md#g2-swf-color-transform-arrays-complete-the-existing-declared-heritage-chain).
No copied base object, closed Adjustment union, pointer reinterpretation or general refinement model
is supplied. This closes that particular missing declared edge; it does not close this request's
reverse-direction owner recovery or the other unrepaired concrete interfaces.

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


## G2 loader factory: declaration-only repair exposes the construction return boundary

At Flight `a62784923f3be814463286c9fb28edfe6ab15789` / compiler
`806d257d2f4100de745cc97340e61c2f301b0f24`, the committed loader header first fails calling the
refused `initialize_resource_loader`, as covered by
[retained declarations calling omitted callees](retained-declaration-calls-omitted-callee.md).
The initializer's NOT GENERATED marker names source line 315 and `cpp-contextual-union-inequivalent`
for its `options ?? {}` fallback. The factory still emits this body:

```cpp
flight::types::EntityConstruction<flight::Ref<ResourceLoaderInternal>> out =
    flight::entity::allocate_entity<flight::Ref<ResourceLoaderInternal>>();
initialize_resource_loader(out, options);
return out; // declared factory return: flight::Ref<flight::types::ResourceLoader>
```

A temporary `declare-refused-function` candidate used the exact construction-view and optional readonly
options carriers visible at that call. Its exact refusal/call/no-definition witnesses matched, and a
second application changed nothing. The candidate supplied only a declaration, never initialization.
Compiling only `flight/loader/resource_loader.hpp` with Clang 22.1.8 at one worker produced **0/1 pass,
1 fail, 0 unattempted**. The missing name cleared, and the first error became:

```text
no viable conversion from returned value of type
'flight::types::EntityConstruction<flight::Ref<ResourceLoaderInternal>>'
to function return type 'flight::Ref<flight::types::ResourceLoader>'
```

The failure was at candidate line 468 (the stock return is line 466). No negative-link check could run
because the actual repaired header did not compile. The candidate was rejected and no repair or header
change is carried; the six original cluster rows keep their stock failure and **0/6 pass** qualification.
This result measures one root candidate, not a fresh six-header or global sweep.

`finish_entity(out)` can recover only the exact construction owner. It does not add a heritage relation
between the independent `ResourceLoaderInternal` and public `ResourceLoader` structs described above.
A copied public loader would split identity and fields; a native cast would assert absent C++ heritage.
Both the retained-call declaration gap and this owner-return boundary must remain visible until a
faithful representation closes them. No dummy initialization, fallback result, or loader behavior is supplied.

## G2 GL TextLabel private renderer data

At Flight `a627849` / compiler `806d257`, `scene2d-gl/glTextLabel.ts:48` asserts
`RendererData` to its private `GlTextLabelData`. The latter source interface extends `RendererData`,
but the generated owners independently inherit `ReferenceEnabled`. The base has only the entity
runtime key; the private owner also has allocH/allocW, image/surface, lastContentId/lastPixelRatio
and logH/logW. The getter refuses as `cpp-reference-assertion-without-heritage`; a field-prefix copy
or native pointer cast cannot recover that original object and its live raster/cache cells.
The factory's construction/finish path must retain the private allocation before a later destroy or
draw callback recovers it. A checked recovery without that carried allocation is insufficient.

The already-landed partial native glyph override demonstrates the existing exact-owner contract,
without repairing the generated Canvas API: its create-data binding boxes `NativeGlTextLabelData`
in `ErasedRef`; destroy/submit recover that exact type with `as<T>()`, rejecting a mismatched owner.
The render proxy retains its full `TextLabel`, whose typed runtime retains layout and revision cells.
No fixed glyph subset, base-owner replacement or implicit CanvasHost implementation is supplied.
Font selection/shaping remain the explicit glyph capability's responsibility.

G2 original1 now compiles through that preexisting native override (Clang22.1.8, one worker). This is
an effective-header inventory correction, not newly generated getter/draw behavior. The override
stays partial: CanvasHost/ImageHost overloads and the browser singleton renderer are unavailable.
The separately refused draw and retained callback references are recorded in the
[existing retained-declaration request](retained-declaration-calls-omitted-callee.md#g2-gl-textlabel-generated-canvas-path-remains-unavailable).
No declaration repair, normal generation, unused-header link or negative-link probe was run here.

## G2 Physics3D built-in joint solvers: declarations only

`@flighthq/physics3d` `joints.ts` at flight `a627849` refuses six of the seven built-in solver bindings
(fixed, distance, cone-twist, hinge, generic-6dof, slider) as
`cpp-reference-assertion-without-heritage`: each solver method asserts a `Physics3DJoint` to its
concrete joint type, whose emitted owner is independent of `Physics3DJoint`. Ball-and-socket emits.

`registerBuiltInPhysics3DJointSolvers.ts` re-emits the registration for all seven, so
`register_built_in_physics_3d_joint_solvers.hpp` names six members that do not exist. Carried:
`physics3d-declare-refused-*-joint-solver` inserts an `extern` declaration for each refused binding --
no solver object, method, joint assertion or registration result. The unused header compiles, links and
runs; a caller fails to link on all six symbols at once. The bodies stay owed to this request: what they
need is the identity-preserving refined joint view, not a copied owner or a narrowed constraint.

## G2 GL Scale9 shape private raster data

At Flight `a627849` / compiler `806d257`, `scene2d-gl/glScale9Shape.ts:191` asserts RendererData to
private `GlScale9ShapeData`. These are independent flattened owners: the private object adds image,
surface and lastH/lastW/lastScaleX/lastScaleY/lastContentId/lastPixelRatio cells to the entity runtime
key. The getter refuses `cpp-reference-assertion-without-heritage`. Its factory allocates that private
object, initializes it and returns through the generic slot; destroy/draw must later recover that
same allocation and its provider/cache ownership, not copy a common prefix or invent side storage.

This is distinct from the Scale9 node's inherited ShapeRuntime. The full Scale9Shape must retain its
own data commands/grid, bounds and content revision; the existing `shape/scale9_shape.hpp` carry still
marks factory/runtime construction/recovery as refused. The native `gl_shape_data.hpp` checked
ErasedRef lane holds a different mesh-data owner and explicitly throws for raster resource teardown.
Neither is a supplied private Scale9 raster surface lifecycle.

Original GL header1 remains0/1 at missing getter114, Clang22.1.8 jobs1. An exact existing declaration
qualifies its refusal/direct-call/absent-body witnesses, but the actual unused include fails a
[texture-cache carrier conversion](native-texture-source-cache-local-carrier.md) before link.
No declaration repair, source rewrite or normal generation is carried. Draw is independently refused
by an unnamed property-presence test; [retained getter/draw references](retained-declaration-calls-omitted-callee.md#g2-gl-scale9-shape-getter-declaration-stops-at-the-cache-carrier)
remain. Preserve rasterizer dispatch, mapper fallback, replay/revision/resize and host-owned
surface/Image/GPU cache acquisition and teardown; no fake drawing or narrowed shape domain.

## G2 Tilemap cell resolution: declaration only, tilemap owners still owed

`@flighthq/interaction` `registerTilemapHitTest.ts` at flight `a627849` refuses
`resolveTilemapCellIndex(source: NodeAny, x, y): number`: it asserts the `NodeAny` source to the
concrete `Tilemap` and to `Node2D`, and reads the tilemap graph, tile lookup and bounds. Both asserted
owners are flattened independents, so the refinement has no heritage path. `registerTilemapHitTest`
still registers the resolver, so `register_tilemap_hit_test.hpp` names a function nothing declares.

Carried: `interaction-declare-refused-tilemap-cell-index` inserts
`double resolve_tilemap_cell_index(flight::types::NodeAny, double, double)` -- the exact signature the
`HitTestPreciseFunction` carrier expects. No hit result, false-hit sentinel, copied tilemap graph,
narrowed node/domain or runtime provider is supplied. The unused header compiles, links and runs; a
caller that registers the hit test fails to link on the resolver. The body stays owed to this request:
it needs identity-preserving refined `Tilemap` and `Node2D` views, not a copied owner.

## G2 QuadBatch instance resolution: declaration only, quad-batch owners still owed

`@flighthq/interaction` `registerQuadBatchHitTest.ts` at flight `a627849` refuses
`resolveQuadBatchInstanceIndex(source: NodeAny, x, y): number`: it asserts the `NodeAny` source to the
concrete `QuadBatch` and to `Node2D`, then reads the atlas, instance ids, transforms and per-instance
bounds. Both asserted owners are flattened independents, so the refinement has no heritage path, and
`registerQuadBatchHitTest` still registers the resolver.

Carried: `interaction-declare-refused-quad-batch-instance-index` declares
`double resolve_quad_batch_instance_index(flight::types::NodeAny, double, double)` -- the exact
`HitTestPreciseFunction` carrier shape. No hit index, false-hit sentinel, copied quad-batch graph,
narrowed node/domain or runtime provider is supplied. The unused header compiles, links and runs; a
caller that registers the hit test fails to link on the resolver. The body stays owed to this request:
it needs identity-preserving refined `QuadBatch` and `Node2D` views, not a copied owner.

## G2 Scene3D animation needs the original node's optional morph capability

At Flight `a627849` / compiler `806d257`, no-viable-conversion-68fff449 contains one
header, scene3d/scene_animation.hpp:64. It retains **0 pass / 1 fail**, with no unchanged
C++ rerun. The existing scene3d-exact-owner-boundaries source patch replaced the original
`(target.node as Mesh).morph` read with a local SceneAnimationMorphProbe annotation. That
probe has readonly enabled and optional Mesh['morph']; its generated owner is independent:

```cpp
struct SceneAnimationMorphProbe : flight::ReferenceEnabled {
  bool enabled;
  std::variant<flight::Ref<flight::types::MeshMorph>, flight::Null, flight::Undefined> morph;
};
// target.node is Ref<Node3D>, which has no morph member:
flight::Ref<SceneAnimationMorphProbe> mesh = target->node; // no viable conversion
```

A source annotation supplies no native object or owner relationship. A nullable/readonly
respelling cannot restore the morph cell after base Node3D storage lost the concrete owner.
This is the existing identity-preserving refinement family, specifically an optional-capability
view between unrelated emitted owners. Declared Mesh-to-Node3D heritage alone is insufficient:
it does not create Node3D-to-probe inheritance or prove which concrete owner supplied the slot.
Retain the concrete node through target construction/storage and provide a checked view of
that same owner's optional morph, preserving absent and null. No pointer reinterpretation,
probe/node copy, detached weight owner or universal no-morph result is faithful. Requiring
Mesh geometry/kind or enabled/visible would strengthen the source's actual capability check.

The original Scene3DAnimationTarget carries node:Node3D and four declared paths. AnimationChannel
keeps targetRef:unknown, so TRS must still accept ordinary Node3Ds; the Weights branch may use
the same object with a morph capability and skips a null/absent morph before sampling. Preserve
the actual MeshMorph and its mutable Float32 weights array, target order and channel order.
Sample straight into those live weights with the unchanged channel.track/time and component
width, then continue: no TRS scratch write, transform invalidation, target/weight copy, fixed
four-component weights sink, clamping, normalization or deformer invocation is added. Geometry
morphing remains the later updateMeshMorph operation; applying the clip only updates its sink.

Non-Weights channels sample into the existing shared four-number scratch array, then mutate
the same Vector3 position for Translation, Vector3 scale for Scale, or Quaternion rotation
in the source fallback branch, and invalidateNodeLocalTransform once afterward. Preserve
numeric time/track domains, source sampling/easing/errors, vector/quaternion identities,
channel overwrite order and revision effects. Do not replace the node, allocate a new sink,
zero/snapshot the scratch differently, silently drop a transform path or return fake success.
Existing source tests describe translation/scale/quaternion, variable-width live morph weights,
no-morph skips and null-target skips; they are requirements, not native animation evidence.

Two later obligations are visible by inspection, not new measured first errors. First, the
existing source patch removes '|null' from the target assertion; the emitter now calls
Any::object_if<Scene3DAnimationTarget> and throws on mismatch BEFORE the retained null/typeof/
missing-node skip guard, which has become constant false. A source structural target object
or a null/non-object/missing-node value must reach its original shape check without requiring
exact nominal allocation first. The [readonly erased-recovery request](erased-value-readonly-row-recovery.md)
starts with a proven exact owner and does not by itself justify this pre-guard throw. Preserve
exact source checks and their order rather than strengthening them into a nominal Mesh gate.
Second, the TRS invalidation call spells NodeOf<Traits> with Traits undetermined, the existing
[heritage-aware generic deduction](heritage-argument-generic-deduction.md) family; Node3DTraits
must be inferred without a global alias or copied node. Neither inspected issue is promoted
to a new diagnostic or claimed solved by fixing the first owner conversion.

No source patch, repair, override, runtime, generated header, manifest or owner representation
changes in this arc. The existing broad Scene3D patch is provenance, not native owner proof.
The one failed row/cluster and worklist132 now link this request; foreign products and historical
sweep totals remain unchanged. No fake animation, narrowed target/path domain, copied owner,
empty result or native TRS/morph behavior claim is carried.

## G2 morph preparation must refine the original readonly graph owner

At Flight `a627849` / compiler `806d257`, no-viable-conversion-a516fbdb contains one
header, scene3d/prepare_scene_3d_morph.hpp:32, retaining **0 pass / 1 fail**. The existing
scene3d-morph-capability-guard patch replaces the original readonly NodeAny-to-Mesh assertion
with a readonly Mesh type predicate. The later scene3d-mesh-capability-presence patch names
Readonly<Partial<Pick<Mesh,'geometry'>> & {enabled?:boolean}> before its non-null geometry
read. This still emits an independent nominal owner, not a view of the original node:

```cpp
struct geometry_enabled_869b32e775d5849b : flight::ReferenceEnabled {
  std::optional<flight::Ref<flight::types::MeshGeometry>> geometry;
  std::optional<bool> enabled;
};
// inside is_scene_morph_mesh(readonly NodeAny row):
flight::Ref<geometry_enabled_869b32e775d5849b> candidate = node; // no viable conversion
```

The source annotation adds no storage or ownership relation. Keep its geometry-presence query
a readonly view of the SAME retained graph object, preserving absent/null geometry, and carry
the successful predicate's original-owner evidence into updateMeshMorph's readonly Mesh row
(geometry and morph). A geometry probe allocation/copy would split live cells; a mutable probe
reference would invent authority. A boolean alone does not establish a concrete Mesh owner.
Generic NodeAny/heterogeneous traits and custom-kind geometry-bearing meshes remain valid;
closing the walk to Node3D, requiring the literal Mesh kind, or returning false for every
unrecoverable owner would narrow the source graph and silently omit real morphs.

Traversal first checks enabled and returns before geometry/update/runtime/children work for a
disabled node, skipping the WHOLE subtree. Otherwise it processes the current node before
reading that same node's runtime.children and recursing in live source array order. Preserve
null children, shared node/runtime/child identities, getter/error order and the source's dynamic
length reads. Visibility is not this pass's gate. Do not manufacture an empty child list, copy
the graph, traverse only direct children or move disabled descendants into a separate pass.
The emitted runtime access also selects NodeTraits for an any-trait graph; that inspected later
obligation belongs to the existing [inferred-any/default-trait request](node-any-generic-traits-leak.md#g2-an-omitted-inferred-any-selects-the-default-node-runtime-and-breaks-queue-storage).
It is not a new measured first diagnostic, and fixing the probe does not prove that runtime
carrier or the subsequent original-owner Mesh handoff.

The actual deformer reads the original optional MeshMorph and returns before geometry/runtime
work when it is null/absent. Otherwise it uses the original MeshGeometry and its OWN shared
MeshGeometryRuntime. The first call lazily captures the interleaved geometry's bind pose and
reusable scratch there. Later calls compare the full live Float32 weight vector to the stored
snapshot, including length changes and source strict-number equality; an unchanged vector
returns without reblend/version/bounds/upload work. Preserve NaN's dirty behavior, signed-zero
equality and all weight values without clamping or normalization. Bind pose and snapshots
belong to geometry, so meshes sharing geometry keep the source traversal/shared-cache behavior;
per-mesh caches, cloned geometry or a detached weight owner would change it.

A blend restarts from the captured base, adds each nonzero weight's actual target deltas in
source order through min(targets.length,weights.length), and respects shorter sparse delta
streams. Position and present normal/tangent xyz channels are written through the real layout
into the SAME vertices buffer; UVs/colors/tangent handedness and absent channels remain intact.
Then geometry.version increments and the consumed weights are snapshotted, reusing the Float32
snapshot except when its length changes. Bounds are invalidated by version and recomputed lazily
on ensureMeshGeometryBounds, not eagerly here. Run this CPU blend after animation/manual weight
writes and before render preparation; where skinning is composed, preserve morph-before-skin
ordering without adding a skeleton or GPU-deformer dependency to this pass.

Existing source tests specify nested mesh traversal, disabled-subtree skips, rigid/no-morph
no-op, lazy bind capture, base-relative reblending, settled version stability and lazy bounds.
They were inspected as requirements, not run as native morph evidence. No C++, runtime, source
patch, repair, override, manifest, count/refusal/initialization/policy or owner representation
changes in this arc. The sole failed row/root/worklist136 now link this existing refinement
request. Both carried guard patches remain provenance, not proof of a native owner conversion.
No fake morph, copied owner, narrowed graph/weight domain, empty result or native deformation,
geometry-version, bounds or upload success is claimed; no unchanged native/generation rerun.

## G2 SWF cue arrays: the declared heritage carried as a bounded repair

At compiler `806d257` / Flight `a627849`, the two SWF headers in `call-has-no-matching-d42144f0`
fail at `swf_sound_handler.hpp:120` because `TimelineAudioCue` and `TimelineStreamAudioCue` repeat
`TimelineCue`'s `entity_runtime_key`, `frame` and `kind` with no C++ heritage, so the three measured
push sites have no conversion into `flight::Array<flight::Ref<flight::types::TimelineCue>>`.

Because TypeScript declares that heritage explicitly, the bounded repair family already carried for
the adjustment interfaces applies unchanged: `timeline-audio-cue-interface-heritage` and
`timeline-stream-audio-cue-interface-heritage` remove only the duplicated inherited members and name
the declared `TimelineCue` base. No field, value, allocation or function body is added. The cue
initializers already assign inherited members through `row_set`, and no construction site uses a
designated initializer on either type, so the inherited cells remain the same cells on the original
concrete allocation and standard shared-pointer widening keeps its control block and lifetime. A
native probe built and ran: both cue types push into the base array, the element keeps the exact
source address, and inherited `frame`/`kind` read and write through the base reference. Both headers
compile.

This is the same limited answer the request's SWF measurement anticipated; it does not supply a
general refinement model, reverse-direction owner recovery, or a runtime bridge, and the other
interface refinements listed above remain open.
