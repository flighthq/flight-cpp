#pragma once

#include <memory>

#include <flight/host_sdl/export.hpp>
#include <flight/reference.hpp>

namespace flight::types {
struct ApplicationVisibilityBackend;
struct FullscreenBackend;
struct FullscreenTargetHandle;
}

namespace flight::host_sdl {

class Window;

// Binds one SDL window to the dependency-closed generated Flight visibility and fullscreen
// contracts. The generated records and target handles may outlive this adapter; they retain only
// shared registry state and resolve SDL's stable window id at each call.
class FLIGHT_HOST_SDL_SDK_WINDOW_API SdkWindowBackend final {
 public:
  explicit SdkWindowBackend(const Window& window);
  ~SdkWindowBackend() noexcept;

  SdkWindowBackend(const SdkWindowBackend&) noexcept;
  SdkWindowBackend& operator=(const SdkWindowBackend&) noexcept;
  SdkWindowBackend(SdkWindowBackend&&) noexcept;
  SdkWindowBackend& operator=(SdkWindowBackend&&) noexcept;

  [[nodiscard]] flight::types::ApplicationVisibilityBackend visibility_backend() const;
  [[nodiscard]] flight::types::FullscreenBackend fullscreen_backend() const;
  [[nodiscard]] flight::Ref<flight::types::FullscreenTargetHandle> fullscreen_target() const;

 private:
  struct State;
  std::shared_ptr<State> state_;
};

} // namespace flight::host_sdl
