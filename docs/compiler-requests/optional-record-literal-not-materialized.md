# An object literal passed to an `optional<Record<...>>` parameter is emitted as a bare braced pair list

**Family.** Record construction inside an optional parameter slot. **Sites at this pin:** 16 headers across
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

The same shape appears in 16 headers, recorded in `docs/header-failures.md` as cause cluster
`other-40da43f7` and in `generated/header-status.json` with the diagnostic above. They span
`scene2d_formats/rive_skin`, `scene3d_formats/{md2_parse,obj_parse,three_ds_keyframe_handler,three_ds_material_handler,three_ds_mesh_handler,three_ds_parse}`,
`skeleton2d_formats/{dragon_bones_parse,spine_binary_draw_order_timeline_handler,spine_binary_skins_handler,spine_binary_slots_handler,spine_binary_versioned}`
and `swf/{_internal_swf_tag_stream_test_helper,swf_all_tag_handlers,swf_bitmap_tag_family,swf_jpeg_bitmap_handler}`.

## What we do meanwhile

Nothing. No repair, no override, no source patch: a source patch would have to rewrite the object literal at
16 sites across six packages to work around one missing lowering, and the lowering is the defect. The sites
stay failing and counted until the emitter names the storage.
