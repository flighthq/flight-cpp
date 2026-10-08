// Naive C++ port of @flighthq/example-camera2d
// Ported from .dependencies/flight/examples/packages/camera2d/src/app.ts

#include <flight/camera/camera2d.hpp>
#include <flight/camera/view_matrix.hpp>
#include <flight/camera/visible_bounds.hpp>
#include <flight/camera/zoom.hpp>
#include <flight/camera_controls/follow.hpp>
#include <flight/geometry/matrix.hpp>
#include <flight/geometry/rectangle.hpp>
#include <flight/geometry/vector2.hpp>
#include <flight/node/hierarchy.hpp>
#include <flight/node/node_transform2d.hpp>
#include <flight/scene2d/display_object.hpp>
#include <flight/shape/shape.hpp>
#include <flight/shape/shape_commands.hpp>
#include <flight/text/text_label.hpp>

#include <array>
#include <cmath>
#include <iostream>
#include <vector>

namespace {

constexpr double CANVAS_WIDTH = 800.0;
constexpr double CANVAS_HEIGHT = 600.0;
constexpr double WORLD_WIDTH = 2400.0;
constexpr double WORLD_HEIGHT = 1800.0;
constexpr double PLAYER_SIZE = 24.0;
constexpr double PLAYER_SPEED = 300.0;
constexpr double MIN_ZOOM = 0.25;
constexpr double MAX_ZOOM = 4.0;

double seeded_random(int& seed) {
  seed = (seed * 16807 + 0) % 2147483647;
  return static_cast<double>(seed) / 2147483647.0;
}

struct LandmarkData {
  double x, y, width, height;
  std::uint32_t color;
  bool is_circle;
};

} // namespace

int main() {
  using namespace flight::node;
  using namespace flight::shape;
  using namespace flight::scene2d;

  auto worldBounds = flight::geometry::create_rectangle(0.0, 0.0, WORLD_WIDTH, WORLD_HEIGHT);

  auto camera = flight::camera::create_camera2_d(CANVAS_WIDTH, CANVAS_HEIGHT);
  camera->x = WORLD_WIDTH * 0.5;
  camera->y = WORLD_HEIGHT * 0.5;
  camera->zoom = 1.0;

  double playerX = WORLD_WIDTH * 0.5;
  double playerY = WORLD_HEIGHT * 0.5;

  auto root = create_display_object(std::nullopt);

  auto worldContainer = create_display_object(std::nullopt);
  add_node_child(root, worldContainer);

  auto hudContainer = create_display_object(std::nullopt);
  add_node_child(root, hudContainer);

  int seed = 42;
  std::vector<LandmarkData> landmarks;
  for (int i = 0; i < 60; ++i) {
    bool is_circle = seeded_random(seed) > 0.5;
    double size = 20.0 + seeded_random(seed) * 80.0;
    double hue = seeded_random(seed) * 360.0;
    std::uint32_t color = static_cast<std::uint32_t>(hue / 360.0 * 0xffffff) << 8 | 0xff;
    landmarks.push_back({
      100.0 + seeded_random(seed) * (WORLD_WIDTH - 200.0),
      100.0 + seeded_random(seed) * (WORLD_HEIGHT - 200.0),
      size,
      is_circle ? size : 20.0 + seeded_random(seed) * 80.0,
      color,
      is_circle,
    });
  }

  for (auto& lm : landmarks) {
    auto shape = create_shape(std::nullopt);
    append_shape_begin_fill(shape, lm.color);
    if (lm.is_circle) {
      append_shape_circle(shape, lm.x, lm.y, lm.width * 0.5);
    } else {
      append_shape_rectangle(shape, lm.x, lm.y, lm.width, lm.height);
    }
    append_shape_end_fill(shape);
    add_node_child(worldContainer, shape);
  }

  auto gridShape = create_shape(std::nullopt);
  append_shape_line_style(gridShape, 1.0, 0x333333ff);
  for (double gx = 0.0; gx <= WORLD_WIDTH; gx += 200.0) {
    append_shape_move_to(gridShape, gx, 0.0);
    append_shape_line_to(gridShape, gx, WORLD_HEIGHT);
  }
  for (double gy = 0.0; gy <= WORLD_HEIGHT; gy += 200.0) {
    append_shape_move_to(gridShape, 0.0, gy);
    append_shape_line_to(gridShape, WORLD_WIDTH, gy);
  }
  add_node_child(worldContainer, gridShape);

  auto playerShape = create_shape(std::nullopt);
  append_shape_begin_fill(playerShape, 0x44ff88ff);
  append_shape_rectangle(playerShape,
    playerX - PLAYER_SIZE * 0.5, playerY - PLAYER_SIZE * 0.5,
    PLAYER_SIZE, PLAYER_SIZE);
  append_shape_end_fill(playerShape);
  add_node_child(worldContainer, playerShape);

  auto zoomLabel = flight::text::create_text_label();
  zoomLabel->data.text = flight::String("Zoom: 1.00x");
  zoomLabel->x = 10.0;
  zoomLabel->y = 10.0;
  invalidate_node_local_transform(zoomLabel);
  add_node_child(hudContainer, zoomLabel);

  auto readonlyCamera = flight::structural_ref_cast<flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::Camera2D>>>>>(
    flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<flight::types::Camera2D>>>>(camera));
  auto viewMatrix = flight::geometry::create_matrix();
  flight::camera::get_camera2_dview_matrix(readonlyCamera, viewMatrix);
  auto visibleBounds = flight::geometry::create_rectangle();
  flight::camera::get_camera2_dvisible_bounds(readonlyCamera, visibleBounds);

  flight::camera::zoom_camera2_dat_screen_point(camera, CANVAS_WIDTH * 0.5, CANVAS_HEIGHT * 0.5, 1.1);

  constexpr double dt = 1.0 / 60.0;
  for (int frame = 0; frame < 120; ++frame) {
    playerX += PLAYER_SPEED * dt * 0.5;
    playerY += PLAYER_SPEED * dt * 0.3;
    flight::camera_controls::update_camera2_dfollow(camera, playerX, playerY, dt);
  }

  std::cout << "Flight camera2d example (naive C++ port): "
            << landmarks.size() << " landmarks, player at ("
            << playerX << ", " << playerY << ")\n";
  return 0;
}
