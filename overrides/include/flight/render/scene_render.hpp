// Native completion of render/src/sceneRender.ts for flattened C++ owners.
#pragma once

#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>

#include <flight/camera/projection.hpp>
#include <flight/camera/reflection.hpp>
#include <flight/color/pack_color.hpp>
#include <flight/geometry/aabb.hpp>
#include <flight/geometry/frustum.hpp>
#include <flight/geometry/matrix4.hpp>
#include <flight/mesh/mesh_geometry_compute.hpp>
#include <flight/node/node_transform3d.hpp>
#include <flight/runtime.hpp>
#include <flight/structural_ref.hpp>
#include <flight/types/camera3_d.hpp>
#include <flight/types/instanced_mesh.hpp>
#include <flight/types/mesh.hpp>
#include <flight/types/node3_d.hpp>
#include <flight/types/render_state.hpp>
#include <flight/types/scene3_dlight_block.hpp>
#include <flight/types/scene3_dlights.hpp>
#include <flight/types/scene3_drender_list.hpp>
#include <flight/weak_map.hpp>

namespace flight::render {

using SceneNodeView = flight::StructuralRef<flight::RowReadonly<
    flight::RowOf<flight::Ref<flight::types::Node3D>>>>;
using Camera3DView = flight::StructuralRef<flight::RowReadonly<
    flight::RowOf<flight::Ref<flight::types::Camera3D>>>>;
using SceneLightsView = flight::StructuralRef<flight::RowReadonly<
    flight::RowOf<flight::types::Scene3DLightsLike>>>;

template <typename Object>
[[nodiscard]] inline auto readonly_object(
    const std::shared_ptr<Object>& object) {
  return flight::structural_ref_cast<flight::StructuralRef<flight::RowReadonly<
      flight::RowOf<flight::Ref<Object>>>>>(
      flight::StructuralRef<flight::RowWritable<
          flight::RowOf<flight::Ref<Object>>>>(object));
}

template <typename Object, typename Schema>
[[nodiscard]] inline flight::Ref<Object> exact_object(
    const flight::StructuralRef<Schema>& view, const char* description) {
  auto object = view.template shared_exact_object<Object>();
  if (!object) {
    throw std::logic_error(std::string("Scene3D ") + description +
                           " lost its exact native owner");
  }
  return object;
}

template <typename Object>
[[nodiscard]] inline flight::Ref<Object> exact_object(
    const std::shared_ptr<Object>& object, const char*) {
  return object;
}

struct PreparedScene3D : public flight::ReferenceEnabled {
  flight::Ref<flight::types::Frustum> frustum;
  flight::Array<flight::Ref<flight::types::InstancedMesh>> instanced_meshes;
  flight::Ref<flight::types::Scene3DLightBlock> light_block;
  flight::Ref<flight::types::Scene3DRenderList> list;
  flight::Array<flight::Ref<flight::types::Mesh>> meshes;
  flight::Ref<flight::types::Matrix4> view_projection;
  flight::Ref<flight::types::Aabb> world_bounds;
};

template <typename State>
inline flight::WeakMap<std::shared_ptr<State>, flight::Ref<PreparedScene3D>>&
prepared_scene3_dcache() {
  static flight::WeakMap<std::shared_ptr<State>, flight::Ref<PreparedScene3D>>
      cache;
  return cache;
}

template <typename State>
inline flight::Ref<PreparedScene3D> ensure_prepared_scene3_d(
    std::shared_ptr<State> state) {
  auto& cache = prepared_scene3_dcache<State>();
  if (auto existing = cache.get(state); existing.has_value()) {
    return existing.value();
  }
  auto view_projection = flight::geometry::create_matrix4();
  auto light_block = flight::make_ref<flight::types::Scene3DLightBlock>(
      flight::types::Scene3DLightBlock{
          .ambient_count = 0.0,
          .data = flight::Float32Array(flight::types::scene_light_block_floats),
          .directional_count = 0.0,
          .hemisphere_count = 0.0,
          .point_count = 0.0,
          .spot_count = 0.0,
          .version = 0.0,
      });
  flight::Array<flight::Ref<flight::types::Mesh>> meshes;
  flight::Array<flight::Ref<flight::types::InstancedMesh>> instanced_meshes;
  auto list = flight::make_ref<flight::types::Scene3DRenderList>(
      flight::types::Scene3DRenderList{
          .instanced_mesh_count = 0.0,
          .lights = readonly_object(light_block),
          .mesh_count = 0.0,
          .view_projection = readonly_object(view_projection),
          .visible_instanced_meshes = instanced_meshes,
          .visible_meshes = meshes,
      });
  auto prepared = flight::make_ref<PreparedScene3D>(PreparedScene3D{
      .frustum = flight::geometry::create_frustum(),
      .instanced_meshes = instanced_meshes,
      .light_block = light_block,
      .list = list,
      .meshes = meshes,
      .view_projection = view_projection,
      .world_bounds = flight::geometry::create_aabb(),
  });
  cache.set(state, prepared);
  return prepared;
}

inline flight::types::LinearColor scratch_color =
    flight::Array<double>{0.0, 0.0, 0.0, 0.0};
inline flight::Ref<flight::types::Matrix4> scratch_projection =
    flight::geometry::create_matrix4();
inline flight::Float32Array scratch_light_data =
    flight::Float32Array(flight::types::scene_light_block_floats);
inline flight::Ref<flight::types::Aabb> scratch_instance_bounds =
    flight::geometry::create_aabb();
inline std::optional<std::function<void(flight::Ref<flight::types::Mesh>)>>
    skinned_bounds_guard = std::nullopt;

inline bool is_float32_array_equal(flight::Float32Array a,
                                   flight::Float32Array b) {
  if (a.size() != b.size()) return false;
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (a.get_index(static_cast<double>(i)) !=
        b.get_index(static_cast<double>(i))) {
      return false;
    }
  }
  return true;
}

