// Naive C++ port of @flighthq/example-sound
// Ported from .dependencies/flight/examples/packages/sound/src/app.ts

#include <flight/media/audio_mixer.hpp>
#include <flight/audio/audio_resource.hpp>
#include <flight/audio/audio_resource_from.hpp>
#include <flight/media/audio_channel.hpp>
#include <flight/node/hierarchy.hpp>
#include <flight/node/node_transform2d.hpp>
#include <flight/scene2d/display_object.hpp>
#include <flight/shape/shape.hpp>
#include <flight/shape/shape_commands.hpp>
#include <flight/text/text_label.hpp>

#include <cmath>
#include <iostream>
#include <vector>

namespace {

constexpr int SAMPLE_RATE = 44100;

std::vector<float> generate_tone(double frequency, double duration, double decay) {
  int length = static_cast<int>(SAMPLE_RATE * duration);
  std::vector<float> samples(length);
  for (int i = 0; i < length; ++i) {
    double t = static_cast<double>(i) / SAMPLE_RATE;
    double envelope = std::exp(-decay * t);
    samples[i] = static_cast<float>(std::sin(2.0 * M_PI * frequency * t) * envelope);
  }
  return samples;
}

} // namespace

int main() {
  using namespace flight::node;
  using namespace flight::shape;
  using namespace flight::scene2d;
  using namespace flight::audio;

  // NOT AVAILABLE: generated audio mixer and Scene2D construction are partial at this SDK pin.
  auto root = create_display_object(std::nullopt);

  auto mixer = create_audio_mixer();
  auto mainBus = create_audio_bus();
  add_audio_bus_to_mixer(mixer, mainBus);
  set_audio_mixer_master_gain(mixer, 0.8);

  auto sfxBus = create_audio_bus();
  add_audio_bus_to_mixer(mixer, sfxBus);
  set_audio_bus_gain(sfxBus, 0.6);

  auto musicBus = create_audio_bus();
  add_audio_bus_to_mixer(mixer, musicBus);
  set_audio_bus_gain(musicBus, 0.4);

  auto toneSamples = generate_tone(440.0, 0.5, 4.0);
  flight::Float32Array sampleData(toneSamples.size());
  for (std::size_t i = 0; i < toneSamples.size(); ++i) {
    sampleData.set(i, toneSamples[i]);
  }
  auto toneResource = create_audio_resource_from_samples(sampleData, SAMPLE_RATE);

  struct ButtonDef {
    double x, y, w, h;
    std::uint32_t color;
    std::string_view label;
    double frequency;
  };

  std::array<ButtonDef, 4> buttons{{
    {50.0, 100.0, 150.0, 60.0, 0x2196f3ff, "C4 (262 Hz)", 261.63},
    {220.0, 100.0, 150.0, 60.0, 0x4caf50ff, "E4 (330 Hz)", 329.63},
    {390.0, 100.0, 150.0, 60.0, 0xff9800ff, "G4 (392 Hz)", 392.00},
    {560.0, 100.0, 150.0, 60.0, 0xe91e63ff, "C5 (523 Hz)", 523.25},
  }};

  for (auto& btn : buttons) {
    auto shape = create_shape(std::nullopt);
    append_shape_begin_fill(shape, btn.color);
    append_shape_rectangle(shape, btn.x, btn.y, btn.w, btn.h);
    append_shape_end_fill(shape);
    add_node_child(root, shape);

    auto label = flight::text::create_text_label();
    label->data.text = flight::String(btn.label);
    label->x = btn.x + 10.0;
    label->y = btn.y + 20.0;
    invalidate_node_local_transform(label);
    add_node_child(root, label);
  }

  auto gainSlider = create_shape(std::nullopt);
  append_shape_begin_fill(gainSlider, 0x666666ff);
  append_shape_rectangle(gainSlider, 50.0, 250.0, 300.0, 8.0);
  append_shape_end_fill(gainSlider);
  append_shape_begin_fill(gainSlider, 0xffffffff);
  append_shape_circle(gainSlider, 50.0 + 300.0 * 0.8, 254.0, 12.0);
  append_shape_end_fill(gainSlider);
  add_node_child(root, gainSlider);

  auto panSlider = create_shape(std::nullopt);
  append_shape_begin_fill(panSlider, 0x666666ff);
  append_shape_rectangle(panSlider, 50.0, 320.0, 300.0, 8.0);
  append_shape_end_fill(panSlider);
  append_shape_begin_fill(panSlider, 0xffffffff);
  append_shape_circle(panSlider, 50.0 + 150.0, 324.0, 12.0);
  append_shape_end_fill(panSlider);
  add_node_child(root, panSlider);

  play_audio_resource(toneResource, sfxBus);

  std::cout << "Flight sound example (naive C++ port): "
            << buttons.size() << " tone buttons created.\n";
  return 0;
}
