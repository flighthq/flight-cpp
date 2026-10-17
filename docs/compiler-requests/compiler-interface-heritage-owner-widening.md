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

`@flighthq/skeleton2d-formats` at `a627849`: `spineBinarySkinsHandler.ts` (line 116) refuses widening a
`RegionAttachment2D` into the nullable `Attachment2D` slot it is stored in. It is the module's remaining
blocker once its `Float32Array.from` site is carried by
[`skeleton2d-formats-binary-influences-constructor`](../../source-patches/skeleton2d-formats-binary-influences-constructor.patch).

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

### The carrier at bitmapfont is representational, not a TypeScript widening

The two sites reach this boundary differently, and the difference is visible in the declared types. At
glyphatlas the sub-interface value is statically known and widens in the source. At bitmapfont the
TypeScript types on both sides already agree, so nothing widens there:

```ts
// @flighthq/types GlyphSource.ts:20 -- the seam's declared result
getGlyphAtlasImage(page?: number): TextureSource | null;
// @flighthq/types Texture.ts:36 -- the member's declared type
source: TextureSource | null;
```

`bitmapFontGlyphSource.ts` reads `font.pages[page]?.texture` and returns `texture.source`, which is
already `TextureSource | null`. The crossing is introduced by the C++ representation, where the member
is stored as a union of concrete owners while the seam declares the base owner:

```cpp
// @flighthq/types texture.hpp:72 -- stored
std::optional<flight::types::TextureSourceValue> source;
// @flighthq/types texture_source_value.hpp:18 -- a variant of seven concrete owner refs
using TextureSourceValue = std::variant<flight::Ref<VoxelGrid>, flight::Ref<TextureSource>,
    flight::Ref<ExternalTexture>, flight::Ref<Bitmap>, flight::Ref<CompressedImageResource>,
    flight::Ref<RenderTarget>, flight::Ref<ImageResource>>;
// @flighthq/types glyph_source.hpp:57 -- declared
std::function<std::optional<flight::Ref<flight::types::TextureSource>>(std::optional<double>)>
    get_glyph_atlas_image;
```

The refused conversion is therefore `optional<variant<7 concrete owner refs>>` to
`optional<Ref<TextureSource>>`, and the refusal names `VoxelGrid` because it is the first arm that
would need a heritage cast, not the only one.

This does not change the rule or add a supply: either option above closes both sites. It changes what a
fix may assume about the crossing. At glyphatlas the emitter can see the concrete owner; here the
concrete owner is precisely what the variant erases, so a fix that needs the concrete type at the
crossing cannot reach this site on its own. The refusal's own "avoid the boundary" advice does not
apply here either -- it suggests declaring the destination as the derived interface where the concrete
type is known, and this destination is the `GlyphSource` seam's declared type rather than a local
choice.

#### Minimal reproduction: a union-typed member read

```ts
interface Texture2D { readonly dimension: '2d'; source: TextureSource | null }
interface BitmapFont { readonly pages: readonly Texture2D[] }

// Both sides are declared as the base owner, so TypeScript reports no conversion; the emitted member
// is still a variant of concrete owners, so the read cannot be narrowed to the base owner.
export function bindGlyphAtlasImage(font: Readonly<BitmapFont>) {
  return (page = 0): TextureSource | null => {
    const texture = font.pages[page];
    return texture?.dimension === '2d' ? texture.source : null;
  };
}
```


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

## Same defect in image resource resolution

`@flighthq/image` reaches the same boundary in `imageResourceReference.ts`. Its resolver selects a
`Bitmap | null` from the embedded decode arm or an `ImageResource | null` from the external fetch arm,
then returns the selected value through the declared `Promise<TextureSource | null>` result. Both
concrete interfaces extend `TextureSource` in TypeScript, but the emitted owners are independent. At
Flight `a627849` and flight-compiler `2b687ea`, the module refuses with concrete
`Ref<Bitmap>`/`Ref<ImageResource>` source alternatives against a `Ref<TextureSource>` destination.

Annotating the local or splitting the branch does not create an owner-preserving C++ relationship.
Copying the common fields into a new `TextureSource` would discard the exact resource identity returned
by the decoder or fetch callback. The resolver therefore remains an explicit refusal under this request.

## G2 glyph atlas declaration boundary

The original three headers in `undeclared-identifier-145c75d7` at compiler `806d257` / Flight
`a627849` failed because `create_glyph_source_from_glyph_atlas` calls the refused initializer without
a declaration. The existing `declare-refused-function` mechanism now adds only:

```cpp
void initialize_glyph_source_from_glyph_atlas(
    flight::types::EntityConstruction<flight::Ref<flight::types::GlyphSource>> out,
    flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::GlyphAtlas>>>> atlas);
```

The retained call supplies these exact construction-row and readonly-owner carriers. Its live
same-header call, exact `initializeGlyphSourceFromGlyphAtlas` refusal marker and absent body satisfy
the repair guards, so neither a callback `retainedReference` nor cross-module `calledFrom` is needed.
Normal single-module generation with the pinned inputs reproduced only that declaration and its
repair manifest record; no generated body was edited by hand.