inline void apply_scene3_dprojection_jitter(
    flight::Ref<flight::types::Matrix4> out, double x, double y) {
  auto m = out->m;
  m.set_index(0.0, m.get_index(0.0) + x * m.get_index(3.0));
  m.set_index(4.0, m.get_index(4.0) + x * m.get_index(7.0));
  m.set_index(8.0, m.get_index(8.0) + x * m.get_index(11.0));
  m.set_index(12.0, m.get_index(12.0) + x * m.get_index(15.0));
  m.set_index(1.0, m.get_index(1.0) + y * m.get_index(3.0));
  m.set_index(5.0, m.get_index(5.0) + y * m.get_index(7.0));
  m.set_index(9.0, m.get_index(9.0) + y * m.get_index(11.0));
  m.set_index(13.0, m.get_index(13.0) + y * m.get_index(15.0));
}

inline double resolve_scene3_dviewport_aspect(
    flight::Ref<flight::types::Camera3D> camera,
    std::optional<double> viewport_aspect) {
  if (viewport_aspect.has_value() && std::isfinite(viewport_aspect.value()) &&
      viewport_aspect.value() > 0.0) {
    return viewport_aspect.value();
  }
  if (const auto* perspective =
          std::get_if<flight::Ref<flight::types::PerspectiveProjection>>(
              &camera->projection);
      perspective && std::isfinite((*perspective)->aspect) &&
      (*perspective)->aspect > 0.0) {
    return (*perspective)->aspect;
  }
  return 1.0;
}

inline void set_scene3_dview_projection_matrix4(
    flight::Ref<flight::types::Matrix4> out,
    flight::Ref<flight::types::Camera3D> camera, double aspect) {
  flight::camera::set_projection_matrix4(scratch_projection,
                                          camera->projection, aspect,
                                          camera->near, camera->far);
  apply_scene3_dprojection_jitter(scratch_projection, camera->jitter->x,
                                  camera->jitter->y);
  if (camera->near_clip_plane.has_value()) {
    flight::camera::apply_oblique_near_clip_plane(
        scratch_projection, readonly_object(camera->near_clip_plane.value()));
  }
  flight::geometry::multiply_matrix4(
      out, readonly_object(scratch_projection), readonly_object(camera->view));
}

