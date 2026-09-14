#include <flight/host_sdl/sdk_audio.hpp>

#include <flight/host_sdl/audio.hpp>
#include <flight/types/audio_device_backend.hpp>

#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace flight::host_sdl {

namespace {

constexpr double maximum_safe_integer = 9'007'199'254'740'991.0;

template <typename NativeHandle>
[[nodiscard]] NativeHandle native_handle(double value) noexcept {
  if (!std::isfinite(value) || value <= 0.0 || value > maximum_safe_integer ||
      std::trunc(value) != value) {
    return {};
  }
  return NativeHandle{static_cast<std::uint64_t>(value)};
}

template <typename NativeHandle>
[[nodiscard]] double sdk_handle(NativeHandle handle) noexcept {
  if (!handle || handle.value > static_cast<std::uint64_t>(maximum_safe_integer)) return 0.0;
  return static_cast<double>(handle.value);
}

[[nodiscard]] std::optional<std::uint32_t> positive_u32(double value) noexcept {
  if (!std::isfinite(value) || value <= 0.0 || std::trunc(value) != value ||
      value > static_cast<double>(std::numeric_limits<std::uint32_t>::max())) {
    return std::nullopt;
  }
  return static_cast<std::uint32_t>(value);
}

[[nodiscard]] std::optional<std::size_t> positive_size(double value) noexcept {
  constexpr double maximum = std::numeric_limits<std::size_t>::digits > 53
                                 ? maximum_safe_integer
                                 : static_cast<double>(std::numeric_limits<std::size_t>::max());
  if (!std::isfinite(value) || value <= 0.0 || std::trunc(value) != value ||
      value > maximum) {
    return std::nullopt;
  }
  return static_cast<std::size_t>(value);
}

} // namespace

struct SdkAudioDeviceBackend::State final {
  SdlAudioDeviceBackend native;
};

SdkAudioDeviceBackend::SdkAudioDeviceBackend() : state_(std::make_shared<State>()) {}

SdkAudioDeviceBackend::~SdkAudioDeviceBackend() noexcept = default;

SdkAudioDeviceBackend::SdkAudioDeviceBackend(const SdkAudioDeviceBackend&) noexcept = default;

SdkAudioDeviceBackend& SdkAudioDeviceBackend::operator=(
    const SdkAudioDeviceBackend&) noexcept = default;

SdkAudioDeviceBackend::SdkAudioDeviceBackend(SdkAudioDeviceBackend&&) noexcept = default;

SdkAudioDeviceBackend& SdkAudioDeviceBackend::operator=(SdkAudioDeviceBackend&&) noexcept = default;

flight::types::AudioDeviceBackend SdkAudioDeviceBackend::backend() const {
  flight::types::AudioDeviceBackend result;
  const auto state = state_;
  result.entity_runtime_key = std::nullopt;
  result.create_buffer = [state](
                             double device,
                             double channels,
                             double length,
                             double sample_rate,
                             flight::Array<flight::Float32Array> data) {
    const auto native_channels = positive_u32(channels);
    const auto native_length = positive_size(length);
    const auto native_rate = positive_u32(sample_rate);
    if (!native_channels || !native_length || !native_rate) return 0.0;
    std::vector<std::span<const float>> spans;
    spans.reserve(data.size());
    for (const auto& channel : data) spans.emplace_back(channel.begin(), channel.size());
    return sdk_handle(state->native.create_buffer(
        native_handle<AudioDeviceHandle>(device),
        *native_channels,
        *native_length,
        *native_rate,
        spans));
  };
  result.create_device = [state](double sample_rate) {
    const auto native_rate = positive_u32(sample_rate);
    return native_rate ? sdk_handle(state->native.create_device(*native_rate)) : 0.0;
  };
  result.create_source = [state](double device, double buffer) {
    return sdk_handle(state->native.create_source(
        native_handle<AudioDeviceHandle>(device),
        native_handle<AudioBufferHandle>(buffer)));
  };
  result.destroy_buffer = [state](double buffer) {
    state->native.destroy_buffer(native_handle<AudioBufferHandle>(buffer));
  };
  result.destroy_device = [state](double device) {
    state->native.destroy_device(native_handle<AudioDeviceHandle>(device));
  };
  result.destroy_source = [state](double source) {
    state->native.destroy_source(native_handle<AudioSourceHandle>(source));
  };
  result.get_device_time = [state](double device) {
    return state->native.get_device_time(native_handle<AudioDeviceHandle>(device));
  };
  result.on_source_ended = [state](
                               double source,
                               std::optional<std::function<void()>> callback) {
    state->native.on_source_ended(
        native_handle<AudioSourceHandle>(source),
        callback ? std::move(*callback) : std::function<void()>{});
  };
  result.resume_device = [state](double device) {
    state->native.resume_device(native_handle<AudioDeviceHandle>(device));
  };
  result.set_source_gain = [state](double source, double gain) {
    state->native.set_source_gain(native_handle<AudioSourceHandle>(source), gain);
  };
  result.set_source_pan = [state](double source, double pan) {
    state->native.set_source_pan(native_handle<AudioSourceHandle>(source), pan);
  };
  result.set_source_playback_rate = [state](double source, double rate) {
    state->native.set_source_playback_rate(native_handle<AudioSourceHandle>(source), rate);
  };
  result.start_source = [state](double source, double offset, double duration) {
    state->native.start_source(native_handle<AudioSourceHandle>(source), offset, duration);
  };
  result.stop_source = [state](double source) {
    state->native.stop_source(native_handle<AudioSourceHandle>(source));
  };
  return result;
}

std::size_t SdkAudioDeviceBackend::pump() const {
  return state_ == nullptr ? 0 : state_->native.pump();
}

} // namespace flight::host_sdl
