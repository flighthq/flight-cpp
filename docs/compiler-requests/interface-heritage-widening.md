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

Emission repairs `adjustments-color-matrix-interface-heritage` and `adjustments-tint-interface-heritage`
respell the two structs with their declared heritage, removing only the duplicated member declarations.
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