inline void pack_ambient_light(flight::Float32Array data,
                               flight::Ref<flight::types::AmbientLight> light) {
  flight::color::unpack_color_to_linear(scratch_color, light->color);
  data.set_index(flight::types::scene_light_ambient_radiance_offset,
                 scratch_color.element(0.0) * light->intensity);
  data.set_index(flight::types::scene_light_ambient_radiance_offset + 1.0,
                 scratch_color.element(1.0) * light->intensity);
  data.set_index(flight::types::scene_light_ambient_radiance_offset + 2.0,
                 scratch_color.element(2.0) * light->intensity);
}

inline void pack_directional_light(
    flight::Float32Array data,
    flight::Ref<flight::types::DirectionalLight> light) {
  data.set_index(flight::types::scene_light_directional_direction_offset,
                 light->direction->x);
  data.set_index(flight::types::scene_light_directional_direction_offset + 1.0,
                 light->direction->y);
  data.set_index(flight::types::scene_light_directional_direction_offset + 2.0,
                 light->direction->z);
  flight::color::unpack_color_to_linear(scratch_color, light->color);
  data.set_index(flight::types::scene_light_directional_radiance_offset,
                 scratch_color.element(0.0) * light->intensity);
  data.set_index(flight::types::scene_light_directional_radiance_offset + 1.0,
                 scratch_color.element(1.0) * light->intensity);
  data.set_index(flight::types::scene_light_directional_radiance_offset + 2.0,
                 scratch_color.element(2.0) * light->intensity);
}

template <typename Light>
inline void pack_positional_light_head(flight::Float32Array data,
                                       double offset,
                                       std::shared_ptr<Light> light) {
  data.set_index(offset, light->position->x);
  data.set_index(offset + 1.0, light->position->y);
  data.set_index(offset + 2.0, light->position->z);
  data.set_index(offset + 3.0, light->range);
  flight::color::unpack_color_to_linear(scratch_color, light->color);
  data.set_index(offset + 4.0, scratch_color.element(0.0) * light->intensity);
  data.set_index(offset + 5.0, scratch_color.element(1.0) * light->intensity);
  data.set_index(offset + 6.0, scratch_color.element(2.0) * light->intensity);
  data.set_index(offset + 7.0,
                 light->range > 0.0 ? 1.0 / (light->range * light->range)
                                    : 0.0);
}

inline void pack_point_light(flight::Float32Array data, double offset,
                             flight::Ref<flight::types::PointLight> light) {
  pack_positional_light_head(data, offset, light);
}

inline void pack_spot_light(flight::Float32Array data, double offset,
                            flight::Ref<flight::types::SpotLight> light) {
  pack_positional_light_head(data, offset, light);
  data.set_index(offset + 8.0, light->direction->x);
  data.set_index(offset + 9.0, light->direction->y);
  data.set_index(offset + 10.0, light->direction->z);
  data.set_index(offset + 12.0, light->inner_cone_cos);
  data.set_index(offset + 13.0, light->outer_cone_cos);
}

inline void pack_hemisphere_light(
    flight::Float32Array data, double offset,
    flight::Ref<flight::types::HemisphereLight> light) {
  flight::color::unpack_color_to_linear(scratch_color, light->sky_color);
  data.set_index(offset, scratch_color.element(0.0) * light->intensity);
  data.set_index(offset + 1.0, scratch_color.element(1.0) * light->intensity);
  data.set_index(offset + 2.0, scratch_color.element(2.0) * light->intensity);
  flight::color::unpack_color_to_linear(scratch_color, light->ground_color);
  data.set_index(offset + 4.0, scratch_color.element(0.0) * light->intensity);
  data.set_index(offset + 5.0, scratch_color.element(1.0) * light->intensity);
  data.set_index(offset + 6.0, scratch_color.element(2.0) * light->intensity);
  data.set_index(offset + 8.0, 0.0);
  data.set_index(offset + 9.0, 1.0);
  data.set_index(offset + 10.0, 0.0);
}

