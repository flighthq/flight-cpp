# Emit declared interface heritage so derived references widen to their base

Reference-backed interfaces are emitted as independent flattened structs: each derived interface
repeats its bases' fields and inherits only `flight::ReferenceEnabled`. TypeScript widens a derived
object to any base interface implicitly, but in C++ `Ref<Derived>` has no conversion to `Ref<Base>`,
so every widening site fails to compile. Copying the derived object into a base struct would break
reference identity, so the only faithful lowering is real heritage.

## Minimal TypeScript

```ts
interface Adjustment { kind: string }
interface ColorMatrixAdjustment extends Adjustment { colorMatrix: readonly number[] }
interface TintAdjustment extends ColorMatrixAdjustment { kind: 'TintAdjustment' }

declare function createTintAdjustment(rgba: number): TintAdjustment;
declare function setAdjustments(list: Adjustment[]): void;
export function tint(rgba: number): void {
  setAdjustments([createTintAdjustment(rgba)]);
}
```

## Expected C++

```cpp
struct Adjustment : public flight::ReferenceEnabled { /* entity_runtime_key, kind */ };
struct ColorMatrixAdjustment : public Adjustment { flight::Array<double> color_matrix; };
struct TintAdjustment : public ColorMatrixAdjustment {};
```

`flight::Array<flight::Ref<Adjustment>>{create_tint_adjustment(rgba)}` then compiles, and the array
element is the same object `create_tint_adjustment` returned.

Observed at `2b687ea` (flight `a627849`, and identically at `7e2fc7d`) in
`flight/node/node_color_adjustment.hpp:71`:

```
no matching constructor for initialization of 'flight::Array<flight::Ref<flight::types::Adjustment>>'
```

## Affected modules

`packages/node/src/nodeColorAdjustment.ts` (`setNodeColorAdjustmentsTint`). The same flattening
was already repaired for the command family (`command-interface-heritage`). The opposite direction,
refining a base to a derived interface, is
[flattened-interface-refinement-identity](flattened-interface-refinement-identity.md).

The same missing heritage blocks `@flighthq/tilemap` at Flight `a627849` / flight-compiler
`2b687ea`. `setTilemapTileTint` constructs a `TintMaterialData`, which extends `MaterialData`, and
stores that exact object in the tilemap's `Array<MaterialData | null>`. The emitted owners are
independent (`TintMaterialData` and `MaterialData` each inherit only `ReferenceEnabled`), so the
derived reference cannot enter `Array<optional<Ref<MaterialData>>>` without splitting identity. The
tilemap override preserves the single object through the runtime's structural owner attachment until
declared C++ heritage makes the ordinary widening possible.

The same missing heritage is the direct current-pin refusal in all three stateful
`@flighthq/selection` modules. `LassoSelectionRuntime`, `MarqueeSelectionRuntime`, and
`SelectionStateRuntime<NodeType>` each extend `EntityRuntime`, but their emitted structs independently
inherit `flight::ReferenceEnabled` and duplicate `binding` and `uid`. Their constructors consequently
cannot store the exact runtime object in the entity's `Ref<EntityRuntime>` slot. At Flight `a627849` /
flight-compiler `2b687ea`, this refuses `createLassoSelection`, `createMarqueeSelection`, and
`createSelectionState`; the three modules then remain partial even though their other operations emit.
The selection completion headers preserve one object by spelling the declared C++ heritage and removing
only the duplicated base fields.

At flight `a627849`, `@flighthq/gizmo` reaches the same identity boundary while creating state:
the concrete `GizmoRuntime<NodeType>` object must enter the `EntityRuntime` slot on
`GizmoState<NodeType>`. The emitter refuses `createGizmoState` rather than copy or cast between the
independent flattened owners. The gizmo override keeps the concrete owner in an identity-keyed weak
table and gives the public state an ordinary entity marker; it expires with an owner-preserving
heritage carrier.

### Current-pin `@flighthq/selection`

At the same flight/compiler pins, the `selection-runtime-type-evidence` source patch makes the three
runtime object literals' declared owners explicit. That exposes the same faithful widening boundary at
all three assignments to `EntityRuntimeKey`:

- `LassoSelectionRuntime` to `EntityRuntime` in `lassoSelection.ts:28`;
- `MarqueeSelectionRuntime` to `EntityRuntime` in `marqueeSelection.ts:20`;
- `SelectionStateRuntime<NodeType>` to `EntityRuntime` in `selectionState.ts:35`.

All three refuse as `cpp-contextual-union-interface-heritage-carrier-unrepresented`. The package-only
current-pin run therefore emits 3/6 modules. Its six headers were all attempted independently at `-j2`:
2 compile without template instantiation, while lasso, marquee, contract, and index fail through the
missing runtime accessors. The tier remains `partial` with all three refused modules unsupplied. Copying
or materializing a base owner would lose the exact runtime identity, so Selection carries no workaround.

## Carrying workaround

