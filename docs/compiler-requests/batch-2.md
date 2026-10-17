# Compiler request batch 2 — draft (2026-10-17)

**Ready for Foreman review; not sent to Principal or flight-compiler.** Integration accepted the
author's qualified five-header resource confirmation; its one PASS/four FAIL split and first causes
are unchanged, so the allocations below are retained. The recorded snapshot's **731 failed rows**
have primary request coverage across 177 active clusters; primary and related links reach **103
existing request files**, with exact membership counts and no broken paths. The resource-specific
hold is resolved. Foreman must review this draft before release. This draft adds no implementation
or compiler-fix commitment.

Against compiler `806d257` / Flight `a627849`. Counts below use the scoped G2 worklist at
flight-cpp `ed7d6bf43a70fe82d2b10db77d9e966dbb3e536e`: [header inventory](../../generated/header-status.json)
and [cause/disposition worklist](../header-failures.md). There are **3,153 rows: 2,422 PASS / 731 FAIL,
177 active clusters**. This is accumulated scoped evidence, not a fresh full sweep. The original
G1 sweep remains 2,339 PASS / 813 FAIL of 3,152; its historical totals are unchanged.

## Scope and counting

[Batch 1](batch-1.md) already carries seven ranked items, linking eight request files, with about
234 affected headers at its historical snapshot. Its current first-cause footprint is **269 failed
headers**: descriptors 133, local re-export order 40, membership 32, JSON-parse casts 25, optional
Record literals 17, tuple pairs 13 and numeric predicate callbacks 9. These requests are excluded
from the batch-two table; do not add the old 234 estimate to today's counts.

Each remaining failed header is assigned once, using its own primary request, falling back to its
cluster's primary request. This preserves intentional row-specific primaries in mixed clusters.
The table groups **456 failed headers across 63 primary compiler-request files**: 455 are routed to
the compiler in the inventory; the RichText retained-callee row still has a `runtime` route but joins
its actual filed compiler-request family here. That explicit one-row inclusion changes no inventory
route. The six separate binding/runtime rows below remain outside this bundle. Multiple linked
requests in a row retain their distinct triggers; their combined
count is a union of assigned headers, not an estimate of headers one fix will unblock. Related
requests and later diagnostics in the dossiers add no second header count. Includers are included
in these counts; they are not additional independent reproductions.

The complete failed-row partition is **269 batch one + 456 batch two +
6 binding/runtime rows = 731**. The six separately routed rows are
the [DOM shared-style boundary](dom-style-shared-element-boundary.md) (5) and the
[Scene3D present nullable-cell projection](readonly-structural-member-carrier-widening.md) (1).
The 20 PASS rows carried through prior-emission overrides are separate from this failed-row partition.

The qualified owner recheck ran only the five former resource records serially at `-j1` on
`3987edfe`: **one PASS, four ordinary failures, zero reproduced crashes**, with the same four first
diagnostics. Three failures join the readonly-program cause and one joins the union-array cause
below. The author confirmed measured dependency equivalence through `542b0205`; Integration accepted
the confirmation as `ed7d6bf4`, superseding the withdrawn initial evidence. No resource acceptance
remains pending, and no crash request is invented. The original G1 sweep and serial-retry history are
preserved.

## Remaining compiler requests

The linked files retain the source, actual carriers, measured or historical limits, rejected
candidates, identity/absence requirements and current workaround. Counts describe affected headers
at this snapshot; clearing a first diagnostic can expose another refusal, owner boundary or absent
provider. No replacement bodies, permissive owner casts or narrowed source domains are proposed.

