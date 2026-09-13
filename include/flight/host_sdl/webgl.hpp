#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string_view>

#include <SDL3/SDL_video.h>

#include <flight/host_sdl/export.hpp>
#include <flight/host_sdl/window.hpp>
#include <flight/string.hpp>
#include <flight/typed_array.hpp>

namespace flight::host_sdl {

class WebGl2Context;

namespace detail {

enum class WebGlObjectKind {
  buffer,
  framebuffer,
  program,
  renderbuffer,
  shader,
  texture,
  uniform_location,
  vertex_array,
};

struct GlSurfaceState;

struct FLIGHT_HOST_SDL_GL_API WebGlObjectState final {
  WebGlObjectState(
      std::weak_ptr<GlSurfaceState> owner,
      WebGlObjectKind kind,
      std::uint32_t name) noexcept;
  ~WebGlObjectState() noexcept;

  WebGlObjectState(const WebGlObjectState&) = delete;
  WebGlObjectState& operator=(const WebGlObjectState&) = delete;

  std::weak_ptr<GlSurfaceState> owner;
  WebGlObjectKind kind;
  std::uint32_t name;
};

struct GlImageSourceState;

} // namespace detail

template <typename Tag>
class WebGlHandle final {
 public:
  using weak_type = std::weak_ptr<detail::WebGlObjectState>;

  WebGlHandle() noexcept = default;

  [[nodiscard]] explicit operator bool() const noexcept {
    return state_ != nullptr && state_->name != 0;
  }
  [[nodiscard]] friend bool operator==(
      const WebGlHandle&,
      const WebGlHandle&) noexcept = default;
  [[nodiscard]] const void* identity() const noexcept { return state_.get(); }
  [[nodiscard]] std::uint32_t native_name() const noexcept { return state_ ? state_->name : 0; }
  [[nodiscard]] weak_type weaken() const noexcept { return state_; }

  [[nodiscard]] static std::optional<WebGlHandle> lock_weak(const weak_type& weak) noexcept {
    auto state = weak.lock();
    if (!state) return std::nullopt;
    return WebGlHandle(std::move(state));
  }

 private:
  friend class WebGl2Context;

  explicit WebGlHandle(std::shared_ptr<detail::WebGlObjectState> state) noexcept
      : state_(std::move(state)) {}

  std::shared_ptr<detail::WebGlObjectState> state_;
};

struct WebGlBufferTag;
struct WebGlFramebufferTag;
struct WebGlProgramTag;
struct WebGlRenderbufferTag;
struct WebGlShaderTag;
struct WebGlTextureTag;
struct WebGlUniformLocationTag;
struct WebGlVertexArrayObjectTag;

using WebGlBuffer = WebGlHandle<WebGlBufferTag>;
using WebGlFramebuffer = WebGlHandle<WebGlFramebufferTag>;
using WebGlProgram = WebGlHandle<WebGlProgramTag>;
using WebGlRenderbuffer = WebGlHandle<WebGlRenderbufferTag>;
using WebGlShader = WebGlHandle<WebGlShaderTag>;
using WebGlTexture = WebGlHandle<WebGlTextureTag>;
using WebGlUniformLocation = WebGlHandle<WebGlUniformLocationTag>;
using WebGlVertexArrayObject = WebGlHandle<WebGlVertexArrayObjectTag>;

struct WebGlContextAttributes final {
  bool alpha{true};
  bool antialias{true};
  bool depth{true};
  bool desynchronized{false};
  bool fail_if_major_performance_caveat{false};
  String power_preference{"default"};
  bool premultiplied_alpha{true};
  bool preserve_drawing_buffer{false};
  bool stencil{false};
};

class FLIGHT_HOST_SDL_GL_API GlImageSource final {
 public:
  using weak_type = std::weak_ptr<detail::GlImageSourceState>;

  GlImageSource() noexcept = default;

  [[nodiscard]] static GlImageSource rgba8(
      std::size_t width,
      std::size_t height,
      Uint8ClampedArray pixels);

  [[nodiscard]] explicit operator bool() const noexcept { return state_ != nullptr; }
  [[nodiscard]] friend bool operator==(
      const GlImageSource&,
      const GlImageSource&) noexcept = default;
  [[nodiscard]] const void* identity() const noexcept { return state_.get(); }
  [[nodiscard]] std::size_t width() const noexcept;
  [[nodiscard]] std::size_t height() const noexcept;
  [[nodiscard]] std::span<const Uint8Clamped> rgba8_pixels() const noexcept;
  [[nodiscard]] weak_type weaken() const noexcept { return state_; }

