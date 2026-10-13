#include <flight/texture/video_texture.hpp>

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
    std::cout << "texture construction, cloning, and equality match Flight\n";
  }
  return failures == 0 ? 0 : 1;
}
