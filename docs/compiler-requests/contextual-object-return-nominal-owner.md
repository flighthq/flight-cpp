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

## Why no general fix selects a named owner at the physics2d site

Attempted at `2b687ea` and stopped, by measurement rather than by preference. The required lowering above assumes the
declaration site HAS a named contextual owner to select. At the physics2d site it does not, and the substitution cannot
be made structurally:

1. **The declaration is an inline anonymous shape.** `writePhysics2DColliderBounds(collider, out)` declares
   `out: { minX: number; minY: number; maxX: number; maxY: number }` -- an object type written in place, with no name in
   the source at all. The emitter's anonymous owner is therefore the correct reading of THAT declaration, and the
   mismatch appears at the call boundary, where the caller holds `flight::types::SpatialAabb2D` (from the broadphase
   scratch) and the parameter wants the local owner.
2. **A structural rule has to break a tie between two NAMED owners.** This checkout emits three owners whose fields are
   exactly `{min_x, min_y, max_x, max_y}`: `flight::types::CollisionAabb2D`, `flight::types::SpatialAabb2D`, and the
   physics2d anonymous struct. So "use the declared named owner with this shape" has two named answers before it has
   one, and any rule that picks between them is doing so on something other than the fields -- which is package
   knowledge by another name.
3. **Converting between them is exactly what this repository forbids.** The two are unrelated owners with no emitted
   heritage path, so the conversion would be a native pointer cast between distinct types, a materialized replacement
   owner, or side storage -- the three things the emitter's own refusal text rules out, and the same reasoning that
   closed `flattened-interface-refinement-identity`. Similar fields are not a heritage relationship just because the
   shapes coincide.

The two narrow changes that ARE safe, for the repository to choose between rather than for this file to assume:

- **A source patch naming the type** at the declaration (`out: SpatialAabb2D`, an annotation that erases in JavaScript
  and changes no field, no order and no aliasing; the callers already pass `SpatialAabb2D`). That is the workaround this
  file's own header already carries for the spritesheet-formats site, and it is the smallest change that makes the two
  sides one type. It recovers the 14 headers if the parameter is the only blocker among them, which the compile
  inventory says is likely but does not prove.
- **Making the emitter refuse instead of emitting invalid C++.** That is the honest half of the existing bug -- this
  file's first paragraph says the observed C++ carries no diagnostic -- but it recovers nothing, so it is a loudness
  fix rather than the fix.

Neither is taken here. The instruction for this pass was to attempt the general emitter fix and stop with a design
blocker if it could not be made safely, and that is what this section is.

### What the approved annotation actually measured

The source patch was approved, applied, and measured rather than assumed -- and it does **not** recover the headers. The
patch takes effect: `write_physics2_dcollider_bounds`'s parameter is now `flight::Ref<flight::types::SpatialAabb2D>`, and
exactly one header changes. The package still compiles **10 of 26 headers, with the identical 16 failures and the
identical diagnostic**. The parameter was not the only declaration emitting that anonymous owner. Three more inline
bounds shapes do, and because they carry the same four fields the emitter gives them the same anonymous type, so the
mismatches simply moved to their sites:

- `broadphase.ts`'s scratch `bounds` and `bodyBounds` in `createPhysics2DBroadphaseScratch`
- `worldQueries.ts`'s scratch `colliderBounds` in `createPhysics2DQueryScratch`

Naming all four is a known and bounded set of one-line annotations, and it is the obvious next step -- but each is a
separate site in a separate file, which is exactly the "stacking" this pass was told to stop short of. The stopping
instruction was to measure the annotation's effect and stop if it exposed a different blocker. It did, and its exact
shape is now on the record rather than in someone's memory: **the same defect at four declarations, one per inline
bounds shape**, with the emitter unchanged.
