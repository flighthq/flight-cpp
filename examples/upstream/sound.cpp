// Native SDL port of @flighthq/example-sound.
// Source: .dependencies/flight/examples/packages/sound/src/app.ts

#include "sdl_host.hpp"

#include <flight/audio_buffer.hpp>
#include <flight/host_sdl/sdk_audio.hpp>
#include <flight/types/host_audio_device.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <functional>
#include <iostream>
#include <memory>
#include <numbers>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using flight::examples::Canvas;
using flight::examples::Frame;

constexpr std::uint32_t sample_rate = 44'100;

[[nodiscard]] flight::AudioBuffer tone(double frequency, double duration, double decay) {
  const auto length = static_cast<std::size_t>(std::floor(static_cast<double>(sample_rate) * duration));
  flight::Float32Array samples(length);
  for (std::size_t index = 0; index < length; ++index) {
    const double time = static_cast<double>(index) / static_cast<double>(sample_rate);
    samples[index] = static_cast<float>(
        std::sin(2.0 * std::numbers::pi * frequency * time) * std::exp(-decay * time));
  }
  flight::AudioBuffer buffer({
      .length = static_cast<double>(length),
      .number_of_channels = 1.0,
      .sample_rate = static_cast<double>(sample_rate),
  });
  buffer.copy_to_channel(samples, 0.0);
  return buffer;
}

[[nodiscard]] flight::AudioBuffer sweep(
    double start_frequency,
    double end_frequency,
    double duration,
    double decay) {
  const auto length = static_cast<std::size_t>(std::floor(static_cast<double>(sample_rate) * duration));
  flight::Float32Array samples(length);
  double phase{};
  for (std::size_t index = 0; index < length; ++index) {
    const double time = static_cast<double>(index) / static_cast<double>(sample_rate);
    const double frequency = start_frequency + (end_frequency - start_frequency) * (time / duration);
    samples[index] = static_cast<float>(std::sin(phase) * std::exp(-decay * time));
    phase += 2.0 * std::numbers::pi * frequency / static_cast<double>(sample_rate);
  }
  flight::AudioBuffer buffer({
      .length = static_cast<double>(length),
      .number_of_channels = 1.0,
      .sample_rate = static_cast<double>(sample_rate),
  });
  buffer.copy_to_channel(samples, 0.0);
  return buffer;
}

enum class Resource { click, blip, sweep };

struct Button final {
  std::string label;
  std::uint32_t color{};
  std::uint32_t hover_color{};
  double x{};
  double pan{};
  Resource resource{};
};

struct Voice final {
  double source{};
  double gain{};
  double pan{};
  std::atomic_bool completed{};
};

class SoundExample final {
 public:
  SoundExample()
      : click_(tone(440.0, 0.15, 20.0)),
        blip_(tone(880.0, 0.08, 40.0)),
        sweep_(sweep(200.0, 800.0, 0.3, 6.0)),
        backend_(adapter_.backend()) {}

  void frame(const Frame& frame) {
    static_cast<void>(adapter_.pump());
    collect_finished();
    handle_pointer(frame);
    draw(
        frame.canvas,
        frame.input.pointer_x,
        frame.input.pointer_y,
        frame.input.pointer_inside);
  }

  void shutdown() {
    for (const auto& voice : voices_) {
      backend_.stop_source(voice->source);
      backend_.destroy_source(voice->source);
    }
    voices_.clear();
    if (device_ != 0.0) {
      backend_.destroy_device(device_);
      device_ = 0.0;
    }
  }

 private:
  [[nodiscard]] const flight::AudioBuffer& resource(Resource resource) const {
    if (resource == Resource::click) return click_;
    if (resource == Resource::blip) return blip_;
    return sweep_;
  }

  void ensure_device() {
    if (device_ != 0.0) return;
    device_ = backend_.create_device(static_cast<double>(sample_rate));
    if (device_ == 0.0) throw std::runtime_error("SDL could not open the default audio device");
    backend_.resume_device(device_);
  }

