# Contextual object literals for overloaded calls need an owned options carrier

At Flight `a62784923f3be814463286c9fb28edfe6ab15789` and flight-compiler
`2b687eaf6b631e0fa4d7d6e18dad93b845f99633`, an object literal passed to an imported overloaded
factory is emitted as a bare C++ designated initializer even though every available overload takes
an owned options reference or a variant of owned options references.

## Source

```ts
export function createTextureAtlasFromImageResource(resource: ImageResource): TextureAtlas {
  return createTextureAtlas({ texture: createTexture({ dimension: '2d', source: resource }) });
}
```

The `dimension: '2d'` discriminant selects the `CreateTexture2DOptions` overload, whose result is a
`Texture2D`.

## Emitted mismatch

```cpp
flight::texture::create_texture({
  .dimension = flight::String("2d"),
  .source = resource,
})
```

Clang rejects this call. The texture module's exact overload takes
`Ref<CreateTexture2DVariantOptions>` and its compatibility overload takes a variant whose arms are
also `Ref<...>` owners. A bare initializer list materializes neither owner. The generated texture
header has the same representation boundary: its `CreateTextureOptionsView` is a variant of owned
option views, not an aggregate accepting those two designators directly.

## Expected lowering

The compiler must use the contextual overload and materialize its exact object-literal owner before
the call, equivalent to:

```cpp
auto opts = flight::make_ref<CreateTexture2DVariantOptions>();
opts->dimension = flight::String("2d");
opts->source = TextureSourceUnion{
    std::in_place_type<flight::Ref<ImageResource>>, resource};
auto texture = flight::texture::create_texture(opts);
```

The `ImageResource` reference must remain the same allocation selected by the source. Copying it
into a base `TextureSource`, or routing through a union arm that loses the exact owner, changes
identity and is not an equivalent repair.

## The TypeScript-selected overload is the authority, not the C++ designators

The arm is fixed in TypeScript before any C++ is emitted, and it must be read from there. Resolving
the designators against the C++ overload set instead is not merely harder, it is unsound: C++ has no
notion of an optional discriminant, so a bare designated-initializer list leaves every options arm
viable and a designator rule would have to guess which factory runs.

`@flighthq/types` `CreateTextureOptions.ts`:

```ts
type CreateTextureVariantOptions<Type extends TextureLike> = Type extends TextureLike
  ? Omit<Partial<Type>, 'dimension'> &
      (Type['dimension'] extends '2d' ? { readonly dimension?: '2d' }
                                      : { readonly dimension: Type['dimension'] })
  : never;
export type CreateTextureOptions = CreateTextureVariantOptions<TextureLike> & {
  readonly resource?: ImageResourceReference | null;
};
export type CreateTexture2DOptions = Extract<CreateTextureOptions, { dimension?: '2d' }>;
```

`@flighthq/texture` `texture.ts:120-123` declares the matching overload set, two-dimensional first:

```ts
export function createTexture(opts?: Readonly<CreateTexture2DOptions>): Texture2D;
export function createTexture(opts: Readonly<CreateTexture2DArrayOptions>): Extract<Texture, { dimension: '2d-array' }>;
export function createTexture(opts: Readonly<CreateTexture3DOptions>): Extract<Texture, { dimension: '3d' }>;
export function createTexture(opts: Readonly<CreateTextureCubeOptions>): Extract<Texture, { dimension: 'cube' }>;
```

The union is discriminated on `dimension`, and the two-dimensional arm is the only one whose
discriminant is optional; the other three require a literal. TypeScript resolves overloads in order,
so a literal that carries no `dimension` makes those three non-viable and binds the two-dimensional
overload. Every site below therefore has exactly one statically selected arm, and the lowering is
`make_ref` of that arm's options type. This also fixes what the emitted call must return: the
two-dimensional overload returns `Texture2D`, which is the variant arm the call sites already
construct.

## Earlier reach at 2b687ea

`@flighthq/textureatlas` `textureAtlasFrom.ts` is the measured current-pin site. Its header is the
only direct textureatlas failure with `no matching function for call to 'create_texture'`; once
isolated with unlimited diagnostics, the same header also reports four separate missing image
loader declarations inherited from the refused `@flighthq/image` module. Those dependency errors
belong to the browser URL/location binding request and are not part of this options-owner defect.

Seven further call sites are now verified to fail the same way, each compiled as its own translation unit
with the repository's real include order (`-I overrides/include -I generated/include -I include`) and
`-ferror-limit=0`, with the site's own diagnostic present and no fatal error before it. None of these
paths is shadowed by an override, and `deferred-packages.json` is empty, so every one is held to the
compile gate.

| Site | Emitted literal | Selected arm |
| --- | --- | --- |
| `bitmaptext/update_bitmap_text.hpp:121` | `{.dimension = String("2d"), .source = image.value()}` | 2D (`dimension` literal) |
| `scene2d_formats/lottie_image_layer.hpp:99` | `{.dimension = String("2d"), .source = image.value()}` | 2D (`dimension` literal) |
| `scene2d_formats/rive_asset_binding.hpp:106` | `{.dimension = String("2d"), .source = nullptr}` | 2D (`dimension` literal; `null` is the documented 2D sentinel) |
| `scene3d_formats/shared.hpp:100` and `:110` | `{.resource = resource.value()}` | 2D (only arm with an optional discriminant) |
| `swf/swf_video_handler.hpp:72` | `{.sampler = create_sampler(...)}` | 2D (only arm with an optional discriminant) |
| `render_wgpu/wgpu_external_texture.hpp:105` | `{.color_space = ..., .sampler = ..., .dimension = String("2d"), .source = source}` | 2D (`dimension` literal) |

