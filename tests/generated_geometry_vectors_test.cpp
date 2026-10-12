#include <flight/geometry/vector2.hpp>
#include <flight/geometry/vector3.hpp>
#include <flight/geometry/vector4.hpp>

#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <string>
#include <type_traits>
#include <vector>

// Native port of packages/geometry/src/vector2.test.ts, vector3.test.ts and vector4.test.ts at a627849
// (314 cases) against the generated headers (geometry has no override). Each check names the upstream case
// it ports. The three initialize* cases assert only `typeof === 'function'`, which the C++ declaration
// establishes, so they are static_asserts. Upstream's vector-like literals are built as the same Vector
// carrier without passing through create*.
//
// Excluded (2), not callable natively:
// - projectVector3 "supports out === source" passes a Vector3 as the Vector2Like out. project_vector3's
//   out parameter is flight::types::Vector2Like (flight::Ref<Vector2>, flight/geometry/vector3.hpp:251), and
//   a Vector3 owner has no conversion to it: the nominal Vector2/Vector3 structs share no heritage, so the
//   structurally typed aliasing call does not compile.
// - projectVector4 "supports out === source" likewise passes a Vector4 as the Vector3Like out of
//   project_vector4 (flight/geometry/vector4.hpp), which a Vector4 owner cannot convert to.

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

const double pi = M_PI;
const double nan = std::numeric_limits<double>::quiet_NaN();

using V2 = flight::Ref<types::Vector2>;
using V3 = flight::Ref<types::Vector3>;
using V4 = flight::Ref<types::Vector4>;

V2 v2(double x = 0, double y = 0) { return g::create_vector2(x, y); }
V3 v3(double x = 0, double y = 0, double z = 0) { return g::create_vector3(x, y, z); }
V4 v4(double x = 0, double y = 0, double z = 0, double w = 0) { return g::create_vector4(x, y, z, w); }

// Upstream's "vector-like object" cases pass plain literals. Natively a literal of that shape is the same
// Vector carrier, so these are built directly rather than through create*, which is the closest a
// native caller gets to a literal that never passed through the package's constructor.
V2 lit2(double x, double y) { return flight::make_ref<types::Vector2>(types::Vector2{.entity_runtime_key = std::nullopt, .x = x, .y = y}); }
V3 lit3(double x, double y, double z) { return flight::make_ref<types::Vector3>(types::Vector3{.entity_runtime_key = std::nullopt, .x = x, .y = y, .z = z}); }
V4 lit4(double x, double y, double z, double w) {
  return flight::make_ref<types::Vector4>(types::Vector4{.entity_runtime_key = std::nullopt, .x = x, .y = y, .z = z, .w = w});
}

bool is2(const V2& v, double x, double y) { return same(v->x, x) && same(v->y, y); }
bool is3(const V3& v, double x, double y, double z) { return same(v->x, x) && same(v->y, y) && same(v->z, z); }
bool is4(const V4& v, double x, double y, double z, double w) { return same(v->x, x) && same(v->y, y) && same(v->z, z) && same(v->w, w); }
bool near2(const V2& v, double x, double y, int digits = 2) { return close(v->x, x, digits) && close(v->y, y, digits); }
bool near3(const V3& v, double x, double y, double z, int digits = 2) { return close(v->x, x, digits) && close(v->y, y, digits) && close(v->z, z, digits); }
bool near4(const V4& v, double x, double y, double z, double w, int digits = 2) {
  return close(v->x, x, digits) && close(v->y, y, digits) && close(v->z, z, digits) && close(v->w, w, digits);
}

const flight::Null null_value = flight::null;
const flight::Undefined undefined_value = flight::undefined;

