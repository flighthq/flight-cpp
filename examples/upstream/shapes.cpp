// Native SDL port of @flighthq/example-shapes.
// Source: .dependencies/flight/examples/packages/shapes/src/app.ts

#include "sdl_host.hpp"

#include <flight/node/hierarchy.hpp>
#include <flight/scene2d/display_object.hpp>
#include <flight/shape/shape.hpp>
#include <flight/shape/shape_commands.hpp>

#include <array>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <numbers>
#include <vector>

namespace {

using flight::examples::Canvas;
using flight::examples::Frame;
using flight::examples::Point;

constexpr double row_height = 110.0;
constexpr double row_padding = 10.0;
constexpr double column_start = 20.0;
constexpr std::uint32_t fill_color = 0x4488ccff;
constexpr std::uint32_t stroke_color = 0x88ccffff;

[[nodiscard]] constexpr double row_y(int row) {
  return row_padding + static_cast<double>(row) * (row_height + row_padding);
}

[[nodiscard]] std::vector<Point> regular_polygon(
    double center_x,
    double center_y,
    double radius,
    int sides) {
  std::vector<Point> points;
  points.reserve(static_cast<std::size_t>(sides));
  for (int index = 0; index < sides; ++index) {
    const double angle = -std::numbers::pi / 2.0 +
                         static_cast<double>(index) * 2.0 * std::numbers::pi /
                             static_cast<double>(sides);
    points.push_back({
        center_x + std::cos(angle) * radius,
        center_y + std::sin(angle) * radius,
    });
  }
  return points;
}

[[nodiscard]] std::vector<Point> star(
    double center_x,
    double center_y,
    double outer_radius,
    double inner_radius,
    int count) {
  std::vector<Point> points;
  points.reserve(static_cast<std::size_t>(count * 2));
  for (int index = 0; index < count * 2; ++index) {
    const double angle = -std::numbers::pi / 2.0 +
                         static_cast<double>(index) * std::numbers::pi /
                             static_cast<double>(count);
    const double radius = index % 2 == 0 ? outer_radius : inner_radius;
    points.push_back({
        center_x + std::cos(angle) * radius,
        center_y + std::sin(angle) * radius,
    });
  }
  return points;
}

class ShapesExample final {
 public:
  ShapesExample()
      : root_(flight::scene2d::create_display_object(std::nullopt)),
        interactive_star_(flight::shape::create_shape(std::nullopt)) {
    flight::row_set<flight::RowKey<"scaleX">>(root_, 1.0);
    flight::row_set<flight::RowKey<"scaleY">>(root_, 1.0);
    flight::node::add_node_child(root_, interactive_star_);
    rebuild_interactive_commands();
  }

  void frame(const Frame& frame) {
    constexpr double size = 80.0;
    constexpr double star_x = column_start + 650.0;
    constexpr double star_y = row_y(3) + 15.0;
    const bool hovered = flight::examples::contains(
        star_x,
        star_y,
        size,
        size,
        frame.input.pointer_x,
        frame.input.pointer_y);
    const bool pressed = hovered && frame.input.pointer_down;
    if (hovered != hovered_ || pressed != pressed_) {
      hovered_ = hovered;
      pressed_ = pressed;
      rebuild_interactive_commands();
    }

    draw_basic_primitives(frame.canvas);
    draw_polygons(frame.canvas);
    draw_curves(frame.canvas);
    draw_gradients_and_interaction(frame.canvas);
    draw_strokes(frame.canvas);
  }

