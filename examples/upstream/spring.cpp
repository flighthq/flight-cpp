// Naive C++ port of @flighthq/example-spring
// Ported from .dependencies/flight/examples/packages/spring/src/app.ts

#include <flight/easing/ease_cubic.hpp>
#include <flight/node/hierarchy.hpp>
#include <flight/node/node_transform2d.hpp>
#include <flight/scene2d/display_object.hpp>
#include <flight/shape/shape.hpp>
#include <flight/shape/shape_commands.hpp>
#include <flight/spring/spring.hpp>
#include <flight/spring/spring_config.hpp>
#include <flight/tween/tween.hpp>
#include <flight/tween/tween_manager.hpp>

#include <cmath>
#include <iostream>

namespace {

constexpr double STAGE_WIDTH = 600.0;
constexpr double STAGE_HEIGHT = 400.0;
constexpr double CIRCLE_RADIUS = 18.0;
constexpr double TRACK_OFFSET = 22.0;

} // namespace

int main() {
  using namespace flight::node;
  using namespace flight::shape;
  using namespace flight::scene2d;
  using namespace flight::spring;
  using namespace flight::tween;

  auto root = create_display_object(std::nullopt);

  auto springConfig = create_spring_config(3.0, 0.3);

  auto spring2D = create_spring_2_d(STAGE_WIDTH / 2.0, STAGE_HEIGHT / 2.0 - TRACK_OFFSET);

  auto springCircle = create_shape(std::nullopt);
  append_shape_begin_fill(springCircle, 0x2196f3ff);
  append_shape_circle(springCircle, 0.0, 0.0, CIRCLE_RADIUS);
  append_shape_end_fill(springCircle);
  springCircle->x = spring2D->x.value;
  springCircle->y = spring2D->y.value;
  invalidate_node_local_transform(springCircle);
  add_node_child(root, springCircle);

  auto tweenManager = create_tween_manager();
  auto tweenCircle = create_shape(std::nullopt);
  append_shape_begin_fill(tweenCircle, 0xff9800ff);
  append_shape_circle(tweenCircle, 0.0, 0.0, CIRCLE_RADIUS);
  append_shape_end_fill(tweenCircle);
  tweenCircle->x = STAGE_WIDTH / 2.0;
  tweenCircle->y = STAGE_HEIGHT / 2.0 + TRACK_OFFSET;
  invalidate_node_local_transform(tweenCircle);
  add_node_child(root, tweenCircle);

  double targetX = 470.0;
  double targetY = STAGE_HEIGHT / 2.0;

  auto targetMarker = create_shape(std::nullopt);
  append_shape_line_style(targetMarker, 1.5, 0x999999ff);
  append_shape_move_to(targetMarker, targetX - 10.0, targetY);
  append_shape_line_to(targetMarker, targetX + 10.0, targetY);
  append_shape_move_to(targetMarker, targetX, targetY - 10.0);
  append_shape_line_to(targetMarker, targetX, targetY + 10.0);
  add_node_child(root, targetMarker);

  auto legend = create_shape(std::nullopt);
  double legendY = STAGE_HEIGHT - 28.0;
  append_shape_begin_fill(legend, 0x2196f3ff);
  append_shape_rectangle(legend, 12.0, legendY, 12.0, 12.0);
  append_shape_end_fill(legend);
  append_shape_begin_fill(legend, 0xff9800ff);
  append_shape_rectangle(legend, 110.0, legendY, 12.0, 12.0);
  append_shape_end_fill(legend);
  add_node_child(root, legend);

  create_tween(tweenManager, tweenCircle, 900.0,
    {.x = targetX, .y = targetY + TRACK_OFFSET},
    {.ease = flight::easing::ease_in_out_cubic});

  constexpr double dt = 1.0 / 60.0;
  for (int frame = 0; frame < 180; ++frame) {
    update_spring_2_d(spring2D, targetX, targetY - TRACK_OFFSET, springConfig, dt);
    springCircle->x = spring2D->x.value;
    springCircle->y = spring2D->y.value;
    invalidate_node_local_transform(springCircle);

    update_tweens(tweenManager, dt * 1000.0);
    invalidate_node_local_transform(tweenCircle);
  }

  std::cout << "Flight spring example (naive C++ port): "
            << "spring position = (" << spring2D->x.value << ", " << spring2D->y.value << ")\n";
  return 0;
}
