// Correct generic signal carrier for the a627849 selection types.
#pragma once

#include <functional>
#include <optional>

#include <flight/runtime.hpp>
#include <flight/types/entity.hpp>
#include <flight/types/hierarchy_node.hpp>
#include <flight/types/signal.hpp>

namespace flight::types {

struct SelectionModifierState : public flight::ReferenceEnabled {
  bool alt_key;
  bool ctrl_key;
  bool meta_key;
  bool shift_key;
};

template <typename NodeType = NodeAny>
struct SelectionSignals : public flight::ReferenceEnabled {
  std::shared_ptr<Signal<std::function<void(std::optional<NodeType>)>>> on_active_change;
  std::shared_ptr<Signal<std::function<void(flight::Array<NodeType>)>>> on_change;
};

template <typename NodeType = NodeAny>
struct SelectionState : public flight::ReferenceEnabled {
  std::optional<flight::Ref<EntityRuntime>> entity_runtime_key;
};

} // namespace flight::types
