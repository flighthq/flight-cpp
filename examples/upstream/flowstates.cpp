// Naive C++ port of @flighthq/example-flowstates
// Ported from .dependencies/flight/examples/packages/flowstates/src/app.ts

#include <flight/flow/flow_stack.hpp>
#include <flight/flow/flow_state.hpp>
#include <flight/node/hierarchy.hpp>
#include <flight/node/node_transform2d.hpp>
#include <flight/scene2d/display_object.hpp>
#include <flight/shape/shape.hpp>
#include <flight/shape/shape_commands.hpp>
#include <flight/text/text_label.hpp>

#include <iostream>

namespace {

constexpr double WIDTH = 600.0;
constexpr double HEIGHT = 400.0;

} // namespace

int main() {
  using namespace flight::node;
  using namespace flight::shape;
  using namespace flight::scene2d;
  using namespace flight::flow;

  auto root = create_display_object(std::nullopt);

  auto stack = create_flow_stack();
  int score = 0;

  // Boot state: splash screen.
  auto bootBg = create_shape(std::nullopt);
  append_shape_begin_fill(bootBg, 0x1a1a2eff);
  append_shape_rectangle(bootBg, 0.0, 0.0, WIDTH, HEIGHT);
  append_shape_end_fill(bootBg);

  auto bootLabel = flight::text::create_text_label();
  bootLabel->data.text = flight::String("FLIGHT FLOW STATES");
  bootLabel->x = WIDTH / 2.0 - 100.0;
  bootLabel->y = HEIGHT / 2.0 - 20.0;
  invalidate_node_local_transform(bootLabel);

  auto bootContainer = create_display_object(std::nullopt);
  add_node_child(bootContainer, bootBg);
  add_node_child(bootContainer, bootLabel);

  // Menu state.
  auto menuBg = create_shape(std::nullopt);
  append_shape_begin_fill(menuBg, 0x222244ff);
  append_shape_rectangle(menuBg, 0.0, 0.0, WIDTH, HEIGHT);
  append_shape_end_fill(menuBg);

  auto menuTitle = flight::text::create_text_label();
  menuTitle->data.text = flight::String("MAIN MENU");
  menuTitle->x = WIDTH / 2.0 - 60.0;
  menuTitle->y = 60.0;
  invalidate_node_local_transform(menuTitle);

  auto playButton = create_shape(std::nullopt);
  append_shape_begin_fill(playButton, 0x4488eeff);
  append_shape_rectangle(playButton, WIDTH / 2.0 - 80.0, 160.0, 160.0, 50.0);
  append_shape_end_fill(playButton);

  auto menuContainer = create_display_object(std::nullopt);
  add_node_child(menuContainer, menuBg);
  add_node_child(menuContainer, menuTitle);
  add_node_child(menuContainer, playButton);

  // Play state.
  auto playBg = create_shape(std::nullopt);
  append_shape_begin_fill(playBg, 0x1a3322ff);
  append_shape_rectangle(playBg, 0.0, 0.0, WIDTH, HEIGHT);
  append_shape_end_fill(playBg);

  auto scoreLabel = flight::text::create_text_label();
  scoreLabel->data.text = flight::String("Score: 0");
  scoreLabel->x = 20.0;
  scoreLabel->y = 20.0;
  invalidate_node_local_transform(scoreLabel);

  auto playContainer = create_display_object(std::nullopt);
  add_node_child(playContainer, playBg);
  add_node_child(playContainer, scoreLabel);

  // Pause state (overlay).
  auto pauseBg = create_shape(std::nullopt);
  append_shape_begin_fill(pauseBg, 0x00000088);
  append_shape_rectangle(pauseBg, 0.0, 0.0, WIDTH, HEIGHT);
  append_shape_end_fill(pauseBg);

  auto pauseLabel = flight::text::create_text_label();
  pauseLabel->data.text = flight::String("PAUSED");
  pauseLabel->x = WIDTH / 2.0 - 40.0;
  pauseLabel->y = HEIGHT / 2.0 - 15.0;
  invalidate_node_local_transform(pauseLabel);

  auto pauseContainer = create_display_object(std::nullopt);
  add_node_child(pauseContainer, pauseBg);
  add_node_child(pauseContainer, pauseLabel);

  // Push states onto the flow stack.
  auto bootState = create_flow_state();
  auto menuState = create_flow_state();
  auto playState = create_flow_state();
  auto pauseState = create_flow_state();

  push_flow_state(stack, bootState);

  // Simulate state transitions.
  update_flow_stack(stack, 1000.0);
  replace_flow_state(stack, menuState);
  update_flow_stack(stack, 500.0);
  push_flow_state(stack, playState);
  update_flow_stack(stack, 2000.0);
  push_flow_state(stack, pauseState);
  update_flow_stack(stack, 500.0);
  pop_flow_state(stack);
  update_flow_stack(stack, 1000.0);

  auto active = get_active_flow_state(stack);
  auto depth = get_flow_stack_depth(stack);

  add_node_child(root, bootContainer);

  std::cout << "Flight flowstates example (naive C++ port): "
            << "4 states defined, "
            << "stack depth = " << depth << "\n";
  return 0;
}
