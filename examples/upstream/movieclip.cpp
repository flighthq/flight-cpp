// Native SDL port of @flighthq/example-movieclip.
// Source: .dependencies/flight/examples/packages/movieclip/src/app.ts

#include "sdl_host.hpp"

#include <array>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>
#include <string_view>

namespace {

using flight::examples::Frame;
using flight::examples::Point;

constexpr int total_frames = 24;
constexpr double frame_rate = 8.0;

[[nodiscard]] std::string_view label_for(int frame) {
  if (frame >= 19) return "outro";
  if (frame >= 9) return "loop";
  return "intro";
}

class MovieClipExample final {
 public:
  void frame(const Frame& frame) {
    handle_keys(frame);
    if (playing_) {
      accumulator_ += frame.delta_time * frame_rate;
      while (accumulator_ >= 1.0) {
        accumulator_ -= 1.0;
        set_frame(current_frame_ == total_frames ? 1 : current_frame_ + 1);
      }
    }
    draw_content(frame);
    draw_hud(frame);
  }

 private:
  void handle_keys(const Frame& frame) {
    if (frame.input.key_pressed(SDLK_SPACE)) playing_ = !playing_;
    if (frame.input.key_pressed(SDLK_LEFT)) {
      playing_ = false;
      set_frame(current_frame_ == 1 ? total_frames : current_frame_ - 1);
    }
    if (frame.input.key_pressed(SDLK_RIGHT)) {
      playing_ = false;
      set_frame(current_frame_ == total_frames ? 1 : current_frame_ + 1);
    }
    if (frame.input.key_pressed(SDLK_1)) {
      playing_ = true;
      set_frame(1);
    }
    if (frame.input.key_pressed(SDLK_2)) {
      playing_ = true;
      set_frame(9);
    }
    if (frame.input.key_pressed(SDLK_3)) {
      playing_ = false;
      set_frame(19);
    }
  }

  void set_frame(int frame) {
    current_frame_ = frame;
    if (frame == 1 || frame == 9 || frame == 19) {
      script_message_ = "Frame script: entered \"" + std::string(label_for(frame)) + "\"";
    }
  }

  void draw_content(const Frame& frame) const {
    constexpr double x = 400.0;
    constexpr double y = 200.0;
    if (current_frame_ <= 8) {
      const double progress = static_cast<double>(current_frame_) / 8.0;
      const double size = 40.0 + progress * 80.0;
      frame.canvas.fill_rectangle(x - size / 2.0, y - size / 2.0, size, size, 0x4488ccffU);
    } else if (current_frame_ <= 18) {
      const double progress = static_cast<double>(current_frame_ - 8) / 10.0;
      const double size = 60.0 + progress * 40.0;
      const double half = size / 2.0;
      const double height = size * std::sqrt(3.0) / 2.0;
      const std::array<Point, 3> points{{
          {x, y - height / 2.0},
          {x + half, y + height / 2.0},
          {x - half, y + height / 2.0},
      }};
      frame.canvas.fill_polygon(points, 0x44cc88ffU);
    } else {
      const double progress = static_cast<double>(current_frame_ - 18) / 6.0;
      frame.canvas.fill_circle(x, y, 60.0 - progress * 40.0, 0xcc4444ffU);
    }
  }

  void draw_hud(const Frame& frame) const {
    frame.canvas.text("Movie Clip", 20.0, 10.0, 0xffffffffU, 24.0);
    frame.canvas.text(
        "Frame: " + std::to_string(current_frame_) + " / " + std::to_string(total_frames),
        20.0,
        50.0,
        0xccccccffU,
        16.0);
    frame.canvas.text("Label: " + std::string(label_for(current_frame_)), 20.0, 75.0, 0x88aaccffU, 16.0);
    frame.canvas.text(playing_ ? "Playing" : "Stopped", 20.0, 100.0, 0x88cc88ffU, 16.0);
    frame.canvas.text(script_message_, 20.0, 125.0, 0xccaa44ffU, 14.0);
    frame.canvas.text(
        "Space play/stop   Left/Right step   1 intro   2 loop   3 outro",
        20.0,
        470.0,
        0x888888ffU,
        13.0);
  }

  int current_frame_{1};
  bool playing_{true};
  double accumulator_{};
  std::string script_message_{"Frame script: entered \"intro\""};
};

} // namespace

int main(int argc, char** argv) {
  try {
    MovieClipExample example;
    return flight::examples::SdlExample::run(
        argc,
        argv,
        "Flight movie clip - SDL",
        [&](const Frame& frame) { example.frame(frame); });
  } catch (const std::exception& error) {
    std::cerr << "movieclip example failed: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
