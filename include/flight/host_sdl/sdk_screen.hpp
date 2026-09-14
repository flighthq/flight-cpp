#pragma once

#include <flight/host_sdl/export.hpp>

namespace flight::types {
struct ScreenDetailsBackend;
struct ScreenInfo;
struct ScreenQueryBackend;
}

namespace flight::host_sdl {

// Produces the exact generated Flight display-enumeration and native permission records. SDL
// supplies stable display ids, virtual-desktop geometry, modes, orientation, and pointer location;
// fields not reported by SDL retain the sentinels specified by ScreenInfo.
class FLIGHT_HOST_SDL_SDK_SCREEN_API SdkScreenBackend final {
 public:
  [[nodiscard]] flight::types::ScreenQueryBackend query_backend() const;
  [[nodiscard]] flight::types::ScreenDetailsBackend details_backend() const;
};

} // namespace flight::host_sdl
