// Naive C++ port of @flighthq/example-video
// Ported from .dependencies/flight/examples/packages/video/src/app.ts

#include <flight/node/hierarchy.hpp>
#include <flight/node/node_transform2d.hpp>
#include <flight/scene2d/display_object.hpp>
#include <flight/scene2d/sprite.hpp>
#include <flight/video/video_resource.hpp>
#include <flight/video/video_texture.hpp>
#include <flight/video/video_playback.hpp>

#include <iostream>

int main() {
  using namespace flight::node;
  using namespace flight::scene2d;

  auto root = create_display_object(std::nullopt);

  // Three video display nodes with different transforms.
  auto videoNode = create_sprite();
  videoNode->x = 40.0;
  videoNode->y = 40.0;
  add_node_child(root, videoNode);

  auto secondVideoNode = create_sprite();
  secondVideoNode->x = 400.0;
  secondVideoNode->y = 40.0;
  secondVideoNode->scaleX = 1.5;
  secondVideoNode->scaleY = 1.5;
  secondVideoNode->alpha = 0.8;
  invalidate_node_local_transform(secondVideoNode);
  add_node_child(root, secondVideoNode);

  auto thirdVideoNode = create_sprite();
  thirdVideoNode->x = 200.0;
  thirdVideoNode->y = 280.0;
  thirdVideoNode->rotation = 10.0;
  invalidate_node_local_transform(thirdVideoNode);
  add_node_child(root, thirdVideoNode);

  // Create video resources (would normally load from files).
  auto videoResource = flight::video::create_video_resource();

  // Create video textures to display on sprites.
  auto videoTexture = flight::video::create_video_texture();
  videoNode->data.texture = videoTexture;
  invalidate_node_appearance(videoNode);

  auto secondTexture = flight::video::create_video_texture();
  secondVideoNode->data.texture = secondTexture;
  invalidate_node_appearance(secondVideoNode);

  auto thirdTexture = flight::video::create_video_texture();
  thirdVideoNode->data.texture = thirdTexture;
  invalidate_node_appearance(thirdVideoNode);

  // Play and configure video channels.
  // In the web version, playVideoResource returns a VideoChannel for controlling playback.
  // auto channel = flight::video::play_video_resource(videoResource);
  // flight::video::set_video_channel_gain(channel, 1.0);
  // flight::video::set_video_channel_playback_rate(channel, 0.75);

  // Simulate advancing video texture for a few frames.
  for (int frame = 0; frame < 60; ++frame) {
    double deltaTime = 1000.0 / 60.0;
    flight::video::advance_video_texture(videoTexture, deltaTime);
    flight::video::advance_video_texture(secondTexture, deltaTime);
    flight::video::advance_video_texture(thirdTexture, deltaTime);
  }

  std::cout << "Flight video example (naive C++ port): "
            << "3 video nodes created with video textures.\n";
  return 0;
}
