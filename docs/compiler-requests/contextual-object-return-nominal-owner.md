# A contextual object return must use its declared nominal owner

A function with a declared interface return type returns an object literal containing exactly that
interface's fields. The emitter creates a module-local anonymous owner for the literal and returns it
from a function declared as `Ref<Nominal>`, producing invalid C++ without a compiler diagnostic for the
helper.

Measured at Flight `a627849` / flight-compiler `2b687ea` in both serializer metadata helpers in
`@flighthq/spritesheet-formats`:

```ts
interface Meta { app: string; image: string; size: { h: number; w: number } }

function dataToMeta(data: Data): Meta {
  return { app: 'tool', image: data.imageFile, size: { h: data.height, w: data.width } };
}
```

## Observed C++

The generated Aseprite helper has the equivalent shape:

```cpp
inline flight::Ref<flight::types::AsepriteMeta> data_to_meta(...) {
  return flight::make_ref<app_format_frame_tags_image_layers_scale_size_slices_version_...>(...);
}
```

Clang rejects the anonymous `Ref` as not convertible to `Ref<AsepriteMeta>`. TexturePacker emits the
same defect with its own anonymous metadata owner. The headers contain no diagnostic for these helpers;
their public serializer declarations are independently omitted by the named-union `Partial<T>` refusal.

## Required lowering

The function's contextual return type must select the declared nominal owner at the construction site:

```cpp
return flight::make_ref<flight::types::Meta>(flight::types::Meta{...});
```

All properties retain source evaluation order, and the single fresh object returned by JavaScript must
remain the single owner returned by C++.

## Carrying workaround

`source-patches/spritesheet-formats-serializer-existing-shapes.patch` binds each literal to its declared
metadata type immediately before returning it. The annotation erases in JavaScript and the local returns
the identical fresh object. The same patch removes a second anonymous-owner site by letting the internal
helper accept `undefined` directly instead of synthesizing `{}` at its call sites. It expires when
contextual return and call construction use their declared nominal owners directly.

Also blocks `@flighthq/physics2d` at a627849, where it is the package's single largest compile blocker: **14 of its 26
headers** fail on the same shape one type over. The bounds literal a physics2d module builds for a world query is emitted
as a module-local anonymous owner, `Ref<min_x_min_y_max_x_max_y_deb9806a4ba4981e>`, and passed where the named
`flight::types::SpatialAabb2D` is declared -- `no viable conversion from 'Ref<flight::types::SpatialAabb2D>' to
'Ref<min_x_min_y_max_x_max_y_deb9806a4ba4981e>'`. The two structs are field-for-field identical (`min_x`, `min_y`,
`max_x`, `max_y`), which is why this reads as the duplicate-structural-struct family: it is not, because that repair
requires a `flight::types` copy of the anonymous struct to alias TO, and this checkout has none -- the named
`SpatialAabb2D` IS the canonical owner, so the fix is to emit that owner rather than to alias a copy of it. The
blocked headers are `world`, `step`, `solver`, `joints`, `joint_factories`, `joint_registry`,
`register_built_in_physics2djointsolvers`, `islands`, `world_queries`, `broadphase`, `enable_physics2dguards`,
`explain_physics2djoints` and the package's two barrels. No workaround is carried: patching every one of those sites in
the pinned source would be a rewrite of the package's query surface, which is exactly what this request exists to
avoid.
