#pragma once

#include <flight/runtime.hpp>
#include <flight/types/matrix4.hpp>
#include <flight/types/node3_d.hpp>

namespace flight::node {

inline flight::StructuralRef<
    flight::RowReadonly<flight::RowOf<flight::types::Matrix4Like>>>
get_node_world_matrix4(flight::types::Node3D target) {
  (void)target;
  throw flight::Error(flight::String(
      "World transform requires the unavailable checked HasTransform3DRuntime owner"));
}

} // namespace flight::node
