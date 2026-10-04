// Derived from @flighthq/collision/packages/collision/src/enableCollisionGuards.ts.
#pragma once

#include <optional>
#include <variant>

#include <flight/collision/collision_shape_validation2_d.hpp>
#include <flight/collision/collision_shape_validation3_d.hpp>
#include <flight/collision/test_collision2_d.hpp>
#include <flight/collision/test_collision3_d.hpp>
#include <flight/log/contract.hpp>
#include <flight/runtime.hpp>
#include <flight/types/collision.hpp>
#include <flight/types/log.hpp>

static_assert(flight::runtime_contract.compiler_contract == "flight-runtime-contract/2", "Flight compiler/runtime contract mismatch");
static_assert(flight::runtime_contract.cpp_abi == 1, "Flight C++ runtime ABI mismatch");

namespace flight::collision {

inline bool collision_guards_enabled = false;

inline bool is_warnable_collision_status(const std::optional<flight::String>& status) {
  return status == flight::String("degenerate-shape") ||
         status == flight::String("non-convex-polygon") ||
         status == flight::String("unsupported-shape-kind");
}

inline flight::String collision_guard_message(
    const flight::String& dimension,
    const flight::String& status) {
  if (dimension == flight::String("2D")) {
    if (status == flight::String("non-convex-polygon")) {
      return flight::String(
          "testCollision2D: a polygon is non-convex and cannot produce a supported manifold — "
          "call explainCollisionTest2D(a, b) and replace the reported shape with a convex polygon.");
    }
    if (status == flight::String("unsupported-shape-kind")) {
      return flight::String(
          "testCollision2D: a shape kind has no manifold path and was reported as not overlapping — "
          "call explainCollisionTest2D(a, b) for the kind. Segments and points are area-less by design "
          "and answer the boolean testSegment*Collision and getCollisionShapeContainsPoint2D lanes instead.");
    }
    return flight::String(
        "testCollision2D: a shape is degenerate and cannot produce a manifold — call "
        "explainCollisionTest2D(a, b) and replace the reported shape with a finite positive-area collider.");
  }
  if (status == flight::String("unsupported-shape-kind")) {
    return flight::String(
        "testCollision3D: a shape kind has no registered support function and was reported as not "
        "overlapping — call registerBuiltInCollisionSupports3D() once at startup, or "
        "registerCollisionSupport3D(kind, support) for a vendor kind. Call explainCollisionTest3D(a, b) "
        "for which shape it was.");
  }
  return flight::String(
      "testCollision3D: a shape is degenerate and cannot produce a manifold — call "
      "explainCollisionTest3D(a, b) and replace the reported shape with a finite collider of positive extent.");
}

inline void warn_on_invalid_collision_shape(
    const flight::String& kind,
    double shape_index,
    const flight::String& status,
    const flight::String& dimension) {
  const flight::String message = collision_guard_message(dimension, status);
  flight::log::log_once(
      flight::String("collision:") + dimension + flight::String(":") + status +
          flight::String(":") + flight::to_string(shape_index) + flight::String(":") + kind,
      flight::types::LogLevel::Warn,
      flight::types::LogData{
          std::in_place_type<flight::Record<flight::String, flight::Any>>,
          flight::Record<flight::String, flight::Any>{
              {flight::String("kind"), kind},
              {flight::String("message"), message},
              {flight::String("shapeIndex"), shape_index},
              {flight::String("status"), status},
          },
      },
      std::optional<flight::String>{flight::String("collision")});
}

inline void warn_on_invalid_collision_shapes(
    flight::types::CollisionShape2D first,
    flight::types::CollisionShape2D second) {
  const auto first_status = get_collision_shape_validation_status2_d(first);
  if (is_warnable_collision_status(first_status)) {
    warn_on_invalid_collision_shape(
        std::visit([](const auto& value) { return value->kind; }, first),
        0.0,
        first_status.value(),
        flight::String("2D"));
    return;
  }
  const auto second_status = get_collision_shape_validation_status2_d(second);
  if (is_warnable_collision_status(second_status)) {
    warn_on_invalid_collision_shape(
        std::visit([](const auto& value) { return value->kind; }, second),
        1.0,
        second_status.value(),
        flight::String("2D"));
  }
}

inline void warn_on_invalid_collision_shapes3_d(
    flight::types::CollisionShape3D first,
    flight::types::CollisionShape3D second) {
  const auto first_status = get_collision_shape_validation_status3_d(first);
  if (is_warnable_collision_status(first_status)) {
    warn_on_invalid_collision_shape(
        std::visit([](const auto& value) { return value->kind; }, first),
        0.0,
        first_status.value(),
        flight::String("3D"));
    return;
  }
  const auto second_status = get_collision_shape_validation_status3_d(second);
  if (is_warnable_collision_status(second_status)) {
    warn_on_invalid_collision_shape(
        std::visit([](const auto& value) { return value->kind; }, second),
        1.0,
        second_status.value(),
        flight::String("3D"));
  }
}

inline bool are_collision_guards_enabled() { return collision_guards_enabled; }

inline void disable_collision_guards() {
  set_collision_test_guard2_d(std::nullopt);
  set_collision_test_guard3_d(std::nullopt);
  collision_guards_enabled = false;
}

inline void enable_collision_guards() {
  set_collision_test_guard2_d(warn_on_invalid_collision_shapes);
  set_collision_test_guard3_d(warn_on_invalid_collision_shapes3_d);
  collision_guards_enabled = true;
}

} // namespace flight::collision
