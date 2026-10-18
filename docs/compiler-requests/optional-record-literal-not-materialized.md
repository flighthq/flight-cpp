# An object literal passed to an `optional<Record<...>>` parameter is emitted as a bare braced pair list

**Family.** Record construction inside an optional parameter slot. **Historical assigned sites:** 17 headers across
scene2d-formats, scene3d-formats, skeleton2d-formats and swf (2D, 3D and ANIM lanes). **Route:** compiler.

## Reproduction

`packages/scene3d-formats/src/threeDsParse.ts:98`:

```ts
reportImportDiagnostic(diagnostics, ImportDiagnosticSeverity.Reject, '3ds.wrong-main-chunk', 'parse3ds', {
  foundId: mainId,
});
```

emits `flight/scene3d_formats/three_ds_parse.hpp:208`:

```cpp
report_import_diagnostic(diagnostics, flight::types::import_diagnostic_severity->reject,
    flight::String("3ds.wrong-main-chunk"), flight::String("parse3ds"),
    {{flight::String("foundId"), std::variant<bool, double, flight::String>{std::in_place_type<double>, main_id}}})
```

The parameter's declared storage is
`std::optional<flight::Record<flight::String, std::variant<bool, double, flight::String>>>`, and the diagnostic is

```
error: no matching constructor for initialization of
  'std::optional<Record<String, variant<bool, double, String>>>'
```

## Cause

The object literal is lowered to a bare brace-enclosed key/value list. That list would construct a `Record`,
but it is written directly in a slot whose type is `optional<Record<...>>`, and `std::optional` has no
constructor taking a braced-init-list of one `Record`'s contents: the Record has to be NAMED inside the
optional. The emitter materializes the Record's contents but not the Record itself.

This is a lowering gap, not a runtime one: every piece the expression needs already exists in the emitted text.
The fix is to name the declared storage, for example
`std::optional<flight::Record<...>>{flight::Record<...>{...}}` (or `std::in_place`), which is the same value
reached through the constructor the optional actually has. Nothing about absence, reference identity, equality
or ordering changes: the optional is engaged either way, and the Record is the same Record the literal describes.

## Sites

The historical assigned shape appears in 17 headers, recorded in `docs/header-failures.md` as cause cluster
`other-40da43f7` and in `generated/header-status.json` with the diagnostic above. They span
`scene2d_formats/rive_skin`, `scene3d_formats/{md2_parse,obj_parse,three_ds_keyframe_handler,three_ds_material_handler,three_ds_mesh_handler,three_ds_parse}`,
`skeleton2d_formats/{dragon_bones_parse,spine_binary_bones_handler,spine_binary_draw_order_timeline_handler,spine_binary_skins_handler,spine_binary_slots_handler,spine_binary_versioned}`
and `swf/{_internal_swf_tag_stream_test_helper,swf_all_tag_handlers,swf_bitmap_tag_family,swf_jpeg_bitmap_handler}`.

## Shared compiler correction

`compiler-optional-record-literal-materialization` names the existing contextual Record carrier around
ordinary nonempty entries. The empty literal path already names that carrier, and computed/spread entries
already use ordered construction. External descriptor construction stays on its earlier separate path.
No TypeScript caller or runtime owner is replaced. Existing Record arguments keep their identity; fresh
literals allocate once, including an engaged empty Record. Missing values, explicit null/undefined,
field order and abrupt completion remain observable.

The focused original emitted fixture fails at six calls under both the raw pinned backend and the
preceding compiler stack. The corrected fixture compiles, links and runs its ownership, absence and
ordering controls. Normal SDK regeneration and actual original public consumer qualification now pass for the shared
correction. The completed scoped check promotes only the nine measured passing historical rows;
remaining first failures are recorded below.

The current historical cluster has three later follow-ons (`spine_parse`, SWF `_internal_index` and
`contract`) in addition to the assigned17. The newly exposed five `svg_gradient.hpp:106` callers are a
separate observation set. The five `svg_bounds.hpp:114` callers currently stop on absent `get_shape_bounds`,
which is a different cause. Keep these memberships separate and measure their current results rather
than adding them into a claimed gain. Applicability follows the generated package environment declaration.

Several existing Flight patches bind diagnostic details to typed locals as a local workaround, including
the cases in the [related request](object-literal-into-optional-record-argument.md). Their other changes
remain required until separately qualified; carrying this shared fix does not retire composite patches.

## G2 Rive clipping diagnostic detail

