// Shared fixtures for the native @flighthq/physics2d tests.
//
// createPhysics2DCollider is refused at a627849: it stores createPhysics2DColliderWorldShape's
// `CollisionBuiltInShape2D & Entity` result in Physics2DCollider.world, which is declared as the plain union, and
// the two are sibling carriers (docs/compiler-requests/entity-intersection-arm-sibling-carriers.md). collider()
// below follows the refused function step by step, through the generated pieces that do emit:
// - clonePhysics2DLocalShape for the owned local shape;
// - allocateEntity and initializePhysics2DCollider, with the same default filter {1, 0xffffffff, 0} and a copied
//   material;
// - finishEntity.
// The one difference is the world shape. It is built as the plain arm createPhysics2DColliderWorldShape would
// fill (aabb and obb become a zeroed obb, a capsule keeps its radius, a polygon gets a copy of its points), but
// without the entity marker that shape carries in TypeScript. Physics never reads that marker, and the step
// rewrites the world shape from the local one before using it. Tests that depend on this fixture are counted
// separately in the arc's report.
#pragma once

#include <flight/entity/entity.hpp>
#include <flight/physics2d/world.hpp>

#include <cmath>
#include <initializer_list>
#include <optional>
#include <vector>

namespace flight::physics2d_test {

namespace types = flight::types;
using Shape = types::CollisionBuiltInShape2D;
using Collider = flight::Ref<types::Physics2DCollider>;
using Body = flight::Ref<types::RigidBody2D>;
using World = flight::Ref<types::Physics2DWorld>;

template <typename Pointer>
auto view(const Pointer& value) {
  using Element = typename Pointer::element_type;
  return flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<Element>>>>(value);
}

template <typename Pointer>
auto writable(const Pointer& value) {
  using Element = typename Pointer::element_type;
  return flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<Element>>>>(value);
}

inline Shape circle(double x, double y, double radius) {
  using T = types::x_y_radius_kind_90fbb3b4c4569518;
  return flight::make_ref<T>(T{.x = x, .y = y, .radius = radius, .kind = flight::String("circle")});
}
inline Shape aabb(double min_x, double min_y, double max_x, double max_y) {
  using T = types::min_x_min_y_max_x_max_y_kind_329d6080b4f31db4;
  return flight::make_ref<T>(T{.min_x = min_x, .min_y = min_y, .max_x = max_x, .max_y = max_y, .kind = flight::String("aabb")});
}
inline Shape obb(double x, double y, double half_w, double half_h, double rotation) {
  using T = types::x_y_half_w_half_h_rotation_kind_8ffcea5c657a2268;
  return flight::make_ref<T>(
      T{.x = x, .y = y, .half_w = half_w, .half_h = half_h, .rotation = rotation, .kind = flight::String("obb")});
}
inline Shape capsule(double x0, double y0, double x1, double y1, double radius) {
  using T = types::x0_y0_x1_y1_radius_kind_768ba062d07f3ff6;
  return flight::make_ref<T>(T{.x0 = x0, .y0 = y0, .x1 = x1, .y1 = y1, .radius = radius, .kind = flight::String("capsule")});
}
inline Shape polygon(std::initializer_list<double> points) {
  using T = types::points_kind_9b00ebdad27cdb4e;
  flight::Array<double> array;
  for (double value : points) array.push(value);
  return flight::make_ref<T>(T{.points = array, .kind = flight::String("polygon")});
}
inline Shape segment(double x0, double y0, double x1, double y1) {
  using T = types::x0_y0_x1_y1_kind_d4f616534491406f;
  return flight::make_ref<T>(T{.x0 = x0, .y0 = y0, .x1 = x1, .y1 = y1, .kind = flight::String("segment")});
}
inline Shape point(double x, double y) {
  using T = types::x_y_kind_35ad667397176b0f;
  return flight::make_ref<T>(T{.x = x, .y = y, .kind = flight::String("point")});
}

inline flight::Ref<types::Physics2DMaterial> material(double density = 1, double friction = 0.5, double restitution = 0) {
  return flight::make_ref<types::Physics2DMaterial>(
      types::Physics2DMaterial{.density = density, .friction = friction, .restitution = restitution});
}

inline flight::Ref<types::Physics2DCollisionFilter> filter(double category, double mask, double group = 0) {
  return flight::make_ref<types::Physics2DCollisionFilter>(
      types::Physics2DCollisionFilter{.category_bits = category, .mask_bits = mask, .group_index = group});
}

// The world-space arm createPhysics2DColliderWorldShape allocates for `local`, without its entity marker.
inline Shape world_shape_for(const Shape& local) {
  return std::visit(
      [](const auto& value) -> Shape {
        const flight::String kind = value->kind;
        if (kind == flight::String("circle")) {
          const auto& c = std::get<flight::Ref<types::x_y_radius_kind_90fbb3b4c4569518>>(Shape(value));
          return circle(c->x, c->y, c->radius);
        }
        if (kind == flight::String("aabb") || kind == flight::String("obb")) return obb(0, 0, 0, 0, 0);
        if (kind == flight::String("capsule")) {
          const auto& c = std::get<flight::Ref<types::x0_y0_x1_y1_radius_kind_768ba062d07f3ff6>>(Shape(value));
          return capsule(0, 0, 0, 0, c->radius);
        }
        if (kind == flight::String("polygon")) {
          using T = types::points_kind_9b00ebdad27cdb4e;
          const auto& p = std::get<flight::Ref<T>>(Shape(value));
          return flight::make_ref<T>(T{.points = p->points.slice(), .kind = flight::String("polygon")});
        }
        if (kind == flight::String("segment")) {
          const auto& s = std::get<flight::Ref<types::x0_y0_x1_y1_kind_d4f616534491406f>>(Shape(value));
          return segment(s->x0, s->y0, s->x1, s->y1);
        }
        if (kind == flight::String("point")) {
          const auto& p = std::get<flight::Ref<types::x_y_kind_35ad667397176b0f>>(Shape(value));
          return point(p->x, p->y);
        }
        return point(0, 0);
      },
      local);
}

// createPhysics2DCollider(local, material, sensor, filter), as described at the top of this file.
inline Collider collider(const Shape& local, const flight::Ref<types::Physics2DMaterial>& source_material = material(),
                         bool sensor = false,
                         std::optional<flight::Ref<types::Physics2DCollisionFilter>> source_filter = std::nullopt) {
  const Shape owned_local = flight::physics2d::clone_physics2_dlocal_shape(local);
  auto out = flight::entity::allocate_entity<Collider>();
  auto owned_filter = source_filter.has_value() ? filter(source_filter.value()->category_bits, source_filter.value()->mask_bits,
                                                         source_filter.value()->group_index)
                                                : filter(1, 0xffffffff, 0);
  flight::physics2d::initialize_physics2_dcollider(
      out, owned_filter, owned_local,
      material(source_material->density, source_material->friction, source_material->restitution), sensor,
      world_shape_for(owned_local));
  return flight::entity::finish_entity<Collider>(out);
}

inline Body body(const char* type, double x, double y, double angle = 0) {
  return flight::physics2d::create_rigid_body2_d(flight::String(type), x, y, angle);
}

// createPhysics2DWorld(): its own default gravity, (0, -9.81).
inline World world() { return flight::physics2d::create_physics2_dworld(); }

inline World world(double gravity_x, double gravity_y) {
  return flight::physics2d::create_physics2_dworld(gravity_x, gravity_y);
}

}  // namespace flight::physics2d_test
