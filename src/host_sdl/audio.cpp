#include <flight/host_sdl/audio.hpp>

#include "detail.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <limits>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_init.h>

namespace flight::host_sdl {

namespace {

using Clock = std::chrono::steady_clock;

constexpr std::uint32_t maximum_channels = 2;
constexpr std::uint64_t maximum_handle = 9'007'199'254'740'991;
constexpr double half_pi = 1.57079632679489661923;

template <typename Handle>
[[nodiscard]] Handle next_handle(std::uint64_t& next) noexcept {
  if (next == 0 || next > maximum_handle) return {};
  return Handle{next++};
}

[[nodiscard]] float sample_at(
    const std::vector<float>& samples,
    std::size_t length,
    std::uint32_t channel,
    double cursor) noexcept {
  if (length == 0) return 0.0F;
  const auto first = std::min(static_cast<std::size_t>(cursor), length - 1);
  const auto second = std::min(first + 1, length - 1);
  const auto fraction = static_cast<float>(cursor - static_cast<double>(first));
  const float left = samples[static_cast<std::size_t>(channel) * length + first];
  const float right = samples[static_cast<std::size_t>(channel) * length + second];
  return left + (right - left) * fraction;
}

[[nodiscard]] std::array<float, 2> pan_sample(
    float left,
    float right,
    std::uint32_t channels,
    double pan) {
  const double normalized = std::clamp(pan, -1.0, 1.0);
  if (channels == 1) {
    const double position = (normalized + 1.0) * half_pi * 0.5;
    return {
        static_cast<float>(static_cast<double>(left) * std::cos(position)),
        static_cast<float>(static_cast<double>(left) * std::sin(position)),
    };
  }

  // Web Audio's equal-power stereo panner retains the channel on the side being panned toward and
  // folds the opposite channel into it. This keeps native playback behavior aligned with Flight's
  // Web backend for the mono/stereo layouts accepted above.
  if (normalized <= 0.0) {
    const double position = (normalized + 1.0) * half_pi;
    return {
        static_cast<float>(static_cast<double>(left) + static_cast<double>(right) * std::cos(position)),
        static_cast<float>(static_cast<double>(right) * std::sin(position)),
    };
  }
  const double position = normalized * half_pi;
  return {
      static_cast<float>(static_cast<double>(left) * std::cos(position)),
      static_cast<float>(static_cast<double>(right) + static_cast<double>(left) * std::sin(position)),
  };
}

} // namespace

struct SdlAudioDeviceBackend::State final {
  struct Buffer final {
    std::uint32_t channels{};
    std::size_t length{};
    std::uint32_t sample_rate{};
    std::vector<float> samples;
  };

  struct Device final {
    State* state{};
    AudioDeviceHandle handle;
    SDL_AudioStream* stream{};
    std::uint32_t sample_rate{};
    bool running{};
    double elapsed_seconds{};
    Clock::time_point resumed_at{};
    std::vector<float> mix;
  };

  struct Source final {
    AudioSourceHandle handle;
    AudioDeviceHandle device;
    std::shared_ptr<const Buffer> buffer;
    std::function<void()> callback;
    double cursor{};
    double end{};
    double gain{1.0};
    double pan{};
    double playback_rate{1.0};
    std::uint64_t generation{};
    bool playing{};
  };

  struct Completion final {
    AudioSourceHandle source;
    std::uint64_t generation{};
    Clock::time_point ready_at{};
  };

  State() {
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
      detail::throw_sdl_error("SDL_InitSubSystem(SDL_INIT_AUDIO)");
    }
  }

  ~State() noexcept {
    {
      const std::scoped_lock lock(mutex);
      sources.clear();
      buffers.clear();
      completions.clear();
    }
    while (true) {
      std::unique_ptr<Device> device;
      {
        const std::scoped_lock lock(mutex);
        if (devices.empty()) break;
        const auto found = devices.begin();
        device = std::move(found->second);
        devices.erase(found);
      }
      SDL_DestroyAudioStream(device->stream);
    }
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    if (SDL_WasInit(0) == 0) SDL_Quit();
  }

  [[nodiscard]] double device_time(const Device& device, Clock::time_point now) const noexcept {
    if (!device.running) return device.elapsed_seconds;
    return device.elapsed_seconds + std::chrono::duration<double>(now - device.resumed_at).count();
  }

