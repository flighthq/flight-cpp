#pragma once

#include <flight/runtime.hpp>
#include <flight/types/node.hpp>

namespace flight::node {

template <typename Traits>
inline std::optional<flight::types::NodeOf<Traits>> get_node_parent(
    flight::types::NodeOf<Traits> source) {
  (void)source;
  throw flight::Error(flight::String(
      "Node hierarchy access requires the unavailable checked NodeRuntime owner"));
}

} // namespace flight::node
