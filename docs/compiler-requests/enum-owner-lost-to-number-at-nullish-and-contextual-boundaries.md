# Enum ownership is lost to `number` at nullish and contextual boundaries

At Flight `a62784923f3be814463286c9fb28edfe6ab15789` and flight-compiler
`2b687eaf6b631e0fa4d7d6e18dad93b845f99633`, two `TextureAtlasRotation` paths lose the enum owner and
become plain numeric carriers. One produces invalid C++; the other is refused during emission.

## Nullish default emits an impossible union

```ts
const rotation = source.rotation ?? TextureAtlasRotation.None;
out.rotation = rotation;
```

`source.rotation` is optional because `source` is `Partial<TextureAtlasRegionLike>`. Both the present
value and the default are members of `TextureAtlasRotation`, so the local's type is the enum. The
emitter instead produces:

```cpp
std::variant<flight::types::TextureAtlasRotation, double> rotation =
    std::variant<flight::types::TextureAtlasRotation, double>{
        std::in_place_type<flight::types::TextureAtlasRotation>, /* coalesced value */};
out->rotation = rotation;
```

Clang rejects the assignment of that variant to `TextureAtlasRotation`. The default enum member's
numeric representation must not add a `double` alternative when the other arm and destination both
name the enum.

## Mutable enum local is refused as an unrepresented number

The libGDX atlas parser has the same owner loss without a nullish expression:

```ts
let rotation = TextureAtlasRotation.None;
// later assignments use only Counterclockwise90 or Clockwise90
createTextureAtlasRegion({ rotation, /* ... */ });
```

After earlier indexed-string refusals were removed, this declaration was refused as
`cpp-contextual-union-value-type-unrepresented`: “contextual union value type double is not a
represented runtime domain.” Adding the erased annotation
`let rotation: TextureAtlasRotation = TextureAtlasRotation.None` makes the module emit, showing that
the runtime value and control flow were already representable and only the enum owner evidence was
lost.

## Expected lowering and carrying workaround

Both locals should remain `flight::types::TextureAtlasRotation`; every assigned value is a member of
that enum, and the destination is the same enum. The textureatlas-formats migration carries the
explicit annotation for its mutable local in
`source-patches/textureatlas-formats-indexed-lines.patch`. It does not rewrite the
`@flighthq/textureatlas` dependency's nullish-default path.

The textureatlas native repair carries that dependency path in its derived
`texture_atlas_region.hpp`: it reads the optional enum carrier and applies
`TextureAtlasRotation::None` with `value_or`, so no numeric alternative is introduced. The separate
libGDX source annotation remains owned by textureatlas-formats.
