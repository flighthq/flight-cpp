#include <flight/texture/video_texture.hpp>

#include <array>
#include <iostream>
#include <string>
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

  if (failures == 0) {
    std::cout << "texture construction, cloning, copy, and equality match Flight\n";
  }
  return failures == 0 ? 0 : 1;
}
