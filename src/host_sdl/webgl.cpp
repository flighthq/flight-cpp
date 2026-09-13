#include <flight/host_sdl/webgl.hpp>

#include <flight/host_sdl/gl.hpp>

#include <cmath>
#include <functional>
#include <limits>
#include <stdexcept>
#include <utility>

namespace flight::host_sdl::detail {

struct GlSurfaceState final {
  explicit GlSurfaceState(WindowOptions options) : window(std::move(options)), context(window) {}

  Window window;
  GlContext context;
};

struct GlImageSourceState final {
  std::size_t width;
  std::size_t height;
  Uint8ClampedArray pixels;
};

WebGlObjectState::WebGlObjectState(
    std::weak_ptr<GlSurfaceState> owner_value,
    WebGlObjectKind kind_value,
    std::uint32_t name_value) noexcept
    : owner(std::move(owner_value)), kind(kind_value), name(name_value) {}

// Explicit WebGL delete calls will clear names once command forwarding lands. Until then no object
// state is constructed, and this destructor deliberately has no GL side effect.
WebGlObjectState::~WebGlObjectState() noexcept = default;

} // namespace flight::host_sdl::detail

namespace flight::host_sdl {

GlImageSource GlImageSource::rgba8(
    std::size_t width,
    std::size_t height,
    Uint8ClampedArray pixels) {
  if (width != 0 && height > std::numeric_limits<std::size_t>::max() / width) {
    throw std::length_error("SDL GL image dimensions exceed addressable storage");
  }
  const auto pixel_count = width * height;
  if (pixel_count > std::numeric_limits<std::size_t>::max() / 4 ||
      pixels.size() != pixel_count * 4) {
    throw std::invalid_argument("SDL GL RGBA8 image byte count does not match its dimensions");
  }
  return GlImageSource(
      std::make_shared<detail::GlImageSourceState>(
          detail::GlImageSourceState{width, height, std::move(pixels)}));
}

std::size_t GlImageSource::width() const noexcept { return state_ ? state_->width : 0; }

std::size_t GlImageSource::height() const noexcept { return state_ ? state_->height : 0; }

std::span<const Uint8Clamped> GlImageSource::rgba8_pixels() const noexcept {
  return state_ ? state_->pixels.span() : std::span<const Uint8Clamped>{};
}

std::optional<GlImageSource> GlImageSource::lock_weak(const weak_type& weak) noexcept {
  auto state = weak.lock();
  return state ? std::optional<GlImageSource>(GlImageSource(std::move(state))) : std::nullopt;
}

std::size_t GlImageSourceWeakPolicy::hash(identity_type identity) noexcept {
  return std::hash<const void*>{}(identity);
}

int WebGl2Context::drawing_buffer_width() const { return require_state().window.pixel_size().width; }

int WebGl2Context::drawing_buffer_height() const { return require_state().window.pixel_size().height; }

SDL_FunctionPointer WebGl2Context::function_address(std::string_view name) const {
  return require_state().context.function_address(name);
}

void WebGl2Context::make_current() const {
  auto& state = require_state();
  state.context.make_current(state.window);
}

void WebGl2Context::present() const {
  auto& state = require_state();
  state.context.swap(state.window);
}

void WebGl2Context::set_swap_interval(int interval) const {
  require_state().context.set_swap_interval(interval);
}

detail::GlSurfaceState& WebGl2Context::require_state() const {
  if (!state_) throw std::logic_error("SDL WebGL context is empty");
  return *state_;
}

GlCanvas GlCanvas::create(int width, int height, double pixel_ratio, String title, bool hidden) {
  if (width <= 0 || height <= 0) throw std::invalid_argument("SDL GL canvas dimensions must be positive");
  if (!std::isfinite(pixel_ratio) || pixel_ratio <= 0.0) {
    throw std::invalid_argument("SDL GL canvas pixel ratio must be positive and finite");
  }
  const auto scaled_width = static_cast<double>(width) * pixel_ratio;
  const auto scaled_height = static_cast<double>(height) * pixel_ratio;
  if (scaled_width > static_cast<double>(std::numeric_limits<int>::max()) ||
      scaled_height > static_cast<double>(std::numeric_limits<int>::max())) {
    throw std::range_error("SDL GL canvas pixel dimensions exceed the SDL window range");
  }

  WindowOptions options;
  options.title = title.to_utf8();
  options.width = static_cast<int>(std::lround(scaled_width));
  options.height = static_cast<int>(std::lround(scaled_height));
  options.hidden = hidden;
  options.graphics_api = GraphicsApi::open_gl;
  options.open_gl.major_version = 3;
  options.open_gl.minor_version = 0;
  options.open_gl.profile = OpenGlProfile::es;
  return GlCanvas(std::make_shared<detail::GlSurfaceState>(std::move(options)));
}

WindowSize GlCanvas::logical_size() const { return require_state().window.size(); }

WindowSize GlCanvas::pixel_size() const { return require_state().window.pixel_size(); }

SDL_Window* GlCanvas::native_window() const noexcept {
  return state_ ? state_->window.native_handle() : nullptr;
}

WebGl2Context GlCanvas::get_context() const {
  static_cast<void>(require_state());
  return WebGl2Context(state_);
}

detail::GlSurfaceState& GlCanvas::require_state() const {
  if (!state_) throw std::logic_error("SDL GL canvas is empty");
  return *state_;
}

} // namespace flight::host_sdl
