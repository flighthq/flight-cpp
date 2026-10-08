// Naive C++ port of @flighthq/example-clock
// Ported from .dependencies/flight/examples/packages/clock/src/app.ts

#include <flight/clock/clock.hpp>
#include <flight/node/hierarchy.hpp>
#include <flight/node/node_transform2d.hpp>
#include <flight/scene2d/display_object.hpp>
#include <flight/shape/shape.hpp>
#include <flight/shape/shape_commands.hpp>
#include <flight/text/text_label.hpp>

#include <array>
#include <cmath>
#include <iostream>

namespace {

constexpr double CANVAS_WIDTH = 800.0;
constexpr double CANVAS_HEIGHT = 600.0;
constexpr double M_TAU = 2.0 * M_PI;

} // namespace

int main() {
  using namespace flight::clock;
  using namespace flight::node;
  using namespace flight::shape;
  using namespace flight::scene2d;

  auto root = create_display_object(std::nullopt);

  auto parentClock = create_clock();
  auto childClock1 = create_child_clock(parentClock, 0.5);
  auto childClock2 = create_child_clock(parentClock, 2.0);
  auto pausedClock = create_child_clock(parentClock, 1.0);
  pause_clock(pausedClock);

  struct ClockDisplay {
    flight::Ref<flight::types::Clock> clock;
    double cx;
    double cy;
    double radius;
    std::string_view label;
    std::uint32_t color;
  };

  std::array<ClockDisplay, 4> clocks{{
    {parentClock, 200.0, 200.0, 80.0, "Parent (1x)", 0x4de0ffff},
    {childClock1, 500.0, 200.0, 60.0, "Child (0.5x)", 0x9877ffff},
    {childClock2, 200.0, 450.0, 60.0, "Child (2x)", 0xff78c8ff},
    {pausedClock, 500.0, 450.0, 60.0, "Paused", 0x999999ff},
  }};

  for (auto& cd : clocks) {
    auto face = create_shape(std::nullopt);
    append_shape_line_style(face, 2.0, cd.color);
    append_shape_circle(face, cd.cx, cd.cy, cd.radius);
    add_node_child(root, face);

    for (int tick = 0; tick < 12; ++tick) {
      double angle = (static_cast<double>(tick) / 12.0) * M_TAU - M_PI / 2.0;
      double innerR = cd.radius * 0.85;
      auto tickMark = create_shape(std::nullopt);
      append_shape_line_style(tickMark, 1.5, cd.color);
      append_shape_move_to(tickMark,
        cd.cx + innerR * std::cos(angle),
        cd.cy + innerR * std::sin(angle));
      append_shape_line_to(tickMark,
        cd.cx + cd.radius * std::cos(angle),
        cd.cy + cd.radius * std::sin(angle));
      add_node_child(root, tickMark);
    }

    auto hand = create_shape(std::nullopt);
    append_shape_line_style(hand, 3.0, cd.color);
    append_shape_move_to(hand, cd.cx, cd.cy);
    append_shape_line_to(hand, cd.cx, cd.cy - cd.radius * 0.7);
    add_node_child(root, hand);

    auto label = flight::text::create_text_label();
    label->data.text = flight::String(cd.label);
    label->x = cd.cx - 40.0;
    label->y = cd.cy + cd.radius + 15.0;
    invalidate_node_local_transform(label);
    add_node_child(root, label);
  }

  advance_clock(parentClock, 2500.0);
  advance_clock(parentClock, 1000.0);

  resume_clock(pausedClock);
  advance_clock(parentClock, 500.0);

  std::cout << "Flight clock example (naive C++ port): "
            << clocks.size() << " clocks created.\n";
  return 0;
}
