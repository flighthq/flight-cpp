// Naive C++ port of @flighthq/example-tilemap
// Ported from .dependencies/flight/examples/packages/tilemap/src/app.ts

#include <flight/camera/camera2d.hpp>
#include <flight/geometry/matrix.hpp>
#include <flight/geometry/vector2.hpp>
#include <flight/node/hierarchy.hpp>
#include <flight/node/node_transform2d.hpp>
#include <flight/scene2d/display_object.hpp>
#include <flight/scene2d/sprite.hpp>
#include <flight/texture/texture.hpp>
#include <flight/textureatlas/texture_atlas.hpp>
#include <flight/tilemap/tilemap.hpp>

#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <vector>

namespace {

constexpr double CANVAS_WIDTH = 800.0;
constexpr double CANVAS_HEIGHT = 600.0;
constexpr int TILE_SIZE = 32;
constexpr int TILE_COUNT = 8;
constexpr int MAP_COLUMNS = 72;
constexpr int MAP_ROWS = 54;
constexpr double MAP_WIDTH = MAP_COLUMNS * TILE_SIZE;
constexpr double MAP_HEIGHT = MAP_ROWS * TILE_SIZE;
constexpr double CAMERA_SPEED = 520.0;
constexpr double MIN_ZOOM = 0.55;
constexpr double MAX_ZOOM = 2.4;

constexpr std::array<std::string_view, 8> TILE_NAMES = {
  "meadow", "water", "shore", "rock", "trail", "snow", "lava", "forest"
};

} // namespace

int main() {
  using namespace flight::node;
  using namespace flight::scene2d;

  // NOT AVAILABLE: generated Scene2D construction/hierarchy is partial at this SDK pin.
  auto root = create_display_object(std::nullopt);
  auto world = create_display_object(std::nullopt);
  add_node_child(root, world);

  // Generate a simple procedural map.
  std::vector<int> mapData(MAP_COLUMNS * MAP_ROWS);
  for (int row = 0; row < MAP_ROWS; ++row) {
    for (int col = 0; col < MAP_COLUMNS; ++col) {
      int tile = 0; // meadow default
      if (row < 3 || row >= MAP_ROWS - 3) tile = 5; // snow at edges
      else if (col < 2 || col >= MAP_COLUMNS - 2) tile = 3; // rock borders
      else if (row > 20 && row < 25 && col > 10 && col < 60) tile = 4; // trail
      else if (row > 30 && row < 40 && col > 20 && col < 50) tile = 1; // water lake
      else if ((row == 30 || row == 40) && col > 20 && col < 50) tile = 2; // shore
      else if (row > 10 && row < 15 && col > 40 && col < 55) tile = 7; // forest
      else if (row > 45 && row < 48 && col > 5 && col < 15) tile = 6; // lava
      mapData[row * MAP_COLUMNS + col] = tile;
    }
  }

  // Create tilemap from the generated data.
  flight::Array<double> tileArray(mapData.size());
  for (std::size_t i = 0; i < mapData.size(); ++i) {
    tileArray.set(i, static_cast<double>(mapData[i]));
  }

  auto tilemap = flight::tilemap::create_tilemap();
  tilemap->columns = MAP_COLUMNS;
  tilemap->tile_width = TILE_SIZE;
  tilemap->tile_height = TILE_SIZE;
  tilemap->data.tiles = tileArray;
  add_node_child(world, tilemap);

  // Create camera.
  auto camera = flight::camera::create_camera_2_d(CANVAS_WIDTH, CANVAS_HEIGHT);
  camera->x = MAP_WIDTH * 0.5;
  camera->y = MAP_HEIGHT * 0.48;
  camera->zoom = 1.0;

  auto viewMatrix = flight::math::create_matrix();
  auto worldPoint = flight::math::create_vector_2();
  auto pickedCell = flight::math::create_vector_2();

  // Simulate a few frames of camera movement.
  for (int frame = 0; frame < 60; ++frame) {
    double dt = 1.0 / 60.0;
    camera->x += (CAMERA_SPEED * dt) / camera->zoom * 0.5;
    camera->x = std::max(CANVAS_WIDTH / (camera->zoom * 2.0),
                         std::min(MAP_WIDTH - CANVAS_WIDTH / (camera->zoom * 2.0), camera->x));

    flight::camera::get_camera_2_d_view_matrix(camera, viewMatrix);
    world->scale_x = viewMatrix->a;
    world->scale_y = viewMatrix->d;
    world->x = viewMatrix->tx;
    world->y = viewMatrix->ty;
    invalidate_node_local_transform(world);

    // Pick tile at screen center.
    flight::camera::unproject_camera_2_d_point(
      camera, CANVAS_WIDTH / 2.0, CANVAS_HEIGHT / 2.0, worldPoint);
    flight::tilemap::get_tilemap_column_row_at_point(
      pickedCell, tilemap, worldPoint->x, worldPoint->y);
    auto tile = flight::tilemap::get_tilemap_tile(
      tilemap, pickedCell->x, pickedCell->y);
  }

  std::cout << "Flight tilemap example (naive C++ port): "
            << MAP_COLUMNS << "x" << MAP_ROWS << " tile map, "
            << TILE_NAMES.size() << " tile types, "
            << "camera at (" << camera->x << ", " << camera->y << ")\n";
  return 0;
}
