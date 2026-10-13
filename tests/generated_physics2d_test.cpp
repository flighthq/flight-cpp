// Native parity test for @flighthq/physics2d's reachable surface at a627849, the package's first.
//
// The package had no native test of any kind -- no file, no registration, no compile record -- and it was still at the
// old pin, so this unit regenerated it before writing anything. That regeneration brings it to a627849 at 21 of 26
// modules emitted, and of its 26 headers **10 compile**. This file covers the two whose behaviour is closed-form and
// reachable without a world: material mixing and derived mass properties.
//
// What is NOT covered, and why it is not faked: everything that needs a `Physics2DWorld` -- the step, the broadphase,
// the solver, the joints, the queries, the stepping assertions, the joint-suppression index -- because `world.hpp` is
// one of the 16 headers that do not compile. Its blocker is a single defect that takes 14 of them down together: a
// bounds literal a physics2d module builds is emitted as a module-local anonymous owner and passed where the named
// `flight::types::SpatialAabb2D` is declared. That is filed in
// docs/compiler-requests/contextual-object-return-nominal-owner.md with the site, and no workaround is carried.
//
//   * mixPhysics2DFriction / mixPhysics2DRestitution, from material.test.ts. Friction is the geometric mean, chosen so
//     equal surfaces keep their coefficient and either surface at zero makes the pair frictionless -- and so two large
//     finite coefficients do not overflow on the way. Restitution takes the bouncier surface. Both are symmetric,
//     which is what makes them a property of the PAIR rather than of one surface.
//   * computePhysics2DColliderMassData, from massProperties.test.ts. Mass and inertia are DERIVED from the collider's
//     own geometry and density, never assigned, so a body cannot be given an inertia that contradicts its outline.
//     The cases here are the closed forms: a disc (area x density, and ½mr² about its centre), a box (w·h·density, and
//     m(w²+h²)/12 -- which an oriented box uses unchanged, because rotation does not change a rectangle's second
//     moment about its own centre), a capsule (the rectangle plus one disc of area, with the cross term that a naive
//     parallel-axis step drops), and the documented failure behaviour: an area-less shape or an unrecognised kind
//     zeroes the entry rather than faulting, because a sensor is legitimate.
#include <flight/physics2d/mass_properties.hpp>
#include <flight/physics2d/material.hpp>

#include <cmath>
#include <iostream>
#include <limits>
#include <variant>