void test_vector2() {
  // add
  {
    auto pt = v2(2, 10), pt2 = v2(4, 20), result = v2();
    g::add_vector2(result, pt, pt2);
    check(is2(result, 6, 30) && result != pt && result != pt2, "addVector2 adds two coordinates");
  }
  {
    auto pt = v2(3, 7), pt2 = v2(5, 10);
    g::add_vector2(pt, pt, pt2);
    check(is2(pt, 8, 17) && is2(pt2, 5, 10), "addVector2 works correctly if first vector is out");
  }
  {
    auto pt = v2(2, 10), pt2 = v2(4, 20);
    g::add_vector2(pt2, pt, pt2);
    check(is2(pt, 2, 10) && is2(pt2, 6, 30), "addVector2 works correctly if second vector is out");
  }
  {
    auto pt = lit2(2, 10), pt2 = lit2(4, 20), result = lit2(0, 0);
    g::add_vector2(result, pt, pt2);
    check(is2(result, 6, 30) && result != pt && result != pt2, "addVector2 allows a vector-like object");
  }
  // clamp
  {
    auto min = v2(-1, -2), max = v2(10, 20), out = v2();
    g::clamp_vector2(out, v2(-5, -6), min, max);
    bool ok = is2(out, -1, -2);
    g::clamp_vector2(out, v2(100, 200), min, max);
    ok = ok && is2(out, 10, 20);
    g::clamp_vector2(out, v2(0, 1), min, max);
    check(ok && is2(out, 0, 1), "clampVector2 clamps each component to its own bounds, below, above, and not at all");
  }
  {
    auto out = v2();
    g::clamp_vector2(out, v2(5, -2), v2(0, 0), v2(3, 3));
    check(is2(out, 3, 0), "clampVector2 clamps each component independently");
  }
  {
    auto out = v2();
    g::clamp_vector2(out, v2(1, 2), v2(0, 0), v2(3, 3));
    check(is2(out, 1, 2), "clampVector2 leaves values within range unchanged");
  }
  {
    auto out = v2();
    g::clamp_vector2(out, v2(-2, 5), v2(0, 0), v2(3, 3));
    check(is2(out, 0, 3), "clampVector2 clamps x low and y high");
  }
  {
    auto v = v2(5, -1);
    g::clamp_vector2(v, v, v2(0, 0), v2(3, 3));
    check(is2(v, 3, 0), "clampVector2 supports out === value");
  }
  {
    auto min = v2(0, 0);
    g::clamp_vector2(min, v2(5, -2), min, v2(3, 3));
    auto max = v2(3, 3);
    g::clamp_vector2(max, v2(5, -2), v2(0, 0), max);
    check(is2(min, 3, 0) && is2(max, 3, 0), "clampVector2 supports out === min or max");
  }
  // clone / copy / create
  {
    auto pt = v2(1, 2);
    auto result = g::clone_vector2(pt);
    check(same(result->x, pt->x) && same(result->y, pt->y), "cloneVector2 creates a copy of a vector");
  }
  check(g::clone_vector2(lit2(1, 2)) != nullptr, "cloneVector2 returns a Vector2 instance");
  {
    auto pt = v2(), pt2 = v2(1, 2);
    g::copy_vector2(pt, pt2);
    check(is2(pt, 1, 2), "copyVector2 copies coordinates from one vector to another");
  }
  {
    auto pt = v2(1, 2);
    g::copy_vector2(pt, pt);
    check(is2(pt, 1, 2), "copyVector2 supports out === source");
  }
  {
    auto pt = v2();
    g::copy_vector2(pt, lit2(1, 2));
    check(is2(pt, 1, 2), "copyVector2 allows a vector-like object");
  }
  {
    auto p = g::create_vector2();
    check(p != nullptr && is2(p, 0, 0), "createVector2 returns a Vector2 with default coordinates");
  }
  check(is2(v2(2, 4), 2, 4), "createVector2 sets the specified x and y coordinates");
  check(is2(lit2(2, 4), 2, 4), "createVector2 allows a vector-like object");
  {
    auto pt = g::create_vector2_from_polar(5, 0);
    auto pt2 = v2();
    g::set_vector2_from_polar(pt2, 5, 0);
    check(g::equals_vector2(pt, pt2) == true, "createVector2FromPolar makes a createVector2 and calls polar");
  }
  // cross
  check(same(g::cross_vector2(lit2(1, 0), lit2(0, 1)), 1), "crossVector2 returns positive when b is counter-clockwise from a");
  check(same(g::cross_vector2(lit2(0, 1), lit2(1, 0)), -1), "crossVector2 returns negative when b is clockwise from a");
  check(same(g::cross_vector2(lit2(3, 0), lit2(7, 0)), 0), "crossVector2 returns zero for parallel vectors");
  check(same(g::cross_vector2(lit2(2, 3), lit2(5, 7)), 2 * 7 - 3 * 5), "crossVector2 computes the general case");
  // divide
  {
    auto source = v2(6, 8), out = v2();
    g::divide_vector2(out, source, v2(0, 4));
    bool ok = is2(out, 0, 2);
    g::divide_vector2(out, source, v2(2, 0));
    check(ok && is2(out, 3, 0), "divideVector2 gives 0 for a zero divisor on each component in turn");
  }
  {
    auto out = v2();
    g::divide_vector2(out, v2(6, 8), v2(2, 4));
    check(is2(out, 3, 2), "divideVector2 divides component-wise");
  }
  {
    auto out = v2();
    g::divide_vector2(out, v2(6, 8), v2(0, 4));
    check(is2(out, 0, 2), "divideVector2 produces 0 for zero divisor components");
  }
  {
    auto out = v2();
    g::divide_vector2(out, v2(6, 8), v2(2, 0));
    check(is2(out, 3, 0), "divideVector2 produces 0 for a zero y divisor");
  }
  {
    auto v = v2(6, 8);
    g::divide_vector2(v, v, v2(2, 4));
    check(is2(v, 3, 2), "divideVector2 supports out === source");
  }
  {
    auto divisor = v2(2, 4);
    g::divide_vector2(divisor, v2(6, 8), divisor);
    check(is2(divisor, 3, 2), "divideVector2 supports out === divisor");
  }
  // equals
  {
    auto pt = v2();
    check(g::equals_vector2(pt, null_value) == false && g::equals_vector2(undefined_value, pt) == false, "equalsVector2 returns false when either vector is absent");
  }
  {
    auto pt = v2(), pt2 = v2();
    bool ok = g::equals_vector2(pt, pt2) == true;
    pt->x = 1;
    ok = ok && g::equals_vector2(pt, pt2) == false;
    pt2->x = 1;
    check(ok && g::equals_vector2(pt, pt2) == true, "equalsVector2 returns true if vectors are identical, false otherwise");
  }
  {
    auto pt = v2(), pt2 = lit2(0, 0);
    bool ok = g::equals_vector2(pt, pt2) == true;
    pt->x = 1;
    ok = ok && g::equals_vector2(pt, pt2) == false;
    pt2->x = 1;
    check(ok && g::equals_vector2(pt, pt2) == true, "equalsVector2 allows a vector-like object");
  }
  {
    auto a = v2(1, 2);
    check(g::equals_vector2(a, v2(9, 2)) == false && g::equals_vector2(a, v2(1, 9)) == false, "equalsVector2 compares every component, not just the first");
  }
  {
    auto v = v2(1, 2);
    check(!g::equals_vector2(v, null_value) && !g::equals_vector2(null_value, v) && !g::equals_vector2(v, undefined_value) &&
              !g::equals_vector2(undefined_value, v) && !g::equals_vector2(null_value, null_value) && !g::equals_vector2(undefined_value, undefined_value),
          "equalsVector2 returns false when either side is missing");
  }
  {
    auto not_a_number = v2(nan, 2);
    check(g::equals_vector2(not_a_number, not_a_number) == true && g::equals_vector2(not_a_number, v2(nan, 2)) == false,
          "equalsVector2 holds a vector equal to itself even with a NaN component");
  }
  // angle
  check(same(g::get_vector2_angle(lit2(5, 0)), 0), "getVector2Angle returns 0 for a vector along +x");
  check(close(g::get_vector2_angle(lit2(0, 3)), pi / 2), "getVector2Angle returns PI/2 for a vector along +y");
  check(close(g::get_vector2_angle(lit2(0, -1)), -pi / 2), "getVector2Angle returns -PI/2 for a vector along -y");
  check(close(g::get_vector2_angle(lit2(-2, 0)), pi), "getVector2Angle returns PI for a vector along -x");
  check(same(g::get_vector2_angle(lit2(0, 0)), 0), "getVector2Angle returns 0 for the zero vector");
  {
    auto a = v2(1, 0);
    check(close(g::get_vector2_angle_between(a, a), 0), "getVector2AngleBetween returns 0 for identical vectors");
  }
  check(close(g::get_vector2_angle_between(v2(1, 0), v2(0, 1)), pi / 2), "getVector2AngleBetween returns PI/2 for perpendicular vectors");
  check(close(g::get_vector2_angle_between(v2(1, 0), v2(-1, 0)), pi), "getVector2AngleBetween returns PI for opposite vectors");
  check(std::isnan(g::get_vector2_angle_between(v2(0, 0), v2(1, 0))), "getVector2AngleBetween returns NaN for a zero-length vector");
  // distance / dot / length
  {
    const std::vector<std::vector<double>> cases = {{100, 0, 0, 0, 100}, {0, 100, 0, 0, 100}, {3, 4, 0, 0, 5}, {5, 12, 0, 0, 13}, {8, 15, 0, 0, 17}};
    bool ok = true, ok_like = true;
    for (const auto& c : cases) {
      auto pt = v2(), pt2 = v2();
      pt->x = c[0];
      pt->y = c[1];
      pt2->x = c[2];
      pt2->y = c[3];
      ok = ok && same(g::get_vector2_distance(pt, pt2), c[4]);
      ok_like = ok_like && same(g::get_vector2_distance(lit2(c[0], c[1]), lit2(c[2], c[3])), c[4]);
    }
    check(ok, "getVector2Distance calculates Euclidean distance between vectors");
    check(ok_like, "getVector2Distance allows a vector-like object");
  }
  check(same(g::get_vector2_distance_squared(v2(1, 1), v2(4, 5)), 25), "getVector2DistanceSquared returns the squared distance between two vectors");
  check(same(g::get_vector2_distance_squared(lit2(1, 1), lit2(4, 5)), 25), "getVector2DistanceSquared allows a vector-like object");
  check(same(g::get_vector2_dot(v2(1, 2), v2(4, 5)), 14), "getVector2Dot returns the dot product of two vectors");
  check(same(g::get_vector2_dot(lit2(1, 2), lit2(4, 5)), 14), "getVector2Dot allows a vector-like object");
  {
    const std::vector<std::vector<double>> cases = {{100, 0, 100}, {0, 100, 100}, {3, 4, 5}, {5, 12, 13}, {8, 15, 17}};
    bool ok = true, ok_like = true;
    for (const auto& c : cases) {
      auto pt = v2();
      pt->x = c[0];
      pt->y = c[1];
      ok = ok && same(g::get_vector2_length(pt), c[2]);
      ok_like = ok_like && same(g::get_vector2_length(lit2(c[0], c[1])), c[2]);
    }
    check(ok, "getVector2Length returns the length of the vector");
    check(ok_like, "getVector2Length allows a vector-like object");
  }
  check(same(g::get_vector2_length_squared(v2(3, 4)), 9 + 16), "getVector2LengthSquared returns the square of the length");
  check(same(g::get_vector2_length_squared(v2()), 0), "getVector2LengthSquared returns 0 for the origin (0, 0)");
  check(same(g::get_vector2_length_squared(v2(-3, -4)), 9 + 16), "getVector2LengthSquared handles negative values correctly");
  check(same(g::get_vector2_length_squared(v2(2.5, 4.5)), 2.5 * 2.5 + 4.5 * 4.5), "getVector2LengthSquared handles non-integer values");
  check(same(g::get_vector2_length_squared(lit2(3, 4)), 9 + 16), "getVector2LengthSquared allows a vector-like object");
  // interpolate
  {
    const std::vector<std::pair<double, std::function<double(double, double)>>> cases = {
        {0, [](double a, double) { return a; }},
        {1, [](double, double b) { return b; }},
        {0.5, [](double a, double b) { return (a + b) / 2; }},
        {10, [](double a, double b) { return a + (b - a) * 10; }},
        {-1, [](double a, double b) { return a + (b - a) * -1; }},
    };
    auto pt = v2(0, 0), pt2 = v2(100, 100);
    bool ok = true;
    for (const auto& [t, expected] : cases) {
      auto result = v2();
      g::interpolate_vector2(result, pt, pt2, t);
      ok = ok && same(result->x, expected(pt->x, pt2->x)) && same(result->y, expected(pt->y, pt2->y));
    }
    check(ok, "interpolateVector2 interpolates and extrapolates correctly");
    auto lpt = lit2(0, 0), lpt2 = lit2(100, 100);
    bool ok_like = true;
    for (const auto& [t, expected] : cases) {
      auto result = lit2(0, 0);
      g::interpolate_vector2(result, lpt, lpt2, t);
      ok_like = ok_like && same(result->x, expected(lpt->x, lpt2->x)) && same(result->y, expected(lpt->y, lpt2->y));
    }
    check(ok_like, "interpolateVector2 allows vector-like objects");
  }
  {
    auto pt = v2(10, 20), pt2 = v2(30, 40);
    g::interpolate_vector2(pt, pt, pt2, 0.5);
    check(is2(pt, 20, 30) && is2(pt2, 30, 40), "interpolateVector2 works correctly if out is the first vector");
  }
  {
    auto pt = v2(10, 20), pt2 = v2(30, 40);
    g::interpolate_vector2(pt2, pt, pt2, 0.5);
    check(is2(pt, 10, 20) && is2(pt2, 20, 30), "interpolateVector2 works correctly if out is the second vector");
  }
  {
    auto result = v2();
    g::interpolate_vector2(result, v2(0, 0), v2(100, 100), 1000);
    check(is2(result, 100000, 100000), "interpolateVector2 handles extreme extrapolation values for t");
  }
  // max / min
  {
    auto a = v2(1, 5), b = v2(3, 0), out = v2();
    g::max_vector2(out, a, b);
    bool ok = is2(out, 3, 5);
    g::max_vector2(out, b, a);
    check(ok && is2(out, 3, 5), "maxVector2 takes each component from whichever side wins, and gives the same answer either way round");
  }
  {
    auto out = v2();
    g::max_vector2(out, v2(1, 5), v2(3, 2));
    check(is2(out, 3, 5), "maxVector2 returns the component-wise maximum");
  }
  {
    auto a = v2(1, 5), b = v2(3, 2);
    g::max_vector2(a, a, b);
    check(is2(a, 3, 5), "maxVector2 supports out === a");
  }
  {
    auto a = v2(1, 5), b = v2(3, 2);
    g::max_vector2(b, a, b);
    check(is2(b, 3, 5), "maxVector2 supports out === b");
  }
  {
    auto out = v2();
    g::max_vector2(out, v2(5, 1), v2(2, 3));
    check(is2(out, 5, 3), "maxVector2 selects a for x and b for y");
  }
  {
    auto a = v2(1, 5), b = v2(3, 0), out = v2();
    g::min_vector2(out, a, b);
    bool ok = is2(out, 1, 0);
    g::min_vector2(out, b, a);
    check(ok && is2(out, 1, 0), "minVector2 takes each component from whichever side wins, and gives the same answer either way round");
  }
  {
    auto out = v2();
    g::min_vector2(out, v2(1, 5), v2(3, 2));
    check(is2(out, 1, 2), "minVector2 returns the component-wise minimum");
  }
  {
    auto a = v2(1, 5), b = v2(3, 2);
    g::min_vector2(a, a, b);
    check(is2(a, 1, 2), "minVector2 supports out === a");
  }
  {
    auto a = v2(1, 5), b = v2(3, 2);
    g::min_vector2(b, a, b);
    check(is2(b, 1, 2), "minVector2 supports out === b");
  }
  {
    auto out = v2();
    g::min_vector2(out, v2(5, 1), v2(2, 3));
    check(is2(out, 2, 1), "minVector2 selects b for x and a for y");
  }
  // multiply
  {
    auto out = v2();
    g::multiply_vector2(out, v2(2, 3), v2(4, 5));
    check(is2(out, 8, 15), "multiplyVector2 multiplies component-wise (Hadamard)");
  }
  {
    auto a = v2(2, 3);
    g::multiply_vector2(a, a, v2(4, 5));
    check(is2(a, 8, 15), "multiplyVector2 supports out === a");
  }
  {
    auto b = v2(4, 5);
    g::multiply_vector2(b, v2(2, 3), b);
    check(is2(b, 8, 15), "multiplyVector2 supports out === b");
  }
  // nearEquals
  {
    auto a = v2(1, 2);
    check(g::near_equals_vector2(a, a) == true, "nearEqualsVector2 returns true for identical vectors");
  }
  check(g::near_equals_vector2(v2(1, 2), v2(1 + 1e-7, 2)) == true, "nearEqualsVector2 returns true when difference is within default tolerance");
  check(g::near_equals_vector2(v2(1, 2), v2(1 + 1e-5, 2)) == false, "nearEqualsVector2 returns false when difference exceeds default tolerance");
  check(g::near_equals_vector2(v2(1, 2), v2(1.05, 2), 0.1) == true && g::near_equals_vector2(v2(1, 2), v2(1.05, 2), 0.01) == false,
        "nearEqualsVector2 respects a custom tolerance");
  // negate
  {
    auto result = v2();
    g::negate_vector2(result, v2(1, -2));
    check(is2(result, -1, 2), "negateVector2 inverts the values of the vector components");
  }
  {
    auto v = v2(1, -2);
    g::negate_vector2(v, v);
    check(is2(v, -1, 2), "negateVector2 supports out === source");
  }
  // normalize
  {
    auto pt = v2(3, 4), result = v2();
    g::normalize_vector2(result, pt);
    check(pt != result && near2(result, 0.6, 0.8) && close(g::get_vector2_length(result), 1), "normalizeVector2 produces a unit vector");
  }
  {
    auto result = v2();
    check(close(g::normalize_vector2(result, v2(3, 4)), 5), "normalizeVector2 returns the original length");
  }
  {
    auto pt = v2(0, 0), result = v2();
    const double length = g::normalize_vector2(result, pt);
    check(result != pt && is2(result, 0, 0) && same(length, 0), "normalizeVector2 returns zero for a zero-length vector");
  }
  {
    auto result = v2();
    g::normalize_vector2(result, v2(0.0001, 0.0001));
    check(near2(result, 0.7071, 0.7071, 4) && close(g::get_vector2_length(result), 1), "normalizeVector2 handles very small vectors correctly");
  }
  {
    auto pt = v2(3, 4);
    const double length = g::normalize_vector2(pt, pt);
    check(near2(pt, 0.6, 0.8) && close(length, 5), "normalizeVector2 supports out === source");
  }
  {
    auto pt = lit2(3, 4), result = lit2(0, 0);
    g::normalize_vector2(result, pt);
    check(pt != result && near2(result, 0.6, 0.8) && close(g::get_vector2_length(result), 1), "normalizeVector2 allows a vector-like object");
  }
  // offset
  {
    auto result = v2();
    g::offset_vector2(result, v2(), 10, 100);
    check(is2(result, 10, 100), "offsetVector2 adjusts the value of a vector");
  }
  {
    auto result = v2();
    g::offset_vector2(result, v2(), -5, -10);
    check(is2(result, -5, -10), "offsetVector2 works with negative deltas");
  }
  {
    auto pt = v2(1, 2);
    g::offset_vector2(pt, pt, 3, 4);
    check(is2(pt, 4, 6), "offsetVector2 supports out === source");
  }
  {
    auto result = lit2(0, 0);
    g::offset_vector2(result, v2(), 10, 100);
    check(is2(result, 10, 100), "offsetVector2 allows a vector-like object");
  }
  // reflect
  {
    auto out = v2();
    g::reflect_vector2(out, v2(1, -1), v2(1, 0));
    check(near2(out, -1, -1, 6), "reflectVector2 reflects incident vector about a normal");
  }
  {
    auto out = v2();
    g::reflect_vector2(out, v2(0, -1), v2(0, 1));
    check(near2(out, 0, 1, 6), "reflectVector2 reflects straight down about horizontal normal gives straight up");
  }
  {
    auto v = v2(1, -1);
    g::reflect_vector2(v, v, v2(1, 0));
    check(near2(v, -1, -1, 6), "reflectVector2 supports out === incident");
  }
  {
    auto normal = v2(1, 0);
    g::reflect_vector2(normal, v2(1, -1), normal);
    check(near2(normal, -1, -1, 6), "reflectVector2 supports out === normal");
  }
  // rotate
  {
    auto out = v2();
    g::rotate_vector2(out, lit2(1, 0), pi / 2);
    check(near2(out, 0, 1), "rotateVector2 rotates +x by PI/2 to +y");
  }
  {
    auto out = v2();
    g::rotate_vector2(out, lit2(3, 4), pi * 2);
    check(near2(out, 3, 4), "rotateVector2 rotates by a full turn and returns to the original");
  }
  {
    auto v = v2(1, 0);
    g::rotate_vector2(v, v, pi / 2);
    check(near2(v, 0, 1), "rotateVector2 is alias-safe when out equals source");
  }
  // scale
  {
    auto result = v2();
    g::scale_vector2(result, v2(1, 2), 3);
    check(is2(result, 3, 6), "scaleVector2 scales the vector by a scalar");
  }
  {
    auto v = v2(1, 2);
    g::scale_vector2(v, v, 3);
    check(is2(v, 3, 6), "scaleVector2 supports out === source");
  }
  {
    auto pt = v2(3, 4), result = v2();
    g::scale_vector2_to_length(result, pt, 10);
    check(pt != result && near2(result, 6, 8) && close(g::get_vector2_length(result), 10), "scaleVector2ToLength scales a vector to the specified length");
  }
  {
    auto pt = v2(0, 0), result = v2();
    g::scale_vector2_to_length(result, pt, 5);
    check(result != pt && is2(result, 0, 0) && same(g::get_vector2_length(result), 0), "scaleVector2ToLength returns zero for a zero-length vector");
  }
  {
    auto result = v2();
    g::scale_vector2_to_length(result, v2(3, 4), 0);
    check(is2(result, 0, 0) && same(g::get_vector2_length(result), 0), "scaleVector2ToLength scales vector to zero length");
  }
  {
    auto result = v2();
    g::scale_vector2_to_length(result, v2(3, 4), -10);
    check(near2(result, -6, -8) && close(g::get_vector2_length(result), 10), "scaleVector2ToLength scales vector to negative length");
  }
  {
    auto pt = v2(3, 4);
    g::scale_vector2_to_length(pt, pt, 10);
    check(near2(pt, 6, 8) && close(g::get_vector2_length(pt), 10), "scaleVector2ToLength supports out === source");
  }
  {
    auto pt = lit2(3, 4), result = lit2(0, 0);
    g::scale_vector2_to_length(result, pt, 10);
    check(pt != result && near2(result, 6, 8) && close(g::get_vector2_length(result), 10), "scaleVector2ToLength allows a vector-like object");
  }
  // set
  {
    auto pt = v2();
    g::set_vector2(pt, 2, 10);
    check(is2(pt, 2, 10), "setVector2 updates coordinates of a vector");
  }
  {
    auto pt = v2(1, 2);
    g::set_vector2(pt, 0, 0);
    check(is2(pt, 0, 0), "setVector2 sets both coordinates to zero correctly");
  }
  {
    auto pt = lit2(0, 0);
    g::set_vector2(pt, 2, 10);
    check(is2(pt, 2, 10), "setVector2 allows a vector-like object");
  }
  {
    flight::Float32Array array(std::size_t{2});
    array.set_index(0, 1);
    array.set_index(1, 2);
    auto vector = lit2(100, 100);
    g::set_vector2_from_float32_array(vector, 0, array);
    check(is2(vector, 1, 2), "setVector2FromFloat32Array writes the vector from 2 values at the offset");
  }
  {
    auto p = v2();
    g::set_vector2_from_polar(p, 5, 0);
    check(near2(p, 5, 0) && close(g::get_vector2_length(p), 5), "setVector2FromPolar returns a vector at the given length along the x-axis when angle is 0");
  }
  {
    auto p = v2();
    g::set_vector2_from_polar(p, 3, pi / 2);
    check(near2(p, 0, 3) && close(g::get_vector2_length(p), 3), "setVector2FromPolar returns a vector at the given length along the y-axis when angle is pi/2");
  }
  {
    auto p = v2();
    g::set_vector2_from_polar(p, 4, pi);
    check(near2(p, -4, 0) && close(g::get_vector2_length(p), 4), "setVector2FromPolar returns a vector in the correct quadrant for angle pi");
  }
  {
    auto p = v2();
    g::set_vector2_from_polar(p, 2, (3 * pi) / 2);
    check(near2(p, 0, -2) && close(g::get_vector2_length(p), 2), "setVector2FromPolar returns a vector in the correct quadrant for angle 3pi/2");
  }
  {
    auto p = v2();
    g::set_vector2_from_polar(p, 0, pi / 4);
    check(near2(p, 0, 0) && close(g::get_vector2_length(p), 0), "setVector2FromPolar handles zero length");
  }
  {
    auto p = v2();
    g::set_vector2_from_polar(p, -5, 0);
    check(near2(p, -5, 0) && close(g::get_vector2_length(p), 5), "setVector2FromPolar handles negative length");
  }
  {
    const double len = std::sqrt(2.0);
    auto p = v2();
    g::set_vector2_from_polar(p, len, pi / 4);
    check(near2(p, 1, 1) && close(g::get_vector2_length(p), len), "setVector2FromPolar handles arbitrary angles");
  }
  {
    auto p = lit2(0, 0);
    g::set_vector2_from_polar(p, 5, 0);
    check(near2(p, 5, 0), "setVector2FromPolar allows vector-like objects");
  }
  {
    auto out = v2();
    g::set_vector2_from_vector3(out, lit3(3, 7, 99));
    check(is2(out, 3, 7), "setVector2FromVector3 copies x and y from a Vector3Like, dropping z");
  }
  {
    auto out = v2(1, 2);
    g::set_vector2_from_vector3(out, lit3(out->x, out->y, 5));
    check(is2(out, 1, 2), "setVector2FromVector3 is safe when out shares x/y memory with source (plain object)");
  }
  // subtract
  {
    auto result = v2();
    g::subtract_vector2(result, v2(5, 10), v2(2, 4));
    check(is2(result, 3, 6), "subtractVector2 subtracts the coordinates of two vectors into a new vector");
  }
  {
    auto result = v2();
    g::subtract_vector2(result, v2(2, 3), v2(5, 10));
    check(is2(result, -3, -7), "subtractVector2 handles negative results correctly");
  }
  {
    auto pt = v2(7, -3), result = v2();
    g::subtract_vector2(result, pt, pt);
    check(is2(result, 0, 0), "subtractVector2 returns zero when subtracting a vector from itself");
  }
  {
    auto pt = v2(5, 10), pt2 = v2(2, 4);
    g::subtract_vector2(pt, pt, pt2);
    check(is2(pt, 3, 6), "subtractVector2 supports out === source");
  }
  {
    auto pt = v2(5, 10), pt2 = v2(2, 4);
    g::subtract_vector2(pt2, pt, pt2);
    check(is2(pt2, 3, 6), "subtractVector2 supports out === other");
  }
  {
    auto pt = v2(0, 0), pt2 = v2(5, 10), result = v2(), result2 = v2();
    g::subtract_vector2(result, pt, pt2);
    g::subtract_vector2(result2, pt2, pt);
    check(is2(result, -5, -10) && is2(result2, 5, 10), "subtractVector2 works when source or target vectors are at the origin");
  }
  {
    auto result = v2();
    g::subtract_vector2(result, v2(0.0001, 0.0001), v2(0.0001, 0.0001));
    check(near2(result, 0, 0), "subtractVector2 works with very small values and results near zero");
  }
  {
    auto pt = lit2(5, 10), pt2 = lit2(2, 4), result = lit2(0, 0);
    g::subtract_vector2(result, pt, pt2);
    check(is2(result, 3, 6) && result != pt && result != pt2, "subtractVector2 allows vector-like objects");
  }
  {
    flight::Float32Array array(std::size_t{6});
    g::write_vector2_to_float32_array(array, 0, lit2(1, 2));
    bool ok = true;
    for (int i = 0; i < 2; ++i) ok = ok && same(array.get_index(i), i + 1);
    check(ok, "writeVector2ToFloat32Array writes 2 values at the offset");
  }
}

