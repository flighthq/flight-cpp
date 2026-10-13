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

## Carrying workaround

The override above supplies the module by hand at this pin.

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
`cpp-structural-assertion-owner-unproven` at `gizmoState.ts:481`. The source stores a
`GizmoRuntime<NodeType>` in `GizmoState<NodeType>`'s inherited `EntityRuntime` slot during
construction and later recovers it through an assertion. The asserted owner adds the complete gizmo
state: bounds and scratch geometry, camera and selection references, handle/outline nodes, signals,
snap and pivot settings, active drag state, and lifecycle cleanup storage. The emitter flattens
`GizmoRuntime extends EntityRuntime`, so the generated owners have neither declared heritage nor a
checked recovery path and the accessor is omitted. Every public state operation retained in the
partial header calls that missing accessor.

The native completion must keep one derived runtime allocation, store it through the declared base
slot, and recover that same identity along real C++ heritage. It must not copy the runtime or maintain
parallel state beside the entity.

## Related

[`entity-intersection-arm-sibling-carriers.md`](entity-intersection-arm-sibling-carriers.md) — the tray
lane's version of the same family, carried by the `tray-create-result-and-runtime-owner` source patch.
