#include <flight/interaction/node_interaction_state.hpp>

#include <iostream>

namespace {

bool check(bool condition, const char* message) {
  if (!condition) std::cerr << message << '\n';
  return condition;
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

  return 0;
}
