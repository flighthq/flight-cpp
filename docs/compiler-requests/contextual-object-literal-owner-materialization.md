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

## Current reach

`@flighthq/textureatlas` `textureAtlasFrom.ts` is the measured current-pin site. Its header is the
only direct textureatlas failure with `no matching function for call to 'create_texture'`; once
isolated with unlimited diagnostics, the same header also reports four separate missing image
loader declarations inherited from the refused `@flighthq/image` module. Those dependency errors
belong to the browser URL/location binding request and are not part of this options-owner defect.

The carrying partial override materializes the exact owner shown above and is exercised against the
upstream identity, fresh-atlas, and empty-region assertions. Together with the separately bounded
URL/base64 subset, it moves the scoped package compile from the emitted 5/7-module state to 7/7
independent headers at two workers. The package remains `partial`, not shippable, because this repair
does not supply the browser-bound blob/byte loaders or the independent guard callback.
