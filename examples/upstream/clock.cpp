// Native SDL port of @flighthq/example-clock.
// Source: .dependencies/flight/examples/packages/clock/src/app.ts

#include "sdl_host.hpp"

#include <flight/clock/clock.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <iomanip>
#include <iostream>
#include <numbers>
#include <sstream>
#include <string>

namespace {

using flight::examples::Frame;
using flight::examples::Point;

[[nodiscard]] std::string fixed(double value, int precision) {
  std::ostringstream stream;
  stream << std::fixed << std::setprecision(precision) << value;
  return stream.str();
}

[[nodiscard]] Point rotate(Point point, double center_x, double center_y, double degrees) {
  const double radians = degrees * std::numbers::pi / 180.0;
  const double cosine = std::cos(radians);
  const double sine = std::sin(radians);
  return {
      center_x + point.x * cosine - point.y * sine,
      center_y + point.x * sine + point.y * cosine,
  };
}

class ClockExample final {
 public:
  ClockExample()
      : root_(flight::clock::create_clock()),
        child_a_(flight::clock::create_child_clock(root_)),
        child_b_(flight::clock::create_child_clock(root_)) {
    flight::clock::set_clock_scale(child_a_, 1.0);
    flight::clock::set_clock_scale(child_b_, 0.5);
  }

  void frame(const Frame& frame) {
    if (frame.smoke && frame.index == 0) flight::clock::pause_clock(child_b_);
    handle_keys(frame);
    flight::clock::advance_clock(root_, frame.delta_time * 1000.0);
    frame.canvas.text("Hierarchical Clocks", 20.0, 10.0, 0xffffffffU, 24.0);
    frame.canvas.line(400.0, 150.0, 220.0, 260.0, 0x555555ffU, 2.0);
    frame.canvas.line(400.0, 150.0, 580.0, 260.0, 0x555555ffU, 2.0);
    draw_root(frame);
    draw_child_a(frame);
    draw_child_b(frame);
    frame.canvas.text(
        "P root pause    1/2 toggle child pause    Left/Right child A scale    Up/Down child B scale",
        20.0,
        470.0,
        0x888888ffU,
        10.0);
  }

 private:
  static void toggle(const flight::Ref<flight::types::Clock>& clock) {
    if (clock->paused) {
      flight::clock::resume_clock(clock);
    } else {
      flight::clock::pause_clock(clock);
    }
  }

  void handle_keys(const Frame& frame) {
    if (frame.input.key_pressed(SDLK_P)) toggle(root_);
    if (frame.input.key_pressed(SDLK_1)) toggle(child_a_);
    if (frame.input.key_pressed(SDLK_2)) toggle(child_b_);
    if (frame.input.key_pressed(SDLK_LEFT)) {
      flight::clock::set_clock_scale(child_a_, std::max(child_a_->scale - 0.25, 0.0));
    }
    if (frame.input.key_pressed(SDLK_RIGHT)) {
      flight::clock::set_clock_scale(child_a_, std::min(child_a_->scale + 0.25, 4.0));
    }
    if (frame.input.key_pressed(SDLK_DOWN)) {
      flight::clock::set_clock_scale(child_b_, std::max(child_b_->scale - 0.25, 0.0));
    }
    if (frame.input.key_pressed(SDLK_UP)) {
      flight::clock::set_clock_scale(child_b_, std::min(child_b_->scale + 0.25, 4.0));
    }
  }

  void draw_root(const Frame& frame) const {
    constexpr double x = 400.0;
    constexpr double y = 120.0;
    const double rotation = root_->elapsed * 0.09;
    std::array<Point, 4> points{{
        rotate({-30.0, -30.0}, x, y, rotation),
        rotate({30.0, -30.0}, x, y, rotation),
        rotate({30.0, 30.0}, x, y, rotation),
        rotate({-30.0, 30.0}, x, y, rotation),
    }};
    frame.canvas.fill_polygon(points, 0x4488ccffU);
    frame.canvas.text("Root Clock", x - 40.0, y - 55.0, 0xaaaaaaffU, 14.0);
    frame.canvas.text(
        "elapsed " + fixed(root_->elapsed / 1000.0, 1) + "s  dt " + fixed(root_->delta_time, 1) +
            "  scale " + fixed(root_->scale, 2),
        x - 80.0,
        y + 40.0,
        0x8888aaffU,
        10.0);
    if (root_->paused) frame.canvas.text("PAUSED", x - 25.0, y - 8.0, 0xff4444ffU, 14.0);
  }

  void draw_child_a(const Frame& frame) const {
    constexpr double x = 220.0;
    constexpr double y = 300.0;
    constexpr double size = 50.0;
    constexpr double half = size / 2.0;
    constexpr double height = size * 0.8660254037844386;
    const double rotation = child_a_->elapsed * 0.09;
    std::array<Point, 3> points{{
        rotate({0.0, -height / 2.0}, x, y, rotation),
        rotate({half, height / 2.0}, x, y, rotation),
        rotate({-half, height / 2.0}, x, y, rotation),
    }};
    frame.canvas.fill_polygon(points, 0x44cc88ffU);
    frame.canvas.text("Child A (" + fixed(child_a_->scale, 2) + "x)", x - 45.0, y - 55.0, 0xaaaaaaffU, 14.0);
    frame.canvas.text(
        "elapsed " + fixed(child_a_->elapsed / 1000.0, 1) + "s  dt " + fixed(child_a_->delta_time, 1) +
            "  scale " + fixed(child_a_->scale, 2) + "  eff " +
            fixed(flight::clock::get_clock_effective_scale(child_a_), 2),
        x - 80.0,
        y + 45.0,
        0x88aa88ffU,
        9.0);
    if (flight::clock::is_clock_effectively_paused(child_a_)) {
      frame.canvas.text("PAUSED", x - 25.0, y - 8.0, 0xff4444ffU, 14.0);
    }
  }

  void draw_child_b(const Frame& frame) const {
    constexpr double x = 580.0;
    constexpr double y = 300.0;
    constexpr double radius = 30.0;
    const double rotation = child_b_->elapsed * 0.09 * std::numbers::pi / 180.0;
    frame.canvas.fill_circle(x, y, radius, 0xcc8844ffU);
    frame.canvas.line(x, y, x + std::cos(rotation) * radius, y + std::sin(rotation) * radius, 0xffffffffU, 3.0);
    frame.canvas.text("Child B (" + fixed(child_b_->scale, 2) + "x)", x - 50.0, y - 55.0, 0xaaaaaaffU, 14.0);
    frame.canvas.text(
        "elapsed " + fixed(child_b_->elapsed / 1000.0, 1) + "s  dt " + fixed(child_b_->delta_time, 1) +
            "  scale " + fixed(child_b_->scale, 2) + "  eff " +
            fixed(flight::clock::get_clock_effective_scale(child_b_), 2),
        x - 80.0,
        y + 45.0,
        0xaa8844ffU,
        9.0);
    if (flight::clock::is_clock_effectively_paused(child_b_)) {
      frame.canvas.text("PAUSED", x - 25.0, y - 8.0, 0xff4444ffU, 14.0);
    }
  }

  flight::Ref<flight::types::Clock> root_;
  flight::Ref<flight::types::Clock> child_a_;
  flight::Ref<flight::types::Clock> child_b_;
};

} // namespace

int main(int argc, char** argv) {
  try {
    ClockExample example;
    return flight::examples::SdlExample::run(
        argc,
        argv,
        "Flight hierarchical clocks - SDL",
        [&](const Frame& frame) { example.frame(frame); });
  } catch (const std::exception& error) {
    std::cerr << "clock example failed: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
