// Runtime completion of picking/src/pickScene3D.ts at a627849.
#pragma once

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <variant>

#include <flight/camera/picking.hpp>
#include <flight/entity/entity.hpp>
#include <flight/geometry/matrix4.hpp>
#include <flight/geometry/ray3d.hpp>
#include <flight/geometry/vector3.hpp>
#include <flight/mesh/mesh_geometry_attributes.hpp>
#include <flight/mesh/mesh_geometry_compute.hpp>
#include <flight/mesh/mesh_geometry_operations.hpp>
#include <flight/node/node.hpp>
#include <flight/picking/runtime_support.hpp>
#include <flight/runtime.hpp>
#include <flight/structural_ref.hpp>
#include <flight/types/aabb.hpp>
#include <flight/types/camera3_d.hpp>
#include <flight/types/entity.hpp>
#include <flight/types/matrix4.hpp>
#include <flight/types/mesh.hpp>
#include <flight/types/mesh_triangle_vertex_indices.hpp>
#include <flight/types/node3_d.hpp>
#include <flight/types/ray3_d.hpp>
#include <flight/types/scene3_dhit.hpp>
#include <flight/types/scene3_dpick_options.hpp>
#include <flight/types/unity_schema.hpp>
#include <flight/types/vector3.hpp>

namespace flight::picking {

using CameraView = flight::StructuralRef<
    flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::Camera3D>>>>;
using HitView = flight::StructuralRef<
    flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::Scene3DHit>>>>;
using MeshView = flight::StructuralRef<
    flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::Mesh>>>>;
using NodeView = flight::StructuralRef<
    flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::Node3D>>>>;
using OptionsView = flight::StructuralRef<flight::RowReadonly<
    flight::RowOf<flight::Ref<flight::types::Scene3DPickOptions>>>>;
using RayView = flight::StructuralRef<
    flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::Ray3D>>>>;
using SceneTraits = flight::Ref<flight::types::Node3DTraits>;
using SceneNodeView = flight::StructuralRef<flight::RowReadonly<flight::RowMerge<
    flight::RowOf<std::shared_ptr<flight::types::Node<SceneTraits>>>,
    flight::RowOf<SceneTraits>>>>;

template <typename Type>
using ReadonlyRef = flight::StructuralRef<
    flight::RowReadonly<flight::RowOf<flight::Ref<Type>>>>;

template <typename Type>
[[nodiscard]] inline ReadonlyRef<Type> readonly_ref(
    const flight::Ref<Type>& value) {
  return ReadonlyRef<Type>(value);
}

[[nodiscard]] inline flight::Ref<flight::types::Scene3DHit>
exact_hit(const HitView& hit) {
  auto exact = hit.template shared_exact_object<flight::types::Scene3DHit>();
  if (!exact) throw std::bad_cast();
  return exact;
}

inline void initialize_scene3_dhit(
    flight::types::EntityConstruction<
        flight::Ref<flight::types::Scene3DHit>> out) {
  auto exact = out.template shared_exact_object<flight::types::Scene3DHit>();
  if (!exact) throw std::bad_cast();
  exact->distance = 0.0;
  exact->node = std::nullopt;
  exact->normal_x = 0.0;
  exact->normal_y = 0.0;
  exact->normal_z = 0.0;
  exact->point_x = 0.0;
  exact->point_y = 0.0;
  exact->point_z = 0.0;
  exact->triangle_index = -1.0;
  exact->u = 0.0;
  exact->v = 0.0;
  exact->w = 0.0;
}

[[nodiscard]] inline flight::Ref<flight::types::Scene3DHit>
create_scene3_dhit() {
  auto out = flight::entity::allocate_entity<
      flight::Ref<flight::types::Scene3DHit>>();
  initialize_scene3_dhit(out);
  return flight::entity::finish_entity<
      flight::Ref<flight::types::Scene3DHit>>(out);
}

inline void copy_scene3_dhit(
    const flight::Ref<flight::types::Scene3DHit>& out,
    const HitView& source_view) {
  const auto source = exact_hit(source_view);
  out->node = source->node;
  out->distance = source->distance;
  out->triangle_index = source->triangle_index;
  out->u = source->u;
  out->v = source->v;
  out->w = source->w;
  out->point_x = source->point_x;
  out->point_y = source->point_y;
  out->point_z = source->point_z;
  out->normal_x = source->normal_x;
  out->normal_y = source->normal_y;
  out->normal_z = source->normal_z;
}

[[nodiscard]] inline bool build_camera_pick_ray(
    const flight::Ref<flight::types::Ray3D>& out,
    const CameraView& camera_view, double screen_x, double screen_y) {
  const auto camera =
      camera_view.template shared_exact_object<flight::types::Camera3D>();
  if (!camera) throw std::bad_cast();
  const double aspect = std::visit(
      [](const auto& projection) {
        using Projection =
            typename std::remove_cvref_t<decltype(projection)>::element_type;
        if constexpr (std::same_as<Projection,
                                   flight::types::PerspectiveProjection>) {
          return projection->aspect;
        }
        return 1.0;
      },
      camera->projection);
  return flight::camera::get_camera3_dscreen_to_world_ray(
      out, camera_view, screen_x, screen_y, aspect);
}

inline void transform_point_by_matrix4(
    const flight::Ref<flight::types::Vector3>& out,
    const flight::Ref<flight::types::Vector3>& point,
    const flight::Float32Array& matrix) {
  const double x = point->x;
  const double y = point->y;
  const double z = point->z;
  out->x = matrix.get_index(0.0) * x + matrix.get_index(4.0) * y +
           matrix.get_index(8.0) * z + matrix.get_index(12.0);
  out->y = matrix.get_index(1.0) * x + matrix.get_index(5.0) * y +
           matrix.get_index(9.0) * z + matrix.get_index(13.0);
  out->z = matrix.get_index(2.0) * x + matrix.get_index(6.0) * y +
           matrix.get_index(10.0) * z + matrix.get_index(14.0);
}

inline void transform_direction_by_matrix4(
    const flight::Ref<flight::types::Vector3>& out,
    const flight::Ref<flight::types::Vector3>& direction,
    const flight::Float32Array& matrix) {
  const double x = direction->x;
  const double y = direction->y;
  const double z = direction->z;
  out->x = matrix.get_index(0.0) * x + matrix.get_index(4.0) * y +
           matrix.get_index(8.0) * z;
  out->y = matrix.get_index(1.0) * x + matrix.get_index(5.0) * y +
           matrix.get_index(9.0) * z;
  out->z = matrix.get_index(2.0) * x + matrix.get_index(6.0) * y +
           matrix.get_index(10.0) * z;
}

inline void transform_aabb_exact(
    const flight::Ref<flight::types::Aabb>& out,
    const flight::Ref<flight::types::Aabb>& bounds,
    const flight::Float32Array& matrix) {
  out->min->x = out->min->y = out->min->z =
      std::numeric_limits<double>::infinity();
  out->max->x = out->max->y = out->max->z =
      -std::numeric_limits<double>::infinity();
  auto point = flight::geometry::create_vector3();
  auto transformed = flight::geometry::create_vector3();
  for (int corner = 0; corner < 8; ++corner) {
    point->x = (corner & 1) ? bounds->max->x : bounds->min->x;
    point->y = (corner & 2) ? bounds->max->y : bounds->min->y;
    point->z = (corner & 4) ? bounds->max->z : bounds->min->z;
    transform_point_by_matrix4(transformed, point, matrix);
    out->min->x = std::min(out->min->x, transformed->x);
    out->min->y = std::min(out->min->y, transformed->y);
    out->min->z = std::min(out->min->z, transformed->z);
    out->max->x = std::max(out->max->x, transformed->x);
    out->max->y = std::max(out->max->y, transformed->y);
    out->max->z = std::max(out->max->z, transformed->z);
  }
}

[[nodiscard]] inline double intersect_ray_aabb_exact(
    const flight::Ref<flight::types::Ray3D>& ray,
    const flight::Ref<flight::types::Aabb>& bounds) {
  if (bounds->min->x > bounds->max->x ||
      bounds->min->y > bounds->max->y ||
      bounds->min->z > bounds->max->z) {
    return -1.0;
  }
  const double dx = ray->direction->x;
  const double dy = ray->direction->y;
  const double dz = ray->direction->z;
  if (dx * dx + dy * dy + dz * dz == 0.0) return -1.0;
  double t_min = 0.0;
  double t_max = std::numeric_limits<double>::infinity();
  const auto axis = [&](double origin, double direction, double minimum,
                        double maximum) {
    if (direction == 0.0) return origin >= minimum && origin <= maximum;
    double first = (minimum - origin) / direction;
    double second = (maximum - origin) / direction;
    if (first > second) std::swap(first, second);
    t_min = std::max(t_min, first);
    t_max = std::min(t_max, second);
    return t_min <= t_max;
  };
  if (!axis(ray->origin->x, dx, bounds->min->x, bounds->max->x) ||
      !axis(ray->origin->y, dy, bounds->min->y, bounds->max->y) ||
      !axis(ray->origin->z, dz, bounds->min->z, bounds->max->z)) {
    return -1.0;
  }
  return t_min;
}

[[nodiscard]] inline bool write_face_normal(
    const flight::Ref<flight::types::Vector3>& out,
    const flight::Ref<flight::types::Vector3>& a,
    const flight::Ref<flight::types::Vector3>& b,
    const flight::Ref<flight::types::Vector3>& c) {
  const double e1x = b->x - a->x;
  const double e1y = b->y - a->y;
  const double e1z = b->z - a->z;
  const double e2x = c->x - a->x;
  const double e2y = c->y - a->y;
  const double e2z = c->z - a->z;
  const double nx = e1y * e2z - e1z * e2y;
  const double ny = e1z * e2x - e1x * e2z;
  const double nz = e1x * e2y - e1y * e2x;
  const double length_squared = nx * nx + ny * ny + nz * nz;
  if (length_squared == 0.0) return false;
  const double inverse_length = 1.0 / std::sqrt(length_squared);
  out->x = nx * inverse_length;
  out->y = ny * inverse_length;
  out->z = nz * inverse_length;
  return true;
}

inline void write_barycentric(
    const flight::Ref<flight::types::Scene3DHit>& out,
    const flight::Ref<flight::types::Vector3>& point,
    const flight::Ref<flight::types::Vector3>& a,
    const flight::Ref<flight::types::Vector3>& b,
    const flight::Ref<flight::types::Vector3>& c) {
  const double v0x = b->x - a->x;
  const double v0y = b->y - a->y;
  const double v0z = b->z - a->z;
  const double v1x = c->x - a->x;
  const double v1y = c->y - a->y;
  const double v1z = c->z - a->z;
  const double v2x = point->x - a->x;
  const double v2y = point->y - a->y;
  const double v2z = point->z - a->z;
  const double d00 = v0x * v0x + v0y * v0y + v0z * v0z;
  const double d01 = v0x * v1x + v0y * v1y + v0z * v1z;
  const double d11 = v1x * v1x + v1y * v1y + v1z * v1z;
  const double d20 = v2x * v0x + v2y * v0y + v2z * v0z;
  const double d21 = v2x * v1x + v2y * v1y + v2z * v1z;
  const double denominator = d00 * d11 - d01 * d01;
  if (denominator == 0.0) {
    out->u = 1.0;
    out->v = 0.0;
    out->w = 0.0;
    return;
  }
  const double inverse = 1.0 / denominator;
  const double v = (d11 * d20 - d01 * d21) * inverse;
  const double w = (d00 * d21 - d01 * d20) * inverse;
  out->u = 1.0 - v - w;
  out->v = v;
  out->w = w;
}

inline void intersect_mesh_triangles(
    const flight::Ref<flight::types::Mesh>& mesh, const RayView& ray_view,
    double max_distance, bool cull_backfaces,
    const std::function<void(HitView)>& on_hit) {
  const auto ray = ray_view.template shared_exact_object<flight::types::Ray3D>();
  if (!ray) throw std::bad_cast();
  const auto world = detail::node_world_matrix4(mesh);
  const auto world_view = readonly_ref<flight::types::Matrix4>(world);

  auto world_bounds = flight::geometry::create_aabb();
  const auto runtime = mesh->entity_runtime_key;
  const auto* posed = runtime.has_value()
                           ? std::get_if<flight::Ref<flight::types::Aabb>>(
                                 &runtime.value()->deformed_local_bounds)
                           : nullptr;
  if (posed && *posed) {
    transform_aabb_exact(world_bounds, *posed, world->m);
  } else {
    const auto local_bounds =
        flight::mesh::ensure_mesh_geometry_bounds(mesh->geometry);
    if (!local_bounds.has_value()) return;
    transform_aabb_exact(world_bounds, local_bounds.value(), world->m);
  }
  if (intersect_ray_aabb_exact(ray, world_bounds) < 0.0) return;

  auto inverse_world = flight::geometry::create_matrix4();
  if (!flight::geometry::inverse_matrix4(inverse_world, world_view)) return;
  auto local_ray = flight::geometry::create_ray3_d();
  transform_point_by_matrix4(local_ray->origin, ray->origin, inverse_world->m);
  transform_direction_by_matrix4(local_ray->direction, ray->direction,
                                 inverse_world->m);

  auto vertex0 = flight::make_ref<flight::types::x_y_z_82615fd0df8619c4>();
  auto vertex1 = flight::make_ref<flight::types::x_y_z_82615fd0df8619c4>();
  auto vertex2 = flight::make_ref<flight::types::x_y_z_82615fd0df8619c4>();
  auto a = flight::geometry::create_vector3();
  auto b = flight::geometry::create_vector3();
  auto c = flight::geometry::create_vector3();
  auto world_a = flight::geometry::create_vector3();
  auto world_b = flight::geometry::create_vector3();
  auto world_c = flight::geometry::create_vector3();
  auto world_normal = flight::geometry::create_vector3();
  auto local_point = flight::geometry::create_vector3();
  auto world_point = flight::geometry::create_vector3();
  auto triangle =
      flight::make_ref<flight::types::MeshTriangleVertexIndices>();
  auto hit = create_scene3_dhit();

  const double triangle_count =
      flight::mesh::get_mesh_geometry_triangle_count(mesh->geometry);
  for (double triangle_index = 0.0; triangle_index < triangle_count;
       triangle_index += 1.0) {
    if (!flight::mesh::get_mesh_geometry_triangle_vertex_indices(
            triangle, mesh->geometry, triangle_index)) {
      continue;
    }
    if (!flight::mesh::get_mesh_geometry_vertex_position(
            vertex0, mesh->geometry, triangle->i0) ||
        !flight::mesh::get_mesh_geometry_vertex_position(
            vertex1, mesh->geometry, triangle->i1) ||
        !flight::mesh::get_mesh_geometry_vertex_position(
            vertex2, mesh->geometry, triangle->i2)) {
      continue;
    }
    a->x = vertex0->x;
    a->y = vertex0->y;
    a->z = vertex0->z;
    b->x = vertex1->x;
    b->y = vertex1->y;
    b->z = vertex1->z;
    c->x = vertex2->x;
    c->y = vertex2->y;
    c->z = vertex2->z;
    const double distance = flight::geometry::intersect_ray3_dtriangle(
        readonly_ref<flight::types::Ray3D>(local_ray),
        readonly_ref<flight::types::Vector3>(a),
        readonly_ref<flight::types::Vector3>(b),
        readonly_ref<flight::types::Vector3>(c));
    if (distance < 0.0 || distance > max_distance) continue;

    transform_point_by_matrix4(world_a, a, world->m);
    transform_point_by_matrix4(world_b, b, world->m);
    transform_point_by_matrix4(world_c, c, world->m);
    if (!write_face_normal(world_normal, world_a, world_b, world_c)) continue;
    const double facing = ray->direction->x * world_normal->x +
                          ray->direction->y * world_normal->y +
                          ray->direction->z * world_normal->z;
    if (cull_backfaces && facing > 0.0) continue;

    hit->node = mesh;
    hit->distance = distance;
    hit->triangle_index = triangle_index;
    hit->normal_x = world_normal->x;
    hit->normal_y = world_normal->y;
    hit->normal_z = world_normal->z;
    world_point->x = ray->origin->x + ray->direction->x * distance;
    world_point->y = ray->origin->y + ray->direction->y * distance;
    world_point->z = ray->origin->z + ray->direction->z * distance;
    hit->point_x = world_point->x;
    hit->point_y = world_point->y;
    hit->point_z = world_point->z;
    local_point->x = local_ray->origin->x + local_ray->direction->x * distance;
    local_point->y = local_ray->origin->y + local_ray->direction->y * distance;
    local_point->z = local_ray->origin->z + local_ray->direction->z * distance;
    write_barycentric(hit, local_point, a, b, c);
    on_hit(readonly_ref<flight::types::Scene3DHit>(hit));
  }
}

inline void pick_node(
    const SceneNodeView& node, const RayView& ray,
    const std::optional<std::function<bool(MeshView)>>& predicate,
    double max_distance, bool cull_backfaces,
    const std::function<void(HitView)>& on_hit) {
  if (!flight::row_get<flight::RowKey<"enabled">>(node)) return;
  if (const auto mesh =
          node.template shared_exact_object<flight::types::Mesh>()) {
    const MeshView mesh_view(mesh);
    if (!predicate.has_value() || predicate.value()(mesh_view)) {
      intersect_mesh_triangles(mesh, ray, max_distance, cull_backfaces,
                               on_hit);
    }
  }
  const auto runtime = flight::node::get_node_runtime<SceneTraits>(node);
  const auto children =
      flight::row_get<flight::RowKey<"children">>(runtime);
  if (!children.has_value()) return;
  for (const auto& child : children.value()) {
    pick_node(flight::structural_ref_cast<SceneNodeView>(child), ray,
              predicate, max_distance, cull_backfaces, on_hit);
  }
}

inline void for_each_scene3_dray_hit(
    const NodeView& scene, const RayView& ray,
    const std::optional<OptionsView>& options,
    const std::function<void(HitView)>& on_hit) {
  std::optional<std::function<bool(MeshView)>> predicate;
  double max_distance = std::numeric_limits<double>::infinity();
  bool cull_backfaces = false;
  if (options.has_value()) {
    const auto exact = options.value().template shared_exact_object<
        flight::types::Scene3DPickOptions>();
    if (!exact) throw std::bad_cast();
    predicate = exact->predicate;
    max_distance = exact->max_distance.value_or(max_distance);
    cull_backfaces = exact->cull_backfaces.value_or(false);
  }
  pick_node(flight::structural_ref_cast<SceneNodeView>(scene), ray,
            predicate, max_distance, cull_backfaces, on_hit);
}

inline void stable_sort_hits(
    const flight::Array<flight::Ref<flight::types::Scene3DHit>>& hits) {
  for (std::size_t index = 1; index < hits.size(); ++index) {
    const auto value = hits.element(static_cast<double>(index));
    std::size_t insertion = index;
    while (insertion > 0) {
      const auto previous = hits.element(static_cast<double>(insertion - 1));
      if (previous->distance <= value->distance) break;
      hits.element(static_cast<double>(insertion)) = previous;
      --insertion;
    }
    hits.element(static_cast<double>(insertion)) = value;
  }
}

[[nodiscard]] inline flight::Array<flight::Ref<flight::types::Scene3DHit>>
pick_scene3_dall_with_ray3_d(
    const NodeView& scene, const RayView& ray,
    flight::Array<flight::Ref<flight::types::Scene3DHit>> out_array,
    std::optional<OptionsView> options = std::nullopt) {
  std::size_t count = 0;
  for_each_scene3_dray_hit(
      scene, ray, options, [&](const HitView& hit) {
        auto slot = out_array.get(static_cast<double>(count));
        if (!slot.has_value()) {
          slot = create_scene3_dhit();
          out_array.element(static_cast<double>(count)) = slot.value();
        }
        copy_scene3_dhit(slot.value(), hit);
        ++count;
      });
  out_array.resize(static_cast<double>(count));
  stable_sort_hits(out_array);
  return out_array;
}

[[nodiscard]] inline std::optional<flight::Ref<flight::types::Scene3DHit>>
pick_scene3_dwith_ray3_d(
    const NodeView& scene, const RayView& ray,
    const flight::Ref<flight::types::Scene3DHit>& out,
    std::optional<OptionsView> options = std::nullopt) {
  bool found = false;
  double best_distance = std::numeric_limits<double>::infinity();
  for_each_scene3_dray_hit(
      scene, ray, options, [&](const HitView& hit_view) {
        const auto hit = exact_hit(hit_view);
        if (hit->distance >= best_distance) return;
        best_distance = hit->distance;
        found = true;
        copy_scene3_dhit(out, hit_view);
      });
  return found ? std::optional<flight::Ref<flight::types::Scene3DHit>>(out)
               : std::nullopt;
}

[[nodiscard]] inline flight::Array<flight::Ref<flight::types::Scene3DHit>>
pick_scene3_dall(
    const NodeView& scene, const CameraView& camera, double screen_x,
    double screen_y,
    flight::Array<flight::Ref<flight::types::Scene3DHit>> out_array,
    std::optional<OptionsView> options = std::nullopt) {
  const auto camera_ray = flight::geometry::create_ray3_d();
  if (!build_camera_pick_ray(camera_ray, camera, screen_x, screen_y)) {
    out_array.resize(0.0);
    return out_array;
  }
  return pick_scene3_dall_with_ray3_d(
      scene, readonly_ref<flight::types::Ray3D>(camera_ray), out_array,
      options);
}

[[nodiscard]] inline std::optional<flight::Ref<flight::types::Scene3DHit>>
pick_scene3_d(
    const NodeView& scene, const CameraView& camera, double screen_x,
    double screen_y, const flight::Ref<flight::types::Scene3DHit>& out,
    std::optional<OptionsView> options = std::nullopt) {
  const auto camera_ray = flight::geometry::create_ray3_d();
  if (!build_camera_pick_ray(camera_ray, camera, screen_x, screen_y)) {
    return std::nullopt;
  }
  return pick_scene3_dwith_ray3_d(
      scene, readonly_ref<flight::types::Ray3D>(camera_ray), out, options);
}

}  // namespace flight::picking