  [[nodiscard]] static std::optional<GlImageSource> lock_weak(const weak_type& weak) noexcept;

 private:
  explicit GlImageSource(std::shared_ptr<detail::GlImageSourceState> state) noexcept
      : state_(std::move(state)) {}

  std::shared_ptr<detail::GlImageSourceState> state_;
};

struct GlImageSourceWeakPolicy final {
  using key_type = GlImageSource;
  using weak_type = GlImageSource::weak_type;
  using identity_type = const void*;

  [[nodiscard]] static weak_type weaken(const key_type& key) noexcept { return key.weaken(); }
  [[nodiscard]] static std::optional<key_type> lock(const weak_type& key) noexcept {
    return key_type::lock_weak(key);
  }
  [[nodiscard]] static identity_type identity(const key_type& key) noexcept { return key.identity(); }
  [[nodiscard]] static std::size_t hash(identity_type identity) noexcept;
  [[nodiscard]] static bool equal(identity_type left, identity_type right) noexcept {
    return left == right;
  }
};

// A copyable WebGL-shaped view over one SDL window and its OpenGL context. The context and window
// share one owner so a canvas, context, or GPU handle can never observe a half-destroyed surface.
// Rendering remains in Flight's generated render-gl packages; this class supplies only the host
// context, object identities, and presentation boundary.
class FLIGHT_HOST_SDL_GL_API WebGl2Context final {
 public:
  WebGl2Context() noexcept = default;

  [[nodiscard]] explicit operator bool() const noexcept { return state_ != nullptr; }
  [[nodiscard]] friend bool operator==(
      const WebGl2Context&,
      const WebGl2Context&) noexcept = default;
  [[nodiscard]] const void* identity() const noexcept { return state_.get(); }
  [[nodiscard]] int drawing_buffer_width() const;
  [[nodiscard]] int drawing_buffer_height() const;
  [[nodiscard]] SDL_FunctionPointer function_address(std::string_view name) const;

  void make_current() const;
  void present() const;
  void set_swap_interval(int interval) const;

 private:
  friend class GlCanvas;

  explicit WebGl2Context(std::shared_ptr<detail::GlSurfaceState> state) noexcept
      : state_(std::move(state)) {}

  [[nodiscard]] detail::GlSurfaceState& require_state() const;

  std::shared_ptr<detail::GlSurfaceState> state_;
};

// Native replacement for the HTMLCanvasElement returned by GlRenderSurfaceProvider. Logical size
// and drawable pixel size stay separate, matching Flight's width/height/pixelRatio contract.
class FLIGHT_HOST_SDL_GL_API GlCanvas final {
 public:
  GlCanvas() noexcept = default;

  [[nodiscard]] static GlCanvas create(
      int width,
      int height,
      double pixel_ratio = 1.0,
      String title = String("Flight"),
      bool hidden = false);

  [[nodiscard]] explicit operator bool() const noexcept { return state_ != nullptr; }
  [[nodiscard]] friend bool operator==(const GlCanvas&, const GlCanvas&) noexcept = default;
  [[nodiscard]] const void* identity() const noexcept { return state_.get(); }
  [[nodiscard]] WindowSize logical_size() const;
  [[nodiscard]] WindowSize pixel_size() const;
  [[nodiscard]] SDL_Window* native_window() const noexcept;
  [[nodiscard]] WebGl2Context get_context() const;

 private:
  explicit GlCanvas(std::shared_ptr<detail::GlSurfaceState> state) noexcept
      : state_(std::move(state)) {}

  [[nodiscard]] detail::GlSurfaceState& require_state() const;

  std::shared_ptr<detail::GlSurfaceState> state_;
};

} // namespace flight::host_sdl

namespace std {

template <typename Tag>
struct hash<flight::host_sdl::WebGlHandle<Tag>> {
  [[nodiscard]] size_t operator()(const flight::host_sdl::WebGlHandle<Tag>& value) const noexcept {
    return hash<const void*>{}(value.identity());
  }
};

template <>
struct hash<flight::host_sdl::GlImageSource> {
  [[nodiscard]] size_t operator()(const flight::host_sdl::GlImageSource& value) const noexcept {
    return hash<const void*>{}(value.identity());
  }
};

} // namespace std
