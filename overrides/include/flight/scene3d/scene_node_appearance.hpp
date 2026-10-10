// Native completion of scene3d/src/sceneNodeAppearance.ts for flattened Node3D owners.
#pragma once

#include <optional>

#include <flight/runtime.hpp>
#include <flight/scene3d/scene_node.hpp>
#include <flight/types/node3_d.hpp>

namespace flight::scene3d {

inline void set_node3_dalpha(
    flight::Ref<flight::types::Node3D> source,
    double alpha) {
  source->alpha = alpha;
  auto runtime = get_node3_druntime(source);
  runtime->appearance_id = flight::unsigned_right_shift(
      runtime->appearance_id + 1.0, 0.0);
}

inline double world_appearance_revision_counter = 0.0;

template <typename Source>
inline void ensure_node3_dworld_alpha(const Source& source) {
  auto runtime = get_node3_druntime(source);
  double parent_world_alpha = 1.0;
  double parent_world_appearance_id = 0.0;
  if (runtime->parent.has_value()) {
    const auto& parent = runtime->parent.value();
    ensure_node3_dworld_alpha(parent);
    const auto parent_runtime = get_node3_druntime(parent);
    parent_world_alpha = parent_runtime->world_alpha.value_or(1.0);
    parent_world_appearance_id = parent_runtime->world_appearance_id;
  }
  const double appearance_id = runtime->appearance_id;
  if (!runtime->world_alpha.has_value() ||
      runtime->world_alpha_using_appearance_id != appearance_id ||
      runtime->world_alpha_using_parent_appearance_id !=
          parent_world_appearance_id) {
    runtime->world_alpha = parent_world_alpha *
                           flight::row_get<flight::RowKey<"alpha">>(source);
    runtime->world_alpha_using_appearance_id = appearance_id;
    runtime->world_alpha_using_parent_appearance_id =
        parent_world_appearance_id;
    world_appearance_revision_counter = flight::unsigned_right_shift(
        world_appearance_revision_counter + 1.0, 0.0);
    if (world_appearance_revision_counter == 0.0) {
      world_appearance_revision_counter = 1.0;
    }
    runtime->world_appearance_id = world_appearance_revision_counter;
  }
}

template <typename Source>
inline double get_node3_dworld_alpha(const Source& source) {
  ensure_node3_dworld_alpha(source);
  return get_node3_druntime(source)->world_alpha.value_or(1.0);
}

} // namespace flight::scene3d