void test_vector3_a() {
  {
    auto result = v3();
    g::add_vector3(result, v3(1, 2, 3), v3(4, 5, 6));
    check(is3(result, 5, 7, 9), "addVector3 returns a new vector when no target is passed");
  }
  {
    auto a = v3(1, 2, 3);
    g::add_vector3(a, a, a);
    check(is3(a, 2, 4, 6), "addVector3 modifies target when same object is passed as target");
  }
  {
    auto a = v3(1, 2, 3), b = v3(4, 5, 6);
    g::add_vector3(a, a, b);
    check(is3(a, 5, 7, 9), "addVector3 supports out === a");
  }
  {
    auto a = v3(1, 2, 3), b = v3(4, 5, 6);
    g::add_vector3(b, a, b);
    check(is3(b, 5, 7, 9), "addVector3 supports out === b");
  }
  {
    auto result = lit3(0, 0, 0);
    g::add_vector3(result, lit3(1, 2, 3), lit3(4, 5, 6));
    check(is3(result, 5, 7, 9), "addVector3 allows vector-like objects");
  }
  {
    auto min = v3(-1, -2, -3), max = v3(10, 20, 30), out = v3();
    g::clamp_vector3(out, v3(-5, -6, -7), min, max);
    bool ok = is3(out, -1, -2, -3);
    g::clamp_vector3(out, v3(100, 200, 300), min, max);
    ok = ok && is3(out, 10, 20, 30);
    g::clamp_vector3(out, v3(0, 1, 2), min, max);
    check(ok && is3(out, 0, 1, 2), "clampVector3 clamps each component to its own bounds, below, above, and not at all");
  }
  {
    auto out = v3();
    g::clamp_vector3(out, v3(5, -2, 1), v3(0, 0, 0), v3(3, 3, 3));
    check(is3(out, 3, 0, 1), "clampVector3 clamps each component independently");
  }
  {
    auto v = v3(5, -1, 7);
    g::clamp_vector3(v, v, v3(0, 0, 0), v3(3, 3, 3));
    check(is3(v, 3, 0, 3), "clampVector3 supports out === value");
  }
  {
    auto min = v3(0, 0, 0);
    g::clamp_vector3(min, v3(5, -2, 1), min, v3(3, 3, 3));
    auto max = v3(3, 3, 3);
    g::clamp_vector3(max, v3(5, -2, 1), v3(0, 0, 0), max);
    check(is3(min, 3, 0, 1) && is3(max, 3, 0, 1), "clampVector3 supports out === min or max");
  }
  {
    auto out = v3();
    g::clamp_vector3(out, v3(-1, 2, 5), v3(0, 0, 0), v3(3, 3, 3));
    bool ok = is3(out, 0, 2, 3);
    g::clamp_vector3(out, v3(2, 5, -1), v3(0, 0, 0), v3(3, 3, 3));
    check(ok && is3(out, 2, 3, 0), "clampVector3 covers every clamp outcome on every component");
  }
  {
    auto original = v3(1, 2, 3);
    auto cloned = g::clone_vector3(original);
    check(cloned != original && cloned != nullptr && is3(cloned, 1, 2, 3), "cloneVector3 creates a new independent vector");
  }
  {
    auto original = lit3(1, 2, 3);
    auto cloned = g::clone_vector3(original);
    check(cloned != original && cloned != nullptr && is3(cloned, 1, 2, 3), "cloneVector3 allows vector-like objects");
  }
  {
    auto target = v3();
    g::copy_vector3(target, v3(1, 2, 3));
    check(is3(target, 1, 2, 3), "copyVector3 copies values from source to target");
  }
  {
    auto vector = v3(1, 2, 3);
    g::copy_vector3(vector, vector);
    check(is3(vector, 1, 2, 3), "copyVector3 does not affect source when same object is used for input and output");
  }
  check(is3(g::create_vector3(), 0, 0, 0), "createVector3 creates a createVector3 with default values");
  check(is3(v3(1, 2, 3), 1, 2, 3), "createVector3 creates a createVector3 with specified values");
  check(near3(g::create_vector3_from_spherical(1, pi / 2, 0), 1, 0, 0, 6), "createVector3FromSpherical produces the correct cartesian point");
  {
    auto v = g::create_vector3_from_spherical(2, pi / 3, pi / 4);
    auto sph = v3();
    g::get_vector3_spherical(sph, v);
    check(near3(sph, 2, pi / 3, pi / 4, 5), "createVector3FromSpherical round-trips with getVector3Spherical");
  }
  {
    auto result = v3();
    g::cross_vector3(result, v3(1, 0, 0), v3(0, 1, 0));
    check(is3(result, 0, 0, 1), "crossVector3 returns the cross product of two vectors");
  }
  {
    auto a = v3(1, 0, 0), b = v3(0, 1, 0);
    g::cross_vector3(a, a, b);
    check(is3(a, 0, 0, 1), "crossVector3 modifies target when same object is passed as target");
  }
  {
    auto a = v3(1, 0, 0), b = v3(0, 1, 0);
    g::cross_vector3(b, a, b);
    check(is3(b, 0, 0, 1), "crossVector3 supports out === other");
  }
  {
    auto source = v3(6, 8, 9), out = v3();
    g::divide_vector3(out, source, v3(0, 4, 3));
    bool ok = is3(out, 0, 2, 3);
    g::divide_vector3(out, source, v3(2, 0, 3));
    ok = ok && is3(out, 3, 0, 3);
    g::divide_vector3(out, source, v3(2, 4, 0));
    check(ok && is3(out, 3, 2, 0), "divideVector3 gives 0 for a zero divisor on each component in turn");
  }
  {
    auto out = v3();
    g::divide_vector3(out, v3(6, 8, 9), v3(2, 4, 3));
    check(is3(out, 3, 2, 3), "divideVector3 divides component-wise");
  }
  {
    auto out = v3();
    g::divide_vector3(out, v3(6, 8, 9), v3(0, 4, 0));
    check(is3(out, 0, 2, 0), "divideVector3 produces 0 for zero divisor components");
  }
  {
    auto v = v3(6, 8, 9);
    g::divide_vector3(v, v, v3(2, 4, 3));
    check(is3(v, 3, 2, 3), "divideVector3 supports out === source");
  }
  {
    auto divisor = v3(2, 4, 3);
    g::divide_vector3(divisor, v3(6, 8, 9), divisor);
    check(is3(divisor, 3, 2, 3), "divideVector3 supports out === divisor");
  }
  {
    auto out = v3();
    g::divide_vector3(out, v3(6, 8, 9), v3(2, 0, 3));
    check(is3(out, 3, 0, 3), "divideVector3 produces 0 for a zero y divisor");
  }
  check(g::equals_vector3(v3(), null_value) == false && g::equals_vector3(undefined_value, v3()) == false, "equalsVector3 returns false when either vector is absent");
  check(g::equals_vector3(v3(1, 2, 3), v3(1, 2, 3)) == true, "equalsVector3 returns true if vectors are equal");
  check(g::equals_vector3(v3(1, 2, 3), v3(4, 5, 6)) == false, "equalsVector3 returns false if vectors are not equal");
  {
    auto a = v3(1, 2, 3);
    check(!g::equals_vector3(a, v3(9, 2, 3)) && !g::equals_vector3(a, v3(1, 9, 3)) && !g::equals_vector3(a, v3(1, 2, 9)),
          "equalsVector3 compares every component, not just the first");
  }
  {
    auto v = v3(1, 2, 3);
    check(!g::equals_vector3(v, null_value) && !g::equals_vector3(null_value, v) && !g::equals_vector3(v, undefined_value) &&
              !g::equals_vector3(undefined_value, v) && !g::equals_vector3(null_value, null_value) && !g::equals_vector3(undefined_value, undefined_value),
          "equalsVector3 returns false when either side is missing");
  }
  {
    auto not_a_number = v3(nan, 2, 3);
    check(g::equals_vector3(not_a_number, not_a_number) == true && g::equals_vector3(not_a_number, v3(nan, 2, 3)) == false,
          "equalsVector3 holds a vector equal to itself even with a NaN component");
  }
  {
    auto a = v3(1, 0, 0);
    check(close(g::get_vector3_angle_between(a, a), 0), "getVector3AngleBetween returns 0 for identical vectors");
  }
  check(close(g::get_vector3_angle_between(v3(1, 0, 0), v3(0, 1, 0)), pi / 2), "getVector3AngleBetween returns PI/2 for perpendicular vectors");
  check(close(g::get_vector3_angle_between(v3(1, 0, 0), v3(-1, 0, 0)), pi), "getVector3AngleBetween returns PI for opposite vectors");
  check(std::isnan(g::get_vector3_angle_between(v3(0, 0, 0), v3(1, 0, 0))), "getVector3AngleBetween returns NaN for a zero-length vector");
  check(close(g::get_vector3_distance(v3(1, 1, 1), v3(4, 5, 6)), 7.071068, 5), "getVector3Distance returns the distance between two vectors");
  check(same(g::get_vector3_distance_squared(v3(1, 1, 1), v3(4, 5, 6)), 50), "getVector3DistanceSquared returns the squared distance between two vectors");
  check(same(g::get_vector3_dot(v3(1, 2, 3), v3(4, 5, 6)), 32), "getVector3Dot returns the dot product of two vectors");
  check(same(g::get_vector3_length(v3(3, 4, 0)), 5), "getVector3Length returns the length of the vector");
  check(same(g::get_vector3_length(lit3(3, 4, 0)), 5), "getVector3Length allows a vector-like object");
  check(same(g::get_vector3_length_squared(v3(3, 4, 0)), 25), "getVector3LengthSquared returns the squared length of the vector");
  check(same(g::get_vector3_length_squared(lit3(3, 4, 0)), 25), "getVector3LengthSquared allows a vector-like object");
  {
    auto sph = v3();
    g::get_vector3_spherical(sph, v3(1, 0, 0));
    check(near3(sph, 1, pi / 2, 0, 6), "getVector3Spherical returns radius, theta, phi for a known point");
  }
  {
    auto sph = v3();
    g::get_vector3_spherical(sph, v3(0, 0, 0));
    check(is3(sph, 0, 0, 0), "getVector3Spherical handles zero vector");
  }
  {
    auto vector = v3(1, 0, 0);
    g::get_vector3_spherical(vector, vector);
    check(near3(vector, 1, pi / 2, 0, 6), "getVector3Spherical supports out === source");
  }
  {
    auto out = v3(), a = v3(1, 2, 3), b = v3(4, 5, 6);
    g::interpolate_vector3(out, a, b, 0);
    bool ok = is3(out, 1, 2, 3);
    g::interpolate_vector3(out, a, b, 1);
    check(ok && is3(out, 4, 5, 6), "interpolateVector3 returns a at t=0 and b at t=1");
  }
  {
    auto out = v3();
    g::interpolate_vector3(out, v3(0, 0, 0), v3(2, 4, 6), 0.5);
    check(is3(out, 1, 2, 3), "interpolateVector3 interpolates midpoint at t=0.5");
  }
  {
    auto a = v3(0, 0, 0), b = v3(2, 4, 6);
    g::interpolate_vector3(a, a, b, 0.5);
    check(is3(a, 1, 2, 3), "interpolateVector3 supports out === a");
  }
  {
    auto a = v3(0, 0, 0), b = v3(2, 4, 6);
    g::interpolate_vector3(b, a, b, 0.5);
    check(is3(b, 1, 2, 3), "interpolateVector3 supports out === b");
  }
  {
    auto a = v3(1, 5, 2), b = v3(3, 0, 7), out = v3();
    g::max_vector3(out, a, b);
    bool ok = is3(out, 3, 5, 7);
    g::max_vector3(out, b, a);
    check(ok && is3(out, 3, 5, 7), "maxVector3 takes each component from whichever side wins, and gives the same answer either way round");
  }
  {
    auto out = v3();
    g::max_vector3(out, v3(1, 5, 2), v3(3, 2, 7));
    check(is3(out, 3, 5, 7), "maxVector3 returns the component-wise maximum");
  }
  {
    auto a = v3(1, 5, 2);
    g::max_vector3(a, a, v3(3, 2, 7));
    auto b = v3(3, 2, 7);
    g::max_vector3(b, v3(1, 5, 2), b);
    check(is3(a, 3, 5, 7) && is3(b, 3, 5, 7), "maxVector3 supports out === a or b");
  }
  {
    auto out = v3();
    g::max_vector3(out, v3(5, 1, 7), v3(2, 3, 4));
    check(is3(out, 5, 3, 7), "maxVector3 selects the complementary component outcomes");
  }
}