The same optional Record construction appears in scene2d_formats/rive_clipping.hpp:231 at Flight
a627849 / compiler806d257: report_import_diagnostic receives bare pairs for index and source when
shapePaths has no geometry. With only the exact refused-transform prototype in a scratch header,
Clang22.1.8 reports the constructor error at candidate233, after the nullable target payload error.
The actual unused-header compile exits1; no positive link/run or qualified negative link is reached.
The original owned census row stays on missing create_rive_relative_transforms207 because this
candidate is rejected. Preserve both detail keys/numeric values, Drop severity, optional sink and
continue behavior; dropping the diagnostic is not a clipping fix. See the
[retained-callee audit](retained-declaration-calls-omitted-callee.md#g2-rive-clipping-declaration-does-not-pass-the-unused-header).

## G2 SWF filter: a local two-site materialization, not a family fix

The `swf-filter-effect-owner-pushes` patch also names the two filter diagnostic detail objects in
`swfFilter.ts` as typed `Readonly<Record<string, boolean | number | string>>` locals before passing
them to `reportImportDiagnostic`. The emitter then emits the declared
`flight::Record<...> detail = {{...}, {...}};` storage and passes it to the `optional<Record<...>>`
parameter, which compiles. The same three fields and values are built before the same call, so nothing
about the diagnostic changes.

This is deliberately local: two sites in one module, carried as part of a bounded repair whose original
root was effect-owner heritage. It does not claim the 16-site family is fixed, and the remaining sites
across the other packages still fail exactly as recorded above; the emitter must still name the storage
for an object literal written directly in an optional Record slot.

## Scoped native qualification

At Flight `a627849` / compiler `806d257`, the normal compiler stack of 72 patches applies, builds and
reverts to the exact clean pin. The four root packages lower 2005 modules and process the requested
28 source modules through normal best-effort generation: 19 emitted, nine existing partial refusals.
All original function definitions and every existing `NOT GENERATED` marker line survive in the 19
changed normal headers. The applied patch digest is retained exactly; unrelated generated provenance
and source-patch records are unchanged. No refused function gains a body or a narrower source domain.

Applicability comes from each package's generated environment declaration and
`deferred-packages.json.applicableEnvironments`: all 27 assigned headers are applicable.

| Assigned membership | Applicable | Header pass | Header fail |
| --- | ---: | ---: | ---: |
| Historical optional Record sites | 17 | 9 | 8 |
| Previously exposed gradient106 callers | 5 | 0 | 5 |
| Previously exposed bounds114 callers | 5 | 0 | 5 |
| Total distinct assigned headers | 27 | 9 | 18 |

All 27 were attempted with Clang 22.1.8 at jobs 2; none is unattempted. A separate direct
`svg_gradient.hpp` support-header check fails at its later callback result and is outside that 27.
The three later cluster follow-ons are not requalified. No failing assigned header retains the old
optional Record constructor as its first diagnostic.

The nine passing historical headers are `rive_skin`, `md2_parse`, `three_ds_keyframe_handler`,
`three_ds_material_handler`, `three_ds_parse`, `dragon_bones_parse`,
`spine_binary_draw_order_timeline_handler`, `spine_binary_slots_handler` and `spine_binary_versioned`.
Passing a partial header does not complete its refused public surface; existing omission markers remain.
Only those nine rows are promoted. Failing assignment labels remain historical; current observations
are in the delivered qualification report. Global historical sweep totals are unchanged.

| Current first failure | Assigned headers |
| --- | ---: |
| `obj_parse.hpp:204`: readonly `SequenceView` lacks `find` | 1 |
| `three_ds_mesh_handler.hpp:391`: retained call to refused material conversion | 1 |
| `spine_binary_bones_handler.hpp:48`: anonymous preset array storage | 1 |
| `spine_binary_skins_handler.hpp:274`: retained call to refused attachment reader | 1 |
| `swf_jpeg_bitmap_handler.hpp:233`: different package owners for the same anonymous bitmap shape | 4 |
| `svg_bounds.hpp:114`: unavailable Shape bounds function | 5 |
| `svg_gradient.hpp:174`: anonymous stop-map result array disagrees with the retained stop array | 5 |

The bounds case has a real raw TypeScript body at `shape.ts:63`. Generation refuses its writable
ShapeRuntime refinement (`cpp-structural-assertion-writable-capability-unproven`), and the existing
partial Shape override explicitly leaves runtime recovery, bounds and revision behavior unsupported.
It is separate from Record construction. The gradient map explicitly creates fresh stop/color literals;
converting existing owners into another nominal array would not repair its missing contextual owner.

The original exported `parseThreeDsDocumentWithDispatch` and `tallyThreeDsDrop` now compile, link and run
in the existing `flight_cpp.generated_scene3d_formats` CMake target (CTest 1/1 PASS). Before regeneration,
that actual consumer failed exactly at `three_ds_parse.hpp:208` optional Record construction. The same
cases pass against the original pinned TypeScript import closure: wrong file ID and exact numeric detail,
absent detail/sink, malformed child recovery, ordered aggregated fields, first-offender count and retained
Record identity/live mutation. No copied parser, fake helper, replacement owner or declaration-only
link workaround supplies the behavior. Existing scene3d-formats controls also pass.

The required full compiler static check retains four known baseline gate failures (format, lint, import
order and five peer fixture type errors). Own scoped formatting/lint and production TypeScript pass;
source-patch contracts pass 2/2. These results establish this shared correction and the measured subset,
not full SDK or package completion. Local diagnostic-detail workarounds remain for separate expiry
qualification.
