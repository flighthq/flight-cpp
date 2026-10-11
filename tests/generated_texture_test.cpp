#include <flight/texture/texture.hpp>

#include <iostream>
#include <variant>

namespace {

int failures = 0;

void check(bool condition, const char* message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

}  // namespace

int main() {
  namespace texture = flight::texture;
  namespace types = flight::types;

  const auto defaults = texture::create_texture();
  check(defaults->dimension == flight::String("2d"), "the default constructor selects 2D");
  check(defaults->color_space == flight::String("srgb"), "the default color space is sRGB");
  check(defaults->sampler != nullptr, "the default constructor creates a sampler");
  check(defaults->uv_offset->x == 0.0 && defaults->uv_offset->y == 0.0,
        "the default UV offset is zero");
  check(defaults->uv_scale->x == 1.0 && defaults->uv_scale->y == 1.0,
        "the default UV scale is one");
  check(!defaults->source.has_value(), "the default 2D source is absent");

  const auto direct = flight::make_ref<types::Texture2D>();
  auto direct_row = flight::StructuralRef<
      flight::RowWritable<flight::RowOf<flight::Ref<types::Texture2D>>>>(direct);
  flight::row_set<flight::RowKey<"dimension">>(direct_row, flight::String("2d"));
  flight::row_set<flight::RowKey<"sampler">>(direct_row, defaults->sampler);
  check(direct->dimension == flight::String("2d") && direct->sampler == defaults->sampler,
        "the generated structural table retains construction-only texture fields");

  const auto source = flight::make_ref<types::TextureSource>();
  source->kind = flight::String("image");
  source->width = 32.0;
  source->height = 16.0;
  const auto sampler = flight::texture::create_sampler(std::nullopt);
  sampler->anisotropy = 4.0;
  const auto offset = flight::geometry::create_vector2(0.25, 0.5);
  const auto scale = flight::geometry::create_vector2(0.5, 0.75);
  const auto resource = flight::make_ref<types::ExternalImageResourceReference>();

  const auto options = flight::make_ref<types::CreateTexture2DOptions>();
  options->source = decltype(types::CreateTexture2DOptions::source){
      std::in_place_type<flight::Ref<types::TextureSource>>, source};
  options->sampler = sampler;
  options->uv_offset = offset;
  options->uv_scale = scale;
  options->version = 7.0;
  options->resource = decltype(types::CreateTexture2DOptions::resource){
      std::in_place_type<flight::Ref<types::ExternalImageResourceReference>>, resource};
  const auto two_d = texture::create_texture2_d(options);
  check(two_d->source.has_value() &&
            std::get<flight::Ref<types::TextureSource>>(two_d->source.value()) == source,
        "2D construction retains the source owner");
  check(two_d->sampler != sampler && two_d->sampler->anisotropy == 4.0,
        "2D construction deep-clones the sampler");
  check(two_d->uv_offset != offset && two_d->uv_offset->x == 0.25,
        "2D construction deep-clones the UV offset");
  check(two_d->uv_scale != scale && two_d->uv_scale->y == 0.75,
        "2D construction deep-clones the UV scale");
  check(resource->textures.has_value() && resource->textures->size() == 1,
        "2D construction attaches once to its resource");
  check(std::get<flight::Ref<types::Texture2D>>(resource->textures->get(0.0).value()) == two_d,
        "resource attachment retains the same 2D texture owner");

  const auto array_options = flight::make_ref<texture::CreateTexture2DArrayVariantOptions>();
  array_options->dimension = flight::String("2d-array");
  array_options->sources = flight::Array<std::optional<types::TextureSourceValue>>{
      types::TextureSourceValue{
          std::in_place_type<flight::Ref<types::TextureSource>>, source}};
  const auto array_texture = texture::create_texture(array_options);
  const auto array_source = array_texture->sources.get(0.0);
  check(array_texture->dimension == flight::String("2d-array") &&
            array_texture->sources.size() == 1 && array_source.has_value() &&
            array_source.value().has_value() &&
            std::get<flight::Ref<types::TextureSource>>(
                array_source.value().value()) == source,
        "2D-array construction preserves its source list");

  const auto voxel = flight::make_ref<types::VoxelGrid>();
  const auto three_d_options = flight::make_ref<texture::CreateTexture3DVariantOptions>();
  three_d_options->dimension = flight::String("3d");
  three_d_options->source = decltype(texture::CreateTexture3DVariantOptions::source){
      std::in_place_type<flight::Ref<types::VoxelGrid>>, voxel};
  const auto three_d = texture::create_texture(three_d_options);
  check(three_d->dimension == flight::String("3d") && three_d->source == voxel,
        "3D construction preserves the voxel source owner");

  const auto cube_options = flight::make_ref<texture::CreateTextureCubeVariantOptions>();
  cube_options->dimension = flight::String("cube");
  const auto cube = texture::create_texture(cube_options);
  check(cube->dimension == flight::String("cube") && cube->sources.size() == 6,
        "cube construction supplies six absent faces by default");

  const auto source_view = flight::make_ref<types::Texture2D>();
  source_view->color_space = flight::String("linear");
  source_view->flip_x = true;
  source_view->flip_y = false;
  source_view->sampler = sampler;
  source_view->uv_offset = offset;
  source_view->uv_rotation = 0.75;
  source_view->uv_scale = scale;
  source_view->version = 9.0;
  source_view->dimension = flight::String("2d");
  source_view->source = source;
  types::TextureLike source_union{
      std::in_place_type<flight::Ref<types::Texture2D>>, source_view};
  const auto clone_union = texture::clone_texture(source_union);
  const auto clone = std::get<flight::Ref<types::Texture2D>>(clone_union);
  check(clone->source == source_view->source, "clone retains the source owner");
  check(clone->sampler != sampler && clone->sampler->anisotropy == sampler->anisotropy,
        "clone deep-copies sampler state");
  check(clone->uv_offset != offset && clone->uv_scale != scale,
        "clone deep-copies both UV vectors");
  check(clone->color_space == flight::String("linear") && clone->version == 9.0,
        "clone preserves common scalar state");

  if (failures == 0) std::cout << "texture construction and cloning match Flight\n";
  return failures == 0 ? 0 : 1;
}