  void play(const Button& button) {
    ensure_device();
    const auto& decoded = resource(button.resource);
    const auto channel = decoded.get_channel_data(0.0);
    const double buffer = backend_.create_buffer(
        device_,
        decoded.number_of_channels,
        decoded.length,
        decoded.sample_rate,
        flight::Array<flight::Float32Array>{channel});
    if (buffer == 0.0) throw std::runtime_error("SDL could not create an audio buffer");
    auto voice = std::make_shared<Voice>();
    voice->source = backend_.create_source(device_, buffer);
    backend_.destroy_buffer(buffer);
    if (voice->source == 0.0) throw std::runtime_error("SDL could not create an audio source");
    voice->gain = 0.8;
    voice->pan = button.pan;
    backend_.on_source_ended(
        voice->source,
        std::function<void()>([voice] { voice->completed.store(true); }));
    voices_.push_back(voice);
    update_voice(*voice);
    backend_.start_source(voice->source, 0.0, 0.0);
    status_ = "Played: " + button.label;
  }

  void update_voice(const Voice& voice) {
    backend_.set_source_gain(voice.source, voice.gain * sfx_gain_ * master_gain_);
    backend_.set_source_pan(voice.source, std::clamp(voice.pan + sfx_pan_, -1.0, 1.0));
  }

  void update_voices() {
    for (const auto& voice : voices_) update_voice(*voice);
  }

  void collect_finished() {
    std::erase_if(voices_, [&](const auto& voice) {
      if (!voice->completed.load()) return false;
      backend_.destroy_source(voice->source);
      return true;
    });
  }

  void handle_pointer(const Frame& frame) {
    if (!frame.input.pointer_pressed) return;
    for (const auto& button : buttons_) {
      if (flight::examples::contains(button.x, 120.0, 180.0, 80.0, frame.input.pointer_x, frame.input.pointer_y)) {
        play(button);
        return;
      }
    }
    if (flight::examples::contains(60.0, 280.0, 680.0, 30.0, frame.input.pointer_x, frame.input.pointer_y)) {
      master_gain_ = std::clamp((frame.input.pointer_x - 60.0) / 680.0, 0.0, 1.0);
      update_voices();
      status_ = "Master volume: " + std::to_string(static_cast<int>(std::lround(master_gain_ * 100.0))) + "%";
      return;
    }
    if (flight::examples::contains(60.0, 380.0, 300.0, 24.0, frame.input.pointer_x, frame.input.pointer_y)) {
      sfx_gain_ = std::clamp((frame.input.pointer_x - 60.0) / 300.0, 0.0, 1.0);
      update_voices();
      status_ = "SFX bus gain: " + std::to_string(static_cast<int>(std::lround(sfx_gain_ * 100.0))) + "%";
      return;
    }
    if (flight::examples::contains(440.0, 380.0, 300.0, 24.0, frame.input.pointer_x, frame.input.pointer_y)) {
      music_gain_ = std::clamp((frame.input.pointer_x - 440.0) / 300.0, 0.0, 1.0);
      status_ = "Music bus gain: " + std::to_string(static_cast<int>(std::lround(music_gain_ * 100.0))) + "%";
      return;
    }
    if (flight::examples::contains(60.0, 450.0, 300.0, 24.0, frame.input.pointer_x, frame.input.pointer_y)) {
      sfx_pan_ = std::clamp((frame.input.pointer_x - 60.0) / 150.0 - 1.0, -1.0, 1.0);
      update_voices();
      status_ = "SFX bus pan: " + pan_text(sfx_pan_);
      return;
    }
    if (flight::examples::contains(440.0, 450.0, 300.0, 24.0, frame.input.pointer_x, frame.input.pointer_y)) {
      music_pan_ = std::clamp((frame.input.pointer_x - 440.0) / 150.0 - 1.0, -1.0, 1.0);
      status_ = "Music bus pan: " + pan_text(music_pan_);
    }
  }

  [[nodiscard]] static std::string pan_text(double pan) {
    const int hundredths = static_cast<int>(std::lround(pan * 100.0));
    const int absolute = std::abs(hundredths);
    return (hundredths < 0 ? "-" : "") + std::to_string(absolute / 100) + "." +
           (absolute % 100 < 10 ? "0" : "") + std::to_string(absolute % 100);
  }

  static void slider(
      Canvas& canvas,
      double x,
      double y,
      double width,
      double height,
      double amount,
      std::uint32_t fill) {
    canvas.fill_rectangle(x, y, width, height, 0x444444ffU, 0.4);
    canvas.fill_rectangle(x, y, std::max(1.0, width * amount), height, fill, 0.8);
  }