The underlying refusal above remains: page zero must return the atlas's exact bitmap owner through
`TextureSource | null`; other pages return null. The bound methods must retain the same atlas for
rasterize-on-miss glyph entries, kerning, shared metrics and placement revision. Copying common
texture fields, replacing the owner, narrowing the public GlyphSource slot or inventing glyph data
would not implement that contract. No such behavior was supplied.

Scoped Clang 22.1.8 verification at one worker: three selected headers compile, an unused program
including all three links and runs, and a call through the actual retained factory compiles to an
object but fails to link only on `flight::glyphatlas::initialize_glyph_source_from_glyph_atlas`.
This clears the declaration error and keeps the missing body visible. It does not close this
owner-widening request or make the partial initializer/factory usable. See also
[retained callers of omitted callees](retained-declaration-calls-omitted-callee.md).

## G2 GL Scale9 sprite material owner

At Flight `a627849` / compiler `806d257`, `scene2d-gl/glScale9Sprite.ts:26` refuses the entire
`drawGlScale9Sprite` body as `cpp-contextual-union-interface-heritage-carrier-unrepresented`:
nullable `Material2D` widens to `Material` when resolving the proxy's quad material renderer.
Both generated interfaces independently inherit `ReferenceEnabled` and repeat entity runtime key,
kind and three-state name cells. Those matching fields do not create C++ inheritance or permission
to replace the original material. Keep its owner through nullable absence, kind lookup and renderer
selection; a copied Material prefix, unchecked pointer cast or always-default material changes the API.

The source retains a full `Scale9Sprite`, its data/grid/runtime and texture family. After the real
2D/source/positive-size and mapper guards, it resolves the same texture/sampler and working color
space, applies its UV matrix/render-target V reflection, and writes nine row-major affine slices
through the actual batch/material/color pipeline. Geometry/UV writes, scale division, post-prepare
indices and final count must remain those of the original source; no ordinary-Sprite copy or fixed
texture kind is an owner-widening fix.

