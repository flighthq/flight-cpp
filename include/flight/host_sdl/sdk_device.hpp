#pragma once

#include <cstdint>

#include <flight/host_sdl/export.hpp>

namespace flight::types {
struct HostDeviceProvider;
}

namespace flight::host_sdl {

class Window;

// Produces the exact generated Flight device snapshot backend for the display containing one SDL
// window. Retained generated records resolve SDL's stable window id on every read and fall back to
// Flight's specified sentinels after the native window is destroyed.
class FLIGHT_HOST_SDL_SDK_DEVICE_API SdkDeviceBackend final {
 public:
  explicit SdkDeviceBackend(const Window& window);

  [[nodiscard]] flight::types::HostDeviceProvider backend() const;

 private:
  std::uint32_t window_id_{};
};

} // namespace flight::host_sdl
