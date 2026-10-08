// Naive C++ port of @flighthq/example-tween
// Ported from .dependencies/flight/examples/packages/tween/src/app.ts

#include <flight/easing/ease_bounce.hpp>
#include <flight/easing/ease_cubic.hpp>
#include <flight/easing/ease_elastic.hpp>
#include <flight/easing/ease_exponential.hpp>
#include <flight/easing/ease_quadratic.hpp>
#include <flight/easing/ease_sine.hpp>
#include <flight/node/hierarchy.hpp>
#include <flight/node/node_transform2d.hpp>
#include <flight/scene2d/display_object.hpp>
#include <flight/shape/shape.hpp>
#include <flight/shape/shape_commands.hpp>
#include <flight/signals/slot.hpp>
#include <flight/text/text_label.hpp>
#include <flight/tween/tween.hpp>
#include <flight/tween/tween_manager.hpp>
#include <flight/tween/timer.hpp>
#include <flight/app/app_loop.hpp>

#include <array>
#include <cmath>
#include <iostream>
#include <string_view>

namespace {

constexpr double CANVAS_WIDTH = 800.0;
constexpr double CANVAS_HEIGHT = 600.0;
constexpr int COLUMNS = 3;
constexpr int ROWS = 5;
constexpr double CELL_WIDTH = CANVAS_WIDTH / COLUMNS;
constexpr double CELL_HEIGHT = CANVAS_HEIGHT / ROWS;
constexpr double CIRCLE_RADIUS = 8.0;
constexpr double TWEEN_DURATION = 3200.0;
constexpr double TRACK_STAGGER = 110.0;
constexpr double END_HOLD = 900.0;
constexpr double TRACK_MARGIN = 20.0;

struct EasingEntry {
  std::string_view name;
  flight::types::EasingFunction ease;
};

} // namespace

int main() {
  using namespace flight::easing;
  using namespace flight::node;
  using namespace flight::shape;
  using namespace flight::scene2d;
  using namespace flight::tween;
  using namespace flight::signals;

  const std::array<EasingEntry, 15> easings{{
    {"easeInQuadratic", ease_in_quadratic},
    {"easeOutQuadratic", ease_out_quadratic},
    {"easeInOutQuadratic", ease_in_out_quadratic},
    {"easeInCubic", ease_in_cubic},
    {"easeOutCubic", ease_out_cubic},
    {"easeInOutCubic", ease_in_out_cubic},
    {"easeInSine", ease_in_sine},
    {"easeOutSine", ease_out_sine},
    {"easeInOutSine", ease_in_out_sine},
    {"easeInExponential", ease_in_exponential},
    {"easeOutExponential", ease_out_exponential},
    {"easeInOutExponential", ease_in_out_exponential},
    {"easeInElastic", ease_in_elastic},
    {"easeOutElastic", ease_out_elastic},
    {"easeOutBounce", ease_out_bounce},
  }};

  auto manager = create_tween_manager();
  // NOT AVAILABLE: generated Scene2D construction/hierarchy is partial at this SDK pin.
  auto root = create_display_object(std::nullopt);

  for (int i = 0; i < static_cast<int>(easings.size()); ++i) {
    int col = i % COLUMNS;
    int row = i / COLUMNS;
    double cellX = col * CELL_WIDTH;
    double cellY = row * CELL_HEIGHT;

    auto label = flight::text::create_text_label();
    label->data.text = flight::String(easings[i].name);
    label->x = cellX + 10.0;
    label->y = cellY + 8.0;
    invalidate_node_local_transform(label);
    add_node_child(root, label);

    double trackStartX = cellX + TRACK_MARGIN;
    double trackEndX = cellX + CELL_WIDTH - TRACK_MARGIN;
    double trackY = cellY + CELL_HEIGHT * 0.62;

    auto circle = create_shape(std::nullopt);
    append_shape_begin_fill(circle, 0x44aaeeff);
    append_shape_circle(circle, 0.0, 0.0, CIRCLE_RADIUS);
    append_shape_end_fill(circle);
    circle->x = trackStartX;
    circle->y = trackY;
    invalidate_node_local_transform(circle);
    add_node_child(root, circle);

    create_tween(manager, circle, TWEEN_DURATION, {.x = trackEndX}, {
      .delay = static_cast<double>(i) * TRACK_STAGGER,
      .ease = easings[i].ease,
    });
  }

  auto app = flight::app::create_app_loop();
  connect_signal(app.onUpdate, [&](double delta) {
    update_tweens(manager, delta);
  });

  flight::app::step_app_loop(app, 0.0);

  std::cout << "Flight tween example (naive C++ port): "
            << easings.size() << " easing curves configured.\n";
  return 0;
}
