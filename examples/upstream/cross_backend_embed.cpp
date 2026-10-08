// Naive C++ port of @flighthq/example-cross-backend-embed
// Ported from .dependencies/flight/examples/packages/cross-backend-embed/src/app.ts

#include <flight/node/hierarchy.hpp>
#include <flight/node/node_transform2d.hpp>
#include <flight/scene2d/display_object.hpp>
#include <flight/scene2d/sprite.hpp>
#include <flight/scene2d/html_view.hpp>
#include <flight/shape/shape.hpp>
#include <flight/shape/shape_commands.hpp>
#include <flight/text/text_label.hpp>
#include <flight/texture/texture.hpp>
#include <flight/textureatlas/texture_atlas.hpp>
#include <flight/quadbatch/quad_batch.hpp>

#include <iostream>

int main() {
  using namespace flight::node;
  using namespace flight::scene2d;
  using namespace flight::texture;

  constexpr double PRODUCER_WIDTH = 280.0;
  constexpr double PRODUCER_HEIGHT = 180.0;
  constexpr int QUAD_SIZE = 24;
  constexpr int INSTANCE_COUNT = 24;

  // NOT AVAILABLE: generated Scene2D construction/hierarchy is partial at this SDK pin.
  auto root = create_display_object(std::nullopt);

  // Producer scene (quad batch)
  auto producerRoot = create_display_object(std::nullopt);
  auto atlas = create_texture_atlas({});
  add_texture_atlas_region(atlas, 0, 0, QUAD_SIZE, QUAD_SIZE);

  auto batch = flight::quadbatch::create_quad_batch();
  batch->data.atlas = atlas;
  for (int i = 0; i < INSTANCE_COUNT; ++i) {
    flight::quadbatch::append_quad_batch_instance(batch, i % 3, 0.0, 0.0);
  }
  add_node_child(producerRoot, batch);

  // Consumer scene
  auto portableSprite = create_sprite({});
  portableSprite->x = 24.0;
  portableSprite->y = 92.0;
  invalidate_node_local_transform(portableSprite);
  add_node_child(root, portableSprite);

  // HtmlView is web-only; in C++ we'd use a different embedding approach
  // auto liveView = create_html_view();

  // Labels
  auto titleLabel = flight::text::create_text_label();
  titleLabel->data.text = flight::String("ONE GL QUADBATCH PRODUCER, TWO EMBEDS");
  titleLabel->x = 24.0;
  titleLabel->y = 18.0;
  invalidate_node_local_transform(titleLabel);
  add_node_child(root, titleLabel);

  auto portableLabel = flight::text::create_text_label();
  portableLabel->data.text = flight::String("PORTABLE - Sprite + ImageResource");
  portableLabel->x = 24.0;
  portableLabel->y = 62.0;
  invalidate_node_local_transform(portableLabel);
  add_node_child(root, portableLabel);

  auto descLabel = flight::text::create_text_label();
  descLabel->data.text = flight::String("Producer owns pixels + cadence. Consumer owns placement.");
  descLabel->x = 24.0;
  descLabel->y = 336.0;
  invalidate_node_local_transform(descLabel);
  add_node_child(root, descLabel);

  std::cout << "Flight cross-backend-embed example (naive C++ port): "
            << INSTANCE_COUNT << " quad batch instances.\n";
  return 0;
}
