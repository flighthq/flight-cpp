// Naive C++ port of @flighthq/example-motionpath
// Ported from .dependencies/flight/examples/packages/motionpath/src/app.ts

#include <flight/math/constants.hpp>
#include <flight/motionpath/motion_path.hpp>
#include <flight/node/hierarchy.hpp>
#include <flight/node/node_transform2d.hpp>
#include <flight/path/path.hpp>
#include <flight/scene2d/display_object.hpp>
#include <flight/shape/shape.hpp>
#include <flight/shape/shape_commands.hpp>
#include <flight/text/text_label.hpp>

#include <array>
#include <cmath>
#include <iostream>

int main() {
  using namespace flight::node;
  using namespace flight::shape;
  using namespace flight::scene2d;
  using namespace flight::motionpath;

  auto root = create_display_object(std::nullopt);

  // Build a bezier path: S-curve across the canvas.
  auto path = flight::path::create_path();
  flight::path::append_path_move_to(path, 100.0, 400.0);
  flight::path::append_path_cubic_curve_to(path, 250.0, 100.0, 350.0, 100.0, 400.0, 250.0);
  flight::path::append_path_cubic_curve_to(path, 450.0, 400.0, 550.0, 400.0, 700.0, 100.0);

  // Create motion path driver.
  double speed = 150.0;
  auto mp = create_motion_path(path, speed, flight::String("loop"));

  // Draw visible track as a shape.
  auto track = create_shape(std::nullopt);
  append_shape_line_style(track, 2.0, 0x4488aaff);
  append_shape_move_to(track, 100.0, 400.0);
  append_shape_cubic_curve_to(track, 250.0, 100.0, 350.0, 100.0, 400.0, 250.0);
  append_shape_cubic_curve_to(track, 450.0, 400.0, 550.0, 400.0, 700.0, 100.0);
  add_node_child(root, track);

  // Control point markers.
  constexpr std::array<std::array<double, 2>, 8> controlPoints = {{
    {100.0, 400.0}, {250.0, 100.0}, {350.0, 100.0}, {400.0, 250.0},
    {450.0, 400.0}, {550.0, 400.0}, {700.0, 100.0},
  }};

  for (auto& cp : controlPoints) {
    auto dot = create_shape(std::nullopt);
    append_shape_begin_fill(dot, 0x666666ff);
    append_shape_circle(dot, cp[0], cp[1], 4.0);
    append_shape_end_fill(dot);
    add_node_child(root, dot);
  }

  // Moving object (triangle arrow).
  auto arrow = create_shape(std::nullopt);
  append_shape_begin_fill(arrow, 0xff6644ff);
  append_shape_move_to(arrow, 12.0, 0.0);
  append_shape_line_to(arrow, -8.0, -7.0);
  append_shape_line_to(arrow, -8.0, 7.0);
  append_shape_line_to(arrow, 12.0, 0.0);
  append_shape_end_fill(arrow);
  add_node_child(root, arrow);

  // HUD label.
  auto label = flight::text::create_text_label();
  label->data.text = flight::String("MOTION PATH");
  label->x = 20.0;
  label->y = 20.0;
  invalidate_node_local_transform(label);
  add_node_child(root, label);

  // Simulate motion along the path.
  for (int frame = 0; frame < 180; ++frame) {
    double dt = 1.0 / 60.0;
    update_motion_path(mp, dt);

    auto pos = get_motion_path_position(mp);
    auto heading = get_motion_path_heading(mp);
    auto progress = get_motion_path_progress(mp);

    arrow->x = pos->x;
    arrow->y = pos->y;
    arrow->rotation = heading * flight::math::RAD_TO_DEG;
    invalidate_node_local_transform(arrow);
  }

  auto finalProgress = get_motion_path_progress(mp);
  auto finalPos = get_motion_path_position(mp);

  std::cout << "Flight motionpath example (naive C++ port): "
            << "progress=" << finalProgress << ", "
            << "position=(" << finalPos->x << ", " << finalPos->y << ")\n";
  return 0;
}
