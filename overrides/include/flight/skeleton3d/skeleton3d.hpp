// Faithful native completion of skeleton3d/src/skeleton3d.ts.
#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <unordered_map>
#include <variant>

#include <flight/entity/entity.hpp>
#include <flight/geometry/matrix3.hpp>
#include <flight/geometry/matrix4.hpp>
#include <flight/runtime.hpp>
#include <flight/scene3d/scene_node.hpp>
#include <flight/skeleton3d/runtime_access.hpp>
#include <flight/types/entity.hpp>
#include <flight/types/matrix3.hpp>
#include <flight/types/matrix4.hpp>
#include <flight/types/node_3d.hpp>
#include <flight/types/skeleton_3d.hpp>
#include <flight/types/skeleton_3d_validation_diagnostic.hpp>

namespace flight::skeleton3d {

using flight::types::EntityConstruction;
using flight::types::Matrix3;
using flight::types::Matrix4;
using flight::types::Matrix4Like;
using flight::types::Node3D;
using flight::types::Skeleton3D;
using flight::types::Skeleton3DValidationDiagnostic;

using Skeleton3DNames =
    std::variant<flight::Array<flight::String>, flight::Null,
                 flight::Undefined>;
using ReadonlyNode3D = flight::StructuralRef<
    flight::RowReadonly<flight::RowOf<flight::Ref<Node3D>>>>;
using ReadonlySkeleton3D = flight::StructuralRef<
    flight::RowReadonly<flight::RowOf<flight::Ref<Skeleton3D>>>>;
using CloneSkeleton3DJoint =
    std::function<flight::Ref<Node3D>(ReadonlyNode3D, double)>;

inline void initialize_skeleton_3d(
    EntityConstruction<flight::Ref<Skeleton3D>> out,
    flight::Float32Array inverse_bind_matrices,
    flight::Float32Array joint_matrices,
    flight::Array<flight::Ref<Node3D>> joints, Skeleton3DNames names,
    flight::Float32Array normal_matrices) {
  out->inverse_bind_matrices = std::move(inverse_bind_matrices);
  out->joint_matrices = std::move(joint_matrices);
  out->joints = std::move(joints);
  out->names = std::move(names);
  out->normal_matrices = std::move(normal_matrices);
}

inline Skeleton3DNames clone_skeleton3_dnames(const Skeleton3DNames& names) {
  if (const auto* present =
          std::get_if<flight::Array<flight::String>>(&names)) {
    return Skeleton3DNames{
        std::in_place_type<flight::Array<flight::String>>, present->slice()};
  }
  if (std::holds_alternative<flight::Null>(names)) {
    return Skeleton3DNames{std::in_place_type<flight::Null>, flight::null};
  }
  return Skeleton3DNames{std::in_place_type<flight::Undefined>,
                         flight::undefined};
}

inline void dispose_skeleton_3d(flight::Ref<Skeleton3D> skeleton) {
  skeleton->joints.resize(std::size_t{0});
  skeleton->names =
      Skeleton3DNames{std::in_place_type<flight::Null>, flight::null};
}

inline bool equals_skeleton_3d(ReadonlySkeleton3D a,
                               ReadonlySkeleton3D b) {
  if (a == b) return true;
  if (a->joints.size() != b->joints.size()) return false;
  if (a->inverse_bind_matrices.size() != b->inverse_bind_matrices.size()) {
    return false;
  }
  for (std::size_t i = 0; i < a->inverse_bind_matrices.size(); ++i) {
    if (a->inverse_bind_matrices[i] != b->inverse_bind_matrices[i]) {
      return false;
    }
  }

  const auto* a_names =
      std::get_if<flight::Array<flight::String>>(&a->names);
  const auto* b_names =
      std::get_if<flight::Array<flight::String>>(&b->names);
  if (a_names == nullptr || b_names == nullptr) {
    // Source `?? null` treats both absence sentinels as the same value.
    return a_names == nullptr && b_names == nullptr;
  }
  if (a_names->size() != b_names->size()) return false;
  for (std::size_t i = 0; i < a_names->size(); ++i) {
    if ((*a_names)[i] != (*b_names)[i]) return false;
  }
  return true;
}

inline double get_skeleton_3d_joint_index_by_name(
    ReadonlySkeleton3D skeleton, flight::String name) {
  const auto* names =
      std::get_if<flight::Array<flight::String>>(&skeleton->names);
  return names == nullptr ? -1.0
                          : static_cast<double>(names->index_of(name));
}

// The generated Node3D interface is a flattened owner rather than Node<Traits>.
// Use the already-supported flattened Scene3D runtime view, but retain the node's
// exact owner and revision counters while computing its world transform.
inline void ensure_skeleton3_dnode_local_matrix(
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

inline void ensure_skeleton3_dnode_world_matrix(
    const flight::scene3d::Node3DView& node) {
  auto runtime = flight::scene3d::get_node_3d_runtime(node);
  const auto parent = flight::scene3d::get_node3_dparent(node);
  flight::Ref<flight::types::Node3DRuntime> parent_runtime;
  double parent_world_transform_id = 0.0;
  if (parent.has_value()) {
    ensure_skeleton3_dnode_world_matrix(parent.value());
    parent_runtime = flight::scene3d::get_node_3d_runtime(parent.value());
    parent_world_transform_id = parent_runtime->world_transform_id;
  }

  if (runtime->world_transform_using_local_transform_id ==
          runtime->local_transform_id &&
      runtime->world_transform_using_parent_transform_id ==
          parent_world_transform_id) {
    return;
  }

  ensure_skeleton3_dnode_local_matrix(node, runtime);
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

inline flight::Ref<Matrix4> get_skeleton3_dnode_world_matrix(
    const flight::Ref<Node3D>& node) {
  const auto view = flight::scene3d::node3_dview(node);
  ensure_skeleton3_dnode_world_matrix(view);
  return flight::scene3d::get_node_3d_runtime(view)->world_matrix4.value();
}

inline bool get_skeleton_3d_joint_world_matrix(
    flight::Ref<Matrix4Like> out, ReadonlySkeleton3D skeleton,
    double joint_index) {
  if (joint_index < 0.0 ||
      joint_index >= static_cast<double>(skeleton->joints.size())) {
    return false;
  }
  flight::geometry::copy_matrix4(
      out, get_skeleton3_dnode_world_matrix(
               skeleton->joints.element(joint_index)));
  return true;
}

inline bool get_skeleton_3d_joint_world_matrix_by_name(
    flight::Ref<Matrix4Like> out, ReadonlySkeleton3D skeleton,
    flight::String name) {
  return get_skeleton_3d_joint_world_matrix(
      out, skeleton,
      get_skeleton_3d_joint_index_by_name(skeleton, std::move(name)));
}

inline flight::Ref<Skeleton3D> clone_skeleton_3d(
    ReadonlySkeleton3D skeleton) {
  auto out = flight::entity::allocate_entity<flight::Ref<Skeleton3D>>();
  initialize_skeleton_3d(
      out, flight::Float32Array::from(skeleton->inverse_bind_matrices),
      flight::Float32Array::from(skeleton->joint_matrices),
      skeleton->joints.slice(), clone_skeleton3_dnames(skeleton->names),
      flight::Float32Array::from(skeleton->normal_matrices));
  return flight::entity::finish_entity<flight::Ref<Skeleton3D>>(out);
}

inline flight::Ref<Skeleton3D> clone_skeleton_3d_joint_hierarchy(
    ReadonlySkeleton3D skeleton, CloneSkeleton3DJoint clone_joint) {
  const auto& source_joints = skeleton->joints;
  flight::Array<flight::Ref<Node3D>> joints(source_joints.size());
  std::unordered_map<const flight::RowOwner*, flight::Ref<Node3D>>
      clones_by_source;

  for (std::size_t i = 0; i < source_joints.size(); ++i) {
    const auto& source = source_joints[i];
    auto clone = clone_joint(ReadonlyNode3D(source), static_cast<double>(i));
    joints[i] = clone;
    clones_by_source.emplace(
        flight::scene3d::node3_dview(source).shared_owner().get(),
        std::move(clone));
  }
  for (std::size_t i = 0; i < source_joints.size(); ++i) {
    const auto parent =
        flight::scene3d::get_node3_dparent(source_joints[i]);
    if (!parent.has_value()) continue;
    const auto found =
        clones_by_source.find(parent->shared_owner().get());
    if (found == clones_by_source.end()) continue;
    const auto parent_view = flight::scene3d::node3_dview(found->second);
    const auto child_view = flight::scene3d::node3_dview(joints[i]);
    flight::scene3d::add_node3_dchild_at(
        parent_view, child_view,
        flight::scene3d::get_node3_dchild_count(parent_view));
  }

  auto out = flight::entity::allocate_entity<flight::Ref<Skeleton3D>>();
  initialize_skeleton_3d(
      out, flight::Float32Array::from(skeleton->inverse_bind_matrices),
      flight::Float32Array::from(skeleton->joint_matrices), std::move(joints),
      clone_skeleton3_dnames(skeleton->names),
      flight::Float32Array::from(skeleton->normal_matrices));
  return flight::entity::finish_entity<flight::Ref<Skeleton3D>>(out);
}

inline std::optional<flight::Ref<Skeleton3DValidationDiagnostic>>
validate_skeleton_3d(ReadonlySkeleton3D skeleton) {
  const double joint_count = static_cast<double>(skeleton->joints.size());
  const double inverse_bind_matrices_length =
      static_cast<double>(skeleton->inverse_bind_matrices.size());
  const double expected_inverse_bind_matrices_length = joint_count * 16.0;
  if (inverse_bind_matrices_length ==
      expected_inverse_bind_matrices_length) {
    return std::nullopt;
  }
  return flight::make_ref<Skeleton3DValidationDiagnostic>(
      Skeleton3DValidationDiagnostic{
          .joint_count = joint_count,
          .inverse_bind_matrices_length = inverse_bind_matrices_length,
          .expected_inverse_bind_matrices_length =
              expected_inverse_bind_matrices_length,
          .message = flight::String("Skeleton3D inverseBindMatrices length ") +
                     flight::to_string(inverse_bind_matrices_length) +
                     flight::String(" does not match jointCount ") +
                     flight::to_string(joint_count) +
                     flight::String(" * 16 = ") +
                     flight::to_string(expected_inverse_bind_matrices_length) +
                     flight::String(".")});
}

inline flight::Ref<Matrix4> inv_bind = flight::geometry::create_matrix4();
inline flight::Ref<Matrix4> result = flight::geometry::create_matrix4();
inline flight::Ref<Matrix3> normal = flight::geometry::create_matrix3();

inline void compute_skeleton_3d_joint_matrices(
    ReadonlySkeleton3D skeleton) {
  for (std::size_t joint = 0; joint < skeleton->joints.size(); ++joint) {
    const double base = static_cast<double>(joint * 16);
    for (std::size_t component = 0; component < 16; ++component) {
      inv_bind->m[component] = skeleton->inverse_bind_matrices[
          joint * 16 + component];
    }
    flight::geometry::multiply_matrix4(
        result, get_skeleton3_dnode_world_matrix(skeleton->joints[joint]),
        inv_bind);
    skeleton->joint_matrices.set(result->m, base);
    flight::geometry::set_matrix3_normal_from_matrix4(normal, result);
    const std::size_t normal_base = joint * 12;
    for (std::size_t column = 0; column < 3; ++column) {
      skeleton->normal_matrices[normal_base + column * 4] =
          normal->m[column * 3];
      skeleton->normal_matrices[normal_base + column * 4 + 1] =
          normal->m[column * 3 + 1];
      skeleton->normal_matrices[normal_base + column * 4 + 2] =
          normal->m[column * 3 + 2];
      skeleton->normal_matrices[normal_base + column * 4 + 3] = 0.0F;
    }
  }
}

inline std::optional<
    std::function<void(ReadonlySkeleton3D, double)>>
    skeleton_3d_bind_pose_guard = std::nullopt;

inline void set_skeleton_3d_bind_pose(ReadonlySkeleton3D skeleton) {
  for (std::size_t joint = 0; joint < skeleton->joints.size(); ++joint) {
    if (flight::geometry::inverse_matrix4(
            result,
            get_skeleton3_dnode_world_matrix(skeleton->joints[joint]))) {
      skeleton->inverse_bind_matrices.set(result->m,
                                          static_cast<double>(joint * 16));
      continue;
    }
    flight::geometry::set_matrix4_identity(result);
    skeleton->inverse_bind_matrices.set(result->m,
                                        static_cast<double>(joint * 16));
    if (skeleton_3d_bind_pose_guard.has_value()) {
      skeleton_3d_bind_pose_guard.value()(skeleton,
                                         static_cast<double>(joint));
    }
  }
}

inline flight::Ref<Skeleton3D> create_skeleton_3d(
    flight::Array<flight::Ref<Node3D>> joints,
    std::optional<flight::Float32Array> inverse_bind_matrices = std::nullopt,
    std::optional<std::optional<flight::Array<flight::String>>> names =
        std::nullopt) {
  const double count = static_cast<double>(joints.size());
  auto skeleton =
      flight::entity::allocate_entity<flight::Ref<Skeleton3D>>();
  Skeleton3DNames stored_names{std::in_place_type<flight::Null>,
                               flight::null};
  if (names.has_value() && names.value().has_value()) {
    stored_names = Skeleton3DNames{
        std::in_place_type<flight::Array<flight::String>>,
        names.value().value()};
  }
  initialize_skeleton_3d(
      skeleton,
      inverse_bind_matrices.value_or(flight::Float32Array(count * 16.0)),
      flight::Float32Array(count * 16.0), std::move(joints),
      std::move(stored_names), flight::Float32Array(count * 12.0));
  auto finished =
      flight::entity::finish_entity<flight::Ref<Skeleton3D>>(skeleton);
  if (!inverse_bind_matrices.has_value()) {
    set_skeleton_3d_bind_pose(finished);
  }
  return finished;
}

inline void set_skeleton_3d_bind_pose_guard(
    std::optional<std::function<void(ReadonlySkeleton3D, double)>>
        guard) {
  skeleton_3d_bind_pose_guard = std::move(guard);
}

}  // namespace flight::skeleton3d
