#include <flight/texture/cube_texture.hpp>
#include <flight/texture/texture_color_space.hpp>
#include <flight/texture/video_texture.hpp>
#include <flight/texture/voxel_grid.hpp>
#include <flight/types/render_target.hpp>

#include <array>
#include <cmath>
#include <iostream>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

namespace {

int failures = 0;

void check(bool condition, const char* message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

}  // namespace

int main(int argc, char** argv) {
  namespace texture = flight::texture;
  namespace types = flight::types;
  const std::string mutation = argc > 1 ? argv[1] : "";
  const auto near = [](double actual, double expected) {
    return std::abs(actual - expected) <= 1e-5;
  };
  const auto apply_uv_matrix = [](const flight::Ref<types::Matrix3>& matrix,
                                  double u, double v) {
    return std::array<double, 2>{
        matrix->m.get_index(0.0) * u + matrix->m.get_index(3.0) * v +
            matrix->m.get_index(6.0),
        matrix->m.get_index(1.0) * u + matrix->m.get_index(4.0) * v +
            matrix->m.get_index(7.0)};
  };
  const auto make_video_resource = [](std::size_t width, std::size_t height) {
    const auto resource = flight::make_ref<types::VideoResource>();
    resource->element = flight::host_sdl::ImageSource::rgba8(
        width, height, flight::Uint8ClampedArray(width * height * 4),
        flight::host_sdl::ImageSourceKind::video_element);
    return resource;
  };

  const auto defaults = texture::create_texture();
  check(defaults->dimension == flight::String("2d"), "the default constructor selects 2D");
  check(defaults->color_space == flight::String("srgb"), "the default color space is sRGB");
  check(defaults->sampler != nullptr, "the default constructor creates a sampler");
  check(defaults->uv_offset->x == 0.0 && defaults->uv_offset->y == 0.0,
        "the default UV offset is zero");
  check(defaults->uv_scale->x == 1.0 && defaults->uv_scale->y == 1.0,
        "the default UV scale is one");
  check(!defaults->source.has_value(), "the default 2D source is absent");

  const auto structural_options = flight::make_structural_ref<flight::RowReadonly<
      flight::RowOf<flight::Ref<types::CreateTexture2DOptions>>>>(
      flight::row_field<flight::RowKey<"sampler">>(
          std::optional<flight::Ref<types::Sampler>>{defaults->sampler}));
  const auto structural = texture::create_texture2_d(structural_options);
  check(structural->dimension == flight::String("2d"),
        "the structural 2D constructor selects 2D");
  check(structural->sampler != defaults->sampler,
        "the structural 2D constructor clones supplied sampling state");
  check(!structural->source.has_value(),
        "the structural 2D constructor defaults an omitted source");

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

  const auto initialize_sampler = texture::create_sampler(std::nullopt);
  initialize_sampler->anisotropy = 8.0;
  const auto initialize_offset = flight::geometry::create_vector2(2.0, 3.0);
  const auto initialize_scale = flight::geometry::create_vector2(4.0, 5.0);
  const auto initialize_options = flight::make_ref<types::CreateTexture2DOptions>();
  initialize_options->color_space = flight::String("linear");
  initialize_options->flip_x = true;
  initialize_options->sampler = initialize_sampler;
  initialize_options->source = decltype(types::CreateTexture2DOptions::source){
      std::in_place_type<flight::Ref<types::TextureSource>>, source};
  initialize_options->uv_offset = initialize_offset;
  initialize_options->uv_rotation = 0.75;
  initialize_options->uv_scale = initialize_scale;
  initialize_options->version = 13.0;
  auto construction =
      flight::entity::allocate_entity<flight::Ref<types::Texture2D>>();
  const auto construction_owner = construction.shared_owner();
  texture::initialize_texture2_d(
      construction,
      std::optional<flight::Ref<types::CreateTexture2DOptions>>{initialize_options});
  const auto initialized =
      flight::entity::finish_entity<flight::Ref<types::Texture2D>>(construction);
  using WritableTexture2DView = flight::StructuralRef<
      flight::RowWritable<flight::RowOf<flight::Ref<types::Texture2D>>>>;
  const WritableTexture2DView initialized_view(initialized);
  bool initializer_kept_view = initialized_view.shared_owner() == construction_owner;
  if (mutation == "--mutate-initialize-view") initializer_kept_view = false;
  if (mutation == "--mutate-initialize-copy") {
    initialized->uv_offset = initialize_offset;
  }
  check(initializer_kept_view,
        "initializeTexture2D writes through the allocation view retained by finishEntity");
  check(initialized->dimension == flight::String("2d") &&
            initialized->color_space == flight::String("linear") && initialized->flip_x &&
            initialized->uv_rotation == 0.75 && initialized->version == 13.0,
        "initializeTexture2D writes every scalar into the construction owner");
  check(initialized->sampler != initialize_sampler &&
            initialized->sampler->anisotropy == 8.0 &&
            initialized->uv_offset != initialize_offset && initialized->uv_offset->x == 2.0 &&
            initialized->uv_scale != initialize_scale && initialized->uv_scale->y == 5.0,
        "initializeTexture2D owns cloned sampler and UV state");
  check(initialized->source.has_value() &&
            std::get<flight::Ref<types::TextureSource>>(initialized->source.value()) == source,
        "initializeTexture2D retains the supplied source owner as a shared identity");

  const auto copy_options = flight::make_ref<types::CreateTexture2DOptions>();
  copy_options->color_space = flight::String("linear");
  copy_options->flip_x = true;
  copy_options->sampler = sampler;
  copy_options->source = decltype(types::CreateTexture2DOptions::source){
      std::in_place_type<flight::Ref<types::TextureSource>>, source};
  copy_options->uv_offset = offset;
  copy_options->uv_rotation = 1.0;
  copy_options->uv_scale = scale;
  copy_options->version = 21.0;
  const auto copy_source = texture::create_texture2_d(copy_options);
  const auto copy_out = texture::create_texture();
  const auto copy_out_sampler = copy_out->sampler;
  const auto copy_out_offset = copy_out->uv_offset;
  const auto copy_out_scale = copy_out->uv_scale;
  types::TextureLike copy_source_view{
      std::in_place_type<flight::Ref<types::Texture2D>>, copy_source};
  types::TextureLike copy_out_view{
      std::in_place_type<flight::Ref<types::Texture2D>>, copy_out};
  texture::copy_texture(copy_out_view, copy_source_view);
  if (mutation == "--mutate-copy-owner") copy_out->uv_offset = copy_source->uv_offset;
  check(copy_out->sampler == copy_out_sampler && copy_out->uv_offset == copy_out_offset &&
            copy_out->uv_scale == copy_out_scale && copy_out->sampler != copy_source->sampler &&
            copy_out->uv_offset != copy_source->uv_offset &&
            copy_out->uv_scale != copy_source->uv_scale,
        "copyTexture preserves destination sampling owners without aliasing source state");
  check(copy_out->color_space == flight::String("linear") && copy_out->flip_x &&
            copy_out->uv_rotation == 1.0 && copy_out->uv_scale->x == 0.5 &&
            copy_out->version == 21.0 && copy_out->source == copy_source->source,
        "copyTexture writes every common field and shares only the source owner");
  copy_source->uv_offset->x = 0.9;
  check(copy_out->uv_offset->x == 0.25,
        "source sampling mutation does not leak into the owning destination copy");

  const auto alias_options = flight::make_ref<types::CreateTexture2DOptions>();
  alias_options->color_space = flight::String("linear");
  alias_options->source = decltype(types::CreateTexture2DOptions::source){
      std::in_place_type<flight::Ref<types::TextureSource>>, source};
  alias_options->uv_rotation = 2.0;
  alias_options->uv_scale = flight::geometry::create_vector2(7.0, 8.0);
  const auto alias_texture = texture::create_texture2_d(alias_options);
  const auto alias_sampler = alias_texture->sampler;
  const auto alias_offset = alias_texture->uv_offset;
  const auto alias_scale = alias_texture->uv_scale;
  types::TextureLike alias_view{
      std::in_place_type<flight::Ref<types::Texture2D>>, alias_texture};
  texture::copy_texture(alias_view, alias_view);
  if (mutation == "--mutate-alias-copy") alias_texture->uv_scale->x = 0.0;
  check(alias_texture->sampler == alias_sampler && alias_texture->uv_offset == alias_offset &&
            alias_texture->uv_scale == alias_scale &&
            alias_texture->color_space == flight::String("linear") &&
            alias_texture->uv_rotation == 2.0 && alias_texture->uv_scale->x == 7.0 &&
            alias_texture->source.has_value() &&
            std::get<flight::Ref<types::TextureSource>>(alias_texture->source.value()) == source,
        "copyTexture is safe when destination and source are the same owner");

  const auto copy_array_source_options =
      flight::make_ref<texture::CreateTexture2DArrayVariantOptions>();
  copy_array_source_options->dimension = flight::String("2d-array");
  copy_array_source_options->sources =
      flight::Array<std::optional<types::TextureSourceValue>>{
          types::TextureSourceValue{
              std::in_place_type<flight::Ref<types::TextureSource>>, source}};
  const auto copy_array_source = texture::create_texture(copy_array_source_options);
  const auto copy_array_out_options =
      flight::make_ref<texture::CreateTexture2DArrayVariantOptions>();
  copy_array_out_options->dimension = flight::String("2d-array");
  const auto copy_array_out = texture::create_texture(copy_array_out_options);
  types::TextureLike copy_array_source_view{
      std::in_place_type<flight::Ref<types::Texture2DArray>>, copy_array_source};
  types::TextureLike copy_array_out_view{
      std::in_place_type<flight::Ref<types::Texture2DArray>>, copy_array_out};
  texture::copy_texture(copy_array_out_view, copy_array_source_view);
  if (mutation == "--mutate-copy-list") {
    copy_array_out->sources = copy_array_source->sources;
  }
  check(copy_array_out->sources.size() == 1 &&
            copy_array_out->sources.get(0.0).has_value() &&
            copy_array_out->sources.get(0.0).value().has_value() &&
            std::get<flight::Ref<types::TextureSource>>(
                copy_array_out->sources.get(0.0).value().value()) == source,
        "copyTexture retains exact owners in copied source lists");
  copy_array_out->sources[0] = std::nullopt;
  check(copy_array_source->sources.get(0.0).has_value() &&
            copy_array_source->sources.get(0.0).value().has_value(),
        "copyTexture gives array textures fresh list storage");

  const auto copy_voxel = flight::make_ref<types::VoxelGrid>();
  const auto copy_three_d_source_options =
      flight::make_ref<texture::CreateTexture3DVariantOptions>();
  copy_three_d_source_options->dimension = flight::String("3d");
  copy_three_d_source_options->source =
      decltype(texture::CreateTexture3DVariantOptions::source){
          std::in_place_type<flight::Ref<types::VoxelGrid>>, copy_voxel};
  const auto copy_three_d_source = texture::create_texture(copy_three_d_source_options);
  const auto copy_three_d_out_options =
      flight::make_ref<texture::CreateTexture3DVariantOptions>();
  copy_three_d_out_options->dimension = flight::String("3d");
  const auto copy_three_d_out = texture::create_texture(copy_three_d_out_options);
  types::TextureLike copy_three_d_source_view{
      std::in_place_type<flight::Ref<types::Texture3D>>, copy_three_d_source};
  types::TextureLike copy_three_d_out_view{
      std::in_place_type<flight::Ref<types::Texture3D>>, copy_three_d_out};
  texture::copy_texture(copy_three_d_out_view, copy_three_d_source_view);
  check(copy_three_d_out->source.has_value() && copy_three_d_out->source.value() == copy_voxel,
        "copyTexture shares the exact voxel source owner for 3D textures");

  const auto copy_cube_source_options =
      flight::make_ref<texture::CreateTextureCubeVariantOptions>();
  copy_cube_source_options->dimension = flight::String("cube");
  copy_cube_source_options->sources = types::TextureSourceCubeFaces{
      types::TextureSourceValue{
          std::in_place_type<flight::Ref<types::TextureSource>>, source},
      std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  const auto copy_cube_source = texture::create_texture(copy_cube_source_options);
  const auto copy_cube_out_options =
      flight::make_ref<texture::CreateTextureCubeVariantOptions>();
  copy_cube_out_options->dimension = flight::String("cube");
  const auto copy_cube_out = texture::create_texture(copy_cube_out_options);
  types::TextureLike copy_cube_source_view{
      std::in_place_type<flight::Ref<types::TextureCube>>, copy_cube_source};
  types::TextureLike copy_cube_out_view{
      std::in_place_type<flight::Ref<types::TextureCube>>, copy_cube_out};
  texture::copy_texture(copy_cube_out_view, copy_cube_source_view);
  copy_cube_out->sources[0] = std::nullopt;
  check(copy_cube_source->sources.get(0.0).has_value() &&
            copy_cube_source->sources.get(0.0).value().has_value(),
        "copyTexture gives cube textures fresh face-list storage");

  const std::array<types::TextureLike, 4> dimension_textures{
      copy_out_view, copy_array_source_view, copy_three_d_source_view,
      copy_cube_source_view};
  const std::array<const char*, 4> dimension_mutations{
      "--mutate-dimension-2d", "--mutate-dimension-2d-array",
      "--mutate-dimension-3d", "--mutate-dimension-cube"};
  const std::array<const char*, 4> dimension_messages{
      "copyTexture rejects every mismatched dimension for a 2D destination",
      "copyTexture rejects every mismatched dimension for a 2D-array destination",
      "copyTexture rejects every mismatched dimension for a 3D destination",
      "copyTexture rejects every mismatched dimension for a cube destination"};
  for (std::size_t out_index = 0; out_index < dimension_textures.size(); ++out_index) {
    const auto before_color = std::visit(
        [](const auto& value) { return value->color_space; }, dimension_textures[out_index]);
    const auto before_sampler = std::visit(
        [](const auto& value) { return value->sampler; }, dimension_textures[out_index]);
    bool rejected_all = true;
    for (std::size_t source_index = 0; source_index < dimension_textures.size();
         ++source_index) {
      if (source_index == out_index) continue;
      bool rejected = false;
      try {
        texture::copy_texture(dimension_textures[out_index],
                              dimension_textures[source_index]);
      } catch (const flight::Error& error) {
        rejected = error.message() ==
                   flight::String("copyTexture requires matching dimensions");
      }
      if (mutation == dimension_mutations[out_index] &&
          source_index == ((out_index + 1) % dimension_textures.size())) {
        rejected = false;
      }
      rejected_all = rejected_all && rejected;
    }
    check(rejected_all, dimension_messages[out_index]);
    check(std::visit([](const auto& value) { return value->color_space; },
                     dimension_textures[out_index]) == before_color &&
              std::visit([](const auto& value) { return value->sampler; },
                         dimension_textures[out_index]) == before_sampler,
          "dimension rejection occurs before destination mutation");
  }

  const auto rejected_source = flight::make_ref<types::TextureSource>();
  const std::optional<types::TextureSourceValue> replacement_source{
      types::TextureSourceValue{
          std::in_place_type<flight::Ref<types::TextureSource>>, rejected_source}};
  bool set_source_rejected_all = true;
  for (std::size_t index = 1; index < dimension_textures.size(); ++index) {
    const auto before_version = std::visit(
        [](const auto& value) { return value->version; }, dimension_textures[index]);
    bool rejected = false;
    try {
      texture::set_texture_source(dimension_textures[index], replacement_source);
    } catch (const flight::Error& error) {
      rejected = error.message() ==
                 flight::String("setTextureSource requires a Texture2D");
    }
    if (mutation == "--mutate-set-source-dimension" && index == 1) rejected = false;
    set_source_rejected_all = set_source_rejected_all && rejected &&
                              std::visit([](const auto& value) { return value->version; },
                                         dimension_textures[index]) == before_version;
  }
  check(set_source_rejected_all,
        "setTextureSource rejects every non-2D dimension before mutation");

  const auto direct_source = flight::make_ref<types::TextureSource>();
  direct_source->kind = types::image_texture_source_kind;
  direct_source->width = 32.0;
  direct_source->height = 64.0;
  const types::TextureSourceValue direct_source_value{
      std::in_place_type<flight::Ref<types::TextureSource>>, direct_source};
  const std::optional<types::TextureSourceValue> bound_direct_source{direct_source_value};

  const auto source_state_texture = texture::create_texture();
  types::TextureLike source_state_view{
      std::in_place_type<flight::Ref<types::Texture2D>>, source_state_texture};
  texture::set_texture_source(source_state_view, bound_direct_source);
  const auto selected_direct_source = texture::get_texture_source(source_state_view);
  bool direct_source_identity =
      selected_direct_source.has_value() &&
      std::holds_alternative<flight::Ref<types::TextureSource>>(
          selected_direct_source.value()) &&
      std::get<flight::Ref<types::TextureSource>>(selected_direct_source.value()) ==
          direct_source;
  if (mutation == "--mutate-source-direct") direct_source_identity = false;
  check(direct_source_identity,
        "getTextureSource returns the exact direct 2D source owner");
  bool bound_readiness = texture::has_texture_source(source_state_view) &&
                         texture::is_texture_ready(source_state_view);
  if (mutation == "--mutate-readiness") bound_readiness = false;
  check(bound_readiness,
        "a bound 2D source is present and ready through the shared source field");
  auto direct_kind = texture::get_texture_source_kind(source_state_view);
  if (mutation == "--mutate-source-kind") direct_kind = flight::String("mutated");
  check(direct_kind == std::optional<flight::String>{types::image_texture_source_kind},
        "getTextureSourceKind reads the active image-source key");
  double direct_width = texture::get_texture_width(source_state_view);
  if (mutation == "--mutate-width-2d") direct_width += 1.0;
  check(direct_width == 32.0, "getTextureWidth derives the bound 2D image width");
  double direct_height = texture::get_texture_height(source_state_view);
  if (mutation == "--mutate-height-2d") direct_height += 1.0;
  check(direct_height == 64.0, "getTextureHeight derives the bound 2D image height");
  check(source_state_texture->version == 1.0,
        "setTextureSource advances the version for the first binding");

  texture::set_texture_source(source_state_view, bound_direct_source);
  check(source_state_texture->version == 1.0,
        "setTextureSource leaves the version unchanged for the same source owner");

  const auto render_target = flight::make_ref<types::RenderTarget>();
  render_target->kind = types::render_target_texture_source_kind;
  render_target->width = 96.0;
  render_target->height = 48.0;
  const std::optional<types::TextureSourceValue> bound_render_target{
      types::TextureSourceValue{
          std::in_place_type<flight::Ref<types::RenderTarget>>, render_target}};
  texture::set_texture_source(source_state_view, bound_render_target);
  const auto selected_render_target = texture::get_texture_source(source_state_view);
  bool replacement_identity =
      selected_render_target.has_value() &&
      std::holds_alternative<flight::Ref<types::RenderTarget>>(
          selected_render_target.value()) &&
      std::get<flight::Ref<types::RenderTarget>>(selected_render_target.value()) ==
          render_target;
  if (mutation == "--mutate-source-replacement") replacement_identity = false;
  check(replacement_identity && source_state_texture->version == 2.0,
        "setTextureSource replaces the owner in place and advances the version");
  auto replacement_kind = texture::get_texture_source_kind(source_state_view);
  if (mutation == "--mutate-replacement-kind") {
    replacement_kind = types::image_texture_source_kind;
  }
  check(replacement_kind ==
            std::optional<flight::String>{types::render_target_texture_source_kind},
        "getTextureSourceKind distinguishes a render-target source");
  double render_target_width = texture::get_texture_width(source_state_view);
  double render_target_height = texture::get_texture_height(source_state_view);
  if (mutation == "--mutate-render-target-width") render_target_width += 1.0;
  if (mutation == "--mutate-render-target-height") render_target_height += 1.0;
  check(render_target_width == 96.0,
        "getTextureWidth derives the render-target source width");
  check(render_target_height == 48.0,
        "getTextureHeight derives the render-target source height");

  texture::set_texture_source(source_state_view, std::nullopt);
  bool cleared_source = !texture::get_texture_source(source_state_view).has_value() &&
                        !texture::has_texture_source(source_state_view) &&
                        !texture::is_texture_ready(source_state_view) &&
                        source_state_texture->version == 3.0;
  if (mutation == "--mutate-source-clear") cleared_source = false;
  check(cleared_source,
        "setTextureSource clears the owner and returns the texture to not-ready");

  source_state_texture->version = 4294967295.0;
  texture::set_texture_source(source_state_view, bound_direct_source);
  bool source_version_wrapped = source_state_texture->version == 0.0;
  if (mutation == "--mutate-source-version") source_version_wrapped = false;
  check(source_version_wrapped, "setTextureSource wraps its content version as u32");

  const auto array_primary = flight::make_ref<types::TextureSource>();
  array_primary->kind = types::image_texture_source_kind;
  array_primary->width = 40.0;
  array_primary->height = 20.0;
  const auto array_later = flight::make_ref<types::TextureSource>();
  array_later->kind = types::image_texture_source_kind;
  array_later->width = 90.0;
  array_later->height = 80.0;
  const auto source_array_options =
      flight::make_ref<texture::CreateTexture2DArrayVariantOptions>();
  source_array_options->dimension = flight::String("2d-array");
  source_array_options->sources =
      flight::Array<std::optional<types::TextureSourceValue>>{
          std::nullopt,
          types::TextureSourceValue{
              std::in_place_type<flight::Ref<types::TextureSource>>, array_primary},
          types::TextureSourceValue{
              std::in_place_type<flight::Ref<types::TextureSource>>, array_later}};
  const auto source_array = texture::create_texture(source_array_options);
  types::TextureLike source_array_view{
      std::in_place_type<flight::Ref<types::Texture2DArray>>, source_array};
  const auto selected_array_source = texture::get_texture_source(source_array_view);
  bool array_source_identity =
      selected_array_source.has_value() &&
      std::holds_alternative<flight::Ref<types::TextureSource>>(
          selected_array_source.value()) &&
      std::get<flight::Ref<types::TextureSource>>(selected_array_source.value()) ==
          array_primary &&
      texture::has_texture_source(source_array_view) &&
      texture::is_texture_ready(source_array_view);
  if (mutation == "--mutate-source-array") array_source_identity = false;
  check(array_source_identity,
        "layered textures select the first bound source after absent slots");
  double array_width = texture::get_texture_width(source_array_view);
  if (mutation == "--mutate-width-array") array_width += 1.0;
  check(array_width == 40.0,
        "getTextureWidth derives the first bound 2D-array source width");
  double array_height = texture::get_texture_height(source_array_view);
  if (mutation == "--mutate-height-array") array_height += 1.0;
  check(array_height == 20.0,
        "getTextureHeight derives the first bound 2D-array source height");

  const auto cube_primary = flight::make_ref<types::TextureSource>();
  cube_primary->kind = types::image_texture_source_kind;
  cube_primary->width = 30.0;
  cube_primary->height = 10.0;
  const auto source_cube_options =
      flight::make_ref<texture::CreateTextureCubeVariantOptions>();
  source_cube_options->dimension = flight::String("cube");
  source_cube_options->sources = types::TextureSourceCubeFaces{
      std::nullopt, std::nullopt,
      types::TextureSourceValue{
          std::in_place_type<flight::Ref<types::TextureSource>>, cube_primary},
      std::nullopt, std::nullopt, std::nullopt};
  const auto source_cube = texture::create_texture(source_cube_options);
  types::TextureLike source_cube_view{
      std::in_place_type<flight::Ref<types::TextureCube>>, source_cube};
  const auto selected_cube_source = texture::get_texture_source(source_cube_view);
  bool cube_source_identity =
      selected_cube_source.has_value() &&
      std::holds_alternative<flight::Ref<types::TextureSource>>(
          selected_cube_source.value()) &&
      std::get<flight::Ref<types::TextureSource>>(selected_cube_source.value()) ==
          cube_primary &&
      texture::has_texture_source(source_cube_view) &&
      texture::is_texture_ready(source_cube_view);
  if (mutation == "--mutate-source-cube") cube_source_identity = false;
  check(cube_source_identity,
        "cube textures select the first bound face after absent faces");
  double cube_width = texture::get_texture_width(source_cube_view);
  if (mutation == "--mutate-width-cube") cube_width += 1.0;
  check(cube_width == 30.0,
        "getTextureWidth derives the first bound cube-face width");
  double cube_height = texture::get_texture_height(source_cube_view);
  if (mutation == "--mutate-height-cube") cube_height += 1.0;
  check(cube_height == 10.0,
        "getTextureHeight derives the first bound cube-face height");

  const auto voxel_source = flight::make_ref<types::VoxelGrid>();
  voxel_source->kind = types::voxel_grid_texture_source_kind;
  voxel_source->width = 7.0;
  voxel_source->height = 5.0;
  voxel_source->depth = 3.0;
  voxel_source->data = flight::Uint8Array(105);
  voxel_source->format = flight::String("rgba8unorm");
  const auto source_three_d_options =
      flight::make_ref<texture::CreateTexture3DVariantOptions>();
  source_three_d_options->dimension = flight::String("3d");
  source_three_d_options->source =
      decltype(texture::CreateTexture3DVariantOptions::source){
          std::in_place_type<flight::Ref<types::VoxelGrid>>, voxel_source};
  const auto source_three_d = texture::create_texture(source_three_d_options);
  types::TextureLike source_three_d_view{
      std::in_place_type<flight::Ref<types::Texture3D>>, source_three_d};
  const auto selected_voxel_source = texture::get_texture_source(source_three_d_view);
  bool voxel_source_identity =
      selected_voxel_source.has_value() &&
      std::holds_alternative<flight::Ref<types::VoxelGrid>>(
          selected_voxel_source.value()) &&
      std::get<flight::Ref<types::VoxelGrid>>(selected_voxel_source.value()) ==
          voxel_source &&
      texture::has_texture_source(source_three_d_view) &&
      texture::is_texture_ready(source_three_d_view);
  if (mutation == "--mutate-source-3d") voxel_source_identity = false;
  check(voxel_source_identity,
        "3D textures expose the exact voxel-grid source owner");
  double voxel_width = texture::get_texture_width(source_three_d_view);
  if (mutation == "--mutate-width-3d") voxel_width += 1.0;
  check(voxel_width == 7.0,
        "getTextureWidth derives the voxel-grid width for a 3D texture");
  double voxel_height = texture::get_texture_height(source_three_d_view);
  if (mutation == "--mutate-height-3d") voxel_height += 1.0;
  check(voxel_height == 5.0,
        "getTextureHeight derives the voxel-grid height for a 3D texture");
  double voxel_depth =
      std::get<flight::Ref<types::VoxelGrid>>(selected_voxel_source.value())->depth;
  if (mutation == "--mutate-depth-3d") voxel_depth += 1.0;
  check(voxel_depth == 3.0,
        "the exact 3D source owner preserves its voxel depth without a substitute getter");
  const auto voxel_data = voxel_source->data;
  texture::invalidate_voxel_grid(voxel_source);
  bool voxel_invalidation =
      voxel_source->version == 1.0 && voxel_source->data == voxel_data;
  if (mutation == "--mutate-voxel-invalidation") voxel_invalidation = false;
  check(voxel_invalidation,
        "invalidateVoxelGrid advances content state without replacing voxel bytes");
  voxel_source->version = 4294967295.0;
  texture::invalidate_voxel_grid(voxel_source);
  check(voxel_source->version == 0.0,
        "invalidateVoxelGrid wraps its shared content version as u32");

  const auto absent_array_options =
      flight::make_ref<texture::CreateTexture2DArrayVariantOptions>();
  absent_array_options->dimension = flight::String("2d-array");
  absent_array_options->sources =
      flight::Array<std::optional<types::TextureSourceValue>>{std::nullopt};
  const auto absent_array = texture::create_texture(absent_array_options);
  const auto absent_three_d_options =
      flight::make_ref<texture::CreateTexture3DVariantOptions>();
  absent_three_d_options->dimension = flight::String("3d");
  const auto absent_three_d = texture::create_texture(absent_three_d_options);
  const std::array<types::TextureLike, 4> absent_source_textures{
      types::TextureLike{std::in_place_type<flight::Ref<types::Texture2D>>,
                         texture::create_texture()},
      types::TextureLike{std::in_place_type<flight::Ref<types::Texture2DArray>>,
                         absent_array},
      types::TextureLike{std::in_place_type<flight::Ref<types::TextureCube>>, cube},
      types::TextureLike{std::in_place_type<flight::Ref<types::Texture3D>>,
                         absent_three_d}};
  bool all_absent_sources_are_empty = true;
  for (const auto& absent_source_texture : absent_source_textures) {
    const auto empty_view = flight::geometry::create_vector2(4.0, 5.0);
    texture::get_texture_view_size(empty_view, absent_source_texture);
    all_absent_sources_are_empty =
        all_absent_sources_are_empty &&
        !texture::get_texture_source(absent_source_texture).has_value() &&
        !texture::get_texture_source_kind(absent_source_texture).has_value() &&
        texture::get_texture_width(absent_source_texture) == -1.0 &&
        texture::get_texture_height(absent_source_texture) == -1.0 &&
        !texture::has_texture_source(absent_source_texture) &&
        !texture::is_texture_ready(absent_source_texture) && empty_view->x == 0.0 &&
        empty_view->y == 0.0;
  }
  if (mutation == "--mutate-absent-sources") all_absent_sources_are_empty = false;
  check(all_absent_sources_are_empty,
        "all four texture forms expose absent sources as -1 size, not-ready, and empty view");

  const auto view_source = flight::make_ref<types::TextureSource>();
  view_source->kind = types::image_texture_source_kind;
  view_source->width = 100.0;
  view_source->height = 50.0;
  const auto view_options = flight::make_ref<types::CreateTexture2DOptions>();
  view_options->source = decltype(types::CreateTexture2DOptions::source){
      std::in_place_type<flight::Ref<types::TextureSource>>, view_source};
  view_options->uv_scale = flight::geometry::create_vector2(0.3, 0.2);
  view_options->uv_rotation = -flight::pi / 2.0;
  const auto view_texture = texture::create_texture2_d(view_options);
  const types::TextureLike view_texture_like{
      std::in_place_type<flight::Ref<types::Texture2D>>, view_texture};
  const auto view_size = flight::geometry::create_vector2();
  texture::get_texture_view_size(view_size, view_texture_like);
  double view_width = view_size->x;
  double view_height = view_size->y;
  if (mutation == "--mutate-view-width") view_width += 1.0;
  if (mutation == "--mutate-view-height") view_height += 1.0;
  check(view_width == 15.0,
        "getTextureViewSize measures the rotated scaled 2D width in backing pixels");
  check(view_height == 20.0,
        "getTextureViewSize measures the rotated scaled 2D height in backing pixels");

  source_array->uv_scale = flight::geometry::create_vector2(0.5, 2.0);
  const auto array_view_size = flight::geometry::create_vector2();
  texture::get_texture_view_size(array_view_size, source_array_view);
  bool array_view_derived =
      array_view_size->x == 20.0 && array_view_size->y == 40.0;
  if (mutation == "--mutate-view-array") array_view_derived = false;
  check(array_view_derived,
        "getTextureViewSize derives an unrotated 2D-array view from its first source");

  source_cube->uv_scale = flight::geometry::create_vector2(2.0, 3.0);
  source_cube->uv_rotation = flight::pi / 2.0;
  const auto cube_view_size = flight::geometry::create_vector2();
  texture::get_texture_view_size(cube_view_size, source_cube_view);
  bool cube_view_derived =
      cube_view_size->x == 20.0 && cube_view_size->y == 90.0;
  if (mutation == "--mutate-view-cube") cube_view_derived = false;
  check(cube_view_derived,
        "getTextureViewSize derives a quarter-turned cube view from its first face");

  source_three_d->uv_scale = flight::geometry::create_vector2(2.0, 3.0);
  source_three_d->uv_rotation = flight::pi;
  const auto three_d_view_size = flight::geometry::create_vector2();
  texture::get_texture_view_size(three_d_view_size, source_three_d_view);
  bool three_d_view_derived =
      three_d_view_size->x == 14.0 && three_d_view_size->y == 15.0;
  if (mutation == "--mutate-view-3d") three_d_view_derived = false;
  check(three_d_view_derived,
        "getTextureViewSize derives a half-turned 3D view from voxel width and height");

  const auto invalid_source = flight::make_ref<types::TextureSource>();
  invalid_source->kind = types::image_texture_source_kind;
  invalid_source->width = 0.0;
  invalid_source->height = -4.0;
  const auto invalid_options = flight::make_ref<types::CreateTexture2DOptions>();
  invalid_options->source = decltype(types::CreateTexture2DOptions::source){
      std::in_place_type<flight::Ref<types::TextureSource>>, invalid_source};
  const auto invalid_texture = texture::create_texture2_d(invalid_options);
  const types::TextureLike invalid_texture_view{
      std::in_place_type<flight::Ref<types::Texture2D>>, invalid_texture};
  const auto invalid_view_size = flight::geometry::create_vector2(4.0, 5.0);
  texture::get_texture_view_size(invalid_view_size, invalid_texture_view);
  bool invalid_size_is_empty = texture::is_texture_ready(invalid_texture_view) &&
                               texture::get_texture_width(invalid_texture_view) == 0.0 &&
                               texture::get_texture_height(invalid_texture_view) == -4.0 &&
                               invalid_view_size->x == 0.0 && invalid_view_size->y == 0.0;
  if (mutation == "--mutate-invalid-view") invalid_size_is_empty = false;
  check(invalid_size_is_empty,
        "a present non-positive source stays ready while its derived view is empty");

  const auto uv_texture = texture::create_texture();
  uv_texture->version = 17.0;
  types::TextureLike uv_texture_view{
      std::in_place_type<flight::Ref<types::Texture2D>>, uv_texture};
  const auto uv_matrix = flight::geometry::create_matrix3();
  const auto uv_matrix_storage = uv_matrix->m;
  texture::get_texture_uv_matrix(uv_matrix, uv_texture_view);
  bool default_uv_matrix =
      uv_matrix->m.get_index(0.0) == 1.0 &&
      uv_matrix->m.get_index(1.0) == 0.0 &&
      uv_matrix->m.get_index(2.0) == 0.0 &&
      uv_matrix->m.get_index(3.0) == 0.0 &&
      uv_matrix->m.get_index(4.0) == 1.0 &&
      uv_matrix->m.get_index(5.0) == 0.0 &&
      uv_matrix->m.get_index(6.0) == 0.0 &&
      uv_matrix->m.get_index(7.0) == 0.0 &&
      uv_matrix->m.get_index(8.0) == 1.0;
  if (mutation == "--mutate-uv-default") default_uv_matrix = false;
  check(default_uv_matrix,
        "getTextureUvMatrix writes the column-major identity for default UV state");
  const auto default_transformed_uv = flight::geometry::create_vector2();
  texture::transform_texture_uv(
      default_transformed_uv, uv_texture_view, 0.25, 0.75);
  bool default_uv_transform = near(default_transformed_uv->x, 0.25) &&
                              near(default_transformed_uv->y, 0.75);
  if (mutation == "--mutate-uv-transform-default") {
    default_uv_transform = false;
  }
  check(default_uv_transform,
        "transformTextureUv leaves coordinates unchanged under identity state");

  const auto uv_offset_owner = uv_texture->uv_offset;
  const auto uv_scale_owner = uv_texture->uv_scale;
  texture::set_texture_uv_scale(uv_texture_view, 2.0, 3.0);
  texture::set_texture_uv_offset(uv_texture_view, 0.1, 0.2);
  texture::set_texture_uv_rotation(uv_texture_view, flight::pi / 4.0);
  bool uv_mutators_preserve_state =
      uv_texture->uv_offset == uv_offset_owner &&
      uv_texture->uv_scale == uv_scale_owner && uv_texture->version == 17.0 &&
      near(uv_texture->uv_offset->x, 0.1) && near(uv_texture->uv_offset->y, 0.2) &&
      near(uv_texture->uv_scale->x, 2.0) && near(uv_texture->uv_scale->y, 3.0) &&
      near(uv_texture->uv_rotation, flight::pi / 4.0);
  if (mutation == "--mutate-uv-mutator-identity") {
    uv_mutators_preserve_state = false;
  }
  check(uv_mutators_preserve_state,
        "UV mutators write existing vector owners without dirtying the content version");

  texture::get_texture_uv_matrix(uv_matrix, uv_texture_view);
  const double quarter_cos = std::cos(flight::pi / 4.0);
  const double quarter_sin = std::sin(flight::pi / 4.0);
  bool uv_scale_x_component =
      near(uv_matrix->m.get_index(0.0), 2.0 * quarter_cos) &&
      near(uv_matrix->m.get_index(1.0), 2.0 * quarter_sin);
  if (mutation == "--mutate-uv-scale-x") uv_scale_x_component = false;
  check(uv_scale_x_component,
        "getTextureUvMatrix rotates the scaled U basis into its first column");
  bool uv_scale_y_component =
      near(uv_matrix->m.get_index(3.0), -3.0 * quarter_sin) &&
      near(uv_matrix->m.get_index(4.0), 3.0 * quarter_cos);
  if (mutation == "--mutate-uv-scale-y") uv_scale_y_component = false;
  check(uv_scale_y_component,
        "getTextureUvMatrix rotates the scaled V basis into its second column");
  bool uv_rotation_component =
      near(uv_matrix->m.get_index(1.0) / uv_matrix->m.get_index(0.0),
           std::tan(flight::pi / 4.0)) &&
      near(-uv_matrix->m.get_index(3.0) / uv_matrix->m.get_index(4.0),
           std::tan(flight::pi / 4.0));
  if (mutation == "--mutate-uv-rotation") uv_rotation_component = false;
  check(uv_rotation_component,
        "getTextureUvMatrix applies the requested rotation to both scaled axes");
  bool uv_offset_component = near(uv_matrix->m.get_index(6.0), 0.1) &&
                             near(uv_matrix->m.get_index(7.0), 0.2);
  if (mutation == "--mutate-uv-offset") uv_offset_component = false;
  check(uv_offset_component,
        "getTextureUvMatrix places translation in the final column after rotation");
  bool uv_matrix_recomputed =
      uv_matrix->m == uv_matrix_storage &&
      !near(uv_matrix->m.get_index(0.0), 1.0) && uv_texture->version == 17.0;
  if (mutation == "--mutate-uv-recompute") uv_matrix_recomputed = false;
  check(uv_matrix_recomputed,
        "matrix derivation recomputes into the same output buffer without a dirty cache");

  const auto transformed_uv = flight::geometry::create_vector2();
  texture::transform_texture_uv(transformed_uv, uv_texture_view, 0.25, 0.5);
  const double ordered_x =
      2.0 * quarter_cos * 0.25 - 3.0 * quarter_sin * 0.5 + 0.1;
  const double ordered_y =
      2.0 * quarter_sin * 0.25 + 3.0 * quarter_cos * 0.5 + 0.2;
  bool uv_transform_order = near(transformed_uv->x, ordered_x) &&
                            near(transformed_uv->y, ordered_y);
  if (mutation == "--mutate-uv-order") uv_transform_order = false;
  check(uv_transform_order,
        "transformTextureUv applies scale, then rotation, then translation");
  const auto transformed_by_matrix = apply_uv_matrix(uv_matrix, 0.25, 0.5);
  check(near(transformed_by_matrix[0], transformed_uv->x) &&
            near(transformed_by_matrix[1], transformed_uv->y),
        "the inline UV transform matches column-major matrix multiplication");

  const auto flip_texture = texture::create_texture();
  types::TextureLike flip_texture_view{
      std::in_place_type<flight::Ref<types::Texture2D>>, flip_texture};
  const auto flip_matrix = flight::geometry::create_matrix3();
  texture::set_texture_flip(flip_texture_view, true, false);
  texture::get_texture_uv_matrix(flip_matrix, flip_texture_view);
  auto flipped_point = apply_uv_matrix(flip_matrix, 0.2, 0.3);
  bool horizontal_flip = near(flipped_point[0], 0.8) &&
                         near(flipped_point[1], 0.3);
  if (mutation == "--mutate-uv-flip-x") horizontal_flip = false;
  check(horizontal_flip, "horizontal UV flip maps u to one minus u");
  texture::set_texture_flip(flip_texture_view, false, true);
  texture::get_texture_uv_matrix(flip_matrix, flip_texture_view);
  flipped_point = apply_uv_matrix(flip_matrix, 0.2, 0.3);
  bool vertical_flip = near(flipped_point[0], 0.2) &&
                       near(flipped_point[1], 0.7);
  if (mutation == "--mutate-uv-flip-y") vertical_flip = false;
  check(vertical_flip, "vertical UV flip maps v to one minus v");
  check(flip_texture->version == 0.0,
        "setTextureFlip mutates sampling state without changing content version");

  texture::set_texture_flip(flip_texture_view, true, true);
  texture::set_texture_uv_scale(flip_texture_view, 2.0, 3.0);
  texture::set_texture_uv_rotation(flip_texture_view, 0.5);
  texture::set_texture_uv_offset(flip_texture_view, 0.1, 0.2);
  texture::get_texture_uv_matrix(flip_matrix, flip_texture_view);
  const auto flip_inline = flight::geometry::create_vector2();
  texture::transform_texture_uv(flip_inline, flip_texture_view, 0.35, 0.6);
  const auto flip_by_matrix = apply_uv_matrix(flip_matrix, 0.35, 0.6);
  const double flip_expected_x =
      2.0 * std::cos(0.5) * (1.0 - 0.35) -
      3.0 * std::sin(0.5) * (1.0 - 0.6) + 0.1;
  const double flip_expected_y =
      2.0 * std::sin(0.5) * (1.0 - 0.35) +
      3.0 * std::cos(0.5) * (1.0 - 0.6) + 0.2;
  bool composed_flip_order =
      near(flip_inline->x, flip_expected_x) &&
      near(flip_inline->y, flip_expected_y) &&
      near(flip_by_matrix[0], flip_expected_x) &&
      near(flip_by_matrix[1], flip_expected_y);
  if (mutation == "--mutate-uv-flip-order") composed_flip_order = false;
  check(composed_flip_order,
        "flips precede non-uniform scale, rotation, and translation in both paths");

  const auto cardinal_texture = texture::create_texture();
  types::TextureLike cardinal_view{
      std::in_place_type<flight::Ref<types::Texture2D>>, cardinal_texture};
  texture::set_texture_uv_scale(cardinal_view, 2.0, 3.0);
  texture::set_texture_uv_offset(cardinal_view, 0.25, 0.5);
  const auto cardinal_matrix = flight::geometry::create_matrix3();
  const std::array<double, 5> cardinal_rotations{
      0.0, flight::pi / 2.0, -flight::pi / 2.0, flight::pi, -flight::pi};
  const std::array<std::array<double, 4>, 5> cardinal_columns{{
      {2.0, 0.0, 0.0, 3.0},
      {0.0, 2.0, -3.0, 0.0},
      {0.0, -2.0, 3.0, 0.0},
      {-2.0, 0.0, 0.0, -3.0},
      {-2.0, 0.0, 0.0, -3.0},
  }};
  bool cardinal_edges_exact = true;
  for (std::size_t index = 0; index < cardinal_rotations.size(); ++index) {
    texture::set_texture_uv_rotation(cardinal_view, cardinal_rotations[index]);
    texture::get_texture_uv_matrix(cardinal_matrix, cardinal_view);
    cardinal_edges_exact =
        cardinal_edges_exact &&
        cardinal_matrix->m.get_index(0.0) == cardinal_columns[index][0] &&
        cardinal_matrix->m.get_index(1.0) == cardinal_columns[index][1] &&
        cardinal_matrix->m.get_index(3.0) == cardinal_columns[index][2] &&
        cardinal_matrix->m.get_index(4.0) == cardinal_columns[index][3] &&
        cardinal_matrix->m.get_index(6.0) == 0.25 &&
        cardinal_matrix->m.get_index(7.0) == 0.5;
  }
  texture::set_texture_uv_rotation(cardinal_view, 0.0);
  texture::set_texture_uv_scale(cardinal_view, -2.0, -3.0);
  texture::get_texture_uv_matrix(cardinal_matrix, cardinal_view);
  cardinal_edges_exact = cardinal_edges_exact &&
                         cardinal_matrix->m.get_index(0.0) == -2.0 &&
                         cardinal_matrix->m.get_index(4.0) == -3.0;
  if (mutation == "--mutate-uv-cardinal-edges") cardinal_edges_exact = false;
  check(cardinal_edges_exact,
        "cardinal rotations produce exact zero terms and negative scales retain sign");

  const auto default_inverse = flight::geometry::create_matrix3();
  const auto default_inverse_storage = default_inverse->m;
  texture::get_texture_inverse_uv_matrix(
      default_inverse,
      types::TextureLike{std::in_place_type<flight::Ref<types::Texture2D>>,
                         texture::create_texture()});
  bool default_inverse_identity =
      default_inverse->m == default_inverse_storage &&
      default_inverse->m.get_index(0.0) == 1.0 &&
      default_inverse->m.get_index(4.0) == 1.0 &&
      default_inverse->m.get_index(8.0) == 1.0 &&
      default_inverse->m.get_index(1.0) == 0.0 &&
      default_inverse->m.get_index(3.0) == 0.0 &&
      default_inverse->m.get_index(6.0) == 0.0 &&
      default_inverse->m.get_index(7.0) == 0.0;
  if (mutation == "--mutate-uv-inverse-default") {
    default_inverse_identity = false;
  }
  check(default_inverse_identity,
        "getTextureInverseUvMatrix writes identity into caller-owned default storage");

  const auto expected_inverse = flight::geometry::create_matrix3();
  flight::geometry::inverse_matrix3(expected_inverse, uv_matrix);
  const auto derived_inverse = flight::geometry::create_matrix3();
  const auto derived_inverse_storage = derived_inverse->m;
  texture::get_texture_inverse_uv_matrix(derived_inverse, uv_texture_view);
  bool inverse_matches_forward = derived_inverse->m == derived_inverse_storage;
  for (double index = 0.0; index < 9.0; index += 1.0) {
    inverse_matches_forward =
        inverse_matches_forward &&
        near(derived_inverse->m.get_index(index),
             expected_inverse->m.get_index(index));
  }
  const auto round_trip_forward = apply_uv_matrix(uv_matrix, 0.3, 0.8);
  const auto round_trip_inverse = apply_uv_matrix(
      derived_inverse, round_trip_forward[0], round_trip_forward[1]);
  inverse_matches_forward = inverse_matches_forward &&
                            near(round_trip_inverse[0], 0.3) &&
                            near(round_trip_inverse[1], 0.8);
  if (mutation == "--mutate-uv-inverse") inverse_matches_forward = false;
  check(inverse_matches_forward,
        "inverse UV derivation matches in-place inversion and reverses the forward matrix");

  const auto singular_texture = texture::create_texture();
  types::TextureLike singular_view{
      std::in_place_type<flight::Ref<types::Texture2D>>, singular_texture};
  texture::set_texture_uv_scale(singular_view, 0.0, 1.0);
  const auto singular_inverse = flight::geometry::create_matrix3();
  texture::get_texture_inverse_uv_matrix(singular_inverse, singular_view);
  bool singular_inverse_is_nan = true;
  for (double index = 0.0; index < 9.0; index += 1.0) {
    singular_inverse_is_nan = singular_inverse_is_nan &&
                              std::isnan(singular_inverse->m.get_index(index));
  }
  if (mutation == "--mutate-uv-inverse-singular") {
    singular_inverse_is_nan = false;
  }
  check(singular_inverse_is_nan,
        "a zero-scale UV transform exposes the documented singular NaN inverse");

  const auto presence_texture = texture::create_texture();
  types::TextureLike presence_view{
      std::in_place_type<flight::Ref<types::Texture2D>>, presence_texture};
  bool uv_presence_detection =
      !texture::has_texture_like_uv_transform(presence_view);
  texture::set_texture_uv_scale(presence_view, 2.0, 1.0);
  uv_presence_detection = uv_presence_detection &&
                          texture::has_texture_like_uv_transform(presence_view);
  texture::reset_texture_uv_transform(presence_view);
  texture::set_texture_uv_offset(presence_view, 0.0, 0.5);
  uv_presence_detection = uv_presence_detection &&
                          texture::has_texture_like_uv_transform(presence_view);
  texture::reset_texture_uv_transform(presence_view);
  texture::set_texture_uv_rotation(presence_view, 0.5);
  uv_presence_detection = uv_presence_detection &&
                          texture::has_texture_like_uv_transform(presence_view);
  texture::reset_texture_uv_transform(presence_view);
  texture::set_texture_flip(presence_view, true, false);
  uv_presence_detection = uv_presence_detection &&
                          texture::has_texture_like_uv_transform(presence_view);
  texture::set_texture_flip(presence_view, false, true);
  uv_presence_detection = uv_presence_detection &&
                          texture::has_texture_like_uv_transform(presence_view);
  texture::reset_texture_uv_transform(presence_view);
  uv_presence_detection = uv_presence_detection &&
                          !texture::has_texture_like_uv_transform(presence_view);
  if (mutation == "--mutate-uv-presence") uv_presence_detection = false;
  check(uv_presence_detection,
        "hasTextureUvTransform detects every component and reset restores identity");

  const auto exact_uv_state = flight::make_ref<types::TextureUvTransform>();
  exact_uv_state->uv_offset = flight::geometry::create_vector2();
  exact_uv_state->uv_scale = flight::geometry::create_vector2(1.0, 1.0);
  bool exact_uv_owner_detection =
      !texture::has_texture_uv_transform(exact_uv_state);
  exact_uv_state->flip_x = true;
  exact_uv_owner_detection = exact_uv_owner_detection &&
                             texture::has_texture_uv_transform(exact_uv_state);
  if (mutation == "--mutate-uv-exact-owner") exact_uv_owner_detection = false;
  check(exact_uv_owner_detection,
        "the exact emitted TextureUvTransform owner retains its identity predicate");

  const auto reset_options = flight::make_ref<types::CreateTexture2DOptions>();
  reset_options->color_space = flight::String("linear");
  reset_options->source = decltype(types::CreateTexture2DOptions::source){
      std::in_place_type<flight::Ref<types::TextureSource>>, direct_source};
  reset_options->flip_x = true;
  reset_options->flip_y = true;
  reset_options->uv_offset = flight::geometry::create_vector2(0.4, 0.6);
  reset_options->uv_rotation = flight::pi;
  reset_options->uv_scale = flight::geometry::create_vector2(5.0, 7.0);
  reset_options->version = 9.0;
  const auto reset_texture = texture::create_texture2_d(reset_options);
  types::TextureLike reset_view{
      std::in_place_type<flight::Ref<types::Texture2D>>, reset_texture};
  const auto reset_offset_owner = reset_texture->uv_offset;
  const auto reset_scale_owner = reset_texture->uv_scale;
  const auto reset_sampler_owner = reset_texture->sampler;
  const auto reset_source_owner = reset_texture->source;
  texture::reset_texture_uv_transform(reset_view);
  bool reset_values = !reset_texture->flip_x && !reset_texture->flip_y &&
                      reset_texture->uv_offset->x == 0.0 &&
                      reset_texture->uv_offset->y == 0.0 &&
                      reset_texture->uv_rotation == 0.0 &&
                      reset_texture->uv_scale->x == 1.0 &&
                      reset_texture->uv_scale->y == 1.0 &&
                      !texture::has_texture_like_uv_transform(reset_view);
  if (mutation == "--mutate-uv-reset-values") reset_values = false;
  check(reset_values, "resetTextureUvTransform restores every identity component");
  bool reset_preserves_owners =
      reset_texture->uv_offset == reset_offset_owner &&
      reset_texture->uv_scale == reset_scale_owner &&
      reset_texture->sampler == reset_sampler_owner &&
      reset_texture->source == reset_source_owner &&
      reset_texture->color_space == flight::String("linear") &&
      reset_texture->version == 9.0;
  if (mutation == "--mutate-uv-reset-identity") reset_preserves_owners = false;
  check(reset_preserves_owners,
        "UV reset preserves vector, sampler, source, color, and version identity");

  const auto rect_source = flight::make_ref<types::TextureSource>();
  rect_source->kind = types::image_texture_source_kind;
  rect_source->width = 200.0;
  rect_source->height = 100.0;
  const auto rect_options = flight::make_ref<types::CreateTexture2DOptions>();
  rect_options->source = decltype(types::CreateTexture2DOptions::source){
      std::in_place_type<flight::Ref<types::TextureSource>>, rect_source};
  rect_options->version = 6.0;
  const auto rect_texture = texture::create_texture2_d(rect_options);
  types::TextureLike rect_view{
      std::in_place_type<flight::Ref<types::Texture2D>>, rect_texture};
  const auto rect_offset_owner = rect_texture->uv_offset;
  const auto rect_scale_owner = rect_texture->uv_scale;
  texture::set_texture_uv_from_pixel_rect(rect_view, 20.0, 10.0, 50.0, 25.0);
  bool normalized_rect = rect_texture->uv_offset == rect_offset_owner &&
                         rect_texture->uv_scale == rect_scale_owner &&
                         rect_texture->version == 6.0 &&
                         near(rect_texture->uv_offset->x, 0.1) &&
                         near(rect_texture->uv_offset->y, 0.1) &&
                         near(rect_texture->uv_scale->x, 0.25) &&
                         near(rect_texture->uv_scale->y, 0.25);
  if (mutation == "--mutate-uv-pixel-rect") normalized_rect = false;
  check(normalized_rect,
        "pixel rectangles normalize in place without replacing UV owners or version");
  texture::set_texture_uv_from_pixel_rect(
      rect_view, -20.0, 110.0, -50.0, 200.0);
  bool edge_rect = near(rect_texture->uv_offset->x, -0.1) &&
                   near(rect_texture->uv_offset->y, 1.1) &&
                   near(rect_texture->uv_scale->x, -0.25) &&
                   near(rect_texture->uv_scale->y, 2.0);
  if (mutation == "--mutate-uv-pixel-edge") edge_rect = false;
  check(edge_rect,
        "pixel rectangle normalization preserves negative and out-of-range edge values");
  const auto absent_rect_texture = texture::create_texture();
  types::TextureLike absent_rect_view{
      std::in_place_type<flight::Ref<types::Texture2D>>, absent_rect_texture};
  texture::set_texture_uv_from_pixel_rect(
      absent_rect_view, 20.0, 10.0, 50.0, 25.0);
  texture::set_texture_uv_from_pixel_rect(
      invalid_texture_view, 20.0, 10.0, 50.0, 25.0);
  bool invalid_rect_is_empty = absent_rect_texture->uv_offset->x == 0.0 &&
                               absent_rect_texture->uv_offset->y == 0.0 &&
                               absent_rect_texture->uv_scale->x == 0.0 &&
                               absent_rect_texture->uv_scale->y == 0.0 &&
                               invalid_texture->uv_offset->x == 0.0 &&
                               invalid_texture->uv_offset->y == 0.0 &&
                               invalid_texture->uv_scale->x == 0.0 &&
                               invalid_texture->uv_scale->y == 0.0;
  if (mutation == "--mutate-uv-pixel-invalid") invalid_rect_is_empty = false;
  check(invalid_rect_is_empty,
        "pixel rectangles become an empty window for non-positive source dimensions");

  const auto transform_alias_texture = texture::create_texture();
  types::TextureLike transform_alias_view{
      std::in_place_type<flight::Ref<types::Texture2D>>, transform_alias_texture};
  texture::set_texture_uv_scale(transform_alias_view, 2.0, 3.0);
  texture::set_texture_uv_offset(transform_alias_view, 0.1, 0.2);
  const auto transform_alias_owner = transform_alias_texture->uv_offset;
  texture::transform_texture_uv(
      transform_alias_owner, transform_alias_view, 0.5, 0.5);
  bool transform_output_alias =
      transform_alias_texture->uv_offset == transform_alias_owner &&
      near(transform_alias_owner->x, 1.1) &&
      near(transform_alias_owner->y, 1.7);
  if (mutation == "--mutate-uv-transform-alias") transform_output_alias = false;
  check(transform_output_alias,
        "transformTextureUv safely writes through an output alias after reading scalar state");

  using SamplerView = flight::StructuralRef<
      flight::RowReadonly<flight::RowOf<types::SamplerLike>>>;
  using OptionalSampler =
      std::variant<SamplerView, flight::Null, flight::Undefined>;
  const auto equal_sampler_a = texture::create_sampler(std::nullopt);
  const auto equal_sampler_b = texture::create_sampler(std::nullopt);
  if (mutation == "--mutate-sampler-equality") equal_sampler_b->mipmaps = false;
  const OptionalSampler equal_sampler_a_view{
      std::in_place_type<SamplerView>, equal_sampler_a};
  const OptionalSampler equal_sampler_b_view{
      std::in_place_type<SamplerView>, equal_sampler_b};
  check(texture::equals_sampler(equal_sampler_a_view, equal_sampler_b_view),
        "sampler equality compares identical state by value");
  equal_sampler_b->mipmaps = equal_sampler_a->mipmaps;
  check(texture::equals_sampler(equal_sampler_a_view, equal_sampler_a_view),
        "sampler equality accepts the same owner");
  equal_sampler_b->wrap_u = flight::String("repeat");
  check(!texture::equals_sampler(equal_sampler_a_view, equal_sampler_b_view),
        "sampler equality rejects any differing field");
  const OptionalSampler absent_sampler{std::in_place_type<flight::Null>};
  const OptionalSampler undefined_sampler{std::in_place_type<flight::Undefined>};
  check(!texture::equals_sampler(equal_sampler_a_view, absent_sampler) &&
            !texture::equals_sampler(absent_sampler, equal_sampler_a_view) &&
            !texture::equals_sampler(undefined_sampler, undefined_sampler),
        "sampler equality rejects absent operands");

  const auto equality_options = flight::make_ref<types::CreateTexture2DOptions>();
  equality_options->color_space = flight::String("linear");
  equality_options->source = decltype(types::CreateTexture2DOptions::source){
      std::in_place_type<flight::Ref<types::TextureSource>>, source};
  equality_options->uv_rotation = 0.5;
  const auto equal_texture_a = texture::create_texture2_d(equality_options);
  const auto equal_texture_b = texture::create_texture2_d(equality_options);
  if (mutation == "--mutate-texture-equality") equal_texture_b->uv_rotation = 0.0;
  check(texture::equals_texture(equal_texture_a, equal_texture_b),
        "texture equality compares identical state and shared source identity by value");
  equal_texture_b->uv_rotation = equal_texture_a->uv_rotation;
  check(texture::equals_texture(equal_texture_a, equal_texture_a),
        "texture equality accepts the same owner");

  const auto other_source = flight::make_ref<types::TextureSource>();
  other_source->kind = source->kind;
  other_source->width = source->width;
  other_source->height = source->height;
  equal_texture_b->source = types::TextureSourceValue{
      std::in_place_type<flight::Ref<types::TextureSource>>, other_source};
  check(!texture::equals_texture(equal_texture_a, equal_texture_b),
        "texture equality preserves source identity rather than comparing source fields");
  equal_texture_b->source = equal_texture_a->source;

  equal_texture_b->color_space = flight::String("srgb");
  check(!texture::equals_texture(equal_texture_a, equal_texture_b),
        "texture equality rejects a different color space");
  equal_texture_b->color_space = equal_texture_a->color_space;
  equal_texture_b->uv_rotation = 0.0;
  check(!texture::equals_texture(equal_texture_a, equal_texture_b),
        "texture equality rejects a different UV rotation");
  equal_texture_b->uv_rotation = equal_texture_a->uv_rotation;
  equal_texture_b->uv_offset->x = 0.25;
  check(!texture::equals_texture(equal_texture_a, equal_texture_b),
        "texture equality rejects a different UV offset");
  equal_texture_b->uv_offset->x = equal_texture_a->uv_offset->x;
  equal_texture_b->uv_scale->y = 2.0;
  check(!texture::equals_texture(equal_texture_a, equal_texture_b),
        "texture equality rejects a different UV scale");
  equal_texture_b->uv_scale->y = equal_texture_a->uv_scale->y;
  equal_texture_b->sampler->mipmaps = false;
  check(!texture::equals_texture(equal_texture_a, equal_texture_b),
        "texture equality includes sampler state");

  const texture::OptionalTexture absent_texture{
      std::in_place_type<flight::Null>};
  const texture::OptionalTexture undefined_texture{
      std::in_place_type<flight::Undefined>};
  check(!texture::equals_texture(equal_texture_a, absent_texture) &&
            !texture::equals_texture(absent_texture, equal_texture_a) &&
            !texture::equals_texture(undefined_texture, undefined_texture),
        "texture equality rejects absent operands");

  const auto cube_equality_face = flight::make_ref<types::TextureSource>();
  cube_equality_face->kind = types::image_texture_source_kind;
  cube_equality_face->width = 64.0;
  cube_equality_face->height = 64.0;
  const types::TextureSourceCubeFaces cube_equality_faces{
      types::TextureSourceValue{
          std::in_place_type<flight::Ref<types::TextureSource>>,
          cube_equality_face},
      types::TextureSourceValue{
          std::in_place_type<flight::Ref<types::TextureSource>>,
          cube_equality_face},
      types::TextureSourceValue{
          std::in_place_type<flight::Ref<types::TextureSource>>,
          cube_equality_face},
      types::TextureSourceValue{
          std::in_place_type<flight::Ref<types::TextureSource>>,
          cube_equality_face},
      types::TextureSourceValue{
          std::in_place_type<flight::Ref<types::TextureSource>>,
          cube_equality_face},
      types::TextureSourceValue{
          std::in_place_type<flight::Ref<types::TextureSource>>,
          cube_equality_face}};
  const auto cube_equality_options_a =
      flight::make_ref<texture::CreateTextureCubeVariantOptions>();
  cube_equality_options_a->dimension = flight::String("cube");
  cube_equality_options_a->color_space = flight::String("linear");
  cube_equality_options_a->sources = cube_equality_faces;
  const auto cube_equality_options_b =
      flight::make_ref<texture::CreateTextureCubeVariantOptions>();
  cube_equality_options_b->dimension = flight::String("cube");
  cube_equality_options_b->color_space = flight::String("linear");
  cube_equality_options_b->sources = cube_equality_faces;
  const auto equal_cube_a = texture::create_texture(cube_equality_options_a);
  const auto equal_cube_b = texture::create_texture(cube_equality_options_b);
  using OptionalCubeTexture =
      std::variant<flight::Ref<types::CubeTexture>, flight::Null,
                   flight::Undefined>;
  const OptionalCubeTexture equal_cube_a_view{
      std::in_place_type<flight::Ref<types::CubeTexture>>, equal_cube_a};
  const OptionalCubeTexture equal_cube_b_view{
      std::in_place_type<flight::Ref<types::CubeTexture>>, equal_cube_b};
  bool equal_cube_content =
      texture::equals_cube_texture(equal_cube_a_view, equal_cube_b_view) &&
      texture::equals_cube_texture(equal_cube_a_view, equal_cube_a_view);
  if (mutation == "--mutate-cube-equality-content") {
    equal_cube_content = false;
  }
  check(equal_cube_content,
        "cube equality accepts equal value state with the same six face owners");

  const auto distinct_cube_face = flight::make_ref<types::TextureSource>();
  distinct_cube_face->kind = cube_equality_face->kind;
  distinct_cube_face->width = cube_equality_face->width;
  distinct_cube_face->height = cube_equality_face->height;
  equal_cube_b->sources[0] = types::TextureSourceValue{
      std::in_place_type<flight::Ref<types::TextureSource>>,
      distinct_cube_face};
  bool cube_face_identity =
      !texture::equals_cube_texture(equal_cube_a_view, equal_cube_b_view);
  if (mutation == "--mutate-cube-equality-face-identity") {
    cube_face_identity = false;
  }
  check(cube_face_identity,
        "cube equality compares face owners rather than equal-looking face fields");
  equal_cube_b->sources[0] = equal_cube_a->sources.get(0.0).value();

  equal_cube_b->color_space = flight::String("srgb");
  bool cube_value_state =
      !texture::equals_cube_texture(equal_cube_a_view, equal_cube_b_view);
  equal_cube_b->color_space = equal_cube_a->color_space;
  equal_cube_b->sampler->mipmaps = false;
  cube_value_state =
      cube_value_state &&
      !texture::equals_cube_texture(equal_cube_a_view, equal_cube_b_view);
  if (mutation == "--mutate-cube-equality-value-state") {
    cube_value_state = false;
  }
  check(cube_value_state,
        "cube equality includes common color-space and sampler value state");

  const OptionalCubeTexture absent_cube{std::in_place_type<flight::Null>};
  const OptionalCubeTexture undefined_cube{
      std::in_place_type<flight::Undefined>};
  bool cube_absence =
      !texture::equals_cube_texture(equal_cube_a_view, absent_cube) &&
      !texture::equals_cube_texture(absent_cube, equal_cube_a_view) &&
      !texture::equals_cube_texture(equal_cube_a_view, undefined_cube) &&
      !texture::equals_cube_texture(undefined_cube, equal_cube_a_view) &&
      !texture::equals_cube_texture(undefined_cube, undefined_cube);
  if (mutation == "--mutate-cube-equality-absence") cube_absence = false;
  check(cube_absence,
        "cube equality rejects null and undefined operands in either position");

  struct ColorSpaceCase {
    const char* source;
    const char* working;
    bool decode;
    const char* sample;
    bool premultiply;
    const char* mutation;
    const char* message;
  };
  const std::array<ColorSpaceCase, 4> color_space_cases{{
      {"srgb", "linear", true, "srgb", false,
       "--mutate-color-space-srgb-linear",
       "sRGB content in linear working space decodes and defers premultiplication"},
      {"srgb", "srgb", false, "linear", true,
       "--mutate-color-space-srgb-srgb",
       "sRGB content in sRGB working space remains byte-through and premultiplies on upload"},
      {"linear", "linear", false, "linear", true,
       "--mutate-color-space-linear-linear",
       "linear content in linear working space remains byte-through and premultiplies on upload"},
      {"linear", "srgb", false, "linear", true,
       "--mutate-color-space-linear-srgb",
       "linear content in sRGB working space is not silently encoded on sample"},
  }};
  const auto observe_color_space = [](flight::String source_space,
                                      flight::String working_space) {
    return std::tuple{
        texture::should_decode_texture_on_sample(source_space, working_space),
        texture::get_texture_sample_color_space(source_space, working_space),
        texture::should_premultiply_texture_on_upload(source_space,
                                                       working_space)};
  };
  for (const auto& color_case : color_space_cases) {
    const auto observed = observe_color_space(
        flight::String(color_case.source), flight::String(color_case.working));
    bool row_matches =
        observed == std::tuple{color_case.decode,
                               flight::String(color_case.sample),
                               color_case.premultiply};
    if (mutation == color_case.mutation) row_matches = false;
    check(row_matches, color_case.message);
  }

  const auto default_color_observation = observe_color_space(
      defaults->color_space, flight::String("srgb"));
  bool default_color_path =
      defaults->color_space == flight::String("srgb") &&
      default_color_observation ==
          std::tuple{false, flight::String("linear"), true};
  if (mutation == "--mutate-color-space-default") {
    default_color_path = false;
  }
  check(default_color_path,
        "the default sRGB texture and sRGB working path stays byte-through with upload premultiplication");

  bool sample_space_coupling = true;
  bool premultiply_coupling = true;
  for (const auto& color_case : color_space_cases) {
    const flight::String source_space(color_case.source);
    const flight::String working_space(color_case.working);
    const bool decode = texture::should_decode_texture_on_sample(
        source_space, working_space);
    sample_space_coupling =
        sample_space_coupling &&
        texture::get_texture_sample_color_space(source_space, working_space) ==
            (decode ? flight::String("srgb") : flight::String("linear"));
    premultiply_coupling =
        premultiply_coupling &&
        texture::should_premultiply_texture_on_upload(source_space,
                                                       working_space) ==
            !decode;
  }
  if (mutation == "--mutate-color-space-sample-coupling") {
    sample_space_coupling = false;
  }
  if (mutation == "--mutate-color-space-premultiply-coupling") {
    premultiply_coupling = false;
  }
  check(sample_space_coupling,
        "sample format selects sRGB exactly when the shared predicate decodes");
  check(premultiply_coupling,
        "upload premultiplication is the exact inverse of the shared decode predicate");

  flight::String transition_source("srgb");
  flight::String transition_working("srgb");
  const auto transition_default =
      observe_color_space(transition_source, transition_working);
  transition_working = flight::String("linear");
  const auto working_first_decode =
      observe_color_space(transition_source, transition_working);
  transition_source = flight::String("linear");
  const auto working_first_linear =
      observe_color_space(transition_source, transition_working);
  transition_working = flight::String("srgb");
  const auto working_first_encoded =
      observe_color_space(transition_source, transition_working);
  transition_source = flight::String("srgb");
  const auto working_first_restored =
      observe_color_space(transition_source, transition_working);

  transition_source = flight::String("linear");
  const auto source_first_encoded =
      observe_color_space(transition_source, transition_working);
  transition_working = flight::String("linear");
  const auto source_first_linear =
      observe_color_space(transition_source, transition_working);
  transition_source = flight::String("srgb");
  const auto source_first_decode =
      observe_color_space(transition_source, transition_working);
  transition_working = flight::String("srgb");
  const auto source_first_restored =
      observe_color_space(transition_source, transition_working);
  transition_source = flight::String("srgb");
  transition_working = flight::String("srgb");
  const auto repeated_restored =
      observe_color_space(transition_source, transition_working);
  bool transition_order =
      working_first_decode == source_first_decode &&
      working_first_linear == source_first_linear &&
      working_first_encoded == source_first_encoded &&
      working_first_restored == transition_default &&
      source_first_restored == transition_default &&
      repeated_restored == transition_default;
  if (mutation == "--mutate-color-space-transition-order") {
    transition_order = false;
  }
  check(transition_order,
        "color-space decisions are order-independent, repeatable, and restore after either transition order");

  const std::array<std::pair<flight::String, flight::String>, 5>
      invalid_color_pairs{{
          {flight::String("display-p3"), flight::String("linear")},
          {flight::String("display-p3"), flight::String("srgb")},
          {flight::String("srgb"), flight::String("display-p3")},
          {flight::String("linear"), flight::String("display-p3")},
          {flight::String(""), flight::String("")},
      }};
  bool invalid_color_fallthrough = true;
  for (const auto& invalid_pair : invalid_color_pairs) {
    invalid_color_fallthrough =
        invalid_color_fallthrough &&
        observe_color_space(invalid_pair.first, invalid_pair.second) ==
            std::tuple{false, flight::String("linear"), true};
  }
  if (mutation == "--mutate-color-space-invalid") {
    invalid_color_fallthrough = false;
  }
  check(invalid_color_fallthrough,
        "erased invalid color-space strings take the conservative no-decode branch");

  using ColorDecodeFunction =
      bool (*)(types::TextureColorSpace, types::RenderTargetColorSpace);
  using ColorSampleFunction =
      types::TextureColorSpace (*)(types::TextureColorSpace,
                                   types::RenderTargetColorSpace);
  using ColorPremultiplyFunction =
      bool (*)(types::TextureColorSpace, types::RenderTargetColorSpace);
  constexpr bool color_space_absence_unrepresentable =
      !std::is_invocable_v<ColorDecodeFunction, flight::Null,
                           types::RenderTargetColorSpace> &&
      !std::is_invocable_v<ColorDecodeFunction, types::TextureColorSpace,
                           flight::Undefined> &&
      !std::is_invocable_v<ColorSampleFunction, flight::Null,
                           types::RenderTargetColorSpace> &&
      !std::is_invocable_v<ColorSampleFunction, types::TextureColorSpace,
                           flight::Undefined> &&
      !std::is_invocable_v<ColorPremultiplyFunction, flight::Null,
                           types::RenderTargetColorSpace> &&
      !std::is_invocable_v<ColorPremultiplyFunction,
                           types::TextureColorSpace,
                           flight::Undefined>;
  bool color_space_absence_domain = color_space_absence_unrepresentable;
  if (mutation == "--mutate-color-space-absence-domain") {
    color_space_absence_domain = false;
  }
  check(color_space_absence_domain,
        "the emitted color-space surface admits erased strings but no null or undefined operands");

  const auto host_video = flight::make_ref<types::HostVideoCapability>();
  host_video->get_width = [](types::HostImageSource source) { return source.video_width(); };
  host_video->get_height = [](types::HostImageSource source) { return source.video_height(); };
  host_video->is_ready = [](types::HostImageSource source) { return source.ready_state() >= 2.0; };
  const auto video_source = flight::make_ref<types::VideoResource>();
  video_source->element = flight::host_sdl::ImageSource::rgba8(
      2,
      1,
      flight::Uint8ClampedArray{255, 0, 0, 255, 0, 255, 0, 255},
      flight::host_sdl::ImageSourceKind::video_element);
  const auto video_options = flight::make_ref<types::CreateTexture2DOptions>();
  video_options->flip_x = true;
  video_options->uv_rotation = 0.5;
  const auto video = texture::create_video_texture(host_video, video_source, video_options);
  types::TextureLike video_union{
      std::in_place_type<flight::Ref<types::Texture2D>>, video};
  bool set_video_source_rejected_all = true;
  for (std::size_t index = 1; index < dimension_textures.size(); ++index) {
    const auto before_version = std::visit(
        [](const auto& value) { return value->version; }, dimension_textures[index]);
    bool rejected = false;
    try {
      texture::set_video_texture_source(host_video, dimension_textures[index], video_source);
    } catch (const flight::Error& error) {
      rejected = error.message() ==
                 flight::String("setVideoTextureSource requires a Texture2D");
    }
    if (mutation == "--mutate-set-video-source-dimension" && index == 1) rejected = false;
    set_video_source_rejected_all = set_video_source_rejected_all && rejected &&
                                    std::visit([](const auto& value) { return value->version; },
                                               dimension_textures[index]) == before_version;
  }
  check(set_video_source_rejected_all,
        "setVideoTextureSource rejects every non-2D dimension before mutation");
  const auto video_image = texture::get_video_image(video_union);
  check(video->flip_x && video->uv_rotation == 0.5,
        "video construction retains supplied 2D sampling options");
  check(video_image.has_value() && (*video_image)->source == video_source->element.value(),
        "video construction retains the borrowed host source identity");
  check(texture::get_video_texture_width(host_video, video_union) == 2.0 &&
            texture::get_video_texture_height(host_video, video_union) == 1.0,
        "video dimensions come from the host capability");
  check(texture::is_video_texture_frame_ready(host_video, video_union),
        "a decoded non-empty host frame is ready");
  const auto video_uv_options = flight::make_ref<types::CreateTexture2DOptions>();
  video_uv_options->uv_offset = flight::geometry::create_vector2(5.0, 7.0);
  video_uv_options->uv_scale = flight::geometry::create_vector2(2.0, 3.0);
  const auto video_uv =
      texture::create_video_texture(host_video, video_source, video_uv_options);
  const types::TextureLike video_uv_view{
      std::in_place_type<flight::Ref<types::Texture2D>>, video_uv};
  const auto video_uv_matrix = flight::geometry::create_matrix3();
  texture::get_video_texture_uv_matrix(video_uv_matrix, video_uv_view);
  bool video_forward_uv = video_uv_matrix->m.get_index(0.0) == 2.0 &&
                          video_uv_matrix->m.get_index(4.0) == 3.0 &&
                          video_uv_matrix->m.get_index(6.0) == 5.0 &&
                          video_uv_matrix->m.get_index(7.0) == 7.0 &&
                          video_uv_matrix->m.get_index(8.0) == 1.0;
  if (mutation == "--mutate-video-uv-forward") video_forward_uv = false;
  check(video_forward_uv,
        "video UV matrix delegates scale and offset in column-major order");
  const auto video_inverse_uv_matrix = flight::geometry::create_matrix3();
  texture::get_video_texture_inverse_uv_matrix(
      video_inverse_uv_matrix, video_uv_view);
  const auto video_mapped = apply_uv_matrix(video_uv_matrix, 0.25, 0.5);
  const auto video_recovered = apply_uv_matrix(
      video_inverse_uv_matrix, video_mapped[0], video_mapped[1]);
  bool video_inverse_uv = near(video_recovered[0], 0.25) &&
                          near(video_recovered[1], 0.5);
  if (mutation == "--mutate-video-uv-inverse") video_inverse_uv = false;
  check(video_inverse_uv,
        "video inverse UV matrix reverses the delegated forward transform");
  check(texture::advance_video_texture(host_video, video_union) == 0.0 &&
            video->version == 0.0 && (*video_image)->version == 0.0,
        "the first video advance wraps the all-ones revision to zero");
  texture::reset_video_texture_frame(video_union);
  check(video->version == texture::initial_video_version &&
            (*video_image)->version == texture::initial_video_version,
        "video reset restores both shared revisions");
  texture::destroy_video_texture(video);
  check(!video->source.has_value() && video->version == texture::initial_video_version,
        "video destruction detaches only the borrowed image source");
  texture::destroy_video_texture(video_uv);

  auto video_image_construction =
      flight::entity::allocate_entity<flight::Ref<types::ImageResource>>();
  texture::initialize_video_image_resource(
      video_image_construction, video_source->element.value(), 23.0);
  const auto initialized_video_image =
      flight::entity::finish_entity<flight::Ref<types::ImageResource>>(
          video_image_construction);
  bool video_initializer_fields =
      initialized_video_image->source == video_source->element.value() &&
      initialized_video_image->alpha_type == flight::String("straight") &&
      initialized_video_image->gamut == flight::String("srgb") &&
      initialized_video_image->kind == types::image_texture_source_kind &&
      initialized_video_image->width == 0.0 &&
      initialized_video_image->height == 0.0 &&
      initialized_video_image->version == 23.0;
  if (mutation == "--mutate-video-initializer") video_initializer_fields = false;
  check(video_initializer_fields,
        "initializeVideoImageResource retains the host handle and initializes every source field");

  const auto provider_resource = make_video_resource(1, 1);
  const auto provider_element = provider_resource->element.value();
  const auto provider_host = flight::make_ref<types::HostVideoCapability>();
  provider_host->can_play_type = [](flight::String) { return true; };
  int provider_width_calls = 0;
  int provider_height_calls = 0;
  int provider_ready_calls = 0;
  bool provider_received_borrowed_identity = true;
  provider_host->get_width = [&](types::HostImageSource received) {
    ++provider_width_calls;
    provider_received_borrowed_identity =
        provider_received_borrowed_identity && received == provider_element;
    return 34.0;
  };
  provider_host->get_height = [&](types::HostImageSource received) {
    ++provider_height_calls;
    provider_received_borrowed_identity =
        provider_received_borrowed_identity && received == provider_element;
    return 12.0;
  };
  provider_host->is_ready = [&](types::HostImageSource received) {
    ++provider_ready_calls;
    provider_received_borrowed_identity =
        provider_received_borrowed_identity && received == provider_element;
    return true;
  };
  const auto provider_texture =
      texture::create_video_texture(provider_host, provider_resource);
  const types::TextureLike provider_texture_view{
      std::in_place_type<flight::Ref<types::Texture2D>>, provider_texture};
  const auto provider_image = texture::get_video_image(provider_texture_view);
  const double provider_width =
      texture::get_video_texture_width(provider_host, provider_texture_view);
  const double provider_height =
      texture::get_video_texture_height(provider_host, provider_texture_view);
  const bool provider_ready =
      texture::is_video_texture_frame_ready(provider_host, provider_texture_view);
  bool provider_routing =
      provider_image.has_value() && (*provider_image)->source == provider_element &&
      (*provider_image)->width == 34.0 && (*provider_image)->height == 12.0 &&
      provider_width == 34.0 && provider_height == 12.0 && provider_ready &&
      provider_received_borrowed_identity && provider_width_calls == 3 &&
      provider_height_calls == 3 && provider_ready_calls == 1;
  if (mutation == "--mutate-video-provider-routing") provider_routing = false;
  check(provider_routing,
        "video construction and accessors route the borrowed handle only through the host provider");

  const auto missing_provider = flight::make_ref<types::HostVideoCapability>();
  missing_provider->can_play_type = [](flight::String) { return false; };
  const auto missing_provider_texture =
      texture::create_video_texture(missing_provider, provider_resource);
  const types::TextureLike missing_provider_view{
      std::in_place_type<flight::Ref<types::Texture2D>>, missing_provider_texture};
  const auto missing_provider_image =
      texture::get_video_image(missing_provider_view);
  bool missing_provider_sentinels =
      missing_provider_image.has_value() &&
      (*missing_provider_image)->width == 0.0 &&
      (*missing_provider_image)->height == 0.0 &&
      texture::get_video_texture_width(
          missing_provider, missing_provider_view) == -1.0 &&
      texture::get_video_texture_height(
          missing_provider, missing_provider_view) == -1.0 &&
      !texture::is_video_texture_frame_ready(
          missing_provider, missing_provider_view);
  if (mutation == "--mutate-video-missing-provider") {
    missing_provider_sentinels = false;
  }
  check(missing_provider_sentinels,
        "omitted video inspection methods retain a source but expose dimension and readiness sentinels");

  const auto absent_video_resource = flight::make_ref<types::VideoResource>();
  const auto absent_video_texture =
      texture::create_video_texture(provider_host, absent_video_resource);
  const types::TextureLike absent_video_view{
      std::in_place_type<flight::Ref<types::Texture2D>>, absent_video_texture};
  bool absent_video_sentinels =
      !texture::get_video_image(absent_video_view).has_value() &&
      !absent_video_texture->source.has_value() &&
      absent_video_texture->version == texture::initial_video_version &&
      texture::get_video_texture_width(provider_host, absent_video_view) == -1.0 &&
      texture::get_video_texture_height(provider_host, absent_video_view) == -1.0 &&
      !texture::is_video_texture_frame_ready(provider_host, absent_video_view) &&
      texture::advance_video_texture(provider_host, absent_video_view) ==
          texture::initial_video_version;
  if (mutation == "--mutate-video-absent-source") absent_video_sentinels = false;
  check(absent_video_sentinels,
        "an absent borrowed host element produces an unbound texture and stable sentinels");

  const auto readiness_resource = make_video_resource(1, 1);
  const auto readiness_host = flight::make_ref<types::HostVideoCapability>();
  readiness_host->can_play_type = [](flight::String) { return true; };
  bool readiness_flag = true;
  double readiness_width = 5.0;
  double readiness_height = 4.0;
  readiness_host->is_ready =
      [&](types::HostImageSource) { return readiness_flag; };
  readiness_host->get_width =
      [&](types::HostImageSource) { return readiness_width; };
  readiness_host->get_height =
      [&](types::HostImageSource) { return readiness_height; };
  const auto readiness_texture =
      texture::create_video_texture(readiness_host, readiness_resource);
  const types::TextureLike readiness_view{
      std::in_place_type<flight::Ref<types::Texture2D>>, readiness_texture};
  bool readiness_domain =
      texture::is_video_texture_frame_ready(readiness_host, readiness_view);
  readiness_flag = false;
  readiness_domain = readiness_domain &&
                     !texture::is_video_texture_frame_ready(
                         readiness_host, readiness_view);
  readiness_flag = true;
  readiness_width = 0.0;
  readiness_domain = readiness_domain &&
                     !texture::is_video_texture_frame_ready(
                         readiness_host, readiness_view);
  readiness_width = 5.0;
  readiness_height = 0.0;
  readiness_domain = readiness_domain &&
                     !texture::is_video_texture_frame_ready(
                         readiness_host, readiness_view);
  if (mutation == "--mutate-video-readiness-domain") readiness_domain = false;
  check(readiness_domain,
        "video readiness requires provider readiness and both positive dimensions");

  const auto lifecycle_resource = make_video_resource(1, 1);
  const auto lifecycle_element = lifecycle_resource->element.value();
  const auto lifecycle_host = flight::make_ref<types::HostVideoCapability>();
  lifecycle_host->can_play_type = [](flight::String) { return true; };
  double lifecycle_width = 3.0;
  double lifecycle_height = 2.0;
  lifecycle_host->get_width =
      [&](types::HostImageSource) { return lifecycle_width; };
  lifecycle_host->get_height =
      [&](types::HostImageSource) { return lifecycle_height; };
  lifecycle_host->is_ready = [](types::HostImageSource) { return true; };
  const auto lifecycle_options = flight::make_ref<types::CreateTexture2DOptions>();
  lifecycle_options->color_space = flight::String("linear");
  lifecycle_options->uv_offset = flight::geometry::create_vector2(2.0, 3.0);
  lifecycle_options->uv_scale = flight::geometry::create_vector2(4.0, 5.0);
  const auto lifecycle_texture = texture::create_video_texture(
      lifecycle_host, lifecycle_resource, lifecycle_options);
  types::TextureLike lifecycle_view{
      std::in_place_type<flight::Ref<types::Texture2D>>, lifecycle_texture};
  const auto lifecycle_image = texture::get_video_image(lifecycle_view).value();
  lifecycle_width = 6.0;
  lifecycle_height = 4.0;
  const double first_frame_version =
      texture::advance_video_texture(lifecycle_host, lifecycle_view);
  bool advance_state = first_frame_version == 0.0 &&
                       lifecycle_texture->version == 0.0 &&
                       lifecycle_image->version == 0.0 &&
                       lifecycle_image->width == 6.0 &&
                       lifecycle_image->height == 4.0 &&
                       texture::get_video_image(lifecycle_view).value() == lifecycle_image;
  const double second_frame_version =
      texture::advance_video_texture(lifecycle_host, lifecycle_view);
  advance_state = advance_state && second_frame_version == 1.0 &&
                  lifecycle_texture->version == 1.0 &&
                  lifecycle_image->version == 1.0;
  if (mutation == "--mutate-video-advance-state") advance_state = false;
  check(advance_state,
        "video advance refreshes dimensions and synchronizes source and texture u32 revisions");

  const auto video_clone_union =
      texture::clone_video_texture(lifecycle_view);
  const auto video_clone =
      std::get<flight::Ref<types::Texture2D>>(video_clone_union);
  const types::TextureLike video_clone_view{
      std::in_place_type<flight::Ref<types::Texture2D>>, video_clone};
  bool video_clone_ownership =
      video_clone != lifecycle_texture &&
      texture::get_video_image(video_clone_view).value() == lifecycle_image &&
      video_clone->sampler != lifecycle_texture->sampler &&
      video_clone->uv_offset != lifecycle_texture->uv_offset &&
      video_clone->uv_scale != lifecycle_texture->uv_scale &&
      video_clone->uv_offset->x == 2.0 && video_clone->uv_scale->y == 5.0 &&
      video_clone->version == 1.0;
  video_clone->uv_offset->x = 9.0;
  video_clone_ownership = video_clone_ownership &&
                          lifecycle_texture->uv_offset->x == 2.0;
  if (mutation == "--mutate-video-clone-ownership") {
    video_clone_ownership = false;
  }
  check(video_clone_ownership,
        "cloneVideoTexture shares the image source but owns fresh sampler and UV state");

  const auto copy_video_resource = make_video_resource(2, 2);
  const auto copy_video_out =
      texture::create_video_texture(lifecycle_host, copy_video_resource);
  const auto copy_video_sampler = copy_video_out->sampler;
  const auto copy_video_offset = copy_video_out->uv_offset;
  const auto copy_video_scale = copy_video_out->uv_scale;
  types::TextureLike copy_video_view{
      std::in_place_type<flight::Ref<types::Texture2D>>, copy_video_out};
  texture::copy_video_texture(copy_video_view, lifecycle_view);
  bool video_copy_ownership =
      copy_video_out->sampler == copy_video_sampler &&
      copy_video_out->uv_offset == copy_video_offset &&
      copy_video_out->uv_scale == copy_video_scale &&
      copy_video_out->sampler != lifecycle_texture->sampler &&
      copy_video_out->uv_offset != lifecycle_texture->uv_offset &&
      copy_video_out->color_space == flight::String("linear") &&
      copy_video_out->uv_scale->x == 4.0 && copy_video_out->version == 1.0 &&
      texture::get_video_image(copy_video_view).value() == lifecycle_image;
  const auto alias_video_sampler = lifecycle_texture->sampler;
  const auto alias_video_offset = lifecycle_texture->uv_offset;
  const auto alias_video_scale = lifecycle_texture->uv_scale;
  texture::copy_video_texture(lifecycle_view, lifecycle_view);
  video_copy_ownership = video_copy_ownership &&
                         lifecycle_texture->sampler == alias_video_sampler &&
                         lifecycle_texture->uv_offset == alias_video_offset &&
                         lifecycle_texture->uv_scale == alias_video_scale &&
                         lifecycle_texture->uv_scale->x == 4.0 &&
                         lifecycle_texture->version == 1.0;
  if (mutation == "--mutate-video-copy-ownership") video_copy_ownership = false;
  check(video_copy_ownership,
        "copyVideoTexture preserves destination owners, shares the image, and is alias-safe");

  texture::reset_video_texture_frame(lifecycle_view);
  bool reset_video_revision =
      lifecycle_texture->version == texture::initial_video_version &&
      lifecycle_image->version == texture::initial_video_version;
  if (mutation == "--mutate-video-reset-state") reset_video_revision = false;
  check(reset_video_revision,
        "resetVideoTextureFrame resets both shared revisions to the pre-first-frame sentinel");

  const auto replacement_video_resource = make_video_resource(4, 3);
  const auto replacement_video_element =
      replacement_video_resource->element.value();
  lifecycle_width = 9.0;
  lifecycle_height = 7.0;
  texture::set_video_texture_source(
      lifecycle_host, lifecycle_view, replacement_video_resource);
  const auto replacement_video_image =
      texture::get_video_image(lifecycle_view).value();
  bool video_source_replaced = replacement_video_image != lifecycle_image &&
                               replacement_video_image->source ==
                                   replacement_video_element &&
                               replacement_video_image->width == 9.0 &&
                               replacement_video_image->height == 7.0 &&
                               replacement_video_image->version ==
                                   texture::initial_video_version &&
                               lifecycle_texture->version ==
                                   texture::initial_video_version &&
                               lifecycle_image->source == lifecycle_element &&
                               lifecycle_resource->element.value() == lifecycle_element;
  if (mutation == "--mutate-video-source-replacement") {
    video_source_replaced = false;
  }
  check(video_source_replaced,
        "setVideoTextureSource swaps to a new borrowed image owner and leaves the old resource intact");

  texture::set_video_texture_source(
      lifecycle_host, lifecycle_view, absent_video_resource);
  bool video_source_cleared =
      !texture::get_video_image(lifecycle_view).has_value() &&
      !lifecycle_texture->source.has_value() &&
      lifecycle_texture->version == texture::initial_video_version &&
      texture::get_video_texture_width(lifecycle_host, lifecycle_view) == -1.0 &&
      texture::get_video_texture_height(lifecycle_host, lifecycle_view) == -1.0;
  if (mutation == "--mutate-video-source-clear") video_source_cleared = false;
  check(video_source_cleared,
        "setVideoTextureSource clears the texture when the replacement host element is absent");

  texture::set_video_texture_source(
      lifecycle_host, lifecycle_view, replacement_video_resource);
  const auto destroy_video_image = texture::get_video_image(lifecycle_view).value();
  texture::destroy_video_texture(lifecycle_texture);
  texture::destroy_video_texture(lifecycle_texture);
  bool video_destroy_state =
      !lifecycle_texture->source.has_value() &&
      lifecycle_texture->version == texture::initial_video_version &&
      replacement_video_resource->element.value() == replacement_video_element &&
      destroy_video_image->source == replacement_video_element &&
      texture::get_video_texture_width(lifecycle_host, lifecycle_view) == -1.0 &&
      texture::get_video_texture_height(lifecycle_host, lifecycle_view) == -1.0 &&
      !texture::is_video_texture_frame_ready(lifecycle_host, lifecycle_view) &&
      texture::advance_video_texture(lifecycle_host, lifecycle_view) ==
          texture::initial_video_version;
  if (mutation == "--mutate-video-destroy-state") video_destroy_state = false;
  check(video_destroy_state,
        "video destruction is idempotent, preserves the borrowed resource, and leaves stable sentinels");

  if (failures == 0) {
    std::cout << "texture construction, source, UV, copy, and equality match Flight\n";
  }
  return failures == 0 ? 0 : 1;
}
