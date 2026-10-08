// Naive C++ port of @flighthq/example-movieclip
// Ported from .dependencies/flight/examples/packages/movieclip/src/app.ts

#include <flight/movieclip/movie_clip.hpp>
#include <flight/node/hierarchy.hpp>
#include <flight/node/node_transform2d.hpp>
#include <flight/scene2d/display_object.hpp>
#include <flight/shape/shape.hpp>
#include <flight/shape/shape_commands.hpp>
#include <flight/text/text_label.hpp>

#include <cmath>
#include <iostream>

namespace {

constexpr int TOTAL_FRAMES = 24;
constexpr int FRAME_RATE = 8;

} // namespace

int main() {
  using namespace flight::node;
  using namespace flight::shape;
  using namespace flight::scene2d;
  using namespace flight::movieclip;

  // NOT AVAILABLE: generated MovieClip timeline and Scene2D construction are partial at this SDK pin.
  auto root = create_display_object(std::nullopt);

  // Create a MovieClip with labeled sections.
  auto clip = create_movie_clip();
  clip->x = 100.0;
  clip->y = 100.0;
  invalidate_node_local_transform(clip);
  add_node_child(root, clip);

  // Create a timeline source for frame-based animation.
  auto source = flight::movieclip::create_timeline_source();

  // Set up the movie clip with its source.
  set_movie_clip_source(clip, source);

  // Add frame scripts at specific frames.
  add_movie_clip_frame_script(clip, 0, [&]() {
    // Frame 0: draw initial state.
  });
  add_movie_clip_frame_script(clip, 12, [&]() {
    // Frame 12: midpoint action.
  });

  // Child shape for visual content.
  auto shape = create_shape(std::nullopt);
  shape->x = 300.0;
  shape->y = 180.0;
  invalidate_node_local_transform(shape);
  add_node_child(clip, shape);

  // Draw a simple animated shape based on frame.
  for (int frame = 0; frame < TOTAL_FRAMES; ++frame) {
    double t = static_cast<double>(frame) / TOTAL_FRAMES;
    double radius = 30.0 + 20.0 * std::sin(t * M_PI * 2.0);
    double hue = t * 360.0;

    clear_shape_commands(shape);
    std::uint32_t color = static_cast<std::uint32_t>(
      (static_cast<int>(128.0 + 127.0 * std::sin(hue * M_PI / 180.0)) << 24) |
      (static_cast<int>(128.0 + 127.0 * std::sin((hue + 120.0) * M_PI / 180.0)) << 16) |
      (static_cast<int>(128.0 + 127.0 * std::sin((hue + 240.0) * M_PI / 180.0)) << 8) |
      0xff);
    append_shape_begin_fill(shape, color);
    append_shape_circle(shape, 0.0, 0.0, radius);
    append_shape_end_fill(shape);
  }

  // Playback controls.
  play_movie_clip(clip);
  goto_and_play_movie_clip(clip, 0);

  // Simulate playback for a few seconds.
  for (int i = 0; i < 120; ++i) {
    double dt = 1000.0 / 60.0;
    update_movie_clip(clip, dt);
  }

  auto currentFrame = get_movie_clip_current_frame(clip);
  auto totalFrames = get_movie_clip_total_frames(clip);
  bool playing = is_movie_clip_playing(clip);

  stop_movie_clip(clip);
  next_frame_movie_clip(clip);
  prev_frame_movie_clip(clip);
  goto_and_stop_movie_clip(clip, 5);

  // HUD label.
  auto label = flight::text::create_text_label();
  label->data.text = flight::String("MOVIE CLIP EXAMPLE");
  label->x = 20.0;
  label->y = 20.0;
  invalidate_node_local_transform(label);
  add_node_child(root, label);

  std::cout << "Flight movieclip example (naive C++ port): "
            << TOTAL_FRAMES << " frames, "
            << "current=" << currentFrame << ", "
            << "total=" << totalFrames << ", "
            << "playing=" << (playing ? "true" : "false") << "\n";
  return 0;
}