`swf_video_handler.hpp:72` is the cleanest instance in the tree: compiling that header yields exactly
one error and nothing else, so it isolates the defect with no package context. Its literal names
only `.sampler`, which all four options shapes carry identically, so the designators cannot
place the arm; the TypeScript discriminant does.

The `null` source at `rive_asset_binding.hpp:106` is not an unknown and must not be treated as one:
`texture.ts` records that two-dimensional content "retains a null source sentinel during the loader
migration", so a null source is a valid two-dimensional form and the `dimension: '2d'` discriminant
still selects the arm.

### One site is not verified

`scene3d_formats/gltf_parse.hpp:813` carries `{.resource = resource.value()}` and by the same
TypeScript reasoning is two-dimensional, but it is **not** a verified compile failure: its translation
unit dies on a fatal error before reaching the call, because
`overrides/include/flight/scene3d/mesh.hpp:15` includes `<flight/types/material_value.hpp>`, which
exists nowhere in the repository. Its arm is determined; its failure is untested. Those are different
claims and this request makes only the first.

## Minimal regression cases

A focused regression should cover the shapes that distinguish "selected by a literal discriminant"
from "selected because no other arm is viable", and should assert the materialized owner rather than
only that the call compiles.

```ts
// selected by the literal discriminant
createTexture({ dimension: '2d', source: bitmap });

// selected because the discriminant is optional and the other arms require one
createTexture({ resource: resource });
createTexture({ sampler: sampler });

// the documented 2D null sentinel -- still the 2D arm
createTexture({ dimension: '2d', source: null });
```

The first three shapes are the ones the verified sites actually emit. Each must lower to
`make_ref` of the two-dimensional options owner with the same source allocation, and a case that
selects a non-2D arm by its literal discriminant (`dimension: '3d'`, `'cube'`, `'2d-array'`) must
materialize *that* arm's owner instead -- the lowering follows the selected overload, it does not
hard-code two dimensions.

No ambiguity negative case is proposed here. An invented one would assert the absence of an arm that
the TypeScript checker in fact resolves, which is the opposite of the truth; a negative control is
only meaningful against a shape the checker genuinely cannot resolve, and no such instance of this
call was found.

## Investigation lead, not established feasibility

`packages/compiler-backend-cpp/src/cppCompilerBackend.ts:22907` calls
`getIrInvocationArgumentExpectedTypeCpp(expression, index)`, so the emitter already computes an
invocation argument's expected type. That is a hook that appears to be the right place to read the
selected options carrier. It is recorded as a lead only: no prototype was built or verified, so
nothing here establishes that the change is reachable without wider emitter design, and no claim is
made about evaluation order, allocation count, or identity preservation beyond the requirement stated
under "Expected lowering".

## Carrying override and package tier

The carrying partial override materializes the exact owner shown above and is exercised against the
upstream identity, fresh-atlas, and empty-region assertions. Together with the separately bounded
URL/base64 subset, it moves the scoped package compile from the emitted 5/7-module state to 7/7
independent headers at two workers. The package remains `partial`, not shippable, because this repair
does not supply the browser-bound blob/byte loaders or the independent guard callback.

## G2: the reserved 27-header cluster at 806d257

After E0a at compiler `806d257` / Flight `a627849`, the original 27 rows in
`call-has-no-matching-3882b3e9` were each compiled with Clang 22.1.8, one worker and
`-Wfatal-errors`. **0/27 compile**. Their actual first errors are:

| first site | selected headers | source literal |
| --- | --- | --- |
| `scene3d_formats/shared.hpp:100` | 17 | `{ resource }` |
| `scene2d_formats/rive_asset_binding.hpp:106` | 6 | `{ dimension: '2d', source: null }` |
| `bitmaptext/update_bitmap_text.hpp:121` | 4 | `{ dimension: '2d', source: image }` |

The old cluster description attributed these to the WebGPU device's variadic `create_texture`.
The actual function is `flight::texture::create_texture`; the current native override aliases
`CreateTexture2DVariantOptions` to canonical `types::CreateTexture2DOptions` and accepts its exact
owned reference (also through the optional compatibility entry). The emitted braces build neither
that owner nor the readonly view. This is the contextual owner construction defect above, so the
worklist now links this request and routes the cluster to the compiler.

The 2D choice comes from the existing TypeScript overload set: explicit `'2d'` selects it, and
`{ resource }` can use only the arm whose discriminant is optional. The Rive `null` source remains the
authored 2D null sentinel. No blanket descriptor overload, copied source/resource owner, source patch
or new compiler prototype is carried by this audit. All selected headers and their options/factory
files are unchanged from check start `1fe98c66` through delivered `c343d73a`; no unattempted row or
crashed compiler is counted as a source failure.

The older `gltf_parse.hpp` fatal-include observation above is historical. In the separate current-pin
29-header Traits audit, that header fails first at `scene3d/scene_document.hpp:106` on the free
`Traits` parameter. Its own texture call remains unreached and is not added to these 27 measured
texture failures. G1 sweep totals and all unowned rows remain unchanged.
