// Derived from @flighthq/collision/packages/collision/src/collisionSupport2D.ts.
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

using CollisionAabb2DValue = flight::types::min_x_min_y_max_x_max_y_kind_329d6080b4f31db4;
using CollisionCircle2DValue = flight::types::x_y_radius_kind_90fbb3b4c4569518;
using CollisionObb2DValue = flight::types::x_y_half_w_half_h_rotation_kind_8ffcea5c657a2268;
using CollisionPolygon2DValue = flight::types::points_kind_9b00ebdad27cdb4e;
using CollisionCapsule2DValue = flight::types::x0_y0_x1_y1_radius_kind_768ba062d07f3ff6;
using CollisionPoint2DValue = flight::types::x_y_kind_35ad667397176b0f;
using CollisionSegment2DValue = flight::types::x0_y0_x1_y1_kind_d4f616534491406f;
using CollisionVendorView2D = flight::StructuralRef<flight::RowReadonly<
    flight::RowOf<flight::Ref<flight::types::CollisionVendorShape2D>>>>;
using GeneratedCollisionShape2D = std::variant<
    flight::Ref<CollisionAabb2DValue>,
    flight::Ref<CollisionCircle2DValue>,
    flight::Ref<CollisionObb2DValue>,
    flight::Ref<CollisionPolygon2DValue>,
    flight::Ref<CollisionCapsule2DValue>,
    flight::Ref<CollisionPoint2DValue>,
    flight::Ref<CollisionSegment2DValue>,
    CollisionVendorView2D>;

inline GeneratedCollisionShape2D to_generated_collision_shape2_d(
    const flight::types::CollisionShape2D& shape) {
  return std::visit(
      [](const auto& value) -> GeneratedCollisionShape2D {
        using Value = std::remove_cvref_t<decltype(value)>;
        if constexpr (std::is_same_v<Value, flight::Ref<flight::types::CollisionVendorShape2D>>) {
          return CollisionVendorView2D(value);
        } else {
          return GeneratedCollisionShape2D(value);
        }
      },
      shape);
}

inline flight::String get_collision_pair_key2_d(
    const flight::types::CollisionShapeKind2D& kind_a,
    const flight::types::CollisionShapeKind2D& kind_b) {
  return kind_a + flight::String::from_char_code(0.0) + kind_b;
}

inline flight::Map<flight::String, flight::types::CollisionPairTest2D> collision_pair_tests2_d;
inline flight::Map<flight::types::CollisionShapeKind2D, flight::types::CollisionSupport2D>
    collision_supports2_d;

inline std::optional<flight::types::CollisionPairTest2D> get_collision_pair_test2_d(
    const flight::types::CollisionShapeKind2D& kind_a,
    const flight::types::CollisionShapeKind2D& kind_b) {
  return collision_pair_tests2_d.get(get_collision_pair_key2_d(kind_a, kind_b));
}

inline void register_collision_pair_test2_d(
    const flight::types::CollisionShapeKind2D& kind_a,
    const flight::types::CollisionShapeKind2D& kind_b,
    flight::types::CollisionPairTest2D test) {
  collision_pair_tests2_d.set(get_collision_pair_key2_d(kind_a, kind_b), std::move(test));
}

template <typename Callback>
  requires std::is_invocable_r_v<
      bool,
      Callback&,
      GeneratedCollisionShape2D,
      GeneratedCollisionShape2D,
      flight::Ref<flight::types::CollisionManifold2D>>
inline void register_collision_pair_test2_d(
    const flight::types::CollisionShapeKind2D& kind_a,
    const flight::types::CollisionShapeKind2D& kind_b,
    Callback callback) {
  flight::types::CollisionPairTest2D adapted =
      [callback = std::move(callback)](
          flight::types::CollisionShape2D a,
          flight::types::CollisionShape2D b,
          flight::Ref<flight::types::CollisionManifold2D> out) mutable {
        return std::invoke(
            callback,
            to_generated_collision_shape2_d(a),
            to_generated_collision_shape2_d(b),
            std::move(out));
      };
  register_collision_pair_test2_d(kind_a, kind_b, std::move(adapted));
}

inline std::optional<flight::types::CollisionSupport2D> get_collision_support2_d(
    const flight::types::CollisionShapeKind2D& kind) {
  return collision_supports2_d.get(kind);
}