The retained global renderer references the refused draw. Original1 still fails0/1 at line168,
Clang22.1.8 jobs1. An exact declaration-only actual unused-header candidate instead fails compiling
its create-data callback before link, so no declaration/normal generation or body is supplied.
See [aggregate callback evidence](retained-declaration-calls-omitted-callee.md#g2-gl-scale9-sprite-retains-a-refused-aggregate-callback)
and [native/backend owner carriers](readonly-callback-derived-owner-recovery.md#g2-scale9-sprite-native-sprite-callbacks-are-not-generic-renderer-callbacks).

## G2 GL QuadBatch material owner

At Flight `a627849` / compiler `806d257`, `scene2d-gl/glQuadBatch.ts:26` refuses all of
`submitGlQuadBatch` as `cpp-contextual-union-interface-heritage-carrier-unrepresented` at nullable
Material2D-to-Material widening. The proxy's original material feeds quad-renderer selection and the
same batch's prepare call; matching flattened entity/kind/name fields do not create C++ heritage or
permit a base copy/default material. Preserve owner identity, null and the selected registered renderer.

The complete QuadBatch/data and original atlas/texture, ids/transforms and per-quad materialData must
survive. Source vector2 and affine instance transforms compose with the original parent, atlas regions
retain both clockwise/counterclockwise UV mappings, invalid ids and nonpositive regions do not write
packets, and valid packets stay compact. After prepare can flush, material/color writes use returned
startInstance plus valid drawCount, while per-quad color lookup uses original input index. Preserve
per-quad ColorScaleBias/Tint/readonly-number-array versus node matrix/scale-bias fallback, working color,
sampler/straight alpha, proxy alpha and final valid-draw count. No copied Sprite, material or arrays.

Original1 remains0/1 at missing submit191, Clang22.1.8 jobs1. Its exact declaration-only actual unused
include fails the [NodeAny-to-Sprite callback conversion](interface-heritage-widening.md#g2-gl-quadbatch-create-data-callback)
before link. The [retained aggregate request](retained-declaration-calls-omitted-callee.md#g2-gl-quadbatch-retains-its-refused-submit-callback)
records why no declaration repair, normal generation, fake packet or usable submission claim is carried.

## G2 GL Tilemap material owner

At Flight `a627849` / compiler `806d257`, `scene2d-gl/glTilemap.ts:26` refuses the full
`submitGlTilemap` body as `cpp-contextual-union-interface-heritage-carrier-unrepresented` at nullable
Material2D-to-Material widening. The proxy's original material selects the quad renderer and feeds
the same prepare call. Matching flattened fields do not supply C++ heritage; preserve null, original
owner identity and registered renderer rather than copying a base prefix or substituting a material.

Preserve the original Tilemap/data, atlas/texture, signed Int16Array tiles, per-cell materialData and
actual GL writer runtime. Traverse row-major `row * columns + col`; skip negative/out-of-range ids
and nonpositive atlas regions. Parent affine composition translates by column/row times tile sizes.
Geometry uses tileWidth/tileHeight even for rotated atlas regions; region dimensions and both rotation
directions determine UVs only. Preserve texture/source and zero-dimension guards, working color,
sampler, blend, straight alpha, proxy alpha and original node materialData.

Prepare reserves columns*rows and may flush. Compact valid packet/material/color writes use returned
startInstance plus drawCount, while per-tile color reads the original input cell index. Keep the
ColorScaleBias/Tint/readonly-number-array override and null/absence fallback to node matrix then node
scale-bias, stride and final count increment by actual valid draws. A Sprite copy or narrowed cell,
atlas, color or rotation domain cannot implement this body.

Original1 remains 0/1 at missing submit165, Clang22.1.8 jobs1, 34.9 seconds. An exact-declaration-only
actual unused header fails the [NodeAny-to-Sprite callback conversion](interface-heritage-widening.md#g2-gl-tilemap-create-data-callback)
before link. The [retained aggregate qualification](retained-declaration-calls-omitted-callee.md#g2-gl-tilemap-retains-its-refused-submit-callback)
records why no declaration repair, normal generation, fake packet or usable submission is claimed.

## G2 WGPU Sprite material owner

At Flight `a627849` / compiler `806d257`, `scene2d-wgpu/wgpuSprite.ts:25` refuses the entire
`drawWgpuSprite` body as `cpp-contextual-union-interface-heritage-carrier-unrepresented` at nullable
Material2D-to-Material widening. The original proxy material selects the registered quad renderer and
feeds prepare, whose batch-change comparison uses material reference identity. Independent flattened
owners have no native inheritance cast; preserve that same owner and null rather than copying a
base prefix, changing its declared carrier or substituting a default material.

Preserve the actual WgpuRenderState and mutable package runtime, render-pass guard, original
proxy/Sprite/data/texture, null/non-2d/source guards and both draw paths. A custom shader flushes the
quad writer, applies the resolved blend hook, realizes the original texture with true/working-color
arguments, binds the real shader to state/proxy and returns. The default path checks positive texture
view size and resolves the material kind, registry miss and registered StandardMaterial fallback;
unresolved means no draw, not a built-in shader/material. Both custom and registered material domains
must survive. Preserve actual texture entry/sampler, batch resource acquisition, affine transform,
six UV entries, alpha, materialData and matrix-before-scale-bias color fallback. Packet/material/color
indices come from prepare's returned instanceIndex after any flush; increment the real writer count once.

Stock original1 remains 0/1 at missing draw134, Clang22.1.8 jobs1, 66.3 seconds. The exact-declaration-only
actual unused include fails the [GL-specific native callback conversion](readonly-callback-derived-owner-recovery.md#g2-wgpu-sprite-retains-gl-specific-native-sprite-helpers)
before link. The [retained aggregate request](retained-declaration-calls-omitted-callee.md#g2-wgpu-sprite-retains-its-refused-draw-callback)
records qualification rejection. No declaration repair, generation, fake shader/draw or usable WGPU
Sprite implementation is carried.

## G2 WGPU Scale9Sprite material owner

At Flight `a627849` / compiler `806d257`, `scene2d-wgpu/wgpuScale9Sprite.ts:24` refuses the entire
`drawWgpuScale9Sprite` body as `cpp-contextual-union-interface-heritage-carrier-unrepresented` at nullable
Material2D-to-Material widening. Quad renderer resolution and prepare need the same original material;
prepare compares its reference identity when changing batches. Preserve that owner/null, the registered
kind/miss/standard fallback semantics and no-draw result. Independent flattened structs permit no
inheritance cast or base copy, and a built-in default material would change the source domain.

Keep the actual WGPU state/mutable runtime, render-pass guard, full Scale9Sprite/data/grid/runtime and
texture family with source/non-2d/null checks. The mapper rejects nonpositive scale or bounds; preserve
all grid coordinates and its compressed-border/negative-center math. Map both four-coordinate axes,
emit all nine row-major slices, divide the original affine basis by source scales, translate each
slice origin and compose all six UV coefficients from the original texture matrix. Preserve actual
texture entry/sampler, blend, true/working-color realization, alpha, materialData and matrix-before-
scale-bias fallback. Material/color indices use the runtime count read after prepare plus instance,
then the real writer count increases by nine. No fixed-grid or ordinary-Sprite replacement.

Separate unexecuted source finding: the draw sets writeBase directly to prepare's returned value;
the helper returns instance count and the affine writer consumes a float-element base with stride13.
That mismatch for a compatible nonempty batch is not repaired or proven by this compiler disposition.

Stock original1 stays 0/1 at missing draw164, Clang22.1.8 jobs1, 58.8 seconds. The exact-declaration-only
actual unused header fails the [native/generic callback conversion](readonly-callback-derived-owner-recovery.md#g2-wgpu-scale9sprite-native-sprite-callback-and-owner-boundaries)
before link. See [retained aggregate qualification](retained-declaration-calls-omitted-callee.md#g2-wgpu-scale9sprite-retains-its-refused-draw-callback).
No declaration repair, generation, shader/draw body, owner copy or usable Scale9 implementation is carried.
