#pragma once

#include <memory>

#include <flight/host_sdl/export.hpp>

namespace flight::types {
struct HostSoftKeyboardChangeProvider;
struct HostSoftKeyboardInfoProvider;
struct HostSoftKeyboardVisibilityProvider;
}

union SDL_Event;

namespace flight::host_sdl {

class Window;

// Binds one SDL window's on-screen keyboard to Flight's exact generated visibility, snapshot, and
// change-subscription records. The active SDL video driver decides whether a screen keyboard exists;
// unsupported drivers return Flight's specified operation/acquisition failure outcomes.
class FLIGHT_HOST_SDL_SDK_KEYBOARD_API SdkSoftKeyboardBackend final {
 public:
  explicit SdkSoftKeyboardBackend(const Window& window);
  ~SdkSoftKeyboardBackend() noexcept;

  SdkSoftKeyboardBackend(const SdkSoftKeyboardBackend&) noexcept;
  SdkSoftKeyboardBackend& operator=(const SdkSoftKeyboardBackend&) noexcept;
  SdkSoftKeyboardBackend(SdkSoftKeyboardBackend&&) noexcept;
  SdkSoftKeyboardBackend& operator=(SdkSoftKeyboardBackend&&) noexcept;

  [[nodiscard]] flight::types::HostSoftKeyboardChangeProvider change_backend() const;
  [[nodiscard]] flight::types::HostSoftKeyboardInfoProvider info_backend() const;
  [[nodiscard]] flight::types::HostSoftKeyboardVisibilityProvider visibility_backend() const;

  // Routes SDL's global screen-keyboard shown/hidden events to subscriptions for a still-live
  // bound window. Other events are ignored and remain owned by the caller.
  [[nodiscard]] bool dispatch(const SDL_Event& event) const;

 private:
  struct State;
  std::shared_ptr<State> state_;
};

} // namespace flight::host_sdl
