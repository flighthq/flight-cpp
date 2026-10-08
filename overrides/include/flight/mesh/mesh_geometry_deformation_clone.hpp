// Runtime completion of meshGeometryDeformationClone.ts.
#pragma once

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include <flight/geometry/aabb.hpp>
#include <flight/mesh/mesh_geometry.hpp>
#include <flight/mesh/mesh_geometry_attributes.hpp>
#include <flight/mesh/mesh_geometry_compute.hpp>
#include <flight/runtime.hpp>
#include <flight/structural_ref.hpp>
#include <flight/types/mesh_geometry.hpp>
#include <flight/types/mesh_morph_bind_pose.hpp>
#include <flight/types/mesh_skin_bind_pose.hpp>

namespace flight::mesh {

inline void restore_float3(flight::Ref<flight::types::MeshGeometry> geometry,
                           flight::String semantic,
                           flight::Float32Array source) {
  const double offset =
      get_vertex_attribute_float_offset(geometry->layout, semantic);
  if (offset < 0.0) return;

  const double floats_per_vertex = geometry->layout->stride / 4.0;
  const auto vertex_count = static_cast<std::size_t>(std::min(
      std::floor(static_cast<double>(geometry->vertices.size()) /
                 floats_per_vertex),
      std::floor(static_cast<double>(source.size()) / 3.0)));
  for (std::size_t vertex = 0; vertex < vertex_count; ++vertex) {
    const double target =
        static_cast<double>(vertex) * floats_per_vertex + offset;
    const double from = static_cast<double>(vertex) * 3.0;
    geometry->vertices.set_index(target, source.get_index(from));
    geometry->vertices.set_index(target + 1.0, source.get_index(from + 1.0));
    geometry->vertices.set_index(target + 2.0, source.get_index(from + 2.0));
  }
}

inline void restore_morph_bind_pose(
    flight::Ref<flight::types::MeshGeometry> geometry,
    flight::Ref<flight::types::MeshMorphBindPose> bind_pose) {
  restore_float3(geometry, flight::String("position"), bind_pose->positions);
  if (bind_pose->normals.has_value()) {
    restore_float3(geometry, flight::String("normal"),
                   bind_pose->normals.value());
  }
  if (bind_pose->tangents.has_value()) {
    restore_float3(geometry, flight::String("tangent"),
                   bind_pose->tangents.value());
  }
}

inline void restore_skin_bind_pose(
    flight::Ref<flight::types::MeshGeometry> geometry,
    flight::Ref<flight::types::MeshSkinBindPose> bind_pose) {
  restore_float3(geometry, flight::String("position"), bind_pose->positions);
  restore_float3(geometry, flight::String("normal"), bind_pose->normals);
}

inline flight::Ref<flight::types::MeshGeometry>
clone_mesh_geometry_for_deformation(
    flight::Ref<flight::types::MeshGeometry> source) {
  auto clone = clone_mesh_geometry(source);
  const auto morph_bind_pose = get_mesh_geometry_morph_bind_pose(source);
  const auto skin_bind_pose = get_mesh_geometry_skin_bind_pose(source);
  if (morph_bind_pose.has_value()) {
    restore_morph_bind_pose(clone, morph_bind_pose.value());
  } else if (skin_bind_pose.has_value()) {
    restore_skin_bind_pose(clone, skin_bind_pose.value());
  }

  auto bounds = flight::geometry::create_aabb(
      std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt,
      std::nullopt);
  compute_mesh_geometry_bounds(bounds, clone);
  clone->bounds = bounds;
  return clone;
}

template <typename Schema>
inline flight::Ref<flight::types::MeshGeometry>
clone_mesh_geometry_for_deformation(
    const flight::StructuralRef<Schema>& source) {
  const auto owner = source.shared_object();
  if (!owner) {
    throw std::logic_error("MeshGeometry structural view has no owner");
  }
  return clone_mesh_geometry_for_deformation(owner);
}

}  // namespace flight::mesh
