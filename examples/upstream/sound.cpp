// Naive C++ port of @flighthq/example-sound
// Ported from .dependencies/flight/examples/packages/sound/src/app.ts

#include <flight/media/audio_mixer.hpp>
#include <flight/node/hierarchy.hpp>
#include <flight/node/node_transform2d.hpp>
#include <flight/scene2d/display_object.hpp>
#include <flight/shape/shape.hpp>
#include <flight/shape/shape_commands.hpp>
#include <flight/text/text_label.hpp>

#include <iostream>

int main() {
  using namespace flight::node;
  using namespace flight::shape;
  using namespace flight::scene2d;

  auto root = create_display_object(std::nullopt);

  auto mixer = flight::make_ref<flight::types::AudioMixer>(flight::types::AudioMixer{
      .entity_runtime_key = std::nullopt,
      .master_gain = 0.8,
      .master_muted = false,
  });
  auto mainBus = flight::media::create_audio_bus();

  auto sfxBus = flight::media::create_audio_bus();
  sfxBus->gain = 0.6;

  auto musicBus = flight::media::create_audio_bus();
  musicBus->gain = 0.4;

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
    label->data->text = flight::String(btn.label);
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

  std::cout << "Flight sound example (naive C++ port): "
            << buttons.size() << " tone buttons and three mixer buses created at master gain "
            << mixer->master_gain << ".\n";
  return 0;
}
