// Naive C++ port of @flighthq/example-platformer
// Ported from .dependencies/flight/examples/packages/platformer/src/app.ts

#include <flight/camera/camera2d.hpp>
#include <flight/collision/collide_contact_manifold2_d.hpp>
#include <flight/flow/flow.hpp>
#include <flight/input/input_manager.hpp>
#include <flight/types/input_state.hpp>
#include <flight/geometry/matrix.hpp>
#include <flight/node/hierarchy.hpp>
#include <flight/node/node_transform2d.hpp>
#include <flight/scene2d/display_object.hpp>
#include <flight/scene2d/sprite.hpp>
#include <flight/shape/shape.hpp>
#include <flight/shape/shape_commands.hpp>
#include <flight/text/text_label.hpp>
#include <flight/texture/texture.hpp>
#include <flight/tween/tween.hpp>
#include <flight/tween/tween_manager.hpp>

#include <cmath>
#include <iostream>
#include <vector>

namespace {

constexpr double CANVAS_WIDTH = 800.0;
constexpr double CANVAS_HEIGHT = 500.0;
constexpr double GRAVITY = 980.0;
constexpr double JUMP_VELOCITY = -420.0;
constexpr double MOVE_SPEED = 220.0;
constexpr double PLAYER_WIDTH = 24.0;
constexpr double PLAYER_HEIGHT = 32.0;

struct Platform {
  double x, y, width, height;
  std::uint32_t color;
};

struct Player {
  double x, y;
  double vx, vy;
  bool onGround;
};

} // namespace

int main() {
  using namespace flight::node;
  using namespace flight::shape;
  using namespace flight::scene2d;

  // NOT AVAILABLE: generated Scene2D construction/hierarchy is partial at this SDK pin.
  auto root = create_display_object(std::nullopt);

  auto camera = flight::camera::create_camera_2_d(CANVAS_WIDTH, CANVAS_HEIGHT, {
    .x = CANVAS_WIDTH * 0.5,
    .y = CANVAS_HEIGHT * 0.5,
    .zoom = 1.0,
  });

  std::vector<Platform> platforms = {
    {0.0, CANVAS_HEIGHT - 40.0, CANVAS_WIDTH * 3.0, 40.0, 0x556b2fff},
    {200.0, 360.0, 150.0, 20.0, 0x8b6914ff},
    {450.0, 280.0, 180.0, 20.0, 0x8b6914ff},
    {700.0, 200.0, 120.0, 20.0, 0x8b6914ff},
    {950.0, 320.0, 200.0, 20.0, 0x8b6914ff},
    {1250.0, 240.0, 160.0, 20.0, 0x8b6914ff},
  };

  for (auto& plat : platforms) {
    auto shape = create_shape(std::nullopt);
    append_shape_begin_fill(shape, plat.color);
    append_shape_rectangle(shape, plat.x, plat.y, plat.width, plat.height);
    append_shape_end_fill(shape);
    add_node_child(root, shape);
  }

  Player player{100.0, 300.0, 0.0, 0.0, false};

  auto playerShape = create_shape(std::nullopt);
  append_shape_begin_fill(playerShape, 0x4488ffff);
  append_shape_rectangle(playerShape, 0.0, 0.0, PLAYER_WIDTH, PLAYER_HEIGHT);
  append_shape_end_fill(playerShape);
  playerShape->x = player.x;
  playerShape->y = player.y;
  invalidate_node_local_transform(playerShape);
  add_node_child(root, playerShape);

  auto manifold = flight::collision::create_collision_manifold_2_d();

  auto tweenManager = flight::tween::create_tween_manager();
  auto flowStack = flight::flowstates::create_flow_stack();

  auto scoreLabel = flight::text::create_text_label();
  scoreLabel->data.text = flight::String("Score: 0");
  scoreLabel->x = 10.0;
  scoreLabel->y = 10.0;
  invalidate_node_local_transform(scoreLabel);
  add_node_child(root, scoreLabel);

  constexpr double dt = 1.0 / 60.0;
  for (int frame = 0; frame < 300; ++frame) {
    player.vy += GRAVITY * dt;
    player.x += player.vx * dt;
    player.y += player.vy * dt;

    player.onGround = false;
    for (auto& plat : platforms) {
      if (player.x + PLAYER_WIDTH > plat.x &&
          player.x < plat.x + plat.width &&
          player.y + PLAYER_HEIGHT > plat.y &&
          player.y + PLAYER_HEIGHT < plat.y + plat.height + 10.0 &&
          player.vy >= 0.0) {
        player.y = plat.y - PLAYER_HEIGHT;
        player.vy = 0.0;
        player.onGround = true;
      }
    }

    playerShape->x = player.x;
    playerShape->y = player.y;
    invalidate_node_local_transform(playerShape);

    flight::camera::update_camera_2_d_follow(camera, player.x, player.y, dt);
    flight::tween::update_tweens(tweenManager, dt * 1000.0);
  }

  std::cout << "Flight platformer example (naive C++ port): "
            << platforms.size() << " platforms, player at ("
            << player.x << ", " << player.y << ")\n";
  return 0;
}
