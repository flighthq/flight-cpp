// Naive C++ port of @flighthq/example-spatial
// Ported from .dependencies/flight/examples/packages/spatial/src/app.ts

#include <flight/node/hierarchy.hpp>
#include <flight/node/node_transform2d.hpp>
#include <flight/scene2d/display_object.hpp>
#include <flight/shape/shape.hpp>
#include <flight/shape/shape_commands.hpp>
#include <flight/spatial/spatial_index2_d.hpp>
#include <flight/spatial/spatial_backend.hpp>
#include <flight/text/text_label.hpp>

#include <cmath>
#include <iostream>
#include <vector>

namespace {

constexpr double CANVAS_WIDTH = 800.0;
constexpr double CANVAS_HEIGHT = 500.0;
constexpr std::uint32_t COLOR_IDLE = 0x4488ccff;
constexpr std::uint32_t COLOR_OVERLAP = 0xcc4444ff;
constexpr std::uint32_t COLOR_POINT_HIT = 0x44cc44ff;
constexpr int OBJECT_COUNT = 20;
constexpr int MOVING_COUNT = 5;

struct SpatialObject {
  double x, y, w, h;
  double vx, vy;
};

double seeded_random(int& seed) {
  seed = (seed * 16807) % 2147483647;
  return static_cast<double>(seed) / 2147483647.0;
}

} // namespace

int main() {
  using namespace flight::node;
  using namespace flight::shape;
  using namespace flight::scene2d;

  auto root = create_display_object(std::nullopt);

  auto backend = flight::spatial::create_uniform_grid_spatial_backend_2_d(100.0);
  auto index = flight::spatial::create_spatial_index_2_d(backend);

  int seed = 0x5a171a1;
  std::vector<SpatialObject> objects;
  for (int i = 0; i < OBJECT_COUNT; ++i) {
    double w = 30.0 + seeded_random(seed) * 60.0;
    double h = 30.0 + seeded_random(seed) * 60.0;
    double x = seeded_random(seed) * (CANVAS_WIDTH - w);
    double y = seeded_random(seed) * (CANVAS_HEIGHT - h);
    double vx = (i < MOVING_COUNT) ? (seeded_random(seed) * 100.0 - 50.0) : 0.0;
    double vy = (i < MOVING_COUNT) ? (seeded_random(seed) * 100.0 - 50.0) : 0.0;
    objects.push_back({x, y, w, h, vx, vy});

    flight::spatial::insert_spatial_object_2_d(index, i, x, y, x + w, y + h);

    auto shape = create_shape(std::nullopt);
    append_shape_begin_fill(shape, COLOR_IDLE);
    append_shape_rectangle(shape, x, y, w, h);
    append_shape_end_fill(shape);
    add_node_child(root, shape);
  }

  auto pairs = flight::spatial::query_spatial_pairs_2_d(index);
  auto pointHits = flight::spatial::query_spatial_point_2_d(index,
    CANVAS_WIDTH * 0.5, CANVAS_HEIGHT * 0.5);
  auto regionHits = flight::spatial::query_spatial_region_2_d(index,
    100.0, 100.0, 300.0, 300.0);

  constexpr double dt = 1.0 / 60.0;
  for (int frame = 0; frame < 60; ++frame) {
    for (int i = 0; i < MOVING_COUNT; ++i) {
      auto& obj = objects[i];
      obj.x += obj.vx * dt;
      obj.y += obj.vy * dt;
      if (obj.x < 0.0 || obj.x + obj.w > CANVAS_WIDTH) obj.vx = -obj.vx;
      if (obj.y < 0.0 || obj.y + obj.h > CANVAS_HEIGHT) obj.vy = -obj.vy;
      flight::spatial::update_spatial_object_2_d(index, i,
        obj.x, obj.y, obj.x + obj.w, obj.y + obj.h);
    }
  }

  std::cout << "Flight spatial example (naive C++ port): "
            << OBJECT_COUNT << " objects in spatial index.\n";
  return 0;
}