  static void SDLCALL supply_audio(
      void* userdata,
      SDL_AudioStream* stream,
      int additional_amount,
      int /*total_amount*/) noexcept {
    auto& device = *static_cast<Device*>(userdata);
    if (additional_amount <= 0) return;
    try {
      device.state->render(device, stream, additional_amount);
    } catch (...) {
      // SDL supplies silence when the callback cannot provide data. Exceptions must never cross its
      // C callback boundary or escape from the audio thread.
    }
  }

  void render(Device& device, SDL_AudioStream* stream, int additional_amount) {
    constexpr std::size_t channel_count = 2;
    constexpr std::size_t bytes_per_frame = channel_count * sizeof(float);
    const auto bytes = static_cast<std::size_t>(additional_amount);
    std::size_t frames = (bytes + bytes_per_frame - 1) / bytes_per_frame;
    if (frames > static_cast<std::size_t>(std::numeric_limits<int>::max()) / bytes_per_frame) return;

    const std::scoped_lock lock(mutex);
    for (const auto& [handle, source] : sources) {
      static_cast<void>(handle);
      if (!source.playing || source.device != device.handle) continue;
      const double step = source.playback_rate *
                          static_cast<double>(source.buffer->sample_rate) /
                          static_cast<double>(device.sample_rate);
      const double remaining = std::ceil((source.end - source.cursor) / step);
      if (std::isfinite(remaining) && remaining > 0.0 &&
          remaining < static_cast<double>(frames)) {
        frames = static_cast<std::size_t>(remaining);
      } else if (remaining <= 0.0) {
        frames = 1;
      }
    }
    device.mix.assign(frames * channel_count, 0.0F);
    for (auto& [handle, source] : sources) {
      static_cast<void>(handle);
      if (!source.playing || source.device != device.handle) continue;
      const double step = source.playback_rate *
                          static_cast<double>(source.buffer->sample_rate) /
                          static_cast<double>(device.sample_rate);
      std::size_t rendered_frames = 0;
      for (std::size_t frame = 0; frame < frames && source.cursor < source.end; ++frame) {
        const float left = sample_at(
            source.buffer->samples,
            source.buffer->length,
            0,
            source.cursor);
        const float right = source.buffer->channels == 1
                                ? left
                                : sample_at(
                                      source.buffer->samples,
                                      source.buffer->length,
                                      1,
                                      source.cursor);
        const auto panned = pan_sample(left, right, source.buffer->channels, source.pan);
        const auto gain = static_cast<float>(std::clamp(
            source.gain,
            -static_cast<double>(std::numeric_limits<float>::max()),
            static_cast<double>(std::numeric_limits<float>::max())));
        device.mix[frame * channel_count] += panned[0] * gain;
        device.mix[frame * channel_count + 1] += panned[1] * gain;
        source.cursor += step;
        ++rendered_frames;
      }
      if (source.cursor >= source.end) {
        source.playing = false;
        const auto delay = std::chrono::duration_cast<Clock::duration>(
            std::chrono::duration<double>(
                static_cast<double>(rendered_frames) /
                static_cast<double>(device.sample_rate)));
        completions.push_back(Completion{source.handle, source.generation, Clock::now() + delay});
      }
    }
    for (float& sample : device.mix) {
      sample = std::isfinite(sample) ? std::clamp(sample, -1.0F, 1.0F) : 0.0F;
    }
    static_cast<void>(SDL_PutAudioStreamData(
        stream,
        device.mix.data(),
        static_cast<int>(frames * bytes_per_frame)));
  }

  mutable std::mutex mutex;
  std::unordered_map<std::uint64_t, std::unique_ptr<Device>> devices;
  std::unordered_map<std::uint64_t, std::shared_ptr<Buffer>> buffers;
  std::unordered_map<std::uint64_t, Source> sources;
  std::vector<Completion> completions;
  std::uint64_t next_device{1};
  std::uint64_t next_buffer{1};
  std::uint64_t next_source{1};
};

SdlAudioDeviceBackend::SdlAudioDeviceBackend() : state_(std::make_unique<State>()) {}

SdlAudioDeviceBackend::~SdlAudioDeviceBackend() noexcept = default;

SdlAudioDeviceBackend::SdlAudioDeviceBackend(SdlAudioDeviceBackend&&) noexcept = default;

SdlAudioDeviceBackend& SdlAudioDeviceBackend::operator=(SdlAudioDeviceBackend&&) noexcept = default;

