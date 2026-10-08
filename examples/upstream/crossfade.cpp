// Naive C++ port of @flighthq/example-crossfade
// Ported from .dependencies/flight/examples/packages/crossfade/src/app.ts

#include <flight/animation/animation_clip.hpp>
#include <flight/animation/animation_layer_stack.hpp>
#include <flight/animation/animation_player.hpp>
#include <flight/animation/animation_state_machine.hpp>
#include <flight/animation/animation_track.hpp>
#include <flight/animation/animation_blend_tree.hpp>
#include <flight/node/hierarchy.hpp>
#include <flight/node/node_transform2d.hpp>
#include <flight/scene2d/display_object.hpp>
#include <flight/shape/shape.hpp>
#include <flight/shape/shape_commands.hpp>
#include <flight/text/text_label.hpp>

#include <array>
#include <cmath>
#include <iostream>
#include <vector>

namespace {

std::vector<double> quaternion_values(const std::vector<double>& angles) {
  std::vector<double> values;
  for (double angle : angles) {
    double halfAngle = (angle * M_PI) / 360.0;
    values.push_back(0.0);
    values.push_back(0.0);
    values.push_back(std::sin(halfAngle));
    values.push_back(std::cos(halfAngle));
  }
  return values;
}

} // namespace

int main() {
  using namespace flight::node;
  using namespace flight::shape;
  using namespace flight::scene2d;
  using namespace flight::animation;

  auto root = create_display_object(std::nullopt);

  // Animation times.
  flight::Array<double> times(5);
  times.set(0, 0.0); times.set(1, 0.5); times.set(2, 1.0); times.set(3, 1.5); times.set(4, 2.0);

  // Idle clip: gentle body bob, subtle leg sway.
  auto idleBodyValues = std::vector<double>{0.0, -1.0, 0.0, -1.0, 0.0};
  flight::Array<double> idleBodyArr(idleBodyValues.size());
  for (std::size_t i = 0; i < idleBodyValues.size(); ++i)
    idleBodyArr.set(i, idleBodyValues[i]);

  auto idleBodyTrack = create_animation_track({
    .components = 1, .times = times, .values = idleBodyArr,
  });

  auto idleLeftAngles = quaternion_values({-3, 3, -3, 3, -3});
  auto idleRightAngles = quaternion_values({3, -3, 3, -3, 3});

  auto idleClip = create_animation_clip();
  auto walkClip = create_animation_clip();
  auto runClip = create_animation_clip();

  auto idlePlayer = create_animation_player(idleClip);
  auto walkPlayer = create_animation_player(walkClip);
  auto runPlayer = create_animation_player(runClip);

  // State machine for crossfade transitions.
  auto stateMachine = create_animation_state_machine();

  auto idleState = create_animation_state_machine_state(stateMachine, flight::String("idle"));
  auto walkState = create_animation_state_machine_state(stateMachine, flight::String("walk"));
  auto runState = create_animation_state_machine_state(stateMachine, flight::String("run"));

  // Layer stack for animation sampling.
  auto layerStack = create_animation_layer_stack();
  auto smLayer = create_animation_state_machine_layer(stateMachine);

  // Blend tree for walk-to-run blending.
  auto blendTree = create_animation_blend_tree();
  auto walkInput = create_animation_blend_tree_input(blendTree, walkPlayer);
  auto runInput = create_animation_blend_tree_input(blendTree, runPlayer);
  auto blendLayer = create_animation_blend_tree_layer(blendTree);

  // Character visual: simple stick figure.
  auto body = create_shape(std::nullopt);
  append_shape_begin_fill(body, 0x4488eeff);
  append_shape_rectangle(body, -15.0, -40.0, 30.0, 50.0);
  append_shape_end_fill(body);
  body->x = 400.0;
  body->y = 250.0;
  invalidate_node_local_transform(body);
  add_node_child(root, body);

  auto leftLeg = create_shape(std::nullopt);
  append_shape_line_style(leftLeg, 4.0, 0x4488eeff);
  append_shape_move_to(leftLeg, -8.0, 10.0);
  append_shape_line_to(leftLeg, -8.0, 50.0);
  leftLeg->x = 400.0;
  leftLeg->y = 250.0;
  invalidate_node_local_transform(leftLeg);
  add_node_child(root, leftLeg);

  auto rightLeg = create_shape(std::nullopt);
  append_shape_line_style(rightLeg, 4.0, 0x4488eeff);
  append_shape_move_to(rightLeg, 8.0, 10.0);
  append_shape_line_to(rightLeg, 8.0, 50.0);
  rightLeg->x = 400.0;
  rightLeg->y = 250.0;
  invalidate_node_local_transform(rightLeg);
  add_node_child(root, rightLeg);

  // State label.
  auto stateLabel = flight::text::create_text_label();
  stateLabel->data.text = flight::String("State: idle");
  stateLabel->x = 20.0;
  stateLabel->y = 20.0;
  invalidate_node_local_transform(stateLabel);
  add_node_child(root, stateLabel);

  // Simulate animation transitions.
  for (int frame = 0; frame < 60; ++frame) {
    advance_animation_layer_stack(layerStack, 1.0 / 60.0);
    sample_animation_layer_stack(layerStack);
  }

  // Transition to walk.
  transition_animation_state_machine(stateMachine, walkState, 0.3);
  for (int frame = 0; frame < 60; ++frame) {
    advance_animation_layer_stack(layerStack, 1.0 / 60.0);
    sample_animation_layer_stack(layerStack);
  }

  // Transition to run.
  transition_animation_state_machine(stateMachine, runState, 0.3);
  for (int frame = 0; frame < 60; ++frame) {
    advance_animation_layer_stack(layerStack, 1.0 / 60.0);
    sample_animation_layer_stack(layerStack);
  }

  auto currentState = get_animation_state_machine_current_state(stateMachine);
  bool transitioning = is_animation_state_machine_transitioning(stateMachine);

  std::cout << "Flight crossfade example (naive C++ port): "
            << "3 animation states (idle/walk/run), "
            << "transitioning=" << (transitioning ? "true" : "false") << "\n";
  return 0;
}
