// Naive C++ port of @flighthq/example-snapshot
// Ported from .dependencies/flight/examples/packages/snapshot/src/app.ts

#include <flight/node/hierarchy.hpp>
#include <flight/node/node_transform2d.hpp>
#include <flight/scene2d/display_object.hpp>
#include <flight/shape/shape.hpp>
#include <flight/shape/shape_commands.hpp>
#include <flight/snapshot/capture_snapshot.hpp>
#include <flight/snapshot/equals_snapshot.hpp>
#include <flight/snapshot/interpolate_snapshots.hpp>
#include <flight/snapshot/restore_snapshot.hpp>
#include <flight/text/text_label.hpp>

#include <array>
#include <cmath>
#include <iostream>
#include <vector>

namespace {

constexpr double CANVAS_WIDTH = 800.0;
constexpr double CANVAS_HEIGHT = 500.0;
constexpr int ITEM_COUNT = 6;
constexpr double COLLECT_RADIUS = 30.0;
constexpr int SLOT_COUNT = 5;

struct ItemState {
  double x, y;
  bool collected;
};

struct PlayerState {
  double x, y, rotation;
  int score;
};

struct GameState {
  PlayerState player;
  std::vector<ItemState> items;
  double time;
};

GameState create_initial_state() {
  constexpr std::array<std::array<double, 2>, ITEM_COUNT> positions = {{
    {170.0, 120.0}, {280.0, 330.0}, {390.0, 105.0},
    {505.0, 330.0}, {620.0, 130.0}, {400.0, 220.0},
  }};
  GameState state;
  state.player = {CANVAS_WIDTH / 2.0, CANVAS_HEIGHT / 2.0, 0.0, 0};
  state.time = 0.0;
  for (int i = 0; i < ITEM_COUNT; ++i) {
    state.items.push_back({positions[i][0], positions[i][1], false});
  }
  return state;
}

} // namespace

int main() {
  using namespace flight::node;
  using namespace flight::shape;
  using namespace flight::scene2d;

  auto root = create_display_object(std::nullopt);

  auto gameState = create_initial_state();

  // Player shape.
  auto playerShape = create_shape(std::nullopt);
  append_shape_begin_fill(playerShape, 0x4488eeff);
  append_shape_circle(playerShape, 0.0, 0.0, 16.0);
  append_shape_end_fill(playerShape);
  append_shape_line_style(playerShape, 2.0, 0xffffffff);
  append_shape_move_to(playerShape, 0.0, 0.0);
  append_shape_line_to(playerShape, 20.0, 0.0);
  playerShape->x = gameState.player.x;
  playerShape->y = gameState.player.y;
  invalidate_node_local_transform(playerShape);
  add_node_child(root, playerShape);

  // Collectible item shapes.
  for (int i = 0; i < ITEM_COUNT; ++i) {
    auto itemShape = create_shape(std::nullopt);
    append_shape_begin_fill(itemShape, 0xffcc00ff);
    append_shape_circle(itemShape, 0.0, 0.0, 10.0);
    append_shape_end_fill(itemShape);
    itemShape->x = gameState.items[i].x;
    itemShape->y = gameState.items[i].y;
    invalidate_node_local_transform(itemShape);
    add_node_child(root, itemShape);
  }

  // Snapshot slots.
  // In the TypeScript version: captureSnapshot/restoreSnapshot/interpolateSnapshots/equalsSnapshot.
  auto snapshot1 = flight::snapshot::capture_snapshot(gameState);

  // Simulate game logic: move player and collect items.
  for (int frame = 0; frame < 120; ++frame) {
    gameState.time += 1.0 / 60.0;
    gameState.player.x += 2.0;
    gameState.player.y += std::sin(gameState.time * 3.0) * 1.5;
    gameState.player.rotation += 0.02;

    for (auto& item : gameState.items) {
      if (item.collected) continue;
      double dx = item.x - gameState.player.x;
      double dy = item.y - gameState.player.y;
      if (dx * dx + dy * dy < COLLECT_RADIUS * COLLECT_RADIUS) {
        item.collected = true;
        gameState.player.score++;
      }
    }
  }

  auto snapshot2 = flight::snapshot::capture_snapshot(gameState);

  bool areEqual = flight::snapshot::equals_snapshot(snapshot1, snapshot2);

  // Interpolate between snapshots at 50%.
  auto interpolated = flight::snapshot::interpolate_snapshots(snapshot1, snapshot2, 0.5);

  // Restore from snapshot.
  flight::snapshot::restore_snapshot(gameState, snapshot1);

  // Score display.
  auto scoreLabel = flight::text::create_text_label();
  scoreLabel->data.text = flight::String("Score: 0");
  scoreLabel->x = 20.0;
  scoreLabel->y = 20.0;
  invalidate_node_local_transform(scoreLabel);
  add_node_child(root, scoreLabel);

  // Snapshot slot indicators.
  for (int i = 0; i < SLOT_COUNT; ++i) {
    auto slotShape = create_shape(std::nullopt);
    append_shape_begin_fill(slotShape, 0x333333ff);
    append_shape_rectangle(slotShape,
      20.0 + i * 50.0, CANVAS_HEIGHT - 40.0,
      40.0, 30.0);
    append_shape_end_fill(slotShape);
    add_node_child(root, slotShape);
  }

  std::cout << "Flight snapshot example (naive C++ port): "
            << ITEM_COUNT << " collectibles, "
            << SLOT_COUNT << " snapshot slots, "
            << "snapshots equal = " << (areEqual ? "true" : "false") << "\n";
  return 0;
}
