// Native SDL port of @flighthq/example-motionpath.
// Source: .dependencies/flight/examples/packages/motionpath/src/app.ts

#include "sdl_host.hpp"

#include <flight/geometry/vector2.hpp>
#include <flight/motionpath/motion_path.hpp>
#include <flight/path/path.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <numbers>
#include <string>

namespace {

using flight::examples::Frame;
using flight::examples::Point;

class MotionPathExample final {
 public:
  MotionPathExample()
      : path_(flight::path::create_path()),
        point_(flight::geometry::create_vector2()),
        tangent_(flight::geometry::create_vector2()) {
    flight::path::append_path_move_to(path_, 100.0, 400.0);
    flight::path::append_path_cubic_curve_to(path_, 250.0, 100.0, 350.0, 100.0, 400.0, 250.0);
    flight::path::append_path_cubic_curve_to(path_, 450.0, 400.0, 550.0, 400.0, 700.0, 100.0);
    motion_path_ = flight::motionpath::create_motion_path(path_, speed_, mode_);
  }

  void frame(const Frame& frame) {
    if (frame.smoke && frame.index == 0) {
      flight::motionpath::set_motion_path_progress(motion_path_, 0.42);
    }
    handle_keys(frame);
    flight::motionpath::update_motion_path(motion_path_, frame.delta_time);
    const bool sampled = flight::motionpath::get_motion_path_position(motion_path_, point_, tangent_);

    frame.canvas.cubic_curve(
        {100.0, 400.0},
        {250.0, 100.0},
        {350.0, 100.0},
        {400.0, 250.0},
        0x4488aaffU,
        2.0);
    frame.canvas.cubic_curve(
        {400.0, 250.0},
        {450.0, 400.0},
        {550.0, 400.0},
        {700.0, 100.0},
        0x4488aaffU,
        2.0);
    constexpr std::array<Point, 7> controls{{
        {100.0, 400.0},
        {250.0, 100.0},
        {350.0, 100.0},
        {400.0, 250.0},
        {450.0, 400.0},
        {550.0, 400.0},
        {700.0, 100.0},
    }};
    for (const auto& control : controls) {
      constexpr std::array<Point, 4> diamond{{{0.0, -4.0}, {4.0, 0.0}, {0.0, 4.0}, {-4.0, 0.0}}};
      std::array<Point, 4> translated{};
      for (std::size_t index = 0; index < diamond.size(); ++index) {
        translated[index] = {control.x + diamond[index].x, control.y + diamond[index].y};
      }
      frame.canvas.fill_polygon(translated, 0x335566ffU);
    }
    if (sampled) draw_arrow(frame);

    const int progress = static_cast<int>(flight::motionpath::get_motion_path_progress(motion_path_) * 100.0);
    frame.canvas.text("Motion Path", 20.0, 10.0, 0xffffffffU, 24.0);
    frame.canvas.text("Speed: " + std::to_string(static_cast<int>(speed_)), 20.0, 445.0, 0xccccccffU, 16.0);
    frame.canvas.text("Mode: " + mode_.to_utf8() + " (1/2/3)", 20.0, 465.0, 0xccccccffU, 16.0);
    frame.canvas.text("Progress: " + std::to_string(progress) + "%", 250.0, 445.0, 0xccccccffU, 16.0);
    frame.canvas.text("+/- speed    1 clamp  2 loop  3 pingpong", 250.0, 465.0, 0x888888ffU, 14.0);
  }

 private:
  void handle_keys(const Frame& frame) {
    if (frame.input.key_pressed(SDLK_PLUS) || frame.input.key_pressed(SDLK_EQUALS)) {
      speed_ = std::min(speed_ + 25.0, 500.0);
      motion_path_->speed = speed_;
    }
    if (frame.input.key_pressed(SDLK_MINUS)) {
      speed_ = std::max(speed_ - 25.0, 25.0);
      motion_path_->speed = speed_;
    }
    if (frame.input.key_pressed(SDLK_1)) set_mode("clamp");
    if (frame.input.key_pressed(SDLK_2)) set_mode("loop");
    if (frame.input.key_pressed(SDLK_3)) set_mode("pingpong");
  }

  void set_mode(std::string_view mode) {
    mode_ = flight::String(mode);
    motion_path_->loop_mode = mode_;
  }

  void draw_arrow(const Frame& frame) const {
    constexpr double length = 20.0;
    constexpr double half_width = 8.0;
    const double heading = flight::motionpath::get_motion_path_heading(motion_path_);
    const double cosine = std::cos(heading);
    const double sine = std::sin(heading);
    constexpr std::array<Point, 3> local{{{length, 0.0}, {-length / 2.0, -half_width}, {-length / 2.0, half_width}}};
    std::array<Point, 3> world{};
    for (std::size_t index = 0; index < local.size(); ++index) {
      world[index] = {
          point_->x + local[index].x * cosine - local[index].y * sine,
          point_->y + local[index].x * sine + local[index].y * cosine,
      };
    }
    frame.canvas.fill_polygon(world, 0xff6644ffU);
  }

  flight::Ref<flight::types::Path> path_;
  double speed_{150.0};
  flight::types::MotionPathLoopMode mode_{"loop"};
  flight::Ref<flight::types::MotionPath> motion_path_;
  flight::Ref<flight::types::Vector2> point_;
  flight::Ref<flight::types::Vector2> tangent_;
};

} // namespace

int main(int argc, char** argv) {
  try {
    MotionPathExample example;
    return flight::examples::SdlExample::run(
        argc,
        argv,
        "Flight motion path - SDL",
        [&](const Frame& frame) { example.frame(frame); });
  } catch (const std::exception& error) {
    std::cerr << "motionpath example failed: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