flight::Float32Array f32(std::initializer_list<float> values) { return flight::Float32Array(values); }
flight::Ref<types::Matrix3> mat3(std::initializer_list<float> values) {
  return flight::make_ref<types::Matrix3>(types::Matrix3{.entity_runtime_key = std::nullopt, .m = f32(values)});
}

template <flight::FixedString Key, typename Axis>
double axis(const Axis& value) {
  return flight::row_get<flight::RowKey<Key>>(value);
}

void test_vector3_b() {
  {
    auto a = v3(1, 5, 2), b = v3(3, 0, 7), out = v3();
    g::min_vector3(out, a, b);
    bool ok = is3(out, 1, 0, 2);
    g::min_vector3(out, b, a);
    check(ok && is3(out, 1, 0, 2), "minVector3 takes each component from whichever side wins, and gives the same answer either way round");
  }
  {
    auto out = v3();
    g::min_vector3(out, v3(1, 5, 2), v3(3, 2, 7));
    check(is3(out, 1, 2, 2), "minVector3 returns the component-wise minimum");
  }
  {
    auto a = v3(1, 5, 2);
    g::min_vector3(a, a, v3(3, 2, 7));
    auto b = v3(3, 2, 7);
    g::min_vector3(b, v3(1, 5, 2), b);
    check(is3(a, 1, 2, 2) && is3(b, 1, 2, 2), "minVector3 supports out === a or b");
  }
  {
    auto out = v3();
    g::min_vector3(out, v3(5, 1, 7), v3(2, 3, 4));
    check(is3(out, 2, 1, 4), "minVector3 selects the complementary component outcomes");
  }
  {
    auto out = v3();
    g::multiply_vector3(out, v3(2, 3, 4), v3(5, 6, 7));
    check(is3(out, 10, 18, 28), "multiplyVector3 multiplies component-wise (Hadamard)");
  }
  {
    auto a = v3(2, 3, 4);
    g::multiply_vector3(a, a, v3(5, 6, 7));
    check(is3(a, 10, 18, 28), "multiplyVector3 supports out === a");
  }
  {
    auto b = v3(5, 6, 7);
    g::multiply_vector3(b, v3(2, 3, 4), b);
    check(is3(b, 10, 18, 28), "multiplyVector3 supports out === b");
  }
  {
    auto a = v3(1, 2, 3);
    check(g::near_equals_vector3(a, a) == true, "nearEqualsVector3 returns true for identical vectors");
  }
  check(g::near_equals_vector3(v3(1, 2, 3), v3(1 + 1e-7, 2, 3)) == true, "nearEqualsVector3 returns true when difference is within default tolerance");
  check(g::near_equals_vector3(v3(1, 2, 3), v3(1 + 1e-5, 2, 3)) == false, "nearEqualsVector3 returns false when difference exceeds default tolerance");
  check(g::near_equals_vector3(v3(1, 2, 3), v3(1.05, 2, 3), 0.1) == true && g::near_equals_vector3(v3(1, 2, 3), v3(1.05, 2, 3), 0.01) == false,
        "nearEqualsVector3 respects a custom tolerance");
  {
    auto result = v3();
    g::negate_vector3(result, v3(1, -2, 3));
    check(is3(result, -1, 2, -3), "negateVector3 inverts the values of the vector components");
  }
  {
    auto v = v3(1, -2, 3);
    g::negate_vector3(v, v);
    check(is3(v, -1, 2, -3), "negateVector3 supports out === source");
  }
  {
    auto result = v3();
    const double length = g::normalize_vector3(result, v3(3, 4, 0));
    check(close(result->x, 0.6, 5) && close(result->y, 0.8, 5) && same(result->z, 0) && same(length, 5), "normalizeVector3 normalizes the vector");
  }
  {
    auto v = v3(3, 4, 0);
    const double length = g::normalize_vector3(v, v);
    check(close(v->x, 0.6, 5) && close(v->y, 0.8, 5) && same(v->z, 0) && same(length, 5), "normalizeVector3 supports out === source");
  }
  {
    auto result = v3(1, 2, 3);
    const double length = g::normalize_vector3(result, v3(0, 0, 0));
    check(is3(result, 0, 0, 0) && same(length, 0), "normalizeVector3 writes zero to out for a zero-length source");
  }
  {
    auto result = v3();
    g::offset_vector3(result, v3(1, 2, 3), 10, 20, 30);
    check(is3(result, 11, 22, 33), "offsetVector3 offsets each component by a scalar per axis");
  }
  {
    auto result = v3();
    g::offset_vector3(result, v3(1, 2, 3), -5, -10, -15);
    check(is3(result, -4, -8, -12), "offsetVector3 works with negative deltas");
  }
  {
    auto v = v3(1, 2, 3);
    g::offset_vector3(v, v, 3, 4, 5);
    check(is3(v, 4, 6, 8), "offsetVector3 supports out === source");
  }
  {
    auto out = lit2(0, 0);
    g::project_vector3(out, v3(4, 6, 2));
    check(is2(out, 2, 3), "projectVector3 divides x and y by z to produce a 2D point");
  }
  {
    auto out = lit2(0, 0);
    g::project_vector3(out, v3(5, 7, 1));
    check(is2(out, 5, 7), "projectVector3 returns the original xy when z is 1");
  }
  {
    auto out = v3();
    g::reflect_vector3(out, v3(1, -1, 0), v3(1, 0, 0));
    check(near3(out, -1, -1, 0, 6), "reflectVector3 reflects incident vector about the x-axis normal");
  }
  {
    auto out = v3();
    g::reflect_vector3(out, v3(0, -1, 0), v3(0, 1, 0));
    check(near3(out, 0, 1, 0, 6), "reflectVector3 reflecting a downward vector about upward normal gives upward");
  }
  {
    auto v = v3(1, -1, 0);
    g::reflect_vector3(v, v, v3(1, 0, 0));
    check(close(v->x, -1, 6) && close(v->y, -1, 6), "reflectVector3 supports out === incident");
  }
  {
    auto normal = v3(1, 0, 0);
    g::reflect_vector3(normal, v3(1, -1, 0), normal);
    check(near3(normal, -1, -1, 0, 6), "reflectVector3 supports out === normal");
  }
  {
    auto result = v3();
    g::scale_vector3(result, v3(1, 1, 1), 2);
    check(is3(result, 2, 2, 2), "scaleVector3 scales the vector by a scalar");
  }
  {
    auto v = v3(1, 1, 1);
    g::scale_vector3(v, v, 2);
    check(is3(v, 2, 2, 2), "scaleVector3 supports out === source");
  }
  {
    auto v = v3();
    g::set_vector3(v, 5, 10, 15);
    check(is3(v, 5, 10, 15), "setVector3 sets the values of the vector");
  }
  {
    auto v = v3(1, 2, 3);
    g::set_vector3(v, 5, 10, 15);
    check(is3(v, 5, 10, 15), "setVector3 modifies target when same object is passed as target");
  }
  {
    auto out = v3();
    g::set_vector3_from_float32_array(out, 1, f32({0, 1, 2, 3, 4}));
    check(is3(out, 1, 2, 3), "setVector3FromFloat32Array reads x/y/z from the array at offset");
  }
  {
    auto out = v3();
    g::set_vector3_from_spherical(out, 5, 0, 0);
    check(near3(out, 0, 5, 0, 6), "setVector3FromSpherical produces the north pole at theta=0");
  }
  {
    auto out = v3();
    g::set_vector3_from_vector4(out, lit4(1, 2, 3, 99));
    check(is3(out, 1, 2, 3), "setVector3FromVector4 copies x, y, z and drops w");
  }
  {
    auto out = v3();
    g::set_vector3_from_vector4(out, lit4(2, 4, 6, 2));
    check(is3(out, 2, 4, 6), "setVector3FromVector4 does not perform a perspective divide");
  }
  {
    auto result = v3();
    g::subtract_vector3(result, v3(4, 5, 6), v3(1, 2, 3));
    check(is3(result, 3, 3, 3), "subtractVector3 returns a new vector when no target is passed");
  }
  {
    auto a = v3(4, 5, 6), b = v3(1, 2, 3);
    g::subtract_vector3(a, a, b);
    check(is3(a, 3, 3, 3), "subtractVector3 supports out === source");
  }
  {
    auto a = v3(4, 5, 6), b = v3(1, 2, 3);
    g::subtract_vector3(b, a, b);
    check(is3(b, 3, 3, 3), "subtractVector3 supports out === other");
  }
  {
    auto out = v3();
    g::transform_vector3_by_matrix3(out, v3(1, 2, 3), mat3({1, 0, 0, 0, 1, 0, 0, 0, 1}));
    check(near3(out, 1, 2, 3, 6), "transformVector3ByMatrix3 transforms by an identity matrix");
  }
  {
    auto out = v3();
    g::transform_vector3_by_matrix3(out, v3(1, 1, 1), mat3({2, 0, 0, 0, 3, 0, 0, 0, 4}));
    check(near3(out, 2, 3, 4, 6), "transformVector3ByMatrix3 transforms by a scale matrix");
  }
  {
    auto v = v3(1, 2, 3);
    g::transform_vector3_by_matrix3(v, v, mat3({1, 0, 0, 0, 1, 0, 0, 0, 1}));
    check(is3(v, 1, 2, 3), "transformVector3ByMatrix3 supports out === source");
  }
  {
    flight::Float32Array arr(std::size_t{5});
    g::write_vector3_to_float32_array(arr, 1, v3(1, 2, 3));
    check(same(arr.get_index(0), 0) && same(arr.get_index(1), 1) && same(arr.get_index(2), 2) && same(arr.get_index(3), 3),
          "writeVector3ToFloat32Array writes x/y/z to the array at offset");
  }
  check(static_cast<bool>(g::vector3_x_axis) && same(axis<"x">(g::vector3_x_axis), 1) && same(axis<"y">(g::vector3_x_axis), 0) && same(axis<"z">(g::vector3_x_axis), 0),
        "X_AXIS returns the unit vector along the X-axis");
  check(static_cast<bool>(g::vector3_y_axis) && same(axis<"x">(g::vector3_y_axis), 0) && same(axis<"y">(g::vector3_y_axis), 1) && same(axis<"z">(g::vector3_y_axis), 0),
        "Y_AXIS returns the unit vector along the Y-axis");
  check(static_cast<bool>(g::vector3_z_axis) && same(axis<"x">(g::vector3_z_axis), 0) && same(axis<"y">(g::vector3_z_axis), 0) && same(axis<"z">(g::vector3_z_axis), 1),
        "Z_AXIS returns the unit vector along the Z-axis");
}

