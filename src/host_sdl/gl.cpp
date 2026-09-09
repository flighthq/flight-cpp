#include <flight/host_sdl/gl.hpp>

#include "detail.hpp"

#include <stdexcept>
#include <string>
#include <utility>

namespace flight::host_sdl {

GlContext::GlContext(const Window& window) {
  if (window.graphics_api() != GraphicsApi::open_gl) {
    throw std::invalid_argument("OpenGL context requires an OpenGL window");
  }
  context_ = SDL_GL_CreateContext(window.native_handle());
  if (context_ == nullptr) detail::throw_sdl_error("SDL_GL_CreateContext");
}

GlContext::~GlContext() noexcept { reset(); }

GlContext::GlContext(GlContext&& other) noexcept : context_(std::exchange(other.context_, nullptr)) {}

GlContext& GlContext::operator=(GlContext&& other) noexcept {
  if (this == &other) return *this;
  reset();
  context_ = std::exchange(other.context_, nullptr);
  return *this;
}

SDL_GLContext GlContext::native_handle() const noexcept { return context_; }

SDL_FunctionPointer GlContext::function_address(std::string_view name) const {
  const std::string value(name);
  const SDL_FunctionPointer result = SDL_GL_GetProcAddress(value.c_str());
  if (result == nullptr) detail::throw_sdl_error("SDL_GL_GetProcAddress");
  return result;
}

int GlContext::swap_interval() const {
  int result = 0;
  detail::require_sdl(SDL_GL_GetSwapInterval(&result), "SDL_GL_GetSwapInterval");
  return result;
}

void GlContext::make_current(const Window& window) const {
  if (window.graphics_api() != GraphicsApi::open_gl) {
    throw std::invalid_argument("OpenGL context can only be made current on an OpenGL window");
  }
  detail::require_sdl(SDL_GL_MakeCurrent(window.native_handle(), context_), "SDL_GL_MakeCurrent");
}

void GlContext::set_swap_interval(int interval) const {
  detail::require_sdl(SDL_GL_SetSwapInterval(interval), "SDL_GL_SetSwapInterval");
}

void GlContext::swap(const Window& window) const {
  if (window.graphics_api() != GraphicsApi::open_gl) {
    throw std::invalid_argument("OpenGL buffers can only be swapped on an OpenGL window");
  }
  detail::require_sdl(SDL_GL_SwapWindow(window.native_handle()), "SDL_GL_SwapWindow");
}

void GlContext::reset() noexcept {
  if (context_ == nullptr) return;
  SDL_GL_DestroyContext(context_);
  context_ = nullptr;
}

} // namespace flight::host_sdl
