// Runtime completion of picking/src/sceneHitAttributes.ts at a627849.
#pragma once

#include <cmath>
#include <optional>
#include <stdexcept>

#include <flight/geometry/matrix4.hpp>
#include <flight/geometry/vector3.hpp>
#include <flight/mesh/mesh_geometry_attributes.hpp>
#include <flight/mesh/mesh_geometry_operations.hpp>
#include <flight/mesh/mesh_geometry_subset.hpp>
#include <flight/geometry/matrix4.hpp>
#include <flight/runtime.hpp>
#include <flight/scene3d/scene_node.hpp>
#include <flight/types/matrix4.hpp>
#include <flight/types/node_3d.hpp>

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
  const auto runtime = flight::scene3d::get_node_3d_runtime(node);
  const auto parent = flight::scene3d::get_node3_dparent(node);
  flight::Ref<flight::types::Node3DRuntime> parent_runtime;
  double parent_world_transform_id = 0.0;
  if (parent.has_value()) {
    ensure_node_world_matrix4(parent.value());
    parent_runtime = flight::scene3d::get_node_3d_runtime(parent.value());
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
  return flight::scene3d::get_node_3d_runtime(view)->world_matrix4.value();
}

}  // namespace flight::picking::detail
#include <flight/runtime.hpp>
#include <flight/structural_ref.hpp>
#include <flight/types/material_3d.hpp>
#include <flight/types/matrix4.hpp>
#include <flight/types/mesh.hpp>
#include <flight/types/mesh_triangle_vertex_indices.hpp>
#include <flight/types/ray_3d.hpp>
#include <flight/types/scene_3d_hit.hpp>
#include <flight/types/screen.hpp>
#include <flight/types/unity_schema.hpp>
#include <flight/types/vector2.hpp>
#include <flight/types/vector3.hpp>

