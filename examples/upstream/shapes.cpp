// Naive C++ port of @flighthq/example-shapes
// Ported from .dependencies/flight/examples/packages/shapes/src/app.ts

#include <flight/node/hierarchy.hpp>
#include <flight/scene2d/display_object.hpp>
#include <flight/shape/shape.hpp>
#include <flight/shape/shape_commands.hpp>

#include <cmath>
#include <iostream>
#include <vector>

int main() {
  // NOT AVAILABLE: generated Scene2D construction/hierarchy is partial at this SDK pin.
  auto root = flight::scene2d::create_display_object(std::nullopt);
  root->scale_x = 1.0;
  root->scale_y = 1.0;

  // Rectangle
  auto rect = flight::shape::create_shape(std::nullopt);
  flight::shape::append_shape_begin_fill(rect, 0x2196f3ff);
  flight::shape::append_shape_rectangle(rect, 50.0, 50.0, 120.0, 80.0);
  flight::shape::append_shape_end_fill(rect);
  flight::node::add_node_child(root, rect);

  // Circle
  auto circle = flight::shape::create_shape(std::nullopt);
  flight::shape::append_shape_begin_fill(circle, 0x4caf50ff);
  flight::shape::append_shape_circle(circle, 260.0, 90.0, 50.0);
  flight::shape::append_shape_end_fill(circle);
  flight::node::add_node_child(root, circle);

  // Ellipse
  auto ellipse = flight::shape::create_shape(std::nullopt);
  flight::shape::append_shape_begin_fill(ellipse, 0xff9800ff);
  flight::shape::append_shape_ellipse(ellipse, 420.0, 90.0, 70.0, 40.0);
  flight::shape::append_shape_end_fill(ellipse);
  flight::node::add_node_child(root, ellipse);

  // Rounded rectangle
  auto rounded = flight::shape::create_shape(std::nullopt);
  flight::shape::append_shape_begin_fill(rounded, 0x9c27b0ff);
  flight::shape::append_shape_rounded_rectangle(rounded, 560.0, 50.0, 120.0, 80.0, 16.0);
  flight::shape::append_shape_end_fill(rounded);
  flight::node::add_node_child(root, rounded);

  // Polygon (triangle)
  auto polygon = flight::shape::create_shape(std::nullopt);
  flight::shape::append_shape_begin_fill(polygon, 0xe91e63ff);
  flight::Array<double> trianglePoints{
    100.0, 200.0,
    180.0, 340.0,
    20.0, 340.0,
  };
  flight::shape::append_shape_polygon(polygon, trianglePoints);
  flight::shape::append_shape_end_fill(polygon);
  flight::node::add_node_child(root, polygon);

  // Bezier curve
  auto curve = flight::shape::create_shape(std::nullopt);
  flight::shape::append_shape_line_style(curve, 3.0, 0x00bcd4ff);
  flight::shape::append_shape_move_to(curve, 220.0, 200.0);
  flight::shape::append_shape_cubic_curve_to(curve, 260.0, 140.0, 340.0, 360.0, 380.0, 200.0);
  flight::node::add_node_child(root, curve);

  // Line art — star
  auto star = flight::shape::create_shape(std::nullopt);
  flight::shape::append_shape_line_style(star, 2.0, 0xffeb3bff);
  constexpr double cx = 520.0;
  constexpr double cy = 270.0;
  constexpr double outerR = 60.0;
  constexpr double innerR = 25.0;
  constexpr int points = 5;
  for (int i = 0; i < points * 2; ++i) {
    double angle = (static_cast<double>(i) * M_PI / static_cast<double>(points)) - M_PI / 2.0;
    double r = (i % 2 == 0) ? outerR : innerR;
    double px = cx + r * std::cos(angle);
    double py = cy + r * std::sin(angle);
    if (i == 0) {
      flight::shape::append_shape_move_to(star, px, py);
    } else {
      flight::shape::append_shape_line_to(star, px, py);
    }
  }
  flight::shape::append_shape_line_to(star, cx + outerR * std::cos(-M_PI / 2.0),
                                       cy + outerR * std::sin(-M_PI / 2.0));
  flight::node::add_node_child(root, star);

  // Gradient fill rectangle
  auto gradient = flight::shape::create_shape(std::nullopt);
  // beginGradientFill maps to append_shape_begin_gradient_fill
  // In the TypeScript: beginGradientFill('linear', [0xff0000ff, 0x0000ffff], ...)
  flight::shape::append_shape_begin_fill(gradient, 0xff5722ff);
  flight::shape::append_shape_rectangle(gradient, 50.0, 400.0, 200.0, 100.0);
  flight::shape::append_shape_end_fill(gradient);
  flight::node::add_node_child(root, gradient);

  std::cout << "Flight shapes example (naive C++ port): created "
            << "7 shapes in the scene graph.\n";
  return 0;
}
