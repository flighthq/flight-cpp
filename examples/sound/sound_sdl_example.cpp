#include "generated/sound.hpp"

#include <flight/audio_buffer.hpp>
#include <flight/host_sdl/sdk_audio.hpp>
#include <flight/types/audio_device_backend.hpp>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <thread>

namespace {

constexpr std::uint32_t sample_rate = 44'100;

struct Voice final {
  double source{};
  bool completed{};
};

[[nodiscard]] flight::AudioBuffer make_audio_buffer(const flight::Array<double>& generated_samples) {
  const auto samples = flight::Float32Array::from(generated_samples);
  flight::AudioBuffer buffer({
      .length = static_cast<double>(samples.size()),
      .number_of_channels = 1.0,
      .sample_rate = static_cast<double>(sample_rate),
  });
  buffer.copy_to_channel(samples, 0.0);
  return buffer;
}

void configure_voice(
    flight::types::AudioDeviceBackend& backend,
    double device,
    const flight::AudioBuffer& decoded,
    Voice& voice,
    double gain,
    double pan) {
  const auto channel = decoded.get_channel_data(0.0);
  const auto buffer = backend.create_buffer(
      device,
      decoded.number_of_channels,
      decoded.length,
      decoded.sample_rate,
      flight::Array<flight::Float32Array>{channel});
  if (buffer == 0.0) throw std::runtime_error("SDL could not create a Flight audio buffer");

  voice.source = backend.create_source(device, buffer);
  backend.destroy_buffer(buffer);
  if (voice.source == 0.0) throw std::runtime_error("SDL could not create a Flight audio source");
  backend.on_source_ended(
      voice.source,
      std::function<void()>([&voice] { voice.completed = true; }));
  backend.set_source_gain(voice.source, gain);
  backend.set_source_pan(voice.source, pan);
}

} // namespace

int main(int argc, char** argv) {
  const bool smoke = argc > 1 && std::string_view(argv[1]) == "--smoke";
  try {
    const double scale = smoke ? 0.1 : 1.0;
    const auto click = make_audio_buffer(flighthq_examples_sound::generate_tone_samples(
        440.0,
        0.15 * scale,
        20.0,
        static_cast<double>(sample_rate)));
    const auto blip = make_audio_buffer(flighthq_examples_sound::generate_tone_samples(
        880.0,
        0.08 * scale,
        40.0,
        static_cast<double>(sample_rate)));
    const auto sweep = make_audio_buffer(flighthq_examples_sound::generate_sweep_samples(
        200.0,
        800.0,
        0.3 * scale,
        6.0,
        static_cast<double>(sample_rate)));

    flight::host_sdl::SdkAudioDeviceBackend adapter;
    auto backend = adapter.backend();
    const auto device = backend.create_device(static_cast<double>(sample_rate));
    if (device == 0.0) throw std::runtime_error("SDL could not open the default audio device");

    std::array<Voice, 3> voices{};
    configure_voice(backend, device, click, voices[0], 0.8, -0.5);
    configure_voice(backend, device, blip, voices[1], 0.7, 0.0);
    configure_voice(backend, device, sweep, voices[2], 0.6, 0.5);
    for (const auto& voice : voices) backend.start_source(voice.source, 0.0, 0.0);
    backend.resume_device(device);

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < deadline) {
      static_cast<void>(adapter.pump());
      if (voices[0].completed && voices[1].completed && voices[2].completed) break;
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    const bool completed = voices[0].completed && voices[1].completed && voices[2].completed;
    for (const auto& voice : voices) backend.destroy_source(voice.source);
    backend.destroy_device(device);
    if (!completed) throw std::runtime_error("SDL audio playback did not complete");

    std::cout << "Flight sound example played compiler-generated 440 Hz, 880 Hz, and sweep PCM\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "Flight sound example failed: " << error.what() << '\n';
    return 1;
  }
}
