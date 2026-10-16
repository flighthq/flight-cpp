// Native test for @flighthq/swf's video texture acquisition (acquireSwfVideoTexture in swfVideoHandler.ts) at a627849.
//
// The module emits under source patch swf-video-texture-2d-constructor, which names createTexture2D where the source
// calls the dimension-less createTexture. That rewrite is only safe if the texture a video character receives is the
// one createTexture's 2D overload builds, so this test pins exactly that, for both smoothing values:
//
//   * every TypeScript default of the 2D branch -- srgb, no flips, identity uv transform, version 0, null source, and a
//     sampler whose filters follow the definition's smoothing with mipmaps off, anisotropy 1 and clamp-to-edge wrap;
//   * field-for-field agreement with create_texture over the exact CreateTexture2DOptions owner, the overload
//     TypeScript selected for the original call;
//   * identity: the texture is cached per character, a repeated acquire returns the same object, and another character
//     gets a distinct texture that shares neither its sampler nor its uv vectors.
//
// The handler object itself (swfVideoHandler) stays refused for its callable union arm; see
// docs/compiler-requests/callable-arm-in-contextual-union.md.
#include <flight/swf/swf_video_handler.hpp>

#include <iostream>
#include <string>

namespace {
int failures = 0;
void check(bool ok, const std::string& what) {
  if (!ok) {
    ++failures;
    std::cerr << "FAIL: " << what << "\n";
  }
}

using flight::types::Sampler;
using flight::types::Texture2D;

// Reference: the overload TypeScript selected for the original `createTexture({ sampler })` call,
// invoked through the exact-owner carrier (the override forwards it to create_texture_2d).
flight::Ref<Texture2D> reference_texture(bool smoothing) {
  auto opts = flight::make_ref<flight::types::CreateTexture2DOptions>();
  opts->sampler = flight::texture::create_sampler(
      flight::make_structural_ref<flight::RowReadonly<flight::RowPartial<flight::RowOf<flight::types::SamplerLike>>>>(
          flight::row_field<flight::RowKey<"magFilter">>(std::optional<flight::String>{flight::String(smoothing ? "linear" : "nearest")}),
          flight::row_field<flight::RowKey<"minFilter">>(std::optional<flight::String>{flight::String(smoothing ? "linear" : "nearest")}),
          flight::row_field<flight::RowKey<"mipmaps">>(std::optional<bool>{false})));
  return flight::texture::create_texture(std::optional<flight::Ref<flight::types::CreateTexture2DOptions>>{opts});
}

void same_texture_state(const flight::Ref<Texture2D>& a, const flight::Ref<Texture2D>& b, const std::string& label) {
  check(a->color_space == b->color_space, label + " colorSpace");
  check(a->flip_x == b->flip_x && a->flip_y == b->flip_y, label + " flip");
  check(a->uv_offset->x == b->uv_offset->x && a->uv_offset->y == b->uv_offset->y, label + " uvOffset");
  check(a->uv_scale->x == b->uv_scale->x && a->uv_scale->y == b->uv_scale->y, label + " uvScale");
  check(a->uv_rotation == b->uv_rotation, label + " uvRotation");
  check(a->version == b->version, label + " version");
  check(a->dimension == b->dimension, label + " dimension");
  check(a->source.has_value() == b->source.has_value(), label + " source presence");
  check(a->sampler->anisotropy == b->sampler->anisotropy, label + " sampler.anisotropy");
  check(a->sampler->mag_filter == b->sampler->mag_filter, label + " sampler.magFilter");
  check(a->sampler->min_filter == b->sampler->min_filter, label + " sampler.minFilter");
  check(a->sampler->mipmaps == b->sampler->mipmaps, label + " sampler.mipmaps");
  check(a->sampler->wrap_u == b->sampler->wrap_u && a->sampler->wrap_v == b->sampler->wrap_v, label + " sampler.wrap");
}

void run(bool smoothing) {
  const std::string label = smoothing ? "smoothing" : "no-smoothing";
  auto parsed = flight::make_ref<flight::types::SwfTagParseResult>();
  auto definition = flight::make_ref<flight::swf::SwfVideoDefinition>();
  definition->smoothing = smoothing;
  definition->width = 32;
  definition->height = 16;
  using Parsed = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::SwfTagParseResult>>>>;
  using Definition = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::swf::SwfVideoDefinition>>>>;

  auto texture = flight::swf::acquire_swf_video_texture(Parsed(parsed), 7.0, Definition(definition));

  // Absolute TypeScript defaults of createTexture's 2D branch.
  const flight::String expected_filter(smoothing ? "linear" : "nearest");
  check(texture->color_space == flight::String("srgb"), label + " default colorSpace srgb");
  check(!texture->flip_x && !texture->flip_y, label + " default flips false");
  check(texture->uv_offset->x == 0 && texture->uv_offset->y == 0, label + " default uvOffset (0,0)");
  check(texture->uv_scale->x == 1 && texture->uv_scale->y == 1, label + " default uvScale (1,1)");
  check(texture->uv_rotation == 0, label + " default uvRotation 0");
  check(texture->version == 0, label + " default version 0");
  check(texture->dimension == flight::String("2d"), label + " dimension 2d");
  check(!texture->source.has_value(), label + " source null");
  check(texture->sampler->mag_filter == expected_filter, label + " magFilter from smoothing");
  check(texture->sampler->min_filter == expected_filter, label + " minFilter from smoothing");
  check(texture->sampler->mipmaps == false, label + " mipmaps false");
  check(texture->sampler->anisotropy == 1, label + " anisotropy default 1");
  check(texture->sampler->wrap_u == flight::String("clamp-to-edge") && texture->sampler->wrap_v == flight::String("clamp-to-edge"), label + " wrap default clamp-to-edge");

  // Same state as the overload TypeScript selected for the original call.
  same_texture_state(texture, reference_texture(smoothing), label + " vs createTexture");

  // Identity: cached per character, distinct per character, fresh uv vectors and sampler per texture.
  check(parsed->video_textures.get(7.0).has_value() && parsed->video_textures.get(7.0).value() == texture, label + " cached under characterId");
  auto again = flight::swf::acquire_swf_video_texture(Parsed(parsed), 7.0, Definition(definition));
  check(again == texture, label + " repeated acquire returns the same texture identity");
  auto other = flight::swf::acquire_swf_video_texture(Parsed(parsed), 8.0, Definition(definition));
  check(other != texture, label + " another character gets a distinct texture");
  check(other->sampler != texture->sampler, label + " samplers are not shared across textures");
  check(other->uv_offset != texture->uv_offset && other->uv_scale != texture->uv_scale, label + " uv vectors are not shared");
  check(parsed->video_textures.size() == 2, label + " exactly two cache entries");
}
}  // namespace

int main() {
  run(true);
  run(false);
  if (failures == 0) std::cout << "swf video texture: all checks passed\n";
  return failures == 0 ? 0 : 1;
}
