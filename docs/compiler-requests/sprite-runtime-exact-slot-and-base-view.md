# Sprite runtime: one exact slot and common views

The source-derived Sprite slot should retain `SpriteRuntime | undefined`. Generated B already does this. The generic `NodeRuntime<Node2DTraits>` slot belongs to the older native Sprite override, preserved through Builder2's75a snapshot. It is not a constraint imposed by B. Builder2 remains the sole Types writer; this proposal makes no Types, runtime, factory or SDK change.

The [design receipt](../../artifacts/sprite-runtime-carrier-abi-design.json) records Builder4's immutable39-case source proof and136 refusal archive, the supplied native/B layout snapshots, and two native component probes. The installed-slot counterexample passes20 checks; the exact-slot/common-view probe passes25. These are API and owner observations, not execution of the original Sprite factory or bounds handlers.

The original runtime extends Node2DRuntime in TypeScript, but its C++ record is flat. It cannot be stored as a generic runtime pointer or recovered from an object allocated as that generic runtime. Its recursive bounds callback receivers also differ from Node2DRuntime's receivers. Pasting a C++ base or copying fields would make an additional claim about storage and callable compatibility that the source proof does not establish.

The counterexample writes a flat SpriteRuntime through the legacy symbol interface. The physical generic slot still holds the old runtime while a separate typed attachment answers with the new runtime. A control using the real NodeRuntime-to-EntityRuntime inheritance edge shows the same split: ordinary C++ pointer convertibility does not make a typed computed-cell write reach that cell. That is a counterexample to this workaround, not a proposed implementation.

The smallest measured mapping uses the existing families:

1. Re-derive the Sprite override from the exact generated declaration in a later Builder2 coherent pass. Store the optional exact runtime in the one physical symbol slot. Read and write it at that physical type; no dynamic replacement value accompanies it.
2. Build an existing StructuralRef over that exact runtime. For generic runtime operations, convert it to the existing writable or readonly row over NodeRuntime<Node2DTraits>. The generated member table already proves this common view. It retains the exact allocation, RowOwner, live parent slot and callable cell. Use row reads and writes through the view.
3. Keep bounds callables in their owner-declared types. At each invocation, construct that callable's readonly receiver view from the original source row. Both measured receiver types accept the same Sprite owner without changing stored function types. Four fixture invocations preserve that owner. No callback wrapper, different-name native alias or copied state is required by this mechanism.

The common view has no generic native subobject: `shared_object()` is empty. Dereferencing its pointer-style interface would therefore be wrong. A pointer-returning `native_node_2d_runtime` cannot keep its present result type and return a flat exact Sprite runtime. This is the explicit local compatibility decision for Foreman and Builder2: introduce a distinct common-view accessor and migrate its callers, keeping the old construction pointer alias separate. Retain or retire the old pointer-only accessor deliberately; if retained, a flat exact-owner boundary remains explicitly partial. Do not hide this in an alias replacement or change a global object ABI.

The bounded direct-accessor search finds29 references in10 files, including two tests. The required migration is concrete:

| Consumer | Required handling |
| --- | --- |
| `include/flight/node/native_node2d_access.hpp` | Read each concrete owner's physical optional runtime slot before projecting common fields; use row writes for invalidations. An unknown fallback must preserve its actual storage/absence boundary. |
| `overrides/include/flight/node/hierarchy.hpp` | Preserve live parent/children cells, canAddChild callable, signal ownership and revision writes. Replace generic-pointer field operations with common-view operations. |
| `overrides/include/flight/node/node_transform2d.hpp` | Read and write the original matrix/rotation cells through a view; do not copy a transform runtime. |
| `overrides/include/flight/render/render_proxy.hpp` | Preserve traversal arrays, parent ownership and local-content/appearance revisions. |
| `overrides/include/flight/scene2d_gl/gl_node_2d.hpp` and `gl_mesh_shape_renderer.hpp` | Read original children/content cells. The current probe does not qualify rendering. |
| `overrides/include/flight/scene2d/display_object.hpp` | Separate runtime creation carriers from the common runtime read accessor. Changing a return alias mechanically is insufficient. |
| `overrides/include/flight/text/text_label.hpp` | Its use of NativeNode2DRuntimeOwner::element_type and existing runtime-base assignment need separate source-faithful review. A view cannot stand in for a base to copy. |
| SVG appearance and GL owner tests | Assert exact owner identity and live cells through the selected accessors, while retaining their original behavior/framebuffer obligations. |

The two bounds operations also appear in generated node bounds/trait/stage-fit code and the quadbatch, tilemap, selection and text-label overrides. The receipt retains exact locations. Preserve the callable signature at each owner and adapt the receiver view at the call site; do not force every callable into one std::function type. Schema-only receiver constructibility proves owner forwarding here, not that every possible member read through that schema is represented. The original bounds computation, validity checks and actual source callback bodies still require their own native consumer qualification.

The exact allocation must exist before it reaches this slot. Builder4's type-only experiment is useful evidence, but its dependent node constructors and renderer recoveries remain refused, and its strict diagnostics are not a pass. Neither StructuralRef nor recover_owner can turn a previously allocated generic Node into a fresh Sprite. The original image consumer remains unrun. This proposal neither requires a new compiler defect nor authorizes an SDK run, a duplicate Types worker, copied storage or a new runtime family.

The first common-view probe used row_has for JavaScript own presence and failed that one expectation. The runtime explicitly distinguishes value engagement from row_has_own_property; correcting that probe call, retaining the failed receipt and adding callback-owner controls yields25 passing checks. No runtime presence fix was made.
