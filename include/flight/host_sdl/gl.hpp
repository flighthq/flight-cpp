#pragma once

#include <string_view>

#include <SDL3/SDL_video.h>

#include <flight/host_sdl/export.hpp>
#include <flight/host_sdl/window.hpp>

namespace flight::host_sdl {

class FLIGHT_HOST_SDL_GL_API GlContext final {
 public:
  explicit GlContext(const Window& window);
  ~GlContext() noexcept;

  GlContext(const GlContext&) = delete;
  GlContext& operator=(const GlContext&) = delete;
  GlContext(GlContext&& other) noexcept;
  GlContext& operator=(GlContext&& other) noexcept;

  [[nodiscard]] SDL_GLContext native_handle() const noexcept;
  [[nodiscard]] SDL_FunctionPointer function_address(std::string_view name) const;
  [[nodiscard]] int swap_interval() const;

  void make_current(const Window& window) const;
  void set_swap_interval(int interval) const;
  void swap(const Window& window) const;

 private:
  void reset() noexcept;

  SDL_GLContext context_{nullptr};
};

} // namespace flight::host_sdl
