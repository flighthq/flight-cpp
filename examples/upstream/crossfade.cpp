// Naive C++ port of @flighthq/example-crossfade
// Ported from .dependencies/flight/examples/packages/crossfade/src/app.ts

#include <flight/animation/animation_clip.hpp>
#include <flight/animation/animation_crossfade.hpp>
#include <flight/animation/animation_player.hpp>
#include <flight/animation/animation_track.hpp>
#include <flight/node/hierarchy.hpp>
#include <flight/node/node_transform2d.hpp>
#include <flight/scene2d/display_object.hpp>
#include <flight/shape/shape.hpp>
#include <flight/shape/shape_commands.hpp>
#include <flight/text/text_label.hpp>

#include <iostream>

namespace {

flight::Ref<flight::types::AnimationTrack> scalar_track(
    flight::Array<double> times,
    flight::Array<double> values) {
  using Options = flight::animation::
      times_values_components_interpolation_quaternion_easing_segment_easings_9b0eadfed5e06042;
  return flight::animation::create_animation_track(flight::make_ref<Options>(Options{
      .times = flight::SequenceView<double>(std::move(times)),
      .values = flight::SequenceView<double>(std::move(values)),
      .components = 1.0,
      .interpolation = std::nullopt,
      .quaternion = false,
  }));
}

flight::Ref<flight::types::AnimationClip> body_clip(
    flight::Array<double> values) {
  auto track = scalar_track(
      flight::Array<double>{0.0, 0.5, 1.0, 1.5, 2.0}, std::move(values));
  auto channel = flight::animation::create_animation_channel(
      track, flight::Any(flight::String("body.y")));
  return flight::animation::create_animation_clip(
      flight::Array<flight::Ref<flight::types::AnimationChannel>>{channel}, 2.0);
}

} // namespace

int main() {
  using namespace flight::node;
  using namespace flight::shape;
  using namespace flight::scene2d;
  using namespace flight::animation;

  auto root = create_display_object(std::nullopt);

  auto idleClip = body_clip(flight::Array<double>{0.0, -1.0, 0.0, -1.0, 0.0});
  auto walkClip = body_clip(flight::Array<double>{0.0, -4.0, 0.0, -4.0, 0.0});
  auto runClip = body_clip(flight::Array<double>{0.0, -8.0, 0.0, -8.0, 0.0});

  auto idlePlayer = create_animation_player(idleClip);
  auto walkPlayer = create_animation_player(walkClip);
  auto runPlayer = create_animation_player(runClip);

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
  stateLabel->data->text = flight::String("State: idle");
  stateLabel->x = 20.0;
  stateLabel->y = 20.0;
  invalidate_node_local_transform(stateLabel);
  add_node_child(root, stateLabel);

  auto idleToWalk = create_animation_crossfade(idlePlayer, walkPlayer, 0.3);
  for (int frame = 0; frame < 60; ++frame) {
    advance_animation_crossfade(idleToWalk, 1.0 / 60.0);
  }

  auto walkToRun = create_animation_crossfade(walkPlayer, runPlayer, 0.3);
  for (int frame = 0; frame < 60; ++frame) {
    advance_animation_crossfade(walkToRun, 1.0 / 60.0);
  }

  const bool completed = is_animation_crossfade_complete(walkToRun);

  std::cout << "Flight crossfade example (naive C++ port): "
            << "idle/walk/run body tracks, "
            << "completed=" << (completed ? "true" : "false")
            << ", final weight=" << walkToRun->weight << "\n";
  return 0;
}
