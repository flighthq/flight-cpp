#include <flight/interaction/node_interaction_state.hpp>
#include <flight/interaction/spatial_query.hpp>

#include <iostream>

// Mutation evidence (2026-10-13): the focused assertions killed replacing the
// caller output with a fresh array and inverting the circle disabled-root guard.

namespace {

bool check(bool condition, const char* message) {
  if (!condition) std::cerr << message << '\n';
  return condition;
}

flight::types::Node2D make_node2d(bool enabled) {
  return flight::make_structural_ref<flight::types::Node2D::schema_type>(
      flight::row_field<flight::RowKey<"enabled">>(enabled),
      flight::row_field<flight::RowKey<"kind">>(flight::String("DisplayObject")),
      flight::row_field<flight::RowKey<"name">>(std::optional<flight::String>{}),
      flight::row_field<flight::RowKey<"alpha">>(1.0),
      flight::row_field<flight::RowKey<"visible">>(true),
      flight::row_field<flight::RowKey<"blendMode">>(std::optional<flight::String>{}),
      flight::row_field<flight::RowKey<"clip">>(
          std::optional<flight::Ref<flight::types::ClipRegion>>{}),
      flight::row_field<flight::RowKey<"material">>(
          std::optional<flight::Ref<flight::types::Material2D>>{}),
      flight::row_field<flight::RowKey<"materialData">>(
          std::optional<flight::Ref<flight::types::MaterialData>>{}),
      flight::row_field<flight::RowKey<"pivotX">>(0.0),
      flight::row_field<flight::RowKey<"pivotY">>(0.0),
      flight::row_field<flight::RowKey<"rotation">>(0.0),
      flight::row_field<flight::RowKey<"scaleX">>(1.0),
      flight::row_field<flight::RowKey<"scaleY">>(1.0),
      flight::row_field<flight::RowKey<"skewX">>(0.0),
      flight::row_field<flight::RowKey<"skewY">>(0.0),
      flight::row_field<flight::RowKey<"x">>(0.0),
      flight::row_field<flight::RowKey<"y">>(0.0));
}

auto rectangle_view(double x, double y, double width, double height) {
  auto rectangle = flight::make_ref<flight::types::Rectangle>();
  rectangle->x = x;
  rectangle->y = y;
  rectangle->width = width;
  rectangle->height = height;
  auto writable = flight::StructuralRef<flight::RowWritable<
      flight::RowOf<flight::Ref<flight::types::Rectangle>>>>(rectangle);
  return flight::structural_ref_cast<flight::StructuralRef<flight::RowReadonly<
      flight::RowOf<flight::Ref<flight::types::Rectangle>>>>>(writable);
}

} // namespace

