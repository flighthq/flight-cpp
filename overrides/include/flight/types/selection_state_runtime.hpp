// Declared EntityRuntime heritage for the a627849 generic selection runtime.
#pragma once

#include <optional>

#include <flight/runtime.hpp>
#include <flight/types/entity.hpp>
#include <flight/types/hierarchy_node.hpp>
#include <flight/types/selection_state.hpp>

namespace flight::types {

template <typename NodeType = NodeAny>
struct SelectionStateRuntime : public EntityRuntime {
  std::optional<NodeType> active_node;
  flight::Set<NodeType> selected_node_set;
  flight::Array<NodeType> selected_nodes;
  flight::Ref<SelectionSignals<NodeType>> signals;
};

} // namespace flight::types
