#include <flight/camera/camera2d.hpp>
#include <flight/gizmo/contract.hpp>
#include <flight/scene2d/display_object.hpp>
#include <flight/selection/selection_state.hpp>
#include <flight/signals/emitter.hpp>
#include <flight/signals/slot.hpp>

#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <numbers>
#include <optional>
#include <string>
#include <vector>

// Native ports of the a627849 gizmo suites. Floating transform checks use 1e-8 because the
// upstream assertions use toBeCloseTo(..., 8), and projection plus trigonometry is not exact.
//
// MUTATION SENSITIVITY, measured 2026-10-13: four one-at-a-time implementation mutations were
// compiled and run, then restored. The suite killed a wrong semantic handle name, removal of the
// pointer-identity guard, forward rather than reverse listener cleanup, and removal of rotation-seam
// normalization.

namespace {

namespace camera = flight::camera;
namespace geometry = flight::geometry;
namespace gizmo = flight::gizmo;
namespace selection = flight::selection;
namespace signals = flight::signals;
namespace types = flight::types;

int failures = 0;
constexpr double tolerance = 1e-8;

void check(bool condition, const std::string& message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

void check_close(double actual, double expected, const std::string& message) {
  check(std::abs(actual - expected) <= tolerance, message);
}

template <typename Type>
auto readonly(const std::shared_ptr<Type>& value) {
  using View = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<Type>>>>;
  return View(value);
}

template <typename Callback>
void expect_bad_cast(Callback&& callback, const std::string& message) {
  try {
    callback();
    check(false, message);
  } catch (const std::bad_cast&) {
  } catch (...) {
    check(false, message + " (wrong exception type)");
  }
}

using RectangleView = flight::StructuralRef<
    flight::RowReadonly<flight::RowOf<types::RectangleLike>>>;

flight::SequenceView<RectangleView> rectangle_sequence(
    flight::Array<flight::Ref<types::Rectangle>> values) {
  return flight::SequenceView<RectangleView>(std::move(values));
}

bool values_are(
    const flight::Array<double>& actual,
    std::initializer_list<double> expected) {
  if (actual.size() != expected.size()) return false;
  std::size_t index = 0;
  for (double value : expected) {
    if (actual.element(static_cast<double>(index)) != value) return false;
    ++index;
  }
  return true;
}

void test_alignment_and_guides() {
  flight::Array<double> out;
  auto bounds = flight::Array<flight::Ref<types::Rectangle>>{
      geometry::create_rectangle(10.0, 4.0, 10.0, 8.0),
      geometry::create_rectangle(-5.0, 20.0, 4.0, 6.0),
      geometry::create_rectangle(20.0, -2.0, -8.0, 10.0),
  };
  gizmo::compute_gizmo_alignment_deltas(
      out, rectangle_sequence(bounds), flight::String("left"));
  check(values_are(out, {-15.0, 0.0, 0.0, 0.0, -17.0, 0.0}),
        "left alignment preserves selection order and negative-width bounds");

  out = {1.0, 2.0};
  gizmo::compute_gizmo_alignment_deltas(
      out, rectangle_sequence({}), flight::String("bottom"));
  check(out.empty(), "empty alignment clears stale output");

  auto result = flight::make_ref<types::GizmoSmartGuideResult>(
      types::GizmoSmartGuideResult{.delta_x = 99.0,
                                   .delta_y = 99.0,
                                   .guide_x = 99.0,
                                   .guide_y = 99.0});
  auto moving = geometry::create_rectangle(-10.0, 20.0, -10.0, -10.0);
  auto candidates = flight::Array<flight::Ref<types::Rectangle>>{
      geometry::create_rectangle(-7.0, 100.0, 4.0, 4.0),
      geometry::create_rectangle(-30.0, 100.0, 7.0, 4.0),
  };
  check(gizmo::find_gizmo_smart_guides(
            result, flight::structural_ref_cast<RectangleView>(readonly(moving)),
            rectangle_sequence(candidates), 3.0),
        "smart guides find a threshold match");
  check(result->delta_x == 3.0 && result->delta_y == 0.0 &&
            result->guide_x == std::optional<double>{-7.0} &&
            !result->guide_y.has_value(),
        "smart-guide ties use candidate and anchor order deterministically");

  result->delta_x = 99.0;
  result->delta_y = 99.0;
  result->guide_x = 99.0;
  result->guide_y = 99.0;
  check(!gizmo::find_gizmo_smart_guides(
            result, flight::structural_ref_cast<RectangleView>(readonly(moving)),
            rectangle_sequence(candidates),
            std::numeric_limits<double>::infinity()) &&
            result->delta_x == 0.0 && result->delta_y == 0.0 &&
            !result->guide_x.has_value() && !result->guide_y.has_value(),
        "invalid guide thresholds reset stale output");
}

using Node = flight::Ref<types::Shape>;
using NodeView = flight::StructuralRef<flight::RowReadonly<flight::RowOf<Node>>>;

struct NodeFeatures {
  Node node;
  flight::Ref<types::Rectangle> bounds;
  double origin_x;
  double origin_y;
  double rotation;
  bool available;
};

struct Context {
  flight::Ref<types::Camera2D> camera;
  flight::Ref<types::Scene2D> overlay;
  flight::Ref<types::SelectionState<Node>> selection;
  flight::Ref<types::GizmoNode2DFeatures<Node>> features;
  flight::Ref<types::GizmoState<Node>> state;
  flight::Ref<gizmo::GizmoRuntime<Node>> runtime;
  std::shared_ptr<std::vector<NodeFeatures>> node_features;
};

NodeFeatures* find_features(
    const std::shared_ptr<std::vector<NodeFeatures>>& values,
    const NodeView& view) {
  auto node = view.template shared_exact_object<types::Shape>();
  for (auto& value : *values) {
    if (value.node == node) return &value;
  }
  return nullptr;
}

flight::Ref<types::Scene2D> create_overlay() {
  auto overlay = flight::make_ref<types::Scene2D>();
  overlay->entity_runtime_key = std::nullopt;
  overlay->align = flight::String("topleft");
  overlay->color = std::nullopt;
  overlay->root = flight::scene2d::create_display_object();
  overlay->scale_mode = flight::String("noscale");
  overlay->scene2d_height = 100.0;
  overlay->scene2d_width = 200.0;
  return overlay;
}

Context create_context() {
  Context context;
  context.camera = camera::create_camera2_d();
  context.overlay = create_overlay();
  context.selection = selection::create_selection_state<Node>();
  context.node_features = std::make_shared<std::vector<NodeFeatures>>();

  auto first = flight::shape::create_shape();
  auto second = flight::shape::create_shape();
  context.node_features->push_back(NodeFeatures{
      .node = first,
      .bounds = geometry::create_rectangle(0.0, 0.0, 20.0, 10.0),
      .origin_x = 10.0,
      .origin_y = 5.0,
      .rotation = 30.0,
      .available = true,
  });
  context.node_features->push_back(NodeFeatures{
      .node = second,
      .bounds = geometry::create_rectangle(20.0, -10.0, 10.0, 20.0),
      .origin_x = 25.0,
      .origin_y = 0.0,
      .rotation = -15.0,
      .available = true,
  });
  selection::select_all_nodes(
      context.selection, flight::Array<Node>{first, second});

  context.features = flight::make_ref<types::GizmoNode2DFeatures<Node>>();
  auto values = context.node_features;
  context.features->get_world_bounds_rectangle =
      [values](flight::Ref<types::Rectangle> out, NodeView node) {
        auto* value = find_features(values, node);
        if (!value || !value->available) return false;
        out->x = value->bounds->x;
        out->y = value->bounds->y;
        out->width = value->bounds->width;
        out->height = value->bounds->height;
        return true;
      };
  context.features->get_world_origin =
      [values](types::Vector2Like out, NodeView node) {
        auto* value = find_features(values, node);
        out->x = value->origin_x;
        out->y = value->origin_y;
      };
  context.features->get_world_rotation = [values](NodeView node) {
    return find_features(values, node)->rotation;
  };

  auto options = flight::make_ref<types::GizmoCreateOptions<Node>>();
  options->camera = readonly(context.camera);
  options->features = readonly(context.features);
  options->overlay_scene = context.overlay;
  options->selection = context.selection;
  options->viewport_height = 100.0;
  options->viewport_width = 200.0;
  context.state = gizmo::create_gizmo_state<Node>(readonly(options));
  context.runtime = gizmo::get_gizmo_runtime<Node>(readonly(context.state));
  return context;
}

flight::Ref<gizmo::GizmoHandle> find_handle(
    const Context& context, const flight::String& placement) {
  for (std::size_t index = 0; index < context.runtime->handles.size(); ++index) {
    auto handle = context.runtime->handles.element(static_cast<double>(index));
    if (handle->placement == placement) return handle;
  }
  return nullptr;
}

flight::Ref<types::PointerEventData> pointer_data(
    double x, double y, double pointer_id = 1.0, bool shift = false) {
  return flight::make_ref<types::PointerEventData>(types::PointerEventData{
      .alt_key = false,
      .button = 0.0,
      .buttons = 1.0,
      .ctrl_key = false,
      .current_target = std::nullopt,
      .delta_x = 0.0,
      .delta_y = 0.0,
      .local_x = x,
      .local_y = y,
      .meta_key = false,
      .pointer_id = pointer_id,
      .pointer_type = flight::String("mouse"),
      .shift_key = shift,
      .target = std::nullopt,
      .world_x = x,
      .world_y = y,
      .x = x,
      .y = y,
  });
}

void emit_pointer(
    const std::shared_ptr<types::Signal<gizmo::GizmoPointerSlot>>& signal,
    double x, double y, double pointer_id = 1.0, bool shift = false) {
  signals::emit_signal<gizmo::GizmoPointerSlot>(
      signal, readonly(pointer_data(x, y, pointer_id, shift)));
}

void test_state_creation_and_layout() {
  auto context = create_context();
  check(context.runtime->handles.size() == 12,
        "state creates the fixed twelve-handle set");
  const char* names[] = {
      "GizmoRotateHandle",         "GizmoTranslateXHandle",
      "GizmoTranslateYHandle",     "GizmoTranslateXYHandle",
      "GizmoScaleEastHandle",      "GizmoScaleWestHandle",
      "GizmoScaleNorthHandle",     "GizmoScaleSouthHandle",
      "GizmoScaleNortheastHandle", "GizmoScaleNorthwestHandle",
      "GizmoScaleSoutheastHandle", "GizmoScaleSouthwestHandle",
  };
  for (std::size_t index = 0; index < 12; ++index) {
    check(context.runtime->handles.element(static_cast<double>(index))->node->name ==
              std::optional<flight::String>{flight::String(names[index])},
          "handle name and order match the semantic handle set");
  }
  check(context.runtime->bounds->x == 0.0 && context.runtime->bounds->y == -10.0 &&
            context.runtime->bounds->width == 30.0 &&
            context.runtime->bounds->height == 20.0,
        "update combines multi-selection bounds");
  check_close(context.runtime->pivot_world->x, 15.0,
              "translate pivot uses combined bounds center x");
  check_close(context.runtime->pivot_world->y, 0.0,
              "translate pivot uses combined bounds center y");
  check(context.runtime->cleanups.size() == 12,
        "each handle contributes one reversible listener cleanup");
  check(gizmo::get_gizmo_mode<Node>(readonly(context.state)) ==
            flight::String("translate") &&
            gizmo::get_gizmo_space<Node>(readonly(context.state)) ==
                flight::String("world"),
        "mode and space expose initialized state");
  auto first_signals = gizmo::get_gizmo_signals<Node>(readonly(context.state));
  auto second_signals = gizmo::get_gizmo_signals<Node>(readonly(context.state));
  check(first_signals == second_signals,
        "gizmo signal access preserves stable owner identity");

  gizmo::set_gizmo_mode<Node>(context.state, flight::String("scale"));
  gizmo::update_gizmo<Node>(context.state);
  check_close(find_handle(context, flight::String("scale-east"))->node->x, 15.0,
              "east handle uses the combined projected edge");
  check_close(find_handle(context, flight::String("scale-north"))->node->y, -10.0,
              "north handle uses the combined projected edge");
  check_close(find_handle(context, flight::String("scale-southwest"))->node->x, -15.0,
              "southwest handle keeps deterministic corner placement x");
  check_close(find_handle(context, flight::String("scale-southwest"))->node->y, 10.0,
              "southwest handle keeps deterministic corner placement y");

  std::vector<int> cleanup_order;
  context.runtime->cleanups.push([&] { cleanup_order.push_back(1); });
  context.runtime->cleanups.push([&] { cleanup_order.push_back(2); });
  auto handle_signals = gizmo::enable_gizmo_interaction_signals(
      find_handle(context, flight::String("translate-x"))->node);

  gizmo::dispose_gizmo_state<Node>(context.state);
  gizmo::dispose_gizmo_state<Node>(context.state);
  check(context.runtime->disposed && context.runtime->cleanups.empty() &&
            !context.runtime->camera.has_value() &&
            !context.runtime->features.has_value() &&
            !context.runtime->selection.has_value() &&
            !gizmo::get_gizmo_node_parent(context.runtime->overlay_root).has_value(),
        "dispose detaches visuals and clears listeners/references idempotently");
  check(cleanup_order == std::vector<int>({2, 1}),
        "dispose executes cleanups in reverse registration order exactly once");
  check(!handle_signals->on_pointer_down->data.has_value() &&
            !handle_signals->on_pointer_move->data.has_value() &&
            !handle_signals->on_pointer_up->data.has_value() &&
            !handle_signals->on_pointer_cancel->data.has_value() &&
            !handle_signals->on_release_outside->data.has_value(),
        "dispose disconnects every handle listener");

  auto empty = flight::make_ref<types::GizmoState<Node>>();
  empty->entity_runtime_key = std::nullopt;
  expect_bad_cast(
      [&] { static_cast<void>(gizmo::get_gizmo_runtime<Node>(readonly(empty))); },
      "runtime recovery refuses a state without its exact runtime owner");
}

void use_single_origin_node(Context& context, double rotation = 0.0) {
  auto& feature = context.node_features->front();
  feature.bounds->x = 0.0;
  feature.bounds->y = 0.0;
  feature.bounds->width = 0.0;
  feature.bounds->height = 0.0;
  feature.origin_x = 0.0;
  feature.origin_y = 0.0;
  feature.rotation = rotation;
  selection::select_node(context.selection, feature.node);
  gizmo::update_gizmo<Node>(context.state);
}

void test_interaction_state_and_math() {
  auto context = create_context();
  use_single_origin_node(context);
  int began = 0;
  int ended = 0;
  std::vector<std::pair<double, double>> translated;
  std::vector<double> rotated;
  std::vector<std::pair<double, double>> scaled;
  signals::connect_signal<std::function<void()>>(
      context.runtime->signals->on_transform_begin, [&] { ++began; });
  signals::connect_signal<std::function<void()>>(
      context.runtime->signals->on_transform_end, [&] { ++ended; });
  signals::connect_signal<std::function<void(double, double)>>(
      context.runtime->signals->on_translate,
      [&](double x, double y) { translated.emplace_back(x, y); });
  signals::connect_signal<std::function<void(double)>>(
      context.runtime->signals->on_rotate,
      [&](double degrees) { rotated.push_back(degrees); });
  signals::connect_signal<std::function<void(double, double)>>(
      context.runtime->signals->on_scale,
      [&](double x, double y) { scaled.emplace_back(x, y); });

  auto translate_x = gizmo::enable_gizmo_interaction_signals(
      find_handle(context, flight::String("translate-x"))->node);
  gizmo::set_gizmo_snap_translate<Node>(context.state, 10.0);
  emit_pointer(translate_x->on_pointer_down, 172.0, 50.0);
  emit_pointer(translate_x->on_pointer_down, 172.0, 50.0);
  emit_pointer(translate_x->on_pointer_move, 185.0, 74.0, 2.0);
  check(translated.empty(), "foreign pointer motion is ignored");
  emit_pointer(translate_x->on_pointer_move, 185.0, 74.0);
  check(translated.size() == 1 && translated.back().first == 10.0 &&
            translated.back().second == 0.0,
        "world translation snaps and constrains the x axis");
  emit_pointer(translate_x->on_pointer_up, 185.0, 74.0, 2.0);
  check(began == 1 && ended == 0,
        "duplicate down and foreign up preserve one active bracket");
  emit_pointer(translate_x->on_pointer_up, 185.0, 74.0);
  emit_pointer(translate_x->on_pointer_up, 185.0, 74.0);
  check(began == 1 && ended == 1,
        "matching up closes one transform bracket exactly once");

  use_single_origin_node(context, 90.0);
  gizmo::set_gizmo_space<Node>(context.state, flight::String("local"));
  gizmo::set_gizmo_snap_translate<Node>(context.state, 0.0);
  gizmo::update_gizmo<Node>(context.state);
  emit_pointer(translate_x->on_pointer_down, 100.0, 122.0);
  emit_pointer(translate_x->on_pointer_move, 100.0, 135.0);
  check_close(translated.back().first, 0.0,
              "local x translation rotates into world y (x component)");
  check_close(translated.back().second, 13.0,
              "local x translation rotates into world y (y component)");
  emit_pointer(translate_x->on_pointer_up, 100.0, 135.0);

  gizmo::set_gizmo_mode<Node>(context.state, flight::String("rotate"));
  gizmo::set_gizmo_space<Node>(context.state, flight::String("world"));
  gizmo::set_gizmo_snap_rotation<Node>(context.state, 15.0);
  gizmo::update_gizmo<Node>(context.state);
  auto rotate = gizmo::enable_gizmo_interaction_signals(
      find_handle(context, flight::String("rotate"))->node);
  emit_pointer(rotate->on_pointer_down, 152.0, 50.0);
  emit_pointer(rotate->on_pointer_move, 100.0, 102.0);
  check(rotated.size() == 1 && rotated.back() == 90.0,
        "rotation emits snapped degrees");
  emit_pointer(rotate->on_pointer_up, 100.0, 102.0);

  gizmo::set_gizmo_snap_rotation<Node>(context.state, 0.0);
  constexpr double radius = 52.0;
  const double start = 170.0 * std::numbers::pi / 180.0;
  const double current = -170.0 * std::numbers::pi / 180.0;
  emit_pointer(rotate->on_pointer_down,
               100.0 + std::cos(start) * radius,
               50.0 + std::sin(start) * radius);
  emit_pointer(rotate->on_pointer_move,
               100.0 + std::cos(current) * radius,
               50.0 + std::sin(current) * radius);
  check_close(rotated.back(), 20.0,
              "rotation normalizes across the positive/negative seam");
  emit_pointer(rotate->on_pointer_up, 100.0, 50.0);

  gizmo::set_gizmo_mode<Node>(context.state, flight::String("scale"));
  gizmo::set_gizmo_snap_scale<Node>(context.state, 0.25);
  gizmo::update_gizmo<Node>(context.state);
  auto scale_x = gizmo::enable_gizmo_interaction_signals(
      find_handle(context, flight::String("scale-east"))->node);
  emit_pointer(scale_x->on_pointer_down, 172.0, 50.0);
  emit_pointer(scale_x->on_pointer_move, 208.0, 50.0, 2.0);
  check(scaled.empty(), "scale ignores traffic from another pointer identity");
  emit_pointer(scale_x->on_pointer_move, 208.0, 50.0);
  check(scaled.size() == 1 && scaled.back().first == 1.5 &&
            scaled.back().second == 1.0,
        "axis scale emits a snapped multiplier and neutral orthogonal scale");
  emit_pointer(scale_x->on_pointer_up, 208.0, 50.0);
  emit_pointer(scale_x->on_pointer_down, 172.0, 50.0);
  emit_pointer(scale_x->on_pointer_move, 208.0, 50.0, 1.0, true);
  check(scaled.back().first == 1.5 && scaled.back().second == 1.5,
        "shift makes axis scaling uniform");

  const int ended_before_mode_change = ended;
  gizmo::set_gizmo_mode<Node>(context.state, flight::String("translate"));
  check(ended == ended_before_mode_change + 1,
        "switching transform modes cancels the active bracket");
  gizmo::set_gizmo_mode<Node>(context.state, flight::String("translate"));
  check(ended == ended_before_mode_change + 1,
        "setting the same mode leaves bracket state unchanged");

  gizmo::set_gizmo_mode<Node>(context.state, flight::String("none"));
  gizmo::update_gizmo<Node>(context.state);
  check(flight::row_get<flight::RowKey<"visible">>(context.runtime->overlay_root) &&
            !flight::row_get<flight::RowKey<"visible">>(context.runtime->handle_root) &&
            context.runtime->outline->visible,
        "none mode hides handles but preserves the selection outline");
  selection::clear_selection<Node>(context.selection);
  gizmo::update_gizmo<Node>(context.state);
  check(!flight::row_get<flight::RowKey<"visible">>(context.runtime->overlay_root),
        "empty selection hides the owned overlay");

  gizmo::dispose_gizmo_state<Node>(context.state);
}

}  // namespace

int main() {
  test_alignment_and_guides();
  test_state_creation_and_layout();
  test_interaction_state_and_math();
  if (failures != 0) {
    std::cerr << failures << " gizmo test(s) failed\n";
    return 1;
  }
  std::cout << "generated gizmo tests passed\n";
  return 0;
}