namespace {

namespace physics = flight::physics2d;
namespace types = flight::types;

using Shape = types::CollisionBuiltInShape2D;
using Circle = flight::types::x_y_radius_kind_90fbb3b4c4569518;
using Capsule = flight::types::x0_y0_x1_y1_radius_kind_768ba062d07f3ff6;
using Point = flight::types::x_y_kind_35ad667397176b0f;
using Segment = flight::types::x0_y0_x1_y1_kind_d4f616534491406f;
using Aabb = flight::types::min_x_min_y_max_x_max_y_kind_329d6080b4f31db4;
using Obb = flight::types::x_y_half_w_half_h_rotation_kind_8ffcea5c657a2268;
using Material = flight::Ref<types::Physics2DMaterial>;
using ColliderRow = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<types::Physics2DCollider>>>>;

int failures = 0;

void check(bool condition, const char* message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

bool close(double value, double expected, double tolerance = 1e-9) {
  return std::abs(value - expected) <= tolerance * std::max(1.0, std::abs(expected));
}

Shape circle(double x, double y, double radius) {
  return Shape{flight::make_ref<Circle>(Circle{.x = x, .y = y, .radius = radius, .kind = flight::String("circle")})};
}
Shape capsule(double x0, double y0, double x1, double y1, double radius) {
  return Shape{flight::make_ref<Capsule>(
      Capsule{.x0 = x0, .y0 = y0, .x1 = x1, .y1 = y1, .radius = radius, .kind = flight::String("capsule")})};
}
Shape point(double x, double y) {
  return Shape{flight::make_ref<Point>(Point{.x = x, .y = y, .kind = flight::String("point")})};
}
Shape box(double min_x, double min_y, double max_x, double max_y) {
  return Shape{flight::make_ref<Aabb>(Aabb{.min_x = min_x, .min_y = min_y, .max_x = max_x, .max_y = max_y,
                                           .kind = flight::String("aabb")})};
}
Shape oriented_box(double half_w, double half_h, double rotation) {
  return Shape{flight::make_ref<Obb>(Obb{.x = 0.0, .y = 0.0, .half_w = half_w, .half_h = half_h, .rotation = rotation,
                                         .kind = flight::String("obb")})};
}

// The mass function reads only `local` and `material.density`, so a collider is built as a row over exactly those.
ColliderRow collider(const Shape& local, double density) {
  return flight::make_structural_ref<flight::RowReadonly<flight::RowOf<flight::Ref<types::Physics2DCollider>>>>(
      flight::row_field<flight::RowKey<"local">>(local),
      flight::row_field<flight::RowKey<"material">>(flight::make_ref<types::Physics2DMaterial>(
          types::Physics2DMaterial{.density = density, .friction = 0.2, .restitution = 0.0})));
}

types::Physics2DMassData masses_of(const Shape& local, double density) {
  const auto out = flight::make_ref<types::Physics2DMassData>(
      types::Physics2DMassData{.mass = 0.0, .inertia = 0.0, .center_x = 0.0, .center_y = 0.0});
  physics::compute_physics2_dcollider_mass_data(collider(local, density), out);
  return *out;
}

// Material mixing: the geometric mean for friction, the bouncier surface for restitution, both symmetric.
void test_material_mixing() {
  check(physics::mix_physics2_dfriction(0.0, 1.0) == 0.0 && physics::mix_physics2_dfriction(1.0, 0.0) == 0.0,
        "either surface at zero friction makes the pair frictionless");
  check(close(physics::mix_physics2_dfriction(0.25, 1.0), physics::mix_physics2_dfriction(1.0, 0.25)),
        "friction mixing is symmetric in the pair");
  check(close(physics::mix_physics2_dfriction(0.36, 0.36), 0.36), "equal coefficients keep their value");
  check(close(physics::mix_physics2_dfriction(4.0, 9.0), 6.0), "and the mean is the geometric one");

  const double widest = std::numeric_limits<double>::max();
  const double mixed = physics::mix_physics2_dfriction(widest, widest);
  check(std::isfinite(mixed) && close(mixed / widest, 1.0),
        "two large finite coefficients do not overflow the mix");

  check(physics::mix_physics2_drestitution(0.2, 0.8) == 0.8 &&
            physics::mix_physics2_drestitution(0.8, 0.2) == 0.8,
        "the bouncier surface decides restitution, whichever side it is on");
  check(physics::mix_physics2_drestitution(0.0, 0.9) == 0.9, "and a dead surface does not damp a live one");
}

// Disc and boxes: derived mass, the closed-form inertia about the collider's own centroid, and the centroid itself.
void test_derived_mass_for_closed_forms() {
  const auto disc = masses_of(circle(0, 0, 2), 3.0);
  check(close(disc.mass, M_PI * 4.0 * 3.0), "a disc's mass is its area times density");
  check(close(disc.inertia, 0.5 * disc.mass * 4.0), "and its inertia is half the mass times r squared");
  check(disc.center_x == 0.0 && disc.center_y == 0.0, "with its centre at the collider's position");

  const auto rectangle = masses_of(box(0, 0, 4, 2), 3.0);
  check(close(rectangle.mass, 8.0 * 3.0), "a box's mass is its area times density");
  check(close(rectangle.inertia, rectangle.mass * (16.0 + 4.0) / 12.0), "and its inertia is m(w^2+h^2)/12");
  check(close(rectangle.center_x, 2.0) && close(rectangle.center_y, 1.0), "with its centre at the box's midpoint");

  // The same rectangle, rotated: the formula is unchanged, which is the module's stated reason an oriented box can
  // use it at all.
  const auto oriented = masses_of(oriented_box(2, 1, M_PI / 3.0), 3.0);
  check(close(oriented.mass, rectangle.mass) && close(oriented.inertia, rectangle.inertia),
        "an oriented box uses the same mass and inertia as the axis-aligned one");
}

// The capsule is a rectangle plus exactly one disc of area, and its inertia carries the cross term that shifting a
// half-disc's own centroid onto the axis introduces.
void test_capsule_inertia_keeps_its_cross_term() {
  const double radius = 1.0;
  const double length = 4.0;
  const double density = 2.0;
  const auto stadium = masses_of(capsule(0, 0, 4, 0, radius), density);

  const double rectangle_mass = 2 * radius * length * density;
  const double disc_mass = M_PI * radius * radius * density;
  check(close(stadium.mass, rectangle_mass + disc_mass),
        "a capsule's mass is its rectangle plus one disc of area");
  const double expected_inertia = (rectangle_mass * (length * length + 4 * radius * radius)) / 12 +
                                  disc_mass * ((radius * radius) / 2 + (length * length) / 4 +
                                               (4 * radius * length) / (3 * M_PI));
  check(close(stadium.inertia, expected_inertia), "and its inertia includes the end-cap cross term");
  check(!close(stadium.inertia,
               (rectangle_mass * (length * length + 4 * radius * radius)) / 12 +
                   disc_mass * ((radius * radius) / 2 + (length * length) / 4),
               1e-3),
        "which the naive rectangle-plus-disc sum would drop");
  check(close(stadium.center_x, 2.0) && close(stadium.center_y, 0.0), "with its centroid at the axis midpoint");
}

// The documented failure behaviour: an area-less shape is a legitimate sensor, so it zeros rather than faulting.
void test_area_less_shapes_carry_no_mass() {
  for (const auto& [name, shape] : std::initializer_list<std::pair<const char*, Shape>>{
           {"a point", point(3, 4)}, {"a segment", Shape{flight::make_ref<Segment>(
                                                          Segment{.x0 = 0, .y0 = 0, .x1 = 5, .y1 = 0,
                                                                  .kind = flight::String("segment")})}}}) {
    const auto data = masses_of(shape, 7.0);
    const bool zeroed = data.mass == 0.0 && data.inertia == 0.0 && data.center_x == 0.0 && data.center_y == 0.0;
    if (!zeroed) {
      std::cerr << "FAIL: " << name << " has no area, so it must carry no mass\n";
      ++failures;
    }
  }
  check(masses_of(circle(0, 0, 2), 7.0).mass > 0.0, "while a shape with area carries mass at the same density");
}

}  // namespace

int main() {
  test_material_mixing();
  test_derived_mass_for_closed_forms();
  test_capsule_inertia_keeps_its_cross_term();
  test_area_less_shapes_carry_no_mass();
  if (failures == 0) std::cout << "generated physics2d: all checks passed\n";
  return failures == 0 ? 0 : 1;
}
