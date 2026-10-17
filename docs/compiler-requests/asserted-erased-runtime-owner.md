# An assertion from the erased runtime slot to a module-local runtime reads members it does not declare

A module recovers its runtime from the entity's erased slot and asserts it to a local runtime interface.
The emitter refuses because the target declares members the source type does not, and it will not
reinterpret a structural owner into a richer one.

Measured at `2b687ea`, flight `a627849`.

## Observed

```
cpp emission failed for @flighthq/app/packages/app/src/appRenderView.ts: the asserted row from
flight::Ref<flight::types::EntityRuntime> to flight::Ref<AppRenderViewRuntime<flight::Ref<flight::types::RenderState>,
flight::Ref<flight::types::RenderTargetDimensions>>> reads attached, resize and synchronize, which the source
type does not declare, and a structural owner binds the members and storage representations of the type the
object was first reached as …
```

## Second measured site: `@flighthq/dialog` `fileDialog.ts`

The same pair of refusals, from the P-PL3 increment, on a module that is supplied by hand today:

```
cpp emission failed for @flighthq/dialog/packages/dialog/src/fileDialog.ts: the asserted row from
flight::Ref<flight::types::EntityRuntime> to flight::Ref<flight::types::FileDialogHandleRuntime> reads
operations, which the source type does not declare …
cpp emission failed for @flighthq/dialog/packages/dialog/src/fileDialog.ts: a reference assertion from the
createEntityRuntime factory result flight::Ref<flight::types::EntityRuntime> to
flight::Ref<FileDialogHandleRuntime> has no heritage to cast along …
```

`getFileDialogHandleOperations` (`cpp-structural-assertion-owner-unproven`, source line 32) and
`initializeFileDialogHandle` (`cpp-reference-assertion-without-heritage`, source line 41) — the same two
declarations the app-render-view module loses, for the same reason, in a module that keeps a *nullable*
authority rather than a whole runtime. `@flighthq/dialog` is `assisted` because
`overrides/include/flight/dialog/file_dialog.hpp` supplies both and the package's five headers then
compile; without that supply it is the same broken module this request describes. At this pin the module's
*third* refusal (a dual-sentinel optional property result,
[`compiler-present-dual-sentinel-multivariant.md`](compiler-present-dual-sentinel-multivariant.md)) no
longer fires, so the emitter now emits `createFileDialogHandle` and the three `show*` functions itself and
refuses only the two declarations above.

## Why the module does it

`appRenderView.ts` keeps a per-view runtime — `attached`, `resize`, `synchronize` — beside the view entity,
reached through the entity's runtime slot. The slot is declared as `EntityRuntime`, the local runtime is
`AppRenderViewRuntime`, and the recovery is an assertion from the former to the latter. This is the same
shape the tray lane carried with a source patch that narrows the entity's slot to the local runtime
(`tray-create-result-and-runtime-owner.patch`): the slot's declared type, not the assertion, is what makes
the recovery expressible.

## Expected behavior

Either the package's own declaration narrows the slot the way tray's now does — in which case this is a
source-side change the owning lane makes deliberately, and the request is the record that it is owed — or
the emitter represents a checked recovery from the erased slot instead of an unchecked reinterpretation.
The second is a runtime capability, and it is the one that would serve every package holding a local
runtime behind the erased slot; today each one either narrows its declaration or refuses.

## Affected modules

`@flighthq/app` — `appRenderView.ts`, one of its two refused modules, supplied by the
`app-render-view-unqualified-types-and-member-access` override.

### Third measured site: `@flighthq/gizmo`

At flight `a627849`, `packages/gizmo/src/gizmoState.ts` refuses `getGizmoRuntime` for the same
reason. `GizmoState<NodeType>` retains only `EntityRuntime`, while the asserted
`GizmoRuntime<NodeType>` adds the complete transform, handle, selection, camera, snapping, and
viewport state. The refusal is honest: the flattened C++ owners have no checked recovery relation.
The package override therefore keeps the exact `GizmoRuntime<NodeType>` owner in a weak,
state-identity-keyed table instead of casting the erased slot or materializing a replacement.

### Fourth measured site: `@flighthq/selection`

At flight `a627849`, each Selection constructor stores its exact runtime behind `EntityRuntimeKey`, and
each matching accessor asserts that erased slot back to the concrete owner. After the existing
`selection-runtime-type-evidence` patch supplies the object-literal types, current generation records
`cpp-structural-assertion-owner-unproven` for:

- `getLassoSelectionRuntime`: `EntityRuntime` to `LassoSelectionRuntime`;
- `getMarqueeSelectionRuntime`: `EntityRuntime` to `MarqueeSelectionRuntime`;
- `getSelectionStateRuntime<NodeType>`: `EntityRuntime` to `SelectionStateRuntime<NodeType>`.

