// Faithful native completion of skeleton3d/src/prepareScene3DSkinning.ts.
#pragma once

#include <memory>
#include <variant>

#include <flight/geometry/aabb.hpp>
#include <flight/mesh/mesh_geometry.hpp>
#include <flight/runtime.hpp>
#include <flight/scene3d/scene_node.hpp>
#include <flight/skeleton3d/get_mesh_skin_bounds.hpp>
#include <flight/skeleton3d/skeleton3d.hpp>
#include <flight/skeleton3d/skin_mesh_geometry.hpp>
#include <flight/types/aabb.hpp>
#include <flight/types/mesh.hpp>
#include <flight/types/mesh_geometry.hpp>
#include <flight/types/mesh_skin_bind_pose.hpp>
#include <flight/types/skin.hpp>

namespace flight::skeleton3d {

using ReadonlyMeshGeometry = flight::StructuralRef<
    flight::RowReadonly<
        flight::RowOf<flight::Ref<flight::types::MeshGeometry>>>>;
using ReadonlyMeshSkinBindPose = flight::StructuralRef<
    flight::RowReadonly<
        flight::RowOf<flight::Ref<flight::types::MeshSkinBindPose>>>>;

inline void prepare_mesh_skinning(flight::Ref<flight::types::Mesh> mesh) {
  const auto* skin =
      std::get_if<flight::Ref<flight::types::Skin>>(&mesh->skin);
  if (skin == nullptr) return;

  const ReadonlySkeleton3D skeleton((*skin)->skeleton);
  compute_skeleton_3d_joint_matrices(skeleton);

  const auto& geometry = mesh->geometry;
  const ReadonlyMeshGeometry geometry_view(geometry);
  auto bind_pose = flight::mesh::get_mesh_geometry_skin_bind_pose(geometry);
  if (!bind_pose.has_value()) {
    bind_pose = capture_mesh_skin_bind_pose(geometry_view);
    flight::mesh::set_mesh_geometry_skin_bind_pose(geometry,
                                                    bind_pose);
  }

  if (!mesh->entity_runtime_key.has_value() ||
      !mesh->entity_runtime_key.value()) {
    throw std::logic_error("Mesh has no runtime owner");
  }
  const auto runtime = mesh->entity_runtime_key.value();
  flight::Ref<flight::types::Aabb> bounds;
  if (const auto* present =
          std::get_if<flight::Ref<flight::types::Aabb>>(
              &runtime->deformed_local_bounds)) {
    bounds = *present;
  } else {
    bounds = flight::geometry::create_aabb(
        std::nullopt, std::nullopt, std::nullopt, std::nullopt,
        std::nullopt, std::nullopt);
    runtime->deformed_local_bounds =
        decltype(runtime->deformed_local_bounds){
            std::in_place_type<flight::Ref<flight::types::Aabb>>,
            bounds};
  }

  // Aabb and AabbLike are flattened sibling owners. The bounds routine writes
  // only through min/max, so retain those exact Vector3 owners in the view.
  const auto bounds_view = flight::make_ref<flight::types::AabbLike>(
      flight::types::AabbLike{.max = bounds->max, .min = bounds->min});
  get_mesh_skin_conservative_bounds(
      bounds_view, ReadonlyMeshSkinBindPose(bind_pose.value()), skeleton);
}

inline void prepare_scene_3d_skinning(
    const flight::scene3d::Node3DView& scene) {
  if (!flight::row_get<flight::RowKey<"enabled">>(scene)) return;

  if (const auto mesh =
          scene.template shared_exact_object<flight::types::Mesh>();
      mesh && mesh->geometry) {
    prepare_mesh_skinning(mesh);
  }

  const auto& children =
      flight::scene3d::get_node_3d_runtime(scene)->children;
  if (!children.has_value()) return;
  for (std::size_t i = 0; i < children.value().size(); ++i) {
    prepare_scene_3d_skinning(children.value()[i]);
  }
}

template <typename Node>
inline void prepare_scene_3d_skinning(
    const std::shared_ptr<Node>& scene) {
  prepare_scene_3d_skinning(flight::scene3d::node3_dview(scene));
}

}  // namespace flight::skeleton3d
