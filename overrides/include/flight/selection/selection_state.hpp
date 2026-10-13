// Complete native implementation of Flight a627849 selectionState.ts.
#pragma once

#include <functional>
#include <optional>
#include <stdexcept>

#include <flight/signals/emitter.hpp>
#include <flight/signals/signal.hpp>
#include <flight/structural_ref.hpp>
#include <flight/types/selection_state.hpp>
#include <flight/types/selection_state_runtime.hpp>

namespace flight::selection {

template <typename NodeType>
inline flight::Ref<flight::types::SelectionStateRuntime<NodeType>> get_selection_state_runtime(
    const flight::Ref<flight::types::SelectionState<NodeType>>& state) {
  if (!state->entity_runtime_key.has_value()) throw std::bad_cast();
  return std::static_pointer_cast<flight::types::SelectionStateRuntime<NodeType>>(
      state->entity_runtime_key.value());
}

template <typename NodeType = flight::types::NodeAny>
inline flight::Ref<flight::types::SelectionState<NodeType>> create_selection_state() {
  using ActiveSlot = std::function<void(std::optional<NodeType>)>;
  using ChangeSlot = std::function<void(flight::Array<NodeType>)>;

  auto signals = flight::make_ref<flight::types::SelectionSignals<NodeType>>();
  signals->on_active_change = flight::signals::create_signal<ActiveSlot>();
  signals->on_change = flight::signals::create_signal<ChangeSlot>();

  auto runtime = flight::make_ref<flight::types::SelectionStateRuntime<NodeType>>();
  runtime->binding = std::nullopt;
  runtime->uid = std::nullopt;
  runtime->active_node = std::nullopt;
  runtime->selected_node_set = flight::Set<NodeType>();
  runtime->selected_nodes = flight::Array<NodeType>{};
  runtime->signals = signals;

  auto state = flight::make_ref<flight::types::SelectionState<NodeType>>();
  state->entity_runtime_key = runtime;
  return state;
}

template <typename NodeType>
inline void emit_selection_changes(
    flight::Ref<flight::types::SelectionStateRuntime<NodeType>> runtime,
    bool selection_changed,
    std::optional<NodeType> previous_active_node) {
  if (selection_changed && runtime->signals->on_change->data.has_value()) {
    flight::signals::emit_signal<std::function<void(flight::Array<NodeType>)>>(
        runtime->signals->on_change, runtime->selected_nodes.slice());
  }
  if (previous_active_node != runtime->active_node) {
    flight::signals::emit_signal<std::function<void(std::optional<NodeType>)>>(
        runtime->signals->on_active_change, runtime->active_node);
  }
}

template <typename NodeType>
inline void add_node_to_selection(
    flight::Ref<flight::types::SelectionState<NodeType>> state, NodeType node) {
  auto runtime = get_selection_state_runtime<NodeType>(state);
  if (runtime->selected_node_set.has(node)) return;
  auto previous_active_node = runtime->active_node;
  runtime->selected_nodes.push(node);
  runtime->selected_node_set.add(node);
  runtime->active_node = node;
  emit_selection_changes(runtime, true, previous_active_node);
}

template <typename NodeType>
inline void clear_selection(flight::Ref<flight::types::SelectionState<NodeType>> state) {
  auto runtime = get_selection_state_runtime<NodeType>(state);
  if (runtime->selected_nodes.empty()) return;
  auto previous_active_node = runtime->active_node;
  runtime->selected_nodes = {};
  runtime->selected_node_set = flight::Set<NodeType>();
  runtime->active_node = std::nullopt;
  emit_selection_changes(runtime, true, previous_active_node);
}

template <typename NodeType>
inline std::optional<NodeType> get_active_node(
    flight::Ref<flight::types::SelectionState<NodeType>> state) {
  return get_selection_state_runtime<NodeType>(state)->active_node;
}

template <typename NodeType>
inline flight::Array<NodeType> get_selected_nodes(
    flight::Ref<flight::types::SelectionState<NodeType>> state) {
  return get_selection_state_runtime<NodeType>(state)->selected_nodes;
}

template <typename NodeType>
inline double get_selection_count(
    flight::Ref<flight::types::SelectionState<NodeType>> state) {
  return static_cast<double>(
      get_selection_state_runtime<NodeType>(state)->selected_nodes.size());
}

template <typename NodeType>
inline flight::StructuralRef<flight::RowReadonly<
    flight::RowOf<flight::Ref<flight::types::SelectionSignals<NodeType>>>>>
get_selection_signals(flight::Ref<flight::types::SelectionState<NodeType>> state) {
  using ReadonlySignals = flight::StructuralRef<flight::RowReadonly<
      flight::RowOf<flight::Ref<flight::types::SelectionSignals<NodeType>>>>>;
  return flight::structural_ref_cast<ReadonlySignals>(
      get_selection_state_runtime<NodeType>(state)->signals);
}

template <typename NodeType>
inline bool has_selection(flight::Ref<flight::types::SelectionState<NodeType>> state) {
  return !get_selection_state_runtime<NodeType>(state)->selected_nodes.empty();
}

template <typename NodeType>
inline bool is_node_selected(
    flight::Ref<flight::types::SelectionState<NodeType>> state, NodeType node) {
  return get_selection_state_runtime<NodeType>(state)->selected_node_set.has(node);
}

template <typename NodeType>
inline void remove_node_from_selection(
    flight::Ref<flight::types::SelectionState<NodeType>> state, NodeType node) {
  auto runtime = get_selection_state_runtime<NodeType>(state);
  if (!runtime->selected_node_set.has(node)) return;
  auto previous_active_node = runtime->active_node;
  runtime->selected_node_set.erase(node);
  runtime->selected_nodes.splice(runtime->selected_nodes.index_of(node), 1.0);
  if (runtime->active_node == std::optional<NodeType>{node}) {
    runtime->active_node = runtime->selected_nodes.get(
        static_cast<double>(runtime->selected_nodes.size()) - 1.0);
  }
  emit_selection_changes(runtime, true, previous_active_node);
}

template <typename NodeType>
inline void select_all_nodes(
    flight::Ref<flight::types::SelectionState<NodeType>> state,
    flight::Array<NodeType> candidates) {
  auto runtime = get_selection_state_runtime<NodeType>(state);
  flight::Set<NodeType> selected_node_set;
  flight::Array<NodeType> selected_nodes;
  for (std::size_t i = 0; i < candidates.size(); ++i) {
    NodeType candidate = candidates.element(static_cast<double>(i));
    if (selected_node_set.has(candidate)) continue;
    selected_node_set.add(candidate);
    selected_nodes.push(candidate);
  }
  bool selection_changed = runtime->selected_nodes.size() != selected_nodes.size();
  if (!selection_changed) {
    for (std::size_t i = 0; i < selected_nodes.size(); ++i) {
      if (runtime->selected_nodes.element(static_cast<double>(i)) !=
          selected_nodes.element(static_cast<double>(i))) {
        selection_changed = true;
        break;
      }
    }
  }
  if (!selection_changed) return;
  auto previous_active_node = runtime->active_node;
  runtime->selected_node_set = selected_node_set;
  runtime->selected_nodes = selected_nodes;
  runtime->active_node = selected_nodes.get(static_cast<double>(selected_nodes.size()) - 1.0);
  emit_selection_changes(runtime, true, previous_active_node);
}

template <typename NodeType>
inline void select_node(
    flight::Ref<flight::types::SelectionState<NodeType>> state, NodeType node) {
  auto runtime = get_selection_state_runtime<NodeType>(state);
  if (runtime->selected_nodes.size() == 1 && runtime->selected_nodes.element(0.0) == node) return;
  auto previous_active_node = runtime->active_node;
  runtime->selected_node_set = flight::Set<NodeType>(flight::Array<NodeType>{node});
  runtime->selected_nodes = flight::Array<NodeType>{node};
  runtime->active_node = node;
  emit_selection_changes(runtime, true, previous_active_node);
}

template <typename NodeType>
inline void toggle_node_selection(
    flight::Ref<flight::types::SelectionState<NodeType>> state, NodeType node) {
  if (is_node_selected<NodeType>(state, node)) {
    remove_node_from_selection<NodeType>(state, node);
  } else {
    add_node_to_selection<NodeType>(state, node);
  }
}

} // namespace flight::selection
