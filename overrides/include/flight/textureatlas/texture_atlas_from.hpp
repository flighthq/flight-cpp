// Faithful native subset of textureAtlasFrom.ts at Flight a627849.
#pragma once

#include <flight/abort.hpp>
#include <flight/structural_ref.hpp>
#include <flight/task.hpp>
#include <flight/runtime.hpp>

#include <flight/texture/texture.hpp>
#include <flight/textureatlas/texture_atlas.hpp>
#include <flight/types/image_resource.hpp>
#include <flight/types/texture.hpp>

namespace flight::textureatlas {

inline flight::Ref<flight::types::TextureAtlas> create_texture_atlas_from_image_resource(
    flight::Ref<flight::types::ImageResource> resource) {
  auto texture_options = flight::make_ref<flight::texture::CreateTexture2DVariantOptions>();
  texture_options->dimension = flight::String("2d");
  texture_options->source = decltype(texture_options->source){
      std::in_place_type<flight::Ref<flight::types::ImageResource>>, resource};
  const auto texture = flight::texture::create_texture(texture_options);

  auto atlas_options = flight::make_ref<
      entity_runtime_key_image_height_image_name_image_width_regions_scale_texture_043bb97832a671f4>();
  atlas_options->texture = decltype(atlas_options->texture){
      std::in_place_type<flight::Ref<flight::types::Texture2D>>, texture};
  return create_texture_atlas(atlas_options);
}

inline flight::Task<flight::Ref<flight::types::TextureAtlas>> load_texture_atlas_from_url(
    flight::StructuralRef<flight::RowReadonly<flight::RowOf<
        flight::Ref<flight::types::HostImageCapability>>>> host_image,
    flight::String url,
    std::optional<flight::String> cross_origin = std::nullopt,
    std::optional<flight::AbortSignal> signal = std::nullopt) {
  const auto resource = co_await flight::row_get<flight::RowKey<"loadImageFromUrl">>(host_image)(
      url, cross_origin, signal);
  co_return create_texture_atlas_from_image_resource(resource);
}

inline flight::Task<flight::Ref<flight::types::TextureAtlas>> load_texture_atlas_from_base64(
    flight::StructuralRef<flight::RowReadonly<flight::RowOf<
        flight::Ref<flight::types::HostImageCapability>>>> host_image,
    flight::String base64,
    flight::String mime_type,
    std::optional<flight::AbortSignal> signal = std::nullopt) {
  co_return co_await load_texture_atlas_from_url(
      host_image,
      flight::String("data:") + mime_type + flight::String(";base64,") + base64,
      std::nullopt,
      signal);
}

// loadTextureAtlasFromBlob and loadTextureAtlasFromBytes remain intentionally absent: the pinned
// runtime has no browser object-URL creation/revocation contract, so their finally-settled lifetime
// cannot be reproduced faithfully in this package.

} // namespace flight::textureatlas