| Failed headers | Existing requests | Recorded cause / required boundary |
| ---: | --- | --- |
| 96 | [Retained omitted callee](retained-declaration-calls-omitted-callee.md) (54); [refused runner/registration](custom-shader-effect-runner-owner.md) (42) | Retained code names refused functions or bindings. Keep declaration dependencies coherent; declarations alone do not supply bodies. Runner variables and eager initializers have separate qualification limits. |
| 50 | [Declared heritage](interface-heritage-widening.md) (42); [nullable heritage owner](compiler-interface-heritage-owner-widening.md) (8) | Flattened independent owners cannot perform source-declared base widening, including callback and nullable storage boundaries. Preserve the original owner. |
| 36 | [Generic argument substitution](generic-call-argument-uses-unsubstituted-parameter-type.md) | Caller conversions/literals name the callee's unsubstituted `Traits` or `K`; use the inferred type argument. |
| 30 | [Contextual owned literal](contextual-object-literal-owner-materialization.md) | A source-selected SDK factory overload needs its owned options carrier, rather than bare designators. This differs from batch one's bound WebGPU descriptors. |
| 26 | [Erased array reads](erased-array-element-reading.md) | `Array.isArray` guards leave iteration, indexed reads or length on `Any`; recover the original array and its element/length evidence. |
| 19 | [Attachment slot lift](nullable-array-element-into-dual-sentinel-slot.md) (18); [nullish field storage](nullish-union-into-optional-field.md) (1) | Adapt the actual nullable/sentinel value to the destination slot while preserving distinct null/undefined states where declared. |
| 16 | [Guarded readonly helper](guarded-readonly-optional-helper-call.md) (14); [readonly/writable round trip](readonly-view-to-writable.md) (2) | Pass the retained readonly program/source view directly to the readonly helper; do not manufacture write authority. |
| 15 | [Heritage argument deduction](heritage-argument-generic-deduction.md) | Infer concrete node traits through declared interface heritage, rather than leaking `Traits` or selecting the default constraint. |
| 10 | [Class implementing interface](class-instance-as-implemented-interface.md) | SwfReader and its structural interface need one live reader, cursor and method owner. |
| 10 | [Reference-backed variant reads](reference-backed-outcome-variant-narrowing.md) (5); [cube-face reads](cube-texture-width-and-completeness.md) (4); [nominal/row member access](collapsed-union-record-member-access.md) (1) | Select the active referenced alternative and use its actual member carrier; do not read properties on an optional/variant or invent a separate row. |
| 9 | [Callable identity](local-handler-callable-identity.md) | Registry comparisons and signal cleanup must retain the same function object's identity through callable storage. |
| 9 | [Bound weak-key policy](bound-interface-weak-map-key-policy.md) | GlContext interface uses do not reach the bound native weak-key policy; omitted caches precede additional refused passes. |
| 8 | [Node runtime-owner parameter](node-transform-runtime-owner-argument.md) | Native transform helpers must accept the original node/runtime rows and inferred traits, rather than unrelated nominal runtime-owner references. |
| 8 | [Construction view at concrete call](entity-construction-view-passed-to-owner-parameter.md) | Joint initializers receive `EntityConstruction` where the non-generic signature expects its finished owner. |
| 8 | [Optional assertion](optional-assertion-cast-must-unwrap.md) (4); [indexed/member flow](optional-index-member-present-flow.md) (2); [guarded helper argument](narrowed-optional-template-argument.md) (1); [nullable element payload](nullable-array-element-null-test.md) (1) | Project the correct present payload at the call/cast boundary, retaining the source's absence and evaluation behavior. |
| 7 | [Named view versus Record](named-view-record-ternary.md) | The DragonBones ternary and Spine callback join an aliasing `NamedProperties` view with owning Record storage; copying would lose the source alias. |
| 7 | [Imported narrowed-union widening](narrowed-imported-union-into-declared-parameter.md) | Physics3D collider guards prove a built-in subset before convex calls; imported arm qualification/order must preserve the surviving owners. Retained callers still name omitted bodies. |
| 7 | [Imported Record alias](imported-record-alias-loses-runtime-representation.md) (6); [imported alias local flow](compiler-imported-alias-local-flow-abi.md) (1) | Retain the defining module's represented alias across import classification and inferred local storage. |
| 6 | [JSON value assertions](json-value-erased-assertion.md) (4); [dynamic JSON shape reads](parsed-json-object-shape-reads.md) (2) | Use the actual JsonValue/dynamic-object carrier and checked same-object projection; `Any::object_if`, bare `Any` returns and owning Record casts are not interchangeable. Batch one's JSON.parse static-cast rows stay excluded. |
| 6 | [Structural reference argument](structural-subtype-reference-argument-owner.md) (5); [writable wider out owner](writable-structural-out-of-a-wider-owner.md) (1) | Preserve writes to the caller's original SwfShapeState/Aabb through the callee's smaller structural parameter. |
| 6 | [Node call with absent trait argument](node-call-trait-absent-from-scope.md) | Command NodeAny child-count calls cannot deduce through their merged-row parameter; retain the source's inferred trait argument. |
| 5 | [Flattened refinement](flattened-interface-refinement-identity.md) (3); [derived render-state assertion](canvas-render-state-owner-asserted-without-heritage.md) (2) | Recover a proven capability of the original graph/render-state owner; a copied or unchecked derived owner is insufficient. |
| 5 | [Outcome literal construction](object-literal-into-outcome-variant.md) | Construct the selected concrete outcome arm before the outer variant, retaining resource/reference owners. |
| 5 | [Token discriminant comparison](token-discriminant-comparison.md) | Morph-paint guards compare a string token arm, rather than the entire optional ShapeCommandToken variant. |
| 4 | [Guarded record member](typeof-narrowed-record-member-read.md) | A numeric `typeof` check must narrow the following record-member read; skeleton erased-array rows use their own primary request above. |
| 4 | [User type predicate](type-predicate-narrowing-not-applied.md) | Carry `isMesh` evidence into the guarded caller's geometry read, rather than retaining only the Node3D schema. |
| 4 | [Readonly Record value projection](contextual-record-value-carrier-projection.md) | Rive rejection details forward an existing numeric Record into diagnostic-value storage without losing its identity or live cells. Fresh-literal graph allocation has a separate carried fix. |
| 4 | [Readonly sequence storage](readonly-sequence-owning-array.md) | Rive import-context initializers must store the same caller array behind a projected readonly sequence; reconstructing an owning array copies it. |
| 3 | [Generic narrower row argument](generic-structural-parameter-given-a-narrower-row.md) | Effect-capture geometry passes a Node2D row to a merged generic spatial-node parameter without the inferred trait/view composition. |
| 3 | [Readonly return owner recovery](readonly-row-return-owner-recovery.md) | The inherited color-scale return must recover its retained nominal owner as the sibling return already does. |
| 3 | [Numeric optional-chain result](numeric-optional-chain-result-erased.md) | SVG selector match length is computed numerically but stored as `optional<Any>` before assignment to double. |
| 3 | [Asserted view into nominal parameter](asserted-row-view-into-nominal-parameter.md) | A MovieClip assertion keeps an owner-preserving row, then omits exact owner recovery at nominal calls. |
| 2 | [Inferred-any/default runtime](node-any-generic-traits-leak.md) | Render queue and interaction recursion select default NodeTraits from NodeAny, changing child carriers before storage/recursive calls. |
| 2 | [Unchecked native assertion](assertion-lowered-to-unchecked-static-cast.md) | Native erased surface handles need a checked identity-preserving projection, not `static_cast`. |
| 2 | [Readonly callback derived owner](readonly-callback-derived-owner-recovery.md) | The bounds callback must retain the exact node and runtime capability rather than recover it from an erased base row. |
| 2 | [Required-read nullish fallback](host-initialize-optional-fallback-on-a-required-read.md) | glTF skin indexing emits `value_or` on a present double; use the actual lookup/missing-value boundary. |
| 2 | [Map-values local carrier](map-values-spread-local-element-carrier.md) | SWF placement spread storage must retain its typed elements instead of inferring `Array<Any>`. |
| 2 | [Array of union into owner array](array-of-union-into-array-of-owner.md) | PBR extension guards and the extended renderer pass union-valued elements to an array of the base owner; an element-copying conversion is not equivalent. |
| 1 | [Guarded external optional](guarded-external-optional-member-view.md) | WGPU capture loses the guarded texture payload; provider/readback and timer refusals remain separately recorded. |
| 1 | [Same-carrier union domains](distinct-union-domains-one-carrier.md) | WGPU target initialization loses source-domain evidence at its format fallback; declaring it alone exposes absent texture size APIs. |
| 1 | [External image discriminator/profile](image-source-union-domain-discriminator.md) | WGPU upload still lacks represented external-image value bindings **and a real provider copy operation**. Compiler/profile work alone is insufficient. |
| 1 | [Structural row into unknown](structural-row-erasure-to-unknown.md) | Logging a readonly render-texture row needs owner-preserving erasure to Any, not a copied visible prefix. |
| 1 | [Optional callback coalesce storage](compiler-flattened-optional-chain-union-storage.md) | Slot resolution applies `get_if` to an optional Node2D callback result; plan from its actual emitted storage. The carried property/texture patch does not cover this call trigger. |
| 1 | [Merged-row nominal recovery](merged-row-native-recovery-must-project.md) | DisposeNode must project a merged row before checked nominal recovery; runtime's deliberate assertion must stay intact. |
| 1 | [Typed-array static from](typed-array-static-from-binding.md) | AWD2 geometry retains an omitted parser; the ambient `Uint32Array.from` binding and related typed-array/SequenceView union conversion are both owed. |
| 1 | [Readonly unknown-array argument](typed-array-into-readonly-unknown-array-parameter.md) | The fake GL audit passes Array<double> to a readonly unknown-array parameter; preserve its original readonly sequence without a permissive Any-array conversion. |
| 1 | [Readonly Record entries](readonly-record-entry-enumeration.md) | Scene document field cloning needs one represented Record construction/enumeration owner, rather than iterable operations on an unsupported readonly row. |
| 1 | [Erased object Set key](erased-object-identity-set-membership.md) | A typeof-object guard supplies object identity, but Set<ErasedRef> receives unprojected Any during document cycle checks. |
| 1 | [Satisfies-table preset owner](satisfies-record-member-carrier.md) | Spine JSON bones store a shared anonymous preset reference into an unrelated nominal TransformInherit2D slot; retain that preset owner. |
| 1 | [Callable slot adaptation](callable-arm-in-contextual-union.md) | The SWF video handler's represented callable needs its trailing-argument adaptation and Sprite-to-Node2D result widening, without replacement nodes. |

