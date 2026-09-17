#pragma once

#include <flight/host_sdl/export.hpp>

namespace flight::types {
struct HostPlatformProvider;
}

namespace flight::host_sdl {

// Produces the exact generated Flight platform snapshot backend. SDL supplies dynamic host facts;
// fields SDL cannot report retain the empty/unknown sentinels required by PlatformInfo.
class FLIGHT_HOST_SDL_SDK_PLATFORM_API SdkPlatformBackend final {
 public:
  [[nodiscard]] flight::types::HostPlatformProvider backend() const;
};

} // namespace flight::host_sdl
