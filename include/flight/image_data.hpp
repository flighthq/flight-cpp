#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <new>
#include <optional>
#include <utility>

#include <flight/dom_exception.hpp>
#include <flight/error.hpp>
#include <flight/math.hpp>
#include <flight/string.hpp>
#include <flight/typed_array.hpp>

namespace flight {

struct ImageDataSettings final {
  std::optional<String> color_space;
};

// CPU-backed RGBA pixels with the Web ImageData surface used by the pinned Flight SDK. The data
// constructor retains the supplied typed-array object. Copies retain both ImageData identity and
// the same live pixel storage, matching copies of other reference-owned external bindings.
class ImageData final {
 public:
  explicit ImageData(double source_width, double source_height,
                     ImageDataSettings settings = {})
      : ImageData(validate_dimensions(source_width, source_height), std::move(settings),
                  AllocateTag{}) {}

  explicit ImageData(Uint8ClampedArray source, double source_width,
                     std::optional<double> source_height = std::nullopt,
                     ImageDataSettings settings = {})
      : ImageData(validate_source(source, source_width, source_height), std::move(source),
                  std::move(settings), SourceTag{}) {}

  Uint8ClampedArray data;
  double width{};
  double height{};
  String color_space;

  [[nodiscard]] const void* identity() const noexcept { return identity_.get(); }

  [[nodiscard]] friend bool operator==(const ImageData& left, const ImageData& right) noexcept {
    return left.identity_ == right.identity_;
  }

 private:
  struct AllocateTag final {};
  struct SourceTag final {};
  struct Dimensions final {
    std::uint32_t width;
    std::uint32_t height;
  };
  struct Identity final {};

  ImageData(Dimensions dimensions, ImageDataSettings settings, AllocateTag)
      : data(allocate_pixels(dimensions)),
        width(static_cast<double>(dimensions.width)),
        height(static_cast<double>(dimensions.height)),
        color_space(resolve_color_space(settings)),
        identity_(std::make_shared<Identity>()) {}

  ImageData(Dimensions dimensions, Uint8ClampedArray source, ImageDataSettings settings,
            SourceTag)
      : data(std::move(source)),
        width(static_cast<double>(dimensions.width)),
        height(static_cast<double>(dimensions.height)),
        color_space(resolve_color_space(settings)),
        identity_(std::make_shared<Identity>()) {}

  [[nodiscard]] static Dimensions validate_dimensions(double source_width,
                                                       double source_height) {
    const auto normalized_width = detail::number_to_uint32(source_width);
    const auto normalized_height = detail::number_to_uint32(source_height);
    if (normalized_width == 0 || normalized_height == 0) {
      throw DOMException("ImageData dimensions must be greater than zero", "IndexSizeError");
    }
    return {normalized_width, normalized_height};
  }

  [[nodiscard]] static Dimensions validate_source(const Uint8ClampedArray& source,
                                                  double source_width,
                                                  std::optional<double> source_height) {
    constexpr std::size_t components_per_pixel = 4;
    if (source.byte_length == 0 || source.byte_length % components_per_pixel != 0) {
      throw DOMException("ImageData source length is not an integral number of pixels",
                         "InvalidStateError");
    }

    const auto normalized_width = detail::number_to_uint32(source_width);
    const auto pixel_count = source.byte_length / components_per_pixel;
    if (normalized_width == 0 || pixel_count % normalized_width != 0) {
      throw DOMException("ImageData source length is inconsistent with its width",
                         "IndexSizeError");
    }
    const auto derived_height = pixel_count / normalized_width;
    if (derived_height > std::numeric_limits<std::uint32_t>::max()) {
      throw DOMException("ImageData source height is outside the Web IDL range",
                         "IndexSizeError");
    }
    if (source_height.has_value() &&
        detail::number_to_uint32(*source_height) != derived_height) {
      throw DOMException("ImageData source length is inconsistent with its height",
                         "IndexSizeError");
    }
    return {normalized_width, static_cast<std::uint32_t>(derived_height)};
  }

  [[nodiscard]] static std::size_t pixel_element_count(Dimensions dimensions) {
    constexpr std::size_t components_per_pixel = 4;
    constexpr auto maximum = std::numeric_limits<std::size_t>::max();
    const auto normalized_width = static_cast<std::size_t>(dimensions.width);
    const auto normalized_height = static_cast<std::size_t>(dimensions.height);
    if (normalized_width > maximum / components_per_pixel ||
        normalized_height > maximum / (normalized_width * components_per_pixel)) {
      throw RangeError("ImageData storage exceeds addressable memory");
    }
    return normalized_width * normalized_height * components_per_pixel;
  }

  [[nodiscard]] static Uint8ClampedArray allocate_pixels(Dimensions dimensions) {
    try {
      return Uint8ClampedArray(pixel_element_count(dimensions));
    } catch (const std::bad_alloc&) {
      throw RangeError("ImageData pixel storage could not be allocated");
    } catch (const std::length_error&) {
      throw RangeError("ImageData pixel storage could not be allocated");
    }
  }

  [[nodiscard]] static String resolve_color_space(const ImageDataSettings& settings) {
    const auto color_space = settings.color_space.value_or(String("srgb"));
    if (color_space != String("srgb") && color_space != String("display-p3")) {
      throw TypeError("ImageData colorSpace is not a supported Web IDL enum value");
    }
    return color_space;
  }

  std::shared_ptr<Identity> identity_;
};

} // namespace flight