inline void pack_scene3_dlight_block(
    flight::Ref<flight::types::Scene3DLightBlock> out,
    flight::Ref<flight::types::Scene3DLights> lights) {
  scratch_light_data.fill(0.0);
  double directional_count = 0.0;
  if (lights->directional.has_value()) {
    pack_directional_light(
        scratch_light_data,
        exact_object<flight::types::DirectionalLight>(
            lights->directional.value(), "directional light"));
    directional_count = 1.0;
  }
  double ambient_count = 0.0;
  if (lights->ambient.has_value()) {
    pack_ambient_light(
        scratch_light_data,
        exact_object<flight::types::AmbientLight>(lights->ambient.value(),
                                                  "ambient light"));
    ambient_count = 1.0;
  }
  const auto count_for = [](const auto& source) {
    return source.has_value()
               ? std::min<std::size_t>(
                     source->size(), static_cast<std::size_t>(
                                         flight::types::max_forward_lights))
               : std::size_t{0};
  };
  const auto point_count = count_for(lights->point);
  for (std::size_t i = 0; i < point_count; ++i) {
    pack_point_light(
        scratch_light_data,
        flight::types::scene_light_point_offset +
            static_cast<double>(i) * flight::types::scene_light_point_stride,
        exact_object<flight::types::PointLight>(
            lights->point->element(static_cast<double>(i)), "point light"));
  }
  const auto spot_count = count_for(lights->spot);
  for (std::size_t i = 0; i < spot_count; ++i) {
    pack_spot_light(
        scratch_light_data,
        flight::types::scene_light_spot_offset +
            static_cast<double>(i) * flight::types::scene_light_spot_stride,
        exact_object<flight::types::SpotLight>(
            lights->spot->element(static_cast<double>(i)), "spot light"));
  }
  const auto hemisphere_count = count_for(lights->hemisphere);
  for (std::size_t i = 0; i < hemisphere_count; ++i) {
    pack_hemisphere_light(
        scratch_light_data,
        flight::types::scene_light_hemisphere_offset +
            static_cast<double>(i) *
                flight::types::scene_light_hemisphere_stride,
        exact_object<flight::types::HemisphereLight>(
            lights->hemisphere->element(static_cast<double>(i)),
            "hemisphere light"));
  }

  if (out->directional_count == directional_count &&
      out->ambient_count == ambient_count &&
      out->point_count == static_cast<double>(point_count) &&
      out->spot_count == static_cast<double>(spot_count) &&
      out->hemisphere_count == static_cast<double>(hemisphere_count) &&
      is_float32_array_equal(out->data, scratch_light_data)) {
    return;
  }
  out->data.set(scratch_light_data);
  out->directional_count = directional_count;
  out->ambient_count = ambient_count;
  out->point_count = static_cast<double>(point_count);
  out->spot_count = static_cast<double>(spot_count);
  out->hemisphere_count = static_cast<double>(hemisphere_count);
  out->version += 1.0;
}

