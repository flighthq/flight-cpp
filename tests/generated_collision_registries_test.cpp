// Focused native parity for the @flighthq/collision kind registries, re-derived at a627849 against
// the String-kind registry and CollisionShape3D/2D variant surface. The face, support, and guard
// modules are supplied by overrides because the emitter still refuses their per-kind bodies; these
// checks pin what those overrides promise: String-keyed registration with overwrite, NUL-delimited
// pair keys that cannot collide with concatenated kinds, absent lookups for unregistered kinds, the
// source support formulas, the AABB face corners, the capsule alignment cutoff, and the guard
// install/uninstall flag.
#include <flight/collision/collision_face3_d.hpp>
#include <flight/collision/collision_support2_d.hpp>
#include <flight/collision/collision_support3_d.hpp>
#include <flight/collision/enable_collision_guards.hpp>

#include <cmath>
#include <iostream>
#include <string>
#include <utility>

namespace {

namespace collision = flight::collision;
namespace types = flight::types;

using Aabb2D = types::min_x_min_y_max_x_max_y_kind_329d6080b4f31db4;
using Circle2D = types::x_y_radius_kind_90fbb3b4c4569518;
using Aabb3D = types::min_x_min_y_min_z_max_x_max_y_max_z_kind_748c675e8e0cf88a;
using Capsule3D = types::x0_y0_z0_x1_y1_z1_radius_kind_3ca9ada7b527e64b;
using Sphere3D = types::x_y_z_radius_kind_b0bc53ca3f0e8006;

int failures = 0;

void check(bool condition, const std::string& message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

bool near(double actual, double expected) { return std::abs(actual - expected) <= 1e-12; }

template <typename Value>
types::CollisionShape2D shape2_d(Value value) {
  return types::CollisionShape2D{std::in_place_type<flight::Ref<Value>>, flight::make_ref<Value>(std::move(value))};
}

template <typename Value>
types::CollisionShape3D shape3_d(Value value) {
  return types::CollisionShape3D{std::in_place_type<flight::Ref<Value>>, flight::make_ref<Value>(std::move(value))};
}

void check_face_queries3_d() {
  check(!collision::get_collision_face_query3_d(flight::String("box")).has_value(),
        "an unregistered face kind is absent before built-ins are registered");
  collision::register_built_in_collision_face_queries3_d();
  check(collision::get_collision_face_query3_d(flight::String("box")).has_value(), "box face query registered");
  check(!collision::get_collision_face_query3_d(flight::String("sphere")).has_value(),
        "sphere has no face query, so its lookup stays absent");

  flight::Array<double> out(18.0);
  const auto aabb = shape3_d(Aabb3D{
      .min_x = -1.0, .min_y = -2.0, .min_z = -3.0, .max_x = 1.0, .max_y = 2.0, .max_z = 3.0,
      .kind = flight::String("aabb")});
  const auto aabb_query = *collision::get_collision_face_query3_d(flight::String("aabb"));
  check(aabb_query(aabb, 1.0, 0.0, 0.0, out) == 4.0, "AABB face query writes four corners");
  bool on_face = true;
  for (double corner = 0.0; corner < 4.0; corner += 1.0) {
    on_face = on_face && near(out.element(corner * 3.0), 1.0) &&
              near(std::abs(out.element(corner * 3.0 + 1.0)), 2.0) &&
              near(std::abs(out.element(corner * 3.0 + 2.0)), 3.0);
  }
  check(on_face, "AABB +X face corners lie on max_x at the extents");

  const auto capsule = shape3_d(Capsule3D{
      .x0 = 0.0, .y0 = 0.0, .z0 = 0.0, .x1 = 4.0, .y1 = 0.0, .z1 = 0.0, .radius = 0.5,
      .kind = flight::String("capsule")});
  const auto capsule_query = *collision::get_collision_face_query3_d(flight::String("capsule"));
  check(capsule_query(capsule, 1.0, 0.0, 0.0, out) == 0.0, "an axis-aligned capsule direction has no face");
  check(capsule_query(capsule, 0.0, 2.0, 0.0, out) == 2.0, "a side direction yields the capsule edge");
  check(near(out.element(0.0), 0.0) && near(out.element(1.0), 0.5) && near(out.element(3.0), 4.0) &&
            near(out.element(4.0), 0.5),
        "capsule edge is offset by the radius along the unit direction");

  int calls = 0;
  collision::register_collision_face_query3_d(
      flight::String("aabb"),
      [&calls](types::CollisionShape3D, double, double, double, flight::Array<double>) {
        ++calls;
        return 7.0;
      });
  check((*collision::get_collision_face_query3_d(flight::String("aabb")))(aabb, 1.0, 0.0, 0.0, out) == 7.0 &&
            calls == 1,
        "re-registering a face kind replaces the previous query");
}

void check_supports2_d() {
  collision::register_built_in_collision_supports2_d();
  flight::Array<double> out{0.0, 0.0};
  const auto circle = shape2_d(Circle2D{.x = 1.0, .y = 1.0, .radius = 5.0, .kind = flight::String("circle")});
  (*collision::get_collision_support2_d(flight::String("circle")))(circle, 3.0, 4.0, out);
  check(near(out.element(0.0), 4.0) && near(out.element(1.0), 5.0), "circle support is centre plus radius along the unit direction");

  const auto aabb = shape2_d(Aabb2D{.min_x = -1.0, .min_y = -2.0, .max_x = 3.0, .max_y = 4.0, .kind = flight::String("aabb")});
  (*collision::get_collision_support2_d(flight::String("aabb")))(aabb, -1.0, 1.0, out);
  check(near(out.element(0.0), -1.0) && near(out.element(1.0), 4.0), "AABB support picks the corner per axis sign");
  check(!collision::get_collision_support2_d(flight::String("segment")).has_value(), "segment has no support function");

  const flight::String key = collision::get_collision_pair_key2_d(flight::String("a"), flight::String("bc"));
  check(key.length() == 4 && key != collision::get_collision_pair_key2_d(flight::String("ab"), flight::String("c")),
        "pair keys keep the NUL separator so concatenations cannot collide");
  collision::register_collision_pair_test2_d(
      flight::String("a"), flight::String("bc"),
      types::CollisionPairTest2D{[](types::CollisionShape2D, types::CollisionShape2D, flight::Ref<types::CollisionManifold2D>) { return true; }});
  check(collision::get_collision_pair_test2_d(flight::String("a"), flight::String("bc")).has_value(), "registered pair test is found");
  check(!collision::get_collision_pair_test2_d(flight::String("ab"), flight::String("c")).has_value(),
        "a pair whose kinds concatenate identically is not found");
  check(!collision::get_collision_pair_test2_d(flight::String("bc"), flight::String("a")).has_value(),
        "pair keys are ordered");
}

void check_supports3_d() {
  collision::register_built_in_collision_supports3_d();
  flight::Array<double> out{0.0, 0.0, 0.0};
  const auto aabb = shape3_d(Aabb3D{
      .min_x = -1.0, .min_y = -2.0, .min_z = -3.0, .max_x = 1.0, .max_y = 2.0, .max_z = 3.0,
      .kind = flight::String("aabb")});
  (*collision::get_collision_support3_d(flight::String("aabb")))(aabb, 1.0, -1.0, 1.0, out);
  check(near(out.element(0.0), 1.0) && near(out.element(1.0), -2.0) && near(out.element(2.0), 3.0),
        "3D AABB support picks the corner per axis sign");

  const auto sphere = shape3_d(Sphere3D{.x = 1.0, .y = 2.0, .z = 3.0, .radius = 2.0, .kind = flight::String("sphere")});
  (*collision::get_collision_support3_d(flight::String("sphere")))(sphere, 0.0, 0.0, -5.0, out);
  check(near(out.element(0.0), 1.0) && near(out.element(1.0), 2.0) && near(out.element(2.0), 1.0),
        "sphere support is centre plus radius along the unit direction");
  check(!collision::get_collision_support3_d(flight::String("vendor:thing")).has_value(), "unregistered vendor kind is absent");
}

void check_guards() {
  check(!collision::are_collision_guards_enabled(), "guards start disabled");
  collision::enable_collision_guards();
  check(collision::are_collision_guards_enabled(), "enable installs the guards");
  collision::disable_collision_guards();
  check(!collision::are_collision_guards_enabled(), "disable uninstalls the guards");
  check(!collision::is_warnable_collision_status(std::nullopt), "a usable shape is not warnable");
  check(collision::is_warnable_collision_status(flight::String("unsupported-shape-kind")), "unsupported kinds warn");
  check(!collision::is_warnable_collision_status(flight::String("separated")), "result statuses do not warn");
}

}  // namespace

int main() {
  check_face_queries3_d();
  check_supports2_d();
  check_supports3_d();
  check_guards();
  if (failures != 0) {
    std::cerr << failures << " collision registry check(s) failed\n";
    return 1;
  }
  std::cout << "collision registry checks passed\n";
  return 0;
}