AudioDeviceHandle SdlAudioDeviceBackend::create_device(std::uint32_t sample_rate) {
  if (state_ == nullptr || sample_rate == 0 ||
      sample_rate > static_cast<std::uint32_t>(std::numeric_limits<int>::max())) {
    return {};
  }

  auto device = std::make_unique<State::Device>();
  device->state = state_.get();
  device->sample_rate = sample_rate;
  const SDL_AudioSpec specification{SDL_AUDIO_F32, 2, static_cast<int>(sample_rate)};
  device->stream = SDL_OpenAudioDeviceStream(
      SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
      &specification,
      &State::supply_audio,
      device.get());
  if (device->stream == nullptr) return {};

  const std::scoped_lock lock(state_->mutex);
  const auto handle = next_handle<AudioDeviceHandle>(state_->next_device);
  if (!handle) {
    SDL_DestroyAudioStream(device->stream);
    return {};
  }
  device->handle = handle;
  state_->devices.emplace(handle.value, std::move(device));
  return handle;
}

void SdlAudioDeviceBackend::destroy_device(AudioDeviceHandle device_handle) noexcept {
  if (state_ == nullptr || !device_handle) return;
  std::unique_ptr<State::Device> device;
  {
    const std::scoped_lock lock(state_->mutex);
    const auto found = state_->devices.find(device_handle.value);
    if (found == state_->devices.end()) return;
    std::erase_if(state_->sources, [&](const auto& entry) {
      return entry.second.device == device_handle;
    });
    std::erase_if(state_->completions, [&](const State::Completion& completion) {
      return !state_->sources.contains(completion.source.value);
    });
    device = std::move(found->second);
    state_->devices.erase(found);
  }
  SDL_DestroyAudioStream(device->stream);
}

double SdlAudioDeviceBackend::get_device_time(AudioDeviceHandle device_handle) const noexcept {
  if (state_ == nullptr || !device_handle) return 0.0;
  const std::scoped_lock lock(state_->mutex);
  const auto found = state_->devices.find(device_handle.value);
  return found == state_->devices.end()
             ? 0.0
             : state_->device_time(*found->second, Clock::now());
}

void SdlAudioDeviceBackend::resume_device(AudioDeviceHandle device_handle) noexcept {
  if (state_ == nullptr || !device_handle) return;
  const std::scoped_lock lock(state_->mutex);
  const auto found = state_->devices.find(device_handle.value);
  if (found == state_->devices.end() || found->second->running) return;
  if (!SDL_ResumeAudioStreamDevice(found->second->stream)) return;
  found->second->running = true;
  found->second->resumed_at = Clock::now();
}

AudioBufferHandle SdlAudioDeviceBackend::create_buffer(
    AudioDeviceHandle device,
    std::uint32_t channels,
    std::size_t length,
    std::uint32_t sample_rate,
    std::span<const std::span<const float>> data) {
  if (state_ == nullptr || !device || channels == 0 || channels > maximum_channels ||
      length == 0 || sample_rate == 0) {
    return {};
  }

  {
    const std::scoped_lock lock(state_->mutex);
    if (!state_->devices.contains(device.value)) return {};
  }

  if (length > std::numeric_limits<std::size_t>::max() / channels) return {};
  auto buffer = std::make_shared<State::Buffer>();
  buffer->channels = channels;
  buffer->length = length;
  buffer->sample_rate = sample_rate;
  buffer->samples.assign(length * channels, 0.0F);
  for (std::size_t channel = 0;
       channel < std::min<std::size_t>(channels, data.size());
       ++channel) {
    const auto count = std::min(length, data[channel].size());
    std::copy_n(
        data[channel].begin(),
        count,
        buffer->samples.begin() + static_cast<std::ptrdiff_t>(channel * length));
  }

  const std::scoped_lock lock(state_->mutex);
  if (!state_->devices.contains(device.value)) return {};
  const auto handle = next_handle<AudioBufferHandle>(state_->next_buffer);
  if (!handle) return {};
  state_->buffers.emplace(handle.value, std::move(buffer));
  return handle;
}

void SdlAudioDeviceBackend::destroy_buffer(AudioBufferHandle buffer) noexcept {
  if (state_ == nullptr || !buffer) return;
  const std::scoped_lock lock(state_->mutex);
  state_->buffers.erase(buffer.value);
}

AudioSourceHandle SdlAudioDeviceBackend::create_source(
    AudioDeviceHandle device,
    AudioBufferHandle buffer) {
  if (state_ == nullptr || !device || !buffer) return {};
  const std::scoped_lock lock(state_->mutex);
  if (!state_->devices.contains(device.value)) return {};
  const auto found_buffer = state_->buffers.find(buffer.value);
  if (found_buffer == state_->buffers.end()) return {};
  const auto handle = next_handle<AudioSourceHandle>(state_->next_source);
  if (!handle) return {};
  State::Source source;
  source.handle = handle;
  source.device = device;
  source.buffer = found_buffer->second;
  state_->sources.emplace(handle.value, std::move(source));
  return handle;
}

