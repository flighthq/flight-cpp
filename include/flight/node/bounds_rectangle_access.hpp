#pragma once

#include <flight/error.hpp>
#include <flight/reference.hpp>
#include <flight/string.hpp>
#include <flight/structural_ref.hpp>

namespace flight::types {
struct Rectangle;
}

namespace flight::node {

template <typename Out, typename Source, typename TargetCoordinateSpace>
inline void compute_node_bounds_rectangle(
    Out&& out,
    Source&& source,
    TargetCoordinateSpace&& target_coordinate_space) {
  (void)out;
  (void)source;
  (void)target_coordinate_space;
  throw flight::Error(flight::String(
      "Node bounds require the unavailable checked HasBoundsRectangleRuntime owner"));
}

template <typename Target>
inline flight::StructuralRef<
    flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::Rectangle>>>>
get_node_world_bounds_rectangle(const Target& target) {
  (void)target;
  throw flight::Error(flight::String(
      "World bounds require the unavailable checked HasBoundsRectangleRuntime owner"));
}

} // namespace flight::node

namespace flight::interaction {

// Some retained interaction modules emit these imported node helpers unqualified. Keep the
// forwarding boundary in their lookup namespace while the underlying runtime remains in node.
template <typename Source>
inline auto get_node_parent(const Source& source) {
  return flight::node::get_node_parent(source);
}

template <typename Target>
inline auto get_node_world_bounds_rectangle(const Target& target) {
  return flight::node::get_node_world_bounds_rectangle(target);
}

} // namespace flight::interaction