 private:
  void rebuild_interactive_commands() {
    const std::uint32_t color = pressed_ ? 0x00f5d4ff : hovered_ ? 0xfee440ff : 0xf15bb5ff;
    flight::shape::clear_shape_commands(interactive_star_);
    flight::shape::append_shape_begin_fill(interactive_star_, color);
    flight::shape::append_shape_line_style(interactive_star_, pressed_ ? 7.0 : 3.0, 0xffffffff);
    const auto points = star(40.0, 40.0, 40.0, 16.0, 5);
    flight::shape::append_shape_move_to(interactive_star_, points.front().x, points.front().y);
    for (std::size_t index = 1; index < points.size(); ++index) {
      flight::shape::append_shape_line_to(interactive_star_, points[index].x, points[index].y);
    }
    flight::shape::append_shape_line_to(interactive_star_, points.front().x, points.front().y);
    flight::shape::append_shape_end_fill(interactive_star_);
  }

  static void draw_basic_primitives(Canvas& canvas) {
    const double y = row_y(0);
    canvas.fill_rectangle(column_start, y + 15.0, 120.0, 80.0, fill_color);
    canvas.fill_circle(column_start + 210.0, y + 55.0, 50.0, fill_color);
    canvas.fill_ellipse(column_start + 370.0, y + 55.0, 70.0, 45.0, fill_color);
    canvas.fill_rounded_rectangle(column_start + 500.0, y + 10.0, 140.0, 90.0, 10.0, fill_color);
  }

  static void draw_polygons(Canvas& canvas) {
    constexpr double size = 45.0;
    const double y = row_y(1);
    canvas.fill_polygon(regular_polygon(column_start + size, y + size + 5.0, size, 3), fill_color);
    canvas.fill_polygon(regular_polygon(column_start + 140.0 + size, y + size + 5.0, size, 5), fill_color);
    canvas.fill_polygon(regular_polygon(column_start + 280.0 + size, y + size + 5.0, size, 6), fill_color);
    canvas.fill_polygon(star(column_start + 420.0 + size, y + size + 5.0, size, size * 0.4, 5), fill_color);
  }

  static void draw_curves(Canvas& canvas) {
    constexpr double width = 200.0;
    constexpr double height = 80.0;
    const double y = row_y(2);
    canvas.line(column_start, y + 15.0 + height / 2.0, column_start + width, y + 15.0 + height / 2.0, stroke_color, 3.0);
    canvas.quadratic_curve(
        {column_start + 240.0, y + 10.0 + height},
        {column_start + 240.0 + width / 2.0, y + 10.0 - height * 0.4},
        {column_start + 240.0 + width, y + 10.0 + height},
        stroke_color,
        3.0);
    canvas.cubic_curve(
        {column_start + 500.0, y + 10.0 + height},
        {column_start + 500.0 + width * 0.33, y + 10.0 - height * 0.3},
        {column_start + 500.0 + width * 0.66, y + 10.0 + height * 1.3},
        {column_start + 500.0 + width, y + 10.0},
        stroke_color,
        3.0);
  }

