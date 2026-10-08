// Native SDL port of @flighthq/example-interaction.
// Source: .dependencies/flight/examples/packages/interaction/src/app.ts

#include "sdl_host.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>

namespace {

using flight::examples::Canvas;
using flight::examples::Frame;
using flight::examples::Point;

enum class Kind { rectangle, circle };

struct Draggable final {
  std::string name;
  std::uint32_t base_color{};
  std::uint32_t hover_color{};
  Kind kind{};
  double x{};
  double y{};
  double width{};
  double height{};
};

[[nodiscard]] bool contains(const Draggable& shape, Point point) {
  if (shape.kind == Kind::circle) {
    return std::hypot(point.x - shape.x, point.y - shape.y) <= shape.width;
  }
  return flight::examples::contains(
      shape.x - shape.width / 2.0,
      shape.y - shape.height / 2.0,
      shape.width,
      shape.height,
      point.x,
      point.y);
}

[[nodiscard]] bool overlap(const Draggable& a, const Draggable& b) {
  if (a.kind == Kind::circle && b.kind == Kind::circle) {
    return std::hypot(a.x - b.x, a.y - b.y) <= a.width + b.width;
  }
  if (a.kind == Kind::rectangle && b.kind == Kind::rectangle) {
    return std::abs(a.x - b.x) * 2.0 <= a.width + b.width &&
           std::abs(a.y - b.y) * 2.0 <= a.height + b.height;
  }
  const Draggable& circle = a.kind == Kind::circle ? a : b;
  const Draggable& rectangle = a.kind == Kind::rectangle ? a : b;
  const double closest_x = std::clamp(
      circle.x,
      rectangle.x - rectangle.width / 2.0,
      rectangle.x + rectangle.width / 2.0);
  const double closest_y = std::clamp(
      circle.y,
      rectangle.y - rectangle.height / 2.0,
      rectangle.y + rectangle.height / 2.0);
  return std::hypot(circle.x - closest_x, circle.y - closest_y) <= circle.width;
}

class InteractionExample final {
 public:
  void frame(const Frame& frame) {
    const Point pointer{frame.input.pointer_x, frame.input.pointer_y};
    const std::size_t previous_hovered = hovered_;
    hovered_ = shapes_.size();
    for (std::size_t offset = 0; offset < shapes_.size(); ++offset) {
      const std::size_t index = shapes_.size() - 1 - offset;
      if (frame.input.pointer_inside && contains(shapes_[index], pointer)) {
        hovered_ = index;
        break;
      }
    }
    if (hovered_ != previous_hovered) {
      last_event_ = hovered_ < shapes_.size() ? "pointerOver" : "pointerOut";
    }

    if (frame.input.pointer_pressed && hovered_ < shapes_.size()) {
      dragged_ = hovered_;
      drag_offset_ = {pointer.x - shapes_[dragged_].x, pointer.y - shapes_[dragged_].y};
      last_event_ = "pointerDown";
    }
    const bool moved = pointer.x != last_pointer_.x || pointer.y != last_pointer_.y;
    if (dragged_ < shapes_.size() && frame.input.pointer_down && moved) {
      shapes_[dragged_].x = pointer.x - drag_offset_.x;
      shapes_[dragged_].y = pointer.y - drag_offset_.y;
      last_event_ = "pointerMove";
    }
    if (frame.input.pointer_released && dragged_ < shapes_.size()) {
      dragged_ = shapes_.size();
      last_event_ = "pointerUp";
    }
    last_pointer_ = pointer;
    draw(frame.canvas);
  }

 private:
  void draw(Canvas& canvas) const {
    canvas.text(
        "Drag shapes. Hover for precise hit + grab cursor. Overlaps update live.",
        10.0,
        10.0,
        0x999999ffU,
        16.0);
    for (std::size_t index = 0; index < shapes_.size(); ++index) {
      const auto& shape = shapes_[index];
      const std::uint32_t color = index == hovered_ ? shape.hover_color : shape.base_color;
      if (shape.kind == Kind::circle) {
        canvas.fill_circle(shape.x, shape.y, shape.width, color, 0.85);
      } else {
        canvas.fill_rectangle(
            shape.x - shape.width / 2.0,
            shape.y - shape.height / 2.0,
            shape.width,
            shape.height,
            color,
            0.85);
      }
    }

    std::string overlaps;
    for (std::size_t first = 0; first < shapes_.size(); ++first) {
      for (std::size_t second = first + 1; second < shapes_.size(); ++second) {
        if (overlap(shapes_[first], shapes_[second])) {
          if (!overlaps.empty()) overlaps += ", ";
          overlaps += shapes_[first].name + " + " + shapes_[second].name;
        }
      }
    }
    if (overlaps.empty()) overlaps = "none";
    const std::string hovered = hovered_ < shapes_.size() ? shapes_[hovered_].name : "none";
    const std::string drag = dragged_ < shapes_.size() ? "dragging " + shapes_[dragged_].name : "idle";
    canvas.text("Overlaps: " + overlaps, 10.0, 498.0, 0xddddddffU, 11.0);
    canvas.text("Event: " + last_event_, 10.0, 520.0, 0xddddddffU, 14.0);
    canvas.text("Hovered: " + hovered, 10.0, 542.0, 0xddddddffU, 14.0);
    canvas.text("Drag: " + drag, 10.0, 564.0, 0xddddddffU, 14.0);
  }

  std::array<Draggable, 6> shapes_{{
      {"Red Rect", 0xcc4444ffU, 0xff6666ffU, Kind::rectangle, 100.0, 100.0, 100.0, 80.0},
      {"Green Circle", 0x44cc44ffU, 0x66ff66ffU, Kind::circle, 300.0, 150.0, 60.0, 60.0},
      {"Blue Rect", 0x4444ccffU, 0x6666ffffU, Kind::rectangle, 500.0, 100.0, 120.0, 90.0},
      {"Yellow Circle", 0xcccc44ffU, 0xffff66ffU, Kind::circle, 200.0, 350.0, 50.0, 50.0},
      {"Cyan Rect", 0x44ccccffU, 0x66ffffffU, Kind::rectangle, 450.0, 300.0, 90.0, 70.0},
      {"Magenta Circle", 0xcc44ccffU, 0xff66ffffU, Kind::circle, 475.0, 300.0, 55.0, 55.0},
  }};
  std::size_t hovered_{shapes_.size()};
  std::size_t dragged_{shapes_.size()};
  Point drag_offset_{};
  Point last_pointer_{};
  std::string last_event_{"none"};
};

} // namespace

int main(int argc, char** argv) {
  try {
    InteractionExample example;
    return flight::examples::SdlExample::run(
        argc,
        argv,
        "Flight interaction - SDL",
        [&](const Frame& frame) { example.frame(frame); });
  } catch (const std::exception& error) {
    std::cerr << "interaction example failed: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