  static void pan_slider(
      Canvas& canvas,
      double x,
      double y,
      double pan,
      std::uint32_t fill) {
    constexpr double slider_width = 300.0;
    constexpr double slider_height = 24.0;
    canvas.fill_rectangle(x, y, slider_width, slider_height, 0x444444ffU, 0.4);
    const double center = x + slider_width / 2.0;
    const double position = x + (pan / 2.0 + 0.5) * slider_width;
    canvas.fill_rectangle(std::min(center, position), y, std::max(1.0, std::abs(position - center)), slider_height, fill, 0.8);
  }

  void draw(Canvas& canvas, double pointer_x, double pointer_y, bool pointer_inside) const {
    canvas.text("Sound Example - Procedural Audio & Mixer", 60.0, 30.0, 0xccccccffU, 18.0);
    canvas.text("Click the buttons to play procedurally generated tones.", 60.0, 65.0, 0x888888ffU, 13.0);
    for (const auto& button : buttons_) {
      const bool hovered = pointer_inside &&
                           flight::examples::contains(
                               button.x,
                               120.0,
                               180.0,
                               80.0,
                               pointer_x,
                               pointer_y);
      canvas.fill_rectangle(button.x, 120.0, 180.0, 80.0, hovered ? button.hover_color : button.color, 0.9);
      canvas.text(button.label, button.x + 12.0, 152.0, 0xffffffffU, 12.0);
    }

    canvas.text("Master Volume", 60.0, 256.0, 0xaaaaaaffU, 13.0);
    slider(canvas, 60.0, 280.0, 680.0, 30.0, master_gain_, 0xddddddffU);
    canvas.fill_rectangle(60.0 + 680.0 * master_gain_ - 4.0, 276.0, 8.0, 38.0, 0xffffffffU);
    canvas.text(std::to_string(static_cast<int>(std::lround(master_gain_ * 100.0))) + "%", 752.0, 284.0, 0xddddddffU, 13.0);

    canvas.text("SFX Bus Gain", 60.0, 360.0, 0x8899bbffU, 12.0);
    canvas.text("Music Bus Gain", 440.0, 360.0, 0x88bb99ffU, 12.0);
    slider(canvas, 60.0, 380.0, 300.0, 24.0, sfx_gain_, 0x3a7bd5ffU);
    slider(canvas, 440.0, 380.0, 300.0, 24.0, music_gain_, 0x2ecc71ffU);
    canvas.text("SFX Bus Pan", 60.0, 430.0, 0x8899bbffU, 12.0);
    canvas.text("Music Bus Pan", 440.0, 430.0, 0x88bb99ffU, 12.0);
    pan_slider(canvas, 60.0, 450.0, sfx_pan_, 0x3a7bd5ffU);
    pan_slider(canvas, 440.0, 450.0, music_pan_, 0x2ecc71ffU);
    canvas.text(status_, 60.0, 530.0, 0x999999ffU, 13.0);
  }

  const flight::AudioBuffer click_;
  const flight::AudioBuffer blip_;
  const flight::AudioBuffer sweep_;
  flight::host_sdl::SdkAudioDeviceBackend adapter_;
  flight::types::HostAudioDeviceCapability backend_;
  double device_{};
  std::vector<std::shared_ptr<Voice>> voices_;
  double master_gain_{0.7};
  double sfx_gain_{0.8};
  double music_gain_{0.6};
  double sfx_pan_{};
  double music_pan_{};
  std::string status_{"Ready"};
  const std::array<Button, 3> buttons_{{
      {"Click (440 Hz)", 0x3a7bd5ffU, 0x5a9bf5ffU, 60.0, -0.5, Resource::click},
      {"Blip (880 Hz)", 0x2ecc71ffU, 0x4eec91ffU, 280.0, 0.0, Resource::blip},
      {"Sweep (200-800 Hz)", 0xe74c3cffU, 0xf76c5cffU, 500.0, 0.5, Resource::sweep},
  }};
};

} // namespace

int main(int argc, char** argv) {
  try {
    SoundExample example;
    return flight::examples::SdlExample::run(
        argc,
        argv,
        "Flight sound - SDL",
        [&](const Frame& frame) { example.frame(frame); },
        0x1a1a2effU,
        [&] { example.shutdown(); });
  } catch (const std::exception& error) {
    std::cerr << "sound example failed: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
