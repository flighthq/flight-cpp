// Naive C++ port of @flighthq/example-benchmark
// Ported from .dependencies/flight/examples/packages/benchmark/src/app.ts

#include <flight/node/hierarchy.hpp>
#include <flight/node/node_transform2d.hpp>
#include <flight/scene2d/display_object.hpp>
#include <flight/text/text_label.hpp>
#include <flight/texture/texture.hpp>
#include <flight/texture/texture_atlas.hpp>
#include <flight/quadbatch/quad_batch.hpp>

#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {

constexpr double GRAVITY = 0.5;
constexpr double WIDTH = 800.0;
constexpr double HEIGHT = 500.0;
constexpr int INITIAL_COUNT = 10;
constexpr int BATCH_SIZE = 100;
constexpr int SHAPE_SIZE = 16;

std::uint32_t rng_state = 0xbe7c;

double pseudo_random() {
  rng_state = rng_state * 1664525u + 1013904223u;
  return static_cast<double>(rng_state) / 4294967296.0;
}

} // namespace

int main() {
  using namespace flight::node;
  using namespace flight::scene2d;
  using namespace flight::quadbatch;
  using namespace flight::texture;

  auto root = create_display_object(std::nullopt);

  auto atlas = create_texture_atlas({});
  add_texture_atlas_region(atlas, 0, 0, SHAPE_SIZE, SHAPE_SIZE);

  auto quadBatch = create_quad_batch();
  quadBatch->data.atlas = atlas;
  add_node_child(root, quadBatch);

  auto countLabel = flight::text::create_text_label();
  countLabel->data.text = flight::String("0 shapes");
  countLabel->x = 10.0;
  countLabel->y = HEIGHT - 24.0;
  invalidate_node_local_transform(countLabel);
  add_node_child(root, countLabel);

  std::vector<double> posX, posY, speedX, speedY;

  auto add_shape = [&]() {
    resize_quad_batch(quadBatch, static_cast<int>(posX.size()) + 1);
    invalidate_node_appearance(quadBatch);
    posX.push_back(0.0);
    posY.push_back(0.0);
    speedX.push_back(pseudo_random() * 5.0);
    speedY.push_back(pseudo_random() * 5.0 - 2.5);
  };

  for (int i = 0; i < INITIAL_COUNT; ++i) {
    add_shape();
  }

  // Simulate a few frames
  for (int frame = 0; frame < 60; ++frame) {
    int count = static_cast<int>(posX.size());
    for (int i = 0; i < count; ++i) {
      posX[i] += speedX[i];
      posY[i] += speedY[i];
      speedY[i] += GRAVITY;

      if (posX[i] > WIDTH - SHAPE_SIZE) {
        speedX[i] *= -1.0;
        posX[i] = WIDTH - SHAPE_SIZE;
      } else if (posX[i] < 0.0) {
        speedX[i] *= -1.0;
        posX[i] = 0.0;
      }

      if (posY[i] > HEIGHT - SHAPE_SIZE) {
        speedY[i] *= -0.8;
        posY[i] = HEIGHT - SHAPE_SIZE;
        if (pseudo_random() > 0.5) {
          speedY[i] -= 3.0 + pseudo_random() * 4.0;
        }
      } else if (posY[i] < 0.0) {
        speedY[i] = 0.0;
        posY[i] = 0.0;
      }
    }

    invalidate_node_appearance(quadBatch);

    // Add shapes every 10 frames
    if (frame % 10 == 0) {
      for (int j = 0; j < BATCH_SIZE; ++j) {
        add_shape();
      }
    }
  }

  flight::text::set_text_label_string(countLabel,
    flight::String(std::to_string(posX.size()) + " shapes"));

  std::cout << "Flight benchmark example (naive C++ port): "
            << posX.size() << " shapes simulated over 60 frames.\n";
  return 0;
}
