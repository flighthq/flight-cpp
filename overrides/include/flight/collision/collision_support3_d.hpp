// Derived from @flighthq/collision/packages/collision/src/collisionSupport3D.ts.
#pragma once

#include <cmath>
#include <functional>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

#include <flight/runtime.hpp>
#include <flight/sequence_view.hpp>
#include <flight/structural_ref.hpp>
#include <flight/types/collision.hpp>

static_assert(flight::runtime_contract.compiler_contract == "flight-runtime-contract/2", "Flight compiler/runtime contract mismatch");
static_assert(flight::runtime_contract.cpp_abi == 1, "Flight C++ runtime ABI mismatch");

namespace flight::collision {

using CollisionAabb3DValue = flight::types::min_x_min_y_min_z_max_x_max_y_max_z_kind_748c675e8e0cf88a;
using CollisionBox3DValue = flight::types::x_y_z_half_x_half_y_half_z_rotation_x_rotation_y_rotation_z_rotation_w_kind_bdd50a8e154d5a6a;
using CollisionCapsule3DValue = flight::types::x0_y0_z0_x1_y1_z1_radius_kind_3ca9ada7b527e64b;
using CollisionCone3DValue = flight::types::apex_x_apex_y_apex_z_base_x_base_y_base_z_radius_kind_21c650ad310bf539;
using CollisionConvex3DValue = flight::types::points_kind_35a8f5f2a3a8bd66;
using CollisionCylinder3DValue = flight::types::x0_y0_z0_x1_y1_z1_radius_kind_aef60734fa17cff8;
using CollisionSphere3DValue = flight::types::x_y_z_radius_kind_b0bc53ca3f0e8006;
using CollisionVendorView3D = flight::StructuralRef<flight::RowReadonly<
    flight::RowOf<flight::Ref<flight::types::CollisionVendorShape3D>>>>;
using GeneratedCollisionShape3D = std::variant<
    flight::Ref<CollisionAabb3DValue>,
    flight::Ref<CollisionBox3DValue>,
    flight::Ref<CollisionCapsule3DValue>,
    flight::Ref<CollisionSphere3DValue>,
    flight::Ref<CollisionCone3DValue>,
    flight::Ref<CollisionConvex3DValue>,
    flight::Ref<CollisionCylinder3DValue>,
    CollisionVendorView3D>;

inline GeneratedCollisionShape3D to_generated_collision_shape3_d(
    const flight::types::CollisionShape3D& shape) {
  return std::visit(
      [](const auto& value) -> GeneratedCollisionShape3D {
        using Value = std::remove_cvref_t<decltype(value)>;
        if constexpr (std::is_same_v<Value, flight::Ref<flight::types::CollisionVendorShape3D>>) {
          return CollisionVendorView3D(value);
        } else {
          return GeneratedCollisionShape3D(value);
        }
      },
      shape);
}

inline flight::String get_collision_pair_key3_d(
    const flight::types::CollisionShapeKind3D& kind_a,
    const flight::types::CollisionShapeKind3D& kind_b) {
  return kind_a + flight::String::from_char_code(0.0) + kind_b;
}

inline flight::Map<flight::String, flight::types::CollisionPairTest3D> collision_pair_tests3_d;
inline flight::Map<flight::types::CollisionShapeKind3D, flight::types::CollisionSupport3D>
    collision_supports3_d;

inline void clear_collision_pair_tests3_d() { collision_pair_tests3_d.clear(); }
inline void clear_collision_supports3_d() { collision_supports3_d.clear(); }

inline std::optional<flight::types::CollisionPairTest3D> get_collision_pair_test3_d(
    const flight::types::CollisionShapeKind3D& kind_a,
    const flight::types::CollisionShapeKind3D& kind_b) {
  return collision_pair_tests3_d.get(get_collision_pair_key3_d(kind_a, kind_b));
}

inline std::optional<flight::types::CollisionSupport3D> get_collision_support3_d(
    const flight::types::CollisionShapeKind3D& kind) {
  return collision_supports3_d.get(kind);
}

inline void register_collision_pair_test3_d(
    const flight::types::CollisionShapeKind3D& kind_a,
    const flight::types::CollisionShapeKind3D& kind_b,
    flight::types::CollisionPairTest3D test) {
  collision_pair_tests3_d.set(get_collision_pair_key3_d(kind_a, kind_b), std::move(test));
}

template <typename Callback>
  requires std::is_invocable_r_v<
      bool,
      Callback&,
      GeneratedCollisionShape3D,
      GeneratedCollisionShape3D,
      flight::Ref<flight::types::CollisionManifold3D>>
inline void register_collision_pair_test3_d(
    const flight::types::CollisionShapeKind3D& kind_a,
    const flight::types::CollisionShapeKind3D& kind_b,
    Callback callback) {
  flight::types::CollisionPairTest3D adapted =
      [callback = std::move(callback)](
          flight::types::CollisionShape3D a,
          flight::types::CollisionShape3D b,
          flight::Ref<flight::types::CollisionManifold3D> out) mutable {
        return std::invoke(
            callback,
            to_generated_collision_shape3_d(a),
            to_generated_collision_shape3_d(b),
            std::move(out));
      };
  register_collision_pair_test3_d(kind_a, kind_b, std::move(adapted));
}

inline void register_collision_support3_d(
    const flight::types::CollisionShapeKind3D& kind,
    flight::types::CollisionSupport3D support) {
  collision_supports3_d.set(kind, std::move(support));
}

inline void rotate_vector_by_quaternion(
    double vector_x,
    double vector_y,
    double vector_z,
    double quaternion_x,
    double quaternion_y,
    double quaternion_z,
    double quaternion_w,
    flight::Array<double> out) {
  const double temp_x = quaternion_y * vector_z - quaternion_z * vector_y + quaternion_w * vector_x;
  const double temp_y = quaternion_z * vector_x - quaternion_x * vector_z + quaternion_w * vector_y;
  const double temp_z = quaternion_x * vector_y - quaternion_y * vector_x + quaternion_w * vector_z;
  out.element(0.0) = vector_x + 2.0 * (quaternion_y * temp_z - quaternion_z * temp_y);
  out.element(1.0) = vector_y + 2.0 * (quaternion_z * temp_x - quaternion_x * temp_z);
  out.element(2.0) = vector_z + 2.0 * (quaternion_x * temp_y - quaternion_y * temp_x);
}

inline void write_radial_component3_d(
    double axis_x,
    double axis_y,
    double axis_z,
    double dir_x,
    double dir_y,
    double dir_z,
    flight::Array<double> out) {
  const double axis_length_squared = axis_x * axis_x + axis_y * axis_y + axis_z * axis_z;
  double radial_x = dir_x;
  double radial_y = dir_y;
  double radial_z = dir_z;
  if (axis_length_squared > 0.0) {
    const double projection =
        (dir_x * axis_x + dir_y * axis_y + dir_z * axis_z) / axis_length_squared;
    radial_x -= axis_x * projection;
    radial_y -= axis_y * projection;
    radial_z -= axis_z * projection;
  }
  out.element(0.0) = radial_x;
  out.element(1.0) = radial_y;
  out.element(2.0) = radial_z;
  out.element(3.0) = std::sqrt(radial_x * radial_x + radial_y * radial_y + radial_z * radial_z);
}

inline void write_vertex_list_support3_d(
    flight::SequenceView<double> vertices,
    double count,
    double dir_x,
    double dir_y,
    double dir_z,
    flight::Array<double> out) {
  double best_x = vertices[0.0];
  double best_y = vertices[1.0];
  double best_z = vertices[2.0];
  double best = best_x * dir_x + best_y * dir_y + best_z * dir_z;
  for (double index = 1.0; index < count; index += 1.0) {
    const double x = vertices[index * 3.0];
    const double y = vertices[index * 3.0 + 1.0];
    const double z = vertices[index * 3.0 + 2.0];
    const double projection = x * dir_x + y * dir_y + z * dir_z;
    if (projection > best) {
      best = projection;
      best_x = x;
      best_y = y;
      best_z = z;
    }
  }
  out.element(0.0) = best_x;
  out.element(1.0) = best_y;
  out.element(2.0) = best_z;
}

inline flight::Array<double> local_corner{0.0, 0.0, 0.0};
inline flight::Array<double> local_direction{0.0, 0.0, 0.0};
inline flight::Array<double> radial_scratch{0.0, 0.0, 0.0, 0.0};

inline void support_collision_aabb3_d(
    flight::types::CollisionShape3D shape,
    double dir_x,
    double dir_y,
    double dir_z,
    flight::Array<double> out) {
  const auto aabb = std::get<flight::Ref<CollisionAabb3DValue>>(shape);
  out.element(0.0) = dir_x >= 0.0 ? aabb->max_x : aabb->min_x;
  out.element(1.0) = dir_y >= 0.0 ? aabb->max_y : aabb->min_y;
  out.element(2.0) = dir_z >= 0.0 ? aabb->max_z : aabb->min_z;
}

inline void support_collision_box3_d(
    flight::types::CollisionShape3D shape,
    double dir_x,
    double dir_y,
    double dir_z,
    flight::Array<double> out) {
  const auto box = std::get<flight::Ref<CollisionBox3DValue>>(shape);
  rotate_vector_by_quaternion(
      dir_x, dir_y, dir_z,
      -box->rotation_x, -box->rotation_y, -box->rotation_z, box->rotation_w,
      local_direction);
  const double corner_x = local_direction.element(0.0) >= 0.0 ? box->half_x : -box->half_x;
  const double corner_y = local_direction.element(1.0) >= 0.0 ? box->half_y : -box->half_y;
  const double corner_z = local_direction.element(2.0) >= 0.0 ? box->half_z : -box->half_z;
  rotate_vector_by_quaternion(
      corner_x, corner_y, corner_z,
      box->rotation_x, box->rotation_y, box->rotation_z, box->rotation_w,
      local_corner);
  out.element(0.0) = box->x + local_corner.element(0.0);
  out.element(1.0) = box->y + local_corner.element(1.0);
  out.element(2.0) = box->z + local_corner.element(2.0);
}

inline void support_collision_capsule3_d(
    flight::types::CollisionShape3D shape,
    double dir_x,
    double dir_y,
    double dir_z,
    flight::Array<double> out) {
  const auto capsule = std::get<flight::Ref<CollisionCapsule3DValue>>(shape);
  const double projection0 = capsule->x0 * dir_x + capsule->y0 * dir_y + capsule->z0 * dir_z;
  const double projection1 = capsule->x1 * dir_x + capsule->y1 * dir_y + capsule->z1 * dir_z;
  const bool use_second = projection1 > projection0;
  const double base_x = use_second ? capsule->x1 : capsule->x0;
  const double base_y = use_second ? capsule->y1 : capsule->y0;
  const double base_z = use_second ? capsule->z1 : capsule->z0;
  const double length = std::sqrt(dir_x * dir_x + dir_y * dir_y + dir_z * dir_z);
  if (length == 0.0) {
    out.element(0.0) = base_x;
    out.element(1.0) = base_y;
    out.element(2.0) = base_z;
    return;
  }
  const double scale = capsule->radius / length;
  out.element(0.0) = base_x + dir_x * scale;
  out.element(1.0) = base_y + dir_y * scale;
  out.element(2.0) = base_z + dir_z * scale;
}

inline void support_collision_cone3_d(
    flight::types::CollisionShape3D shape,
    double dir_x,
    double dir_y,
    double dir_z,
    flight::Array<double> out) {
  const auto cone = std::get<flight::Ref<CollisionCone3DValue>>(shape);
  const double apex_projection = cone->apex_x * dir_x + cone->apex_y * dir_y + cone->apex_z * dir_z;
  write_radial_component3_d(
      cone->base_x - cone->apex_x,
      cone->base_y - cone->apex_y,
      cone->base_z - cone->apex_z,
      dir_x,
      dir_y,
      dir_z,
      radial_scratch);
  const double radial_length = radial_scratch.element(3.0);
  const double rim_projection = cone->base_x * dir_x + cone->base_y * dir_y +
                                cone->base_z * dir_z + cone->radius * radial_length;
  if (apex_projection >= rim_projection) {
    out.element(0.0) = cone->apex_x;
    out.element(1.0) = cone->apex_y;
    out.element(2.0) = cone->apex_z;
    return;
  }
  const double scale = radial_length > 0.0 ? cone->radius / radial_length : 0.0;
  out.element(0.0) = cone->base_x + radial_scratch.element(0.0) * scale;
  out.element(1.0) = cone->base_y + radial_scratch.element(1.0) * scale;
  out.element(2.0) = cone->base_z + radial_scratch.element(2.0) * scale;
}

inline void support_collision_convex3_d(
    flight::types::CollisionShape3D shape,
    double dir_x,
    double dir_y,
    double dir_z,
    flight::Array<double> out) {
  const auto convex = std::get<flight::Ref<CollisionConvex3DValue>>(shape);
  write_vertex_list_support3_d(
      convex->points,
      std::floor(static_cast<double>(convex->points.size()) / 3.0),
      dir_x,
      dir_y,
      dir_z,
      out);
}

inline void support_collision_cylinder3_d(
    flight::types::CollisionShape3D shape,
    double dir_x,
    double dir_y,
    double dir_z,
    flight::Array<double> out) {
  const auto cylinder = std::get<flight::Ref<CollisionCylinder3DValue>>(shape);
  const double axis_x = cylinder->x1 - cylinder->x0;
  const double axis_y = cylinder->y1 - cylinder->y0;
  const double axis_z = cylinder->z1 - cylinder->z0;
  const bool use_second = axis_x * dir_x + axis_y * dir_y + axis_z * dir_z > 0.0;
  const double base_x = use_second ? cylinder->x1 : cylinder->x0;
  const double base_y = use_second ? cylinder->y1 : cylinder->y0;
  const double base_z = use_second ? cylinder->z1 : cylinder->z0;
  write_radial_component3_d(axis_x, axis_y, axis_z, dir_x, dir_y, dir_z, radial_scratch);
  const double radial_length = radial_scratch.element(3.0);
  const double scale = radial_length > 0.0 ? cylinder->radius / radial_length : 0.0;
  out.element(0.0) = base_x + radial_scratch.element(0.0) * scale;
  out.element(1.0) = base_y + radial_scratch.element(1.0) * scale;
  out.element(2.0) = base_z + radial_scratch.element(2.0) * scale;
}

inline void support_collision_sphere3_d(
    flight::types::CollisionShape3D shape,
    double dir_x,
    double dir_y,
    double dir_z,
    flight::Array<double> out) {
  const auto sphere = std::get<flight::Ref<CollisionSphere3DValue>>(shape);
  const double length = std::sqrt(dir_x * dir_x + dir_y * dir_y + dir_z * dir_z);
  if (length == 0.0) {
    out.element(0.0) = sphere->x;
    out.element(1.0) = sphere->y;
    out.element(2.0) = sphere->z;
    return;
  }
  const double scale = sphere->radius / length;
  out.element(0.0) = sphere->x + dir_x * scale;
  out.element(1.0) = sphere->y + dir_y * scale;
  out.element(2.0) = sphere->z + dir_z * scale;
}

inline void register_built_in_collision_supports3_d() {
  register_collision_support3_d(flight::String("aabb"), support_collision_aabb3_d);
  register_collision_support3_d(flight::String("box"), support_collision_box3_d);
  register_collision_support3_d(flight::String("capsule"), support_collision_capsule3_d);
  register_collision_support3_d(flight::String("cone"), support_collision_cone3_d);
  register_collision_support3_d(flight::String("convex"), support_collision_convex3_d);
  register_collision_support3_d(flight::String("cylinder"), support_collision_cylinder3_d);
  register_collision_support3_d(flight::String("sphere"), support_collision_sphere3_d);
}

} // namespace flight::collision