  void draw_gradients_and_interaction(Canvas& canvas) const {
    constexpr double size = 80.0;
    const double y = row_y(3) + 15.0;
    constexpr std::array<std::uint32_t, 3> linear_colors{0xff4d6dffU, 0xffc300ffU, 0x00bbf9ffU};
    canvas.horizontal_gradient(column_start, y, 120.0, size, linear_colors);

    for (int ring = 50; ring > 0; --ring) {
      const double amount = 1.0 - static_cast<double>(ring) / 50.0;
      const std::uint32_t color = amount < 0.55
                                      ? flight::examples::lerp_color(0x007200ff, 0x70e000ff, amount / 0.55)
                                      : flight::examples::lerp_color(0x70e000ff, 0xffffffff, (amount - 0.55) / 0.45);
      canvas.fill_ellipse(column_start + 210.0, y + size / 2.0, static_cast<double>(ring), static_cast<double>(ring) * 0.8, color);
    }

    std::array<Point, 65> gradient_curve{};
    for (std::size_t index = 0; index < gradient_curve.size(); ++index) {
      const double amount = static_cast<double>(index) / static_cast<double>(gradient_curve.size() - 1);
      const double inverse = 1.0 - amount;
      gradient_curve[index] = {
          column_start + 300.0 + 3.0 * inverse * inverse * amount * 55.0 +
              3.0 * inverse * amount * amount * 145.0 + amount * amount * amount * 210.0,
          y + inverse * inverse * inverse * 65.0 + 3.0 * inverse * inverse * amount * -15.0 +
              3.0 * inverse * amount * amount * 95.0 + amount * amount * amount * 15.0,
      };
    }
    for (std::size_t index = 1; index < gradient_curve.size(); ++index) {
      constexpr std::array<std::uint32_t, 4> colors{
          0x9b5de5ffU,
          0xf15bb5ffU,
          0xfee440ffU,
          0x00f5d4ffU,
      };
      const double amount = static_cast<double>(index) / static_cast<double>(gradient_curve.size() - 1);
      const double scaled = amount * static_cast<double>(colors.size() - 1);
      const auto left = static_cast<std::size_t>(std::floor(scaled));
      const auto right = std::min(left + 1, colors.size() - 1);
      canvas.line(
          gradient_curve[index - 1].x,
          gradient_curve[index - 1].y,
          gradient_curve[index].x,
          gradient_curve[index].y,
          flight::examples::lerp_color(colors[left], colors[right], scaled - static_cast<double>(left)),
          10.0);
    }

    const std::uint32_t interactive_color = pressed_ ? 0x00f5d4ff : hovered_ ? 0xfee440ff : 0xf15bb5ff;
    const auto points = star(column_start + 650.0 + 40.0, y + 40.0, 40.0, 16.0, 5);
    canvas.fill_polygon(points, interactive_color);
    canvas.polyline(points, 0xffffffff, pressed_ ? 7.0 : 3.0, true);
  }

  static void draw_strokes(Canvas& canvas) {
    const double y = row_y(4);
    constexpr double length = 160.0;
    canvas.line(column_start, y + 20.0, column_start + length, y + 20.0, stroke_color, 1.0);
    canvas.line(column_start, y + 50.0, column_start + length, y + 50.0, stroke_color, 4.0);
    canvas.line(column_start, y + 80.0, column_start + length, y + 80.0, stroke_color, 10.0);

    const double caps_x = column_start + 230.0;
    canvas.line(caps_x, y + 20.0, caps_x + length - 20.0, y + 20.0, 0xcc8844ff, 10.0);
    canvas.line(caps_x, y + 50.0, caps_x + length - 20.0, y + 50.0, 0x44cc88ff, 10.0);
    canvas.fill_circle(caps_x, y + 50.0, 5.0, 0x44cc88ff);
    canvas.fill_circle(caps_x + length - 20.0, y + 50.0, 5.0, 0x44cc88ff);
    canvas.line(caps_x - 5.0, y + 80.0, caps_x + length - 15.0, y + 80.0, 0x8844ccff, 10.0);

    constexpr std::array<double, 3> joins_x{column_start + 440.0, column_start + 550.0, column_start + 660.0};
    constexpr std::array<std::uint32_t, 3> colors{0xcc8844ff, 0x44cc88ff, 0x8844ccff};
    for (std::size_t index = 0; index < joins_x.size(); ++index) {
      const std::array<Point, 3> points{{
          {joins_x[index], y + 90.0},
          {joins_x[index] + 40.0, y + 20.0},
          {joins_x[index] + 80.0, y + 90.0},
      }};
      canvas.polyline(points, colors[index], 6.0);
      if (index == 1) canvas.fill_circle(points[1].x, points[1].y, 3.0, colors[index]);
    }
  }

  flight::types::DisplayObject root_;
  flight::Ref<flight::types::Shape> interactive_star_;
  bool hovered_{};
  bool pressed_{};
};

} // namespace

int main(int argc, char** argv) {
  try {
    ShapesExample example;
    return flight::examples::SdlExample::run(
        argc,
        argv,
        "Flight shapes - SDL",
        [&](const Frame& frame) { example.frame(frame); });
  } catch (const std::exception& error) {
    std::cerr << "shapes example failed: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
