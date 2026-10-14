// Focused native parity for @flighthq/collision. The current emitter now supplies the complete
// pointContainment3D module, so these checks retire its stale override while pinning exact shape
// owners, point argument order, closed-form inclusive boundaries, convex scratch reuse, and
// hull-before-probe GJK dispatch. Measured public sweep contracts extend the package row without
// claiming that this one executable ports the entire upstream corpus.
//
// MUTATION SENSITIVITY, measured 2026-10-14 against 14 isolated one-at-a-time edits to the
// generated header: all anchors matched exactly once (the argument swap validated each of its two
// anchors once), all builds succeeded, and every mutant was caught. Sphere/AABB/box boundary,
// box conjugate, capsule clamp/degenerate, cylinder cap/degenerate, cone taper/degenerate,
// probe-coordinate, GJK argument-order, and empty-hull mutants failed their focused assertions.
// Replacing the convex point-array binding failed its identity assertion before a later abort. Every
// generated edit was restored after measurement.
//
// The full-package re-audit retained that floor and compiled seven additional isolated shadow-header
// mutants without editing generated or override output. Convex kind dispatch, inclusive sphere
// boundary, latest-array scratch rebinding, degenerate sweep validation, shape-A contact-coordinate
// sign, reversed-argument normal, and failure-output clearing mutations produced 4/1/1/1/2/1/2
// focused assertion failures, respectively; every mutant was discarded after its run.
#include <flight/collision/point_containment3_d.hpp>
#include <flight/collision/sweep_collision_shape2_d.hpp>

#include <cmath>
#include <iostream>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace {

namespace collision = flight::collision;
namespace types = flight::types;

using Aabb3D = types::min_x_min_y_min_z_max_x_max_y_max_z_kind_748c675e8e0cf88a;
using Box3D = types::x_y_z_half_x_half_y_half_z_rotation_x_rotation_y_rotation_z_rotation_w_kind_bdd50a8e154d5a6a;
using Capsule3D = types::x0_y0_z0_x1_y1_z1_radius_kind_3ca9ada7b527e64b;
using Cone3D = types::apex_x_apex_y_apex_z_base_x_base_y_base_z_radius_kind_21c650ad310bf539;
using Convex3D = types::points_kind_35a8f5f2a3a8bd66;
using Cylinder3D = types::x0_y0_z0_x1_y1_z1_radius_kind_aef60734fa17cff8;
using Point2D = types::x_y_kind_35ad667397176b0f;
using Sphere3D = types::x_y_z_radius_kind_b0bc53ca3f0e8006;

int failures = 0;