inline flight::Ref<flight::types::Aabb> ensure_instanced_mesh_local_bounds(
    flight::Ref<flight::types::InstancedMesh> mesh,
    flight::Ref<flight::types::Aabb> bounds) {
  if (!mesh->entity_runtime_key.has_value()) {
    throw std::logic_error("InstancedMesh has no runtime owner");
  }
  auto runtime = mesh->entity_runtime_key.value();
  if (const auto* cached =
          std::get_if<flight::Ref<flight::types::Aabb>>(
              &runtime->instance_local_bounds);
      cached && runtime->instance_local_bounds_version.has_value() &&
      runtime->instance_local_bounds_version.value() == mesh->version) {
    return *cached;
  }
  auto out = std::get_if<flight::Ref<flight::types::Aabb>>(
                 &runtime->instance_local_bounds)
                 ? std::get<flight::Ref<flight::types::Aabb>>(
                       runtime->instance_local_bounds)
                 : flight::geometry::create_aabb();
  for (std::size_t i = 0;
       i < static_cast<std::size_t>(mesh->instance_count); ++i) {
    const auto matrix =
        mesh->instance_matrices.element(static_cast<double>(i))->m;
    const double cx = (bounds->min->x + bounds->max->x) * 0.5;
    const double cy = (bounds->min->y + bounds->max->y) * 0.5;
    const double cz = (bounds->min->z + bounds->max->z) * 0.5;
    const double ex = (bounds->max->x - bounds->min->x) * 0.5;
    const double ey = (bounds->max->y - bounds->min->y) * 0.5;
    const double ez = (bounds->max->z - bounds->min->z) * 0.5;
    const double tcx = matrix.get_index(0.0) * cx +
                       matrix.get_index(4.0) * cy +
                       matrix.get_index(8.0) * cz + matrix.get_index(12.0);
    const double tcy = matrix.get_index(1.0) * cx +
                       matrix.get_index(5.0) * cy +
                       matrix.get_index(9.0) * cz + matrix.get_index(13.0);
    const double tcz = matrix.get_index(2.0) * cx +
                       matrix.get_index(6.0) * cy +
                       matrix.get_index(10.0) * cz + matrix.get_index(14.0);
    const double tex = std::abs(matrix.get_index(0.0)) * ex +
                       std::abs(matrix.get_index(4.0)) * ey +
                       std::abs(matrix.get_index(8.0)) * ez;
    const double tey = std::abs(matrix.get_index(1.0)) * ex +
                       std::abs(matrix.get_index(5.0)) * ey +
                       std::abs(matrix.get_index(9.0)) * ez;
    const double tez = std::abs(matrix.get_index(2.0)) * ex +
                       std::abs(matrix.get_index(6.0)) * ey +
                       std::abs(matrix.get_index(10.0)) * ez;
    scratch_instance_bounds->min->x = tcx - tex;
    scratch_instance_bounds->min->y = tcy - tey;
    scratch_instance_bounds->min->z = tcz - tez;
    scratch_instance_bounds->max->x = tcx + tex;
    scratch_instance_bounds->max->y = tcy + tey;
    scratch_instance_bounds->max->z = tcz + tez;
    if (i == 0) {
      out->min->x = scratch_instance_bounds->min->x;
      out->min->y = scratch_instance_bounds->min->y;
      out->min->z = scratch_instance_bounds->min->z;
      out->max->x = scratch_instance_bounds->max->x;
      out->max->y = scratch_instance_bounds->max->y;
      out->max->z = scratch_instance_bounds->max->z;
    } else {
      out->min->x = std::min(out->min->x, scratch_instance_bounds->min->x);
      out->min->y = std::min(out->min->y, scratch_instance_bounds->min->y);
      out->min->z = std::min(out->min->z, scratch_instance_bounds->min->z);
      out->max->x = std::max(out->max->x, scratch_instance_bounds->max->x);
      out->max->y = std::max(out->max->y, scratch_instance_bounds->max->y);
      out->max->z = std::max(out->max->z, scratch_instance_bounds->max->z);
    }
  }
  runtime->instance_local_bounds = out;
  runtime->instance_local_bounds_version = mesh->version;
  return out;
}

inline bool is_instanced_mesh_visible(
    flight::Ref<flight::types::InstancedMesh> mesh,
    flight::Ref<flight::types::Frustum> frustum,
    flight::Ref<flight::types::Aabb> world_bounds) {
  auto bounds = flight::mesh::ensure_mesh_geometry_bounds(mesh->geometry);
  if (!bounds.has_value()) return true;
  auto exact_bounds = exact_object<flight::types::Aabb>(bounds.value(),
                                                        "geometry bounds");
  auto local_bounds = ensure_instanced_mesh_local_bounds(mesh, exact_bounds);
  auto world_matrix = flight::node::get_node_world_matrix4(mesh);
  const auto m = world_matrix->m;
  const double cx = (local_bounds->min->x + local_bounds->max->x) * 0.5;
  const double cy = (local_bounds->min->y + local_bounds->max->y) * 0.5;
  const double cz = (local_bounds->min->z + local_bounds->max->z) * 0.5;
  const double ex = (local_bounds->max->x - local_bounds->min->x) * 0.5;
  const double ey = (local_bounds->max->y - local_bounds->min->y) * 0.5;
  const double ez = (local_bounds->max->z - local_bounds->min->z) * 0.5;
  const double tcx = m.get_index(0.0) * cx + m.get_index(4.0) * cy +
                     m.get_index(8.0) * cz + m.get_index(12.0);
  const double tcy = m.get_index(1.0) * cx + m.get_index(5.0) * cy +
                     m.get_index(9.0) * cz + m.get_index(13.0);
  const double tcz = m.get_index(2.0) * cx + m.get_index(6.0) * cy +
                     m.get_index(10.0) * cz + m.get_index(14.0);
  const double tex = std::abs(m.get_index(0.0)) * ex +
                     std::abs(m.get_index(4.0)) * ey +
                     std::abs(m.get_index(8.0)) * ez;
  const double tey = std::abs(m.get_index(1.0)) * ex +
                     std::abs(m.get_index(5.0)) * ey +
                     std::abs(m.get_index(9.0)) * ez;
  const double tez = std::abs(m.get_index(2.0)) * ex +
                     std::abs(m.get_index(6.0)) * ey +
                     std::abs(m.get_index(10.0)) * ez;
  world_bounds->min->x = tcx - tex;
  world_bounds->min->y = tcy - tey;
  world_bounds->min->z = tcz - tez;
  world_bounds->max->x = tcx + tex;
  world_bounds->max->y = tcy + tey;
  world_bounds->max->z = tcz + tez;
  return flight::geometry::is_frustum_intersecting_aabb(
      readonly_object(frustum), readonly_object(world_bounds));
}

