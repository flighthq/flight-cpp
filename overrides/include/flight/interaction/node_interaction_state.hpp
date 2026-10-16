// Derived from the a627849 nodeInteractionState.ts emission; retains native compatibility and exact readonly row queries.
#pragma once

#include <optional>
#include <utility>
#include <variant>
#include <flight/structural_ref.hpp>

#include <flight/runtime.hpp>
#include <flight/entity/entity.hpp>
#include <flight/types/cursor.hpp>
#include <flight/types/entity.hpp>
#include <flight/types/node.hpp>
#include <flight/types/node_2d.hpp>
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

using ReadonlyInteractionNode = flight::StructuralRef<
    flight::RowReadonly<flight::RowOf<flight::types::NodeAny>>>;

// Read the declared NodeRuntime<Any> cell on this same readonly row owner.
inline std::optional<flight::Ref<NodeInteractionState>> get_node_interaction_state(
    ReadonlyInteractionNode source) {
  using Runtime = flight::Ref<flight::types::NodeRuntime<flight::Any>>;
  const auto runtime = flight::row_get<std::optional<Runtime>>(
      source, flight::types::entity_runtime_key);
  if (!runtime.has_value()) {
    throw flight::TypeError(flight::String(
        "Cannot read properties of undefined (reading 'interactionState')"));
  }
  return runtime.value()->interaction_state;
}

inline bool are_node_children_hit_test_enabled(flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::types::NodeAny>>> source) {
  return ([&]() -> bool { auto nullish_coalesce_left = ([&]() -> std::optional<bool> { auto optional_chain_receiver = get_node_interaction_state(source); if (!optional_chain_receiver.has_value()) return std::nullopt; return optional_chain_receiver.value()->children_hit_test_enabled; }()); if (nullish_coalesce_left.has_value()) return nullish_coalesce_left.value(); return true; }());
}

inline std::optional<flight::String> get_node_cursor(flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::types::NodeAny>>> source) {
  return ([&]() -> std::optional<flight::String> { auto nullish_coalesce_left = ([&]() -> std::variant<flight::String, flight::Null, flight::Undefined> { auto optional_chain_receiver = get_node_interaction_state(source); if (!optional_chain_receiver.has_value()) return std::variant<flight::String, flight::Null, flight::Undefined>{std::in_place_type<flight::Undefined>, flight::undefined}; return ([&]() -> std::variant<flight::String, flight::Null, flight::Undefined> { auto optional_chain_projected = optional_chain_receiver.value()->cursor; if (!optional_chain_projected.has_value()) return std::variant<flight::String, flight::Null, flight::Undefined>{std::in_place_type<flight::Null>, flight::null}; return std::variant<flight::String, flight::Null, flight::Undefined>{std::in_place_type<flight::String>, optional_chain_projected.value()}; }()); }()); if (const auto* alternative = std::get_if<flight::String>(&nullish_coalesce_left)) return std::optional<flight::String>{*alternative}; return std::nullopt; }());
}

inline std::optional<flight::types::HitArea> get_node_hit_area(flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::types::NodeAny>>> source) {
  return ([&]() -> std::optional<flight::types::HitArea> { auto optional_chain_receiver = get_node_interaction_state(source); if (!optional_chain_receiver.has_value()) return std::nullopt; return optional_chain_receiver.value()->hit_area; }());
}

inline double get_node_tab_index(flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::types::NodeAny>>> source) {
  return ([&]() -> double { auto nullish_coalesce_left = ([&]() -> std::optional<double> { auto optional_chain_receiver = get_node_interaction_state(source); if (!optional_chain_receiver.has_value()) return std::nullopt; return optional_chain_receiver.value()->tab_index; }()); if (nullish_coalesce_left.has_value()) return nullish_coalesce_left.value(); return -1.0; }());
}

inline bool is_node_focusable(flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::types::NodeAny>>> source) {
  return ([&]() -> bool { auto nullish_coalesce_left = ([&]() -> std::optional<bool> { auto optional_chain_receiver = get_node_interaction_state(source); if (!optional_chain_receiver.has_value()) return std::nullopt; return optional_chain_receiver.value()->focusable; }()); if (nullish_coalesce_left.has_value()) return nullish_coalesce_left.value(); return false; }());
}

