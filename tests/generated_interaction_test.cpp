#include <flight/interaction/node_interaction_state.hpp>
#include <flight/interaction/node_interactive_state_binding.hpp>
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
  const NodeAny node = flight::make_ref<Node<flight::Any>>();
  node->enabled = true;
  node->kind = String("Node");
  node->entity_runtime_key = runtime;

  flight::Ref<flight::types::NodeTraits> traits = node;
  if (!check(traits.get() == node.get() && !traits.owner_before(node) &&
                 !node.owner_before(traits) && &traits->enabled == &node->enabled &&
                 &traits->kind == &node->kind,
             "declared NodeTraits widening replaced owner or inherited cells")) return 1;
  traits->enabled = false;
  if (!check(!node->enabled, "base trait write did not reach the original node")) return 1;
  traits->enabled = true;

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

  using ReadonlyNode = flight::interaction::ReadonlyInteractionNode;
  using StateReader = std::optional<flight::Ref<flight::types::NodeInteractionState>> (*)(ReadonlyNode);
  const StateReader read_state = &flight::interaction::get_node_interaction_state;
  bool (*const read_children)(ReadonlyNode) = &flight::interaction::are_node_children_hit_test_enabled;
  std::optional<flight::types::Cursor> (*const read_cursor)(ReadonlyNode) = &flight::interaction::get_node_cursor;
  std::optional<flight::types::HitArea> (*const read_hit_area)(ReadonlyNode) = &flight::interaction::get_node_hit_area;
  double (*const read_tab_index)(ReadonlyNode) = &flight::interaction::get_node_tab_index;
  bool (*const read_focusable)(ReadonlyNode) = &flight::interaction::is_node_focusable;
  bool (*const read_hit_enabled)(ReadonlyNode) = &flight::interaction::is_node_hit_test_enabled;
  bool (*const read_double_click)(ReadonlyNode) = &flight::interaction::is_node_pointer_double_click_enabled;
  const ReadonlyNode readonly_node(node);
  if (!check(!read_state(readonly_node).has_value() && read_children(readonly_node) &&
                 !read_cursor(readonly_node).has_value() && !read_hit_area(readonly_node).has_value() &&
                 read_tab_index(readonly_node) == -1.0 && !read_focusable(readonly_node) &&
                 !read_hit_enabled(readonly_node) && !read_double_click(readonly_node),
             "canonical readonly callable readers lost the absent-state defaults")) {
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

  if (!check(read_state(readonly_node).value() == state &&
                 runtime->interaction_state.value() == state,
             "readonly reader replaced the node runtime or interaction state owner") ||
      !check(!read_children(readonly_node) && read_cursor(readonly_node) == String("pointer") &&
                 read_hit_area(readonly_node).has_value() && read_focusable(readonly_node) &&
                 read_hit_enabled(readonly_node) && read_double_click(readonly_node) &&
                 read_tab_index(readonly_node) == 4.0,
             "canonical readonly readers did not observe writes through the same native owner")) {
    return 1;
  }

  auto rectangle_hit = rectangle_view(1.0, 2.0, 3.0, 4.0);
  flight::interaction::set_node_hit_area(node, flight::types::HitArea{rectangle_hit});
  const auto returned_hit = read_hit_area(readonly_node);
  if (!check(returned_hit.has_value() &&
                 std::holds_alternative<decltype(rectangle_hit)>(*returned_hit) &&
                 std::get<decltype(rectangle_hit)>(*returned_hit).identity() == rectangle_hit.identity(),
             "readonly hit-area reader replaced the original rectangle owner")) return 1;

  flight::interaction::set_node_cursor(node, std::nullopt);
  flight::interaction::set_node_hit_area(node, std::nullopt);
  if (!check(!flight::interaction::get_node_cursor(node).has_value(),
             "cursor clear did not restore absence") ||
      !check(!flight::interaction::get_node_hit_area(node).has_value(),
             "hit-area clear did not restore absence")) {
    return 1;
  }

  if (!check(!read_cursor(readonly_node).has_value() && !read_hit_area(readonly_node).has_value(),
             "readonly readers did not preserve cleared optional values")) {
    return 1;
  }

  const NodeAny no_runtime = flight::make_ref<Node<flight::Any>>();
  no_runtime->enabled = true;
  no_runtime->kind = String("Node");
  bool missing_read_runtime_failed = false;
  bool missing_enable_runtime_failed = false;
  try {
    static_cast<void>(read_state(ReadonlyNode(no_runtime)));
  } catch (const flight::TypeError& error) {
    missing_read_runtime_failed = error.message() == String(
        "Cannot read properties of undefined (reading 'interactionState')");
  }
  try {
    static_cast<void>(flight::interaction::enable_node_interaction_state(no_runtime));
  } catch (const flight::TypeError& error) {
    missing_enable_runtime_failed = error.message() == String(
        "Cannot read properties of undefined (reading 'interactionState')");
  }
  if (!check(missing_read_runtime_failed && missing_enable_runtime_failed &&
                 !no_runtime->entity_runtime_key.has_value(),
             "missing runtime lost the source TypeError or fabricated a runtime owner")) {
    return 1;
  }

  // Closed phase names do not constrain the extension record's own keys.
  using InteractiveState = flight::types::FlightDocumentInteractiveState;
  using InteractiveStates = flight::types::FlightDocumentInteractiveStates;
  using Flags = flight::types::NodeInteractiveStateFlags;
  using ReadonlyStates = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<InteractiveStates>>>>;
  using ReadonlyFlags = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<Flags>>>>;
  using ReadonlyState = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<InteractiveState>>>>;
  const auto phase_states = flight::make_ref<InteractiveStates>();
  const auto hover = flight::make_ref<InteractiveState>();
  const auto pressed = flight::make_ref<InteractiveState>();
  const auto extension = [](const char* kind, std::initializer_list<const char*> keys) {
    auto result = flight::make_ref<flight::types::FlightDocumentInteractiveStateExtensionDescriptor>();
    result->kind = String(kind);
    for (const auto* key : keys) result->fields.set(String(key), flight::types::FlightDocumentValue{});
    return result;
  };
  hover->extensions.push(extension("custom-kind", {"free.key", "shared"}));
  pressed->extensions.push(extension("custom-kind", {"shared", "another-key"}));
  pressed->extensions.push(extension("other-kind", {"unlisted"}));
  phase_states->hover = hover;
  phase_states->pressed = pressed;
  const ReadonlyStates readonly_states(phase_states);
  const auto flags = flight::make_ref<Flags>(Flags{.disabled = false, .hovered = true, .pressed = true});
  const ReadonlyFlags readonly_flags(flags);
  const auto layers = flight::interaction::get_active_layers(readonly_states, readonly_flags);
  const auto names = flight::interaction::collect_extension_field_names(readonly_states);
  const auto custom_names = names.get(String("custom-kind"));
  const auto other_names = names.get(String("other-kind"));
  if (!check(layers.size() == 2 && layers[0].identity() == ReadonlyState(hover).identity() &&
                 layers[1].identity() == ReadonlyState(pressed).identity(),
             "active layers lost order or original state owner identity") ||
      !check(custom_names.has_value() && custom_names->size() == 3 &&
                 (*custom_names)[0] == String("free.key") && (*custom_names)[1] == String("shared") &&
                 (*custom_names)[2] == String("another-key") && other_names.has_value() &&
                 other_names->size() == 1 && (*other_names)[0] == String("unlisted"),
             "fixed phase dispatch narrowed open extension keys or lost ordered deduplication") ||
      !check(flight::interaction::same_flags(readonly_flags, readonly_flags),
             "same flags rejected matching readonly fields")) return 1;
  flags->disabled = true;
  if (!check(flight::interaction::get_active_layers(readonly_states, readonly_flags).empty(),
             "null disabled phase incorrectly fell through to hover/pressed")) return 1;
  phase_states->disabled = pressed;
  const auto disabled_layers = flight::interaction::get_active_layers(readonly_states, readonly_flags);
  const auto different_flags = flight::make_ref<Flags>(Flags{.disabled = true, .hovered = false, .pressed = true});
  if (!check(disabled_layers.size() == 1 && disabled_layers[0].identity() == ReadonlyState(pressed).identity(),
             "disabled state lost its precedence or owner identity") ||
      !check(!flight::interaction::same_flags(readonly_flags, ReadonlyFlags(different_flags)),
             "same flags ignored a changed flag") ||
      !check(!phase_states->hover.value()->extensions[0]->fields.has(String("another-key")),
             "field collection mutated an extension record")) return 1;

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
