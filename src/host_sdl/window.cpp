#include <flight/host_sdl/window.hpp>

#include "detail.hpp"

#include <stdexcept>
#include <string>
#include <utility>

namespace flight::host_sdl {
namespace {

void set_gl_attribute(SDL_GLAttr attribute, int value) {
  detail::require_sdl(SDL_GL_SetAttribute(attribute, value), "SDL_GL_SetAttribute");
}

int profile_mask(OpenGlProfile profile) {
  switch (profile) {
    case OpenGlProfile::core:
      return SDL_GL_CONTEXT_PROFILE_CORE;
    case OpenGlProfile::compatibility:
      return SDL_GL_CONTEXT_PROFILE_COMPATIBILITY;
    case OpenGlProfile::es:
      return SDL_GL_CONTEXT_PROFILE_ES;
  }
  throw std::invalid_argument("unknown OpenGL profile");
}

void configure_open_gl(const OpenGlOptions& options) {
  if (options.major_version < 1 || options.minor_version < 0) {
    throw std::invalid_argument("invalid OpenGL version");
  }
  int context_flags = 0;
  if (options.debug) context_flags |= SDL_GL_CONTEXT_DEBUG_FLAG;
  if (options.forward_compatible) context_flags |= SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG;

  set_gl_attribute(SDL_GL_CONTEXT_MAJOR_VERSION, options.major_version);
  set_gl_attribute(SDL_GL_CONTEXT_MINOR_VERSION, options.minor_version);
  set_gl_attribute(SDL_GL_CONTEXT_PROFILE_MASK, profile_mask(options.profile));
  set_gl_attribute(SDL_GL_CONTEXT_FLAGS, context_flags);
  set_gl_attribute(SDL_GL_RED_SIZE, options.red_bits);
  set_gl_attribute(SDL_GL_GREEN_SIZE, options.green_bits);
  set_gl_attribute(SDL_GL_BLUE_SIZE, options.blue_bits);
  set_gl_attribute(SDL_GL_ALPHA_SIZE, options.alpha_bits);
  set_gl_attribute(SDL_GL_DEPTH_SIZE, options.depth_bits);
  set_gl_attribute(SDL_GL_STENCIL_SIZE, options.stencil_bits);
  set_gl_attribute(SDL_GL_DOUBLEBUFFER, options.double_buffer ? 1 : 0);
  set_gl_attribute(SDL_GL_MULTISAMPLEBUFFERS, options.multisample_buffers);
  set_gl_attribute(SDL_GL_MULTISAMPLESAMPLES, options.multisample_samples);
}

SDL_WindowFlags window_flags(const WindowOptions& options) {
  constexpr SDL_WindowFlags graphics_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_VULKAN | SDL_WINDOW_METAL;
  if ((options.extra_flags & graphics_flags) != 0) {
    throw std::invalid_argument("graphics flags must be selected with WindowOptions::graphics_api");
  }

  SDL_WindowFlags flags = options.extra_flags;
  if (options.resizable) flags |= SDL_WINDOW_RESIZABLE;
  if (options.high_pixel_density) flags |= SDL_WINDOW_HIGH_PIXEL_DENSITY;
  if (options.hidden) flags |= SDL_WINDOW_HIDDEN;

  switch (options.graphics_api) {
    case GraphicsApi::none:
      break;
    case GraphicsApi::open_gl:
      flags |= SDL_WINDOW_OPENGL;
      break;
    case GraphicsApi::vulkan:
      flags |= SDL_WINDOW_VULKAN;
      break;
  }
  return flags;
}

} // namespace

Window::Window(const WindowOptions& options) : graphics_api_(options.graphics_api) {
  if (options.width <= 0 || options.height <= 0) {
    throw std::invalid_argument("SDL window dimensions must be positive");
  }
  if (graphics_api_ == GraphicsApi::open_gl) configure_open_gl(options.open_gl);
  window_ = SDL_CreateWindow(options.title.c_str(), options.width, options.height, window_flags(options));
  if (window_ == nullptr) detail::throw_sdl_error("SDL_CreateWindow");
}

Window::~Window() noexcept { reset(); }

Window::Window(Window&& other) noexcept
    : window_(std::exchange(other.window_, nullptr)),
      graphics_api_(std::exchange(other.graphics_api_, GraphicsApi::none)) {}

Window& Window::operator=(Window&& other) noexcept {
  if (this == &other) return *this;
  reset();
  window_ = std::exchange(other.window_, nullptr);
  graphics_api_ = std::exchange(other.graphics_api_, GraphicsApi::none);
  return *this;
}

SDL_Window* Window::native_handle() const noexcept { return window_; }

SDL_WindowID Window::id() const {
  const SDL_WindowID value = SDL_GetWindowID(window_);
  if (value == 0) detail::throw_sdl_error("SDL_GetWindowID");
  return value;
}

GraphicsApi Window::graphics_api() const noexcept { return graphics_api_; }

WindowSize Window::size() const {
  WindowSize result;
  detail::require_sdl(SDL_GetWindowSize(window_, &result.width, &result.height), "SDL_GetWindowSize");
  return result;
}

WindowSize Window::pixel_size() const {
  WindowSize result;
  detail::require_sdl(
      SDL_GetWindowSizeInPixels(window_, &result.width, &result.height),
      "SDL_GetWindowSizeInPixels");
  return result;
}

void Window::set_title(std::string_view title) {
  const std::string value(title);
  detail::require_sdl(SDL_SetWindowTitle(window_, value.c_str()), "SDL_SetWindowTitle");
}

void Window::show() { detail::require_sdl(SDL_ShowWindow(window_), "SDL_ShowWindow"); }

void Window::hide() { detail::require_sdl(SDL_HideWindow(window_), "SDL_HideWindow"); }

void Window::reset() noexcept {
  if (window_ == nullptr) return;
  SDL_DestroyWindow(window_);
  window_ = nullptr;
  graphics_api_ = GraphicsApi::none;
}

} // namespace flight::host_sdl