inline bool is_mesh_visible(flight::Ref<flight::types::Mesh> mesh,
                            flight::Ref<flight::types::Frustum> frustum,
                            flight::Ref<flight::types::Aabb> world_bounds) {
  if (!mesh->entity_runtime_key.has_value()) {
    throw std::logic_error("Mesh has no runtime owner");
  }
  auto runtime = mesh->entity_runtime_key.value();
  const auto* deformed = std::get_if<flight::Ref<flight::types::Aabb>>(
      &runtime->deformed_local_bounds);
  const bool is_skinned =
      std::holds_alternative<flight::Ref<flight::types::Skin>>(mesh->skin);
  if (is_skinned && !deformed && skinned_bounds_guard.has_value()) {
    skinned_bounds_guard.value()(mesh);
  }
  flight::Ref<flight::types::Aabb> bounds;
  if (deformed) {
    bounds = *deformed;
  } else {
    auto geometry_bounds =
        flight::mesh::ensure_mesh_geometry_bounds(mesh->geometry);
    if (!geometry_bounds.has_value()) return true;
    bounds = exact_object<flight::types::Aabb>(geometry_bounds.value(),
                                               "geometry bounds");
  }
  auto world_matrix = flight::node::get_node_world_matrix4(mesh);
  const auto m = world_matrix->m;
  const double cx = (bounds->min->x + bounds->max->x) * 0.5;
  const double cy = (bounds->min->y + bounds->max->y) * 0.5;
  const double cz = (bounds->min->z + bounds->max->z) * 0.5;
  const double ex = (bounds->max->x - bounds->min->x) * 0.5;
  const double ey = (bounds->max->y - bounds->min->y) * 0.5;
  const double ez = (bounds->max->z - bounds->min->z) * 0.5;
  const double tcx = m.get_index(0.0) * cx + m.get_index(4.0) * cy +
                     m.get_index(8.0) * cz + m.get_index(12.0);
  const double tcy = m.get_index(1.0) * cx + m.get_index(5.0) * cy +
                     m.get_index(9.0) * cz + m.get_index(13.0);
  const double tcz = m.get_index(2.0) * cx + m.get_index(6.0) * cy +
                     m.get_index(10.0) * cz + m.get_index(14.0);
  const double tex = std::abs(m.get_index(0.0)) * ex +
                     std::abs(m.get_index(4.0)) * ey +
                     std::abs(m.get_index(8.0)) * ez;
  const double tey = std::abs(m.get_index(1.0)) * ex +
                     std::abs(m.get_index(5.0)) * ey +
                     std::abs(m.get_index(9.0)) * ez;
  const double tez = std::abs(m.get_index(2.0)) * ex +
                     std::abs(m.get_index(6.0)) * ey +
                     std::abs(m.get_index(10.0)) * ez;
  world_bounds->min->x = tcx - tex;
  world_bounds->min->y = tcy - tey;
  world_bounds->min->z = tcz - tez;
  world_bounds->max->x = tcx + tex;
  world_bounds->max->y = tcy + tey;
  world_bounds->max->z = tcz + tez;
  return flight::geometry::is_frustum_intersecting_aabb(
      readonly_object(frustum), readonly_object(world_bounds));
}

