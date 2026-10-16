#pragma once
#include <flight/types/node_2d.hpp>
#include <flight/types/shape.hpp>
#include <flight/types/sprite.hpp>

namespace flight::node {
using NativeNode2DRuntimeOwner = std::shared_ptr<flight::types::NodeRuntime<flight::Ref<flight::types::Node2DTraits>>>;
inline NativeNode2DRuntimeOwner native_node_2d_runtime(flight::types::Node2D source);
inline void invalidate_node_local_transform(flight::types::Node2D source);
inline void invalidate_node_appearance(flight::types::Node2D source);
inline void invalidate_node_parent_reference(flight::types::Node2D source);
inline void invalidate_node_world_bounds(flight::types::Node2D source);
}
#include <flight/scene2d/display_object.hpp>
#include <stdexcept>

namespace flight::node {
// The generated family changes the data member's static type. Retain that exact
// concrete owner; graph operations consume only its common Node2D trait cells.
// Backends recover the original Shape/Sprite before reading family-specific data.
template <typename Object>
  requires(std::same_as<Object, flight::types::Shape> ||
           std::same_as<Object, flight::types::Sprite>)
flight::types::Node2D native_node_2d(const std::shared_ptr<Object>& source) {
  using View = flight::StructuralRef<flight::RowWritable<flight::RowOf<std::shared_ptr<Object>>>>;
  View original(source);
  auto result = flight::types::Node2D::from_owner(original.shared_owner());
  static_cast<void>(flight::row_get<flight::RowKey<"enabled">>(result));
  static_cast<void>(flight::row_get<flight::RowKey<"x">>(result));
  static_cast<void>(flight::row_get<flight::RowKey<"alpha">>(result));
  return result;
}

inline NativeNode2DRuntimeOwner native_node_2d_runtime(flight::types::Node2D source) {
  if (auto shape = source.shared_exact_object<flight::types::Shape>()) return shape->entity_runtime_key.value();
  if (auto sprite = source.shared_exact_object<flight::types::Sprite>()) return sprite->entity_runtime_key.value();
  return flight::scene2d::get_node_2d_runtime(source);
}

inline void invalidate_node_appearance(flight::types::Node2D source) {
  auto runtime = native_node_2d_runtime(source);
  runtime->appearance_id = flight::unsigned_right_shift(runtime->appearance_id + 1, 0);
}
inline void invalidate_node_local_transform(flight::types::Node2D source) {
  auto runtime = native_node_2d_runtime(source);
  runtime->local_transform_id = flight::unsigned_right_shift(runtime->local_transform_id + 1, 0);
}
inline void invalidate_node_parent_reference(flight::types::Node2D source) {
  auto runtime = native_node_2d_runtime(source);
  runtime->parent_reference_id = flight::unsigned_right_shift(runtime->parent_reference_id + 1, 0);
  runtime->world_transform_using_parent_transform_id = -1;
}
inline void invalidate_node_world_bounds(flight::types::Node2D source) {
  std::optional<flight::types::Node2D> current = source;
  while (current) {
    auto runtime = native_node_2d_runtime(*current);
    runtime->world_bounds_using_world_transform_id = -1;
    runtime->world_bounds_using_local_bounds_id = -1;
    current = runtime->parent;
  }
}
} // namespace flight::node
