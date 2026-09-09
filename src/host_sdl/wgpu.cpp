#include <flight/host_sdl/wgpu.hpp>

#include <stdexcept>
#include <utility>

namespace flight::host_sdl {

WgpuSurface::WgpuSurface(
    const Window& window,
    WgpuInstanceHandle instance,
    WgpuSurfaceCallbacks callbacks)
    : instance_(instance), callbacks_(callbacks) {
  if (instance_ == nullptr) throw std::invalid_argument("WebGPU instance cannot be null");
  if (callbacks_.create == nullptr || callbacks_.destroy == nullptr) {
    throw std::invalid_argument("WebGPU surface callbacks must provide create and destroy");
  }
  if (window.graphics_api() == GraphicsApi::open_gl) {
    throw std::invalid_argument("WebGPU surface cannot use an OpenGL window");
  }
  surface_ = callbacks_.create(callbacks_.userdata, instance_, window.native_handle());
  if (surface_ == nullptr) throw std::runtime_error("native WebGPU implementation did not create a surface");
}

WgpuSurface::~WgpuSurface() noexcept { reset(); }

WgpuSurface::WgpuSurface(WgpuSurface&& other) noexcept
    : instance_(std::exchange(other.instance_, nullptr)),
      surface_(std::exchange(other.surface_, nullptr)),
      callbacks_(std::exchange(other.callbacks_, {})) {}

WgpuSurface& WgpuSurface::operator=(WgpuSurface&& other) noexcept {
  if (this == &other) return *this;
  reset();
  instance_ = std::exchange(other.instance_, nullptr);
  surface_ = std::exchange(other.surface_, nullptr);
  callbacks_ = std::exchange(other.callbacks_, {});
  return *this;
}

WgpuInstanceHandle WgpuSurface::instance() const noexcept { return instance_; }

WgpuSurfaceHandle WgpuSurface::native_handle() const noexcept { return surface_; }

WgpuSurfaceHandle WgpuSurface::release() noexcept {
  instance_ = nullptr;
  callbacks_ = {};
  return std::exchange(surface_, nullptr);
}

void WgpuSurface::reset() noexcept {
  if (surface_ != nullptr) callbacks_.destroy(callbacks_.userdata, instance_, surface_);
  instance_ = nullptr;
  surface_ = nullptr;
  callbacks_ = {};
}

} // namespace flight::host_sdl