## Header PASS and remaining callable bodies

These are separate acceptance dimensions. The current `declare-refused-function` manifest entries
occur in **29 PASS defining headers, declaring 30 functions** (two material listers share one header).
Those entries supply declarations, not implementations. This count excludes includers and other
extern/binding repair kinds; it is not an exhaustive count of unavailable APIs, nor an addition to 456.

- [Retained-callee dispositions](retained-declaration-calls-omitted-callee.md) preserve each qualified
  unused-header positive link and explicit-use negative link, or the exact prerequisite that rejected
  a candidate. STL, MD5 features, video URL loading, Scene2D/WGPU runtime access, SVG image/color,
  Lottie walking, hit registrations and constraint solvers must not become parser/provider/draw claims.
  An eager initializer failing the unused link is a rejected candidate, not a qualified negative link.
- [Glyph initialization](compiler-interface-heritage-owner-widening.md#g2-glyph-atlas-declaration-boundary),
  [readonly Map guard](readonly-map-instanceof-member-evidence.md) and
  [constraint/material owner recovery](assertion-to-undeclared-row-member.md) remain body requests
  when an exact declaration makes their headers usable. The partial native GL TextLabel carry also
  leaves its [generated Canvas path](retained-declaration-calls-omitted-callee.md#g2-gl-textlabel-generated-canvas-path-remains-unavailable)
  unavailable; no new draw proof follows from its PASS row.
- Carried G2 fixes with no assigned failed row still owe upstream lowering:
  [whole readonly-map key carrier](readonly-map-key-enumeration-value-carrier.md),
  [typed empty-map fallback](contextual-empty-array-fallback-element-evidence.md),
  [named ObjMaterial row alias](named-interface-value-recast-as-anonymous-row.md),
  [bound readonly context carrier](readonly-bound-external-value-carrier.md), and
  [native texture-cache local carrier](native-texture-source-cache-local-carrier.md).
  Their request-specific source/native checks have their stated scope; coverage graph/rendering
  behavior is separate. Rive graph fresh diagnostic allocation and SVG transform application likewise
  have bounded execution evidence, while Rive document rejection projection and SVG parsing remain open.

Existing compiler-patch debt outside this G2 failure/disposition batch remains in the
[request index](README.md). This draft does not re-file those historical requests or retire their
workarounds. Removing any carry requires its own expiry and semantic checks.

## Release hold

RichText's final `undeclared-identifier-74ca0a55` row now links the
[retained-callee request at line327](retained-declaration-calls-omitted-callee.md#g2-site-scene2d-gl-rich-text-draw-scene2d_glgl_rich_texthpp327).
The request records the actual `cpp-presence-test-without-absence-storage` overlay marker and links
its separate absence-storage mechanism. The heritage refusals belong to destroy/layout siblings.
The old cache-local request is history/supporting context, not the primary cause of the remaining
failure. No optional-overlay signature or declaration/body was invented, and the header remains FAIL.
WGPU bake also remains FAIL under batch one's descriptor request, with both faithful typing
candidates discarded. Resource reconciliation is qualified as recorded above.

The coverage-explain header is already PASS. The custom mesh header remains FAIL after its real
uniform upload fix, now linked to the existing readonly-source request above; neither result proves
custom drawing or complete graph coverage. The 16 row/cluster primary-request differences still
represent complementary causes, rather than missing links. Counts and the 29-header/30-declaration
PASS boundary were refreshed from the named recorded snapshot; only the newly landed cause slices
needed content review after the earlier full request audit.

Foreman must review the final draft before one upstream handoff. Integration has accepted the
qualified five-header results; their classifications require no count changes. This documentation
does not independently repeat or attest those executions. Do not send this draft upstream or send
individual request pings meanwhile. No build, generation, native or full sweep was rerun to prepare
this documentation.
