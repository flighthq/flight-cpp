// Derived from @flighthq/collision/packages/collision/src/pointContainment3D.ts.
#pragma once

#include <cmath>
#include <type_traits>
#include <variant>

#include <flight/collision/gjk3_d.hpp>
#include <flight/runtime.hpp>
#include <flight/types/collision.hpp>

static_assert(flight::runtime_contract.compiler_contract == "flight-runtime-contract/2", "Flight compiler/runtime contract mismatch");
static_assert(flight::runtime_contract.cpp_abi == 1, "Flight C++ runtime ABI mismatch");

namespace flight::collision {

using PointContainmentConvex3D = flight::types::points_kind_35a8f5f2a3a8bd66;
using PointContainmentSphere3D = flight::types::x_y_z_radius_kind_b0bc53ca3f0e8006;

inline flight::Ref<PointContainmentConvex3D> point_containment_scratch_hull =
    flight::make_ref<PointContainmentConvex3D>(PointContainmentConvex3D{
        .points = flight::Array<double>{},
        .kind = flight::String("convex"),
    });
inline flight::Ref<PointContainmentSphere3D> point_containment_scratch_probe =
    flight::make_ref<PointContainmentSphere3D>(PointContainmentSphere3D{
        .x = 0.0,
        .y = 0.0,
        .z = 0.0,
        .radius = 0.0,
        .kind = flight::String("sphere"),
    });

inline bool is_point_in_convex_hull3_d(
    flight::Array<double> points,
    double x,
    double y,
    double z) {
  if (points.size() < 3U) return false;
  point_containment_scratch_hull->points = points;
  point_containment_scratch_probe->x = x;
  point_containment_scratch_probe->y = y;
  point_containment_scratch_probe->z = z;
  return test_collision_support_overlap3_d(
      flight::types::CollisionShape3D{
          std::in_place_type<flight::Ref<PointContainmentConvex3D>>,
          point_containment_scratch_hull,
      },
      flight::types::CollisionShape3D{
          std::in_place_type<flight::Ref<PointContainmentSphere3D>>,
          point_containment_scratch_probe,
      });
}

inline bool get_collision_shape_contains_point3_d(
    flight::types::CollisionBuiltInShape3D shape,
    double x,
    double y,
    double z) {
  return std::visit(
      [&](const auto& value) {
        const auto& kind = value->kind;
        if (kind == flight::String("sphere")) {
          if constexpr (requires { value->x; value->y; value->z; value->radius; }) {
            const double dx = x - value->x;
            const double dy = y - value->y;
            const double dz = z - value->z;
            return dx * dx + dy * dy + dz * dz <= value->radius * value->radius;
          }
        } else if (kind == flight::String("aabb")) {
          if constexpr (requires {
                          value->min_x; value->min_y; value->min_z;
                          value->max_x; value->max_y; value->max_z;
                        }) {
            return x >= value->min_x && x <= value->max_x &&
                   y >= value->min_y && y <= value->max_y &&
                   z >= value->min_z && z <= value->max_z;
          }
        } else if (kind == flight::String("box")) {
          if constexpr (requires {
                          value->x; value->y; value->z;
                          value->half_x; value->half_y; value->half_z;
                          value->rotation_x; value->rotation_y;
                          value->rotation_z; value->rotation_w;
                        }) {
            const double dx = x - value->x;
            const double dy = y - value->y;
            const double dz = z - value->z;
            const double quaternion_x = -value->rotation_x;
            const double quaternion_y = -value->rotation_y;
            const double quaternion_z = -value->rotation_z;
            const double quaternion_w = value->rotation_w;
            const double temporary_x = 2.0 * (quaternion_y * dz - quaternion_z * dy);
            const double temporary_y = 2.0 * (quaternion_z * dx - quaternion_x * dz);
            const double temporary_z = 2.0 * (quaternion_x * dy - quaternion_y * dx);
            const double local_x = dx + quaternion_w * temporary_x +
                                   quaternion_y * temporary_z - quaternion_z * temporary_y;
            const double local_y = dy + quaternion_w * temporary_y +
                                   quaternion_z * temporary_x - quaternion_x * temporary_z;
            const double local_z = dz + quaternion_w * temporary_z +
                                   quaternion_x * temporary_y - quaternion_y * temporary_x;
            return std::abs(local_x) <= value->half_x &&
                   std::abs(local_y) <= value->half_y &&
                   std::abs(local_z) <= value->half_z;
          }
        } else if (kind == flight::String("capsule")) {
          if constexpr (requires {
                          value->x0; value->y0; value->z0;
                          value->x1; value->y1; value->z1; value->radius;
                        }) {
            const double axis_x = value->x1 - value->x0;
            const double axis_y = value->y1 - value->y0;
            const double axis_z = value->z1 - value->z0;
            const double length_squared = axis_x * axis_x + axis_y * axis_y + axis_z * axis_z;
            double parameter = 0.0;
            if (length_squared > 0.0) {
              parameter = ((x - value->x0) * axis_x +
                           (y - value->y0) * axis_y +
                           (z - value->z0) * axis_z) /
                          length_squared;
              parameter = flight::maximum(0.0, flight::minimum(1.0, parameter));
            }
            const double dx = x - (value->x0 + axis_x * parameter);
            const double dy = y - (value->y0 + axis_y * parameter);
            const double dz = z - (value->z0 + axis_z * parameter);
            return dx * dx + dy * dy + dz * dz <= value->radius * value->radius;
          }
        } else if (kind == flight::String("cylinder")) {
          if constexpr (requires {
                          value->x0; value->y0; value->z0;
                          value->x1; value->y1; value->z1; value->radius;
                        }) {
            const double axis_x = value->x1 - value->x0;
            const double axis_y = value->y1 - value->y0;
            const double axis_z = value->z1 - value->z0;
            const double length_squared = axis_x * axis_x + axis_y * axis_y + axis_z * axis_z;
            if (length_squared <= 0.0) return false;
            const double parameter = ((x - value->x0) * axis_x +
                                      (y - value->y0) * axis_y +
                                      (z - value->z0) * axis_z) /
                                     length_squared;
            if (parameter < 0.0 || parameter > 1.0) return false;
            const double dx = x - (value->x0 + axis_x * parameter);
            const double dy = y - (value->y0 + axis_y * parameter);
            const double dz = z - (value->z0 + axis_z * parameter);
            return dx * dx + dy * dy + dz * dz <= value->radius * value->radius;
          }
        } else if (kind == flight::String("cone")) {
          if constexpr (requires {
                          value->apex_x; value->apex_y; value->apex_z;
                          value->base_x; value->base_y; value->base_z; value->radius;
                        }) {
            const double axis_x = value->base_x - value->apex_x;
            const double axis_y = value->base_y - value->apex_y;
            const double axis_z = value->base_z - value->apex_z;
            const double length_squared = axis_x * axis_x + axis_y * axis_y + axis_z * axis_z;
            if (length_squared <= 0.0) return false;
            const double parameter = ((x - value->apex_x) * axis_x +
                                      (y - value->apex_y) * axis_y +
                                      (z - value->apex_z) * axis_z) /
                                     length_squared;
            if (parameter < 0.0 || parameter > 1.0) return false;
            const double dx = x - (value->apex_x + axis_x * parameter);
            const double dy = y - (value->apex_y + axis_y * parameter);
            const double dz = z - (value->apex_z + axis_z * parameter);
            const double permitted = value->radius * parameter;
            return dx * dx + dy * dy + dz * dz <= permitted * permitted;
          }
        } else if (kind == flight::String("convex")) {
          if constexpr (requires { value->points; }) {
            return is_point_in_convex_hull3_d(value->points, x, y, z);
          }
        }
        return false;
      },
      shape);
}

} // namespace flight::collision
