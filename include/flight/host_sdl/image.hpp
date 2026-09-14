#pragma once

#include <cstddef>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <utility>

#include <flight/typed_array.hpp>

namespace flight::host_sdl {

// Native image sources share one decoded RGBA representation across GL and WebGPU. The kind retains
// the provider's source domain for future compiler-lowered external type tests without making either
// graphics API own image decoding or video-frame acquisition.
enum class ImageSourceKind {
  decoded_rgba,
  image_element,
  video_element,
  image_bitmap,
  offscreen_canvas,
  svg_image_element,
  video_frame,
};

namespace detail {

struct ImageSourceState final {
  ImageSourceKind kind;
  std::size_t width;
  std::size_t height;
  Uint8ClampedArray pixels;
};

} // namespace detail

class ImageSource final {
 public:
  using weak_type = std::weak_ptr<detail::ImageSourceState>;

  ImageSource() noexcept = default;

  [[nodiscard]] static ImageSource rgba8(
      std::size_t width,
      std::size_t height,
      Uint8ClampedArray pixels,
      ImageSourceKind kind = ImageSourceKind::decoded_rgba) {
    if (width != 0 && height > std::numeric_limits<std::size_t>::max() / width) {
      throw std::length_error("SDL image dimensions exceed addressable storage");
    }
    const auto pixel_count = width * height;
    if (pixel_count > std::numeric_limits<std::size_t>::max() / 4 ||
        pixels.size() != pixel_count * 4) {
      throw std::invalid_argument("SDL RGBA8 image byte count does not match its dimensions");
    }
    return ImageSource(std::make_shared<detail::ImageSourceState>(
        detail::ImageSourceState{kind, width, height, std::move(pixels)}));
  }

  [[nodiscard]] explicit operator bool() const noexcept { return state_ != nullptr; }
  [[nodiscard]] friend bool operator==(const ImageSource&, const ImageSource&) noexcept = default;
  [[nodiscard]] const void* identity() const noexcept { return state_.get(); }
  [[nodiscard]] ImageSourceKind kind() const noexcept {
    return state_ ? state_->kind : ImageSourceKind::decoded_rgba;
  }
  [[nodiscard]] std::size_t width() const noexcept { return state_ ? state_->width : 0; }
  [[nodiscard]] std::size_t height() const noexcept { return state_ ? state_->height : 0; }
  [[nodiscard]] std::span<const Uint8Clamped> rgba8_pixels() const noexcept {
    return state_ ? state_->pixels.span() : std::span<const Uint8Clamped>{};
  }
  [[nodiscard]] weak_type weaken() const noexcept { return state_; }

  [[nodiscard]] static std::optional<ImageSource> lock_weak(const weak_type& weak) noexcept {
    auto state = weak.lock();
    return state ? std::optional<ImageSource>(ImageSource(std::move(state))) : std::nullopt;
  }

 private:
  explicit ImageSource(std::shared_ptr<detail::ImageSourceState> state) noexcept
      : state_(std::move(state)) {}

  std::shared_ptr<detail::ImageSourceState> state_;
};

struct ImageSourceWeakPolicy final {
  using key_type = ImageSource;
  using weak_type = ImageSource::weak_type;
  using identity_type = const void*;

  [[nodiscard]] static weak_type weaken(const key_type& key) noexcept { return key.weaken(); }
  [[nodiscard]] static std::optional<key_type> lock(const weak_type& key) noexcept {
    return key_type::lock_weak(key);
  }
  [[nodiscard]] static identity_type identity(const key_type& key) noexcept { return key.identity(); }
  [[nodiscard]] static std::size_t hash(identity_type identity) noexcept {
    return std::hash<const void*>{}(identity);
  }
  [[nodiscard]] static bool equal(identity_type left, identity_type right) noexcept {
    return left == right;
  }
};

} // namespace flight::host_sdl

namespace std {

template <>
struct hash<flight::host_sdl::ImageSource> {
  [[nodiscard]] size_t operator()(const flight::host_sdl::ImageSource& value) const noexcept {
    return hash<const void*>{}(value.identity());
  }
};

} // namespace std