inline bool is_node_hit_test_enabled(flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::types::NodeAny>>> source) {
  return ([&]() -> bool { auto nullish_coalesce_left = ([&]() -> std::optional<bool> { auto optional_chain_receiver = get_node_interaction_state(source); if (!optional_chain_receiver.has_value()) return std::nullopt; return optional_chain_receiver.value()->hit_test_enabled; }()); if (nullish_coalesce_left.has_value()) return nullish_coalesce_left.value(); return false; }());
}

inline bool is_node_pointer_double_click_enabled(flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::types::NodeAny>>> source) {
  return ([&]() -> bool { auto nullish_coalesce_left = ([&]() -> std::optional<bool> { auto optional_chain_receiver = get_node_interaction_state(source); if (!optional_chain_receiver.has_value()) return std::nullopt; return optional_chain_receiver.value()->pointer_double_click_enabled; }()); if (nullish_coalesce_left.has_value()) return nullish_coalesce_left.value(); return false; }());
}

inline std::optional<flight::Ref<NodeInteractionState>> get_node_interaction_state(NodeAny source) {
  return get_node_interaction_state(ReadonlyInteractionNode(source));
}

// Node2D is an owner-preserving structural view rather than Node<Any>. Read its exact
// NodeRuntime<Node2DTraits> cell through the registered EntityRuntime symbol so GUI callers do not
// have to retype the owner or materialize a second node.
inline flight::Ref<flight::types::NodeRuntime<flight::Ref<flight::types::Node2DTraits>>>
get_node2_dinteraction_runtime(Node2D source) {
  using Runtime = flight::Ref<flight::types::NodeRuntime<flight::Ref<flight::types::Node2DTraits>>>;
  const auto runtime = flight::row_get<std::optional<Runtime>>(source, flight::types::entity_runtime_key);
  if (!runtime.has_value()) {
    throw flight::TypeError(flight::String(
        "Cannot read properties of undefined (reading 'interactionState')"));
  }
  return runtime.value();
}

inline std::optional<flight::Ref<NodeInteractionState>> get_node_interaction_state(Node2D source) {
  return get_node2_dinteraction_runtime(source)->interaction_state;
}

template <typename Source>
  requires requires(const Source& source) {
    source->entity_runtime_key.value()->interaction_state;
  }
inline std::optional<flight::Ref<NodeInteractionState>> get_node_interaction_state(
    const Source& source) {
  if (!source) return std::nullopt;
  if (!source->entity_runtime_key.has_value()) {
    throw flight::TypeError(flight::String(
        "Cannot read properties of undefined (reading 'interactionState')"));
  }
  return source->entity_runtime_key.value()->interaction_state;
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
  if (!source->entity_runtime_key.has_value()) {
    throw flight::TypeError(flight::String(
        "Cannot read properties of undefined (reading 'interactionState')"));
  }
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

template <typename Source>
  requires requires(const Source& source) {
    source->entity_runtime_key.value()->interaction_state;
  }
inline flight::Ref<NodeInteractionState> enable_node_interaction_state(const Source& source) {
  if (!source) {
    throw flight::Error(flight::String("Node interaction runtime is unavailable"));
  }
  if (!source->entity_runtime_key.has_value()) {
    throw flight::TypeError(flight::String(
        "Cannot read properties of undefined (reading 'interactionState')"));
  }
  auto runtime = source->entity_runtime_key.value();
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

template <typename Source>
  requires requires(const Source& source) {
    source->entity_runtime_key.value()->interaction_state;
  }
inline bool is_node_hit_test_enabled(const Source& source) {
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

template <typename Source>
  requires requires(const Source& source) {
    source->entity_runtime_key.value()->interaction_state;
  }
inline void set_node_cursor(const Source& source, std::optional<Cursor> cursor) {
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

template <typename Source>
  requires requires(const Source& source) {
    source->entity_runtime_key.value()->interaction_state;
  }
inline void set_node_hit_test_enabled(const Source& source, bool enabled) {
  enable_node_interaction_state(source)->hit_test_enabled = enabled;
}

inline void set_node_pointer_double_click_enabled(NodeAny source, bool enabled) {
  enable_node_interaction_state(source)->pointer_double_click_enabled = enabled;
}

inline void set_node_tab_index(NodeAny source, double tab_index) {
  enable_node_interaction_state(source)->tab_index = tab_index;
}

} // namespace flight::interaction
