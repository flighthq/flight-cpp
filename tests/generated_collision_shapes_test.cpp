// Native parity test for the reachable @flighthq/collision surface at a627849, beyond the sweep regression.
//
// The package already has a native test -- tests/generated_collision_test.cpp drives the public SweepCircle2D
// regression -- so this file covers two emitting modules that had none, and no others: contact identity and
// closed-form point containment. The package is 32 of 38 modules emitted with 21 refused, so most of its surface
// (the manifolds, the GJK and SAT paths, the shape-collision pairs, the 3D hulls) is behind refusals rather than
// behind a missing test; see docs/parity/anim.md.
//
//   * packContactFeatureId, from contactFeatureId.test.ts. A contact's identity is what lets a solver recognise,
//     next frame, that it is looking at the same contact; the regression the module was written for is a packing
//     whose fields overflowed into one another, so two unrelated face pairs shared an id. The cases ported here
//     are identity itself: the aliased pair stays distinct, every distinct feature across each field's boundary
//     has its own id, the widest id is an exact integer, and the two bits that ride on top distinguish their
//     features. The upstream file's first assertion is about the OLD shift packing as a JavaScript expression and
//     is not portable; the value it protects is the pairwise inequality asserted below it, which is ported.
//   * getCollisionShapeContainsPoint2D, from pointContainment2D.test.ts: the capsule tested as a segment plus its
//     two round caps (including the diagonal just off an end, which a rectangle-plus-discs reading gets wrong),
//     boundary-inclusive circle, axis-aligned box and rotated oriented box, a convex polygon inside and out, a
//     polygon with fewer than three vertices rejected, a tiny polygon staying proportional, a segment, a point
//     collider, and a zero-radius circle and zero-length segment behaving as points. The upstream case that casts
//     an unknown kind is not ported: the emitted signature takes the shape as a closed variant, so an unlisted
//     kind cannot be constructed to ask about.
#include <flight/collision/contact_feature_id.hpp>
#include <flight/collision/point_containment2_d.hpp>

#include <cmath>
#include <iostream>
#include <variant>

