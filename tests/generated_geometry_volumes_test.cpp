#include <flight/geometry/aabb.hpp>
#include <flight/geometry/bounding_sphere.hpp>
#include <flight/geometry/capsule.hpp>
#include <flight/geometry/frustum.hpp>
#include <flight/geometry/matrix4.hpp>
#include <flight/geometry/obb.hpp>
#include <flight/geometry/plane.hpp>
#include <flight/geometry/ray3d.hpp>
#include <flight/geometry/enable_geometry_pool_guards.hpp>
#include <flight/geometry/geometry_pool_guards.hpp>
#include <flight/geometry/matrix3.hpp>
#include <flight/geometry/matrix3_pool.hpp>
#include <flight/geometry/matrix4_pool.hpp>
#include <flight/geometry/matrix_pool.hpp>
#include <flight/geometry/quaternion_pool.hpp>
#include <flight/geometry/rectangle_pool.hpp>
#include <flight/geometry/vector2_pool.hpp>
#include <flight/geometry/vector3.hpp>
#include <flight/geometry/vector3_pool.hpp>
#include <flight/geometry/vector4_pool.hpp>
#include <flight/log/log.hpp>

#include <array>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <limits>
#include <set>
#include <string>
#include <type_traits>

// Native port of the geometry volume, pool and pool-guard cases at a627849 (aabb, boundingSphere, capsule,
// frustum, obb, plane, ray3d, the *Pool tests, enableGeometryPoolGuards, geometryPoolGuards) against the
// generated headers. geometry has no override. Each check names the upstream case it ports.
//
// Substitutions and exclusions:
// Substitution (aabb, a native defect kept visible rather than hidden):
// - Every aabb writer declares `out: AabbLike` (flight/geometry/aabb.hpp: copy_aabb, expand_aabb_by_point,
//   expand_aabb_by_sphere, intersect_aabb, set_aabb, set_aabb_from_points, transform_aabb_by_matrix4,
//   union_aabb). It emits as flight::Ref<types::AabbLike>, while createAabb returns
//   flight::Ref<types::Aabb>, and the two are unrelated owners: passing one is
//   "no viable conversion from 'Ref<flight::types::Aabb>' to 'Ref<flight::types::AabbLike>'". So
//   upstream's `const out = createAabb()` and `writer(a, a, ...)` cannot be written natively.
// - Those cases use an AabbLike carrying fresh min/max vectors, exactly what createAabb fills (see
//   aabb_like). The function under test and the oracle are unchanged.
// - Readers that take a createAabb result go through the readonly conversion emitted consumers use.
// - The same mismatch breaks emitted code: scene3d/scene_node_culling.hpp passes its Ref<Aabb> scratch
//   to transform_aabb_by_matrix4.
//
// Established by compilation (static_assert), not at run time: the seven `typeof initializeX ===
// 'function'` cases (aabb, boundingSphere, capsule, frustum, obb, plane, ray3d).
//
// Excluded (11 cases):
// - obb "remains correct when reading an input reenters another OBB query". It installs a JavaScript
//   accessor with Object.defineProperty on the orientationY field. types::Obb's fields are plain
//   doubles, so there is no getter to reenter from, and the case has no native form.
// - "detaches the previous owner binding before the <value> is reused", in all eight pool test files.
//   It needs entity::attach_entity_binding(flight::Ref<types::Entity>, ...) on a pooled value, and no
//   geometry owner converts to Ref<Entity>: "no viable conversion from 'Ref<flight::types::Vector3>' to
//   'Ref<flight::types::Entity>'" (flight/entity/binding.hpp:20). Writing entity_runtime_key by hand
//   would fabricate the binding.
// - enableGeometryPoolGuards "warns once for a double release from every geometry pool and stays silent
//   for valid brackets" and disableGeometryPoolGuards "restores the production-default silent release
//   path". Both capture through createMemoryLogSink. At this base the log override builds the sink
//   handle with the initialize_log_entity stub (overrides/include/flight/log/log.hpp:970), which throws
//   "flight::log::initialize_log_entity was not generated for this profile" before a sink exists. The
//   override records this as partial.