Emission repairs `adjustments-color-matrix-interface-heritage`, `adjustments-tint-interface-heritage`
and `adjustments-color-scale-bias-interface-heritage` respell those structs with their declared heritage,
removing only duplicated base member declarations.
Selection carries the same declaration-only correction in its runtime-type overrides because scoped
selection generation cannot rewrite its already-current `@flighthq/types` dependency row. Every field
keeps its name, type and declaration order. They expire when the emitter retains
declared interface heritage.

## Same defect, the GL render state at flight a627849 (registry migration group)

`@flighthq/render-gl` `glRenderState.ts`, `destroyGlRenderState`: `destroyRenderState(state)` passes a
`GlRenderState` where `RenderState` is declared. The emitted call fails because the generated interfaces
do not preserve the declared heritage. This blocks 144 renderer header compilations across render-gl,
effects-gl, scene2d-gl and scene3d-gl. The faithful fix remains real heritage
(`GlRenderState : RenderState`), retaining one owner and enabling the intended pointer conversion.

## Same defect in Surface runtime storage

At flight `a627849` and flight-compiler `2b687ea`, `SurfaceRuntime extends EntityRuntime` is emitted as
another independent flattened struct. The `@flighthq/surface` allocator must retain that exact runtime
through the entity's `EntityRuntime` slot and return it from `getSurfaceRuntime`; without real heritage,
the current-pin override's previously valid recovery fails during scoped compilation:

```
static_cast from 'flight::types::EntityRuntime *' to 'flight::types::SurfaceRuntime *',
which are not related by inheritance, is not allowed
```

Copying the `binding` and `uid` prefix into a second object, or storing `handle` in side storage, would
break the source's single runtime identity and live-cell semantics. This site therefore needs the same
declared-interface heritage rule: one owner with the inherited `EntityRuntime` cells and the additional
`handle` cell. The surface package carries that exact shape locally until generated type heritage does.

## scene3d-formats glTF PBR extension handlers (measured 2026-10-17, not carried)

`attachGltfPbrExtension(document: Scene3DDocument, index: number, extension: PbrExtension): boolean`
is refused at flight `a627849` (`cpp-structural-assertion-owner-unproven`, its
`existing as unknown as ExtendedPbrMaterial`; see
[assertion to undeclared row member](assertion-to-undeclared-row-member.md)), so
`flight/scene3d_formats/gltf_material_extension.hpp` carries the NOT GENERATED marker while five
handlers retain calls to it (`gltf_anisotropy.hpp:121`, `gltf_clearcoat.hpp:121`,
`gltf_iridescence.hpp:127`, `gltf_sheen.hpp:124`, `gltf_specular.hpp:124`), and
`register_gltf_material_extension_handlers.hpp` fails through their includes. All three witnesses for
a `declare-refused-function` repair are present -- the marker, the retained calls, and no definition
anywhere in the tree -- but the declaration cannot be written: the emitter's own parameter is
`flight::Ref<flight::types::PbrExtension>`, while each call passes its concrete extension
(`flight::Ref<flight::types::AnisotropyPbrExtension>` and the five siblings). Every source
`XPbrExtension extends PbrExtension`; every emitted struct repeats the base fields and inherits only
`flight::ReferenceEnabled`, so the widening has no C++ path:

```
gltf_anisotropy.hpp:121:117: error: no viable conversion from 'Ref<flight::types::AnisotropyPbrExtension>' to 'Ref<flight::types::PbrExtension>'
```

Measured with a scratch declaration at the refused function's own signature, not inferred. A template
spelling `Ref<Extension>` does not rescue it either: `Ref` is a dependent alias, so deduction fails and
the call reports `no matching function`. Real heritage is what makes the declaration-only repair
applicable here. It is also not sufficient on its own: every one of the six headers still fails after
the declaration, on a second family the declaration cannot address -- the source's `Texture | null`
fields are emitted as six-arm nullish carriers that do not narrow to the declared
`optional<variant<...>>` row field.

## physics2d joint factories (measured 2026-10-14, not carried)

`@flighthq/physics2d` `jointFactories.ts` builds each concrete joint with
`allocateEntity<Physics2DDistanceJoint>()` (and the eight others), then passes the construction to
`initJointBase(out: Physics2DJoint, …)`. The nine joint structs in `flight/types/physics2_d.hpp` repeat
`Physics2DJoint`'s 18 members as an exact prefix, the shape the adjustments and command repairs respell, and a
scratch inventory with that heritage restored was measured. Heritage alone does not make `joint_factories.hpp`
compile. The argument is a writable structural view of the derived owner,
`StructuralRef<RowWritable<RowOf<shared_ptr<Physics2DDistanceJoint>>>>`, and the parameter is the nominal
`Ref<Physics2DJoint>`; the runtime has no view-to-base-owner conversion (compare
[asserted-writable-local-as-map-key](asserted-writable-local-as-map-key.md)). Carrying the factories therefore
needs a `flight/types` heritage repair shared by every consumer of those structs, plus a runtime StructuralRef
conversion. Both are cross-package decisions, and neither was made here. Even with both, the factories' joints
could be stored and read only as `Physics2DJoint`, because solving or drawing them is the refinement boundary in
[flattened-interface-refinement-identity](flattened-interface-refinement-identity.md).

