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

### Why the two lowerings disagree, in one function

The same function shows both lowerings side by side, which pins the cause. `x`, `y`, `width`,
`height` and `id` are `number` (non-nullable) and they coalesce correctly, consuming the optional
carrier the read actually produced:

```cpp
const double x = ([&]() -> double {
  auto nullish_coalesce_left = flight::row_get<flight::RowKey<"x">>(source);
  if (nullish_coalesce_left.has_value()) return nullish_coalesce_left.value();
  return 0.0;
}());
```

`name`, `originalHeight`, `originalWidth`, `pageName`, `pivotX` and `pivotY` are `T | null` and they
take the variant path. The operand is the same partial read in every case, so the two paths cannot
both be right: **the `??` lowering is selected by the coalesce's RESULT type (`T | null` picks the
variant path) instead of by the OPERAND's carrier** (`Partial<T>` reads are `std::optional<T>`). It
is only when the two disagree -- a nullable property read from a partial row -- that the emitted
`holds_alternative` has no matching overload.

A bounded search of the emitted tree confirms this is one function and not a class of them: of the
sites emitting `holds_alternative<flight::Undefined>(optional_property)`, only the six whose
enclosing function takes a `RowPartial` are affected, and all six are in `set_texture_atlas_region`.
The others read a complete `RowOf<T>` row, where a `T | null | undefined` property really is a
variant and the variant path is correct.

### Reproducer

The defect is in the emitter's OUTPUT, so this must reach the **generated** file rather than the
header name: a `textureatlas-region-nullable-partial-read` override now shadows
`flight/textureatlas/texture_atlas_region.hpp` on the normal include path, so compiling the header
name succeeds and reproduces nothing. Compile the generated path directly:

```
printf '#include "generated/include/flight/textureatlas/texture_atlas_region.hpp"\nint main(){return 0;}\n' > /tmp/probe.cpp
c++ -std=c++20 -fsyntax-only -Ioverrides/include -Igenerated/include -Iinclude /tmp/probe.cpp
```

```
generated/include/flight/textureatlas/texture_atlas_region.hpp:290:271:
    error: no matching function for call to 'holds_alternative'
libcxx/include/variant:1324:52: note: candidate template ignored:
    could not match 'variant' against 'std::optional'
```

The note is the whole defect in one line: the lowering asks for a `variant` and is handed an
`std::optional`.

For this nullish-coalescing use, source `null` and `undefined` intentionally have the same result.
The correct lowering can therefore consume the actual optional carrier directly: return its payload
when present and `std::nullopt` otherwise. A more general read that must distinguish null from
undefined needs a carrier that retains both states; it must not emit variant operations against an
optional.

## Reach

**Historical, before the override landed.** The generated `texture_atlas_region.hpp` failed clang at
`:290`, and because that module is in the package's include closure the error was inherited by the
`@flighthq/textureatlas-formats` consumers: six of its eight headers failed to compile for this
reason while textureatlas-formats itself emitted 8/8 modules. A file-based native test of
`texture_atlas_page_meta.hpp` compiled and ran because it does not include this dependency header.

**Current.** The root is carried by the `textureatlas-region-nullable-partial-read` override, so the
inherited failure is gone. Measured on this branch, one
`clang -std=c++20 -fsyntax-only -w -Ioverrides/include -Igenerated/include -Iinclude` per header:

- `texture_atlas_aseprite_parse.hpp`, `texture_atlas_libgdx_parse.hpp`,
  `texture_atlas_starling_parse.hpp`, `texture_packer_atlas_parse.hpp` and
  `texture_atlas_page_meta.hpp` all exit 0 -- the four parse headers now compile.
- `_internal_index.hpp`, `contract.hpp` and `texture_atlas_detect.hpp` still exit 1, but each one's
  first error is at `texture_atlas_detect.hpp:67` and **`:290` appears zero times** in the output.
  They do not reach this defect.

The override's own manifest `remaining` reads "None in textureAtlasRegion.ts at this pin"; its
`expires` names what would let it be dropped -- nullable `Partial` reads lowering against their actual
carrier, enum defaults retaining their owner, and narrowed `Texture2D` rows retaining a recoverable
concrete owner.

**Consumers beyond textureatlas-formats, withdrawn.** `@flighthq/bitmaptext` and
`@flighthq/movieclip` were recorded elsewhere as consumers of this root. Neither is one now:
bitmaptext's compile output mentions `:290` zero times and its four failing headers are its own errors
(`no matching function for call to 'create_texture'` at `update_bitmap_text.hpp:121`, and
`use of undeclared identifier 'build_bitmap_text_words'` at `:254` and `:303`); movieclip's first
failure is `scene2d/sprite.hpp:107`.

**Overrides are now carried.** The earlier statement that no dependency override or source rewrite
accompanies this work no longer holds: `textureatlas-region-nullable-partial-read` carries this root,
and `textureatlas-formats` carries `textureatlas-formats-aseprite-checked-json` and
`textureatlas-formats-texture-packer-checked-json`, both `complete`. The emitter defect itself is
unpatched -- the override shadows its output rather than repairing it, which is why the generated-path
reproducer above still fails while the header name compiles.

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
