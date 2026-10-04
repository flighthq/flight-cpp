// Derived from @flighthq/collision/packages/collision/src/collisionFace3D.ts.
#pragma once

#include <cmath>
#include <limits>
#include <optional>
#include <utility>
#include <variant>

#include <flight/runtime.hpp>
#include <flight/types/collision.hpp>

static_assert(flight::runtime_contract.compiler_contract == "flight-runtime-contract/2", "Flight compiler/runtime contract mismatch");
static_assert(flight::runtime_contract.cpp_abi == 1, "Flight C++ runtime ABI mismatch");

namespace flight::collision {

using FaceAabb3DValue = flight::types::min_x_min_y_min_z_max_x_max_y_max_z_kind_748c675e8e0cf88a;
using FaceBox3DValue = flight::types::x_y_z_half_x_half_y_half_z_rotation_x_rotation_y_rotation_z_rotation_w_kind_bdd50a8e154d5a6a;
using FaceCapsule3DValue = flight::types::x0_y0_z0_x1_y1_z1_radius_kind_3ca9ada7b527e64b;
using FaceConvex3DValue = flight::types::points_kind_35a8f5f2a3a8bd66;

inline constexpr double capsule_axis_alignment_limit = 0.94;
inline constexpr double face_epsilon = 1e-12;
inline constexpr double face_plane_tolerance = 1e-6;
inline constexpr double max_face_vertices = 16.0;

inline flight::Array<double> identity_rotation{0.0, 0.0, 0.0, 1.0};
inline flight::Float64Array face_angles(max_face_vertices);
inline flight::Array<double> local_face_corner{0.0, 0.0, 0.0};
inline flight::Array<double> local_face_direction{0.0, 0.0, 0.0};
inline flight::Array<double> plane_axis_u{0.0, 0.0, 0.0};
inline flight::Array<double> plane_axis_v{0.0, 0.0, 0.0};
inline flight::Array<double> rotated_corner{0.0, 0.0, 0.0};
inline flight::Array<double> rotation{0.0, 0.0, 0.0, 1.0};

inline flight::Map<flight::types::CollisionShapeKind3D, flight::types::CollisionFaceQuery3D>
    collision_face_queries3_d;

inline std::optional<flight::types::CollisionFaceQuery3D> get_collision_face_query3_d(
    const flight::types::CollisionShapeKind3D& kind) {
  return collision_face_queries3_d.get(kind);
}

inline void register_collision_face_query3_d(
    const flight::types::CollisionShapeKind3D& kind,
    flight::types::CollisionFaceQuery3D query) {
  collision_face_queries3_d.set(kind, std::move(query));
}

inline void rotate_face_vector(
    double vector_x,
    double vector_y,
    double vector_z,
    flight::Array<double> quaternion,
    bool conjugate,
    flight::Array<double> out) {
  const double sign = conjugate ? -1.0 : 1.0;
  const double quaternion_x = quaternion.element(0.0) * sign;
  const double quaternion_y = quaternion.element(1.0) * sign;
  const double quaternion_z = quaternion.element(2.0) * sign;
  const double quaternion_w = quaternion.element(3.0);
  const double temporary_x =
      quaternion_y * vector_z - quaternion_z * vector_y + quaternion_w * vector_x;
  const double temporary_y =
      quaternion_z * vector_x - quaternion_x * vector_z + quaternion_w * vector_y;
  const double temporary_z =
      quaternion_x * vector_y - quaternion_y * vector_x + quaternion_w * vector_z;
  out.element(0.0) = vector_x + 2.0 * (quaternion_y * temporary_z - quaternion_z * temporary_y);
  out.element(1.0) = vector_y + 2.0 * (quaternion_z * temporary_x - quaternion_x * temporary_z);
  out.element(2.0) = vector_z + 2.0 * (quaternion_x * temporary_y - quaternion_y * temporary_x);
}

inline void write_face_plane_axis(
    double normal_x,
    double normal_y,
    double normal_z,
    flight::Array<double> out) {
  const double absolute_x = std::abs(normal_x);
  const double absolute_y = std::abs(normal_y);
  const double absolute_z = std::abs(normal_z);
  const double axis_x = absolute_x <= absolute_y && absolute_x <= absolute_z ? 1.0 : 0.0;
  const double axis_y = axis_x == 0.0 && absolute_y <= absolute_z ? 1.0 : 0.0;
  const double axis_z = axis_x == 0.0 && axis_y == 0.0 ? 1.0 : 0.0;
  double x = normal_y * axis_z - normal_z * axis_y;
  double y = normal_z * axis_x - normal_x * axis_z;
  double z = normal_x * axis_y - normal_y * axis_x;
  const double length = std::sqrt(x * x + y * y + z * z);
  if (length > face_epsilon) {
    x /= length;
    y /= length;
    z /= length;
  }
  out.element(0.0) = x;
  out.element(1.0) = y;
  out.element(2.0) = z;
}

inline void sort_face_vertices_by_angle(
    flight::Array<double> vertices,
    double count,
    double normal_x,
    double normal_y,
    double normal_z) {
  double centroid_x = 0.0;
  double centroid_y = 0.0;
  double centroid_z = 0.0;
  for (double index = 0.0; index < count; index += 1.0) {
    centroid_x += vertices.element(index * 3.0);
    centroid_y += vertices.element(index * 3.0 + 1.0);
    centroid_z += vertices.element(index * 3.0 + 2.0);
  }
  centroid_x /= count;
  centroid_y /= count;
  centroid_z /= count;

  write_face_plane_axis(normal_x, normal_y, normal_z, plane_axis_u);
  plane_axis_v.element(0.0) = normal_y * plane_axis_u.element(2.0) -
                              normal_z * plane_axis_u.element(1.0);
  plane_axis_v.element(1.0) = normal_z * plane_axis_u.element(0.0) -
                              normal_x * plane_axis_u.element(2.0);
  plane_axis_v.element(2.0) = normal_x * plane_axis_u.element(1.0) -
                              normal_y * plane_axis_u.element(0.0);

  for (double index = 0.0; index < count; index += 1.0) {
    const double offset_x = vertices.element(index * 3.0) - centroid_x;
    const double offset_y = vertices.element(index * 3.0 + 1.0) - centroid_y;
    const double offset_z = vertices.element(index * 3.0 + 2.0) - centroid_z;
    face_angles.set_index(
        index,
        std::atan2(
            offset_x * plane_axis_v.element(0.0) +
                offset_y * plane_axis_v.element(1.0) +
                offset_z * plane_axis_v.element(2.0),
            offset_x * plane_axis_u.element(0.0) +
                offset_y * plane_axis_u.element(1.0) +
                offset_z * plane_axis_u.element(2.0)));
  }

  for (double index = 1.0; index < count; index += 1.0) {
    const double angle = face_angles.get_index(index);
    const double x = vertices.element(index * 3.0);
    const double y = vertices.element(index * 3.0 + 1.0);
    const double z = vertices.element(index * 3.0 + 2.0);
    double previous = index - 1.0;
    while (previous >= 0.0 && face_angles.get_index(previous) > angle) {
      face_angles.set_index(previous + 1.0, face_angles.get_index(previous));
      vertices.element((previous + 1.0) * 3.0) = vertices.element(previous * 3.0);
      vertices.element((previous + 1.0) * 3.0 + 1.0) = vertices.element(previous * 3.0 + 1.0);
      vertices.element((previous + 1.0) * 3.0 + 2.0) = vertices.element(previous * 3.0 + 2.0);
      previous -= 1.0;
    }
    face_angles.set_index(previous + 1.0, angle);
    vertices.element((previous + 1.0) * 3.0) = x;
    vertices.element((previous + 1.0) * 3.0 + 1.0) = y;
    vertices.element((previous + 1.0) * 3.0 + 2.0) = z;
  }
}

inline void write_box_face_corners(
    double centre_x,
    double centre_y,
    double centre_z,
    double half_x,
    double half_y,
    double half_z,
    double dir_x,
    double dir_y,
    double dir_z,
    flight::Array<double> quaternion,
    flight::Array<double> out) {
  rotate_face_vector(dir_x, dir_y, dir_z, quaternion, true, local_face_direction);
  const double local_x = local_face_direction.element(0.0);
  const double local_y = local_face_direction.element(1.0);
  const double local_z = local_face_direction.element(2.0);
  const double absolute_x = std::abs(local_x);
  const double absolute_y = std::abs(local_y);
  const double absolute_z = std::abs(local_z);
  double axis = 0.0;
  if (absolute_y >= absolute_x && absolute_y >= absolute_z) axis = 1.0;
  else if (absolute_z >= absolute_x && absolute_z >= absolute_y) axis = 2.0;
  const double sign = (axis == 0.0 ? local_x : axis == 1.0 ? local_y : local_z) >= 0.0
                          ? 1.0
                          : -1.0;

  for (double corner = 0.0; corner < 4.0; corner += 1.0) {
    const double u_sign = corner == 0.0 || corner == 3.0 ? -1.0 : 1.0;
    const double v_sign = corner < 2.0 ? -1.0 : 1.0;
    if (axis == 0.0) {
      local_face_corner.element(0.0) = half_x * sign;
      local_face_corner.element(1.0) = half_y * u_sign;
      local_face_corner.element(2.0) = half_z * v_sign;
    } else if (axis == 1.0) {
      local_face_corner.element(0.0) = half_x * v_sign;
      local_face_corner.element(1.0) = half_y * sign;
      local_face_corner.element(2.0) = half_z * u_sign;
    } else {
      local_face_corner.element(0.0) = half_x * u_sign;
      local_face_corner.element(1.0) = half_y * v_sign;
      local_face_corner.element(2.0) = half_z * sign;
    }
    rotate_face_vector(
        local_face_corner.element(0.0),
        local_face_corner.element(1.0),
        local_face_corner.element(2.0),
        quaternion,
        false,
        rotated_corner);
    out.element(corner * 3.0) = centre_x + rotated_corner.element(0.0);
    out.element(corner * 3.0 + 1.0) = centre_y + rotated_corner.element(1.0);
    out.element(corner * 3.0 + 2.0) = centre_z + rotated_corner.element(2.0);
  }
}

inline double query_collision_aabb_face3_d(
    flight::types::CollisionShape3D shape,
    double dir_x,
    double dir_y,
    double dir_z,
    flight::Array<double> out) {
  const auto box = std::get<flight::Ref<FaceAabb3DValue>>(shape);
  write_box_face_corners(
      (box->min_x + box->max_x) / 2.0,
      (box->min_y + box->max_y) / 2.0,
      (box->min_z + box->max_z) / 2.0,
      (box->max_x - box->min_x) / 2.0,
      (box->max_y - box->min_y) / 2.0,
      (box->max_z - box->min_z) / 2.0,
      dir_x,
      dir_y,
      dir_z,
      identity_rotation,
      out);
  return 4.0;
}

inline double query_collision_box_face3_d(
    flight::types::CollisionShape3D shape,
    double dir_x,
    double dir_y,
    double dir_z,
    flight::Array<double> out) {
  const auto box = std::get<flight::Ref<FaceBox3DValue>>(shape);
  rotation.element(0.0) = box->rotation_x;
  rotation.element(1.0) = box->rotation_y;
  rotation.element(2.0) = box->rotation_z;
  rotation.element(3.0) = box->rotation_w;
  write_box_face_corners(
      box->x, box->y, box->z,
      box->half_x, box->half_y, box->half_z,
      dir_x, dir_y, dir_z, rotation, out);
  return 4.0;
}

inline double query_collision_capsule_face3_d(
    flight::types::CollisionShape3D shape,
    double dir_x,
    double dir_y,
    double dir_z,
    flight::Array<double> out) {
  const auto capsule = std::get<flight::Ref<FaceCapsule3DValue>>(shape);
  const double axis_x = capsule->x1 - capsule->x0;
  const double axis_y = capsule->y1 - capsule->y0;
  const double axis_z = capsule->z1 - capsule->z0;
  const double axis_length = std::sqrt(axis_x * axis_x + axis_y * axis_y + axis_z * axis_z);
  const double direction_length = std::sqrt(dir_x * dir_x + dir_y * dir_y + dir_z * dir_z);
  if (axis_length <= face_epsilon || direction_length <= face_epsilon) return 0.0;
  const double alignment = std::abs(
      (axis_x * dir_x + axis_y * dir_y + axis_z * dir_z) /
      (axis_length * direction_length));
  if (alignment > capsule_axis_alignment_limit) return 0.0;
  const double scale = capsule->radius / direction_length;
  out.element(0.0) = capsule->x0 + dir_x * scale;
  out.element(1.0) = capsule->y0 + dir_y * scale;
  out.element(2.0) = capsule->z0 + dir_z * scale;
  out.element(3.0) = capsule->x1 + dir_x * scale;
  out.element(4.0) = capsule->y1 + dir_y * scale;
  out.element(5.0) = capsule->z1 + dir_z * scale;
  return 2.0;
}

inline double query_collision_convex_face3_d(
    flight::types::CollisionShape3D shape,
    double dir_x,
    double dir_y,
    double dir_z,
    flight::Array<double> out) {
  const auto convex = std::get<flight::Ref<FaceConvex3DValue>>(shape);
  const double count = std::floor(static_cast<double>(convex->points.size()) / 3.0);
  if (count == 0.0) return 0.0;
  const double direction_length = std::sqrt(dir_x * dir_x + dir_y * dir_y + dir_z * dir_z);
  if (direction_length <= face_epsilon) return 0.0;
  const double unit_x = dir_x / direction_length;
  const double unit_y = dir_y / direction_length;
  const double unit_z = dir_z / direction_length;

  double best = -std::numeric_limits<double>::infinity();
  for (double index = 0.0; index < count; index += 1.0) {
    const double projection = convex->points.element(index * 3.0) * unit_x +
                              convex->points.element(index * 3.0 + 1.0) * unit_y +
                              convex->points.element(index * 3.0 + 2.0) * unit_z;
    if (projection > best) best = projection;
  }

  double written = 0.0;
  for (double index = 0.0; index < count && written < max_face_vertices; index += 1.0) {
    const double x = convex->points.element(index * 3.0);
    const double y = convex->points.element(index * 3.0 + 1.0);
    const double z = convex->points.element(index * 3.0 + 2.0);
    if (best - (x * unit_x + y * unit_y + z * unit_z) > face_plane_tolerance) continue;
    out.element(written * 3.0) = x;
    out.element(written * 3.0 + 1.0) = y;
    out.element(written * 3.0 + 2.0) = z;
    written += 1.0;
  }
  if (written < 3.0) return written;
  sort_face_vertices_by_angle(out, written, unit_x, unit_y, unit_z);
  return written;
}

inline void register_built_in_collision_face_queries3_d() {
  register_collision_face_query3_d(flight::String("aabb"), query_collision_aabb_face3_d);
  register_collision_face_query3_d(flight::String("box"), query_collision_box_face3_d);
  register_collision_face_query3_d(flight::String("capsule"), query_collision_capsule_face3_d);
  register_collision_face_query3_d(flight::String("convex"), query_collision_convex_face3_d);
}

} // namespace flight::collision
