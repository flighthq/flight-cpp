#pragma once

#include <memory>

#include <flight/host_sdl/export.hpp>

namespace flight::types {
struct HapticsBackend;
}

namespace flight::host_sdl {

// Produces Flight's exact generated haptics record over the first connected SDL gamepad with rumble
// support. The adapter owns its opened gamepad reference and discovers a replacement as devices
// change. SDL supplies amplitude-controlled continuous rumble but no timed multi-step waveform.
class FLIGHT_HOST_SDL_SDK_HAPTICS_API SdkHapticsBackend final {
 public:
  SdkHapticsBackend();
  ~SdkHapticsBackend() noexcept;

  SdkHapticsBackend(const SdkHapticsBackend&) noexcept;
  SdkHapticsBackend& operator=(const SdkHapticsBackend&) noexcept;
  SdkHapticsBackend(SdkHapticsBackend&&) noexcept;
  SdkHapticsBackend& operator=(SdkHapticsBackend&&) noexcept;

  [[nodiscard]] flight::types::HapticsBackend backend() const;

 private:
  struct State;
  std::shared_ptr<State> state_;
};

} // namespace flight::host_sdl
