#pragma once

#include <memory>

#include <flight/host_sdl/export.hpp>

namespace flight::types {
struct HostScreenChangeProvider;
struct HostScreenDetailsProvider;
struct ScreenInfo;
struct HostScreenQueryProvider;
}

union SDL_Event;

namespace flight::host_sdl {

// Produces the exact generated Flight display-enumeration and native permission records. SDL
// supplies stable display ids, virtual-desktop geometry, modes, orientation, and pointer location;
// fields not reported by SDL retain the sentinels specified by ScreenInfo.
class FLIGHT_HOST_SDL_SDK_SCREEN_API SdkScreenBackend final {
 public:
  SdkScreenBackend();
  ~SdkScreenBackend() noexcept;

  SdkScreenBackend(const SdkScreenBackend&) noexcept;
  SdkScreenBackend& operator=(const SdkScreenBackend&) noexcept;
  SdkScreenBackend(SdkScreenBackend&&) noexcept;
  SdkScreenBackend& operator=(SdkScreenBackend&&) noexcept;

  [[nodiscard]] flight::types::HostScreenQueryProvider query_backend() const;
  [[nodiscard]] flight::types::HostScreenDetailsProvider details_backend() const;
  [[nodiscard]] flight::types::HostScreenChangeProvider change_backend() const;

  // Routes SDL display add/remove/metrics events to generated subscriptions and refreshes the
  // adapter's last-known snapshot. Other events remain owned by the caller.
  [[nodiscard]] bool dispatch(const SDL_Event& event) const;

 private:
  struct State;
  std::shared_ptr<State> state_;
};

} // namespace flight::host_sdl
