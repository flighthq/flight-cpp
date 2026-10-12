# Interface heritage is not a C++ owner relation

A value whose TypeScript type is a sub-interface cannot be stored in a slot typed as its super-interface
when the type still has to cross a nullable union. Each interface is emitted as its own flattened
struct, so the declared C++ owners are unrelated and there is no pointer cast to follow:

```
cpp emission failed for .../glyphSource.ts: contextual nullable interface conversion from Bitmap to
TextureSource follows TypeScript interface heritage, but flight-cpp stores those declarations in
independent flight::Ref<flight::types::Bitmap> and flight::Ref<flight::types::TextureSource> owner
types: each interface struct is emitted flattened, so the heritage is not a C++ base relation and no
pointer cast exists to follow. A native pointer cast would claim C++ inheritance the emitted interface
structs do not have, materializing the target would replace object identity, and a structural row would
change the declared result carrier.
```

TypeScript accepts this assignment because `Bitmap` declares `TextureSource` in its heritage. In C++
`flight::types::Bitmap` and `flight::types::TextureSource` are separate structs with no base relation,
so the three obvious repairs are each wrong for a stated reason: a `static_cast` would claim an
inheritance the emitted structs do not have, constructing a `TextureSource` would replace the object's
identity (the runtime's reference identity is part of the semantics this project exists to preserve),
and a structural row would change the declared carrier of the result.

## Minimal TypeScript

```ts
interface Image { readonly width: number; readonly height: number }
interface TextureSource extends Image { readonly version: number }
interface Bitmap extends TextureSource { readonly pixels: Uint8Array }

declare function getBitmap(atlas: Readonly<GlyphAtlas>): Bitmap;

// Bitmap widens to TextureSource | null through the heritage chain.
export function imageSlot(atlas: Readonly<GlyphAtlas>, page = 0): TextureSource | null {
  return page === 0 ? getBitmap(atlas) : null;
}
```

## Expected C++

One of these two, chosen deliberately rather than by rewriting the source:

```cpp
// (a) an owner-preserving heritage carrier in the runtime contract
std::optional<flight::Ref<flight::types::TextureSource>> image_slot(..., std::optional<double> page) {
  if (page.value_or(0.0) != 0.0) return std::nullopt;
  return flight::Ref<flight::types::TextureSource>{get_bitmap(atlas)};  // retains the Bitmap owner
}
```

or (b) emitted interface structs that carry their heritage as a real base relation, so the upcast is an
ordinary pointer conversion and nothing is materialized.

Whichever is chosen, the stored value must keep the exact owner it was created as — reference identity,
absence, and the declared result carrier all have to survive the widening.

## Affected modules

`@flighthq/glyphatlas`'s `glyphSource.ts` — `initializeGlyphSourceFromGlyphAtlas` binds
`getGlyphAtlasImage` to a lambda returning `Bitmap | null` where the declared slot is
`TextureSource | null`. The same shape recurs wherever a package hands a concrete resource back through
a super-interface slot (`Image`/`TextureSource`/`Bitmap` appear across the resource and bitmap
packages).

## Carrying workaround

None. Two target-neutral spellings that the refusal itself suggests were tried and both still refuse
the module: an early return that removes the ternary, and declaring the lambda's result as
`TextureSource | null` with the concrete value bound to a local first. Neither changes the fact that
the crossing itself is unrepresentable, so this is left as a request and the module is reported as a
retained refusal rather than papered over with an override.

## Same defect, second package (linked evidence, 2026-10-12)

`@flighthq/bitmapfont` is refused for the identical conversion, with different type names:

```
cpp emission failed for .../bitmapFontGlyphSource.ts: contextual nullable interface conversion from
VoxelGrid to TextureSource follows TypeScript interface heritage, but flight-cpp stores those
declarations in independent flight::Ref<flight::types::VoxelGrid> and
flight::Ref<flight::types::TextureSource> owner types ...
```

`Bitmap -> TextureSource` (glyphatlas) and `VoxelGrid -> TextureSource` (bitmapfont) are one rule, not
two: a sub-interface value widening into a super-interface slot across a nullable union, with the emitted
structs carrying no base relation. A fix for either is a fix for both, and the `unblocks` figures for
each should be read together rather than as two separate capabilities.

Both packages also have a wider consumer: `@flighthq/bitmapfont-formats` depends on bitmapfont, so the
bitmapfont refusal cascades into that package's denominator as well.

## Same defect in P-3D2

`@flighthq/scene3d-formats` records the same rule at
`gltfPunctualLights.ts:47`. `buildGltfPunctualLight` declares `Light | null`, but its directional branch
returns `createDirectionalLight(...)`, whose exact owner is `DirectionalLight`. TypeScript accepts the
declared `DirectionalLight extends Light` heritage; flight-cpp emits independent
`flight::Ref<flight::types::DirectionalLight>` and `flight::Ref<flight::types::Light>` owners and refuses
the nullable crossing for the same identity reason as Bitmap and VoxelGrid above.

Point and spot branches reach their own concrete light owners. They do not make the directional
conversion safe, and copying a common `Light` prefix would still replace the object returned by the
factory. This site therefore extends the affected package set; it does not define a third compiler
request.
