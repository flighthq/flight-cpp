#pragma once

#include <SDL3/SDL_video.h>

#include <flight/host_sdl/export.hpp>
#include <flight/host_sdl/window.hpp>

namespace flight::host_sdl {

using WgpuInstanceHandle = void*;
using WgpuSurfaceHandle = void*;

struct WgpuSurfaceCallbacks {
  void* userdata{nullptr};
  WgpuSurfaceHandle (*create)(void* userdata, WgpuInstanceHandle instance, SDL_Window* window){nullptr};
  void (*destroy)(void* userdata, WgpuInstanceHandle instance, WgpuSurfaceHandle surface) noexcept{nullptr};
};

class FLIGHT_HOST_SDL_WGPU_API WgpuSurface final {
 public:
  WgpuSurface(const Window& window, WgpuInstanceHandle instance, WgpuSurfaceCallbacks callbacks);
  ~WgpuSurface() noexcept;

  WgpuSurface(const WgpuSurface&) = delete;
  WgpuSurface& operator=(const WgpuSurface&) = delete;
  WgpuSurface(WgpuSurface&& other) noexcept;
  WgpuSurface& operator=(WgpuSurface&& other) noexcept;

  [[nodiscard]] WgpuInstanceHandle instance() const noexcept;
  [[nodiscard]] WgpuSurfaceHandle native_handle() const noexcept;
  [[nodiscard]] WgpuSurfaceHandle release() noexcept;

 private:
  void reset() noexcept;

  WgpuInstanceHandle instance_{nullptr};
  WgpuSurfaceHandle surface_{nullptr};
  WgpuSurfaceCallbacks callbacks_{};
};

} // namespace flight::host_sdl
