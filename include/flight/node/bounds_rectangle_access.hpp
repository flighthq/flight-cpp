#pragma once

#include <flight/runtime.hpp>
#include <flight/types/node.hpp>
#include <flight/types/node2_d.hpp>
#include <flight/types/rectangle.hpp>

namespace flight::node {

inline void compute_node_bounds_rectangle(
    flight::Ref<flight::types::Rectangle> out,
    flight::types::Node2D source,
    flight::types::Node2D target_coordinate_space) {
  (void)out;
  (void)source;
  (void)target_coordinate_space;
  throw flight::Error(flight::String(
      "Node bounds require the unavailable checked HasBoundsRectangleRuntime owner"));
}

inline flight::StructuralRef<
    flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::Rectangle>>>>
get_node_world_bounds_rectangle(flight::types::Node2D target) {
  (void)target;
  throw flight::Error(flight::String(
      "World bounds require the unavailable checked HasBoundsRectangleRuntime owner"));
}

template <typename Schema>
inline flight::StructuralRef<
    flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::Rectangle>>>>
get_node_world_bounds_rectangle(flight::StructuralRef<Schema> target) {
  (void)target;
  throw flight::Error(flight::String(
      "World bounds require the unavailable checked HasBoundsRectangleRuntime owner"));
}

} // namespace flight::node