void check(bool condition, const std::string& message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

template <typename Value>
types::CollisionBuiltInShape3D built_in_shape(Value value) {
  using Ref = flight::Ref<Value>;
  return types::CollisionBuiltInShape3D{
      std::in_place_type<Ref>,
      flight::make_ref<Value>(std::move(value)),
  };
}

template <typename Value>
types::CollisionBuiltInShape2D built_in_shape2_d(Value value) {
  using Ref = flight::Ref<Value>;
  return types::CollisionBuiltInShape2D{
      std::in_place_type<Ref>,
      flight::make_ref<Value>(std::move(value)),
  };
}

void check_point_containment3_d() {
  const auto sphere = built_in_shape(Sphere3D{
      .x = 1.0,
      .y = 2.0,
      .z = 3.0,
      .radius = 2.0,
      .kind = flight::String("sphere"),
  });
  check(collision::get_collision_shape_contains_point3_d(sphere, 1.0, 2.0, 3.0),
        "sphere contains its center with ordered point coordinates");
  check(collision::get_collision_shape_contains_point3_d(sphere, 1.0, 2.0, 5.0),
        "sphere includes its surface");
  check(!collision::get_collision_shape_contains_point3_d(sphere, 1.0, 2.0, 5.01),
        "sphere excludes a point outside on Z");

  const auto aabb = built_in_shape(Aabb3D{
      .min_x = -1.0,
      .min_y = -2.0,
      .min_z = -3.0,
      .max_x = 1.0,
      .max_y = 2.0,
      .max_z = 3.0,
      .kind = flight::String("aabb"),
  });
  check(collision::get_collision_shape_contains_point3_d(aabb, 0.0, 0.0, 0.0),
        "AABB contains an interior point");
  check(collision::get_collision_shape_contains_point3_d(aabb, 1.0, 2.0, 3.0),
        "AABB includes its max corner boundary");
  check(!collision::get_collision_shape_contains_point3_d(aabb, 0.0, 0.0, -3.01),
        "AABB excludes a point beyond its min Z face");

  constexpr double eighth_turn_sine = 0.3826834323650897717;
  constexpr double eighth_turn_cosine = 0.9238795325112867561;
  const auto box = built_in_shape(Box3D{
      .x = 0.0,
      .y = 0.0,
      .z = 0.0,
      .half_x = 2.0,
      .half_y = 0.5,
      .half_z = 0.5,
      .rotation_x = 0.0,
      .rotation_y = eighth_turn_sine,
      .rotation_z = 0.0,
      .rotation_w = eighth_turn_cosine,
      .kind = flight::String("box"),
  });
  check(collision::get_collision_shape_contains_point3_d(box, 1.2, 0.0, -1.2),
        "oriented box includes a point on its rotated long axis");
  check(!collision::get_collision_shape_contains_point3_d(box, 1.2, 0.0, 1.2),
        "oriented box uses the conjugate rather than the forward rotation");
  const auto identity_box = built_in_shape(Box3D{
      .x = 0.0,
      .y = 0.0,
      .z = 0.0,
      .half_x = 1.0,
      .half_y = 1.0,
      .half_z = 1.0,
      .rotation_x = 0.0,
      .rotation_y = 0.0,
      .rotation_z = 0.0,
      .rotation_w = 1.0,
      .kind = flight::String("box"),
  });
  check(collision::get_collision_shape_contains_point3_d(
            identity_box, 1.0, 0.0, 0.0),
        "box includes its face boundary");
  check(!collision::get_collision_shape_contains_point3_d(
            identity_box, 1.01, 0.0, 0.0),
        "box excludes a point beyond its face");

  const auto capsule = built_in_shape(Capsule3D{
      .x0 = -2.0,
      .y0 = 0.0,
      .z0 = 0.0,
      .x1 = 2.0,
      .y1 = 0.0,
      .z1 = 0.0,
      .radius = 0.5,
      .kind = flight::String("capsule"),
  });
  check(collision::get_collision_shape_contains_point3_d(capsule, 0.0, 0.5, 0.0),
        "capsule includes its cylindrical side boundary");
  check(collision::get_collision_shape_contains_point3_d(capsule, 2.5, 0.0, 0.0),
        "capsule includes its rounded cap boundary");
  check(!collision::get_collision_shape_contains_point3_d(capsule, 2.51, 0.0, 0.0),
        "capsule excludes a point beyond its rounded cap");
  const auto degenerate_capsule = built_in_shape(Capsule3D{
      .x0 = 1.0,
      .y0 = 1.0,
      .z0 = 1.0,
      .x1 = 1.0,
      .y1 = 1.0,
      .z1 = 1.0,
      .radius = 1.0,
      .kind = flight::String("capsule"),
  });
  check(collision::get_collision_shape_contains_point3_d(
            degenerate_capsule, 1.0, 2.0, 1.0),
        "zero-length capsule remains a boundary-inclusive sphere");
  check(!collision::get_collision_shape_contains_point3_d(
            degenerate_capsule, 1.0, 2.01, 1.0),
        "zero-length capsule excludes a point beyond its radius");

  const auto cylinder = built_in_shape(Cylinder3D{
      .x0 = 0.0,
      .y0 = -2.0,
      .z0 = 0.0,
      .x1 = 0.0,
      .y1 = 2.0,
      .z1 = 0.0,
      .radius = 1.0,
      .kind = flight::String("cylinder"),
  });
  check(collision::get_collision_shape_contains_point3_d(cylinder, 1.0, 2.0, 0.0),
        "cylinder includes its rim and flat-cap boundary");
  check(!collision::get_collision_shape_contains_point3_d(cylinder, 0.99, 2.01, 0.0),
        "cylinder excludes the rounded-corner region beyond its cap");
  check(!collision::get_collision_shape_contains_point3_d(cylinder, 1.01, 0.0, 0.0),
        "cylinder excludes a point beyond its radial boundary");
  const auto degenerate_cylinder = built_in_shape(Cylinder3D{
      .x0 = 1.0,
      .y0 = 1.0,
      .z0 = 1.0,
      .x1 = 1.0,
      .y1 = 1.0,
      .z1 = 1.0,
      .radius = 1.0,
      .kind = flight::String("cylinder"),
  });
  check(!collision::get_collision_shape_contains_point3_d(
            degenerate_cylinder, 1.0, 1.0, 1.0),
        "zero-length cylinder is rejected rather than rounded like a capsule");

  const auto cone = built_in_shape(Cone3D{
      .apex_x = 0.0,
      .apex_y = 3.0,
      .apex_z = 0.0,
      .base_x = 0.0,
      .base_y = 0.0,
      .base_z = 0.0,
      .radius = 1.5,
      .kind = flight::String("cone"),
  });
  check(collision::get_collision_shape_contains_point3_d(cone, 0.75, 1.5, 0.0),
        "cone includes its tapered side boundary");
  check(!collision::get_collision_shape_contains_point3_d(cone, 0.8, 1.5, 0.0),
        "cone applies the interpolated radius rather than the base radius");
  check(collision::get_collision_shape_contains_point3_d(cone, 0.0, 3.0, 0.0),
        "cone includes its apex boundary");
  check(!collision::get_collision_shape_contains_point3_d(cone, 0.0, 3.01, 0.0),
        "cone excludes a point beyond its apex");
  const auto degenerate_cone = built_in_shape(Cone3D{
      .apex_x = 1.0,
      .apex_y = 1.0,
      .apex_z = 1.0,
      .base_x = 1.0,
      .base_y = 1.0,
      .base_z = 1.0,
      .radius = 1.0,
      .kind = flight::String("cone"),
  });
  check(!collision::get_collision_shape_contains_point3_d(
            degenerate_cone, 1.0, 1.0, 1.0),
        "zero-length cone is rejected before radius interpolation");

  const auto scratch_hull_owner = collision::scratch_hull;
  const auto scratch_probe_owner = collision::scratch_probe;
  const flight::Array<double> identity_points{
      0.0, 0.0, 0.0,
      1.0, 0.0, 0.0,
      0.0, 1.0, 0.0,
  };
  std::vector<int> support_order;
  bool exact_support_owners = true;
  collision::clear_collision_supports3_d();
  collision::register_collision_support3_d(
      flight::String("convex"),
      [&](types::CollisionShape3D shape, double, double, double,
          flight::Array<double> out) {
        support_order.push_back(1);
        exact_support_owners = exact_support_owners &&
            std::get<flight::Ref<Convex3D>>(shape) == scratch_hull_owner;
        out.element(0.0) = 0.0;
        out.element(1.0) = 0.0;
        out.element(2.0) = 0.0;
      });
  collision::register_collision_support3_d(
      flight::String("sphere"),
      [&](types::CollisionShape3D shape, double, double, double,
          flight::Array<double> out) {
        support_order.push_back(2);
        exact_support_owners = exact_support_owners &&
            std::get<flight::Ref<Sphere3D>>(shape) == scratch_probe_owner;
        out.element(0.0) = 0.0;
        out.element(1.0) = 0.0;
        out.element(2.0) = 0.0;
      });
  static_cast<void>(collision::is_point_in_convex_hull3_d(
      identity_points, 4.0, 5.0, 6.0));
  check(collision::scratch_hull == scratch_hull_owner &&
            collision::scratch_probe == scratch_probe_owner,
        "convex queries retain both scratch owners");
  check(collision::scratch_hull->points.identity() == identity_points.identity(),
        "convex scratch rebinds to the caller point-array owner");
  check(collision::scratch_probe->x == 4.0 &&
            collision::scratch_probe->y == 5.0 &&
            collision::scratch_probe->z == 6.0 &&
            collision::scratch_probe->radius == 0.0,
        "convex scratch writes ordered point coordinates into a zero-radius probe");
  check(exact_support_owners && support_order.size() >= 2U &&
            support_order[0] == 1 && support_order[1] == 2,
        "convex GJK receives the exact hull first and exact point probe second");

  collision::clear_collision_supports3_d();
  collision::register_built_in_collision_supports3_d();
  const flight::Array<double> cube_points{
      -1.0, -1.0, -1.0, 1.0, -1.0, -1.0,
      1.0, 1.0, -1.0, -1.0, 1.0, -1.0,
      -1.0, -1.0, 1.0, 1.0, -1.0, 1.0,
      1.0, 1.0, 1.0, -1.0, 1.0, 1.0,
  };
  const auto convex = built_in_shape(Convex3D{
      .points = cube_points,
      .kind = flight::String("convex"),
  });
  check(collision::get_collision_shape_contains_point3_d(convex, 0.0, 0.0, 0.0),
        "convex GJK contains an interior point");
  check(!collision::get_collision_shape_contains_point3_d(convex, 1.1, 0.0, 0.0),
        "convex GJK excludes an exterior point");
  const flight::Array<double> tetra_points{
      0.0, 0.0, 0.0,
      1.0, 0.0, 0.0,
      0.0, 1.0, 0.0,
      0.0, 0.0, 1.0,
  };
  const auto tetra = built_in_shape(Convex3D{
      .points = tetra_points,
      .kind = flight::String("convex"),
  });
  check(collision::get_collision_shape_contains_point3_d(tetra, 0.1, 0.1, 0.1),
        "convex GJK contains a point inside a tetrahedron");
  check(!collision::get_collision_shape_contains_point3_d(tetra, 0.9, 0.9, 0.9),
        "convex GJK rejects a tetrahedron bounding-box corner");
  const auto empty_convex = built_in_shape(Convex3D{
      .points = flight::Array<double>{},
      .kind = flight::String("convex"),
  });
  check(!collision::get_collision_shape_contains_point3_d(
            empty_convex, 0.0, 0.0, 0.0),
        "empty convex hull returns false before GJK");

  const flight::Array<double> large_tetra_points{
      0.0, 0.0, 0.0,
      10.0, 0.0, 0.0,
      0.0, 10.0, 0.0,
      0.0, 0.0, 10.0,
  };
  const auto large_tetra = built_in_shape(Convex3D{
      .points = large_tetra_points,
      .kind = flight::String("convex"),
  });
  check(collision::get_collision_shape_contains_point3_d(large_tetra, 2.0, 2.0, 2.0),
        "first convex query reads the caller's larger hull");
  check(!collision::get_collision_shape_contains_point3_d(tetra, 2.0, 2.0, 2.0),
        "second convex query does not retain the first caller hull");
  check(collision::scratch_hull->points.identity() == tetra_points.identity(),
        "convex scratch rebinds to the latest caller point-array owner");
}

void check_sweep_collision_shape2_d() {
  using collision::SweepAabb2D;
  using collision::SweepCircle2D;

  auto out = collision::create_collision_time_of_impact2_d();
  check(out->fraction == 0.0 && out->x == 0.0 && out->y == 0.0 &&
            out->normal_x == 0.0 && out->normal_y == 0.0,
        "sweep output starts in the reusable miss state");

  const auto moving_circle = built_in_shape2_d(SweepCircle2D{
      .x = 0.0,
      .y = 0.0,
      .radius = 1.0,
      .kind = flight::String("circle"),
  });
  const auto fixed_circle = built_in_shape2_d(SweepCircle2D{
      .x = 5.0,
      .y = 0.0,
      .radius = 1.0,
      .kind = flight::String("circle"),
  });
  check(!collision::sweep_collision_shape2_d(
            moving_circle, 0.0, 0.0, fixed_circle, 0.0, 0.0, out),
        "separated stationary circles have no time of impact");
  check(collision::sweep_collision_shape2_d(
            moving_circle, 10.0, 0.0, fixed_circle, 0.0, 0.0, out),
        "public sweep dispatch reaches the exact circle solver");
  check(std::abs(out->fraction - 0.3) <= 1e-12 && out->normal_x == -1.0 &&
            out->normal_y == 0.0 && out->x == 4.0 && out->y == 0.0,
        "circle sweep preserves fraction, normal, and shape-A contact coordinates");

  out->fraction = 9.0;
  out->x = 9.0;
  out->y = 9.0;
  out->normal_x = 9.0;
  out->normal_y = 9.0;
  check(!collision::sweep_collision_shape2_d(
            moving_circle, 10.0, 0.0, fixed_circle, 0.0, 0.0, out, 0.2),
        "sweep honours maxFraction");
  check(out->fraction == 0.0 && out->x == 0.0 && out->y == 0.0 &&
            out->normal_x == 0.0 && out->normal_y == 0.0,
        "a maxFraction miss clears reused sweep output");

  const auto fixed_box = built_in_shape2_d(SweepAabb2D{
      .min_x = 5.0,
      .min_y = 5.0,
      .max_x = 7.0,
      .max_y = 7.0,
      .kind = flight::String("aabb"),
  });
  const auto face_circle = built_in_shape2_d(SweepCircle2D{
      .x = 0.0,
      .y = 6.0,
      .radius = 1.0,
      .kind = flight::String("circle"),
  });
  check(collision::sweep_collision_shape2_d(
            face_circle, 10.0, 0.0, fixed_box, 0.0, 0.0, out),
        "circle-to-box sweep reaches polygon-face dispatch");
  check(std::abs(out->fraction - 0.4) <= 1e-12 && out->normal_x == -1.0 &&
            out->normal_y == 0.0 && out->x == 5.0 && out->y == 6.0,
        "circle-to-box sweep writes the exact face hit on shape A");
  check(collision::sweep_collision_shape2_d(
            fixed_box, 0.0, 0.0, face_circle, 10.0, 0.0, out),
        "reversed box-to-circle sweep remains reachable");
  check(std::abs(out->fraction - 0.4) <= 1e-12 && out->normal_x == 1.0 &&
            out->normal_y == 0.0 && out->x == 5.0 && out->y == 6.0,
        "reversed sweep flips the normal while keeping the hit on shape A");

  const auto overlapping_circle = built_in_shape2_d(SweepCircle2D{
      .x = 1.0,
      .y = 0.0,
      .radius = 1.0,
      .kind = flight::String("circle"),
  });
  check(collision::sweep_collision_shape2_d(
            moving_circle, 1.0, 0.0, overlapping_circle, 0.0, 0.0, out),
        "an initially overlapping pair reports a zero-fraction hit");
  check(out->fraction == 0.0 && out->normal_x == -1.0,
        "initial-overlap sweep keeps the ordered shape-A normal");
  const auto touching_circle = built_in_shape2_d(SweepCircle2D{
      .x = 2.0,
      .y = 0.0,
      .radius = 1.0,
      .kind = flight::String("circle"),
  });
  check(!collision::sweep_collision_shape2_d(
            moving_circle, -1.0, 0.0, touching_circle, 0.0, 0.0, out),
        "a touching pair moving apart has no future impact");

  const auto unsupported_point = built_in_shape2_d(Point2D{
      .x = 0.0,
      .y = 0.0,
      .kind = flight::String("point"),
  });
  out->fraction = 9.0;
  out->x = 9.0;
  out->normal_x = 9.0;
  check(!collision::sweep_collision_shape2_d(
            unsupported_point, 1.0, 0.0, fixed_circle, 0.0, 0.0, out),
        "unsupported area-less sweep input fails closed");
  check(out->fraction == 0.0 && out->x == 0.0 && out->normal_x == 0.0,
        "unsupported input clears reused sweep output");
  const auto degenerate_circle = built_in_shape2_d(SweepCircle2D{
      .x = 0.0,
      .y = 0.0,
      .radius = 0.0,
      .kind = flight::String("circle"),
  });
  check(!collision::sweep_collision_shape2_d(
            degenerate_circle, 10.0, 0.0, fixed_circle, 0.0, 0.0, out),
        "zero-radius sweep input is rejected as degenerate");
  check(!collision::sweep_collision_shape2_d(
            moving_circle,
            std::numeric_limits<double>::quiet_NaN(),
            0.0,
            fixed_circle,
            0.0,
            0.0,
            out),
        "non-finite sweep motion fails closed");
}

} // namespace

int main() {
  check_point_containment3_d();
  check_sweep_collision_shape2_d();
  return failures == 0 ? 0 : 1;
}
