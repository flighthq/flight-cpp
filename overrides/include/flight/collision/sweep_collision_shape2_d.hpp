// Derived from @flighthq/collision/packages/collision/src/sweepCollisionShape2D.ts.
#pragma once

#include <cmath>
#include <exception>
#include <limits>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

#include <flight/collision/collide_contact_manifold2_d.hpp>
#include <flight/collision/collision_shape_validation2_d.hpp>
#include <flight/collision/contact_manifold2_d.hpp>
#include <flight/entity/entity.hpp>
#include <flight/runtime.hpp>
#include <flight/sequence_view.hpp>
#include <flight/types/collision.hpp>
#include <flight/types/entity.hpp>

static_assert(flight::runtime_contract.compiler_contract == "flight-runtime-contract/2", "Flight compiler/runtime contract mismatch");
static_assert(flight::runtime_contract.cpp_abi == 1, "Flight C++ runtime ABI mismatch");

namespace flight::collision {

using SweepAabb2D = flight::types::min_x_min_y_max_x_max_y_kind_329d6080b4f31db4;
using SweepCapsule2D = flight::types::x0_y0_x1_y1_radius_kind_768ba062d07f3ff6;
using SweepCircle2D = flight::types::x_y_radius_kind_90fbb3b4c4569518;
using SweepObb2D = flight::types::x_y_half_w_half_h_rotation_kind_8ffcea5c657a2268;
using SweepPolygon2D = flight::types::points_kind_9b00ebdad27cdb4e;

inline void initialize_collision_time_of_impact2_d(
    flight::types::EntityConstruction<flight::Ref<flight::types::CollisionTimeOfImpact2D>> out) {
  flight::row_set<flight::RowKey<"fraction">>(out, 0.0);
  flight::row_set<flight::RowKey<"x">>(out, 0.0);
  flight::row_set<flight::RowKey<"y">>(out, 0.0);
  flight::row_set<flight::RowKey<"normalX">>(out, 0.0);
  flight::row_set<flight::RowKey<"normalY">>(out, 0.0);
}

inline flight::Ref<flight::types::CollisionTimeOfImpact2D> create_collision_time_of_impact2_d() {
  auto out = flight::entity::allocate_entity<flight::Ref<flight::types::CollisionTimeOfImpact2D>>();
  initialize_collision_time_of_impact2_d(out);
  return flight::entity::finish_entity<flight::Ref<flight::types::CollisionTimeOfImpact2D>>(out);
}

inline flight::types::CollisionShape2D widen_collision_built_in_shape2_d(
    const flight::types::CollisionBuiltInShape2D& shape) {
  return std::visit(
      [](const auto& value) -> flight::types::CollisionShape2D {
        return flight::types::CollisionShape2D(value);
      },
      shape);
}

inline bool sweep_circle_circle(
    double ax,
    double ay,
    double radius_a,
    double bx,
    double by,
    double radius_b,
    double velocity_x,
    double velocity_y,
    flight::Ref<flight::types::CollisionTimeOfImpact2D> out) {
  const double offset_x = ax - bx;
  const double offset_y = ay - by;
  const double radius = radius_a + radius_b;
  const double a = velocity_x * velocity_x + velocity_y * velocity_y;
  if (!(a > 0.0)) return false;
  const double b = 2.0 * (offset_x * velocity_x + offset_y * velocity_y);
  const double c = offset_x * offset_x + offset_y * offset_y - radius * radius;
  const double discriminant = b * b - 4.0 * a * c;
  if (discriminant < 0.0) return false;
  const double fraction = (-b - std::sqrt(discriminant)) / (2.0 * a);
  if (fraction < 0.0) return false;
  const double normal_x = offset_x + velocity_x * fraction;
  const double normal_y = offset_y + velocity_y * fraction;
  const double length = std::hypot(normal_x, normal_y);
  if (!(length > 0.0)) return false;
  out->fraction = fraction;
  out->normal_x = normal_x / length;
  out->normal_y = normal_y / length;
  return true;
}

inline double ray_circle_fraction(
    double origin_x,
    double origin_y,
    double direction_x,
    double direction_y,
    double center_x,
    double center_y,
    double radius) {
  const double a = direction_x * direction_x + direction_y * direction_y;
  if (!(a > 0.0)) return -1.0;
  const double offset_x = origin_x - center_x;
  const double offset_y = origin_y - center_y;
  const double b = 2.0 * (offset_x * direction_x + offset_y * direction_y);
  const double c = offset_x * offset_x + offset_y * offset_y - radius * radius;
  const double discriminant = b * b - 4.0 * a * c;
  if (discriminant < 0.0) return -1.0;
  return (-b - std::sqrt(discriminant)) / (2.0 * a);
}

inline std::optional<flight::SequenceView<double>> write_shape_vertices(
    const flight::types::CollisionBuiltInShape2D& shape,
    flight::Float64Array scratch) {
  return std::visit(
      [&](const auto& value) -> std::optional<flight::SequenceView<double>> {
        using Value = std::remove_cvref_t<decltype(value)>;
        if constexpr (std::is_same_v<Value, flight::Ref<SweepAabb2D>>) {
          scratch.set_index(0.0, value->min_x);
          scratch.set_index(1.0, value->min_y);
          scratch.set_index(2.0, value->max_x);
          scratch.set_index(3.0, value->min_y);
          scratch.set_index(4.0, value->max_x);
          scratch.set_index(5.0, value->max_y);
          scratch.set_index(6.0, value->min_x);
          scratch.set_index(7.0, value->max_y);
          return flight::SequenceView<double>(scratch);
        } else if constexpr (std::is_same_v<Value, flight::Ref<SweepObb2D>>) {
          const double cosine = std::cos(value->rotation);
          const double sine = std::sin(value->rotation);
          const double width_x = cosine * value->half_w;
          const double width_y = sine * value->half_w;
          const double height_x = -sine * value->half_h;
          const double height_y = cosine * value->half_h;
          scratch.set_index(0.0, value->x - width_x - height_x);
          scratch.set_index(1.0, value->y - width_y - height_y);
          scratch.set_index(2.0, value->x + width_x - height_x);
          scratch.set_index(3.0, value->y + width_y - height_y);
          scratch.set_index(4.0, value->x + width_x + height_x);
          scratch.set_index(5.0, value->y + width_y + height_y);
          scratch.set_index(6.0, value->x - width_x + height_x);
          scratch.set_index(7.0, value->y - width_y + height_y);
          return flight::SequenceView<double>(scratch);
        } else if constexpr (std::is_same_v<Value, flight::Ref<SweepPolygon2D>>) {
          return flight::SequenceView<double>(value->points);
        } else {
          return std::nullopt;
        }
      },
      shape);
}

inline double polygon_area_twice(flight::SequenceView<double> vertices) {
  double area = 0.0;
  const std::size_t count = vertices.size() >> 1U;
  for (std::size_t index = 0; index < count; ++index) {
    const std::size_t next = (index + 1U) % count;
    area += vertices[index * 2U] * vertices[next * 2U + 1U] -
            vertices[next * 2U] * vertices[index * 2U + 1U];
  }
  return area;
}

inline bool sweep_circle_polygon(
    double center_x,
    double center_y,
    double radius,
    flight::SequenceView<double> vertices,
    double velocity_x,
    double velocity_y,
    flight::Ref<flight::types::CollisionTimeOfImpact2D> out) {
  double best_fraction = std::numeric_limits<double>::infinity();
  double best_normal_x = 0.0;
  double best_normal_y = 0.0;
  const std::size_t count = vertices.size() >> 1U;
  const double winding = polygon_area_twice(vertices) >= 0.0 ? 1.0 : -1.0;
  for (std::size_t index = 0; index < count; ++index) {
    const std::size_t next = (index + 1U) % count;
    const double x0 = vertices[index * 2U];
    const double y0 = vertices[index * 2U + 1U];
    const double x1 = vertices[next * 2U];
    const double y1 = vertices[next * 2U + 1U];
    const double edge_x = x1 - x0;
    const double edge_y = y1 - y0;
    const double edge_length = std::hypot(edge_x, edge_y);
    if (!(edge_length > 0.0)) continue;
    const double normal_x = winding * edge_y / edge_length;
    const double normal_y = -winding * edge_x / edge_length;
    const double speed = normal_x * velocity_x + normal_y * velocity_y;
    const double separation = normal_x * (center_x - x0) + normal_y * (center_y - y0);
    if (speed < 0.0 && separation >= radius) {
      const double fraction = (radius - separation) / speed;
      const double hit_center_x = center_x + velocity_x * fraction;
      const double hit_center_y = center_y + velocity_y * fraction;
      const double edge_fraction =
          ((hit_center_x - x0) * edge_x + (hit_center_y - y0) * edge_y) /
          (edge_length * edge_length);
      if (fraction >= 0.0 && edge_fraction >= 0.0 && edge_fraction <= 1.0 &&
          fraction < best_fraction) {
        best_fraction = fraction;
        best_normal_x = normal_x;
        best_normal_y = normal_y;
      }
    }
    const double vertex_fraction =
        ray_circle_fraction(center_x, center_y, velocity_x, velocity_y, x0, y0, radius);
    if (vertex_fraction >= 0.0 && vertex_fraction < best_fraction) {
      const double hit_x = center_x + velocity_x * vertex_fraction;
      const double hit_y = center_y + velocity_y * vertex_fraction;
      const double normal_length = std::hypot(hit_x - x0, hit_y - y0);
      if (normal_length > 0.0) {
        best_fraction = vertex_fraction;
        best_normal_x = (hit_x - x0) / normal_length;
        best_normal_y = (hit_y - y0) / normal_length;
      }
    }
  }
  if (!std::isfinite(best_fraction)) return false;
  out->fraction = best_fraction;
  out->normal_x = best_normal_x;
  out->normal_y = best_normal_y;
  return true;
}

inline void clear_collision_time_of_impact(
    flight::Ref<flight::types::CollisionTimeOfImpact2D> out) {
  out->fraction = 0.0;
  out->x = 0.0;
  out->y = 0.0;
  out->normal_x = 0.0;
  out->normal_y = 0.0;
}

inline double canonical_zero(double value) { return value == 0.0 ? 0.0 : value; }

struct CollisionSweepPiece : public flight::ReferenceEnabled {
  double x;
  double y;
  double radius;
  std::optional<flight::SequenceView<double>> vertices;
  flight::Float64Array storage;
};

struct CollisionSweepScratch : public flight::ReferenceEnabled {
  flight::Ref<flight::types::CollisionContactManifold2D> manifold;
  flight::Float64Array vertices_a;
  flight::Float64Array vertices_b;
  flight::Array<flight::Ref<CollisionSweepPiece>> pieces_a;
  flight::Array<flight::Ref<CollisionSweepPiece>> pieces_b;
  flight::Ref<flight::types::CollisionTimeOfImpact2D> piece_hit;
  double projection_min;
  double projection_max;
  double entry;
  double exit;
  double normal_x;
  double normal_y;
};

inline void write_shape_a_support(
    const flight::types::CollisionBuiltInShape2D& shape,
    double translation_x,
    double translation_y,
    flight::Ref<flight::types::CollisionTimeOfImpact2D> out,
    flight::Ref<CollisionSweepScratch> scratch) {
  const bool written = std::visit(
      [&](const auto& value) {
        using Value = std::remove_cvref_t<decltype(value)>;
        if constexpr (std::is_same_v<Value, flight::Ref<SweepCircle2D>>) {
          out->x = value->x + translation_x - out->normal_x * value->radius;
          out->y = value->y + translation_y - out->normal_y * value->radius;
          return true;
        } else if constexpr (std::is_same_v<Value, flight::Ref<SweepCapsule2D>>) {
          const double projection0 = value->x0 * out->normal_x + value->y0 * out->normal_y;
          const double projection1 = value->x1 * out->normal_x + value->y1 * out->normal_y;
          const double axis_x = projection0 <= projection1 ? value->x0 : value->x1;
          const double axis_y = projection0 <= projection1 ? value->y0 : value->y1;
          out->x = axis_x + translation_x - out->normal_x * value->radius;
          out->y = axis_y + translation_y - out->normal_y * value->radius;
          return true;
        } else {
          return false;
        }
      },
      shape);
  if (written) return;

  const auto vertices = write_shape_vertices(shape, scratch->vertices_a);
  if (!vertices) return;
  double best = -std::numeric_limits<double>::infinity();
  for (std::size_t index = 0; index < vertices->size(); index += 2U) {
    const double projection =
        -out->normal_x * (*vertices)[index] - out->normal_y * (*vertices)[index + 1U];
    if (projection > best) best = projection;
  }
  const double epsilon = 1e-9 * std::max(1.0, std::abs(best));
  double count = 0.0;
  double x = 0.0;
  double y = 0.0;
  for (std::size_t index = 0; index < vertices->size(); index += 2U) {
    const double projection =
        -out->normal_x * (*vertices)[index] - out->normal_y * (*vertices)[index + 1U];
    if (std::abs(projection - best) > epsilon) continue;
    x += (*vertices)[index];
    y += (*vertices)[index + 1U];
    count += 1.0;
  }
  if (count > 0.0) {
    out->x = x / count + translation_x;
    out->y = y / count + translation_y;
  }
}

inline void project_vertices(
    flight::SequenceView<double> vertices,
    double axis_x,
    double axis_y,
    flight::Ref<CollisionSweepScratch> scratch) {
  scratch->projection_min = std::numeric_limits<double>::infinity();
  scratch->projection_max = -std::numeric_limits<double>::infinity();
  for (std::size_t index = 0; index < vertices.size(); index += 2U) {
    const double projection = vertices[index] * axis_x + vertices[index + 1U] * axis_y;
    if (projection < scratch->projection_min) scratch->projection_min = projection;
    if (projection > scratch->projection_max) scratch->projection_max = projection;
  }
}

inline bool sweep_polygon_axes(
    flight::SequenceView<double> axes,
    flight::SequenceView<double> vertices_a,
    flight::SequenceView<double> vertices_b,
    double velocity_x,
    double velocity_y,
    flight::Ref<CollisionSweepScratch> scratch) {
  const std::size_t count = axes.size() >> 1U;
  for (std::size_t index = 0; index < count; ++index) {
    const std::size_t next = (index + 1U) % count;
    const double edge_x = axes[next * 2U] - axes[index * 2U];
    const double edge_y = axes[next * 2U + 1U] - axes[index * 2U + 1U];
    const double length = std::hypot(edge_x, edge_y);
    if (!(length > 0.0)) continue;
    const double axis_x = -edge_y / length;
    const double axis_y = edge_x / length;
    project_vertices(vertices_a, axis_x, axis_y, scratch);
    const double min_a = scratch->projection_min;
    const double max_a = scratch->projection_max;
    project_vertices(vertices_b, axis_x, axis_y, scratch);
    const double min_b = scratch->projection_min;
    const double max_b = scratch->projection_max;
    const double speed = velocity_x * axis_x + velocity_y * axis_y;
    if (speed == 0.0) {
      if (max_a < min_b || max_b < min_a) return false;
      continue;
    }
    double entry = (min_b - max_a) / speed;
    double exit = (max_b - min_a) / speed;
    double normal_x = -axis_x;
    double normal_y = -axis_y;
    if (entry > exit) {
      std::swap(entry, exit);
      normal_x = axis_x;
      normal_y = axis_y;
    }
    if (entry > scratch->entry) {
      scratch->entry = entry;
      scratch->normal_x = normal_x;
      scratch->normal_y = normal_y;
    }
    if (exit < scratch->exit) scratch->exit = exit;
    if (scratch->entry > scratch->exit) return false;
  }
  return true;
}

inline bool sweep_polygon_polygon(
    flight::SequenceView<double> vertices_a,
    flight::SequenceView<double> vertices_b,
    double velocity_x,
    double velocity_y,
    double max_fraction,
    flight::Ref<flight::types::CollisionTimeOfImpact2D> out,
    flight::Ref<CollisionSweepScratch> scratch) {
  scratch->entry = -std::numeric_limits<double>::infinity();
  scratch->exit = max_fraction;
  scratch->normal_x = 0.0;
  scratch->normal_y = 0.0;
  if (!sweep_polygon_axes(vertices_a, vertices_a, vertices_b, velocity_x, velocity_y, scratch)) {
    return false;
  }
  if (!sweep_polygon_axes(vertices_b, vertices_a, vertices_b, velocity_x, velocity_y, scratch)) {
    return false;
  }
  if (scratch->entry < 0.0 || scratch->entry > scratch->exit ||
      scratch->entry > max_fraction) {
    return false;
  }
  out->fraction = scratch->entry;
  out->normal_x = scratch->normal_x;
  out->normal_y = scratch->normal_y;
  return true;
}

inline bool sweep_piece_pair(
    const flight::Ref<CollisionSweepPiece>& a,
    const flight::Ref<CollisionSweepPiece>& b,
    double velocity_x,
    double velocity_y,
    double max_fraction,
    flight::Ref<flight::types::CollisionTimeOfImpact2D> out,
    flight::Ref<CollisionSweepScratch> scratch) {
  if (!a->vertices && !b->vertices) {
    return sweep_circle_circle(
        a->x, a->y, a->radius, b->x, b->y, b->radius, velocity_x, velocity_y, out);
  }
  if (!a->vertices) {
    return sweep_circle_polygon(
        a->x, a->y, a->radius, *b->vertices, velocity_x, velocity_y, out);
  }
  if (!b->vertices) {
    if (!sweep_circle_polygon(
            b->x, b->y, b->radius, *a->vertices, -velocity_x, -velocity_y, out)) {
      return false;
    }
    out->normal_x = -out->normal_x;
    out->normal_y = -out->normal_y;
    return true;
  }
  return sweep_polygon_polygon(
      *a->vertices, *b->vertices, velocity_x, velocity_y, max_fraction, out, scratch);
}

inline flight::Array<flight::Ref<CollisionSweepPiece>> create_sweep_pieces() {
  flight::Array<flight::Ref<CollisionSweepPiece>> pieces;
  for (std::size_t index = 0; index < 3U; ++index) {
    pieces.push(flight::make_ref<CollisionSweepPiece>(CollisionSweepPiece{
        .x = 0.0,
        .y = 0.0,
        .radius = 0.0,
        .vertices = std::nullopt,
        .storage = flight::Float64Array(8.0),
    }));
  }
  return pieces;
}

inline flight::Ref<CollisionSweepScratch> create_collision_sweep_scratch() {
  return flight::make_ref<CollisionSweepScratch>(CollisionSweepScratch{
      .manifold = create_collision_contact_manifold2_d(),
      .vertices_a = flight::Float64Array(8.0),
      .vertices_b = flight::Float64Array(8.0),
      .pieces_a = create_sweep_pieces(),
      .pieces_b = create_sweep_pieces(),
      .piece_hit = create_collision_time_of_impact2_d(),
      .projection_min = 0.0,
      .projection_max = 0.0,
      .entry = 0.0,
      .exit = 0.0,
      .normal_x = 0.0,
      .normal_y = 0.0,
  });
}

inline flight::Array<flight::Ref<CollisionSweepScratch>> collision_sweep_scratch_pool{
    create_collision_sweep_scratch()};

inline flight::Ref<CollisionSweepScratch> acquire_collision_sweep_scratch() {
  const auto scratch = collision_sweep_scratch_pool.pop();
  return scratch.value_or(create_collision_sweep_scratch());
}

inline void release_collision_sweep_scratch(flight::Ref<CollisionSweepScratch> scratch) {
  collision_sweep_scratch_pool.push(std::move(scratch));
}

inline constexpr double sweep_epsilon = 1e-9;

inline double write_capsule_pieces(
    const flight::types::CollisionBuiltInShape2D& shape,
    flight::Array<flight::Ref<CollisionSweepPiece>> pieces) {
  if (const auto* circle = std::get_if<flight::Ref<SweepCircle2D>>(&shape)) {
    auto piece = pieces.element(0.0);
    piece->x = (*circle)->x;
    piece->y = (*circle)->y;
    piece->radius = (*circle)->radius;
    piece->vertices = std::nullopt;
    return 1.0;
  }
  if (const auto* capsule = std::get_if<flight::Ref<SweepCapsule2D>>(&shape)) {
    auto first = pieces.element(0.0);
    first->x = (*capsule)->x0;
    first->y = (*capsule)->y0;
    first->radius = (*capsule)->radius;
    first->vertices = std::nullopt;
    const double axis_x = (*capsule)->x1 - (*capsule)->x0;
    const double axis_y = (*capsule)->y1 - (*capsule)->y0;
    const double length = std::hypot(axis_x, axis_y);
    if (length <= sweep_epsilon) return 1.0;

    auto second = pieces.element(1.0);
    second->x = (*capsule)->x1;
    second->y = (*capsule)->y1;
    second->radius = (*capsule)->radius;
    second->vertices = std::nullopt;

    const double offset_x = -axis_y / length * (*capsule)->radius;
    const double offset_y = axis_x / length * (*capsule)->radius;
    auto body = pieces.element(2.0);
    body->storage.set_index(0.0, (*capsule)->x0 + offset_x);
    body->storage.set_index(1.0, (*capsule)->y0 + offset_y);
    body->storage.set_index(2.0, (*capsule)->x1 + offset_x);
    body->storage.set_index(3.0, (*capsule)->y1 + offset_y);
    body->storage.set_index(4.0, (*capsule)->x1 - offset_x);
    body->storage.set_index(5.0, (*capsule)->y1 - offset_y);
    body->storage.set_index(6.0, (*capsule)->x0 - offset_x);
    body->storage.set_index(7.0, (*capsule)->y0 - offset_y);
    body->vertices = flight::SequenceView<double>(body->storage);
    return 3.0;
  }

  auto piece = pieces.element(0.0);
  const auto vertices = write_shape_vertices(shape, piece->storage);
  if (!vertices) return 0.0;
  piece->vertices = *vertices;
  return 1.0;
}

inline bool sweep_capsule_pair(
    const flight::types::CollisionBuiltInShape2D& shape_a,
    const flight::types::CollisionBuiltInShape2D& shape_b,
    double velocity_x,
    double velocity_y,
    double max_fraction,
    flight::Ref<flight::types::CollisionTimeOfImpact2D> out,
    flight::Ref<CollisionSweepScratch> scratch) {
  const double count_a = write_capsule_pieces(shape_a, scratch->pieces_a);
  const double count_b = write_capsule_pieces(shape_b, scratch->pieces_b);
  if (count_a == 0.0 || count_b == 0.0) return false;
  bool hit = false;
  double best_fraction = std::numeric_limits<double>::infinity();
  for (double index_a = 0.0; index_a < count_a; index_a += 1.0) {
    for (double index_b = 0.0; index_b < count_b; index_b += 1.0) {
      if (!sweep_piece_pair(
              scratch->pieces_a.element(index_a),
              scratch->pieces_b.element(index_b),
              velocity_x,
              velocity_y,
              max_fraction,
              scratch->piece_hit,
              scratch)) {
        continue;
      }
      if (scratch->piece_hit->fraction >= best_fraction) continue;
      best_fraction = scratch->piece_hit->fraction;
      hit = true;
      out->fraction = scratch->piece_hit->fraction;
      out->normal_x = scratch->piece_hit->normal_x;
      out->normal_y = scratch->piece_hit->normal_y;
      out->x = scratch->piece_hit->x;
      out->y = scratch->piece_hit->y;
    }
  }
  return hit;
}

inline bool sweep_collision_shape_with_scratch(
    const flight::types::CollisionBuiltInShape2D& shape_a,
    double translation_ax,
    double translation_ay,
    const flight::types::CollisionBuiltInShape2D& shape_b,
    double translation_bx,
    double translation_by,
    flight::Ref<flight::types::CollisionTimeOfImpact2D> out,
    double max_fraction,
    flight::Ref<CollisionSweepScratch> scratch) {
  if (collide_contact_manifold2_d(shape_a, shape_b, scratch->manifold)) {
    out->normal_x = canonical_zero(scratch->manifold->normal_x);
    out->normal_y = canonical_zero(scratch->manifold->normal_y);
    write_shape_a_support(shape_a, 0.0, 0.0, out, scratch);
    return true;
  }

  const double relative_x = translation_ax - translation_bx;
  const double relative_y = translation_ay - translation_by;
  bool hit = false;
  if (std::holds_alternative<flight::Ref<SweepCapsule2D>>(shape_a) ||
      std::holds_alternative<flight::Ref<SweepCapsule2D>>(shape_b)) {
    hit = sweep_capsule_pair(
        shape_a, shape_b, relative_x, relative_y, max_fraction, out, scratch);
  } else if (const auto* circle_a = std::get_if<flight::Ref<SweepCircle2D>>(&shape_a)) {
    if (const auto* circle_b = std::get_if<flight::Ref<SweepCircle2D>>(&shape_b)) {
      hit = sweep_circle_circle(
          (*circle_a)->x,
          (*circle_a)->y,
          (*circle_a)->radius,
          (*circle_b)->x,
          (*circle_b)->y,
          (*circle_b)->radius,
          relative_x,
          relative_y,
          out);
    } else if (const auto vertices_b = write_shape_vertices(shape_b, scratch->vertices_b)) {
      hit = sweep_circle_polygon(
          (*circle_a)->x,
          (*circle_a)->y,
          (*circle_a)->radius,
          *vertices_b,
          relative_x,
          relative_y,
          out);
    }
  } else if (const auto* circle_b = std::get_if<flight::Ref<SweepCircle2D>>(&shape_b)) {
    if (const auto vertices_a = write_shape_vertices(shape_a, scratch->vertices_a)) {
      hit = sweep_circle_polygon(
          (*circle_b)->x,
          (*circle_b)->y,
          (*circle_b)->radius,
          *vertices_a,
          -relative_x,
          -relative_y,
          out);
      if (hit) {
        out->normal_x = -out->normal_x;
        out->normal_y = -out->normal_y;
      }
    }
  } else {
    const auto vertices_a = write_shape_vertices(shape_a, scratch->vertices_a);
    const auto vertices_b = write_shape_vertices(shape_b, scratch->vertices_b);
    if (vertices_a && vertices_b) {
      hit = sweep_polygon_polygon(
          *vertices_a, *vertices_b, relative_x, relative_y, max_fraction, out, scratch);
    }
  }

  if (!hit || out->fraction < 0.0 || out->fraction > max_fraction) {
    clear_collision_time_of_impact(out);
    return false;
  }
  out->normal_x = canonical_zero(out->normal_x);
  out->normal_y = canonical_zero(out->normal_y);
  write_shape_a_support(
      shape_a,
      translation_ax * out->fraction,
      translation_ay * out->fraction,
      out,
      scratch);
  return true;
}

inline bool sweep_collision_shape2_d(
    flight::types::CollisionBuiltInShape2D shape_a,
    double translation_ax,
    double translation_ay,
    flight::types::CollisionBuiltInShape2D shape_b,
    double translation_bx,
    double translation_by,
    flight::Ref<flight::types::CollisionTimeOfImpact2D> out,
    std::optional<double> max_fraction = std::nullopt) {
  const double limit = max_fraction.value_or(1.0);
  clear_collision_time_of_impact(out);
  if (!std::isfinite(translation_ax) || !std::isfinite(translation_ay) ||
      !std::isfinite(translation_bx) || !std::isfinite(translation_by) ||
      !std::isfinite(limit) || limit < 0.0 ||
      get_collision_shape_validation_status2_d(widen_collision_built_in_shape2_d(shape_a)) ||
      get_collision_shape_validation_status2_d(widen_collision_built_in_shape2_d(shape_b))) {
    return false;
  }

  auto scratch = acquire_collision_sweep_scratch();
  std::optional<bool> result;
  std::exception_ptr failure;
  try {
    result = sweep_collision_shape_with_scratch(
        shape_a,
        translation_ax,
        translation_ay,
        shape_b,
        translation_bx,
        translation_by,
        out,
        limit,
        scratch);
  } catch (...) {
    failure = std::current_exception();
  }
  release_collision_sweep_scratch(std::move(scratch));
  if (failure) std::rethrow_exception(failure);
  return result.value();
}

} // namespace flight::collision
