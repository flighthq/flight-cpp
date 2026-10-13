#include <flight/image/_internal_image_test_helper.hpp>
#include <flight/image/image_resource.hpp>
#include <flight/image/image_source_dimensions.hpp>

#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

// Native ports of every portable case in imageResource.test.ts, imageSourceDimensions.test.ts, and
// imageTestHelper.test.ts at Flight a627849, plus byte-layout and native-carrier bounds evidence.
// imageResourceFrom.test.ts remains outside this target because imageResourceFrom.ts is refused on
// the browser URL/location contract (browser-url-location-runtime-bindings.md); image-codec remains
// its own follow-on unit. imageResourceReference.test.ts remains blocked by the independently filed
// erased-value-stringification.md and compiler-interface-heritage-owner-widening.md requests. This
// target does not counterfeit either missing behavior with a test-only implementation.
//
// MUTATION SENSITIVITY, measured 2026-10-12 from a clean committed baseline and then restored:
//   * replacing cloneImageResource's borrowed source with an empty carrier fails the source-identity check;
//   * changing isImageResourceEmpty's width-or-height condition to require both dimensions fails the
//     one-zero-dimension bounds check.

namespace {

using flight::Ref;
using flight::String;
using flight::StructuralRef;
using flight::Uint8Array;
using flight::Uint8ClampedArray;
using flight::host_sdl::ImageSource;
using flight::types::CompressedImageData;
using flight::types::CompressedImageResource;
using flight::types::HostImageDimensions;
using flight::types::ImageResource;
using flight::types::TextureContainer;
using flight::types::TextureContainerLevel;
namespace image = flight::image;

int failures = 0;

void check(bool condition, const std::string& message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

template <typename Exception, typename Callable>
void check_throws(Callable&& callable, const std::string& message) {
  try {
    callable();
  } catch (const Exception&) {
    return;
  } catch (...) {
  }
  check(false, message);
}

auto readonly_image(const Ref<ImageResource>& resource) {
  using View = StructuralRef<flight::RowReadonly<flight::RowOf<Ref<ImageResource>>>>;
  return flight::structural_ref_cast<View>(resource);
}

auto readonly_compressed(const Ref<CompressedImageData>& compressed) {
  using View = StructuralRef<flight::RowReadonly<flight::RowOf<Ref<CompressedImageData>>>>;
  return flight::structural_ref_cast<View>(compressed);
}

Uint8ClampedArray rgba_fixture() {
  return Uint8ClampedArray{
      0x01, 0x02, 0x03, 0x04,
      0x11, 0x12, 0x13, 0x14,
      0x21, 0x22, 0x23, 0x24,
      0x31, 0x32, 0x33, 0x34,
  };
}

void test_native_pixel_layout_and_bounds() {
  const auto pixels = rgba_fixture();
  const auto source = ImageSource::rgba8(2, 2, pixels);
  const auto view = source.rgba8_pixels();
  check(source.width() == 2 && source.height() == 2, "RGBA8 source preserves pixel dimensions");
  check(view.size() == 16, "RGBA8 source exposes exactly width * height * 4 bytes");
  for (std::size_t i = 0; i < view.size(); ++i) {
    check(static_cast<std::uint8_t>(view[i]) == static_cast<std::uint8_t>(pixels[i]),
          "RGBA8 source preserves byte-exact channel order at index " + std::to_string(i));
  }
  check_throws<std::invalid_argument>(
      [] { static_cast<void>(ImageSource::rgba8(2, 2, Uint8ClampedArray(15))); },
      "RGBA8 source rejects a short pixel buffer");
  check_throws<std::invalid_argument>(
      [] { static_cast<void>(ImageSource::rgba8(1, 1, Uint8ClampedArray(5))); },
      "RGBA8 source rejects a long pixel buffer");
  check_throws<std::length_error>(
      [] {
        static_cast<void>(ImageSource::rgba8(
            std::numeric_limits<std::size_t>::max(), 2, Uint8ClampedArray{}));
      },
      "RGBA8 source rejects overflowing dimensions before multiplying them");
}

void test_dimension_resolver_contract() {
  image::unregister_host_image_dimension_resolver();
  const auto source = ImageSource::rgba8(2, 2, rgba_fixture());
  const auto out = flight::make_ref<HostImageDimensions>(HostImageDimensions{.height = 11, .width = 22});
  check(!image::has_host_image_dimension_resolver(), "dimension resolver starts empty");
  check(!image::get_host_image_source_dimensions(source, out), "missing dimension resolver reports false");
  check(out->height == 11 && out->width == 22, "missing resolver leaves caller output untouched");

  image::register_host_image_dimension_resolver(
      [](ImageSource sized, Ref<HostImageDimensions> dimensions) {
        dimensions->height = static_cast<double>(sized.height());
        dimensions->width = static_cast<double>(sized.width());
        return true;
      });
  check(image::has_host_image_dimension_resolver(), "dimension resolver reports installed state");
  check(image::get_host_image_source_dimensions(source, out), "registered resolver accepts the source");
  check(out->height == 2 && out->width == 2, "registered resolver writes exact dimensions");

  image::register_host_image_dimension_resolver(
      [](ImageSource, Ref<HostImageDimensions>) { return false; });
  out->height = 5;
  out->width = 7;
  check(!image::get_host_image_source_dimensions(source, out), "declining resolver reports false");
  check(out->height == 5 && out->width == 7, "declining resolver leaves caller output untouched");

  image::register_host_image_dimension_resolver(
      [](ImageSource, Ref<HostImageDimensions> dimensions) {
        dimensions->height = 2;
        dimensions->width = 3;
        return true;
      });
  check(image::get_host_image_source_dimensions(source, out) && out->height == 2 && out->width == 3,
        "last dimension-resolver registration wins");
  image::unregister_host_image_dimension_resolver();
  check(!image::has_host_image_dimension_resolver(), "unregister clears dimension resolver");
}

void test_image_resource_identity_and_invalidation() {
  image::register_test_image_dimension_resolver();
  const auto source = ImageSource::rgba8(2, 2, rgba_fixture());
  const auto resource = image::create_image_resource(source);
  check(resource->source == source && resource->source.identity() == source.identity(),
        "image resource borrows the exact host source identity");
  check(resource->width == 2 && resource->height == 2, "image resource measures its source");
  check(resource->kind == String("image") && resource->gamut == String("srgb"),
        "image resource declares image kind and decoded gamut");
  check(resource->alpha_type == String("straight"), "image resource declares straight alpha");
  check(resource->version == 0, "image resource begins at version zero");

  resource->version = 3;
  const auto copy = image::clone_image_resource(readonly_image(resource));
  check(copy.get() != resource.get(), "clone has an independent entity identity");
  check(copy->source.identity() == resource->source.identity(), "clone shares the borrowed host source");
  check(copy->version == 3, "clone copies the current version");
  copy->version += 1;
  check(resource->version == 3 && copy->version == 4, "clone version mutates independently");

  double measured_width = 4;
  double measured_height = 5;
  image::register_host_image_dimension_resolver(
      [&](ImageSource, Ref<HostImageDimensions> dimensions) {
        dimensions->height = measured_height;
        dimensions->width = measured_width;
        return true;
      });
  const auto changing = image::create_image_resource(source);
  check(changing->width == 4 && changing->height == 5, "resource consumes initial host measurement");
  measured_width = 12;
  measured_height = 9;
  image::invalidate_image_resource(changing);
  check(changing->width == 12 && changing->height == 9, "invalidation refreshes host dimensions");
  check(changing->version == 1, "invalidation advances the version");
  changing->version = 4294967295.0;
  image::invalidate_image_resource(changing);
  check(changing->version == 0, "invalidation wraps version as an unsigned 32-bit counter");

  image::unregister_host_image_dimension_resolver();
  const auto unmeasured = image::create_image_resource(source);
  check(unmeasured->width == 0 && unmeasured->height == 0,
        "resource retains zero initialization without a resolver");
  check(image::is_image_resource_empty(readonly_image(unmeasured)), "zero-sized image resource is empty");
  unmeasured->width = 0;
  unmeasured->height = 3;
  check(image::is_image_resource_empty(readonly_image(unmeasured)), "one zero dimension is enough to be empty");
  unmeasured->width = 2;
  check(!image::is_image_resource_empty(readonly_image(unmeasured)), "positive dimensions are not empty");
}

void test_compressed_resource_borrowing() {
  const auto level = flight::make_ref<TextureContainerLevel>(TextureContainerLevel{
      .byte_offset = 0,
      .byte_length = 16,
      .width = 4,
      .height = 4,
  });
  const auto container = flight::make_ref<TextureContainer>(TextureContainer{
      .format = String("bc3"),
      .width = 4,
      .height = 4,
      .depth = 1,
      .mip_levels = 1,
      .layers = 1,
      .faces = 1,
      .supercompression = String("None"),
      .levels = flight::Array<Ref<TextureContainerLevel>>{level},
  });
  Uint8Array payload(16);
  for (std::size_t i = 0; i < payload.size(); ++i) payload[i] = static_cast<std::uint8_t>(0xa0 + i);
  const auto compressed = flight::make_ref<CompressedImageData>(CompressedImageData{
      .container = container,
      .payload = payload,
  });
  const auto resource = image::create_compressed_image_resource(readonly_compressed(compressed));
  check(resource->compressed.get() == compressed.get(), "compressed resource borrows the data identity");
  check(resource->compressed->payload.identity() == payload.identity(), "compressed resource shares payload storage");
  check(resource->width == 4 && resource->height == 4, "compressed resource uses container dimensions");
  check(resource->kind == String("compressedImage") && resource->alpha_type == String("straight"),
        "compressed resource declares kind and decoded alpha");
  for (std::size_t i = 0; i < payload.size(); ++i) {
    check(resource->compressed->payload[i] == static_cast<std::uint8_t>(0xa0 + i),
          "compressed payload remains byte-exact at index " + std::to_string(i));
  }
}

void test_portable_helper_lifecycle() {
  image::unregister_test_image_dimension_resolver();
  const auto source = ImageSource::rgba8(2, 2, rgba_fixture());
  const auto out = flight::make_ref<HostImageDimensions>(HostImageDimensions{.height = 5, .width = 7});
  image::register_test_image_dimension_resolver();
  check(image::has_host_image_dimension_resolver(), "portable helper installs the resolver slot");
  check(image::test_image_dimension_resolver(source, out), "portable helper accepts a sized native source");
  check(out->height == 2 && out->width == 2, "portable helper reports exact dimensions");
  const ImageSource empty;
  out->height = 5;
  out->width = 7;
  check(!image::test_image_dimension_resolver(empty, out), "portable helper declines an empty source");
  check(out->height == 5 && out->width == 7, "portable helper decline leaves output untouched");
  image::unregister_test_image_dimension_resolver();
  check(!image::has_host_image_dimension_resolver(), "portable helper cleanup clears the resolver slot");
}

}  // namespace

int main() {
  test_native_pixel_layout_and_bounds();
  test_dimension_resolver_contract();
  test_image_resource_identity_and_invalidation();
  test_compressed_resource_borrowing();
  test_portable_helper_lifecycle();
  if (failures != 0) {
    std::cerr << failures << " image assertion(s) failed\n";
    return 1;
  }
  std::cout << "image native layout and resource evidence passed\n";
  return 0;
}
