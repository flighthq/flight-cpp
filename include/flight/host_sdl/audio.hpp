#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>

#include <flight/host_sdl/export.hpp>

namespace flight::host_sdl {

struct AudioDeviceHandle final {
  std::uint64_t value{};

  [[nodiscard]] explicit operator bool() const noexcept { return value != 0; }
  [[nodiscard]] auto operator<=>(const AudioDeviceHandle&) const noexcept = default;
};

struct AudioBufferHandle final {
  std::uint64_t value{};

  [[nodiscard]] explicit operator bool() const noexcept { return value != 0; }
  [[nodiscard]] auto operator<=>(const AudioBufferHandle&) const noexcept = default;
};

struct AudioSourceHandle final {
  std::uint64_t value{};

  [[nodiscard]] explicit operator bool() const noexcept { return value != 0; }
  [[nodiscard]] auto operator<=>(const AudioSourceHandle&) const noexcept = default;
};

// SDL implementation of Flight's decoded-PCM audio-device contract. The backend owns one SDL
// playback stream per device and mixes that device's live sources in its audio callback. Public
// operations are safe to call from the application thread while the callback is running.
class FLIGHT_HOST_SDL_API SdlAudioDeviceBackend final {
 public:
  SdlAudioDeviceBackend();
  ~SdlAudioDeviceBackend() noexcept;

  SdlAudioDeviceBackend(const SdlAudioDeviceBackend&) = delete;
  SdlAudioDeviceBackend& operator=(const SdlAudioDeviceBackend&) = delete;
  SdlAudioDeviceBackend(SdlAudioDeviceBackend&&) noexcept;
  SdlAudioDeviceBackend& operator=(SdlAudioDeviceBackend&&) noexcept;

  [[nodiscard]] AudioDeviceHandle create_device(std::uint32_t sample_rate);
  void destroy_device(AudioDeviceHandle device) noexcept;
  [[nodiscard]] double get_device_time(AudioDeviceHandle device) const noexcept;
  void resume_device(AudioDeviceHandle device) noexcept;

  // Flight supplies one Float32 sample span per channel. Missing samples are zero-filled and extra
  // samples are ignored. SDL playback currently accepts the mono and stereo layouts used by Flight.
  [[nodiscard]] AudioBufferHandle create_buffer(
      AudioDeviceHandle device,
      std::uint32_t channels,
      std::size_t length,
      std::uint32_t sample_rate,
      std::span<const std::span<const float>> data);
  void destroy_buffer(AudioBufferHandle buffer) noexcept;

  [[nodiscard]] AudioSourceHandle create_source(
      AudioDeviceHandle device,
      AudioBufferHandle buffer);
  void destroy_source(AudioSourceHandle source) noexcept;
  void on_source_ended(AudioSourceHandle source, std::function<void()> callback);
  void set_source_gain(AudioSourceHandle source, double gain) noexcept;
  void set_source_pan(AudioSourceHandle source, double pan) noexcept;
  void set_source_playback_rate(AudioSourceHandle source, double rate) noexcept;
  void start_source(AudioSourceHandle source, double offset_seconds, double duration_seconds);
  void stop_source(AudioSourceHandle source) noexcept;

  // Completion callbacks execute on the pumping thread, never SDL's audio thread. Each completion
  // that was pending when the turn began is delivered at most once.
  [[nodiscard]] std::size_t pump();

 private:
  struct State;
  std::unique_ptr<State> state_;
};

} // namespace flight::host_sdl
