// Shared runtime support for Picking overrides at a627849.
#pragma once

#include <flight/geometry/matrix4.hpp>
#include <flight/runtime.hpp>
#include <flight/scene3d/scene_node.hpp>
#include <flight/types/matrix4.hpp>
#include <flight/types/node3_d.hpp>

namespace flight::picking::detail {

inline void ensure_node_local_matrix4(
    const flight::scene3d::Node3DView& node,
    const flight::Ref<flight::types::Node3DRuntime>& runtime) {
  if (runtime->local_matrix4.has_value() &&
      runtime->local_transform_using_local_transform_id ==
          runtime->local_transform_id) {
    return;
  }
  if (!runtime->local_matrix4.has_value()) {
    runtime->local_matrix4 = flight::geometry::create_matrix4();
  }
  flight::geometry::compose_matrix4(
      runtime->local_matrix4.value(),
      flight::row_get<flight::RowKey<"position">>(node),
      flight::row_get<flight::RowKey<"rotation">>(node),
      flight::row_get<flight::RowKey<"scale">>(node));
  runtime->local_matrix4_detached = false;
  runtime->local_transform_using_local_transform_id =
      runtime->local_transform_id;
}

inline void ensure_node_world_matrix4(
    const flight::scene3d::Node3DView& node) {
  const auto runtime = flight::scene3d::get_node3_druntime(node);
  const auto parent = flight::scene3d::get_node3_dparent(node);
  flight::Ref<flight::types::Node3DRuntime> parent_runtime;
  double parent_world_transform_id = 0.0;
  if (parent.has_value()) {
    ensure_node_world_matrix4(parent.value());
    parent_runtime = flight::scene3d::get_node3_druntime(parent.value());
    parent_world_transform_id = parent_runtime->world_transform_id;
  }

  if (runtime->world_matrix4.has_value() &&
      runtime->world_transform_using_local_transform_id ==
          runtime->local_transform_id &&
      runtime->world_transform_using_parent_transform_id ==
          parent_world_transform_id) {
    return;
  }

  ensure_node_local_matrix4(node, runtime);
  if (!runtime->world_matrix4.has_value()) {
    runtime->world_matrix4 = flight::geometry::create_matrix4();
  }
  if (parent_runtime) {
    flight::geometry::multiply_matrix4(runtime->world_matrix4.value(),
                                       parent_runtime->world_matrix4.value(),
                                       runtime->local_matrix4.value());
  } else {
    flight::geometry::copy_matrix4(runtime->world_matrix4.value(),
                                   runtime->local_matrix4.value());
  }
  runtime->world_transform_using_local_transform_id =
      runtime->local_transform_id;
  runtime->world_transform_using_parent_transform_id =
      parent_world_transform_id;
  runtime->world_transform_id = flight::unsigned_right_shift(
      runtime->world_transform_id + 1.0, 0.0);
}

template <typename Value>
[[nodiscard]] inline flight::Ref<flight::types::Matrix4> node_world_matrix4(
    const std::shared_ptr<Value>& node) {
  const auto view = flight::scene3d::node3_dview(node);
  ensure_node_world_matrix4(view);
  return flight::scene3d::get_node3_druntime(view)->world_matrix4.value();
}

}  // namespace flight::picking::detail
