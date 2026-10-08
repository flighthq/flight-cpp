// Complete native lowering of canvasImageTextureResolver.ts at the pinned Flight revision.
#pragma once

#include <memory>
#include <optional>
#include <variant>

#include <flight/host_sdl/image.hpp>
#include <flight/scene2d_canvas/canvas_texture_resolver.hpp>
#include <flight/types/canvas_texture_resolver.hpp>
#include <flight/types/image_resource.hpp>
#include <flight/types/texture.hpp>
#include <flight/types/texture_source_kind.hpp>

namespace flight::scene2d_canvas {

inline std::optional<flight::host_sdl::ImageSource> resolve_canvas_image_texture(
    flight::Ref<flight::types::CanvasTextureResolvers>,
    flight::types::Texture texture) {
  const auto* texture_2d = std::get_if<flight::Ref<flight::types::Texture2D>>(&texture);
  if (!texture_2d || !*texture_2d || !(*texture_2d)->source) return std::nullopt;

  const auto& source = *(*texture_2d)->source;
  if (!source || source->kind != flight::types::image_texture_source_kind) return std::nullopt;

  // ImageResource extends TextureSource in TypeScript. The generated owners are flattened sibling
  // structs, but Texture2D retains an aliasing shared_ptr to the original ImageResource allocation.
  // The registry kind is the checked discriminant that authorizes recovering that exact owner.
  const auto image = std::reinterpret_pointer_cast<flight::types::ImageResource>(source);
  if (!image || !image->source) return std::nullopt;
  return image->source;
}

inline void register_canvas_image_texture_resolver(
    flight::Ref<flight::types::CanvasTextureResolvers> resolvers) {
  register_canvas_texture_resolver(
      resolvers,
      flight::types::image_texture_source_kind,
      std::optional<flight::types::CanvasTextureResolver>{resolve_canvas_image_texture});
}

}  // namespace flight::scene2d_canvas
