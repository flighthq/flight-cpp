#include <flight/texture/cube_texture.hpp>

#include <iostream>
#include <type_traits>
#include <variant>
#include <vector>

namespace {
int failures = 0;
void check(bool value, const char* message) {
  if (value) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}
}

int main() {
  namespace texture = flight::texture;
  namespace types = flight::types;
  const auto face = flight::make_ref<types::TextureSource>();
  face->width = 64.0;
  const types::TextureSourceValue value{
      std::in_place_type<flight::Ref<types::TextureSource>>, face};
  const auto cube = texture::create_cube_texture();
  check(cube->sources.size() == 6, "cube construction retains six default face slots");
  check(!texture::is_cube_texture_complete(cube), "six null faces are incomplete");
  check(texture::get_cube_texture_face_size(cube) == -1.0,
        "six null faces have no face size");
  cube->sources = types::TextureSourceCubeFaces{value, value, value, value, value, value};
  check(texture::is_cube_texture_complete(cube), "six populated faces are complete");
  for (double missing = 0; missing < 6; ++missing) {
    cube->sources.element(missing) = std::nullopt;
    check(!texture::is_cube_texture_complete(cube), "each individual null face is incomplete");
    std::vector<double> visited;
    const bool all = cube->sources.every([&](const auto& entry, double index) {
      visited.push_back(index);
      return entry.has_value();
    });
    check(!all && visited.size() == static_cast<std::size_t>(missing + 1) &&
              visited.back() == missing,
          "the Array.every primitive visits in order and stops at the first null");
    cube->sources.element(missing) = value;
  }

  // Upstream every observes existing elements, rather than reading six indices unconditionally.
  cube->sources = types::TextureSourceCubeFaces{};
  check(texture::is_cube_texture_complete(cube), "empty face arrays are vacuously complete upstream");
  check(!cube->sources.get(0.0), "an out-of-range read has an undefined bounds state");
  cube->sources = types::TextureSourceCubeFaces{value};
  check(texture::is_cube_texture_complete(cube), "a short array with no null face is complete upstream");
  cube->sources = types::TextureSourceCubeFaces{std::nullopt};
  check(!texture::is_cube_texture_complete(cube), "a short array with a null face is incomplete");
  check(cube->sources.get(0.0).has_value() && !cube->sources.get(0.0)->has_value() &&
            !cube->sources.get(1.0).has_value(),
        "in-range null and an out-of-range undefined read stay distinct");
  cube->sources = types::TextureSourceCubeFaces{value, value, value, value, value, value, std::nullopt};
  check(!texture::is_cube_texture_complete(cube), "upstream every also observes a seventh null element");

  cube->sources = types::TextureSourceCubeFaces{std::nullopt, value, std::nullopt,
                                               std::nullopt, std::nullopt, std::nullopt};
  check(texture::get_cube_texture_face_size(cube) == 64.0,
        "face size selects the first non-null face in order");
  int width_failures = 0;
  const auto check_width = [&]<typename Owner>(double width) {
    const auto owner = flight::make_ref<Owner>();
    owner->width = width;
    cube->sources.element(0.0) = types::TextureSourceValue{
        std::in_place_type<flight::Ref<Owner>>, owner};
    if (texture::get_cube_texture_face_size(cube) != width) ++width_failures;
    owner->width += 1.0;
    if (texture::get_cube_texture_face_size(cube) != width + 1.0) ++width_failures;
    if (std::get<flight::Ref<Owner>>(*cube->sources.element(0.0)) != owner) ++width_failures;
  };
  check_width.template operator()<types::VoxelGrid>(11.0);
  check_width.template operator()<types::TextureSource>(12.0);
  check_width.template operator()<types::ExternalTexture>(13.0);
  check_width.template operator()<types::Bitmap>(14.0);
  check_width.template operator()<types::CompressedImageResource>(15.0);
  check_width.template operator()<types::RenderTarget>(16.0);
  check_width.template operator()<types::ImageResource>(17.0);
  check(width_failures == 0, "width reads visit each of the seven retained concrete owners");
  return failures == 0 ? 0 : 1;
}