namespace flight::picking {

using AttributeHitView = flight::StructuralRef<
    flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::Scene3DHit>>>>;
using AttributeRayView = flight::StructuralRef<
    flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::Ray3D>>>>;
using AttributeSceneTraits = flight::Ref<flight::types::Node3DTraits>;

[[nodiscard]] inline flight::Ref<flight::types::Scene3DHit>
attribute_exact_hit(const AttributeHitView& hit) {
  auto exact = hit.template shared_exact_object<flight::types::Scene3DHit>();
  if (!exact) throw std::bad_cast();
  return exact;
}

[[nodiscard]] inline std::optional<flight::Ref<flight::types::Material3D>>
get_scene_3d_hit_material(const AttributeHitView& hit_view) {
  const auto hit = attribute_exact_hit(hit_view);
  if (!hit->node.has_value()) return std::nullopt;
  const auto& node = hit->node.value();
  const double subset_index =
      flight::mesh::get_mesh_geometry_triangle_subset_index(
          node->geometry, hit->triangle_index);
  if (subset_index < 0.0 ||
      subset_index >= static_cast<double>(node->materials.size())) {
    return std::nullopt;
  }
  return node->materials.element(subset_index);
}

[[nodiscard]] inline double get_scene_3d_hit_subset_index(
    const AttributeHitView& hit_view) {
  const auto hit = attribute_exact_hit(hit_view);
  if (!hit->node.has_value()) return -1.0;
  return flight::mesh::get_mesh_geometry_triangle_subset_index(
      hit->node.value()->geometry, hit->triangle_index);
}

[[nodiscard]] inline bool is_scene_3d_hit_front_facing(
    const AttributeHitView& hit_view, const AttributeRayView& ray_view) {
  const auto hit = attribute_exact_hit(hit_view);
  const auto ray =
      ray_view.template shared_exact_object<flight::types::Ray3D>();
  if (!ray) throw std::bad_cast();
  return ray->direction->x * hit->normal_x +
             ray->direction->y * hit->normal_y +
             ray->direction->z * hit->normal_z <=
         0.0;
}

[[nodiscard]] inline bool write_normalized(
    const flight::types::Vector3Like& out, double x, double y, double z) {
  const double length_squared = x * x + y * y + z * z;
  if (length_squared == 0.0) return false;
  const double inverse_length = 1.0 / std::sqrt(length_squared);
  out->x = x * inverse_length;
  out->y = y * inverse_length;
  out->z = z * inverse_length;
  return true;
}

struct w_x_y_z_5137f2b28145ccce : public flight::ReferenceEnabled {
  double w;
  double x;
  double y;
  double z;
};

[[nodiscard]] inline bool get_scene_3d_hit_uv0(
    const flight::types::Vector2Like& out,
    const AttributeHitView& hit_view) {
  const auto hit = attribute_exact_hit(hit_view);
  if (!hit->node.has_value()) return false;
  const auto& geometry = hit->node.value()->geometry;
  auto triangle =
      flight::make_ref<flight::types::MeshTriangleVertexIndices>();
  auto uv0 = flight::make_ref<flight::types::x_y_8365950bd60f783f>();
  auto uv1 = flight::make_ref<flight::types::x_y_8365950bd60f783f>();
  auto uv2 = flight::make_ref<flight::types::x_y_8365950bd60f783f>();
  if (!flight::mesh::get_mesh_geometry_triangle_vertex_indices(
          triangle, geometry, hit->triangle_index) ||
      !flight::mesh::get_mesh_geometry_vertex_uv0(uv0, geometry,
                                                   triangle->i0) ||
      !flight::mesh::get_mesh_geometry_vertex_uv0(uv1, geometry,
                                                   triangle->i1) ||
      !flight::mesh::get_mesh_geometry_vertex_uv0(uv2, geometry,
                                                   triangle->i2)) {
    return false;
  }
  out->x = hit->u * uv0->x + hit->v * uv1->x + hit->w * uv2->x;
  out->y = hit->u * uv0->y + hit->v * uv1->y + hit->w * uv2->y;
  return true;
}

[[nodiscard]] inline bool get_scene_3d_hit_vertex_normal(
    const flight::types::Vector3Like& out,
    const AttributeHitView& hit_view) {
  const auto hit = attribute_exact_hit(hit_view);
  if (!hit->node.has_value()) return false;
  const auto& node = hit->node.value();
  const auto& geometry = node->geometry;
  auto triangle =
      flight::make_ref<flight::types::MeshTriangleVertexIndices>();
  auto normal0 =
      flight::make_ref<flight::types::x_y_z_82615fd0df8619c4>();
  auto normal1 =
      flight::make_ref<flight::types::x_y_z_82615fd0df8619c4>();
  auto normal2 =
      flight::make_ref<flight::types::x_y_z_82615fd0df8619c4>();
  if (!flight::mesh::get_mesh_geometry_triangle_vertex_indices(
          triangle, geometry, hit->triangle_index) ||
      !flight::mesh::get_mesh_geometry_vertex_normal(normal0, geometry,
                                                      triangle->i0) ||
      !flight::mesh::get_mesh_geometry_vertex_normal(normal1, geometry,
                                                      triangle->i1) ||
      !flight::mesh::get_mesh_geometry_vertex_normal(normal2, geometry,
                                                      triangle->i2)) {
    return false;
  }

  const double nx =
      hit->u * normal0->x + hit->v * normal1->x + hit->w * normal2->x;
  const double ny =
      hit->u * normal0->y + hit->v * normal1->y + hit->w * normal2->y;
  const double nz =
      hit->u * normal0->z + hit->v * normal1->z + hit->w * normal2->z;
  const auto world = detail::node_world_matrix4(node);
  const auto world_view = flight::StructuralRef<flight::RowReadonly<
      flight::RowOf<flight::Ref<flight::types::Matrix4>>>>(world);
  auto inverse_world = flight::geometry::create_matrix4();
  if (!flight::geometry::inverse_matrix4(inverse_world, world_view)) {
    return false;
  }
  const auto& matrix = inverse_world->m;
  return write_normalized(
      out, matrix.get_index(0.0) * nx + matrix.get_index(1.0) * ny +
               matrix.get_index(2.0) * nz,
      matrix.get_index(4.0) * nx + matrix.get_index(5.0) * ny +
          matrix.get_index(6.0) * nz,
      matrix.get_index(8.0) * nx + matrix.get_index(9.0) * ny +
          matrix.get_index(10.0) * nz);
}

[[nodiscard]] inline bool get_scene_3d_hit_vertex_tangent(
    const flight::Ref<w_x_y_z_5137f2b28145ccce>& out,
    const AttributeHitView& hit_view) {
  const auto hit = attribute_exact_hit(hit_view);
  if (!hit->node.has_value()) return false;
  const auto& node = hit->node.value();
  const auto& geometry = node->geometry;
  auto triangle =
      flight::make_ref<flight::types::MeshTriangleVertexIndices>();
  auto tangent0 =
      flight::make_ref<flight::mesh::w_x_y_z_5137f2b28145ccce>();
  auto tangent1 =
      flight::make_ref<flight::mesh::w_x_y_z_5137f2b28145ccce>();
  auto tangent2 =
      flight::make_ref<flight::mesh::w_x_y_z_5137f2b28145ccce>();
  if (!flight::mesh::get_mesh_geometry_triangle_vertex_indices(
          triangle, geometry, hit->triangle_index) ||
      !flight::mesh::get_mesh_geometry_vertex_tangent(tangent0, geometry,
                                                       triangle->i0) ||
      !flight::mesh::get_mesh_geometry_vertex_tangent(tangent1, geometry,
                                                       triangle->i1) ||
      !flight::mesh::get_mesh_geometry_vertex_tangent(tangent2, geometry,
                                                       triangle->i2)) {
    return false;
  }
  auto world_normal = flight::geometry::create_vector3();
  if (!get_scene_3d_hit_vertex_normal(world_normal, hit_view)) return false;

  const double tx =
      hit->u * tangent0->x + hit->v * tangent1->x + hit->w * tangent2->x;
  const double ty =
      hit->u * tangent0->y + hit->v * tangent1->y + hit->w * tangent2->y;
  const double tz =
      hit->u * tangent0->z + hit->v * tangent1->z + hit->w * tangent2->z;
  const double tw =
      hit->u * tangent0->w + hit->v * tangent1->w + hit->w * tangent2->w;
  const auto world = detail::node_world_matrix4(node);
  const auto& matrix = world->m;
  double wx = matrix.get_index(0.0) * tx + matrix.get_index(4.0) * ty +
              matrix.get_index(8.0) * tz;
  double wy = matrix.get_index(1.0) * tx + matrix.get_index(5.0) * ty +
              matrix.get_index(9.0) * tz;
  double wz = matrix.get_index(2.0) * tx + matrix.get_index(6.0) * ty +
              matrix.get_index(10.0) * tz;
  const double projection = wx * world_normal->x + wy * world_normal->y +
                            wz * world_normal->z;
  wx -= projection * world_normal->x;
  wy -= projection * world_normal->y;
  wz -= projection * world_normal->z;
  const double length_squared = wx * wx + wy * wy + wz * wz;
  if (length_squared == 0.0) return false;
  const double inverse_length = 1.0 / std::sqrt(length_squared);
  const double determinant =
      matrix.get_index(0.0) *
          (matrix.get_index(5.0) * matrix.get_index(10.0) -
           matrix.get_index(9.0) * matrix.get_index(6.0)) -
      matrix.get_index(4.0) *
          (matrix.get_index(1.0) * matrix.get_index(10.0) -
           matrix.get_index(9.0) * matrix.get_index(2.0)) +
      matrix.get_index(8.0) *
          (matrix.get_index(1.0) * matrix.get_index(6.0) -
           matrix.get_index(5.0) * matrix.get_index(2.0));
  out->x = wx * inverse_length;
  out->y = wy * inverse_length;
  out->z = wz * inverse_length;
  out->w = (tw < 0.0 ? -1.0 : 1.0) *
           (determinant < 0.0 ? -1.0 : 1.0);
  return true;
}

}  // namespace flight::picking