### Integration's runtime fix 515dd054 (read 2026-10-14; rebased there as eb53e4a7, not on this base)

That commit lets a base-typed structural view built *from a typed pointer* to an object that already derives from
the base in C++ keep and return that object, so `structural_ref_cast<Ref<Base>>` of such a view recovers the very
carrier instead of throwing `bad_cast`. Every other view, and any request for a type other than the view's own
subject, keeps the exact-owner check. Measured against the Physics2D factory boundary above, it supplies neither
piece:
- No conversion from an existing `StructuralRef<RowWritable<RowOf<shared_ptr<Derived>>>>` (the factories'
  `EntityConstruction`) to `Ref<Base>`.
- No checked base-to-derived refinement.
- It presupposes real C++ inheritance, which the nine joint structs do not have.


## G2 SWF filters: a non-nullable conditional exposes the same missing base owner relation

At Flight `a62784923f3be814463286c9fb28edfe6ab15789` / compiler
`806d257d2f4100de745cc97340e61c2f301b0f24`, G1 cluster `other-2aef05a3` has four SWF headers
first failing at `flight/swf/swf_filter.hpp:257`. The operands of the emitted conditional are
`Ref<InnerShadowEffect>` and `Ref<DropShadowEffect>`.

`packages/swf/src/swfFilter.ts` declares `outEffects: Effect[]` and pushes one selected factory result:

```ts
outEffects.push((flags & FILTER_INNER) !== 0
  ? createInnerShadowEffect({ /* original shadow options */ })
  : createDropShadowEffect({ /* original shadow options */ }));
```

Both concrete TypeScript interfaces extend `Effect`. `packages/types/src/Effect.ts` explicitly defines
Effect as an **open base contract**, with new effects registered by their `kind`; it is not a central
closed union. The emitted destination is consequently `Array<Ref<Effect>>`, not a variant over the two
shadow owners. The effective `Effect`, `InnerShadowEffect`, and `DropShadowEffect` headers each derive
independently from `ReferenceEnabled`. The concrete structs repeat `entity_runtime_key` and `kind`
instead of inheriting the emitted `Effect`; no override supplies a different type relation.

The conditional cannot choose a common C++ pointer type, and selecting the correct branch alone still
requires widening the chosen concrete effect into the array's declared base owner. A variant containing
both concrete refs would express selection but would not provide that `Ref<Effect>` conversion. An
operator or unused runtime helper cannot change the emitted standard-library pointer relationship.

The required widening must retain the exact object returned by the selected factory, its lifetime and
all original field cells. Copying only the common fields into a fresh `Effect` loses that object and its
concrete options; a native pointer cast would invent absent inheritance. Replacing the open effect API
with a closed list changes the contract. This non-nullable site therefore reaches the same representation
requirement as the [nullable resource sites](compiler-interface-heritage-owner-widening.md), rather than a new effect-specific union capability.
The inner/outer glow and gradient effect choices later in this parser use the same pattern; they were
read as related source sites, not counted as independently measured first failures.


A bounded source experiment split only the first shadow conditional into an `if`/`else`, with the
original `outEffects.push(createInnerShadowEffect(...))` and
`outEffects.push(createDropShadowEffect(...))` in their respective branches. Each selected factory
still receives its original fields in order, and its returned object is appended once to the same
array. No object is copied, cast, or substituted.

Normal focused SDK generation applied that provisional source patch at the locked pins, lowered the
1768-module dependency closure, and emitted only `packages/swf/src/swfFilter.ts`. The exact emitted
header, without any handwritten C++ rewrite, was then used as a one-header include overlay against the
canonical generated SDK. Only the four owned headers were compiled, with Clang 22.1.8 at one worker:
**0/4 pass, 4 fail, 0 unattempted**. Every first error is `no matching member function for call to
'push'` at candidate `swf_filter.hpp:288`, the direct inner-shadow push. The conditional mismatch is
removed, but its factory still returns `Ref<InnerShadowEffect>` where the single-item `Array::push`
requires the array's declared `Ref<Effect>`.

The branch patch was rejected and its registry entry withdrawn; the source-patch manifest was restored
byte-for-byte and both dependency checkouts were restored by normal generation. No C++ header, source
patch, binding, emission repair, runtime conversion, or override is carried. No second cast/closed-union
experiment is needed to repeat the same missing owner relationship.

The owned headers are `flight/swf/_internal_index.hpp`, `flight/swf/contract.hpp`,
`flight/swf/enable_swf_guards.hpp`, and `flight/swf/swf_filter.hpp`. Their delivered **0/4 pass, 4 fail**
qualification and original conditional diagnostics remain unchanged from G1; all four rows now link
this request. The changed candidate's new diagnostic remains a failure, not a promotion. There was no
unchanged-stock rerun or global native sweep. Builder5's separate SWF Shape/read-only work was not
changed by this experiment.


