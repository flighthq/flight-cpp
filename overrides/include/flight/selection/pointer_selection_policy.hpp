// Native completion of Flight a627849 pointerSelectionPolicy.ts.
#pragma once

#include <optional>

#include <flight/structural_ref.hpp>
#include <flight/types/selection_state.hpp>
#include <flight/selection/selection_state.hpp>

namespace flight::selection {

using ReadonlySelectionModifiers = flight::StructuralRef<flight::RowReadonly<
    flight::RowOf<flight::Ref<flight::types::SelectionModifierState>>>>;

inline bool has_toggle_modifier(ReadonlySelectionModifiers modifiers) {
  return flight::row_get<flight::RowKey<"ctrlKey">>(modifiers) ||
         flight::row_get<flight::RowKey<"metaKey">>(modifiers) ||
         flight::row_get<flight::RowKey<"shiftKey">>(modifiers);
}

template <typename NodeType>
inline void apply_pointer_selection_policy(
    flight::Ref<flight::types::SelectionState<NodeType>> state,
    std::optional<NodeType> hit,
    ReadonlySelectionModifiers modifiers) {
  if (flight::row_get<flight::RowKey<"altKey">>(modifiers)) return;
  if (!hit.has_value()) {
    if (!has_toggle_modifier(modifiers)) clear_selection<NodeType>(state);
    return;
  }
  if (has_toggle_modifier(modifiers)) {
    if (flight::row_get<flight::RowKey<"shiftKey">>(modifiers) &&
        !is_node_selected<NodeType>(state, hit.value())) {
      add_node_to_selection<NodeType>(state, hit.value());
    } else {
      toggle_node_selection<NodeType>(state, hit.value());
    }
    return;
  }
  if (!is_node_selected<NodeType>(state, hit.value())) select_node<NodeType>(state, hit.value());
}

inline bool has_any_modifier(ReadonlySelectionModifiers modifiers) {
  return flight::row_get<flight::RowKey<"altKey">>(modifiers) ||
         has_toggle_modifier(modifiers);
}

template <typename NodeType>
inline void apply_pointer_up_selection_policy(
    flight::Ref<flight::types::SelectionState<NodeType>> state,
    std::optional<NodeType> hit,
    ReadonlySelectionModifiers modifiers,
    bool did_drag) {
  if (did_drag || !hit.has_value() || has_any_modifier(modifiers) ||
      !is_node_selected<NodeType>(state, hit.value())) {
    return;
  }
  select_node<NodeType>(state, hit.value());
}

} // namespace flight::selection
