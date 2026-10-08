// Native SDL port of @flighthq/example-crossfade.
// Source: .dependencies/flight/examples/packages/crossfade/src/app.ts

#include "sdl_host.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <numbers>
#include <string>

namespace {

using flight::examples::Canvas;
using flight::examples::Frame;
using flight::examples::Point;

[[nodiscard]] Point leg_point(double x, double y, double length, double angle) {
  const double radians = (90.0 + angle) * std::numbers::pi / 180.0;
  return {x + std::cos(radians) * length, y + std::sin(radians) * length};
}

void draw_character(
    Canvas& canvas,
    double x,
    double y,
    double left_angle,
    double right_angle,
    std::uint32_t color) {
  canvas.fill_circle(x, y - 70.0, 18.0, color);
  canvas.line(x, y - 50.0, x, y + 12.0, color, 8.0);
  canvas.line(x, y - 32.0, x - 26.0, y - 4.0, color, 8.0);
  canvas.line(x, y - 32.0, x + 26.0, y - 4.0, color, 8.0);
  const Point left_knee = leg_point(x - 4.0, y + 10.0, 43.0, left_angle);
  const Point right_knee = leg_point(x + 4.0, y + 10.0, 43.0, right_angle);
  canvas.line(x - 4.0, y + 10.0, left_knee.x, left_knee.y, color, 8.0);
  canvas.line(left_knee.x, left_knee.y, left_knee.x - 12.0, left_knee.y + 38.0, color, 8.0);
  canvas.line(x + 4.0, y + 10.0, right_knee.x, right_knee.y, color, 8.0);
  canvas.line(right_knee.x, right_knee.y, right_knee.x + 12.0, right_knee.y + 38.0, color, 8.0);
}

[[nodiscard]] double smooth(double value) { return value * value * (3.0 - 2.0 * value); }

class CrossfadeExample final {
 public:
  void frame(const Frame& frame) {
    if (frame.input.pointer_pressed && !transitioning_) {
      destination_locomotion_ = !locomotion_;
      transition_from_ = locomotion_weight_;
      transition_to_ = destination_locomotion_ ? 1.0 : 0.0;
      transition_time_ = 0.0;
      transitioning_ = true;
    }
    phase_ = std::fmod(phase_ + frame.delta_time, 2.0);
    if (transitioning_) {
      transition_time_ += frame.delta_time;
      const double amount = std::clamp(transition_time_ / 1.2, 0.0, 1.0);
      locomotion_weight_ = transition_from_ + (transition_to_ - transition_from_) * smooth(amount);
      if (amount >= 1.0) {
        transitioning_ = false;
        locomotion_ = destination_locomotion_;
      }
    }
    draw(frame);
  }

 private:
  void draw(const Frame& frame) const {
    auto& canvas = frame.canvas;
    canvas.text("Animation State Machine", 24.0, 18.0, 0xffffffffU, 26.0);
    canvas.text("Idle -> N-way locomotion | masked additive body layer", 24.0, 54.0, 0x9aa9c7ffU, 14.0);
    canvas.line(30.0, 366.0, 770.0, 366.0, 0x34405affU, 2.0);
    draw_character(canvas, 115.0, 265.0, -3.0, 3.0, 0x7086adffU);
    draw_character(canvas, 685.0, 259.0, -28.0, 28.0, 0xf0a85bffU);

    const double wave = std::sin(phase_ * std::numbers::pi);
    const double idle_body = -std::abs(wave);
    const double idle_left = -3.0 * wave;
    const double idle_right = 3.0 * wave;
    const double walk_body = -12.0 * std::abs(wave);
    const double run_body = -18.0 * std::abs(wave);
    const double walk_left = -28.0 * wave;
    const double walk_right = 28.0 * wave;
    const double run_left = -45.0 * wave;
    const double run_right = 45.0 * wave;
    const double locomotion_body = walk_body * 0.7 + run_body * 0.3 - 3.0 * std::abs(wave);
    const double locomotion_left = walk_left * 0.7 + run_left * 0.3;
    const double locomotion_right = walk_right * 0.7 + run_right * 0.3;
    const double body = idle_body + (locomotion_body - idle_body) * locomotion_weight_;
    const double left = idle_left + (locomotion_left - idle_left) * locomotion_weight_;
    const double right = idle_right + (locomotion_right - idle_right) * locomotion_weight_;
    draw_character(canvas, 400.0, 265.0 + body, left, right, 0x63e6beffU);

    canvas.text("IDLE", 86.0, 390.0, 0x7086adffU, 14.0);
    canvas.text("BLENDED POSE", 330.0, 390.0, 0x63e6beffU, 14.0);
    canvas.text("WALK/RUN", 644.0, 390.0, 0xf0a85bffU, 14.0);
    canvas.fill_rectangle(240.0, 414.0, 320.0, 12.0, 0x2c3850ffU);
    canvas.fill_rectangle(240.0, 414.0, std::max(0.32, 320.0 * locomotion_weight_), 12.0, 0x63e6beffU);
    const std::string status = transitioning_
                                   ? "Idle -> locomotion  " + std::to_string(static_cast<int>(locomotion_weight_ * 100.0)) + "%"
                                   : locomotion_ ? "Locomotion | walk/run + masked body accent" : "Idle";
    canvas.text(status, 250.0, 438.0, 0xffffffffU, 15.0);
    canvas.text("CLICK THE SCENE TO CROSSFADE", 283.0, 470.0, 0x8294b7ffU, 13.0);
  }

  double phase_{};
  double locomotion_weight_{};
  double transition_from_{};
  double transition_to_{};
  double transition_time_{};
  bool locomotion_{};
  bool destination_locomotion_{};
  bool transitioning_{};
};

} // namespace

int main(int argc, char** argv) {
  try {
    CrossfadeExample example;
    return flight::examples::SdlExample::run(
        argc,
        argv,
        "Flight animation crossfade - SDL",
        [&](const Frame& frame) { example.frame(frame); });
  } catch (const std::exception& error) {
    std::cerr << "crossfade example failed: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
