// Derived from @flighthq/interaction/packages/interaction/src/nodeInteractionState.ts.
#pragma once

#include <optional>
#include <utility>

#include <flight/runtime.hpp>
#include <flight/entity/entity.hpp>
#include <flight/types/cursor.hpp>
#include <flight/types/entity.hpp>
#include <flight/types/node.hpp>
#include <flight/types/node2_d.hpp>
#include <flight/types/node_interaction.hpp>
#include <flight/types/node_interaction_state.hpp>

static_assert(
    flight::runtime_contract.compiler_contract == "flight-runtime-contract/2",
    "Flight compiler/runtime contract mismatch");
static_assert(
    flight::runtime_contract.cpp_abi == 1,
    "Flight C++ runtime ABI mismatch");

namespace flight::interaction {

using flight::types::Cursor;
using flight::types::EntityConstruction;
using flight::types::HitArea;
using flight::types::NodeAny;
using flight::types::Node2D;
using flight::types::NodeInteractionState;

inline std::optional<flight::Ref<NodeInteractionState>> get_node_interaction_state(NodeAny source) {
  return source->entity_runtime_key.value()->interaction_state;
}

// Node2D is an owner-preserving structural view rather than Node<Any>. Read its exact
// NodeRuntime<Node2DTraits> cell through the registered EntityRuntime symbol so GUI callers do not
// have to retype the owner or materialize a second node.
inline flight::Ref<flight::types::NodeRuntime<flight::Ref<flight::types::Node2DTraits>>>
get_node2_dinteraction_runtime(Node2D source) {
  using Runtime = flight::Ref<flight::types::NodeRuntime<flight::Ref<flight::types::Node2DTraits>>>;
  return flight::row_get<std::optional<Runtime>>(source, flight::types::entity_runtime_key).value();
}

inline std::optional<flight::Ref<NodeInteractionState>> get_node_interaction_state(Node2D source) {
  return get_node2_dinteraction_runtime(source)->interaction_state;
}

inline bool are_node_children_hit_test_enabled(NodeAny source) {
  const auto state = get_node_interaction_state(source);
  return !state.has_value() || state.value()->children_hit_test_enabled;
}

inline std::optional<Cursor> get_node_cursor(NodeAny source) {
  const auto state = get_node_interaction_state(source);
  return state.has_value() ? state.value()->cursor : std::nullopt;
}

inline std::optional<HitArea> get_node_hit_area(NodeAny source) {
  const auto state = get_node_interaction_state(source);
  return state.has_value() ? state.value()->hit_area : std::nullopt;
}

inline std::optional<HitArea> get_node_hit_area(Node2D source) {
  const auto state = get_node_interaction_state(source);
  return state.has_value() ? state.value()->hit_area : std::nullopt;
}

inline double get_node_tab_index(NodeAny source) {
  const auto state = get_node_interaction_state(source);
  return state.has_value() ? state.value()->tab_index : -1.0;
}

inline void initialize_node_interaction_state(
    flight::Ref<EntityConstruction<flight::Ref<NodeInteractionState>>> out) {
  out->children_hit_test_enabled = true;
  out->cursor = std::nullopt;
  out->focusable = false;
  out->hit_area = std::nullopt;
  out->hit_test_enabled = false;
  out->pointer_double_click_enabled = false;
  out->tab_index = -1.0;
}

inline flight::Ref<NodeInteractionState> create_node_interaction_state() {
  auto out = flight::entity::allocate_entity<flight::Ref<NodeInteractionState>>();
  initialize_node_interaction_state(out);
  return flight::entity::finish_entity(out);
}

inline flight::Ref<NodeInteractionState> enable_node_interaction_state(NodeAny source) {
  auto runtime = source->entity_runtime_key.value();
  if (!runtime->interaction_state.has_value()) {
    runtime->interaction_state = create_node_interaction_state();
  }
  return runtime->interaction_state.value();
}

inline flight::Ref<NodeInteractionState> enable_node_interaction_state(Node2D source) {
  auto runtime = get_node2_dinteraction_runtime(source);
  if (!runtime->interaction_state.has_value()) {
    runtime->interaction_state = create_node_interaction_state();
  }
  return runtime->interaction_state.value();
}

inline bool is_node_focusable(NodeAny source) {
  const auto state = get_node_interaction_state(source);
  return state.has_value() && state.value()->focusable;
}

inline bool is_node_hit_test_enabled(NodeAny source) {
  const auto state = get_node_interaction_state(source);
  return state.has_value() && state.value()->hit_test_enabled;
}

inline bool is_node_hit_test_enabled(Node2D source) {
  const auto state = get_node_interaction_state(source);
  return state.has_value() && state.value()->hit_test_enabled;
}

inline bool is_node_pointer_double_click_enabled(NodeAny source) {
  const auto state = get_node_interaction_state(source);
  return state.has_value() && state.value()->pointer_double_click_enabled;
}

inline void set_node_children_hit_test_enabled(NodeAny source, bool enabled) {
  enable_node_interaction_state(source)->children_hit_test_enabled = enabled;
}

inline void set_node_cursor(NodeAny source, std::optional<Cursor> cursor) {
  enable_node_interaction_state(source)->cursor = std::move(cursor);
}

inline void set_node_focusable(NodeAny source, bool focusable) {
  enable_node_interaction_state(source)->focusable = focusable;
}

inline void set_node_hit_area(NodeAny source, std::optional<HitArea> hit_area) {
  enable_node_interaction_state(source)->hit_area = std::move(hit_area);
}

inline void set_node_hit_area(Node2D source, std::optional<HitArea> hit_area) {
  enable_node_interaction_state(source)->hit_area = std::move(hit_area);
}

inline void set_node_hit_test_enabled(NodeAny source, bool enabled) {
  enable_node_interaction_state(source)->hit_test_enabled = enabled;
}

inline void set_node_hit_test_enabled(Node2D source, bool enabled) {
  enable_node_interaction_state(source)->hit_test_enabled = enabled;
}

inline void set_node_pointer_double_click_enabled(NodeAny source, bool enabled) {
  enable_node_interaction_state(source)->pointer_double_click_enabled = enabled;
}

inline void set_node_tab_index(NodeAny source, double tab_index) {
  enable_node_interaction_state(source)->tab_index = tab_index;
}

} // namespace flight::interaction
