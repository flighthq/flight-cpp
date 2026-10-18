// Consumes the whole original public header, including its shared texture-factory dependency.
// Source cases: Flight a627849, scene3d-formats/src/objMaterialHelpers.test.ts.
#include <flight/image/image_resource_reference.hpp>
#include <flight/scene3d_formats/obj_material_helpers.hpp>
#include <flight/types/image_resource_reference.hpp>
#include <flight/types/scene_3d_document.hpp>
#include <flight/types/texture.hpp>

#include <cmath>
#include <iostream>
#include <limits>

namespace formats = flight::scene3d_formats;
namespace types = flight::types;

namespace {
int failures = 0;
void check(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    ++failures;
  }
}

flight::Ref<types::Texture2D> two_d(const types::Texture& texture) {
  return std::get<flight::Ref<types::Texture2D>>(texture);
}

void clamp_and_pack() {
  check(formats::clamp_channel(0.0) == 0.0, "clamp 0");
  check(formats::clamp_channel(1.0) == 255.0, "clamp 1");
  check(formats::clamp_channel(0.5) == 128.0, "clamp midpoint rounds up");
  check(formats::clamp_channel(-0.5) == 0.0, "clamp negative");
  check(formats::clamp_channel(1.5) == 255.0, "clamp above one");
  check(formats::pack_obj_color({1.0, 1.0, 1.0}, 1.0) == 4294967295.0,
        "white opaque is unsigned 0xffffffff");
  check(formats::pack_obj_color({0.0, 0.0, 0.0}, 0.0) == 0.0,
        "black transparent is zero");
  check(formats::pack_obj_color({1.0, 0.0, 0.0}, 1.0) == 4278190335.0,
        "red opaque is unsigned 0xff0000ff");
  check(formats::clamp_channel(0.5 / 255.0) == 1.0 &&
            formats::clamp_channel(std::nextafter(0.5 / 255.0, 0.0)) == 0.0,
        "rounding boundary keeps both adjacent values");
  check(!std::signbit(formats::clamp_channel(-0.0)), "clamp negative zero to positive zero");
  const auto infinity = std::numeric_limits<double>::infinity();
  const auto nan = std::numeric_limits<double>::quiet_NaN();
  check(formats::clamp_channel(infinity) == 255.0 &&
            formats::clamp_channel(-infinity) == 0.0 &&
            std::isnan(formats::clamp_channel(nan)),
        "clamp infinity and NaN follow Math.min/Math.max");
  check(formats::pack_obj_color({nan, -infinity, infinity}, nan) == 65280.0,
        "bitwise packing converts NaN to zero and clamps infinities");
  check(formats::pack_obj_color({0.5, -1.0, 2.0}, 0.25) == 2147548992.0,
        "RGBA order, clamping, fractional alpha and unsigned high bit");
}

void external_obj_texture() {
  auto document = flight::make_ref<types::Scene3DDocument>();
  check(!formats::external_obj_texture(std::nullopt, document, flight::String("srgb")) &&
            document->resources.empty(),
        "null URI has no texture or resource side effect");
  const auto first = formats::external_obj_texture(
      flight::String("diffuse.png"), document, flight::String("srgb"));
  check(first.has_value(), "non-null URI returns a texture");
  if (!first) return;
  const auto first_texture = two_d(*first);
  check(first_texture->color_space == flight::String("srgb"), "source srgb case");
  const auto second = formats::external_obj_texture(
      flight::String("diffuse.png"), document, flight::String("linear"));
  check(second.has_value(), "second non-null URI returns a texture");
  if (!second) return;
  const auto second_texture = two_d(*second);
  check(second_texture->color_space == flight::String("linear") &&
            first_texture->color_space == flight::String("srgb") &&
            first_texture != second_texture,
        "color-space write reaches the actual fresh owner");
  check(document->resources.size() == 1, "matching URI/basePath reuses resource");
  const auto resource = std::get<flight::Ref<types::ExternalImageResourceReference>>(
      document->resources.element(0.0));
  check(resource->uri == flight::String("diffuse.png") && !resource->base_path &&
            resource->state == flight::String("Unresolved") && !resource->failure &&
            resource->textures && resource->textures->size() == 2 &&
            two_d(resource->textures->element(0.0)) == first_texture &&
            two_d(resource->textures->element(1.0)) == second_texture,
        "same unresolved resource retains returned texture owners in construction order");
  check(!first_texture->source && first_texture->sampler &&
            first_texture->uv_scale->x == 1.0 && first_texture->uv_scale->y == 1.0,
        "resource-only options retain 2D defaults without decoding");
  const auto empty_uri = formats::external_obj_texture(
      flight::String(""), document, flight::String("linear"));
  check(empty_uri && document->resources.size() == 2 &&
            two_d(*empty_uri)->color_space == flight::String("linear"),
        "empty URI stays distinct from null and still constructs");
}

void shared_resource_selection() {
  const auto first = flight::image::create_external_image_resource_reference(flight::String("a"));
  const auto duplicate = flight::image::create_external_image_resource_reference(flight::String("a"));
  flight::Array<types::ImageResourceReference> resources{first, duplicate};
  const auto texture = two_d(formats::create_external_texture_ref(
      flight::String("a"), std::nullopt, resources));
  check(resources.size() == 2 && first->textures && first->textures->size() == 1 &&
            two_d(first->textures->element(0.0)) == texture &&
            duplicate->textures && duplicate->textures->empty(),
        "first matching external resource wins without replacing either owner");
  const auto based = two_d(formats::create_external_texture_ref(
      flight::String("a"), std::optional<flight::String>{flight::String("")}, resources));
  check(resources.size() == 3 && based != texture &&
            std::get<flight::Ref<types::ExternalImageResourceReference>>(
                resources.element(2.0))->base_path == std::optional<flight::String>{flight::String("")},
        "present empty basePath remains distinct from null");
  check(two_d(formats::create_external_texture_ref(flight::String("standalone")))->dimension ==
            flight::String("2d"),
        "absent resources array remains accepted");

  flight::Array<types::ImageResourceReference> embedded;
  flight::Uint8Array bytes(2.0);
  bytes.set_index(0.0, 23.0);
  const auto original = two_d(formats::create_embedded_texture_ref(bytes, std::nullopt, embedded));
  const auto reused = two_d(formats::create_embedded_texture_ref(bytes, std::nullopt, embedded));
  const auto resource = std::get<flight::Ref<types::EmbeddedImageResourceReference>>(
      embedded.element(0.0));
  check(embedded.size() == 1 && original != reused && resource->bytes == bytes &&
            resource->textures && resource->textures->size() == 2 &&
            two_d(resource->textures->element(0.0)) == original &&
            two_d(resource->textures->element(1.0)) == reused && !resource->mime_type &&
            resource->state == flight::String("Unresolved"),
        "embedded byte identity deduplicates resources while every texture stays fresh");
  flight::Uint8Array equal_bytes(2.0);
  equal_bytes.set_index(0.0, 23.0);
  formats::create_embedded_texture_ref(equal_bytes, std::nullopt, embedded);
  formats::create_embedded_texture_ref(bytes, flight::String(""), embedded);
  check(embedded.size() == 3, "equal bytes with different identity and empty MIME stay distinct");
}
}  // namespace

int main() {
  clamp_and_pack();
  external_obj_texture();
  shared_resource_selection();
  return failures == 0 ? 0 : 1;
}
