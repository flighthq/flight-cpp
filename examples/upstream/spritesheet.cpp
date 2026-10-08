// Naive C++ port of @flighthq/example-spritesheet
// Ported from .dependencies/flight/examples/packages/spritesheet/src/app.ts

#include <flight/node/hierarchy.hpp>
#include <flight/node/node_transform2d.hpp>
#include <flight/scene2d/display_object.hpp>
#include <flight/scene2d/sprite.hpp>
#include <flight/spritesheet/spritesheet.hpp>
#include <flight/spritesheet/spritesheet_animation.hpp>
#include <flight/spritesheet/spritesheet_player.hpp>
#include <flight/texture/texture.hpp>
#include <flight/textureatlas/texture_atlas.hpp>

#include <iostream>

namespace {

constexpr int FRAME_SIZE = 256;
constexpr int FRAME_COUNT = 12;
constexpr int STRIP_WIDTH = FRAME_SIZE * FRAME_COUNT;
constexpr double DISPLAY_SCALE = 0.66;

} // namespace

int main() {
  using namespace flight::node;
  using namespace flight::scene2d;
  using namespace flight::spritesheet;
  using namespace flight::texture;

  auto root = create_display_object(std::nullopt);

  // In the TypeScript version, a sprite strip is procedurally generated on an HTML canvas.
  // In C++, we skip canvas creation and assume an image resource is provided externally.

  auto sheet = create_spritesheet_from_grid({
    .columns = FRAME_COUNT,
    .imageFile = flight::String(""),
    .imageHeight = FRAME_SIZE,
    .imageWidth = STRIP_WIDTH,
    .rows = 1,
  });

  flight::Array<double> allFrameIndices(FRAME_COUNT);
  for (int i = 0; i < FRAME_COUNT; ++i) {
    allFrameIndices.set(i, static_cast<double>(i));
  }

  auto spinAnimation = create_spritesheet_animation({
    .frameDuration = 80.0,
    .frames = allFrameIndices,
    .repeatCount = -1.0,
  });

  auto pingpongAnimation = create_spritesheet_animation({
    .direction = flight::String("pingpong"),
    .frameDuration = 120.0,
    .frames = allFrameIndices,
    .repeatCount = -1.0,
  });

  auto bitmap1 = flight::scene2d::create_sprite();
  bitmap1->x = 79.0;
  bitmap1->y = 246.0;
  bitmap1->scale_x = DISPLAY_SCALE;
  bitmap1->scale_y = DISPLAY_SCALE;
  invalidate_node_local_transform(bitmap1);
  add_node_child(root, bitmap1);

  auto player1 = create_spritesheet_player();
  play_spritesheet_animation(player1, spinAnimation);
  seek_spritesheet_player_to_frame(player1, 0);

  auto bitmap2 = flight::scene2d::create_sprite();
  bitmap2->x = 316.0;
  bitmap2->y = 246.0;
  bitmap2->scale_x = DISPLAY_SCALE;
  bitmap2->scale_y = DISPLAY_SCALE;
  invalidate_node_local_transform(bitmap2);
  add_node_child(root, bitmap2);

  auto player2 = create_spritesheet_player();
  player2->speed = 1.65;
  play_spritesheet_animation(player2, spinAnimation);
  seek_spritesheet_player_to_frame(player2, 3);

  auto bitmap3 = flight::scene2d::create_sprite();
  bitmap3->x = 553.0;
  bitmap3->y = 246.0;
  bitmap3->scale_x = DISPLAY_SCALE;
  bitmap3->scale_y = DISPLAY_SCALE;
  invalidate_node_local_transform(bitmap3);
  add_node_child(root, bitmap3);

  auto player3 = create_spritesheet_player();
  player3->speed = 0.85;
  play_spritesheet_animation(player3, pingpongAnimation);
  seek_spritesheet_player_to_frame(player3, 7);

  auto frame1 = get_spritesheet_player_frame(player1, sheet);
  auto frame2 = get_spritesheet_player_frame(player2, sheet);
  auto frame3 = get_spritesheet_player_frame(player3, sheet);

  constexpr double dt = 1000.0 / 60.0;
  for (int frame = 0; frame < 120; ++frame) {
    update_spritesheet_player(player1, dt);
    update_spritesheet_player(player2, dt);
    update_spritesheet_player(player3, dt);
  }

  std::cout << "Flight spritesheet example (naive C++ port): "
            << "3 playback modes, " << FRAME_COUNT << " frames each.\n";
  return 0;
}
