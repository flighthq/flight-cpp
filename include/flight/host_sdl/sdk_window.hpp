#pragma once

#include <memory>

#include <flight/host_sdl/export.hpp>
#include <flight/reference.hpp>

namespace flight::types {
struct HostElementFullscreenCapability;
struct FullscreenTargetHandle;
struct HostInputDropFileCapability;
struct HostInputFocusCapability;
struct HostInputPointerLockCapability;
struct HostInputTargetCapability;
struct InputTargetHandle;
}

union SDL_Event;

namespace flight::host_sdl {

class Window;

// Binds one SDL window to the dependency-closed generated Flight fullscreen and input contracts.
//
// There is no visibility member any more. Upstream removed `HostApplicationVisibilityProvider`,
// whose `isVisible()` this adapter answered from SDL's window flags. Its successor
// `HostWindowVisibilityCapability` is a different contract -- `show(AppWindow)` and
// `hide(AppWindow)`, commands addressed by app window rather than a query about this one -- and
// binding it needs an AppWindow-to-SDL-window registry this adapter does not have. That is a host
// design decision, not a rename, so nothing is bound here rather than guessing one. The generated records and target handles may outlive this adapter; they retain only
// shared registry state and resolve SDL's stable window id at each call.
class FLIGHT_HOST_SDL_SDK_WINDOW_API SdkWindowBackend final {
 public:
  explicit SdkWindowBackend(const Window& window);
  ~SdkWindowBackend() noexcept;

  SdkWindowBackend(const SdkWindowBackend&) noexcept;
  SdkWindowBackend& operator=(const SdkWindowBackend&) noexcept;
  SdkWindowBackend(SdkWindowBackend&&) noexcept;
  SdkWindowBackend& operator=(SdkWindowBackend&&) noexcept;

  [[nodiscard]] flight::types::HostElementFullscreenCapability fullscreen_backend() const;
  [[nodiscard]] flight::Ref<flight::types::FullscreenTargetHandle> fullscreen_target() const;
  [[nodiscard]] flight::types::HostInputDropFileCapability input_drop_file_backend() const;
  [[nodiscard]] flight::types::HostInputFocusCapability input_focus_backend() const;
  [[nodiscard]] flight::types::HostInputPointerLockCapability input_pointer_lock_backend() const;
  [[nodiscard]] flight::types::HostInputTargetCapability input_target_backend() const;
  [[nodiscard]] flight::Ref<flight::types::InputTargetHandle> input_target() const;

  // Routes focus and file-drop events to subscriptions made through the generated records.
  // The caller retains ownership of the SDL event and should also pass it to other host dispatchers.
  [[nodiscard]] bool dispatch(const SDL_Event& event) const;

 private:
  struct State;
  std::shared_ptr<State> state_;
};

} // namespace flight::host_sdl