namespace {

namespace g = flight::geometry;
namespace types = flight::types;

int failures = 0;

void check(bool condition, const std::string& message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

// vitest's toBe: Object.is, so -0 is not 0 and NaN is NaN.
bool same(double actual, double expected) {
  if (std::isnan(actual) || std::isnan(expected)) return std::isnan(actual) && std::isnan(expected);
  return actual == expected && std::signbit(actual) == std::signbit(expected);
}

// vitest's toBeCloseTo(expected, digits): |actual - expected| < 10^-digits / 2.
bool close(double actual, double expected, int digits = 2) {
  if (actual == expected) return true;
  return std::abs(actual - expected) < std::pow(10.0, -digits) / 2.0;
}

const double infinity = std::numeric_limits<double>::infinity();

// expect(typeof initializeX).toBe('function'): an initializer is a declared function.
#define FLIGHT_EXPECT_FUNCTION(name) static_assert(std::is_function_v<std::remove_reference_t<decltype(name)>>, #name " is a function")

using Vec3 = flight::Ref<types::Vector3>;
using Aabb = flight::Ref<types::Aabb>;
using AabbLikeRef = flight::Ref<types::AabbLike>;
using AabbRead = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<types::AabbLike>>>>;

Vec3 v3(double x = 0, double y = 0, double z = 0) { return g::create_vector3(x, y, z); }

// A createAabb result read as the AabbLike the readers declare, spelled as emitted consumers spell it
// (scene3d/scene_node_culling.hpp, scene3d/instanced_mesh.hpp).
AabbRead read(const Aabb& aabb) {
  return flight::structural_ref_cast<AabbRead>(flight::StructuralRef<flight::RowWritable<flight::RowOf<Aabb>>>(aabb));
}

// The writers declare `out: AabbLike`, and a createAabb result (Aabb) does not convert to that carrier
// (see the substitution note above). This builds the AabbLike the writers accept with exactly what
// createAabb fills: fresh min/max vectors, defaulting to the empty box.
AabbLikeRef aabb_like(double min_x = infinity, double min_y = infinity, double min_z = infinity, double max_x = -infinity,
                      double max_y = -infinity, double max_z = -infinity) {
  return flight::make_ref<types::AabbLike>(types::AabbLike{.max = v3(max_x, max_y, max_z), .min = v3(min_x, min_y, min_z)});
}

bool xyz(const Vec3& v, double x, double y, double z) { return same(v->x, x) && same(v->y, y) && same(v->z, z); }

flight::Ref<types::Matrix4> translation(double x, double y, double z) {
  auto m = g::create_matrix4();
  g::set_matrix4_position(m, v3(x, y, z));
  return m;
}

void test_aabb() {
  {
    const auto a = g::create_aabb(-1, -2, -3, 4, 5, 6);
    const auto c = g::clone_aabb(read(a));
    bool ok = c != a && c->min != a->min && same(c->min->x, -1) && same(c->max->z, 6);
    c->min->x = 99;
    check(ok && same(a->min->x, -1), "cloneAabb creates an independent copy with independent corner vectors");
  }
  {
    const auto a = g::create_aabb(0, 0, 0, 2, 2, 2);
    check(g::contains_aabb_point(read(a), v3(1, 1, 1)) == true && g::contains_aabb_point(read(a), v3(0, 2, 0)) == true,
          "containsAabbPoint returns true for an interior point and on the boundary");
    check(g::contains_aabb_point(read(a), v3(3, 1, 1)) == false, "containsAabbPoint returns false for a point outside");
  }
  {
    const auto src = g::create_aabb(-1, -2, -3, 4, 5, 6);
    const auto out = aabb_like();
    g::copy_aabb(out, read(src));
    check(same(out->min->x, -1) && same(out->max->y, 5), "copyAabb copies both corners");
  }
  {
    const auto a = aabb_like(-1, -2, -3, 4, 5, 6);
    g::copy_aabb(a, a);
    check(same(a->min->x, -1) && same(a->max->z, 6), "copyAabb supports out === source");
  }
  {
    const auto a = g::create_aabb();
    check(same(a->min->x, infinity) && same(a->max->x, -infinity), "createAabb defaults to an empty box (min > max)");
    const auto b = g::create_aabb(1, 2, 3, 4, 5, 6);
    check(same(b->min->x, 1) && same(b->max->z, 6), "createAabb uses provided corners");
  }
  {
    const auto a = g::create_aabb();
    const auto out = aabb_like();
    g::expand_aabb_by_point(out, read(a), v3(2, 3, 4));
    check(same(out->min->x, 2) && same(out->max->x, 2) && same(out->min->z, 4) && same(out->max->z, 4), "expandAabbByPoint: an empty box snaps exactly to the first point");
  }
  {
    const auto a = g::create_aabb(0, 0, 0, 1, 1, 1);
    const auto out = aabb_like();
    g::expand_aabb_by_point(out, read(a), v3(-5, 2, 0.5));
    check(same(out->min->x, -5) && same(out->max->y, 2) && same(out->max->z, 1), "expandAabbByPoint grows to include a point outside the box");
  }
  {
    const auto a = aabb_like(0, 0, 0, 1, 1, 1);
    g::expand_aabb_by_point(a, a, v3(3, -3, 3));
    check(same(a->min->y, -3) && same(a->max->x, 3), "expandAabbByPoint supports out === aabb");
  }
  {
    const auto a = g::create_aabb(0, 0, 0, 1, 1, 1);
    const auto s = g::create_bounding_sphere(0, 0, 0, 3);
    const auto out = aabb_like();
    g::expand_aabb_by_sphere(out, read(a), s);
    check(same(out->min->x, -3) && same(out->max->x, 3) && same(out->min->y, -3) && same(out->max->y, 3), "expandAabbBySphere grows the box to contain the sphere");
  }
  {
    const auto a = g::create_aabb(0, 0, 0, 1, 1, 1);
    const auto s = g::create_bounding_sphere(0, 0, 0, -1);
    const auto out = aabb_like();
    g::expand_aabb_by_sphere(out, read(a), s);
    check(same(out->min->x, 0) && same(out->max->x, 1), "expandAabbBySphere: an empty sphere leaves the box unchanged");
  }
  {
    const auto a = aabb_like(0, 0, 0, 1, 1, 1);
    g::expand_aabb_by_sphere(a, a, g::create_bounding_sphere(5, 0, 0, 1));
    check(same(a->max->x, 6), "expandAabbBySphere supports out === aabb");
  }
  {
    const auto out = v3();
    g::get_aabb_center(out, read(g::create_aabb(-2, -4, -6, 2, 4, 6)));
    check(xyz(out, 0, 0, 0), "getAabbCenter writes the midpoint of the corners");
  }
  {
    const auto out = v3();
    g::get_aabb_extents(out, read(g::create_aabb(-2, -4, -6, 2, 4, 6)));
    check(xyz(out, 2, 4, 6), "getAabbExtents writes half-widths of the box");
  }
  {
    const auto out = v3();
    g::get_aabb_size(out, read(g::create_aabb(-1, -2, -3, 3, 6, 9)));
    check(xyz(out, 4, 8, 12), "getAabbSize writes the full width/height/depth");
  }
  {
    const auto out = v3();
    g::get_closest_point_on_aabb(out, read(g::create_aabb(0, 0, 0, 2, 2, 2)), v3(5, -1, 1));
    check(xyz(out, 2, 0, 1), "getClosestPointOnAabb clamps an outside point to the nearest face");
  }
  {
    const auto out = v3();
    g::get_closest_point_on_aabb(out, read(g::create_aabb(0, 0, 0, 4, 4, 4)), v3(1, 2, 3));
    check(xyz(out, 1, 2, 3), "getClosestPointOnAabb returns the point unchanged when it is inside the box");
  }
  {
    const auto p = v3(-3, 5, 1);
    g::get_closest_point_on_aabb(p, read(g::create_aabb(0, 0, 0, 2, 2, 2)), p);
    check(xyz(p, 0, 2, 1), "getClosestPointOnAabb supports out === point");
  }
  FLIGHT_EXPECT_FUNCTION(g::initialize_aabb);
  {
    const auto out = aabb_like();
    g::intersect_aabb(out, read(g::create_aabb(0, 0, 0, 4, 4, 4)), read(g::create_aabb(2, 2, 2, 6, 6, 6)));
    check(same(out->min->x, 2) && same(out->max->x, 4) && same(out->min->y, 2) && same(out->max->y, 4), "intersectAabb produces the overlapping sub-box for two overlapping boxes");
  }
  {
    const auto out = aabb_like();
    g::intersect_aabb(out, read(g::create_aabb(0, 0, 0, 1, 1, 1)), read(g::create_aabb(5, 5, 5, 6, 6, 6)));
    check(out->min->x > out->max->x, "intersectAabb produces an empty box (min > max) for non-overlapping boxes");
  }
  {
    const auto a = aabb_like(0, 0, 0, 4, 4, 4);
    g::intersect_aabb(a, a, read(g::create_aabb(2, 2, 2, 6, 6, 6)));
    check(same(a->min->x, 2) && same(a->max->x, 4), "intersectAabb supports out === a");
  }
  {
    const auto b = aabb_like(2, 2, 2, 6, 6, 6);
    g::intersect_aabb(b, read(g::create_aabb(0, 0, 0, 4, 4, 4)), b);
    check(xyz(b->min, 2, 2, 2) && xyz(b->max, 4, 4, 4), "intersectAabb supports out === b");
  }
  check(g::is_aabb_intersecting_aabb(read(g::create_aabb(0, 0, 0, 2, 2, 2)), read(g::create_aabb(1, 1, 1, 3, 3, 3))) == true, "isAabbIntersectingAabb returns true for overlapping boxes");
  check(g::is_aabb_intersecting_aabb(read(g::create_aabb(0, 0, 0, 1, 1, 1)), read(g::create_aabb(1, 0, 0, 2, 1, 1))) == true, "isAabbIntersectingAabb returns true for touching boxes (shared face)");
  check(g::is_aabb_intersecting_aabb(read(g::create_aabb(0, 0, 0, 1, 1, 1)), read(g::create_aabb(5, 5, 5, 6, 6, 6))) == false, "isAabbIntersectingAabb returns false for separated boxes");
  {
    const struct {
      const char* axis;
      Aabb empty;
    } cases[] = {{"x", g::create_aabb(1, 0, 0, 0, 1, 1)}, {"y", g::create_aabb(0, 1, 0, 1, 0, 1)}, {"z", g::create_aabb(0, 0, 1, 1, 1, 0)}};
    for (const auto& c : cases) {
      const auto full = g::create_aabb(-10, -10, -10, 10, 10, 10);
      check(g::is_aabb_intersecting_aabb(read(c.empty), read(full)) == false && g::is_aabb_intersecting_aabb(read(full), read(c.empty)) == false,
            std::string("isAabbIntersectingAabb returns false when either box is empty on ") + c.axis);
    }
  }
  check(g::is_aabb_intersecting_sphere(read(g::create_aabb(-1, -1, -1, 1, 1, 1)), g::create_bounding_sphere(0, 0, 0, 0.5)) == true, "isAabbIntersectingSphere returns true when sphere center is inside the box");
  check(g::is_aabb_intersecting_sphere(read(g::create_aabb(0, 0, 0, 2, 2, 2)), g::create_bounding_sphere(3, 1, 1, 1.5)) == true, "isAabbIntersectingSphere returns true when sphere overlaps a face");
  check(g::is_aabb_intersecting_sphere(read(g::create_aabb(0, 0, 0, 1, 1, 1)), g::create_bounding_sphere(5, 5, 5, 1)) == false, "isAabbIntersectingSphere returns false when sphere is fully outside");
  check(g::is_aabb_intersecting_sphere(read(g::create_aabb(1, 0, 0, 0, 1, 1)), g::create_bounding_sphere(0.5, 0.5, 0.5, 10)) == false, "isAabbIntersectingSphere returns false for an empty aabb");
  check(g::is_aabb_intersecting_sphere(read(g::create_aabb(-10, -10, -10, 10, 10, 10)), g::create_bounding_sphere(0, 0, 0, -1)) == false, "isAabbIntersectingSphere returns false for a negative-radius sphere");
  {
    const auto a = aabb_like();
    g::set_aabb(a, -1, -1, -1, 1, 1, 1);
    check(same(a->min->x, -1) && same(a->max->z, 1), "setAabb sets both corners");
  }
  {
    const auto a = aabb_like();
    flight::Array<flight::StructuralRef<flight::RowReadonly<flight::RowOf<Vec3>>>> points;
    points.push(v3(1, 5, -2));
    points.push(v3(-3, 0, 4));
    points.push(v3(2, 2, 2));
    g::set_aabb_from_points(a, points);
    check(xyz(a->min, -3, 0, -2) && xyz(a->max, 2, 5, 4), "setAabbFromPoints computes the tight box over a set of points");
  }
  {
    const auto a = aabb_like(0, 0, 0, 1, 1, 1);
    g::set_aabb_from_points(a, {});
    check(same(a->min->x, infinity) && same(a->max->x, -infinity), "setAabbFromPoints: empty list yields an empty box");
  }
  {
    const auto out = aabb_like();
    g::transform_aabb_by_matrix4(out, read(g::create_aabb(-1, -1, -1, 1, 1, 1)), translation(10, 0, 0));
    check(close(out->min->x, 9, 6) && close(out->max->x, 11, 6) && close(out->min->y, -1, 6), "transformAabbByMatrix4 translates the box");
  }
  {
    const double c = M_SQRT1_2;
    const auto m = g::create_matrix4(c, c, 0, 0, -c, c, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
    const auto out = aabb_like();
    g::transform_aabb_by_matrix4(out, read(g::create_aabb(-1, -1, -1, 1, 1, 1)), m);
    check(close(out->max->x, M_SQRT2, 5) && close(out->min->x, -M_SQRT2, 5) && close(out->max->z, 1, 6), "transformAabbByMatrix4 grows the box under a 45-degree rotation about z");
  }
  {
    const auto a = aabb_like(-1, -1, -1, 1, 1, 1);
    g::transform_aabb_by_matrix4(a, a, translation(5, 5, 5));
    check(close(a->min->x, 4, 6) && close(a->max->z, 6, 6), "transformAabbByMatrix4 supports out === aabb");
  }
  {
    const auto out = aabb_like();
    g::union_aabb(out, read(g::create_aabb(0, 0, 0, 1, 1, 1)), read(g::create_aabb(-2, 3, -1, -1, 5, 0)));
    check(same(out->min->x, -2) && same(out->min->z, -1) && same(out->max->y, 5) && same(out->max->x, 1), "unionAabb encloses both boxes");
  }
  {
    const auto a = aabb_like(0, 0, 0, 1, 1, 1);
    g::union_aabb(a, a, read(g::create_aabb(-2, 0, 0, 0, 4, 0)));
    check(same(a->min->x, -2) && same(a->max->y, 4), "unionAabb supports out === a");
  }
  {
    const auto b = aabb_like(-2, 0, 0, 0, 4, 0);
    g::union_aabb(b, read(g::create_aabb(0, 0, 0, 1, 1, 1)), b);
    check(same(b->min->x, -2) && same(b->max->y, 4), "unionAabb supports out === b");
  }
}

void test_bounding_sphere() {
  {
    const auto s = g::create_bounding_sphere(1, 2, 3, 4);
    const auto c = g::clone_bounding_sphere(s);
    const bool ok = c != s && c->center != s->center && same(c->center->x, 1) && same(c->radius, 4);
    c->center->x = 99;
    check(ok && same(s->center->x, 1), "cloneBoundingSphere creates an independent copy with an independent center");
  }
  {
    const auto s = g::create_bounding_sphere(0, 0, 0, 2);
    check(g::contains_bounding_sphere_point(s, v3(1, 0, 0)) == true && g::contains_bounding_sphere_point(s, v3(2, 0, 0)) == true, "containsBoundingSpherePoint returns true inside and on the surface");
    check(g::contains_bounding_sphere_point(s, v3(3, 0, 0)) == false, "containsBoundingSpherePoint returns false outside");
    check(g::contains_bounding_sphere_point(g::create_bounding_sphere(0, 0, 0, -1), v3(0, 0, 0)) == false, "containsBoundingSpherePoint: an empty sphere contains no points");
  }
  {
    const auto out = g::create_bounding_sphere();
    g::copy_bounding_sphere(out, g::create_bounding_sphere(1, 2, 3, 4));
    check(same(out->center->x, 1) && same(out->radius, 4), "copyBoundingSphere copies center and radius");
    const auto s = g::create_bounding_sphere(1, 2, 3, 4);
    g::copy_bounding_sphere(s, s);
    check(same(s->center->y, 2) && same(s->radius, 4), "copyBoundingSphere supports out === source");
  }
  {
    const auto s = g::create_bounding_sphere();
    check(same(s->center->x, 0) && same(s->radius, -1), "createBoundingSphere defaults to an empty sphere (negative radius)");
    const auto t = g::create_bounding_sphere(1, 2, 3, 5);
    check(same(t->center->z, 3) && same(t->radius, 5), "createBoundingSphere uses provided center and radius");
  }
  {
    const auto out = v3();
    g::get_closest_point_on_bounding_sphere(out, g::create_bounding_sphere(0, 0, 0, 2), v3(10, 0, 0));
    check(close(out->x, 2, 6) && close(out->y, 0, 6) && close(out->z, 0, 6), "getClosestPointOnBoundingSphere projects an outside point onto the surface");
  }
  {
    const auto out = v3();
    g::get_closest_point_on_bounding_sphere(out, g::create_bounding_sphere(0, 0, 0, 5), v3(0, 1, 0));
    check(close(out->x, 0, 6) && close(out->y, 5, 6) && close(out->z, 0, 6), "getClosestPointOnBoundingSphere pulls an inside point out to the surface");
  }
  {
    const auto out = v3();
    g::get_closest_point_on_bounding_sphere(out, g::create_bounding_sphere(1, 2, 3, 4), v3(1, 2, 3));
    check(close(out->x, 5, 6) && close(out->y, 2, 6) && close(out->z, 3, 6), "getClosestPointOnBoundingSphere uses the +X fallback when the point is at the center");
  }
  {
    const auto out = v3();
    g::get_closest_point_on_bounding_sphere(out, g::create_bounding_sphere(1, 2, 3, -1), v3(9, 9, 9));
    check(xyz(out, 1, 2, 3), "getClosestPointOnBoundingSphere writes the center for an empty sphere");
  }
  {
    const auto p = v3(6, 0, 0);
    g::get_closest_point_on_bounding_sphere(p, g::create_bounding_sphere(0, 0, 0, 3), p);
    check(close(p->x, 3, 6), "getClosestPointOnBoundingSphere supports out === point");
  }
  FLIGHT_EXPECT_FUNCTION(g::initialize_bounding_sphere);
  check(g::is_bounding_sphere_intersecting_bounding_sphere(g::create_bounding_sphere(0, 0, 0, 2), g::create_bounding_sphere(3, 0, 0, 2)) == true, "isBoundingSphereIntersectingBoundingSphere returns true for overlapping spheres");
  check(g::is_bounding_sphere_intersecting_bounding_sphere(g::create_bounding_sphere(0, 0, 0, 1), g::create_bounding_sphere(2, 0, 0, 1)) == true, "isBoundingSphereIntersectingBoundingSphere returns true for touching spheres");
  check(g::is_bounding_sphere_intersecting_bounding_sphere(g::create_bounding_sphere(0, 0, 0, 1), g::create_bounding_sphere(5, 0, 0, 1)) == false, "isBoundingSphereIntersectingBoundingSphere returns false for separated spheres");
  {
    const auto a = g::create_bounding_sphere(0, 0, 0, 1);
    const auto empty = g::create_bounding_sphere(0, 0, 0, -1);
    check(g::is_bounding_sphere_intersecting_bounding_sphere(a, empty) == false && g::is_bounding_sphere_intersecting_bounding_sphere(empty, a) == false,
          "isBoundingSphereIntersectingBoundingSphere returns false if either sphere is empty");
  }
  {
    const auto out = g::create_bounding_sphere();
    g::merge_bounding_sphere(out, g::create_bounding_sphere(-1, 0, 0, 1), g::create_bounding_sphere(1, 0, 0, 1));
    check(g::contains_bounding_sphere_point(out, v3(-2, 0, 0)) && g::contains_bounding_sphere_point(out, v3(2, 0, 0)), "mergeBoundingSphere merges two overlapping spheres into a containing sphere");
  }
  {
    const auto out = g::create_bounding_sphere();
    g::merge_bounding_sphere(out, g::create_bounding_sphere(0, 0, 0, 10), g::create_bounding_sphere(1, 0, 0, 1));
    check(close(out->center->x, 0, 5) && close(out->radius, 10, 5), "mergeBoundingSphere: if a contains b, result equals a");
  }
  {
    const auto out = g::create_bounding_sphere();
    g::merge_bounding_sphere(out, g::create_bounding_sphere(), g::create_bounding_sphere(3, 0, 0, 2));
    check(close(out->center->x, 3, 5) && close(out->radius, 2, 5), "mergeBoundingSphere: empty source returns the other sphere");
  }
  {
    const auto out = g::create_bounding_sphere();
    g::merge_bounding_sphere(out, g::create_bounding_sphere(1, 0, 0, 1), g::create_bounding_sphere(0, 0, 0, 10));
    check(close(out->center->x, 0, 5) && close(out->radius, 10, 5), "mergeBoundingSphere: if b contains a, result equals b");
  }
  {
    const auto out = g::create_bounding_sphere();
    g::merge_bounding_sphere(out, g::create_bounding_sphere(3, 0, 0, 2), g::create_bounding_sphere());
    check(close(out->center->x, 3, 5) && close(out->radius, 2, 5), "mergeBoundingSphere: an empty second sphere returns the first");
  }
  {
    const auto a = g::create_bounding_sphere(-1, 0, 0, 1);
    const auto b = g::create_bounding_sphere(1, 0, 0, 1);
    g::merge_bounding_sphere(b, a, b);
    check(g::contains_bounding_sphere_point(b, v3(-2, 0, 0)) && g::contains_bounding_sphere_point(b, v3(2, 0, 0)), "mergeBoundingSphere supports out === b");
  }
  {
    const auto a = g::create_bounding_sphere(-1, 0, 0, 1);
    const auto b = g::create_bounding_sphere(1, 0, 0, 1);
    g::merge_bounding_sphere(a, a, b);
    check(g::contains_bounding_sphere_point(a, v3(-2, 0, 0)) && g::contains_bounding_sphere_point(a, v3(2, 0, 0)), "mergeBoundingSphere supports out === a");
  }
  {
    const auto s = g::create_bounding_sphere();
    g::set_bounding_sphere(s, 5, 6, 7, 8);
    check(same(s->center->x, 5) && same(s->radius, 8), "setBoundingSphere sets center and radius");
  }
  {
    const auto s = g::create_bounding_sphere();
    g::set_bounding_sphere_from_aabb(s, read(g::create_aabb(-1, -1, -1, 1, 1, 1)));
    check(xyz(s->center, 0, 0, 0) && close(s->radius, std::sqrt(3.0), 6), "setBoundingSphereFromAabb centers on the box and reaches its corners");
  }
  {
    const auto s = g::create_bounding_sphere(0, 0, 0, 5);
    g::set_bounding_sphere_from_aabb(s, read(g::create_aabb()));
    check(same(s->radius, -1), "setBoundingSphereFromAabb: an empty box yields an empty sphere");
  }
  {
    const auto out = g::create_bounding_sphere();
    g::transform_bounding_sphere_by_matrix4(out, g::create_bounding_sphere(0, 0, 0, 2), translation(10, 0, 0));
    check(close(out->center->x, 10, 6) && close(out->radius, 2, 6), "transformBoundingSphereByMatrix4 translates the center and preserves the radius");
  }
  {
    const auto out = g::create_bounding_sphere();
    g::transform_bounding_sphere_by_matrix4(out, g::create_bounding_sphere(0, 0, 0, 1), g::create_matrix4(2, 0, 0, 0, 0, 3, 0, 0, 0, 0, 4, 0, 0, 0, 0, 1));
    check(close(out->radius, 4, 6), "transformBoundingSphereByMatrix4 scales the radius by the largest axis scale");
  }
  {
    const auto s = g::create_bounding_sphere(1, 0, 0, 2);
    g::transform_bounding_sphere_by_matrix4(s, s, translation(0, 5, 0));
    check(close(s->center->x, 1, 6) && close(s->center->y, 5, 6) && close(s->radius, 2, 6), "transformBoundingSphereByMatrix4 supports out === sphere");
  }
  {
    const auto out = g::create_bounding_sphere();
    g::transform_bounding_sphere_by_matrix4(out, g::create_bounding_sphere(0, 0, 0, -1), g::create_matrix4(3, 0, 0, 0, 0, 3, 0, 0, 0, 0, 3, 0, 0, 0, 0, 1));
    check(same(out->radius, -1), "transformBoundingSphereByMatrix4 leaves an empty sphere empty instead of scaling its negative radius");
  }
}

// The smallest distance between two segments, found by walking both of them (upstream's
// measureSegmentDistance): slow and approximate, but independent of the closed form it checks.
double measure_segment_distance(const std::array<double, 6>& first, const std::array<double, 6>& second) {
  const int steps = 600;
  double smallest = infinity;
  for (int i = 0; i <= steps; ++i) {
    const double s = static_cast<double>(i) / steps;
    const double px = first[0] + s * (first[3] - first[0]);
    const double py = first[1] + s * (first[4] - first[1]);
    const double pz = first[2] + s * (first[5] - first[2]);
    for (int j = 0; j <= steps; ++j) {
      const double t = static_cast<double>(j) / steps;
      const double qx = second[0] + t * (second[3] - second[0]);
      const double qy = second[1] + t * (second[4] - second[1]);
      const double qz = second[2] + t * (second[5] - second[2]);
      const double distance = std::hypot(px - qx, py - qy, pz - qz);
      if (distance < smallest) smallest = distance;
    }
  }
  return smallest;
}

flight::Ref<types::Capsule> capsule(const std::array<double, 6>& axis, double radius) {
  return g::create_capsule(axis[0], axis[1], axis[2], axis[3], axis[4], axis[5], radius);
}

bool capsule_fields(const flight::Ref<types::Capsule>& c, double sx, double sy, double sz, double ex, double ey, double ez, double r) {
  return same(c->start_x, sx) && same(c->start_y, sy) && same(c->start_z, sz) && same(c->end_x, ex) && same(c->end_y, ey) && same(c->end_z, ez) && same(c->radius, r);
}

void test_capsule() {
  check(capsule_fields(g::create_capsule(1, 2, 3, 4, 5, 6, 0.5), 1, 2, 3, 4, 5, 6, 0.5), "createCapsule stores all fields");
  {
    const auto out = v3();
    g::get_closest_point_on_capsule(out, g::create_capsule(0, -1, 0, 0, 1, 0, 1), v3(5, 0, 0));
    check(close(out->x, 1, 5) && close(out->y, 0, 5) && close(out->z, 0, 5), "getClosestPointOnCapsule returns the nearest surface point for a point off the side");
  }
  {
    const auto out = v3();
    g::get_closest_point_on_capsule(out, g::create_capsule(0, 0, 0, 0, 2, 0, 1), v3(0, -5, 0));
    check(close(out->x, 0, 5) && close(out->y, -1, 5) && close(out->z, 0, 5), "getClosestPointOnCapsule returns the start cap surface for a point beyond the start");
  }
  {
    const auto out = v3();
    g::get_closest_point_on_capsule(out, g::create_capsule(0, 0, 0, 0, 2, 0, 1), v3(0, 10, 0));
    check(close(out->x, 0, 5) && close(out->y, 3, 5) && close(out->z, 0, 5), "getClosestPointOnCapsule returns the end cap surface for a point beyond the end");
  }
  {
    const auto p = v3(5, 0, 0);
    g::get_closest_point_on_capsule(p, g::create_capsule(0, 0, 0, 0, 0, 0, 1), p);
    check(close(p->x, 1, 5) && close(p->y, 0, 5), "getClosestPointOnCapsule supports out === point");
  }
  {
    const auto out = v3();
    const auto length = [&] { return std::hypot(out->x, out->y, out->z); };
    bool ok = true;
    g::get_closest_point_on_capsule(out, g::create_capsule(-5, 0, 0, 5, 0, 0, 1), v3(0, 0, 0));
    ok = ok && close(length(), 1, 5) && close(out->x, 0, 5);
    g::get_closest_point_on_capsule(out, g::create_capsule(0, -5, 0, 0, 5, 0, 1), v3(0, 0, 0));
    ok = ok && close(length(), 1, 5) && close(out->y, 0, 5);
    g::get_closest_point_on_capsule(out, g::create_capsule(0, 0, -5, 0, 0, 5, 1), v3(0, 0, 0));
    ok = ok && close(length(), 1, 5) && close(out->z, 0, 5);
    g::get_closest_point_on_capsule(out, g::create_capsule(0, 0, 0, 0, 0, 0, 1), v3(0, 0, 0));
    ok = ok && close(length(), 1, 5);
    check(ok, "getClosestPointOnCapsule answers a point on the axis with a surface point, whichever way the axis runs");
  }
  {
    const auto out = v3();
    g::get_closest_point_on_capsule(out, g::create_capsule(-3, -3, -3, 3, 3, 3, 2), v3(0, 0, 0));
    bool ok = close(std::hypot(out->x, out->y, out->z), 2, 5) && close(out->x + out->y + out->z, 0, 5);
    g::get_closest_point_on_capsule(out, g::create_capsule(-3, -2, -1, 3, 2, 1, 2), v3(0, 0, 0));
    ok = ok && close(std::hypot(out->x, out->y, out->z), 2, 5) && close(out->x * 3 + out->y * 2 + out->z * 1, 0, 5);
    check(ok, "getClosestPointOnCapsule answers a point on a diagonal axis with a surface point too");
  }
  FLIGHT_EXPECT_FUNCTION(g::initialize_capsule);
  const auto unit = [] { return g::create_capsule(0, -1, 0, 0, 1, 0, 1); };
  check(close(g::intersect_ray3_dcapsule(g::create_ray3_d(5, 0, 0, -1, 0, 0), unit()), 4, 5), "intersectRay3DCapsule hits a unit capsule from the side");
  check(same(g::intersect_ray3_dcapsule(g::create_ray3_d(5, 0, 0, 1, 0, 0), unit()), -1), "intersectRay3DCapsule returns -1 for a ray pointing away from the capsule");
  check(same(g::intersect_ray3_dcapsule(g::create_ray3_d(0, 0, 0, 1, 0, 0), unit()), 0), "intersectRay3DCapsule returns 0 for a ray starting inside the capsule");
  check(close(g::intersect_ray3_dcapsule(g::create_ray3_d(0, 5, 0, 0, -1, 0), g::create_capsule(0, 0, 0, 0, 0, 0, 1)), 4, 5), "intersectRay3DCapsule hits the end cap from above");
  check(same(g::intersect_ray3_dcapsule(g::create_ray3_d(0, 0, 0, 0, 0, 0), unit()), -1) && same(g::intersect_ray3_dcapsule(g::create_ray3_d(9, 9, 9, 0, 0, 0), unit()), -1),
        "intersectRay3DCapsule returns -1 for a ray with no direction");
  check(same(g::intersect_ray3_dcapsule(g::create_ray3_d(5, 0, 0, -1, 0, 0), g::create_capsule(0, -1, 0, 0, 1, 0, -1)), -1), "intersectRay3DCapsule returns -1 for an empty capsule with a negative radius");
  check(same(g::intersect_ray3_dcapsule(g::create_ray3_d(5, 0, 5, -1, 0, 0), unit()), -1), "intersectRay3DCapsule returns -1 for a ray that passes wide of the capsule");
  check(close(g::intersect_ray3_dcapsule(g::create_ray3_d(0, 5, 0, 0, -1, 0), unit()), 3, 5), "intersectRay3DCapsule hits a cap when the ray runs along the capsule axis");
  check(same(g::intersect_ray3_dcapsule(g::create_ray3_d(0, 1.5, 0, 0, 1, 0), unit()), 0), "intersectRay3DCapsule returns 0 for a ray starting inside a cap and pointing away");
  check(close(g::intersect_ray3_dcapsule(g::create_ray3_d(5, 1.8, 0, -1, 0, 0), unit()), 4.4, 5) && close(g::intersect_ray3_dcapsule(g::create_ray3_d(5, -1.8, 0, -1, 0, 0), unit()), 4.4, 5) &&
            same(g::intersect_ray3_dcapsule(g::create_ray3_d(5, 2.5, 0, -1, 0, 0), unit()), -1),
        "intersectRay3DCapsule falls back to a cap when the body hit lands past the end of the axis");
  check(same(g::intersect_ray3_dcapsule(g::create_ray3_d(0, 3, 0, 1, 1, 0), unit()), -1), "intersectRay3DCapsule returns -1 from inside the infinite cylinder but past the end cap");

  check(g::is_capsule_intersecting_aabb(g::create_capsule(-5, 0, 0, 5, 0, 0, 0.5), read(g::create_aabb(-1, -1, -1, 1, 1, 1))) == true, "isCapsuleIntersectingAabb returns true when the capsule segment passes through the AABB");
  check(g::is_capsule_intersecting_aabb(g::create_capsule(0, 2, 0, 0, 2, 0, 1.5), read(g::create_aabb(-1, -1, -1, 1, 1, 1))) == true, "isCapsuleIntersectingAabb returns true when the capsule radius reaches the AABB");
  check(g::is_capsule_intersecting_aabb(g::create_capsule(10, 10, 10, 10, 12, 10, 0.5), read(g::create_aabb(-1, -1, -1, 1, 1, 1))) == false, "isCapsuleIntersectingAabb returns false when the capsule is far from the AABB");
  check(g::is_capsule_intersecting_aabb(g::create_capsule(0, 0, 0, 0, 0, 0, -1), read(g::create_aabb(-10, -10, -10, 10, 10, 10))) == false, "isCapsuleIntersectingAabb returns false for a negative-radius capsule");
  check(g::is_capsule_intersecting_aabb(g::create_capsule(0, 0, 0, 1, 0, 0, 5), read(g::create_aabb(1, 0, 0, 0, 1, 1))) == false, "isCapsuleIntersectingAabb returns false for an empty AABB");

  check(g::is_capsule_intersecting_capsule(g::create_capsule(0, 0, 0, 0, 2, 0, 1), g::create_capsule(1, 0, 0, 1, 2, 0, 1)) == true, "isCapsuleIntersectingCapsule returns true for two overlapping capsules");
  check(g::is_capsule_intersecting_capsule(g::create_capsule(0, 0, 0, 0, 2, 0, 0.5), g::create_capsule(10, 0, 0, 10, 2, 0, 0.5)) == false, "isCapsuleIntersectingCapsule returns false for two separated capsules");
  {
    const auto a = g::create_capsule(0, 0, 0, 0, 2, 0, 1);
    const auto empty = g::create_capsule(0, 0, 0, 0, 2, 0, -1);
    check(g::is_capsule_intersecting_capsule(a, empty) == false && g::is_capsule_intersecting_capsule(empty, a) == false, "isCapsuleIntersectingCapsule returns false if either capsule has a negative radius");
  }
  {
    const struct {
      const char* name;
      std::array<double, 6> first, second;
    } arrangements[] = {
        {"two points", {1, 2, 3, 1, 2, 3}, {4, 6, 3, 4, 6, 3}},
        {"point and segment", {0, 5, 0, 0, 5, 0}, {-10, 0, 0, 10, 0, 0}},
        {"segment and point", {-10, 0, 0, 10, 0, 0}, {0, 5, 0, 0, 5, 0}},
        {"parallel", {0, 0, 0, 0, 10, 0}, {3, 0, 0, 3, 10, 0}},
        {"skew, nearest approach inside both", {-5, 0, 0, 5, 0, 0}, {0, -5, 2, 0, 5, 2}},
        {"nearest approach before the second start", {0, 0, 0, 10, 0, 0}, {5, 5, 3, 5, 15, 3}},
        {"nearest approach past the second end", {0, 0, 0, 10, 0, 0}, {5, -15, 3, 5, -5, 3}},
    };
    for (const auto& a : arrangements) {
      const double gap = measure_segment_distance(a.first, a.second);
      const auto reach = capsule(a.second, gap * 0.45);
      check(g::is_capsule_intersecting_capsule(capsule(a.first, gap * 0.45), reach) == false && g::is_capsule_intersecting_capsule(capsule(a.first, gap * 0.55), reach) == true,
            std::string("isCapsuleIntersectingCapsule switches over at the true axis distance for every segment arrangement: ") + a.name);
    }
  }
  {
    const auto c = g::create_capsule(0, -2, 0, 0, 2, 0, 1);
    check(g::is_capsule_intersecting_sphere(c, g::create_bounding_sphere(2, 0, 0, 1)) == true, "isCapsuleIntersectingSphere returns true when the sphere overlaps the capsule");
    check(g::is_capsule_intersecting_sphere(c, g::create_bounding_sphere(10, 0, 0, 1)) == false, "isCapsuleIntersectingSphere returns false when the sphere is separated from the capsule");
    check(g::is_capsule_intersecting_sphere(c, g::create_bounding_sphere(0, 0, 0, -1)) == false, "isCapsuleIntersectingSphere returns false if the sphere radius is negative");
    check(g::is_capsule_intersecting_sphere(g::create_capsule(0, -2, 0, 0, 2, 0, -1), g::create_bounding_sphere(0, 0, 0, 1)) == false, "isCapsuleIntersectingSphere returns false if the capsule radius is negative");
    const auto point = g::create_capsule(0, 0, 0, 0, 0, 0, 1);
    check(g::is_capsule_intersecting_sphere(point, g::create_bounding_sphere(2.5, 0, 0, 1)) == false && g::is_capsule_intersecting_sphere(point, g::create_bounding_sphere(1.5, 0, 0, 1)) == true,
          "isCapsuleIntersectingSphere treats a capsule with no length as a sphere");
  }
  {
    const auto c = g::create_capsule(0, 0, 0, 0, 0, 0, 0);
    g::set_capsule(c, 1, 2, 3, 4, 5, 6, 7);
    check(capsule_fields(c, 1, 2, 3, 4, 5, 6, 7), "setCapsule updates all fields in place");
  }
}

bool plane_fields(const flight::Ref<types::Plane>& p, double a, double b, double c, double d) { return same(p->a, a) && same(p->b, b) && same(p->c, c) && same(p->d, d); }

void test_plane() {
  {
    const auto p = g::create_plane(1, 0, 0, -5);
    const auto c = g::clone_plane(p);
    check(c != p && same(c->a, 1) && same(c->d, -5), "clonePlane creates an independent copy");
  }
  {
    const auto out = g::create_plane();
    g::copy_plane(out, g::create_plane(0, 1, 0, 3));
    check(plane_fields(out, 0, 1, 0, 3), "copyPlane copies all coefficients");
    const auto p = g::create_plane(2, 3, 4, 5);
    g::copy_plane(p, p);
    check(same(p->a, 2) && same(p->d, 5), "copyPlane supports out === source");
  }
  check(plane_fields(g::create_plane(), 0, 0, 0, 0), "createPlane defaults to zeros");
  check(plane_fields(g::create_plane(1, 2, 3, 4), 1, 2, 3, 4), "createPlane uses provided coefficients");
  {
    const auto out = v3();
    g::get_closest_point_on_plane(out, g::create_plane(0, 0, 1, 0), v3(2, 3, 5));
    check(close(out->x, 2, 6) && close(out->y, 3, 6) && close(out->z, 0, 6), "getClosestPointOnPlane projects a point onto the plane along the normal");
  }
  {
    const auto out = v3();
    g::get_closest_point_on_plane(out, g::create_plane(1, 0, 0, -5), v3(5, 1, 2));
    check(close(out->x, 5, 6) && close(out->y, 1, 6) && close(out->z, 2, 6), "getClosestPointOnPlane leaves a point already on the plane in place");
  }
  {
    const auto p = g::create_plane(0, 1, 0, -3);
    const auto closest = v3();
    const auto projected = v3();
    g::get_closest_point_on_plane(closest, p, v3(4, 7, -2));
    g::project_vector3_onto_plane(projected, v3(4, 7, -2), p);
    check(close(closest->x, projected->x, 6) && close(closest->y, projected->y, 6) && close(closest->z, projected->z, 6), "getClosestPointOnPlane agrees with projectVector3OntoPlane");
  }
  {
    const auto v = v3(1, 2, 9);
    g::get_closest_point_on_plane(v, g::create_plane(0, 0, 1, 0), v);
    check(close(v->z, 0, 6), "getClosestPointOnPlane supports out === point");
  }
  {
    const auto p = g::create_plane(1, 0, 0, -5);
    const auto out = v3();
    g::get_plane_coplanar_point(out, p);
    check(close(g::get_plane_signed_distance_to_point(p, out), 0, 6), "getPlaneCoplanarPoint: point on the plane has signed distance zero");
  }
  {
    const auto p = g::create_plane(1, 0, 0, 0);
    check(close(g::get_plane_signed_distance_to_point(p, v3(3, 0, 0)), 3, 6) && close(g::get_plane_signed_distance_to_point(p, v3(-2, 5, 9)), -2, 6),
          "getPlaneSignedDistanceToPoint is positive on the normal side and negative behind");
    check(close(g::get_plane_signed_distance_to_point(g::create_plane(1, 0, 0, -4), v3(4, 0, 0)), 0, 6), "getPlaneSignedDistanceToPoint accounts for the plane offset");
  }
  FLIGHT_EXPECT_FUNCTION(g::initialize_plane);
  {
    const auto out = g::create_plane();
    g::normalize_plane(out, g::create_plane(0, 2, 0, 4));
    check(close(out->b, 1, 6) && close(out->d, 2, 6), "normalizePlane normalizes a plane with non-unit normal");
    const auto p = g::create_plane(0, 3, 0, 6);
    g::normalize_plane(p, p);
    check(close(p->b, 1, 6) && close(p->d, 2, 6), "normalizePlane supports out === source");
    const auto degenerate = g::create_plane();
    g::normalize_plane(degenerate, g::create_plane(0, 0, 0, 5));
    check(same(degenerate->d, 5), "normalizePlane leaves a degenerate plane unchanged");
  }
  {
    const auto out = v3();
    g::project_vector3_onto_plane(out, v3(3, 5, 2), g::create_plane(0, 1, 0, 0));
    check(close(out->x, 3, 6) && close(out->y, 0, 6) && close(out->z, 2, 6), "projectVector3OntoPlane projects a point above the XZ plane down to y=0");
    const auto pt = v3(4, 2, 1);
    g::project_vector3_onto_plane(pt, pt, g::create_plane(1, 0, 0, 0));
    check(close(pt->x, 0, 6) && close(pt->y, 2, 6) && close(pt->z, 1, 6), "projectVector3OntoPlane supports out === point");
  }
  {
    const auto p = g::create_plane();
    g::set_plane(p, 9, 8, 7, 6);
    check(plane_fields(p, 9, 8, 7, 6), "setPlane sets all coefficients");
  }
  {
    const auto point = v3(0, 5, 0);
    const auto p = g::create_plane();
    g::set_plane_from_normal_and_point(p, v3(0, 1, 0), point);
    check(close(g::get_plane_signed_distance_to_point(p, point), 0, 6), "setPlaneFromNormalAndPoint builds a plane that contains the given point");
    const auto q = g::create_plane();
    g::set_plane_from_normal_and_point(q, v3(1, 0, 0), v3(3, 0, 0));
    check(close(q->a, 1, 6) && close(q->b, 0, 6) && close(q->c, 0, 6) && close(q->d, -3, 6), "setPlaneFromNormalAndPoint stores the normal in (a, b, c)");
  }
  {
    const auto p = g::create_plane();
    g::set_plane_from_points(p, v3(0, 0, 0), v3(1, 0, 0), v3(0, 0, 1));
    check(close(std::abs(p->b), 1, 5) && close(p->a, 0, 5) && close(p->c, 0, 5), "setPlaneFromPoints builds the XZ plane from three points in XZ");
  }
  {
    const auto a = v3(1, 2, 3);
    const auto b = v3(4, 0, 1);
    const auto c = v3(-1, 3, 5);
    const auto p = g::create_plane();
    g::set_plane_from_points(p, a, b, c);
    check(close(g::get_plane_signed_distance_to_point(p, a), 0, 5) && close(g::get_plane_signed_distance_to_point(p, b), 0, 5) && close(g::get_plane_signed_distance_to_point(p, c), 0, 5),
          "setPlaneFromPoints: all three points lie on the resulting plane");
  }
  {
    const auto p = g::create_plane(1, 1, 1, 1);
    g::set_plane_from_points(p, v3(0, 0, 0), v3(1, 1, 1), v3(2, 2, 2));
    const auto q = g::create_plane(1, 1, 1, 1);
    const auto point = v3(4, 5, 6);
    g::set_plane_from_points(q, point, point, point);
    check(plane_fields(p, 0, 0, 0, 0) && plane_fields(q, 0, 0, 0, 0), "setPlaneFromPoints writes a zero plane for three points that do not span one");
  }
}

// View-projection for a camera at (0,0,5) looking toward the origin down -z (upstream's
// createTestViewProjection).
flight::Ref<types::Matrix4> test_view_projection() {
  const auto projection = g::create_perspective_matrix4(0.5, 1, 0.1, 100);
  const auto view = g::create_matrix4();
  g::set_matrix4_look_at(view, v3(0, 0, 5), v3(0, 0, 0), v3(0, 1, 0));
  const auto vp = g::create_matrix4();
  g::multiply_matrix4(vp, projection, view);
  return vp;
}

flight::Ref<types::Frustum> view_frustum() {
  const auto f = g::create_frustum();
  g::set_frustum_from_matrix4(f, test_view_projection());
  return f;
}

flight::Array<types::Vector3Like> fresh_vectors(int count) {
  flight::Array<types::Vector3Like> out;
  for (int i = 0; i < count; ++i) out.push(v3());
  return out;
}

void test_frustum() {
  {
    const auto f = g::create_frustum();
    check(f->left != f->right && f->near != f->far && same(f->top->a, 0), "createFrustum creates six independent planes");
  }
  {
    const auto inv = g::create_matrix4();
    g::inverse_matrix4(inv, test_view_projection());
    const auto corners = fresh_vectors(8);
    g::get_frustum_corners(corners, inv);
    bool finite = true;
    std::set<std::string> seen;
    for (std::size_t i = 0; i < corners.size(); ++i) {
      const auto& c = corners[i];
      finite = finite && std::isfinite(c->x) && std::isfinite(c->y) && std::isfinite(c->z);
      char key[128];
      std::snprintf(key, sizeof key, "%.4f,%.4f,%.4f", c->x, c->y, c->z);
      seen.insert(key);
    }
    check(finite && seen.size() == 8, "getFrustumCorners writes 8 corners for a perspective frustum");
    bool ordered = true;
    for (std::size_t n = 0; n < 4; ++n)
      for (std::size_t f = 4; f < 8; ++f) ordered = ordered && corners[n]->z > corners[f]->z;
    check(ordered, "getFrustumCorners: near corners are closer to the camera than far corners");
  }
  {
    const auto inv = g::create_matrix4();
    g::inverse_matrix4(inv, test_view_projection());
    const auto corners = fresh_vectors(2);
    g::get_frustum_corners(corners, inv);
    check(std::isfinite(corners[0]->x) && std::isfinite(corners[1]->x), "getFrustumCorners writes only as many corners as out.length");
  }
  {
    const auto corners = fresh_vectors(8);
    g::get_frustum_corners(corners, g::create_matrix4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0));
    bool finite = true;
    for (std::size_t i = 0; i < corners.size(); ++i) finite = finite && std::isfinite(corners[i]->x) && std::isfinite(corners[i]->y) && std::isfinite(corners[i]->z);
    check(finite, "getFrustumCorners writes finite corners for a matrix that divides a corner by zero");
  }
  FLIGHT_EXPECT_FUNCTION(g::initialize_frustum);
  check(g::is_frustum_containing_point(view_frustum(), v3(0, 0, 0)) == true, "isFrustumContainingPoint contains a point in front of the camera");
  check(g::is_frustum_containing_point(view_frustum(), v3(0, 0, 20)) == false, "isFrustumContainingPoint rejects a point behind the camera");
  check(g::is_frustum_containing_point(view_frustum(), v3(100, 0, 0)) == false, "isFrustumContainingPoint rejects a point far off to the side");
  check(g::is_frustum_intersecting_aabb(view_frustum(), read(g::create_aabb(-1, -1, -1, 1, 1, 1))) == true, "isFrustumIntersectingAabb accepts a box straddling the origin");
  check(g::is_frustum_intersecting_aabb(view_frustum(), read(g::create_aabb(-1, -1, 10, 1, 1, 12))) == false, "isFrustumIntersectingAabb rejects a box entirely behind the camera");
  check(g::is_frustum_intersecting_aabb(view_frustum(), read(g::create_aabb(50, 50, -1, 60, 60, 1))) == false, "isFrustumIntersectingAabb rejects a box far off to the side");
  {
    const struct {
      const char* axis;
      Aabb empty;
    } cases[] = {{"x", g::create_aabb(0.5, -0.5, -0.5, -0.5, 0.5, 0.5)}, {"y", g::create_aabb(-0.5, 0.5, -0.5, 0.5, -0.5, 0.5)}, {"z", g::create_aabb(-0.5, -0.5, 0.5, 0.5, 0.5, -0.5)}};
    for (const auto& c : cases) {
      const auto f = g::create_frustum();
      g::set_frustum_from_matrix4(f, g::create_matrix4());
      check(g::is_frustum_intersecting_aabb(f, read(c.empty)) == false, std::string("isFrustumIntersectingAabb rejects an AABB empty on ") + c.axis);
    }
  }
  check(g::is_frustum_intersecting_obb(view_frustum(), g::create_obb(0, 0, 0, 1, 1, 1, 0, 0, 0, 1)) == true, "isFrustumIntersectingObb accepts an axis-aligned OBB inside the frustum");
  check(g::is_frustum_intersecting_obb(view_frustum(), g::create_obb(0, 0, 20, 1, 1, 1, 0, 0, 0, 1)) == false, "isFrustumIntersectingObb rejects an OBB behind the camera");
  check(g::is_frustum_intersecting_obb(view_frustum(), g::create_obb(100, 0, 0, 1, 1, 1, 0, 0, 0, 1)) == false, "isFrustumIntersectingObb rejects an OBB to the far right");
  check(g::is_frustum_intersecting_sphere(view_frustum(), g::create_bounding_sphere(0, 0, 0, 1)) == true, "isFrustumIntersectingSphere accepts a sphere centered in front of the camera");
  check(g::is_frustum_intersecting_sphere(view_frustum(), g::create_bounding_sphere(0, 0, 20, 1)) == false, "isFrustumIntersectingSphere rejects a sphere entirely behind the camera");
  check(g::is_frustum_intersecting_sphere(view_frustum(), g::create_bounding_sphere(0, 0, 0, -1)) == false, "isFrustumIntersectingSphere rejects an empty sphere (negative radius)");
  check(g::is_frustum_intersecting_sphere(view_frustum(), g::create_bounding_sphere(0, 0, 5.5, 2)) == true, "isFrustumIntersectingSphere accepts a sphere that straddles the near plane");
  {
    const auto near = view_frustum()->near;
    check(close(std::sqrt(near->a * near->a + near->b * near->b + near->c * near->c), 1, 5), "setFrustumFromMatrix4 produces unit-length plane normals");
  }
  {
    const auto f = g::create_frustum();
    g::set_frustum_from_matrix4(f, g::create_matrix4(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0));
    bool finite = true;
    for (const auto& p : {f->near, f->far, f->left, f->right, f->top, f->bottom}) finite = finite && std::isfinite(p->a) && std::isfinite(p->b) && std::isfinite(p->c) && std::isfinite(p->d);
    check(finite, "setFrustumFromMatrix4 writes finite coefficients for a matrix with no frustum in it");
  }
}

using Obb = flight::Ref<types::Obb>;
using Orientation = std::array<double, 4>;

// The orientation arguments createObb and setObb take, spelled as a turn about an axis (upstream's
// orientationFromAxisAngle).
Orientation orientation_from_axis_angle(double x, double y, double z, double radians) {
  const double length = std::sqrt(x * x + y * y + z * z);
  const double half = std::sin(radians / 2);
  return {(x / length) * half, (y / length) * half, (z / length) * half, std::cos(radians / 2)};
}

Obb obb(double cx, double cy, double cz, double hx, double hy, double hz, const Orientation& o) { return g::create_obb(cx, cy, cz, hx, hy, hz, o[0], o[1], o[2], o[3]); }

// A thin rod along one local axis (upstream's createRodObb).
Obb rod_obb(double cx, double cy, double cz, int long_axis, const Orientation& o) {
  const double thickness = 0.1;
  return obb(cx, cy, cz, long_axis == 0 ? 3 : thickness, long_axis == 1 ? 3 : thickness, long_axis == 2 ? 3 : thickness, o);
}

// Three distinct half-extents at an orientation sharing no axis with the world (upstream's createTiltedObb).
Obb tilted_obb() { return obb(0, 0, 0, 3, 2, 1, orientation_from_axis_angle(1, 2, 3, 0.8)); }

Vec3 unit_vector3(int axis) { return v3(axis == 0 ? 1 : 0, axis == 1 ? 1 : 0, axis == 2 ? 1 : 0); }

// rotateMatrix4 expects a unit axis (upstream's rotationMatrix4).
flight::Ref<types::Matrix4> rotation_matrix4(double x, double y, double z, double radians) {
  const double length = std::sqrt(x * x + y * y + z * z);
  const auto m = g::create_matrix4();
  g::rotate_matrix4(m, m, v3(x / length, y / length, z / length), radians);
  return m;
}

// Upstream's expectTransformedObbMatchesRotatedPoints: the closest point on the transformed box equals
// the transformed closest point on the original, for probes all around it, to 4 digits.
bool transformed_obb_matches_rotated_points(const flight::Ref<types::Matrix4>& m, const Obb& source) {
  const auto transformed = g::create_obb(0, 0, 0, 0, 0, 0, 0, 0, 0, 1);
  g::transform_obb_by_matrix4(transformed, source, m);
  const auto expected = v3();
  const auto actual = v3();
  const auto probe = v3();
  bool ok = true;
  for (const auto& p : std::array<std::array<double, 3>, 6>{{{10, 0, 0}, {0, 10, 0}, {0, 0, 10}, {4, -6, 2}, {-5, 3, 8}, {7, 7, -7}}}) {
    g::get_closest_point_on_obb(expected, source, v3(p[0], p[1], p[2]));
    g::matrix4_transform_point(expected, m, expected);
    g::matrix4_transform_point(probe, m, v3(p[0], p[1], p[2]));
    g::get_closest_point_on_obb(actual, transformed, probe);
    ok = ok && close(actual->x, expected->x, 4) && close(actual->y, expected->y, 4) && close(actual->z, expected->z, 4);
  }
  return ok;
}

void test_obb() {
  const Orientation identity = {0, 0, 0, 1};
  {
    const auto o = g::create_obb(1, 2, 3, 4, 5, 6, 0, 0, 0, 1);
    check(same(o->center_x, 1) && same(o->center_y, 2) && same(o->center_z, 3) && same(o->half_extent_x, 4) && same(o->half_extent_y, 5) && same(o->half_extent_z, 6) &&
              same(o->orientation_x, 0) && same(o->orientation_y, 0) && same(o->orientation_z, 0) && same(o->orientation_w, 1),
          "createObb stores center, half-extents, and orientation");
  }
  {
    const auto out = v3();
    g::get_closest_point_on_obb(out, obb(0, 0, 0, 1, 1, 1, identity), v3(5, 0, 0));
    check(close(out->x, 1, 6) && close(out->y, 0, 6) && close(out->z, 0, 6), "getClosestPointOnObb clamps an outside point to the nearest face of an axis-aligned OBB");
  }
  {
    const auto out = v3();
    g::get_closest_point_on_obb(out, obb(0, 0, 0, 2, 2, 2, identity), v3(1, 0.5, -0.5));
    check(close(out->x, 1, 6) && close(out->y, 0.5, 6) && close(out->z, -0.5, 6), "getClosestPointOnObb returns a point inside the OBB unchanged");
  }
  {
    const auto out = v3();
    g::get_closest_point_on_obb(out, obb(10, 0, 0, 1, 1, 1, identity), v3(0, 0, 0));
    check(close(out->x, 9, 6) && close(out->y, 0, 6) && close(out->z, 0, 6), "getClosestPointOnObb handles off-center OBB with identity orientation");
  }
  {
    const auto p = v3(5, 0, 0);
    g::get_closest_point_on_obb(p, obb(0, 0, 0, 1, 1, 1, identity), p);
    check(close(p->x, 1, 6), "getClosestPointOnObb supports out === point");
  }
  FLIGHT_EXPECT_FUNCTION(g::initialize_obb);
  check(close(g::intersect_ray3_dobb(g::create_ray3_d(5, 0, 0, -1, 0, 0), obb(0, 0, 0, 1, 1, 1, identity)), 4, 6), "intersectRay3DObb hits a unit OBB at the expected t from outside");
  check(same(g::intersect_ray3_dobb(g::create_ray3_d(5, 0, 0, 1, 0, 0), obb(0, 0, 0, 1, 1, 1, identity)), -1), "intersectRay3DObb returns -1 for a ray pointing away from the OBB");
  check(same(g::intersect_ray3_dobb(g::create_ray3_d(0, 0, 0, 0, 0, 1), obb(0, 0, 0, 2, 2, 2, identity)), 0), "intersectRay3DObb returns 0 for a ray starting inside the OBB");
  check(same(g::intersect_ray3_dobb(g::create_ray3_d(0, 0, 0, 1, 0, 0), obb(10, 10, 10, 1, 1, 1, identity)), -1), "intersectRay3DObb returns -1 for a ray that misses an off-center OBB");
  {
    const auto o = obb(0, 0, 0, 1, 1, 1, identity);
    check(same(g::intersect_ray3_dobb(g::create_ray3_d(0.5, 0.5, 0.5, 0, 0, 0), o), -1) && same(g::intersect_ray3_dobb(g::create_ray3_d(5, 0, 0, 0, 0, 0), o), -1),
          "intersectRay3DObb returns -1 for a ray with no direction, inside the box or out");
  }
  {
    const auto o = obb(0, 0, 0, 0, 1, 1, identity);
    check(close(g::intersect_ray3_dobb(g::create_ray3_d(5, 0, 0, -1, 0, 0), o), 5, 6) && same(g::intersect_ray3_dobb(g::create_ray3_d(5, 3, 0, -1, 0, 0), o), -1),
          "intersectRay3DObb hits a flat OBB whose half-extent along one axis is zero");
  }
  check(g::is_obb_intersecting_aabb(obb(0, 0, 0, 1, 1, 1, identity), read(g::create_aabb(-1, -1, -1, 1, 1, 1))) == true, "isObbIntersectingAabb returns true for coincident OBB and AABB");
  check(g::is_obb_intersecting_aabb(obb(0.5, 0, 0, 1, 1, 1, identity), read(g::create_aabb(-1, -1, -1, 1, 1, 1))) == true, "isObbIntersectingAabb returns true for overlapping OBB and AABB");
  check(g::is_obb_intersecting_aabb(obb(10, 0, 0, 1, 1, 1, identity), read(g::create_aabb(-1, -1, -1, 1, 1, 1))) == false, "isObbIntersectingAabb returns false for separated OBB and AABB");
  {
    const struct {
      const char* axis;
      Aabb empty;
    } cases[] = {{"x", g::create_aabb(1, 0, 0, 0, 1, 1)}, {"y", g::create_aabb(0, 1, 0, 1, 0, 1)}, {"z", g::create_aabb(0, 0, 1, 1, 1, 0)}};
    for (const auto& c : cases)
      check(g::is_obb_intersecting_aabb(obb(0, 0, 0, 10, 10, 10, identity), read(c.empty)) == false, std::string("isObbIntersectingAabb returns false for an AABB empty on ") + c.axis);
  }
  check(g::is_obb_intersecting_obb(obb(0, 0, 0, 1, 1, 1, identity), obb(0, 0, 0, 1, 1, 1, identity)) == true, "isObbIntersectingObb returns true for two coincident OBBs");
  check(g::is_obb_intersecting_obb(obb(0, 0, 0, 1, 1, 1, identity), obb(1.5, 0, 0, 1, 1, 1, identity)) == true, "isObbIntersectingObb returns true for two overlapping OBBs");
  check(g::is_obb_intersecting_obb(obb(0, 0, 0, 1, 1, 1, identity), obb(10, 0, 0, 1, 1, 1, identity)) == false, "isObbIntersectingObb returns false for two separated OBBs");
  {
    const auto tilt = orientation_from_axis_angle(1, 2, 3, 0.9);
    const auto tilt_as_matrix = rotation_matrix4(1, 2, 3, 0.9);
    const auto first = obb(0, 0, 0, 1, 2, 3, identity);
    const struct {
      int axis;
      bool of_turned_box;
      std::array<double, 3> extents;
      double clear;
    } separations[] = {
        {0, false, {2.5, 1.5, 0.5}, 3.8}, {1, false, {2.5, 1.5, 0.5}, 4.9}, {2, false, {0.5, 1.5, 2.5}, 5.9},
        {0, true, {0.5, 1.5, 2.5}, 3.6},  {1, true, {0.5, 1.5, 2.5}, 4.7},  {2, true, {2.5, 1.5, 0.5}, 3.7},
    };
    bool ok = true;
    for (const auto& s : separations) {
      const auto direction = unit_vector3(s.axis);
      if (s.of_turned_box) g::matrix4_transform_point(direction, tilt_as_matrix, direction);
      const auto away = obb(direction->x * s.clear, direction->y * s.clear, direction->z * s.clear, s.extents[0], s.extents[1], s.extents[2], tilt);
      const auto concentric = obb(0, 0, 0, s.extents[0], s.extents[1], s.extents[2], tilt);
      ok = ok && g::is_obb_intersecting_obb(first, away) == false && g::is_obb_intersecting_obb(first, concentric) == true;
    }
    check(ok, "isObbIntersectingObb separates on each of the six face axes on its own");
  }
  {
    const auto tilt = orientation_from_axis_angle(1, 2, 3, 0.9);
    const auto tilt_as_matrix = rotation_matrix4(1, 2, 3, 0.9);
    const auto rod_long_direction = v3();
    bool ok = true;
    for (int first_long_axis = 0; first_long_axis < 3; ++first_long_axis) {
      for (int second_long_axis = 0; second_long_axis < 3; ++second_long_axis) {
        const auto first = rod_obb(0, 0, 0, first_long_axis, identity);
        g::matrix4_transform_point(rod_long_direction, tilt_as_matrix, unit_vector3(second_long_axis));
        const auto perpendicular = v3();
        g::cross_vector3(perpendicular, unit_vector3(first_long_axis), rod_long_direction);
        g::normalize_vector3(perpendicular, perpendicular);
        const double clear = 0.3;
        const auto separated = rod_obb(perpendicular->x * clear, perpendicular->y * clear, perpendicular->z * clear, second_long_axis, tilt);
        const auto concentric = rod_obb(0, 0, 0, second_long_axis, tilt);
        ok = ok && g::is_obb_intersecting_obb(first, separated) == false && g::is_obb_intersecting_obb(first, concentric) == true;
      }
    }
    check(ok, "isObbIntersectingObb separates on every one of the nine edge-cross axes");
  }
  check(g::is_obb_intersecting_sphere(obb(0, 0, 0, 1, 1, 1, identity), g::create_bounding_sphere(0, 0, 0, 0.5)) == true, "isObbIntersectingSphere returns true when sphere is at the OBB center");
  check(g::is_obb_intersecting_sphere(obb(0, 0, 0, 1, 1, 1, identity), g::create_bounding_sphere(1.5, 0, 0, 1)) == true, "isObbIntersectingSphere returns true when sphere touches an OBB face");
  check(g::is_obb_intersecting_sphere(obb(0, 0, 0, 1, 1, 1, identity), g::create_bounding_sphere(5, 5, 5, 1)) == false, "isObbIntersectingSphere returns false when sphere is outside the OBB");
  check(g::is_obb_intersecting_sphere(obb(0, 0, 0, 10, 10, 10, identity), g::create_bounding_sphere(0, 0, 0, -1)) == false, "isObbIntersectingSphere returns false for a negative-radius sphere");
  {
    const auto o = g::create_obb(0, 0, 0, 0, 0, 0, 0, 0, 0, 1);
    g::set_obb(o, 1, 2, 3, 4, 5, 6, 0.1, 0.2, 0.3, 0.9);
    check(same(o->center_x, 1) && same(o->center_y, 2) && same(o->center_z, 3) && same(o->half_extent_x, 4) && same(o->half_extent_y, 5) && same(o->half_extent_z, 6) &&
              same(o->orientation_x, 0.1) && same(o->orientation_w, 0.9),
          "setObb updates all fields in place");
  }
  {
    const auto out = g::create_obb(0, 0, 0, 0, 0, 0, 0, 0, 0, 1);
    g::transform_obb_by_matrix4(out, obb(0, 0, 0, 1, 1, 1, identity), translation(5, 0, 0));
    check(close(out->center_x, 5, 6) && close(out->center_y, 0, 6) && close(out->half_extent_x, 1, 6) && close(out->orientation_w, 1, 5), "transformObbByMatrix4 translates the center by a translation matrix");
  }
  {
    const auto out = g::create_obb(0, 0, 0, 0, 0, 0, 0, 0, 0, 1);
    g::transform_obb_by_matrix4(out, obb(0, 0, 0, 1, 1, 1, identity), g::create_matrix4(2, 0, 0, 0, 0, 2, 0, 0, 0, 0, 2, 0, 0, 0, 0, 1));
    check(close(out->half_extent_x, 2, 6) && close(out->half_extent_y, 2, 6) && close(out->half_extent_z, 2, 6), "transformObbByMatrix4 scales half-extents by a uniform scale matrix");
  }
  {
    const auto o = obb(1, 0, 0, 1, 1, 1, identity);
    g::transform_obb_by_matrix4(o, o, translation(0, 3, 0));
    check(close(o->center_x, 1, 6) && close(o->center_y, 3, 6) && close(o->half_extent_x, 1, 6), "transformObbByMatrix4 supports out === obb");
  }
  check(transformed_obb_matches_rotated_points(rotation_matrix4(1, 0, 0, 0.7), tilted_obb()) && transformed_obb_matches_rotated_points(rotation_matrix4(1, 2, 3, 0.9), tilted_obb()),
        "transformObbByMatrix4 rotates the box the same way the matrix rotates points");
  {
    const double near_half_turn = M_PI - 0.01;
    check(transformed_obb_matches_rotated_points(rotation_matrix4(1, 0, 0, near_half_turn), tilted_obb()) &&
              transformed_obb_matches_rotated_points(rotation_matrix4(0, 1, 0, near_half_turn), tilted_obb()) &&
              transformed_obb_matches_rotated_points(rotation_matrix4(0, 0, 1, near_half_turn), tilted_obb()),
          "transformObbByMatrix4 rotates the box correctly near a half turn about each axis");
  }
  {
    const auto out = g::create_obb(0, 0, 0, 0, 0, 0, 0, 0, 0, 1);
    g::transform_obb_by_matrix4(out, obb(0, 0, 0, 2, 3, 4, identity), g::create_matrix4(0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1));
    check(same(out->half_extent_x, 0) && close(out->half_extent_y, 3, 6) && close(out->half_extent_z, 4, 6) && close(out->orientation_w, 1, 6),
          "transformObbByMatrix4 flattens the box along a collapsed matrix column and keeps the other extents");
  }
  {
    const auto tilt = orientation_from_axis_angle(1, 1, 1, 0.6);
    const auto out = g::create_obb(0, 0, 0, 0, 0, 0, 0, 0, 0, 1);
    g::transform_obb_by_matrix4(out, obb(1, 2, 3, 2, 3, 4, tilt), g::create_matrix4(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 6, 7, 1));
    check(close(out->center_x, 5, 6) && close(out->center_y, 6, 6) && close(out->center_z, 7, 6) && same(out->half_extent_x, 0) && same(out->half_extent_y, 0) &&
              same(out->half_extent_z, 0) && close(out->orientation_x, tilt[0], 6) && close(out->orientation_y, tilt[1], 6) && close(out->orientation_z, tilt[2], 6) &&
              close(out->orientation_w, tilt[3], 6),
          "transformObbByMatrix4 collapses the box to a point at the matrix position when every column is zero");
  }
}

using Ray = flight::Ref<types::Ray3D>;

bool ray_fields(const Ray& r, double ox, double oy, double oz, double dx, double dy, double dz) { return xyz(r->origin, ox, oy, oz) && xyz(r->direction, dx, dy, dz); }

bool finite3(const Vec3& v) { return std::isfinite(v->x) && std::isfinite(v->y) && std::isfinite(v->z); }

// Six rays, one per side of the box (-1, -4, -6)..(2, 3, 5), each with its own entry distance
// (upstream's RAYS_INTO_LOPSIDED_BOX).
const std::array<std::array<double, 7>, 6> rays_into_lopsided_box = {{
    {-10, 0, 0, 1, 0, 0, 9},
    {10, 0, 0, -1, 0, 0, 8},
    {0, -10, 0, 0, 1, 0, 6},
    {0, 10, 0, 0, -1, 0, 7},
    {0, 0, -10, 0, 0, 1, 4},
    {0, 0, 10, 0, 0, -1, 5},
}};

void test_ray3d() {
  check(ray_fields(g::create_ray3_d(), 0, 0, 0, 0, 0, 1), "createRay3D creates a ray at the origin pointing in +Z by default");
  check(ray_fields(g::create_ray3_d(1, 2, 3, 0, 1, 0), 1, 2, 3, 0, 1, 0), "createRay3D creates a ray with explicit origin and direction");
  {
    const auto pa = v3();
    const auto pb = v3();
    g::get_closest_point_between_ray3_ds(pa, pb, g::create_ray3_d(0, 0, 0, 1, 0, 0), g::create_ray3_d(2, 0, 1, 0, 1, 0));
    check(close(pa->x, 2, 6) && close(pa->y, 0, 6) && close(pa->z, 0, 6) && close(pb->x, 2, 6) && close(pb->y, 0, 6) && close(pb->z, 1, 6),
          "getClosestPointBetweenRay3Ds finds the closest points between two skew rays");
  }
  {
    const auto pa = v3();
    const auto pb = v3();
    g::get_closest_point_between_ray3_ds(pa, pb, g::create_ray3_d(0, 0, 0, 1, 0, 0), g::create_ray3_d(-1, 1, 0, -1, 0, 0));
    check(close(pa->x, 0, 6) && close(pa->y, 0, 6) && close(pb->x, -1, 6) && close(pb->y, 1, 6), "getClosestPointBetweenRay3Ds clamps to the origins when both rays point away from each other");
  }
  {
    const auto pa = v3();
    const auto pb = v3();
    g::get_closest_point_between_ray3_ds(pa, pb, g::create_ray3_d(0, 0, 0, 1, 0, 0), g::create_ray3_d(0, 2, 0, 1, 0, 0));
    check(close(std::hypot(pa->x - pb->x, pa->y - pb->y, pa->z - pb->z), 2, 6), "getClosestPointBetweenRay3Ds handles parallel rays by anchoring on ray a");
  }
  {
    const auto pa = v3();
    const auto pb = v3();
    g::get_closest_point_between_ray3_ds(pa, pb, g::create_ray3_d(0, 0, 0, 1, 0, 0), g::create_ray3_d(-5, 1, 0, 0, 0, 1));
    check(close(pa->x, 0, 6) && close(pa->y, 0, 6) && close(pa->z, 0, 6) && close(pb->x, -5, 6) && close(pb->y, 1, 6) && close(pb->z, 0, 6),
          "getClosestPointBetweenRay3Ds clamps to the origin of the first ray when its nearest approach is behind it");
  }
  {
    const auto pa = v3();
    const auto pb = v3();
    g::get_closest_point_between_ray3_ds(pa, pb, g::create_ray3_d(5, 0, 0, 1, 0, 0), g::create_ray3_d(0, 3, 0, 0, 1, 0));
    check(close(pa->x, 5, 6) && close(pa->y, 0, 6) && close(pb->x, 0, 6) && close(pb->y, 3, 6), "getClosestPointBetweenRay3Ds clamps both rays to their origins when each nearest approach is behind the other");
  }
  {
    const auto pa = v3();
    const auto pb = v3();
    g::get_closest_point_between_ray3_ds(pa, pb, g::create_ray3_d(0, 0, 0, 1, 0, 0), g::create_ray3_d(5, 1, 0, 0, 0, 0));
    bool ok = finite3(pa) && same(pb->x, 5) && same(pb->y, 1);
    g::get_closest_point_between_ray3_ds(pa, pb, g::create_ray3_d(0, 0, 0, 0, 0, 0), g::create_ray3_d(0, 0, 5, 0, 0, 1));
    ok = ok && xyz(pa, 0, 0, 0) && finite3(pb);
    check(ok, "getClosestPointBetweenRay3Ds writes finite points when either direction has no length");
  }
  {
    const auto out = v3();
    g::get_closest_point_on_ray3_d(out, g::create_ray3_d(0, 0, 0, 1, 0, 0), v3(3, 4, 0));
    check(close(out->x, 3, 6) && close(out->y, 0, 6) && close(out->z, 0, 6), "getClosestPointOnRay3D projects a point onto the ray");
  }
  {
    const auto out = v3();
    g::get_closest_point_on_ray3_d(out, g::create_ray3_d(0, 0, 0, 1, 0, 0), v3(-5, 2, 0));
    check(close(out->x, 0, 6) && close(out->y, 0, 6) && close(out->z, 0, 6), "getClosestPointOnRay3D clamps to the origin when the point projects behind it");
  }
  {
    const auto out = v3();
    g::get_closest_point_on_ray3_d(out, g::create_ray3_d(0, 0, 0, 2, 0, 0), v3(3, 7, 0));
    check(close(out->x, 3, 6) && close(out->y, 0, 6), "getClosestPointOnRay3D works with a non-normalized direction");
  }
  {
    const auto p = v3(5, 4, 1);
    g::get_closest_point_on_ray3_d(p, g::create_ray3_d(0, 0, 0, 0, 1, 0), p);
    check(close(p->x, 0, 6) && close(p->y, 4, 6) && close(p->z, 0, 6), "getClosestPointOnRay3D supports out === point");
  }
  {
    const auto out = v3();
    g::get_closest_point_on_ray3_d(out, g::create_ray3_d(1, 2, 3, 0, 0, 0), v3(9, 9, 9));
    check(xyz(out, 1, 2, 3), "getClosestPointOnRay3D writes the origin for a ray with no direction");
  }
  {
    const auto out = v3();
    g::get_ray3_dpoint_at(out, g::create_ray3_d(1, 2, 3, 0, 0, 1), 0);
    check(xyz(out, 1, 2, 3), "getRay3DPointAt returns the origin when t=0");
    g::get_ray3_dpoint_at(out, g::create_ray3_d(0, 0, 0, 1, 0, 0), 5);
    check(xyz(out, 5, 0, 0), "getRay3DPointAt advances along the direction by t");
    const auto ray = g::create_ray3_d(1, 2, 3, 0, 1, 0);
    g::get_ray3_dpoint_at(ray->origin, ray, 3);
    check(xyz(ray->origin, 1, 5, 3), "getRay3DPointAt is safe when out aliases ray.origin");
  }
  FLIGHT_EXPECT_FUNCTION(g::initialize_ray3_d);
  const auto unit_box = [] { return read(g::create_aabb(-1, -1, -1, 1, 1, 1)); };
  check(same(g::intersect_ray3_daabb(g::create_ray3_d(0, 0, 0, 0, 0, 1), unit_box()), 0), "intersectRay3DAabb returns t=0 when the ray origin is inside the box");
  check(close(g::intersect_ray3_daabb(g::create_ray3_d(0, 0, -5, 0, 0, 1), unit_box()), 4), "intersectRay3DAabb returns the entry t for a ray hitting the box from outside");
  check(same(g::intersect_ray3_daabb(g::create_ray3_d(5, 0, -5, 0, 0, 1), unit_box()), -1), "intersectRay3DAabb returns -1 when the ray misses the box");
  check(same(g::intersect_ray3_daabb(g::create_ray3_d(0, 0, 5, 0, 0, 1), unit_box()), -1), "intersectRay3DAabb returns -1 when the ray points away from the box");
  check(close(g::intersect_ray3_daabb(g::create_ray3_d(-5, 0, 0, 1, 0, 0), unit_box()), 4), "intersectRay3DAabb handles axis-aligned rays");
  {
    const struct {
      const char* axis;
      Ray ray;
      Aabb empty;
    } cases[] = {
        {"x", g::create_ray3_d(5, 1.5, 1.5, -1, 0, 0), g::create_aabb(2, 1, 1, 1, 2, 2)},
        {"y", g::create_ray3_d(1.5, 5, 1.5, 0, -1, 0), g::create_aabb(1, 2, 1, 2, 1, 2)},
        {"z", g::create_ray3_d(1.5, 1.5, 5, 0, 0, -1), g::create_aabb(1, 1, 2, 2, 2, 1)},
    };
    for (const auto& c : cases) check(same(g::intersect_ray3_daabb(c.ray, read(c.empty)), -1), std::string("intersectRay3DAabb returns -1 for an AABB empty on ") + c.axis);
  }
  check(same(g::intersect_ray3_daabb(g::create_ray3_d(0, 0, 0, 0, 0, 0), unit_box()), -1) && same(g::intersect_ray3_daabb(g::create_ray3_d(5, 0, 0, 0, 0, 0), unit_box()), -1),
        "intersectRay3DAabb returns -1 for a ray with no direction, inside the box or out");
  {
    const auto lopsided = g::create_aabb(-1, -4, -6, 2, 3, 5);
    bool entering = true, away = true;
    for (const auto& r : rays_into_lopsided_box) {
      entering = entering && close(g::intersect_ray3_daabb(g::create_ray3_d(r[0], r[1], r[2], r[3], r[4], r[5]), read(lopsided)), r[6], 6);
      away = away && same(g::intersect_ray3_daabb(g::create_ray3_d(r[0], r[1], r[2], -r[3], -r[4], -r[5]), read(lopsided)), -1);
    }
    check(entering, "intersectRay3DAabb enters each of the six sides at that side's own distance");
    check(away, "intersectRay3DAabb returns -1 for a ray aimed away from the box along any axis");
    check(same(g::intersect_ray3_daabb(g::create_ray3_d(10, 0, 0, 0, 1, 0), read(lopsided)), -1) && same(g::intersect_ray3_daabb(g::create_ray3_d(0, 10, 0, 0, 0, 1), read(lopsided)), -1) &&
              same(g::intersect_ray3_daabb(g::create_ray3_d(0, 0, 10, 1, 0, 0), read(lopsided)), -1),
          "intersectRay3DAabb returns -1 for a ray running alongside the box on any axis");
  }
  check(close(g::intersect_ray3_dplane(g::create_ray3_d(0, 0, -3, 0, 0, 1), g::create_plane(0, 0, 1, 0)), 3), "intersectRay3DPlane returns the t of a simple XY-plane intersection");
  check(same(g::intersect_ray3_dplane(g::create_ray3_d(0, 0, 1, 1, 0, 0), g::create_plane(0, 0, 1, 0)), -1), "intersectRay3DPlane returns -1 when ray is parallel to the plane");
  check(same(g::intersect_ray3_dplane(g::create_ray3_d(0, 0, 3, 0, 0, 1), g::create_plane(0, 0, 1, 0)), -1), "intersectRay3DPlane returns -1 when intersection is behind the origin");
  for (const double scale : {1.0, 1e-6, 1e-12})
    check(close(g::intersect_ray3_dplane(g::create_ray3_d(0, 0, -6, 0, 0, 2), g::create_plane(0, 0, scale, 3 * scale)), 1.5, 10),
          "intersectRay3DPlane is invariant when the plane equation is scaled by " + std::to_string(scale));
  check(same(g::intersect_ray3_dplane(g::create_ray3_d(0, 0, -3, 0, 0, 1), g::create_plane()), -1) && same(g::intersect_ray3_dplane(g::create_ray3_d(0, 0, -3, 0, 0, 0), g::create_plane(0, 0, 1, 0)), -1),
        "intersectRay3DPlane returns -1 for a zero normal or zero direction");
  check(close(g::intersect_ray3_dsphere(g::create_ray3_d(0, 0, -5, 0, 0, 1), g::create_bounding_sphere(0, 0, 0, 1)), 4), "intersectRay3DSphere returns the near t for a ray hitting the sphere");
  check(same(g::intersect_ray3_dsphere(g::create_ray3_d(0, 0, 0, 0, 0, 1), g::create_bounding_sphere(0, 0, 0, 2)), 0), "intersectRay3DSphere returns 0 when the ray origin is inside the sphere");
  check(same(g::intersect_ray3_dsphere(g::create_ray3_d(5, 0, -5, 0, 0, 1), g::create_bounding_sphere(0, 0, 0, 1)), -1), "intersectRay3DSphere returns -1 when the ray misses the sphere");
  check(same(g::intersect_ray3_dsphere(g::create_ray3_d(0, 0, -5, 0, 0, 1), g::create_bounding_sphere(0, 0, 0, -1)), -1), "intersectRay3DSphere returns -1 for an empty sphere (radius < 0)");
  check(same(g::intersect_ray3_dsphere(g::create_ray3_d(5, 0, 0, 1, 0, 0), g::create_bounding_sphere(0, 0, 0, 1)), -1), "intersectRay3DSphere returns -1 when the sphere is squarely behind the ray");
  check(same(g::intersect_ray3_dsphere(g::create_ray3_d(0, 0, 0, 0, 0, 0), g::create_bounding_sphere(0, 0, 0, 1)), -1), "intersectRay3DSphere returns -1 for a ray with no direction");
  {
    const auto a = v3(-1, 0, 0);
    const auto b = v3(1, 0, 0);
    const auto c = v3(0, 1, 0);
    check(close(g::intersect_ray3_dtriangle(g::create_ray3_d(0, 0.3, -3, 0, 0, 1), a, b, c), 3), "intersectRay3DTriangle returns the t for a ray hitting the triangle");
    check(same(g::intersect_ray3_dtriangle(g::create_ray3_d(5, 5, -3, 0, 0, 1), a, b, c), -1), "intersectRay3DTriangle returns -1 when the ray misses the triangle");
    check(same(g::intersect_ray3_dtriangle(g::create_ray3_d(0, 0.3, 3, 0, 0, 1), a, b, c), -1), "intersectRay3DTriangle returns -1 when the ray hits behind the origin");
    check(close(g::intersect_ray3_dtriangle(g::create_ray3_d(0, 0.3, 3, 0, 0, -1), a, b, c), 3), "intersectRay3DTriangle tests both sides of the triangle (no back-face culling)");
    {
      const auto p = v3(0, 0, 0);
      check(same(g::intersect_ray3_dtriangle(g::create_ray3_d(0, 0, -1, 0, 0, 1), p, p, p), -1), "intersectRay3DTriangle returns -1 for a degenerate (zero-area) triangle");
    }
    for (const double scale : {1.0, 1e-6, 1e-12})
      check(close(g::intersect_ray3_dtriangle(g::create_ray3_d(0, 0.3, -3, 0, 0, scale), a, b, c) * scale, 3, 10),
            "intersectRay3DTriangle accepts a non-normalized direction scaled by " + std::to_string(scale));
    for (const double scale : {1.0, 1e-3, 1e-6})
      check(close(g::intersect_ray3_dtriangle(g::create_ray3_d(0, 0.3 * scale, -3, 0, 0, 1), v3(-scale, 0, 0), v3(scale, 0, 0), v3(0, scale, 0)), 3, 10),
            "intersectRay3DTriangle does not treat a triangle scaled by " + std::to_string(scale) + " as degenerate");
    check(same(g::intersect_ray3_dtriangle(g::create_ray3_d(0, 0.3, -3, 0, 0, 0), a, b, c), -1), "intersectRay3DTriangle returns -1 for a zero direction");
    check(same(g::intersect_ray3_dtriangle(g::create_ray3_d(-1, 0.3, -1e-12, 1, 0, 1e-12), a, b, c), -1), "intersectRay3DTriangle returns -1 for a nonzero direction nearly parallel to the triangle");
    check(same(g::intersect_ray3_dtriangle(g::create_ray3_d(0.9, 0.9, -3, 0, 0, 1), a, b, c), -1), "intersectRay3DTriangle returns -1 for points inside the triangle's span but past its far edge");
    check(same(g::intersect_ray3_dtriangle(g::create_ray3_d(0, -0.5, -3, 0, 0, 1), a, b, c), -1), "intersectRay3DTriangle returns -1 for points on the far side of the base");
    check(same(g::intersect_ray3_dtriangle(g::create_ray3_d(3, 0, -3, 0, 0, 1), a, b, c), -1) && same(g::intersect_ray3_dtriangle(g::create_ray3_d(0, 3, -3, 0, 0, 1), a, b, c), -1),
          "intersectRay3DTriangle returns -1 for points beyond either end of the first edge");
  }
  for (const double scale : {1.0, 1e-6, 1e-12}) {
    const auto ray = g::create_ray3_d(5, 0, 0, -scale, 0, 0);
    const double hits[] = {
        g::intersect_ray3_daabb(ray, unit_box()),
        g::intersect_ray3_dsphere(ray, g::create_bounding_sphere(0, 0, 0, 1)),
        g::intersect_ray3_dcapsule(ray, g::create_capsule(0, -1, 0, 0, 1, 0, 1)),
        g::intersect_ray3_dobb(ray, g::create_obb(0, 0, 0, 1, 1, 1, 0, 0, 0, 1)),
        g::intersect_ray3_dplane(ray, g::create_plane(1, 0, 0, -1)),
        g::intersect_ray3_dtriangle(ray, v3(1, -1, -1), v3(1, 1, -1), v3(1, 0, 1)),
    };
    bool ok = true;
    for (const double t : hits) ok = ok && close(t * scale, 4, 10);
    check(ok, "ray-shape direction scale parity preserves the hit point across all six shapes at scale " + std::to_string(scale));
  }
  {
    const auto ray = g::create_ray3_d();
    g::set_ray3_d(ray, v3(4, 5, 6), v3(0, 0, -1));
    check(ray_fields(ray, 4, 5, 6, 0, 0, -1), "setRay3D writes origin and direction into an existing ray");
  }
  {
    const auto ray = g::create_ray3_d(1, 2, 3, 0, 1, 0);
    g::set_ray3_d(ray, ray->origin, v3(1, 0, 0));
    check(ray_fields(ray, 1, 2, 3, 1, 0, 0), "setRay3D is safe when origin aliases the ray origin");
  }
  {
    const auto ray = g::create_ray3_d(0, 0, 0, 0, 1, 0);
    g::set_ray3_d(ray, v3(5, 6, 7), ray->direction);
    check(ray_fields(ray, 5, 6, 7, 0, 1, 0), "setRay3D is safe when direction aliases the ray direction");
  }
  {
    const auto out = g::create_ray3_d();
    g::transform_ray3_dby_matrix4(out, g::create_ray3_d(1, 2, 3, 0, 0, 1), g::create_matrix4());
    check(close(out->origin->x, 1) && close(out->origin->y, 2) && close(out->origin->z, 3) && close(out->direction->x, 0) && close(out->direction->y, 0) && close(out->direction->z, 1),
          "transformRay3DByMatrix4 leaves the ray unchanged under the identity matrix");
  }
  {
    const auto m = g::create_matrix4();
    g::translate_matrix4(m, m, 5, 6, 7);
    const auto out = g::create_ray3_d();
    g::transform_ray3_dby_matrix4(out, g::create_ray3_d(0, 0, 0, 0, 1, 0), m);
    check(close(out->origin->x, 5) && close(out->origin->y, 6) && close(out->origin->z, 7) && close(out->direction->x, 0) && close(out->direction->y, 1) && close(out->direction->z, 0),
          "transformRay3DByMatrix4 translates the origin but not the direction");
  }
  {
    const auto m = g::create_matrix4();
    g::translate_matrix4(m, m, 10, 0, 0);
    const auto ray = g::create_ray3_d(1, 0, 0, 0, 0, 1);
    g::transform_ray3_dby_matrix4(ray, ray, m);
    check(close(ray->origin->x, 11) && close(ray->origin->y, 0) && close(ray->origin->z, 0) && close(ray->direction->x, 0) && close(ray->direction->y, 0) && close(ray->direction->z, 1),
          "transformRay3DByMatrix4 is alias-safe when out is the same ray");
  }
}

// The shared acquire/reuse/LIFO/clear/null-release cases of a pool's test file. `noun` is the word the
// upstream titles use (vector, matrix, rectangle). The pool is cleared before each case, as the
// upstream beforeEach does.
template <typename Acquire, typename Release, typename Clear>
void check_pool_lifecycle(const std::string& type, const std::string& noun, const std::string& plural, Acquire acquire, Release release, Clear clear) {
  clear();
  check(acquire() != nullptr, "acquire" + type + " returns a new " + type + " when pool is empty");
  clear();
  {
    const auto first = acquire();
    release(first);
    check(acquire() == first, "acquire" + type + " reuses released " + plural);
  }
  clear();
  {
    const auto first = acquire();
    const auto second = acquire();
    release(first);
    release(second);
    const auto a = acquire();
    const auto b = acquire();
    check(a == second && b == first, "acquire" + type + " reuses released " + plural + " in last-in-first-out order");
  }
  clear();
  {
    const auto first = acquire();
    release(first);
    clear();
    check(acquire() != first, "clear" + type + "Pool empties the pool so the next get allocates fresh");
  }
  clear();
  {
    bool threw = false;
    try {
      release(nullptr);
    } catch (...) {
      threw = true;
    }
    check(!threw, "release" + type + " handles null safely");
  }
  clear();
  (void)noun;
}

bool matrix3_identity(const flight::Ref<types::Matrix3>& m) {
  bool ok = true;
  for (int row = 0; row < 3; ++row)
    for (int column = 0; column < 3; ++column) ok = ok && same(g::get_matrix3_element(m, row, column), row == column ? 1 : 0);
  return ok;
}

void test_pools() {
  check_pool_lifecycle("Vector2", "vector", "vectors", g::acquire_vector2, g::release_vector2, g::clear_vector2_pool);
  check_pool_lifecycle("Vector3", "vector", "vectors", g::acquire_vector3, g::release_vector3, g::clear_vector3_pool);
  check_pool_lifecycle("Vector4", "vector", "vectors", g::acquire_vector4, g::release_vector4, g::clear_vector4_pool);
  check_pool_lifecycle("Matrix", "matrix", "matrices", g::acquire_matrix, g::release_matrix, g::clear_matrix_pool);
  check_pool_lifecycle("Matrix3", "matrix", "matrices", g::acquire_matrix3, g::release_matrix3, g::clear_matrix3_pool);
  check_pool_lifecycle("Matrix4", "matrix", "matrices", g::acquire_matrix4, g::release_matrix4, g::clear_matrix4_pool);
  check_pool_lifecycle("Rectangle", "rectangle", "rectangles", g::acquire_rectangle, g::release_rectangle, g::clear_rectangle_pool);

  g::clear_vector2_pool();
  {
    const auto v = g::acquire_empty_vector2();
    check(same(v->x, 0) && same(v->y, 0), "acquireEmptyVector2 returns a vector with all components set to 0");
    g::clear_vector2_pool();
    const auto v1 = g::acquire_vector2();
    v1->x = 5;
    v1->y = 10;
    g::release_vector2(v1);
    const auto v2 = g::acquire_empty_vector2();
    check(v2 == v1 && same(v2->x, 0) && same(v2->y, 0), "acquireEmptyVector2 resets a released vector to zero");
  }
  g::clear_vector3_pool();
  {
    const auto v = g::acquire_empty_vector3();
    check(xyz(v, 0, 0, 0), "acquireEmptyVector3 returns a vector with all components set to 0");
    g::clear_vector3_pool();
    const auto v1 = g::acquire_vector3();
    v1->x = 5;
    v1->y = 10;
    v1->z = 15;
    g::release_vector3(v1);
    const auto v2 = g::acquire_empty_vector3();
    check(v2 == v1 && xyz(v2, 0, 0, 0), "acquireEmptyVector3 resets a released vector to zero");
  }
  g::clear_vector4_pool();
  {
    const auto v = g::acquire_empty_vector4();
    check(same(v->x, 0) && same(v->y, 0) && same(v->z, 0) && same(v->w, 0), "acquireEmptyVector4 returns a vector with all components set to 0");
    g::clear_vector4_pool();
    const auto v1 = g::acquire_vector4();
    v1->x = 5;
    v1->y = 10;
    v1->z = 15;
    v1->w = 20;
    g::release_vector4(v1);
    const auto v2 = g::acquire_empty_vector4();
    check(v2 == v1 && same(v2->x, 0) && same(v2->y, 0) && same(v2->z, 0) && same(v2->w, 0), "acquireEmptyVector4 resets a released vector to zero");
  }
  g::clear_matrix_pool();
  {
    const auto m = g::acquire_identity_matrix();
    check(same(m->a, 1) && same(m->b, 0) && same(m->c, 0) && same(m->d, 1) && same(m->tx, 0) && same(m->ty, 0), "acquireIdentityMatrix returns a matrix set to identity");
    g::clear_matrix_pool();
    const auto m1 = g::acquire_matrix();
    m1->a = 5;
    m1->tx = 10;
    g::release_matrix(m1);
    const auto m2 = g::acquire_identity_matrix();
    check(m2 == m1 && same(m2->a, 1) && same(m2->tx, 0), "acquireIdentityMatrix resets a released matrix to identity");
  }
  g::clear_matrix3_pool();
  {
    check(matrix3_identity(g::acquire_identity_matrix3()), "acquireIdentityMatrix3 returns a matrix set to identity");
    g::clear_matrix3_pool();
    const auto m1 = g::acquire_matrix3();
    m1->m[0] = 5;
    m1->m[2] = 10;
    g::release_matrix3(m1);
    const auto m2 = g::acquire_identity_matrix3();
    check(m2 == m1 && same(g::get_matrix3_element(m2, 0, 0), 1) && same(g::get_matrix3_element(m2, 0, 2), 0), "acquireIdentityMatrix3 resets a released matrix to identity");
  }
  g::clear_matrix4_pool();
  {
    const auto m = g::acquire_identity_matrix4();
    check(same(g::get_matrix4_element(m, 0, 0), 1) && same(g::get_matrix4_element(m, 0, 1), 0) && same(g::get_matrix4_element(m, 1, 1), 1) && same(g::get_matrix4_element(m, 2, 2), 1) &&
              same(g::get_matrix4_element(m, 3, 3), 1) && same(g::get_matrix4_element(m, 0, 3), 0) && same(g::get_matrix4_element(m, 3, 0), 0),
          "acquireIdentityMatrix4 returns a matrix set to identity");
    g::clear_matrix4_pool();
    const auto m1 = g::acquire_matrix4();
    m1->m[0] = 5;
    m1->m[12] = 10;
    g::release_matrix4(m1);
    const auto m2 = g::acquire_identity_matrix4();
    check(m2 == m1 && same(g::get_matrix4_element(m2, 0, 0), 1) && same(g::get_matrix4_element(m2, 3, 0), 0), "acquireIdentityMatrix4 resets a released matrix to identity");
  }
  g::clear_rectangle_pool();
  {
    const auto r = g::acquire_empty_rectangle();
    check(same(r->x, 0) && same(r->y, 0) && same(r->width, 0) && same(r->height, 0), "acquireEmptyRectangle returns a rectangle with all properties set to 0");
    g::clear_rectangle_pool();
    const auto r1 = g::acquire_rectangle();
    r1->x = 5;
    r1->y = 10;
    r1->width = 50;
    r1->height = 100;
    g::release_rectangle(r1);
    const auto r2 = g::acquire_empty_rectangle();
    check(r2 == r1 && same(r2->x, 0) && same(r2->y, 0) && same(r2->width, 0) && same(r2->height, 0), "acquireEmptyRectangle resets a released rectangle to empty");
  }
  g::clear_rectangle_pool();

  // quaternionPool.test.ts words its lifecycle cases differently, so they are written out.
  g::clear_quaternion_pool();
  {
    const auto q = g::acquire_identity_quaternion();
    check(same(q->x, 0) && same(q->y, 0) && same(q->z, 0) && same(q->w, 1), "acquireIdentityQuaternion returns an identity quaternion");
    g::release_quaternion(q);
  }
  g::clear_quaternion_pool();
  {
    const auto q = g::acquire_quaternion();
    q->x = 9;
    q->w = 0;
    g::release_quaternion(q);
    const auto reused = g::acquire_identity_quaternion();
    check(reused == q && same(reused->x, 0) && same(reused->w, 1), "acquireIdentityQuaternion resets a recycled quaternion to identity");
  }
  g::clear_quaternion_pool();
  {
    const auto q = g::acquire_quaternion();
    g::release_quaternion(q);
    check(g::acquire_quaternion() == q, "acquireQuaternion reuses a released quaternion");
  }
  g::clear_quaternion_pool();
  {
    const auto a = g::acquire_quaternion();
    const auto b = g::acquire_quaternion();
    check(a != b, "acquireQuaternion allocates when the pool is empty");
  }
  g::clear_quaternion_pool();
  {
    const auto q = g::acquire_quaternion();
    g::release_quaternion(q);
    g::clear_quaternion_pool();
    check(g::acquire_quaternion() != q, "clearQuaternionPool empties the pool so acquire allocates fresh");
  }
  g::clear_quaternion_pool();
  {
    bool threw = false;
    try {
      g::release_quaternion(nullptr);
    } catch (...) {
      threw = true;
    }
    check(!threw && g::acquire_quaternion() != nullptr, "releaseQuaternion ignores a falsy value");
  }
  g::clear_quaternion_pool();
}

void sample_guard(flight::String) {}

// Every pool's clear, as upstream's clearPools.
void clear_pools() {
  g::clear_matrix_pool();
  g::clear_matrix3_pool();
  g::clear_matrix4_pool();
  g::clear_quaternion_pool();
  g::clear_rectangle_pool();
  g::clear_vector2_pool();
  g::clear_vector3_pool();
  g::clear_vector4_pool();
}

void test_pool_guards() {
  {
    g::set_geometry_pool_release_guard(g::GeometryPoolReleaseGuard(&sample_guard));
    // toBe on a function is identity. A std::function holding a function pointer exposes it through
    // target(), so this compares the installed callback against the exact function installed.
    const auto* installed = g::geometry_pool_release_guard.has_value() ? g::geometry_pool_release_guard->target<void (*)(flight::String)>() : nullptr;
    const bool installed_is_guard = installed != nullptr && *installed == &sample_guard;
    g::set_geometry_pool_release_guard(std::nullopt);
    check(installed_is_guard && !g::geometry_pool_release_guard.has_value(), "setGeometryPoolReleaseGuard installs and removes the internal release callback");
  }

  flight::log::clear_log_once_keys();
  clear_pools();
  {
    const bool before = g::are_geometry_pool_guards_enabled();
    g::enable_geometry_pool_guards();
    const bool enabled = g::are_geometry_pool_guards_enabled();
    g::disable_geometry_pool_guards();
    check(before == false && enabled == true && g::are_geometry_pool_guards_enabled() == false, "areGeometryPoolGuardsEnabled reports whether the pool release diagnostic is installed");
  }
  g::disable_geometry_pool_guards();
  clear_pools();

}


}  // namespace

int main() {
  test_aabb();
  test_bounding_sphere();
  test_capsule();
  test_plane();
  test_frustum();
  test_obb();
  test_ray3d();
  test_pools();
  test_pool_guards();
  return failures == 0 ? 0 : 1;
}