int main() {
  using flight::String;
  using flight::types::Node;
  using flight::types::NodeAny;
  using flight::types::NodeRuntime;

  const auto runtime = flight::make_ref<NodeRuntime<flight::Any>>(NodeRuntime<flight::Any>{});
  const NodeAny node = flight::make_ref<Node<flight::Any>>(Node<flight::Any>{
      .enabled = true,
      .kind = String("Node"),
      .entity_runtime_key = runtime,
  });

  if (!check(!flight::interaction::get_node_interaction_state(node).has_value(),
             "fresh node unexpectedly has interaction state") ||
      !check(flight::interaction::are_node_children_hit_test_enabled(node),
             "fresh node did not retain the children-enabled default") ||
      !check(!flight::interaction::is_node_focusable(node),
             "fresh node unexpectedly focusable") ||
      !check(!flight::interaction::is_node_hit_test_enabled(node),
             "fresh node unexpectedly hit-testable") ||
      !check(!flight::interaction::is_node_pointer_double_click_enabled(node),
             "fresh node unexpectedly double-clickable") ||
      !check(flight::interaction::get_node_tab_index(node) == -1.0,
             "fresh node did not retain the natural tab order") ||
      !check(!flight::interaction::get_node_cursor(node).has_value(),
             "fresh node unexpectedly has a cursor") ||
      !check(!flight::interaction::get_node_hit_area(node).has_value(),
             "fresh node unexpectedly has a hit area")) {
    return 1;
  }

  const auto state = flight::interaction::enable_node_interaction_state(node);
  if (!check(state == flight::interaction::enable_node_interaction_state(node),
             "enable replaced an existing interaction state") ||
      !check(runtime->interaction_state == state,
             "enable did not retain state in the node runtime")) {
    return 1;
  }

  flight::interaction::set_node_children_hit_test_enabled(node, false);
  flight::interaction::set_node_cursor(node, String("pointer"));
  flight::interaction::set_node_focusable(node, true);
  flight::interaction::set_node_hit_area(node, flight::types::HitArea{String("bounds")});
  flight::interaction::set_node_hit_test_enabled(node, true);
  flight::interaction::set_node_pointer_double_click_enabled(node, true);
  flight::interaction::set_node_tab_index(node, 4.0);

  const auto cursor = flight::interaction::get_node_cursor(node);
  const auto hit_area = flight::interaction::get_node_hit_area(node);
  if (!check(!flight::interaction::are_node_children_hit_test_enabled(node),
             "children-enabled setter was not visible") ||
      !check(cursor == String("pointer"), "cursor setter was not visible") ||
      !check(flight::interaction::is_node_focusable(node),
             "focusable setter was not visible") ||
      !check(hit_area.has_value() && std::get<String>(*hit_area) == String("bounds"),
             "hit-area setter was not visible") ||
      !check(flight::interaction::is_node_hit_test_enabled(node),
             "hit-test setter was not visible") ||
      !check(flight::interaction::is_node_pointer_double_click_enabled(node),
             "double-click setter was not visible") ||
      !check(flight::interaction::get_node_tab_index(node) == 4.0,
             "tab-index setter was not visible")) {
    return 1;
  }

  flight::interaction::set_node_cursor(node, std::nullopt);
  flight::interaction::set_node_hit_area(node, std::nullopt);
  if (!check(!flight::interaction::get_node_cursor(node).has_value(),
             "cursor clear did not restore absence") ||
      !check(!flight::interaction::get_node_hit_area(node).has_value(),
             "hit-area clear did not restore absence")) {
    return 1;
  }

  const auto disabled = make_node2d(false);
  const auto query_rectangle = rectangle_view(0.0, 0.0, 50.0, 50.0);
  flight::Array<flight::types::Node2D> seeded{disabled};
  const auto seeded_identity = seeded.identity();
  const auto disabled_rectangle_result = flight::interaction::hit_test_area_query(
      disabled, query_rectangle, seeded);
  const auto disabled_circle_result = flight::interaction::hit_test_area_query_circle(
      disabled, 20.0, 20.0, 30.0);
  if (!check(disabled_rectangle_result.identity() == seeded_identity &&
                 disabled_rectangle_result.size() == 1 &&
                 disabled_rectangle_result[0] == disabled,
             "disabled spatial query did not preserve the caller output") ||
      !check(disabled_circle_result.empty(),
             "disabled circle query did not return an empty default output")) {
    return 1;
  }

  const auto enabled = make_node2d(true);
  flight::Array<flight::types::Node2D> untouched{disabled};
  const auto untouched_identity = untouched.identity();
  bool rectangle_failed_loudly = false;
  bool circle_failed_loudly = false;
  try {
    static_cast<void>(flight::interaction::hit_test_area_query(
        enabled, query_rectangle, untouched));
  } catch (const flight::Error& error) {
    rectangle_failed_loudly =
        error.message() == flight::String(
            "World bounds require the unavailable checked HasBoundsRectangleRuntime owner");
  }
  try {
    static_cast<void>(flight::interaction::hit_test_area_query_circle(
        enabled, 20.0, 20.0, 30.0));
  } catch (const flight::Error& error) {
    circle_failed_loudly =
        error.message() == flight::String(
            "World bounds require the unavailable checked HasBoundsRectangleRuntime owner");
  }
  if (!check(rectangle_failed_loudly && circle_failed_loudly,
             "enabled spatial queries did not expose the exact bounds-owner gap") ||
      !check(untouched.identity() == untouched_identity && untouched.size() == 1 &&
                 untouched[0] == disabled,
             "failed spatial query mutated the caller output") ||
      !check(flight::row_get<flight::RowKey<"enabled">>(enabled) &&
                 flight::row_get<flight::RowKey<"x">>(query_rectangle) == 0.0 &&
                 flight::row_get<flight::RowKey<"width">>(query_rectangle) == 50.0,
             "failed spatial query mutated its inputs")) {
    return 1;
  }

  return 0;
}
