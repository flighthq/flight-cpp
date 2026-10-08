// Naive C++ port of @flighthq/example-interaction
// Ported from .dependencies/flight/examples/packages/interaction/src/app.ts

#include <flight/interaction/interaction_manager.hpp>
#include <flight/interaction/hit_test.hpp>
#include <flight/interaction/input_manager.hpp>
#include <flight/node/hierarchy.hpp>
#include <flight/node/node_transform2d.hpp>
#include <flight/scene2d/display_object.hpp>
#include <flight/shape/shape.hpp>
#include <flight/shape/shape_commands.hpp>
#include <flight/text/text_label.hpp>

#include <iostream>

int main() {
  using namespace flight::node;
  using namespace flight::shape;
  using namespace flight::scene2d;
  using namespace flight::interaction;

  auto root = create_display_object(std::nullopt);

  // Register hit test handlers.
  register_default_hit_tests();
  register_shape_hit_test();

  // Create interaction manager.
  auto manager = create_interaction_manager(root);

  // Create input manager and wire up pointer input.
  auto inputManager = create_input_manager();
  connect_input_to_interaction(inputManager, manager);

  // Interactive shapes: draggable circle, clickable rectangle, hoverable polygon.
  auto dragCircle = create_shape(std::nullopt);
  append_shape_begin_fill(dragCircle, 0x2196f3ff);
  append_shape_circle(dragCircle, 200.0, 200.0, 50.0);
  append_shape_end_fill(dragCircle);
  set_node_hit_test_enabled(dragCircle, true);
  set_node_cursor(dragCircle, flight::String("grab"));
  add_node_child(root, dragCircle);

  auto clickRect = create_shape(std::nullopt);
  append_shape_begin_fill(clickRect, 0x4caf50ff);
  append_shape_rectangle(clickRect, 400.0, 150.0, 150.0, 100.0);
  append_shape_end_fill(clickRect);
  set_node_hit_test_enabled(clickRect, true);
  set_node_cursor(clickRect, flight::String("pointer"));
  add_node_child(root, clickRect);

  auto hoverCircle = create_shape(std::nullopt);
  append_shape_begin_fill(hoverCircle, 0xff9800ff);
  append_shape_circle(hoverCircle, 350.0, 400.0, 40.0);
  append_shape_end_fill(hoverCircle);
  set_node_hit_test_enabled(hoverCircle, true);
  add_node_child(root, hoverCircle);

  // Connect interaction signals.
  connect_interaction_signal(manager, dragCircle, flight::String("pointerDown"),
    [&](auto& event) {
      capture_interaction_pointer(manager, dragCircle, event);
    });

  connect_interaction_signal(manager, dragCircle, flight::String("pointerUp"),
    [&](auto& event) {
      release_interaction_pointer(manager, dragCircle, event);
    });

  connect_interaction_signal(manager, clickRect, flight::String("pointerTap"),
    [&](auto& event) {
      // Toggle color on click.
    });

  connect_interaction_signal(manager, hoverCircle, flight::String("pointerOver"),
    [&](auto& event) {
      // Highlight on hover.
    });

  connect_interaction_signal(manager, hoverCircle, flight::String("pointerOut"),
    [&](auto& event) {
      // Remove highlight.
    });

  // Status label.
  auto statusLabel = flight::text::create_text_label();
  statusLabel->data.text = flight::String("Hover/click/drag the shapes");
  statusLabel->x = 20.0;
  statusLabel->y = 20.0;
  invalidate_node_local_transform(statusLabel);
  add_node_child(root, statusLabel);

  // Event log.
  auto logLabel = flight::text::create_text_label();
  logLabel->data.text = flight::String("Events:");
  logLabel->x = 20.0;
  logLabel->y = 540.0;
  invalidate_node_local_transform(logLabel);
  add_node_child(root, logLabel);

  std::cout << "Flight interaction example (naive C++ port): "
            << "3 interactive shapes, interaction manager configured.\n";
  return 0;
}
