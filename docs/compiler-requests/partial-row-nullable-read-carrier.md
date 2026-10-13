# Nullable fields read through `Partial<T>` use the wrong storage domain

At Flight `a62784923f3be814463286c9fb28edfe6ab15789` and flight-compiler
`2b687eaf6b631e0fa4d7d6e18dad93b845f99633`, `setTextureAtlasRegion` emits but its header does not
compile. A nullable field read through `Readonly<Partial<TextureAtlasRegionLike>>` is represented as
an `std::optional<T>`, then lowered as though that same expression were an `std::variant<T, Null,
Undefined>`.

## Source

```ts
export function setTextureAtlasRegion(
  out: TextureAtlasRegion,
  source: Readonly<Partial<TextureAtlasRegionLike>>,
): void {
  const name = source.name ?? null;
  const originalHeight = source.originalHeight ?? null;
  const originalWidth = source.originalWidth ?? null;
  const pageName = source.pageName ?? null;
  const pivotX = source.pivotX ?? null;
  const pivotY = source.pivotY ?? null;
  // ...assign the locals to out...
}
```

Each underlying field is `string | null` or `number | null`; `Partial<T>` adds `undefined`.

## Emitted mismatch

For `name`, `row_get` returns `std::optional<flight::String>`, but the generated expression applies
variant operations to it:

```cpp
auto optional_property = flight::row_get<flight::RowKey<"name">>(source);
if (std::holds_alternative<flight::Undefined>(optional_property)) { /* ... */ }
if (std::holds_alternative<flight::Null>(optional_property)) { /* ... */ }
return std::get<flight::String>(optional_property);
```

Clang rejects both `holds_alternative` calls and the `std::get` because `optional_property` is an
`std::optional`, not a variant. The five numeric nullable fields emit the same mismatch.

For this nullish-coalescing use, source `null` and `undefined` intentionally have the same result.
The correct lowering can therefore consume the actual optional carrier directly: return its payload
when present and `std::nullopt` otherwise. A more general read that must distinguish null from
undefined needs a carrier that retains both states; it must not emit variant operations against an
optional.

## Reach

`@flighthq/textureatlas`'s `texture_atlas_region.hpp` is the first native error inherited by six of
the eight `@flighthq/textureatlas-formats` headers. Package-scoped compilation therefore reports
those consumers as failures even though textureatlas-formats itself emits 8/8 modules. A file-based
native test of `texture_atlas_page_meta.hpp`, which does not include this dependency header, compiles
and executes successfully.

No dependency override or source rewrite is carried by the textureatlas-formats migration.

## Native carrying repair

The textureatlas repair derives `flight/textureatlas/texture_atlas_region.hpp` from the generated
header and changes only the six invalid nullable reads in `setTextureAtlasRegion`. Because every
source expression coalesces both `null` and `undefined` to `null`, consuming the emitted
`std::optional<T>` directly preserves the observable result: an empty carrier stays empty and a
present payload stays present. The same derived copy keeps the independently filed
`TextureAtlasRotation` nullish result in its declared enum domain.

Measured with package-scoped compilation at two workers, textureatlas improves from 1/7 to 3/7
independently compiling headers. Its remaining guard refusal and texture-factory mismatch are
independent. Bitmaptext improves from 2/6 to 6/6: its three concrete consumers and aggregate no
longer fail through this dependency. Bitmaptext still reports its pre-existing stale `bitmap_text.hpp`
override; this compiler repair does not re-derive or change that TEXT behavior.