## G2 SWF color-transform arrays: complete the existing declared heritage chain

At compiler `806d257` / Flight `a627849`, cluster `other-5611a858` first fails at
`flight/swf/swf_appearance.hpp:123`. `readSwfColorTransform` returns a single fresh
`ColorScaleBiasAdjustment` in `readonly Adjustment[] | null`. The emitter constructs
`Array<Ref<Adjustment>>{create_color_scale_bias_adjustment(...)}`, while the factory returns
`Ref<ColorScaleBiasAdjustment>`.

The existing ColorMatrixAdjustment repair already makes that type inherit Adjustment, and the
TintAdjustment repair uses that same matrix base. Only ColorScaleBiasAdjustment remained a flattened
sibling. Its pinned TypeScript declaration explicitly extends ColorMatrixAdjustment. The new repair
removes its duplicated `entity_runtime_key`, `kind` and `color_matrix` members and uses the already
repaired ColorMatrixAdjustment base, leaving only its own `color_scale_bias` member. The exact text
match is guarded by the repair registry, recorded in the generated manifest, and expires when the
emitter supplies that declared relationship. Reapplying the repair is a fixed point.

No function body, pointer cast, owner allocation or Adjustment union is added. The source Adjustment
contract stays open to vendor kinds. All inherited fields are the same cells on the original concrete
allocation, and standard shared-pointer widening retains its control block and lifetime. This extends
the existing bounded adjustment declaration repair; it does not choose a runtime-wide refinement or
owner-recovery design. The broader reverse-direction requests remain open.

The existing native adjustments suite now checks construction of an open base-reference array from
the real concrete factory, element order with a vendor Adjustment, shared control-block identity,
inherited field-cell addresses, writes through the matrix base, retained scale/bias data and release
when the last array reference is cleared. No source tint/alpha formulas or SWF array joining logic changed.
The native target built successfully and `flight_cpp.generated_adjustments` passed **1/1** with
these checks. The repair loader accepted the exact single-header replacement, reapplication changed
nothing, and the generated manifest records that one repair.

