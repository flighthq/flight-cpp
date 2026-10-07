#pragma once

#include <flight/runtime.hpp>
#include <flight/types/matrix.hpp>

namespace flight::node {

template <typename Schema>
inline flight::StructuralRef<
    flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::Matrix>>>>
get_node_world_matrix(flight::StructuralRef<Schema> target) {
  (void)target;
  throw flight::Error(flight::String(
      "World transform requires the unavailable checked HasTransform2DRuntime owner"));
}

} // namespace flight::node