namespace {

namespace collision = flight::collision;
namespace types = flight::types;

using Shape = types::CollisionBuiltInShape2D;
using Circle = flight::types::x_y_radius_kind_90fbb3b4c4569518;
using Capsule = flight::types::x0_y0_x1_y1_radius_kind_768ba062d07f3ff6;
using Point = flight::types::x_y_kind_35ad667397176b0f;
using Polygon = flight::types::points_kind_9b00ebdad27cdb4e;
using Segment = flight::types::x0_y0_x1_y1_kind_d4f616534491406f;
using Aabb = flight::types::min_x_min_y_max_x_max_y_kind_329d6080b4f31db4;
using Obb = flight::types::x_y_half_w_half_h_rotation_kind_8ffcea5c657a2268;

int failures = 0;

void check(bool condition, const char* message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

double pack(bool reference_is_a, double reference_edge, double incident_edge, bool second_point) {
  return collision::pack_contact_feature_id(reference_is_a, reference_edge, incident_edge, second_point);
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
Shape polygon(flight::Array<double> points) {
  return Shape{
      flight::make_ref<Polygon>(Polygon{.points = std::move(points), .kind = flight::String("polygon")})};
}
Shape segment(double x0, double y0, double x1, double y1) {
  return Shape{flight::make_ref<Segment>(
      Segment{.x0 = x0, .y0 = y0, .x1 = x1, .y1 = y1, .kind = flight::String("segment")})};
}
Shape aabb(double min_x, double min_y, double max_x, double max_y) {
  return Shape{flight::make_ref<Aabb>(Aabb{.min_x = min_x, .min_y = min_y, .max_x = max_x, .max_y = max_y,
                                           .kind = flight::String("aabb")})};
}
Shape obb(double x, double y, double half_w, double half_h, double rotation) {
  return Shape{flight::make_ref<Obb>(Obb{.x = x, .y = y, .half_w = half_w, .half_h = half_h, .rotation = rotation,
                                         .kind = flight::String("obb")})};
}

bool contains(const Shape& shape, double x, double y) {
  return collision::get_collision_shape_contains_point2_d(shape, x, y);
}

// Contact identity: one id per feature, and no field aliasing into another.
void test_contact_feature_identity() {
  check(pack(true, 0.0, 1024.0, false) != pack(true, 1.0, 0.0, false),
        "the face pair the old shift packing aliased stays distinct");
  check(pack(true, 3.0, 7.0, false) != pack(true, 3.0, 7.0, true),
        "the two contact points of one face pair are different features");
  check(pack(false, 3.0, 7.0, false) != pack(true, 3.0, 7.0, false),
        "which shape owned the reference face is part of the identity");
  check(collision::feature_index_limit == 33554432.0,
        "the face-index limit is the authored 2^25, the value the scales are built from");

  // Every field swept across its own boundary and the one below it, crossed with every other, must collide
  // nowhere -- this is the property the module exists for, not sixteen separate cases.
  const double edges[] = {0.0, 1.0, 2.0, 1023.0, 1024.0, 1025.0, 2047.0, 2048.0, collision::feature_index_limit - 1.0};
  flight::Map<double, double> seen;
  int collisions = 0;
  for (bool reference_is_a : {false, true}) {
    for (double reference_edge : edges) {
      for (double incident_edge : edges) {
        for (bool second_point : {false, true}) {
          const double id = pack(reference_is_a, reference_edge, incident_edge, second_point);
          if (seen.has(id)) ++collisions;
          seen.set(id, id);
        }
      }
    }
  }
  check(collisions == 0, "no two distinct features share an id across the packing boundaries");
  check(seen.size() == 2.0 * 9.0 * 9.0 * 2.0, "and every feature in the sweep has its own");

  const double widest = pack(true, collision::feature_index_limit - 1.0, collision::feature_index_limit - 1.0, true);
  check(widest + 1.0 != widest, "the widest id is an exact integer, so nothing rounds two features together");
  check(widest < 9007199254740992.0, "and it stays inside the exact-integer range");
}

// Capsules: the body, both rounded ends, and the diagonal a rectangle-plus-discs reading would admit.
void test_capsule_containment() {
  const Shape shape = capsule(0, 0, 4, 0, 1);
  check(contains(shape, 2, 0.9), "the capsule body contains a point within its radius");
  check(!contains(shape, 2, 1.1), "and not one outside it");
  check(contains(shape, -0.9, 0), "the round end at the start contains a point inside its cap");
  check(!contains(shape, -1.1, 0), "and not one outside it");
  check(contains(shape, 4.9, 0), "the round end at the finish contains a point inside its cap");
  check(!contains(shape, 5.1, 0), "and not one outside it");
  check(!contains(shape, 4.8, 0.8), "diagonally off the end is outside the cap, though a bounding box admits it");
  check(contains(capsule(0, 0, 2, 0, 1), 1, 1), "the capsule surface itself counts as contained");
}

// Closed-form kinds: boundary-inclusive, and answered in the shape's own frame.
void test_closed_form_containment() {
  const Shape circle_shape = circle(0, 0, 5);
  check(contains(circle_shape, 1, 1) && contains(circle_shape, 5, 0), "a circle contains inside and on its boundary");
  check(!contains(circle_shape, 6, 0), "and not outside it");

  const Shape box = aabb(0, 0, 10, 10);
  // Both corners, so each of the four bounds decides a case: (0,10) rides the y bound and (10,0) the x bound.
  check(contains(box, 5, 5) && contains(box, 0, 10) && contains(box, 10, 0),
        "a box contains inside and on either corner");
  check(!contains(box, -1, 5) && !contains(box, 11, 5) && !contains(box, 5, -1) && !contains(box, 5, 11),
        "and not past any of its edges");

  // A 45-degree box reaches ~2.83 along its diagonal but only 2 across its flats.
  const Shape rotated = obb(0, 0, 2, 2, 3.14159265358979323846 / 4.0);
  check(contains(rotated, 0, 0) && contains(rotated, 2.8, 0), "a rotated box contains toward its corner");
  check(!contains(rotated, 2.1, 2.1), "and not just past a flat edge");
}

// Polygons: convex containment, degenerate input rejected, and proportionality at a tiny scale.
void test_polygon_containment() {
  const Shape hexagon = polygon(flight::Array<double>{2.0, 0.0, 1.0, 2.0, -1.0, 2.0, -2.0, 0.0, -1.0, -2.0, 1.0, -2.0});
  check(contains(hexagon, 0, 0) && contains(hexagon, 1.9, 0), "a convex hexagon contains its interior");
  check(!contains(hexagon, 3, 0) && !contains(hexagon, 0, 3), "and excludes points beyond it");

  check(!contains(polygon(flight::Array<double>{0.0, 0.0, 1.0, 1.0}), 0.5, 0.5),
        "a polygon with fewer than three vertices contains nothing");

  const Shape tiny = polygon(flight::Array<double>{0.0, 0.0, 1e-6, 0.0, 1e-6, 1e-6, 0.0, 1e-6});
  check(contains(tiny, 0.5e-6, 0.5e-6) && !contains(tiny, 2e-6, 0.5e-6),
        "containment stays proportional for a tiny polygon");
}

// The area-less kinds degrade to an on-shape test.
void test_degenerate_kinds() {
  const Shape line = segment(0, 0, 10, 0);
  check(contains(line, 5, 0) && !contains(line, 5, 0.1) && !contains(line, 11, 0),
        "a segment contains the points on it and nothing beside or past it");

  const Shape dot = point(3, 4);
  check(contains(dot, 3, 4) && !contains(dot, 3, 5), "a point collider contains only its own position");

  check(contains(circle(3, 4, 0), 3, 4) && !contains(circle(3, 4, 0), 3, 4.001),
        "a zero-radius circle behaves as a point");
  check(contains(segment(3, 4, 3, 4), 3, 4) && !contains(segment(3, 4, 3, 4), 3, 4.001),
        "and so does a zero-length segment");
}

}  // namespace

int main() {
  test_contact_feature_identity();
  test_capsule_containment();
  test_closed_form_containment();
  test_polygon_containment();
  test_degenerate_kinds();
  if (failures == 0) std::cout << "generated collision shapes: all checks passed\n";
  return failures == 0 ? 0 : 1;
}
