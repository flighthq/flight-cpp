#pragma once

#include <cstddef>
#include <memory>

#include <flight/host_sdl/export.hpp>

namespace flight::types {
struct HostAudioDeviceProvider;
}

namespace flight::host_sdl {

// Owns the SDL implementation and produces the exact generated Flight AudioDeviceBackend record.
// Callers that invoke backend() must also make the generated AudioDeviceBackend definition visible.
// Copies share one native handle table so records may outlive the adapter that created them.
class FLIGHT_HOST_SDL_SDK_AUDIO_API SdkAudioDeviceBackend final {
 public:
  SdkAudioDeviceBackend();
  ~SdkAudioDeviceBackend() noexcept;

  SdkAudioDeviceBackend(const SdkAudioDeviceBackend&) noexcept;
  SdkAudioDeviceBackend& operator=(const SdkAudioDeviceBackend&) noexcept;
  SdkAudioDeviceBackend(SdkAudioDeviceBackend&&) noexcept;
  SdkAudioDeviceBackend& operator=(SdkAudioDeviceBackend&&) noexcept;

  [[nodiscard]] flight::types::HostAudioDeviceProvider backend() const;
  [[nodiscard]] std::size_t pump() const;

 private:
  struct State;
  std::shared_ptr<State> state_;
};

} // namespace flight::host_sdl
