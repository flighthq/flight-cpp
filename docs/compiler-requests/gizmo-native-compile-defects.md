# `@flighthq/gizmo` native-compile defect inventory

Measured at Flight `a62784923f3be814463286c9fb28edfe6ab15789` and flight-compiler
`2b687eaf6b631e0fa4d7d6e18dad93b845f99633` after package-scoped gizmo generation. The emitter
produces four of five modules and refuses `gizmoState`; independent two-job compilation passes two
of five headers before dependency isolation.

## Refused runtime recovery

`getGizmoRuntime<NodeType>` is refused under `cpp-structural-assertion-owner-unproven`; see
[asserted-erased-runtime-owner](asserted-erased-runtime-owner.md). Its two direct callers at the
construction/lifecycle boundary, `createGizmoState` and `disposeGizmoState`, are omitted as dependent
refusals. The partial header retains the other public state operations even though they call the
missing accessor.

## Baseline dependency boundary

The committed inventory still has `@flighthq/interaction` at the older row. All three state-bearing
gizmo headers stop first at `flight/interaction/interaction_manager.hpp:192`: the current types row
represents `cursorBackend?: CursorBackend | null` as a three-arm `std::variant`, while the older
interaction emission calls `.value_or(nullptr)` as if it were `std::optional`. That row also omits
`enableInteractionSignals` and contains independent current-types mismatches. It is not a gizmo
completion surface and must not be repaired by the gizmo override.

A diagnostic-only include root supplied the narrow interaction signal boundary and no-op shape paint
commands so compilation could continue into the gizmo emission. Nothing from that root is shipped.

## Emitted C++ defects after dependency isolation

1. `createGizmoHandles` lowers `addNodeChild(handleRoot, handles[i].node)` through
   `NodeOf<Traits>` with no `Traits` declaration in scope (`gizmo_state.hpp:915`). The source has two
   concrete node owners and needs no free type parameter.
2. `setGizmoNodeTransform` calls `invalidateNodeLocalTransform` with the generated merged `Node2D`
   structural view. `revision.hpp` attempts native reference recovery from that unprojected merged row
   and trips the runtime assertion that a merged row must be projected first (`gizmo_state.hpp:440`).
3. The emitted hit-area calls pass the exact `Ref<Shape>` owner to the current interaction override,
   whose generic setters cover cursor and hit-test state but whose hit-area overloads accept only
   `NodeAny` and the merged `Node2D` view. The TypeScript source accepts `Shape` through the declared
   Node hierarchy; the C++ boundary has lost that structural acceptance (`gizmo_state.hpp:706` and
   the repeated handle sites).
4. Instantiating the state templates shows that projection calls reconstruct writable
   `StructuralRef<RowWritable<RowOf<Ref<Camera2D>>>>` from the runtime's declared readonly camera
   view, and likewise reconstruct a writable `GizmoNode2DFeatures<NodeType>` row, before casting each
   back to readonly. The structural runtime correctly refuses those capability escalations. The
   emitted calls should pass the existing readonly views directly (the features site is
   `gizmo_state.hpp:181`; the first camera site is `gizmo_state.hpp:574` and the outline sites repeat
   it).
5. `startGizmoTransform` retains the active node as `std::optional<NodeType>` and the local-space
   branch passes that optional itself to `get_world_rotation`, whose generated callback requires a
   readonly row over `NodeType`. The source narrows `active !== null`, so the emitted branch must pass
   `active.value()` (`gizmo_state.hpp:513`).
6. `connectGizmoHandle` stores three capturing lambdas directly in `std::function` and later asks
   `disconnectSignal` to compare copies by JavaScript function identity. The runtime deliberately
   rejects that guess with `callable identity is unavailable after std::function erasure`, so
   disposal cannot remove the five registered listeners. The emitter must retain each closure in an
   identity-bearing `flight::Function` target before it crosses the generated `std::function` ABI
   (`gizmo_state.hpp:646-675`).

## Acceptance

- Preserve one `GizmoRuntime<NodeType>` allocation and recover that identity through declared
  `EntityRuntime` heritage; do not copy or mirror the runtime.
- Preserve exact handle names and insertion order, signal identity, reverse-order listener cleanup,
  idempotent disposal, pointer-identity bracketing, and mode cancellation.
- Preserve screen/world projection, local-axis rotation, pivot choice, snapping, negative/degenerate
  scale behavior, and degree-only rotation output.
- Keep interaction and shape dependency repairs out of the gizmo override; a local compatibility
  call may use their already-generated runtime cells but must not shadow either dependency module.
