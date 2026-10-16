#include <flight/texture/texture.hpp>

#include <iostream>
#include <type_traits>
#include <variant>

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
  using CubeRow = flight::StructuralRef<
      flight::RowReadonly<flight::RowOf<flight::Ref<types::CreateTextureCubeOptions>>>>;
  using WritableCubeRow = flight::StructuralRef<
      flight::RowWritable<flight::RowOf<flight::Ref<types::CreateTextureCubeOptions>>>>;
  static_assert(!std::is_convertible_v<CubeRow, WritableCubeRow>);

  const auto sampler = texture::create_sampler(std::nullopt);
  sampler->anisotropy = 4.0;
  const auto face = flight::make_ref<types::Bitmap>();
  face->width = 32.0;
  types::TextureSourceCubeFaces sources{
      std::nullopt, types::TextureSourceValue{
          std::in_place_type<flight::Ref<types::Bitmap>>, face},
      std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  const auto opts = flight::make_ref<types::CreateTextureCubeOptions>();
  opts->dimension = flight::String("cube");
  opts->sources = sources;
  opts->sampler = sampler;
  opts->color_space = flight::String("linear");
  opts->flip_x = true;
  opts->version = 4294967297.0;
  const auto offset = flight::geometry::create_vector2(0.25, 0.5);
  const auto scale = flight::geometry::create_vector2(0.5, 0.75);
  opts->uv_offset = offset;
  opts->uv_scale = scale;
  opts->uv_rotation = 0.75;
  const auto resource = flight::make_ref<types::ExternalImageResourceReference>();
  opts->resource = resource;
  const CubeRow row(opts);
  const auto cube = texture::create_texture(row);
  check(cube->dimension == flight::String("cube") &&
            cube->color_space == flight::String("linear") && cube->flip_x,
        "readonly cube options retain dimension, color and flip state");
  check(cube->version == 1.0 && cube->uv_rotation == 0.75,
        "readonly cube options preserve numeric fields and u32 version wrapping");
  check(opts->sampler == sampler && cube->sampler != sampler &&
            cube->sampler->anisotropy == 4.0,
        "construction clones sampling state without writing its options");
  check(cube->uv_offset != offset && cube->uv_offset->x == 0.25 &&
            cube->uv_scale != scale && cube->uv_scale->y == 0.75,
        "construction deep-clones supplied UV owners");
  check(cube->sources.size() == 6 && !cube->sources.element(0.0),
        "construction retains six face slots and empty elements");
  check(&cube->sources.element(0.0) != &sources.element(0.0) &&
            std::get<flight::Ref<types::Bitmap>>(*cube->sources.element(1.0)) == face,
        "construction creates fresh face-list storage with the same source owner");
  sources.element(1.0) = std::nullopt;
  check(cube->sources.element(1.0).has_value(),
        "reassigning an input slot cannot change the returned cube");
  check(resource->textures && resource->textures->size() == 1 &&
            std::get<flight::Ref<types::TextureCube>>(resource->textures->element(0.0)) == cube,
        "resource attachment retains the returned cube exactly once");

  const auto literal = flight::make_structural_ref<CubeRow::schema_type>(
      flight::row_field<flight::RowKey<"dimension">>(flight::String("cube")));
  const auto defaults = texture::create_texture(literal);
  check(defaults->sources.size() == 6 && defaults->color_space == flight::String("srgb") &&
            defaults->sampler && defaults->uv_scale->x == 1.0,
        "the generated structural literal boundary supplies the cube defaults");
  for (const auto& value : defaults->sources) check(!value, "default cube faces are empty");

  const auto empty_opts = flight::make_ref<types::CreateTextureCubeOptions>();
  empty_opts->dimension = flight::String("cube");
  empty_opts->sources = types::TextureSourceCubeFaces{};
  empty_opts->color_space = flight::String("");
  const auto empty_cube = texture::create_texture(CubeRow(empty_opts));
  check(empty_cube->sources.empty() && empty_cube->color_space == flight::String(""),
        "present empty arrays and empty strings retain their supplied values");

  // The shared row helper must retain the existing 2D constructor boundary.
  const auto two_d_row = flight::make_structural_ref<texture::CreateTexture2DOptionsRow::schema_type>(
      flight::row_field<flight::RowKey<"colorSpace">>(
          std::optional<flight::String>{flight::String("linear")}));
  const auto two_d = texture::create_texture_2d(two_d_row);
  check(two_d->dimension == flight::String("2d") &&
            two_d->color_space == flight::String("linear") && !two_d->source,
        "the shared readonly helper retains 2D construction and absence");
  return failures == 0 ? 0 : 1;
}