The accessors are omitted rather than reinterpreted. Consequently the package-only compile attempts all
six current-pin headers but lasso, marquee, contract, and index fail on undeclared runtime accessors.
`selection_state.hpp` compiles only while its dependent templates are uninstantiated: its constructor and
runtime accessor are both absent, so this is not a usable partial state surface. A side table would become
a second owner registry and cannot be introduced as a compiler repair; Selection carries no workaround.

## Carrying workaround

The app override above and the gizmo state override supply their modules by hand at this pin.

## Third measured site: `@flighthq/selection`

At Flight `a627849` / flight-compiler `2b687ea`, all three selection runtime accessors are refused after
reading the entity's declared `Ref<EntityRuntime>` slot:

- `getLassoSelectionRuntime` asks for `active` and `path`;
- `getMarqueeSelectionRuntime` asks for `active`, `rectangle`, `startX`, and `startY`;
- `getSelectionStateRuntime<NodeType>` asks for `activeNode`, `selectedNodeSet`, `selectedNodes`, and
  `signals`.

The source has stored the corresponding derived runtime in that slot immediately before every recovery,
but the flattened C++ declarations have no heritage by which to prove it. The selection completion uses
the declared interface heritage, stores the same derived object in the base slot, and recovers it with
`std::static_pointer_cast`; there is no copied or parallel runtime identity.

## Fourth measured site: `@flighthq/gizmo`

At the same Flight/compiler pins, `getGizmoRuntime<NodeType>` is refused under
`cpp-structural-assertion-owner-unproven` at `gizmoState.ts:492`. The source stores a
`GizmoRuntime<NodeType>` in `GizmoState<NodeType>`'s inherited `EntityRuntime` slot during
construction and later recovers it through an assertion. The asserted owner adds the complete gizmo
state: bounds and scratch geometry, camera and selection references, handle/outline nodes, signals,
viewport dimensions, snap and pivot settings, active drag state, and lifecycle cleanup storage. The emitter flattens
`GizmoRuntime extends EntityRuntime`, so the generated owners have neither declared heritage nor a
checked recovery path and the accessor is omitted. `createGizmoState` and `disposeGizmoState` are
then refused as callers of that omitted declaration, while every retained public state operation
also calls it.

The native completion must keep one derived runtime allocation, store it through the declared base
slot, and recover that same identity along real C++ heritage. It must not copy the runtime or maintain
parallel state beside the entity.

## G2 site: `@flighthq/scene3d-wgpu` scene runtime

At compiler `806d257` / Flight `a627849`, `wgpuScene3DRuntime.ts:13` refuses
`getWgpuScene3DRuntime` with `cpp-structural-assertion-owner-unproven`. Its first read is:

```ts
const stateRuntime = state[EntityRuntimeKey] as WgpuRenderStateRuntime;
```

`WgpuRenderState` inherits the `RenderStateRuntime` slot from `RenderState`. The generated slot is
`std::optional<Ref<RenderStateRuntime>>`; the assertion asks for `Ref<WgpuRenderStateRuntime>`, whose
`context`, backend state cells and differently represented `registries` are not supplied by that base
owner. The generated structs each inherit `ReferenceEnabled`, with no declared heritage between them.
The source factory also asserts `createRenderStateRuntime()` to the derived runtime before retaining
it in the base slot. Changing only this accessor's result cannot recover the original concrete owner
through that construction and storage path.

This is not a refused WeakMap value or evidence of a missing GPU provider. The module already emits
`scene_runtimes` as `WeakMap<Ref<WgpuRenderState>, Ref<WgpuScene3DRuntime>>`. Source lookup reuses the
scene for the same state; first use reads the skinning registry and reuses or installs the device-tier
mesh-upload map before storing the scene. Any eventual body must preserve both identities, the mutable
scene caches and the nullable skinning result. A copied runtime, reinterpreted base owner, second owner
table or fresh upload map per derived state would not implement that behavior.

The existing declaration-only repair now lets the retained `getWgpuSkinningAdapter` caller name this
accessor, using the already emitted state and scene carriers. It supplies no body or GPU operations:
calling the retained accessor compiles but fails to link on `get_wgpu_scene_3d_runtime`. The
[retained-callee request](retained-declaration-calls-omitted-callee.md#g2-site-scene3d-wgpu-scene-runtime)
records that separate emission consequence. The ownership refusal stays open; no backend completion
or broader owner redesign is carried by this repair.

## Related

[`entity-intersection-arm-sibling-carriers.md`](entity-intersection-arm-sibling-carriers.md) — the tray
lane's version of the same family, carried by the `tray-create-result-and-runtime-owner` source patch.