void test_vector4() {
  {
    auto result = v4();
    g::add_vector4(result, v4(1, 2, 3, 10), v4(4, 5, 6, 10));
    check(is4(result, 5, 7, 9, 20), "addVector4 returns a new vector when no target is passed");
  }
  {
    auto a = v4(1, 2, 3, 4);
    g::add_vector4(a, a, a);
    check(is4(a, 2, 4, 6, 8), "addVector4 modifies target when same object is passed as target");
  }
  {
    auto a = v4(1, 2, 3, 4), b = v4(4, 5, 6, 7);
    g::add_vector4(a, a, b);
    check(is4(a, 5, 7, 9, 11), "addVector4 supports out === a");
  }
  {
    auto a = v4(1, 2, 3, 4), b = v4(4, 5, 6, 7);
    g::add_vector4(b, a, b);
    check(is4(b, 5, 7, 9, 11), "addVector4 supports out === b");
  }
  {
    auto result = lit4(0, 0, 0, 0);
    g::add_vector4(result, lit4(1, 2, 3, 10), lit4(4, 5, 6, 10));
    check(is4(result, 5, 7, 9, 20), "addVector4 allows vector-like objects");
  }
  {
    auto min = v4(-1, -2, -3, -4), max = v4(10, 20, 30, 40), out = v4();
    g::clamp_vector4(out, v4(-5, -6, -7, -8), min, max);
    bool ok = is4(out, -1, -2, -3, -4);
    g::clamp_vector4(out, v4(100, 200, 300, 400), min, max);
    ok = ok && is4(out, 10, 20, 30, 40);
    g::clamp_vector4(out, v4(0, 1, 2, 3), min, max);
    check(ok && is4(out, 0, 1, 2, 3), "clampVector4 clamps each component to its own bounds, below, above, and not at all");
  }
  {
    auto out = v4();
    g::clamp_vector4(out, v4(5, -2, 1, 10), v4(0, 0, 0, 0), v4(3, 3, 3, 3));
    check(is4(out, 3, 0, 1, 3), "clampVector4 clamps each component independently");
  }
  {
    auto value = v4(5, -2, 1, 10);
    g::clamp_vector4(value, value, v4(0, 0, 0, 0), v4(3, 3, 3, 3));
    auto min = v4(0, 0, 0, 0);
    g::clamp_vector4(min, v4(5, -2, 1, 10), min, v4(3, 3, 3, 3));
    auto max = v4(3, 3, 3, 3);
    g::clamp_vector4(max, v4(5, -2, 1, 10), v4(0, 0, 0, 0), max);
    check(is4(value, 3, 0, 1, 3) && is4(min, 3, 0, 1, 3) && is4(max, 3, 0, 1, 3), "clampVector4 supports out aliasing any input");
  }
  {
    auto out = v4();
    g::clamp_vector4(out, v4(-1, 2, 5, -1), v4(0, 0, 0, 0), v4(3, 3, 3, 3));
    bool ok = is4(out, 0, 2, 3, 0);
    g::clamp_vector4(out, v4(1, 5, -1, 2), v4(0, 0, 0, 0), v4(3, 3, 3, 3));
    check(ok && is4(out, 1, 3, 0, 2), "clampVector4 covers low, middle, and high outcomes on every component");
  }
  {
    auto original = v4(1, 2, 3, 4);
    auto cloned = g::clone_vector4(original);
    check(cloned != original && cloned != nullptr && is4(cloned, 1, 2, 3, 4), "cloneVector4 creates a new independent vector");
  }
  {
    auto original = lit4(1, 2, 3, 4);
    auto cloned = g::clone_vector4(original);
    check(cloned != original && cloned != nullptr && is4(cloned, 1, 2, 3, 4), "cloneVector4 allows vector-like objects");
  }
  {
    auto target = v4();
    g::copy_vector4(target, v4(1, 2, 3, 4));
    check(is4(target, 1, 2, 3, 4), "copyVector4 copies values from source to target");
  }
  {
    auto vector = v4(1, 2, 3, 4);
    g::copy_vector4(vector, vector);
    check(is4(vector, 1, 2, 3, 4), "copyVector4 does not affect source when same object is used for input and output");
  }
  check(is4(g::create_vector4(), 0, 0, 0, 0), "createVector4 creates a createVector4 with default values");
  check(is4(v4(1, 2, 3, 4), 1, 2, 3, 4), "createVector4 creates a createVector4 with specified values");
  {
    auto source = v4(6, 8, 9, 10), out = v4();
    g::divide_vector4(out, source, v4(0, 4, 3, 5));
    bool ok = is4(out, 0, 2, 3, 2);
    g::divide_vector4(out, source, v4(2, 0, 3, 5));
    ok = ok && is4(out, 3, 0, 3, 2);
    g::divide_vector4(out, source, v4(2, 4, 0, 5));
    ok = ok && is4(out, 3, 2, 0, 2);
    g::divide_vector4(out, source, v4(2, 4, 3, 0));
    check(ok && is4(out, 3, 2, 3, 0), "divideVector4 gives 0 for a zero divisor on each component in turn");
  }
  {
    auto out = v4();
    g::divide_vector4(out, v4(6, 8, 9, 12), v4(2, 4, 3, 6));
    check(is4(out, 3, 2, 3, 2), "divideVector4 divides component-wise");
  }
  {
    auto source = v4(6, 8, 9, 12);
    g::divide_vector4(source, source, v4(2, 4, 3, 6));
    auto divisor = v4(2, 4, 3, 6);
    g::divide_vector4(divisor, v4(6, 8, 9, 12), divisor);
    check(is4(source, 3, 2, 3, 2) && is4(divisor, 3, 2, 3, 2), "divideVector4 supports out === source or divisor");
  }
  {
    auto out = v4();
    g::divide_vector4(out, v4(6, 8, 9, 12), v4(0, 4, 0, 6));
    check(is4(out, 0, 2, 0, 2), "divideVector4 produces 0 for zero divisor components");
  }
  {
    auto out = v4();
    g::divide_vector4(out, v4(6, 8, 9, 12), v4(2, 0, 3, 0));
    check(is4(out, 3, 0, 3, 0), "divideVector4 produces 0 for zero y and w divisors");
  }
  check(g::equals_vector4(v4(), null_value) == false && g::equals_vector4(undefined_value, v4()) == false, "equalsVector4 returns false when either vector is absent");
  check(g::equals_vector4(v4(1, 2, 3, 4), v4(1, 2, 3, 4)) == true, "equalsVector4 returns true if vectors are equal");
  check(g::equals_vector4(v4(1, 2, 3, 4), v4(4, 5, 6, 7)) == false, "equalsVector4 returns false if vectors are not equal");
  {
    auto a = v4(1, 2, 3, 4);
    check(!g::equals_vector4(a, v4(9, 2, 3, 4)) && !g::equals_vector4(a, v4(1, 9, 3, 4)) && !g::equals_vector4(a, v4(1, 2, 9, 4)) && !g::equals_vector4(a, v4(1, 2, 3, 9)),
          "equalsVector4 compares every component, not just the first");
  }
  {
    auto v = v4(1, 2, 3, 4);
    check(!g::equals_vector4(v, null_value) && !g::equals_vector4(null_value, v) && !g::equals_vector4(v, undefined_value) &&
              !g::equals_vector4(undefined_value, v) && !g::equals_vector4(null_value, null_value) && !g::equals_vector4(undefined_value, undefined_value),
          "equalsVector4 returns false when either side is missing");
  }
  {
    auto not_a_number = v4(nan, 2, 3, 4);
    check(g::equals_vector4(not_a_number, not_a_number) == true && g::equals_vector4(not_a_number, v4(nan, 2, 3, 4)) == false,
          "equalsVector4 holds a vector equal to itself even with a NaN component");
  }
  {
    auto a = v4(1, 0, 0, 0);
    check(close(g::get_vector4_angle_between(a, a), 0), "getVector4AngleBetween returns 0 for identical vectors");
  }
  check(close(g::get_vector4_angle_between(v4(1, 0, 0, 0), v4(0, 1, 0, 0)), pi / 2), "getVector4AngleBetween returns PI/2 for perpendicular vectors");
  check(std::isnan(g::get_vector4_angle_between(v4(0, 0, 0, 0), v4(1, 0, 0, 0))), "getVector4AngleBetween returns NaN for a zero-length vector");
  check(close(g::get_vector4_distance(v4(1, 1, 1, 1), v4(4, 5, 6, 5)), std::sqrt(66.0)), "getVector4Distance returns the distance between two vectors");
  check(same(g::get_vector4_distance_squared(v4(1, 1, 1, 1), v4(4, 5, 6, 5)), 66), "getVector4DistanceSquared returns the squared distance between two vectors");
  check(same(g::get_vector4_dot(v4(1, 2, 3, 4), v4(4, 5, 6, 7)), 1 * 4 + 2 * 5 + 3 * 6 + 4 * 7), "getVector4Dot returns the dot product of two vectors");
  check(same(g::get_vector4_length(v4(3, 4, 0, 12)), 13), "getVector4Length returns the length of the vector");
  check(same(g::get_vector4_length(lit4(3, 4, 0, 12)), 13), "getVector4Length allows a vector-like object");
  check(same(g::get_vector4_length_squared(v4(3, 4, 0, 12)), 169), "getVector4LengthSquared returns the squared length of the vector");
  check(same(g::get_vector4_length_squared(lit4(3, 4, 0, 12)), 169), "getVector4LengthSquared allows a vector-like object");
  {
    auto out = v4(), a = v4(1, 2, 3, 4), b = v4(5, 6, 7, 8);
    g::interpolate_vector4(out, a, b, 0);
    bool ok = is4(out, 1, 2, 3, 4);
    g::interpolate_vector4(out, a, b, 1);
    check(ok && is4(out, 5, 6, 7, 8), "interpolateVector4 returns a at t=0 and b at t=1");
  }
  {
    auto out = v4();
    g::interpolate_vector4(out, v4(0, 0, 0, 0), v4(2, 4, 6, 8), 0.5);
    check(is4(out, 1, 2, 3, 4), "interpolateVector4 interpolates midpoint at t=0.5");
  }
  {
    auto a = v4(0, 0, 0, 0);
    g::interpolate_vector4(a, a, v4(2, 4, 6, 8), 0.5);
    check(is4(a, 1, 2, 3, 4), "interpolateVector4 supports out === a");
  }
  {
    auto b = v4(2, 4, 6, 8);
    g::interpolate_vector4(b, v4(0, 0, 0, 0), b, 0.5);
    check(is4(b, 1, 2, 3, 4), "interpolateVector4 supports out === b");
  }
  {
    auto a = v4(1, 5, 2, 9), b = v4(3, 0, 7, 4), out = v4();
    g::max_vector4(out, a, b);
    bool ok = is4(out, 3, 5, 7, 9);
    g::max_vector4(out, b, a);
    check(ok && is4(out, 3, 5, 7, 9), "maxVector4 takes each component from whichever side wins, and gives the same answer either way round");
  }
  {
    auto out = v4();
    g::max_vector4(out, v4(1, 5, 2, 9), v4(3, 2, 7, 4));
    check(is4(out, 3, 5, 7, 9), "maxVector4 returns the component-wise maximum");
  }
  {
    auto a = v4(1, 5, 2, 9);
    g::max_vector4(a, a, v4(3, 2, 7, 4));
    auto b = v4(3, 2, 7, 4);
    g::max_vector4(b, v4(1, 5, 2, 9), b);
    check(is4(a, 3, 5, 7, 9) && is4(b, 3, 5, 7, 9), "maxVector4 supports out === a or b");
  }
  {
    auto out = v4();
    g::max_vector4(out, v4(5, 1, 7, 2), v4(2, 3, 4, 9));
    check(is4(out, 5, 3, 7, 9), "maxVector4 selects the complementary component outcomes");
  }
  {
    auto a = v4(1, 5, 2, 9), b = v4(3, 0, 7, 4), out = v4();
    g::min_vector4(out, a, b);
    bool ok = is4(out, 1, 0, 2, 4);
    g::min_vector4(out, b, a);
    check(ok && is4(out, 1, 0, 2, 4), "minVector4 takes each component from whichever side wins, and gives the same answer either way round");
  }
  {
    auto out = v4();
    g::min_vector4(out, v4(1, 5, 2, 9), v4(3, 2, 7, 4));
    check(is4(out, 1, 2, 2, 4), "minVector4 returns the component-wise minimum");
  }
  {
    auto a = v4(1, 5, 2, 9);
    g::min_vector4(a, a, v4(3, 2, 7, 4));
    auto b = v4(3, 2, 7, 4);
    g::min_vector4(b, v4(1, 5, 2, 9), b);
    check(is4(a, 1, 2, 2, 4) && is4(b, 1, 2, 2, 4), "minVector4 supports out === a or b");
  }
  {
    auto out = v4();
    g::min_vector4(out, v4(5, 1, 7, 2), v4(2, 3, 4, 9));
    check(is4(out, 2, 1, 4, 2), "minVector4 selects the complementary component outcomes");
  }
  {
    auto out = v4();
    g::multiply_vector4(out, v4(2, 3, 4, 5), v4(6, 7, 8, 9));
    check(is4(out, 12, 21, 32, 45), "multiplyVector4 multiplies component-wise (Hadamard)");
  }
  {
    auto a = v4(2, 3, 4, 5);
    g::multiply_vector4(a, a, v4(6, 7, 8, 9));
    auto b = v4(6, 7, 8, 9);
    g::multiply_vector4(b, v4(2, 3, 4, 5), b);
    check(is4(a, 12, 21, 32, 45) && is4(b, 12, 21, 32, 45), "multiplyVector4 supports out === a or b");
  }
  {
    auto a = v4(1, 2, 3, 4);
    check(g::near_equals_vector4(a, a) == true, "nearEqualsVector4 returns true for identical vectors");
  }
  check(g::near_equals_vector4(v4(1, 2, 3, 4), v4(1 + 1e-7, 2, 3, 4)) == true, "nearEqualsVector4 returns true when difference is within default tolerance");
  check(g::near_equals_vector4(v4(1, 2, 3, 4), v4(1 + 1e-5, 2, 3, 4)) == false, "nearEqualsVector4 returns false when difference exceeds default tolerance");
  check(g::near_equals_vector4(v4(1, 2, 3, 4), v4(1.05, 2, 3, 4), 0.1) == true && g::near_equals_vector4(v4(1, 2, 3, 4), v4(1.05, 2, 3, 4), 0.01) == false,
        "nearEqualsVector4 respects a custom tolerance");
  {
    auto result = v4();
    g::negate_vector4(result, v4(1, -2, 3, -4));
    check(is4(result, -1, 2, -3, 4), "negateVector4 inverts the values of the vector components");
  }
  {
    auto v = v4(1, -2, 3, -4);
    g::negate_vector4(v, v);
    check(is4(v, -1, 2, -3, 4), "negateVector4 supports out === source");
  }
  {
    auto result = v4();
    const double length = g::normalize_vector4(result, v4(3, 4, 0, 0));
    check(close(result->x, 0.6, 5) && close(result->y, 0.8, 5) && same(result->z, 0) && same(result->w, 0) && same(length, 5), "normalizeVector4 normalizes the vector");
  }
  {
    auto v = v4(3, 4, 0, 0);
    const double length = g::normalize_vector4(v, v);
    check(close(v->x, 0.6, 5) && close(v->y, 0.8, 5) && same(v->z, 0) && same(v->w, 0) && same(length, 5), "normalizeVector4 supports out === source");
  }
  {
    auto result = v4(1, 2, 3, 4);
    const double length = g::normalize_vector4(result, v4(0, 0, 0, 0));
    check(is4(result, 0, 0, 0, 0) && same(length, 0), "normalizeVector4 writes zero to out for a zero-length source");
  }
  {
    auto result = v4();
    g::offset_vector4(result, v4(1, 2, 3, 4), 10, 20, 30, 40);
    check(is4(result, 11, 22, 33, 44), "offsetVector4 offsets each component by a scalar per axis");
  }
  {
    auto result = v4();
    g::offset_vector4(result, v4(1, 2, 3, 4), -5, -10, -15, -20);
    check(is4(result, -4, -8, -12, -16), "offsetVector4 works with negative deltas");
  }
  {
    auto v = v4(1, 2, 3, 4);
    g::offset_vector4(v, v, 3, 4, 5, 6);
    check(is4(v, 4, 6, 8, 10), "offsetVector4 supports out === source");
  }
  {
    auto v = g::create_vector4(10, 20, 30);
    v->w = 5;
    auto result = v3();
    g::project_vector4(result, v);
    check(is3(result, 2, 4, 6), "projectVector4 modifies target when same object is passed as target");
  }
  {
    auto out = v4();
    g::reflect_vector4(out, v4(1, 0, 0, 0), v4(1, 0, 0, 0));
    check(near4(out, -1, 0, 0, 0, 6), "reflectVector4 reflects incident vector about a normal");
  }
  {
    auto v = v4(1, 0, 0, 0);
    g::reflect_vector4(v, v, v4(1, 0, 0, 0));
    check(close(v->x, -1, 6), "reflectVector4 supports out === incident");
  }
  {
    auto normal = v4(1, 0, 0, 0);
    g::reflect_vector4(normal, v4(1, 0, 0, 0), normal);
    check(near4(normal, -1, 0, 0, 0, 6), "reflectVector4 supports out === normal");
  }
  {
    auto result = v4();
    g::scale_vector4(result, v4(1, 1, 1, 1), 2);
    check(is4(result, 2, 2, 2, 2), "scaleVector4 scales the vector by a scalar");
  }
  {
    auto v = v4(1, 1, 1, 1);
    g::scale_vector4(v, v, 2);
    check(is4(v, 2, 2, 2, 2), "scaleVector4 supports out === source");
  }
  {
    auto v = v4();
    g::set_vector4(v, 5, 10, 15, 5);
    check(is4(v, 5, 10, 15, 5), "setVector4 sets the values of the vector");
  }
  {
    auto v = v4(1, 2, 3, 4);
    g::set_vector4(v, 5, 10, 15, 5);
    check(is4(v, 5, 10, 15, 5), "setVector4 modifies target when same object is passed as target");
  }
  {
    auto out = v4();
    g::set_vector4_from_float32_array(out, 1, f32({0, 1, 2, 3, 4, 5}));
    check(is4(out, 1, 2, 3, 4), "setVector4FromFloat32Array reads x/y/z/w from the array at offset");
  }
  {
    auto out = v4();
    g::set_vector4_from_vector3(out, lit3(1, 2, 3));
    check(is4(out, 1, 2, 3, 0), "setVector4FromVector3 copies x, y, z and sets w to 0 by default");
  }
  {
    auto out = v4();
    g::set_vector4_from_vector3(out, lit3(1, 2, 3), 1);
    check(same(out->w, 1), "setVector4FromVector3 sets w to the provided value");
  }
  {
    auto out = v4();
    g::set_vector4_from_vector3(out, v3(5, 6, 7), 0);
    check(is4(out, 5, 6, 7, 0), "setVector4FromVector3 is safe when out aliases a part of source via a plain object");
  }
  {
    auto result = v4();
    g::subtract_vector4(result, v4(4, 5, 6, 7), v4(1, 2, 3, 4));
    check(is4(result, 3, 3, 3, 3), "subtractVector4 returns a new vector when no target is passed");
  }
  {
    auto a = v4(4, 5, 6, 7), b = v4(1, 2, 3, 4);
    g::subtract_vector4(a, a, b);
    check(is4(a, 3, 3, 3, 3), "subtractVector4 supports out === source");
  }
  {
    auto a = v4(4, 5, 6, 7), b = v4(1, 2, 3, 4);
    g::subtract_vector4(b, a, b);
    check(is4(b, 3, 3, 3, 3), "subtractVector4 supports out === other");
  }
  {
    const auto& w_unit = g::vector4_w_unit;
    check(static_cast<bool>(w_unit) && same(axis<"x">(w_unit), 0) && same(axis<"y">(w_unit), 0) && same(axis<"z">(w_unit), 0) && same(axis<"w">(w_unit), 1),
          "W_UNIT returns the unit vector along the W-dimension");
  }
  {
    flight::Float32Array arr(std::size_t{6});
    g::write_vector4_to_float32_array(arr, 1, v4(1, 2, 3, 4));
    check(same(arr.get_index(0), 0) && same(arr.get_index(1), 1) && same(arr.get_index(2), 2) && same(arr.get_index(3), 3) && same(arr.get_index(4), 4),
          "writeVector4ToFloat32Array writes x/y/z/w to the array at offset");
  }
  {
    const auto& x_axis = g::vector4_x_axis;
    check(static_cast<bool>(x_axis) && same(axis<"x">(x_axis), 1) && same(axis<"y">(x_axis), 0) && same(axis<"z">(x_axis), 0) && same(axis<"w">(x_axis), 0),
          "Vector4 X_AXIS returns the unit vector along the X-axis");
  }
  {
    const auto& y_axis = g::vector4_y_axis;
    check(static_cast<bool>(y_axis) && same(axis<"x">(y_axis), 0) && same(axis<"y">(y_axis), 1) && same(axis<"z">(y_axis), 0) && same(axis<"w">(y_axis), 0),
          "Vector4 Y_AXIS returns the unit vector along the Y-axis");
  }
  {
    const auto& z_axis = g::vector4_z_axis;
    check(static_cast<bool>(z_axis) && same(axis<"x">(z_axis), 0) && same(axis<"y">(z_axis), 0) && same(axis<"z">(z_axis), 1) && same(axis<"w">(z_axis), 0),
          "Vector4 Z_AXIS returns the unit vector along the Z-axis");
  }
}

static_assert(std::is_function_v<decltype(g::initialize_vector2)>, "initializeVector2 is the construction initializer of createVector2");
static_assert(std::is_function_v<decltype(g::initialize_vector3)>, "initializeVector3 is the construction initializer of createVector3");
static_assert(std::is_function_v<decltype(g::initialize_vector4)>, "initializeVector4 is the construction initializer of createVector4");


}  // namespace

int main() {
  test_vector2();
  test_vector3_a();
  test_vector3_b();
  test_vector4();
  return failures == 0 ? 0 : 1;
}
