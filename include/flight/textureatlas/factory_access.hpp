#pragma once

#include <flight/runtime.hpp>
#include <flight/types/texture.hpp>
#include <flight/types/texture_atlas.hpp>
#include <flight/types/texture_atlas_region.hpp>

namespace flight::textureatlas {
namespace factory_access_detail {

inline std::optional<flight::String> image_name(
    const std::variant<flight::String, flight::Null, flight::Undefined>& value) {
  const auto* present = std::get_if<flight::String>(&value);
  return present == nullptr ? std::nullopt : std::optional<flight::String>{*present};
}

inline std::optional<double> nullable_number(
    const std::variant<double, flight::Null, flight::Undefined>& value) {
  const auto* present = std::get_if<double>(&value);
  return present == nullptr ? std::nullopt : std::optional<double>{*present};
}

inline std::optional<flight::Ref<flight::types::Texture2D>> texture(
    const std::variant<flight::Ref<flight::types::Texture2D>, flight::Null,
                       flight::Undefined>& value) {
  const auto* present = std::get_if<flight::Ref<flight::types::Texture2D>>(&value);
  return present == nullptr
             ? std::nullopt
             : std::optional<flight::Ref<flight::types::Texture2D>>{*present};
}

inline flight::Ref<flight::types::TextureAtlas> empty() {
  return flight::make_ref<flight::types::TextureAtlas>(flight::types::TextureAtlas{
      .image_height = 0.0,
      .image_name = std::nullopt,
      .image_width = 0.0,
      .regions = {},
      .scale = 1.0,
      .texture = std::nullopt,
  });
}

} // namespace factory_access_detail

inline flight::Ref<flight::types::TextureAtlas> create_texture_atlas(std::nullopt_t) {
  return factory_access_detail::empty();
}

inline flight::Ref<flight::types::TextureAtlasRegion> create_texture_atlas_region(
    std::nullopt_t) {
  return flight::make_ref<flight::types::TextureAtlasRegion>(
      flight::types::TextureAtlasRegion{
          .height = 0.0,
          .id = -1.0,
          .name = std::nullopt,
          .original_height = std::nullopt,
          .original_width = std::nullopt,
          .page_name = std::nullopt,
          .pivot_x = std::nullopt,
          .pivot_y = std::nullopt,
          .rotation = flight::types::TextureAtlasRotation::None,
          .source_x = 0.0,
          .source_y = 0.0,
          .trimmed = false,
          .x = 0.0,
          .y = 0.0,
          .width = 0.0,
      });
}

template <typename Options>
inline flight::Ref<flight::types::TextureAtlasRegion> create_texture_atlas_region(
    const std::shared_ptr<Options>& options) {
  return flight::make_ref<flight::types::TextureAtlasRegion>(
      flight::types::TextureAtlasRegion{
          .height = options->height.value_or(0.0),
          .id = options->id.value_or(-1.0),
          .name = factory_access_detail::image_name(options->name),
          .original_height =
              factory_access_detail::nullable_number(options->original_height),
          .original_width =
              factory_access_detail::nullable_number(options->original_width),
          .page_name = factory_access_detail::image_name(options->page_name),
          .pivot_x = factory_access_detail::nullable_number(options->pivot_x),
          .pivot_y = factory_access_detail::nullable_number(options->pivot_y),
          .rotation = options->rotation.value_or(
              flight::types::TextureAtlasRotation::None),
          .source_x = options->source_x.value_or(0.0),
          .source_y = options->source_y.value_or(0.0),
          .trimmed = options->trimmed.value_or(false),
          .x = options->x.value_or(0.0),
          .y = options->y.value_or(0.0),
          .width = options->width.value_or(0.0),
      });
}

template <typename Options>
inline flight::Ref<flight::types::TextureAtlas> create_texture_atlas(
    const std::shared_ptr<Options>& options) {
  return flight::make_ref<flight::types::TextureAtlas>(flight::types::TextureAtlas{
      .image_height = options->image_height.value_or(0.0),
      .image_name = factory_access_detail::image_name(options->image_name),
      .image_width = options->image_width.value_or(0.0),
      .regions = options->regions.value_or(
          flight::Array<flight::Ref<flight::types::TextureAtlasRegion>>{}),
      .scale = options->scale.value_or(1.0),
      .texture = factory_access_detail::texture(options->texture),
  });
}

} // namespace flight::textureatlas

#include <flight/textureatlas/texture_atlas_grid.hpp>