inline void collect_visible_meshes(
    SceneNodeView root, flight::Ref<flight::types::Frustum> frustum,
    flight::Ref<flight::types::Aabb> world_bounds,
    flight::Array<flight::Ref<flight::types::Mesh>> out_meshes,
    flight::Array<flight::Ref<flight::types::InstancedMesh>> out_instanced,
    bool refresh_transforms) {
  flight::Array<SceneNodeView> stack;
  stack.push(root);
  while (!stack.empty()) {
    auto node = stack.pop().value();
    if (!flight::row_get<flight::RowKey<"enabled">>(node) ||
        !flight::row_get<flight::RowKey<"visible">>(node)) {
      continue;
    }
    auto runtime = flight::node::transform3d_detail::runtime(node);
    if (refresh_transforms &&
        !flight::node::is_node_local_matrix4_detached(node)) {
      runtime->local_transform_id = flight::unsigned_right_shift(
          runtime->local_transform_id + 1.0, 0.0);
    }
    if (auto instanced =
            node.template shared_exact_object<flight::types::InstancedMesh>()) {
      if (instanced->instance_count > 0.0 &&
          is_instanced_mesh_visible(instanced, frustum, world_bounds)) {
        out_instanced.push(instanced);
      }
    } else if (auto mesh =
                   node.template shared_exact_object<flight::types::Mesh>()) {
      if (is_mesh_visible(mesh, frustum, world_bounds)) out_meshes.push(mesh);
    }
    if (runtime->children.has_value()) {
      auto children = runtime->children.value();
      for (std::size_t i = children.size(); i > 0; --i) {
        stack.push(flight::structural_ref_cast<SceneNodeView>(
            children.element(static_cast<double>(i - 1))));
      }
    }
  }
}

template <typename State>
inline flight::Ref<flight::types::Scene3DRenderList> prepare_scene3_drender(
    std::shared_ptr<State> state, SceneNodeView scene, Camera3DView camera_view,
    SceneLightsView lights_view,
    std::optional<double> viewport_aspect = std::nullopt) {
  auto camera = exact_object<flight::types::Camera3D>(camera_view, "camera");
  auto lights =
      exact_object<flight::types::Scene3DLights>(lights_view, "light set");
  auto prepared = ensure_prepared_scene3_d(state);
  set_scene3_dview_projection_matrix4(
      prepared->view_projection, camera,
      resolve_scene3_dviewport_aspect(camera, viewport_aspect));
  flight::geometry::set_frustum_from_matrix4(
      prepared->frustum, readonly_object(prepared->view_projection));
  pack_scene3_dlight_block(prepared->light_block, lights);
  prepared->meshes.resize(0.0);
  prepared->instanced_meshes.resize(0.0);
  collect_visible_meshes(
      scene, prepared->frustum, prepared->world_bounds, prepared->meshes,
      prepared->instanced_meshes,
      state->scene_graph_sync_policy == flight::String("refreshDerivedState"));
  prepared->list->mesh_count = static_cast<double>(prepared->meshes.size());
  prepared->list->instanced_mesh_count =
      static_cast<double>(prepared->instanced_meshes.size());
  return prepared->list;
}

template <typename State>
inline flight::Ref<flight::types::Scene3DRenderList> prepare_scene3_drender(
    std::shared_ptr<State> state, flight::Ref<flight::types::Node3D> scene,
    flight::Ref<flight::types::Camera3D> camera,
    flight::Ref<flight::types::Scene3DLights> lights,
    std::optional<double> viewport_aspect = std::nullopt) {
  return prepare_scene3_drender(
      state, readonly_object(scene), readonly_object(camera),
      flight::structural_ref_cast<SceneLightsView>(readonly_object(lights)),
      viewport_aspect);
}

inline void set_skinned_mesh_bounds_guard(
    std::optional<std::function<void(flight::Ref<flight::types::Mesh>)>> guard) {
  skinned_bounds_guard = std::move(guard);
}

}  // namespace flight::render