inline void register_collision_support2_d(
    const flight::types::CollisionShapeKind2D& kind,
    flight::types::CollisionSupport2D support) {
  collision_supports2_d.set(kind, std::move(support));
}

inline void write_vertex_list_support2_d(
    flight::SequenceView<double> vertices,
    double count,
    double dir_x,
    double dir_y,
    flight::Array<double> out) {
  double best_x = vertices[0.0];
  double best_y = vertices[1.0];
  double best = best_x * dir_x + best_y * dir_y;
  for (double index = 1.0; index < count; index += 1.0) {
    const double x = vertices[index * 2.0];
    const double y = vertices[index * 2.0 + 1.0];
    const double projection = x * dir_x + y * dir_y;
    if (projection > best) {
      best = projection;
      best_x = x;
      best_y = y;
    }
  }
  out.element(0.0) = best_x;
  out.element(1.0) = best_y;
}

inline void support_collision_aabb2_d(
    flight::types::CollisionShape2D shape,
    double dir_x,
    double dir_y,
    flight::Array<double> out) {
  const auto aabb = std::get<flight::Ref<CollisionAabb2DValue>>(shape);
  out.element(0.0) = dir_x >= 0.0 ? aabb->max_x : aabb->min_x;
  out.element(1.0) = dir_y >= 0.0 ? aabb->max_y : aabb->min_y;
}

inline void support_collision_circle2_d(
    flight::types::CollisionShape2D shape,
    double dir_x,
    double dir_y,
    flight::Array<double> out) {
  const auto circle = std::get<flight::Ref<CollisionCircle2DValue>>(shape);
  const double length = std::sqrt(dir_x * dir_x + dir_y * dir_y);
  if (length == 0.0) {
    out.element(0.0) = circle->x;
    out.element(1.0) = circle->y;
    return;
  }
  const double scale = circle->radius / length;
  out.element(0.0) = circle->x + dir_x * scale;
  out.element(1.0) = circle->y + dir_y * scale;
}

inline flight::Float64Array scratch_vertices = flight::Float64Array(8.0);

inline void support_collision_obb2_d(
    flight::types::CollisionShape2D shape,
    double dir_x,
    double dir_y,
    flight::Array<double> out) {
  const auto obb = std::get<flight::Ref<CollisionObb2DValue>>(shape);
  const double cosine = std::cos(obb->rotation);
  const double sine = std::sin(obb->rotation);
  const double width_x = cosine * obb->half_w;
  const double width_y = sine * obb->half_w;
  const double height_x = -sine * obb->half_h;
  const double height_y = cosine * obb->half_h;
  scratch_vertices.set_index(0.0, obb->x - width_x - height_x);
  scratch_vertices.set_index(1.0, obb->y - width_y - height_y);
  scratch_vertices.set_index(2.0, obb->x + width_x - height_x);
  scratch_vertices.set_index(3.0, obb->y + width_y - height_y);
  scratch_vertices.set_index(4.0, obb->x + width_x + height_x);
  scratch_vertices.set_index(5.0, obb->y + width_y + height_y);
  scratch_vertices.set_index(6.0, obb->x - width_x + height_x);
  scratch_vertices.set_index(7.0, obb->y - width_y + height_y);
  write_vertex_list_support2_d(scratch_vertices, 4.0, dir_x, dir_y, out);
}

inline void support_collision_polygon2_d(
    flight::types::CollisionShape2D shape,
    double dir_x,
    double dir_y,
    flight::Array<double> out) {
  const auto polygon = std::get<flight::Ref<CollisionPolygon2DValue>>(shape);
  write_vertex_list_support2_d(
      polygon->points,
      flight::signed_right_shift(static_cast<double>(polygon->points.size()), 1.0),
      dir_x,
      dir_y,
      out);
}

inline void register_built_in_collision_supports2_d() {
  register_collision_support2_d(flight::String("aabb"), support_collision_aabb2_d);
  register_collision_support2_d(flight::String("circle"), support_collision_circle2_d);
  register_collision_support2_d(flight::String("obb"), support_collision_obb2_d);
  register_collision_support2_d(flight::String("polygon"), support_collision_polygon2_d);
}

} // namespace flight::collision