The exact four-header changed-input check completed at one worker: **1/4 pass, 3 fail,
0 unattempted**. `swf_appearance.hpp` passes. `swf_place_object3_handler.hpp` and
`swf_placement_tag_family.hpp` now reach the existing stock shadow-effect conditional mismatch at
`swf_filter.hpp:257`, joining the four already classified rows without recompiling them. The remaining
`swf_place_object_handler.hpp` reaches a retained call to the refused `readLegacyPlaceObject` at
line 106; that separate follow-on is recorded in the
[existing omitted-callee request](retained-declaration-calls-omitted-callee.md#g2-swf-legacy-placement-follow-on-after-adjustment-heritage).
Only the four owned header rows and affected cause counts changed. All three changed diagnostics stay
failures; there was no global sweep or claim that the refused SWF bodies became available.

## G2 GL bitmap text renderer create-data callback

At compiler `806d257` / Flight `a627849`, the retained aggregate in
`scene2d_gl/gl_bitmap_text.hpp` stores `flight::render::noop_renderer_data` in
`SpriteRenderer.create_data`. The actual native function accepts `(Ref<RenderState>, NodeAny)`;
the destination accepts `(Ref<RenderState>, Ref<Sprite>)`. The flattened Sprite owner does not
convert to the NodeAny owner, so those callable carriers are incompatible despite the source's
Sprite/Node relationship. Widening must preserve the same node owner, not copy fields or cast the
function pointer to another callable signature.

One scratch copy adding only the exact refused submit declaration exposed this conversion as the
first fatal error at `.create_data` (scratch line165), before `.submit` was checked. The program only
included the actual header and `main`; it did not reach a link step. The committed two headers still
fail at the missing submit name on line163. No callback adapter or runtime change was carried.
The separate [BitmapText runtime owner](readonly-callback-derived-owner-recovery.md#g2-gl-bitmap-text-needs-its-own-runtime-owner)
and [retained callback declaration](retained-declaration-calls-omitted-callee.md#g2-gl-bitmap-text-retains-a-refused-callback-in-an-aggregate)
remain open.

## G2 MD5 material handler needs the original concrete material owner

At compiler `806d257` / Flight `a627849`, `scene3d-formats/md5MaterialHandler.ts:18`
refuses the entire `md5MaterialHandler` binding as `cpp-reference-assertion-without-heritage`.
The diagnostic identifies `Ref<BlinnPhongMaterial>` to `Ref<Material>` at the source's
`createBlinnPhongMaterial(...) as unknown as Material`. The source declares
`BlinnPhongMaterial extends Material3D extends Material`, while all three emitted structs independently
inherit `ReferenceEnabled`. `MaterialLike` is emitted as `Ref<Material>` too, so merely dropping the
first assertion would leave the same owner crossing at `document.materials.push`.

The callback adds no resources or materials for a null mesh section or empty shader. Otherwise it
passes the original shader path, null base path and document resource array to `createExternalTextureRef`,
then creates one Blinn-Phong material, sets its authored name, appends the document's current material
index to that mesh section and appends that same material object to the document. The texture helper
reuses a matching external resource reference or appends a new reference to that same resource array;
it does not load the image. Each shader-bearing mesh invocation creates its own material.

Faithful emission must retain that material's concrete owner, full map/scalar fields, shared resource
references, name and append order through the declared base/material-like boundary. A base-prefix copy,
an unrelated native pointer cast, side storage, omitted material handler or an empty successful callback
does not implement this source. The separate
[retained-binding request](retained-declaration-calls-omitted-callee.md#g2-md5-material-family-retains-a-refused-binding)
records why declaring the omitted handler does not by itself make its header safe to link.

## G2 MD2 skin handler reaches the same material owner boundary

At compiler `806d257` / Flight `a627849`, `scene3d-formats/md2SkinHandler.ts:21`
independently refuses its whole `md2SkinHandler` binding as
`cpp-reference-assertion-without-heritage`. The actual diagnostic again identifies the factory's
`Ref<BlinnPhongMaterial>` asserted to `Ref<Material>`; `MaterialLike` storage also names
`Ref<Material>`. The source's `BlinnPhongMaterial extends Material3D extends Material` relation is
absent from those three independent emitted native owners. The common owner defect is measured at
this module, rather than inferred from the MD5 disposition above.

MD2's callback has a different contract. It reads up to `numSkins` fixed 64-byte records starting at
`offSkins`, checks each complete record against the original byte length before decoding its
NUL-terminated path with `String.fromCharCode`, reports a truncated record and stops at that point.
Empty names create no material; their count and first record index produce one post-loop diagnostic,
including after a truncation break. Each non-empty name creates one Blinn-Phong material with an
external texture reference using the original document resource array and null base path. The name
is retained as authored identity, the material's pre-append index is captured, and that same material
is appended to the document in record order. Only an empty `meshMaterials` array receives that index,
so alternate skins remain in the document and existing mesh bindings are preserved.

Faithful emission must keep all concrete material fields, object/resource/array identities, byte
bounds, diagnostic aggregation and append/binding order. Omitting alternate skins, substituting a
base-prefix material or empty callback, narrowing the record domain, or dropping the skin handler
from the full-support preset cannot satisfy that contract. The separate
[retained-binding request](retained-declaration-calls-omitted-callee.md#g2-md2-skin-family-retains-its-refused-binding)
records the actual declaration/link prerequisite for this module.

## G2 DragonBones skins need the original derived attachment

At compiler `806d257` / Flight `a627849`, `dragonBonesSkinsHandler.ts:97` declares
`parseDragonBonesDisplay(...): Attachment2D | null`. The exact refusal in
`skeleton2d_formats/dragon_bones_skins_handler.hpp` is
`cpp-contextual-union-interface-heritage-carrier-unrepresented`: its image branch returns the
original RegionAttachment2D through nullable Attachment2D. Source RegionAttachment2D and
MeshAttachment2D both extend Attachment2D, but all three emitted structs independently inherit only
ReferenceEnabled. Identical repeated entity/kind/name fields do not give these Ref owners a native
base relation. The reported first widening is RegionAttachment2D; MeshAttachment2D has the same
inspected flattened declaration, not an additional measured diagnostic in this task.

The actual display owner must survive into the default skin's positional nullable display table and
into each named wardrobe attachment. An unrelated base allocation, copied view, pointer cast, image-only
parser or retagged mesh would lose identity, concrete geometry/skin or the full skin domain. Establish
real declared heritage or an explicit identity-preserving carrier contract; do not substitute an empty
result or a new object containing only the base fields.

The original one-header check is **0/1 pass** at the earlier erased Any iteration373. The
[existing array-read request](erased-array-element-reading.md#g2-dragonbones-skins-emitted-display-iteration-and-refused-collection)
records that measured boundary, the independently refused collector/helpers and the complete
image/unweighted/weighted/alternate-skin/remap/recovery contract. No C++ declaration, body, heritage
repair or runtime/parser behavior is supplied or tested here.

## G2 GL QuadBatch create-data callback

At Flight `a627849` / compiler `806d257`, `gl_quad_batch.hpp` stores the existing
`noop_renderer_data` as `SpriteRenderer.create_data`. Both return
`optional<Ref<RendererData>>` and take `Ref<RenderState>`, but the actual function's source argument
is `NodeAny` (`shared_ptr<Node<Any>>`) while the callback slot takes `Ref<Sprite>`. The independent
native Sprite owner does not implicitly convert to that widest Node owner, so the callable is not
invocable with the slot's declared arguments. An exact-declaration-only scratch actual unused header
fails this conversion first at `.create_data` (candidate193); production original1 stays0/1 at the
earlier missing submit191, Clang22.1.8 jobs1. No native link boundary is reached.

Faithful declared heritage/owner-view or callback adaptation must preserve the same node and the
legitimate no-renderer-data result; casting a function pointer or fabricating a copied Node is not
that conversion. The source submit itself owns a complete QuadBatch, not a Sprite replacement.
Its GLstate-versus-generic-state submit seam is only inspected later and needs the existing
[typed callback/backend contract](readonly-callback-derived-owner-recovery.md).

The submit body independently refuses [nullable material-owner widening](compiler-interface-heritage-owner-widening.md#g2-gl-quadbatch-material-owner).
See [retained aggregate qualification](retained-declaration-calls-omitted-callee.md#g2-gl-quadbatch-retains-its-refused-submit-callback)
for the rejected declaration candidate. No callback adapter, new body or packet/drawing support is supplied.

## G2 GL Tilemap create-data callback

At Flight `a627849` / compiler `806d257`, `gl_tilemap.hpp` stores `noop_renderer_data` as
`SpriteRenderer.create_data`. Both return `optional<Ref<RendererData>>` and take `Ref<RenderState>`,
but the function's node argument is `NodeAny` (`shared_ptr<Node<Any>>`) and the callback slot takes
`Ref<Sprite>`. Those independent native owners do not implicitly convert, so the callable cannot
accept the slot's declared arguments. A separate exact-submit-declaration-only actual unused include
fails this conversion first at `.create_data` (candidate167). Stock original1 remains 0/1 at the
earlier missing submit165, Clang22.1.8 jobs1; no native link boundary is reached.

Faithful declared heritage/owner-view or callback adaptation must preserve the same node and the
legitimate no-renderer-data result. A function-pointer cast or copied Node does not provide that
conversion. The source submit needs the complete Tilemap/data, not a fabricated Sprite. Its later
GLstate-versus-generic-state submit seam is type inspection only and belongs with the existing
[typed callback/backend contract](readonly-callback-derived-owner-recovery.md).

The submit body independently refuses [nullable material-owner widening](compiler-interface-heritage-owner-widening.md#g2-gl-tilemap-material-owner).
See [retained aggregate qualification](retained-declaration-calls-omitted-callee.md#g2-gl-tilemap-retains-its-refused-submit-callback)
for the rejected declaration candidate. No callback adapter, body or packet/drawing support is supplied.
## G2 physics2d joint solvers narrow the base joint in their bodies

At Flight `a627849` / compiler `806d257`, the nine exported solver object literals in
`physics2d/joints.ts` are refused at `cpp-reference-assertion-without-heritage`. Each body receives a
`Ref<Physics2DJoint>` and asserts it to its concrete joint (for example `Ref<Physics2DJoint>` to
`Ref<Physics2DGearJoint>`) before writing impulses. The refusal names the missing relation exactly:
the declarations share no emitted C++ heritage path, and the source carrier does not retain a checked
dynamic owner that can recover the target. The requirement is the same one the factories above state:
either the concrete joint owners carry the declared heritage, or the runtime provides a checked
base-to-derived recovery. A pointer cast, a materialized copy or side storage would each break the
joint identity the solver mutates. Meanwhile the registration header declares the nine
`Ref<Physics2DJointSolver>` slots without a body
([retained callers](retained-declaration-calls-omitted-callee.md#g2-physics2d-registration-retains-nine-refused-solver-bindings)),
so no solver behavior is claimed.

## Site: callbacks over a derived render state in a base-typed hook slot (`@flighthq/scene2d-gl`)

`glClip.ts` builds `webglClipHooks: Scene2DClipHooks`, whose hooks are declared over `RenderState`, and
implements them over `GlRenderState`. That is legal TypeScript, because method parameters are bivariant,
and every caller hands the hooks a GL state. The emission:

```cpp
.finalize = [](flight::Ref<flight::types::GlRenderState> state) { ... }   // gl_clip.hpp:38, :43, :49
```

```
no viable conversion from '(lambda at gl_clip.hpp:38:166)' to 'std::function<void (flight::Ref<flight::types::RenderState>)>'
```

`GlRenderState` is emitted without heritage, so `Ref<GlRenderState>` and `Ref<RenderState>` are unrelated
types. That makes the base-typed slot unable to carry the derived hook at all. Heritage is necessary
here, and then the hook still needs the checked derived-owner recovery that
[readonly-callback-derived-owner-recovery](readonly-callback-derived-owner-recovery.md) asks for. The
stored callable takes the base and recovers the GL state it was registered for, and it must refuse
rather than reinterpret anything else. A copy of the state, or an unchecked cast, would be wrong.
`flight/scene2d_gl/gl_clip.hpp` (G2) fails first on this.

## G2 RenderProxy2D must widen to RenderProxy at the adapter call

`@flighthq/render` at flight `a627849`: `applyRenderProxyAdapter(state, source, data: RenderProxy2D)`
calls `updateRenderProxyRenderer(state, data)` (`renderProxyAdapter.ts:14`), whose emitted parameter is
`Ref<RenderProxy>` (`render_proxy.hpp:317`). `RenderProxy2D extends RenderProxy` in
`@flighthq/types`, and the 2D proxy narrows `material` to `Material2D | null` and `source` to `Node2D`.
The emitted structs are independent flattened owners, so the same object cannot enter the base typed
slot:

```
no viable conversion from 'Ref<flight::types::RenderProxy2D>' to 'Ref<flight::types::RenderProxy>'
  at flight/render/render_proxy_adapter.hpp:54
```

Six headers fail on that one call: `render/render_proxy_adapter.hpp` and `render/render_cache.hpp`
directly, and `scene2d_canvas/canvas_cache.hpp`, `scene2d_dom/dom_cache.hpp`,
`scene2d_dom/scene_2d_dom_pipeline.hpp`, `scene2d_gl/gl_cache.hpp` through their render dependencies.

No workaround is carried. A cast, a copied base owner or a wrapper adapter would split the existing
proxy's identity or narrow the domain; the faithful lowering is the declared heritage on the emitted
structs (`struct RenderProxy2D : RenderProxy`), which keeps the one object and its control block and
lets the base-typed slot receive it unchanged.

## G2 Scene2D node renderers: `noop_renderer_data` into `Scene2DRenderer.create_data`

This is the `Node2D` slot form of the create-data callbacks above. Cluster `no-viable-conversion-9c3803c5`
covers 9 headers at Flight `a627849` / compiler `806d257`.

Three renderers store the generic `flight::render::noop_renderer_data` in `Scene2DRenderer.create_data`:
- `canvas_scene_2d_renderer`, at `scene2d_canvas/canvas_node_2d.hpp:62`;
- the DOM renderer, at `scene2d_dom/dom_node_2d.hpp:53`;
- the WGPU renderer, at `scene2d_wgpu/wgpu_node_2d.hpp:120`.

The source is `noopRendererData(_state: RenderState, _source: NodeAny): RendererData | null`, and the slot
is `createData(state: RenderState, source: Node2D): RendererData | null`. TypeScript accepts the assignment
because a `Node2D` is a `Node`. In C++ the function takes `NodeAny` (`Ref<Node<Any>>`) and the slot's
`std::function` passes a flattened `Node2D`, which does not convert to it. So
`could not convert 'flight::render::noop_renderer_data' ...` (GCC 15.2, one worker; all three roots measured,
3 attempted, 0 pass, 3 fail).

The other six Canvas rows reach `canvas_node_2d.hpp:62` through their includes: `canvas_rich_text`,
`canvas_scale9_shape`, `canvas_scale9_sprite`, `canvas_shape`, `canvas_shape_rasterizer` and
`canvas_sprite`. Several of them also store `noop_renderer_data` later, at their own `create_data`. They
were not recompiled.

The same widening is needed: `Node2D` must reach `NodeAny` while keeping the same node, so the no-data
result stays legitimate. Nothing is carried:
- no adapter lambda that rebuilds a node;
- no function-pointer cast;
- no Canvas, DOM or WGPU provider;
- no narrowed node domain.

## G2 Canvas BitmapText and QuadBatch create-data callbacks

This is the Canvas form of the GL bitmap text and QuadBatch sections above. Cluster
`no-viable-conversion-eaf1a6e0` covers 5 headers at Flight `a627849` / compiler `806d257`.

Two Canvas renderers store `flight::render::noop_renderer_data` (`(Ref<RenderState>, NodeAny)`) in
`SpriteRenderer.create_data`, whose slot passes `Ref<Sprite>`:
- `canvas_bitmap_text_renderer`, at `scene2d_canvas/canvas_bitmap_text.hpp:116`;
- the QuadBatch renderer, at `scene2d_canvas/canvas_quad_batch.hpp:159`.

The flattened `Sprite` owner does not convert to `NodeAny`, so the result is
`could not convert 'flight::render::noop_renderer_data' ...` (GCC 15.2, one worker; both roots measured,
2 attempted, 0 pass, 2 fail). `scene_2d_canvas_pipeline`, `contract` and `_internal_index` reach both roots
through their includes and were not recompiled.

The same `Sprite` to `NodeAny` same-owner widening is required, keeping the legitimate no-data result. Nothing
is carried: no adapter that rebuilds the node, no function-pointer cast, no Canvas text or QuadBatch provider,
and no narrowed domain.

## Site: the GL render-state guards (`@flighthq/render-gl` `enableGlRenderStateGuards.ts`)

Both directions of the missing `GlRenderState : RenderState` heritage meet in one module:

- **Widening.** `areGlRenderStateGuardsEnabled` and `enableGlRenderStateGuards` call
  `getRenderStateRuntime(state)` with their `GlRenderState`. The emitted call
  `flight::render::get_render_state_runtime(state)` fails with `no viable conversion from
  'Ref<GlRenderState>' to 'Ref<RenderState>'` (`enable_gl_render_state_guards.hpp:79`), which is G2
  cluster `no-viable-conversion-b3064463`. The other two of its three headers (`render_gl/_internal_index.hpp`
  and `render_gl/contract.hpp`) fail by including it.
- **Checked recovery.** The guard installed into the base-typed `registries.renderRootGuard` slot,
  `warnOnSecondRenderRoot(state: RenderState, root: NodeAny)`, asserts `state as GlRenderState` to key its
  first-root map. It is refused as `cpp-reference-assertion-without-heritage`.

Substituting `get_gl_render_state_runtime` at the call would pick an owner the source does not name. A
pointer cast, a copied base owner or an always-false guard would each misstate the guard, so nothing is
patched or overridden. With real heritage the call widens implicitly, and the guard's assertion becomes
the checked derived-owner recovery described above. The comparison `guard === warnOnSecondRenderRoot` then
also needs the callable identity of
[local-handler-callable-identity](local-handler-callable-identity.md).

## G2 GL box-blur pass destination and source recipe

At Flight `a627849` / compiler `806d257`, effects-gl/glEffectBoxBlur.ts110 applyBoxBlurPass and
source98 applyBlurBlit both refuse cpp-contextual-union-value-type-unrepresented [compiler-restriction].
Each calls drawGlFullscreenPass with its original dest:GlTextureRenderTarget, whose emitted value is
Ref<types::GlTextureRenderTarget>, where the declared destination is Readonly<GlRenderTarget>|null.
The refusal says this source resembles an alternative but is not the declaration that alternative
names. This is the same concrete-target widening already filed for the GL effect-pass family, now
with its two exact box-blur sites. The cache's WeakMap refusal is separate, not the apply-pass cause.

GlTextureRenderTarget extends GlRenderTarget in source; both emitted native structs independently
inherit ReferenceEnabled. The actual native fullscreen-pass surface takes an optional readonly
GlRenderTarget row and already offers proven original-owner projection. The compiler must construct
that declared view/absence carrier from the original target rather than attempt an unrelated nominal
Ref conversion. Existing runtime row proof does not make this refused TypeScript body emit. A copied
GlRenderTarget, fabricated framebuffer, unchecked pointer cast or reduced attachment/format/depth/MSAA
family would lose owner/cells or change the declared domain. Preserve the same target/context,
framebuffer and dimensions through pass binding/tracking. No actual blur draw is tested here.

The source's full recipe must survive any compiler fix:

- passes is Math.max(1,Math.round(options.passes??1)), blurX/blurY nullishly default to4 and remain
  target Gaussian standard deviations. Preserve explicit zero, source numeric behavior (including
  non-finite inputs), optional edgeColor and computeBoxBlurPassRadius's per-pass odd-width recipe;
  do not substitute a fixed radius, add validation/clamping, or narrow the number domain.
- Fetch the shader before the loop even if no radius later causes a pass. Keep read=source/write=temp
  and, for each pass, X then Y. Draw only for radius>0, then assign read=write and toggle write by
  original temp/dest identity. Distinct caller scratch is the source's documented contract; do not
  allocate replacement targets, invent alias checks or silently change the two targets' roles.
  If read!=dest after the loop, perform the real radius-zero blit, including when every axis was
  skipped. If read==dest, skip only that tail draw, retaining earlier shader lookup/errors.
- A blur draw samples source.texture and writes the same dest via drawGlFullscreenPass. Upload
  texelSize=(1/source.width,1/source.height), radius, direction, then edge uniforms. Undefined edgeColor
  sets RGBA0/use0; an explicit four-number tuple, including transparent zero, uploads those exact
  cells/use1. Then set blendFunc(ONE,ZERO) for replacement rather than compositing. The tail blit
  obtains the shader and sets texelSize0,radius0,direction0,RGBA0/use0, then the same blend factors.
- Preserve the exact GLSL: r=max(0,int(u_radius)); r0 samples once, otherwise sum offsets -r through+r
  and divide by2*r+1. Edge color applies only when useEdgeColor>0.5 and UV lies strictly outside[0,1];
  all other samples use the bound texture. No new premultiplication, altered sampling or constant
  blur result is equivalent. Fullscreen program/target/texture/state operations and their source
  ordering/exception effects remain required; do not invent a stronger finally/restore guarantee.

The immediate actual owned-header result is **0/1 pass**,57.1s, at missing cache115, not a runtime
blur failure or a native diagnostic at either draw. Its [bound-interface cache and shader-owner request](bound-interface-weak-map-key-policy.md#g2-dedicated-gl-box-blur-cache-and-shader-owner)
records that separate first blocker and the dropped base spread; the
[retained-callee request](retained-declaration-calls-omitted-callee.md#g2-gl-box-blur-refuses-a-cache-before-its-omitted-pass-calls)
records why no declaration-only candidate qualifies. No body, target, shader, cache, normal generation,
source patch or native behavior pass is supplied by this docs/products-only disposition.