void SdlAudioDeviceBackend::destroy_source(AudioSourceHandle source) noexcept {
  if (state_ == nullptr || !source) return;
  const std::scoped_lock lock(state_->mutex);
  state_->sources.erase(source.value);
  std::erase_if(state_->completions, [&](const State::Completion& completion) {
    return completion.source == source;
  });
}

void SdlAudioDeviceBackend::on_source_ended(
    AudioSourceHandle source,
    std::function<void()> callback) {
  if (state_ == nullptr || !source) return;
  const std::scoped_lock lock(state_->mutex);
  const auto found = state_->sources.find(source.value);
  if (found != state_->sources.end()) found->second.callback = std::move(callback);
}

void SdlAudioDeviceBackend::set_source_gain(AudioSourceHandle source, double gain) noexcept {
  if (state_ == nullptr || !source || !std::isfinite(gain)) return;
  const std::scoped_lock lock(state_->mutex);
  const auto found = state_->sources.find(source.value);
  if (found != state_->sources.end()) found->second.gain = gain;
}

void SdlAudioDeviceBackend::set_source_pan(AudioSourceHandle source, double pan) noexcept {
  if (state_ == nullptr || !source || !std::isfinite(pan)) return;
  const std::scoped_lock lock(state_->mutex);
  const auto found = state_->sources.find(source.value);
  if (found != state_->sources.end()) found->second.pan = std::clamp(pan, -1.0, 1.0);
}

void SdlAudioDeviceBackend::set_source_playback_rate(
    AudioSourceHandle source,
    double rate) noexcept {
  if (state_ == nullptr || !source || !std::isfinite(rate) || rate <= 0.0) return;
  const std::scoped_lock lock(state_->mutex);
  const auto found = state_->sources.find(source.value);
  if (found != state_->sources.end()) found->second.playback_rate = rate;
}

void SdlAudioDeviceBackend::start_source(
    AudioSourceHandle source_handle,
    double offset_seconds,
    double duration_seconds) {
  if (state_ == nullptr || !source_handle || !std::isfinite(offset_seconds) ||
      !std::isfinite(duration_seconds)) {
    return;
  }
  const std::scoped_lock lock(state_->mutex);
  const auto found = state_->sources.find(source_handle.value);
  if (found == state_->sources.end()) return;
  auto& source = found->second;
  const double length = static_cast<double>(source.buffer->length);
  const double sample_rate = static_cast<double>(source.buffer->sample_rate);
  const double offset = std::clamp(offset_seconds, 0.0, length / sample_rate);
  source.cursor = offset * sample_rate;
  source.end = duration_seconds > 0.0
                   ? std::min(length, source.cursor + duration_seconds * sample_rate)
                   : length;
  ++source.generation;
  source.playing = source.cursor < source.end;
  std::erase_if(state_->completions, [&](const State::Completion& completion) {
    return completion.source == source_handle;
  });
  if (!source.playing) {
    state_->completions.push_back(
        State::Completion{source.handle, source.generation, Clock::now()});
  }
}

void SdlAudioDeviceBackend::stop_source(AudioSourceHandle source_handle) noexcept {
  if (state_ == nullptr || !source_handle) return;
  const std::scoped_lock lock(state_->mutex);
  const auto found = state_->sources.find(source_handle.value);
  if (found == state_->sources.end()) return;
  found->second.playing = false;
  ++found->second.generation;
  std::erase_if(state_->completions, [&](const State::Completion& completion) {
    return completion.source == source_handle;
  });
}

std::size_t SdlAudioDeviceBackend::pump() {
  if (state_ == nullptr) return 0;
  std::vector<std::function<void()>> callbacks;
  {
    const std::scoped_lock lock(state_->mutex);
    const auto turn = std::move(state_->completions);
    state_->completions.clear();
    callbacks.reserve(turn.size());
    const auto now = Clock::now();
    for (const auto& completion : turn) {
      if (completion.ready_at > now) {
        state_->completions.push_back(completion);
        continue;
      }
      const auto found = state_->sources.find(completion.source.value);
      if (found == state_->sources.end() ||
          found->second.generation != completion.generation ||
          !found->second.callback) {
        continue;
      }
      callbacks.push_back(found->second.callback);
    }
  }
  for (auto& callback : callbacks) callback();
  return callbacks.size();
}

} // namespace flight::host_sdl
