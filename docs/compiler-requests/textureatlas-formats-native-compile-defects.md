# Texture-atlas format parsers lose exact object owners

At Flight `a62784923f3be814463286c9fb28edfe6ab15789` and flight-compiler
`2b687eaf6b631e0fa4d7d6e18dad93b845f99633`, `@flighthq/textureatlas-formats` emits all eight
modules with no refusal, but only `texture_atlas_page_meta.hpp` compiles independently. The other
seven headers fail through three distinct boundaries.

## A union common property loses its nominal owner

Both arms of `TextureAtlasAsepriteDocument` declare `meta: TextureAtlasAsepriteMeta`. Likewise, both
arms of `TexturePackerAtlasDocument` declare `meta: TexturePackerAtlasMeta`. Reading the common
property therefore has one exact source owner:

```ts
const meta = doc.meta;
atlas.imageName = meta.image;
```

The emitter instead synthesizes a package-local anonymous struct for each `meta` read and tries to
initialize it from the nominal object returned by `std::visit`:

```cpp
flight::Ref<app_format_frame_tags_image_scale_size_version_d9300326d86029c2> meta =
    std::visit([](const auto& value) { return value->meta; }, doc);
```

Clang rejects conversion from `Ref<flight::types::TextureAtlasAsepriteMeta>` to that unrelated
owner. The emitted anonymous declaration has the same members, member order, and member types as
the nominal declaration. The TexturePacker path fails identically. Common-property access across a
union whose arms agree on one nominal reference type must retain that exact reference type.

## Imported structural options acquire a caller-local owner

All four parsers create fresh region option objects and pass them directly to
`createTextureAtlasRegion` from `@flighthq/textureatlas`:

```ts
atlas.regions.push(createTextureAtlasRegion({
  height: frame.h,
  id: atlas.regions.length,
  name,
  // remaining TextureAtlasRegionLike fields
}));
```

The callee parameter and caller literal emit byte-identical declarations with the same structural
suffix, but the former is owned by `flight::textureatlas` and the latter by
`flight::textureatlas_formats`. Clang consequently rejects the caller `Ref` where the callee accepts
`optional<Ref<flight::textureatlas::...>>`. This is another measured reach of
`cross-package-anonymous-object-owner.md`: the call must retain one exact identity-preserving owner
or lower through a writable structural view.

## Separately owned Registry skew

`texture_atlas_detect.hpp` includes the current-pin registry contract and names
`flight::types::KeyedTable`, while the committed Registry row is intentionally frozen at the prior
pin and does not define that type. This package slice must not rewrite or emulate Registry. Detection
therefore remains an honest compile residual until the atomic Registry migration lands.

## Measured reach and carrying repairs

Before any new repair, package-scoped compilation at two workers reports 1/8 independently compiling
headers: page metadata passes; the two JSON parsers fail on nominal metadata ownership, libGDX and
Starling fail on the cross-package region-options owner, detection fails on `KeyedTable`, and the two
aggregate headers inherit the parser failures.

The carrying emission repairs may only alias the synthetic spellings to their already-declared exact
owners:

- Aseprite meta to `flight::types::TextureAtlasAsepriteMeta`;
- TexturePacker meta to `flight::types::TexturePackerAtlasMeta`;
- region options to the identical owner declared by `flight::textureatlas`.

These aliases add no operation or storage and preserve reference identity. They must self-expire when
the emitter retains the nominal/common owner itself. No repair is permitted for the Registry failure
in this slice.

## Defects exposed after exact-owner repair

With the three owner aliases applied, package compilation improves from 1/8 to 3/8: page metadata,
libGDX, and Starling compile. Two further lowering defects become the first diagnostics.

The Aseprite hash-frame arm is already a `Ref<TextureAtlasAsepriteBaseFrame>`, but the emitter
unnecessarily rematerializes it and writes TypeScript property spellings in the C++ designated copy:

```cpp
.sourceSize = structural_record_source->sourceSize,
.spriteSourceSize = structural_record_source->spriteSourceSize
```

The nominal owner declares `source_size` and `sprite_source_size`. Both sides of each copy must use
the normalized emitted member spelling, as the existing structural-record member-spelling repairs do
for other nominal owners. This is a mechanical spelling repair with no value or control-flow change.

After that spelling is corrected, Aseprite reaches the same JSON assertion defect already visible in
TexturePacker:

```cpp
static_cast<TextureAtlasAsepriteDocument>(flight::Json::parse(json))
static_cast<TexturePackerAtlasDocument>(flight::Json::parse(json))
```

`Json::parse` returns `JsonValue`; neither nominal union has such a conversion. As recorded in
`assertion-lowered-to-unchecked-static-cast.md`, the TypeScript assertion cannot become an unchecked
C++ cast. These public string parsers need checked JSON-object reads that preserve the source's
never-throw sentinel policy, hash/array order, tolerant malformed-frame skipping, defaulted optional
geometry, and page metadata. A carrying implementation must not fabricate a nominal document or
silently accept the cast.

The compiling Starling parser also exposes an absence-lowering defect at runtime. Source reads
`a['name']` in a truthiness guard and compares `a['rotated'] === 'true'`. Missing Record keys yield
`undefined`, so the first skips the element and the second evaluates false. Emission instead calls
`.value()` on both `Record::get` results, throwing `bad_optional_access` for an ordinary unrotated
subtexture or one without a name. A target-neutral carrying patch binds the name through `?? ''` and
the rotated attribute through `?? 'false'`; those fallbacks select exactly the results JavaScript
already produces for absence and leave every present string unchanged.

## Final measured state

Two complete, derived parser overrides keep every repaired emitted declaration and replace only the
two impossible JSON casts with checked `JsonValue` traversal. They accept hash and array documents,
retain source order and atlas identity, preserve every represented frame/page field, skip malformed
frames, and leave an atlas unchanged when JSON or its root shape is invalid. They do not construct a
nominal document owner from unverified data.

Package-scoped compilation then reaches 5/8 headers: page metadata and all four concrete parser
headers compile. Detection and the two aggregate headers fail only through the frozen Registry
`KeyedTable` gap. The package declares parsers and detection but no serializer at this pin, so no
serializer or parse-format-parse round trip can be claimed faithfully. Four isolated shadow mutations
are killed by the native suite: push-to-front JSON order, disabled TexturePacker prefix stripping,
swapped Starling rotated fallback dimensions, and libGDX 270 degrees mapped to the wrong direction.
