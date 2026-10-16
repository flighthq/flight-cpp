#include <flight/math/contract.hpp>

#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <initializer_list>
#include <string>
#include <type_traits>
#include <vector>

// Native port of every case in packages/math/src/*.test.ts at a627849 (325 cases across 15 files), against
// the generated headers; math has no override. Each check names the upstream case it ports. Two cases
// assert only a JavaScript typeof (hashCombine returns a number, randomBool returns a boolean), and the
// C++ return type establishes those, so they are static_asserts. Nothing is excluded.

namespace {

namespace math = flight::math;

int failures = 0;

void check(bool condition, const std::string& message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

// vitest's toBeCloseTo(expected, digits): |actual - expected| < 10^-digits / 2.
bool close(double actual, double expected, int digits = 2) {
  if (actual == expected) return true;
  return std::abs(actual - expected) < std::pow(10.0, -digits) / 2.0;
}

// vitest's toBe: Object.is, so -0 is not 0 and NaN is NaN.
bool same(double actual, double expected) {
  if (std::isnan(actual) || std::isnan(expected)) return std::isnan(actual) && std::isnan(expected);
  return actual == expected && std::signbit(actual) == std::signbit(expected);
}

const double pi = M_PI;
const double nan = std::numeric_limits<double>::quiet_NaN();
const double infinity = std::numeric_limits<double>::infinity();

// The emitter disambiguates the degToRad/radToDeg functions from the DEG_TO_RAD/RAD_TO_DEG constants,
// which snake-case to the same C++ names, by suffixing all four. These are the names emitted consumers
// (swf, physics3d, gizmo) already call.
double deg_to_rad(double degrees) { return math::deg_to_rad_flight_value_function_deg_u000054_o_u000052_ad_flight_source_59eefa411bb2f66d(degrees); }
double rad_to_deg(double radians) { return math::rad_to_deg_flight_value_function_rad_u000054_o_u000044_eg_flight_source_59eefa411bb2f66d(radians); }
const double& DEG_TO_RAD = math::deg_to_rad_flight_value_variable__u000044__u000045__u000047__u00005f__u000054__u00004f__u00005f__u000052__u000041__u000044__flight_source_ad5040a7d8c07bd3;
const double& RAD_TO_DEG = math::rad_to_deg_flight_value_variable__u000052__u000041__u000044__u00005f__u000054__u00004f__u00005f__u000044__u000045__u000047__flight_source_ad5040a7d8c07bd3;

void test_angle() {
  check(same(deg_to_rad(0), 0), "degToRad converts 0 degrees to 0 radians");
  check(close(deg_to_rad(180), pi, 10), "degToRad converts 180 degrees to pi");
  check(close(deg_to_rad(360), pi * 2, 10), "degToRad converts 360 degrees to 2pi");
  check(close(deg_to_rad(90), pi / 2, 10), "degToRad converts 90 degrees to pi/2");
  check(close(deg_to_rad(-90), -pi / 2, 10), "degToRad converts negative degrees");

  check(close(math::delta_angle(1, 1), 0, 10), "deltaAngle returns 0 for identical angles");
  check(close(math::delta_angle(pi - 0.1, -(pi - 0.1)), 0.2, 5), "deltaAngle returns the shortest arc when crossing 0");
  check(close(math::delta_angle(0, pi / 2), pi / 2, 10), "deltaAngle returns a positive delta when the target is ahead");
  check(close(math::delta_angle(pi / 2, 0), -pi / 2, 10), "deltaAngle returns a negative delta when the target is behind");
  check(math::delta_angle(0, pi) == pi, "deltaAngle keeps the positive half-turn in its documented (-pi, pi] range");
  {
    bool ok = true;
    for (int i = -10; i <= 10; ++i) {
      const double d = math::delta_angle(i, i + 3);
      ok = ok && d > -pi && d <= pi;
    }
    check(ok, "deltaAngle result is always within (-pi, pi]");
  }

  check(close(math::normalize_angle(0), 0, 10), "normalizeAngle normalizes 0 to 0");
  check(close(math::normalize_angle(pi), -pi, 5), "normalizeAngle normalizes pi to -pi (range is [-pi, pi))");
  check(close(math::normalize_angle(pi * 2), 0, 10), "normalizeAngle wraps 2pi to 0");
  check(close(math::normalize_angle(pi + 1), -(pi - 1), 10), "normalizeAngle wraps values above pi to the negative side");
  check(close(math::normalize_angle(-pi * 3), -pi, 5), "normalizeAngle wraps negative values into [-pi, pi)");
  {
    bool ok = true;
    for (int i = -20; i <= 20; ++i) {
      const double a = math::normalize_angle(i);
      ok = ok && a >= -pi && a < pi;
    }
    check(ok, "normalizeAngle result is always in [-pi, pi)");
  }

  check(same(rad_to_deg(0), 0), "radToDeg converts 0 radians to 0 degrees");
  check(close(rad_to_deg(pi), 180, 10), "radToDeg converts pi to 180 degrees");
  check(close(rad_to_deg(pi * 2), 360, 10), "radToDeg converts 2pi to 360 degrees");
  check(close(rad_to_deg(deg_to_rad(45)), 45, 10), "radToDeg is the inverse of degToRad");
}

void test_clamp() {
  check(same(math::clamp(-5, 0, 10), 0), "clamp clamps below minimum");
  check(same(math::clamp(15, 0, 10), 10), "clamp clamps above maximum");
  check(same(math::clamp(5, 0, 10), 5), "clamp leaves value within range unchanged");
  check(same(math::clamp(0, 0, 10), 0), "clamp leaves value equal to min unchanged");
  check(same(math::clamp(10, 0, 10), 10), "clamp leaves value equal to max unchanged");
  check(std::isnan(math::clamp(nan, 0, 10)), "clamp propagates NaN");

  check(math::in_range(5, 0, 10) == true, "inRange returns true for a value within range");
  check(math::in_range(0, 0, 10) == true, "inRange returns true at the minimum boundary");
  check(math::in_range(10, 0, 10) == true, "inRange returns true at the maximum boundary");
  check(math::in_range(-1, 0, 10) == false, "inRange returns false below minimum");
  check(math::in_range(11, 0, 10) == false, "inRange returns false above maximum");

  check(same(math::saturate(-1), 0), "saturate clamps below 0 to 0");
  check(same(math::saturate(2), 1), "saturate clamps above 1 to 1");
  check(same(math::saturate(0.5), 0.5), "saturate leaves value within [0, 1] unchanged");
  check(same(math::saturate(0), 0), "saturate leaves 0 unchanged");
  check(same(math::saturate(1), 1), "saturate leaves 1 unchanged");
  check(same(math::saturate(nan), 0), "saturate returns 0 for NaN (GPU semantics)");
}

void test_comparison() {
  check(math::approx_equal(1, 1) == true, "approxEqual returns true for identical values");
  check(math::approx_equal(1, 1 + 1e-7) == true, "approxEqual returns true when values are within the default epsilon");
  check(math::approx_equal(1, 1 + 1e-5) == false, "approxEqual returns false when values differ by more than the default epsilon");
  check(math::approx_equal(1, 1.1, 0.2) == true && math::approx_equal(1, 1.3, 0.2) == false, "approxEqual accepts a custom epsilon");
  check(math::approx_equal(0, 1e-7) == true, "approxEqual returns true for zero and near-zero");
  check(math::approx_equal(-1, -1 - 1e-7) == true, "approxEqual works with negative values");

  check(math::approx_equal_relative(1000, 1000) == true, "approxEqualRelative returns true for identical values");
  check(math::approx_equal_relative(1e8, 1e8 + 1) == true, "approxEqualRelative returns true for large values within relative epsilon");
  check(math::approx_equal_relative(1e8, 1e8 * 2) == false, "approxEqualRelative returns false when large values differ beyond relative epsilon");
  check(math::approx_equal_relative(0, 1e-7) == true, "approxEqualRelative returns true for near-zero values within absolute epsilon");

  check(math::approx_zero(0) == true, "approxZero returns true for exact zero");
  check(math::approx_zero(1e-7) == true, "approxZero returns true for values within epsilon");
  check(math::approx_zero(1e-4) == false, "approxZero returns false for values outside epsilon");
  check(math::approx_zero(0.01, 0.1) == true && math::approx_zero(0.2, 0.1) == false, "approxZero accepts a custom epsilon");
}

void test_constants() {
  check(math::circle_kappa == (4 * (std::sqrt(2.0) - 1)) / 3, "CIRCLE_KAPPA equals four thirds of the square root of two less one");
  {
    const auto radius_at = [](double t) {
      const double u = 1 - t;
      const double x = u * u * u + 3 * u * u * t + 3 * u * t * t * math::circle_kappa;
      const double y = 3 * u * u * t * math::circle_kappa + 3 * u * t * t + t * t * t;
      return std::hypot(x, y);
    };
    bool ok = true;
    for (const double t : {0.0, 0.25, 0.5, 0.75, 1.0}) ok = ok && close(radius_at(t), 1, 3);
    check(ok, "CIRCLE_KAPPA sweeps a unit quarter circle when used as the cubic control distance");
  }
  check(close(180 * DEG_TO_RAD, pi, 10), "DEG_TO_RAD converts 180 degrees to pi");
  check(close(360 * DEG_TO_RAD, pi * 2, 10), "DEG_TO_RAD converts 360 degrees to 2pi");
  check(math::epsilon > 0, "EPSILON is a positive number");
  check(math::epsilon < 1e-5, "EPSILON is smaller than 1e-5");
  check(close(math::half_pi, pi / 2, 10), "HALF_PI equals pi / 2");
  check(close(pi * RAD_TO_DEG, 180, 10), "RAD_TO_DEG converts pi radians to 180 degrees");
  check(close(RAD_TO_DEG * DEG_TO_RAD, 1, 10), "RAD_TO_DEG is the reciprocal of DEG_TO_RAD");
  check(close(math::tau, pi * 2, 10), "TAU equals 2pi");
}

bool is_integer(double value) { return std::isfinite(value) && std::trunc(value) == value; }

void test_hash() {
  {
    auto rng = math::create_random_source_from_hash(3, 7);
    bool ok = true;
    for (int i = 0; i < 20; ++i) {
      const double v = rng();
      ok = ok && v >= 0 && v < 1;
    }
    check(ok, "createRandomSourceFromHash returns a random source that produces values in [0, 1)");
  }
  {
    auto a = math::create_random_source_from_hash(0, 0);
    auto b = math::create_random_source_from_hash(1, 0);
    bool differs = false;
    for (int i = 0; i < 10; ++i)
      if (a() != b()) differs = true;
    check(differs, "createRandomSourceFromHash produces different sequences for different (x, y) positions");
  }
  {
    auto a = math::create_random_source_from_hash(5, 9);
    auto b = math::create_random_source_from_hash(5, 9);
    bool ok = true;
    for (int i = 0; i < 20; ++i) ok = ok && a() == b();
    check(ok, "createRandomSourceFromHash produces the same sequence for the same (x, y)");
  }

  {
    const double h = math::hash_2d(3, 7);
    check(h >= 0 && is_integer(h), "hash2D returns a non-negative integer");
  }
  check(math::hash_2d(5, 10) == math::hash_2d(5, 10), "hash2D is deterministic");
  check(math::hash_2d(0, 0) != math::hash_2d(1, 0) && math::hash_2d(0, 0) != math::hash_2d(0, 1), "hash2D differs for different inputs");

  {
    const double h = math::hash_3d(1, 2, 3);
    check(h >= 0 && is_integer(h), "hash3D returns a non-negative integer");
  }
  check(math::hash_3d(1, 2, 3) == math::hash_3d(1, 2, 3), "hash3D is deterministic");
  check(math::hash_3d(0, 0, 0) != math::hash_3d(1, 0, 0) && math::hash_3d(0, 0, 0) != math::hash_3d(0, 0, 1), "hash3D differs for different inputs");

  static_assert(std::is_same_v<decltype(math::hash_combine(1, 2)), double>, "hashCombine returns a number");
  check(math::hash_combine(100, 200) == math::hash_combine(100, 200), "hashCombine is deterministic");
  check(math::hash_combine(1, 100) != math::hash_combine(2, 100), "hashCombine produces different results for different seeds");
  check(same(math::hash_combine(100, 200), 3347962280.0), "hashCombine pins the complete multi-term mix with a known-answer vector");

  {
    const double h = math::hash_uint32(42);
    check(h >= 0 && is_integer(h), "hashUint32 returns a non-negative integer");
  }
  check(math::hash_uint32(12345) == math::hash_uint32(12345), "hashUint32 is deterministic");
  check(math::hash_uint32(0) != math::hash_uint32(1) && math::hash_uint32(100) != math::hash_uint32(101), "hashUint32 produces different results for different inputs");
  check(math::hash_uint32(0) >= 0, "hashUint32 handles 0");
}

void test_interpolation() {
  check(same(math::inverse_lerp(0, 10, 0), 0), "inverseLerp returns 0 when value equals a");
  check(same(math::inverse_lerp(0, 10, 10), 1), "inverseLerp returns 1 when value equals b");
  check(same(math::inverse_lerp(0, 10, 5), 0.5), "inverseLerp returns 0.5 for the midpoint");
  check(same(math::inverse_lerp(5, 5, 5), 0), "inverseLerp returns 0 when a equals b to avoid division by zero");
  check(same(math::inverse_lerp(-10, 10, 0), 0.5), "inverseLerp works with negative ranges");

  check(same(math::lerp(0, 10, 0), 0), "lerp returns a when t is 0");
  check(same(math::lerp(0, 10, 1), 10), "lerp returns b when t is 1");
  check(same(math::lerp(0, 10, 0.5), 5), "lerp returns the midpoint for t = 0.5");
  check(same(math::lerp(0, 10, 2), 20) && same(math::lerp(0, 10, -1), -10), "lerp extrapolates when t is outside [0, 1]");
  check(same(math::lerp(-10, 10, 0.5), 0), "lerp works with negative values");

  check(same(math::remap(5, 0, 10, 0, 100), 50), "remap maps a value from one range to another");
  check(same(math::remap(0, 0, 10, 50, 150), 50), "remap maps the minimum input to the minimum output");
  check(same(math::remap(10, 0, 10, 50, 150), 150), "remap maps the maximum input to the maximum output");
  check(same(math::remap(5, 5, 5, 0, 100), 0), "remap returns outMin when inMin equals inMax");
  check(same(math::remap(5, 0, 10, 100, 0), 50), "remap works with inverted output ranges");

  check(same(math::smooth_step(0, 1, 0), 0), "smoothStep returns 0 at the lower edge");
  check(same(math::smooth_step(0, 1, 1), 1), "smoothStep returns 1 at the upper edge");
  check(same(math::smooth_step(0, 1, 0.5), 0.5), "smoothStep returns 0.5 at the midpoint");
  check(same(math::smooth_step(0, 1, -1), 0), "smoothStep returns 0 for x below the lower edge");
  check(same(math::smooth_step(0, 1, 2), 1), "smoothStep returns 1 for x above the upper edge");
  {
    const double h = 0.001;
    const double d_low = (math::smooth_step(0, 1, h) - math::smooth_step(0, 1, 0)) / h;
    const double d_high = (math::smooth_step(0, 1, 1) - math::smooth_step(0, 1, 1 - h)) / h;
    check(close(d_low, 0, 1) && close(d_high, 0, 1), "smoothStep has zero derivative at the edges");
  }

  check(same(math::step(0.5, 0.3), 0), "step returns 0 when x is less than edge");
  check(same(math::step(0.5, 0.5), 1), "step returns 1 when x equals edge");
  check(same(math::step(0.5, 0.7), 1), "step returns 1 when x is greater than edge");
}

void test_interpolation_advanced() {
  {
    const double result = math::damp(0, 10, 1, 1);
    check(result > 0 && result < 10, "damp moves current toward target over time");
  }
  check(same(math::damp(5, 10, 2, 0), 5), "damp returns current when deltaTime is 0");
  check(same(math::damp(5, 10, 0, 1), 5), "damp returns current when lambda is 0");
  {
    const double step_value = math::damp(0, 1, 5, 0.1);
    check(step_value > 0.39 && step_value < 0.41, "damp approaches target asymptotically");
  }
  {
    const double full = math::damp(0, 1, 2, 0.2);
    const double half1 = math::damp(0, 1, 2, 0.1);
    const double half2 = math::damp(half1, 1, 2, 0.1);
    check(close(half2, full, 10), "damp is frame-rate independent: two half-steps equal one full step");
  }

  check(close(math::lerp_angle(0, pi / 2, 0.5), pi / 4, 10), "lerpAngle interpolates between two close angles");
  {
    const double tau = pi * 2;
    const double result = math::lerp_angle(pi * 1.9, pi * 0.1, 0.5);
    const double normalised = std::fmod(std::fmod(result, tau) + tau, tau);
    check(close(normalised, 0, 5), "lerpAngle takes the shortest arc across 0/2pi");
  }
  check(close(math::lerp_angle(pi * 0.1, pi * 1.9, 0.5), 0, 10), "lerpAngle takes the shortest arc clockwise across 0/2pi");
  check(close(math::lerp_angle(1, 2, 0), 1, 10), "lerpAngle returns a at t = 0");
  check(close(math::lerp_angle(1, 2, 1), 2, 10), "lerpAngle returns b at t = 1");

  check(same(math::move_towards(0, 10, 3), 3), "moveTowards moves toward target by maxDelta");
  check(same(math::move_towards(9, 10, 5), 10), "moveTowards snaps to target when within maxDelta");
  check(same(math::move_towards(10, 0, 3), 7), "moveTowards works when target is below current");
  check(same(math::move_towards(5, 5, 1), 5), "moveTowards returns target when current equals target");

  check(same(math::ping_pong(0, 1), 0), "pingPong starts at 0");
  check(same(math::ping_pong(1, 1), 1), "pingPong reaches length at t = length");
  check(close(math::ping_pong(1.5, 1), 0.5, 10), "pingPong bounces back at t = 1.5 * length");
  check(std::isnan(math::ping_pong(1, 0)), "pingPong propagates NaN when length is 0");
  check(close(math::ping_pong(3, 2), 1, 10), "pingPong works with larger lengths");

  check(close(math::repeat(1.6, 1), 0.6, 10), "repeat wraps t over [0, length)");
  check(close(math::repeat(2, 1), 0, 10), "repeat returns 0 for exact multiple");
  {
    const double r = math::repeat(-0.3, 1);
    check(r >= 0 && r < 1, "repeat handles negative t");
  }
  check(std::isnan(math::repeat(1, 0)), "repeat propagates NaN when length is 0");

  check(same(math::smoother_step(0, 1, 0), 0), "smootherStep returns 0 at the lower edge");
  check(same(math::smoother_step(0, 1, 1), 1), "smootherStep returns 1 at the upper edge");
  check(same(math::smoother_step(0, 1, 0.5), 0.5), "smootherStep returns 0.5 at the midpoint");
  check(same(math::smoother_step(0, 1, -1), 0), "smootherStep returns 0 for x below the lower edge");
  check(same(math::smoother_step(0, 1, 2), 1), "smootherStep returns 1 for x above the upper edge");
  {
    const double h = 0.001;
    const double d_low = (math::smoother_step(0, 1, h) - math::smoother_step(0, 1, 0)) / h;
    const double d_high = (math::smoother_step(0, 1, 1) - math::smoother_step(0, 1, 1 - h)) / h;
    check(close(d_low, 0, 1) && close(d_high, 0, 1), "smootherStep has zero first and second derivatives at the edges");
  }
}

// expect(() => ...).toThrow(RangeError): the call must throw, and what it throws must be a RangeError.
template <typename Call>
bool throws_range_error(Call call) {
  try {
    call();
  } catch (const flight::RangeError&) {
    return true;
  } catch (...) {
    return false;
  }
  return false;
}

void test_next_power_of_two() {
  check(math::is_power_of_two(1) && math::is_power_of_two(2) && math::is_power_of_two(4) && math::is_power_of_two(64) && math::is_power_of_two(1024),
        "isPowerOfTwo returns true for powers of two");
  check(!math::is_power_of_two(3) && !math::is_power_of_two(5) && !math::is_power_of_two(100), "isPowerOfTwo returns false for non-powers of two");
  check(!math::is_power_of_two(0), "isPowerOfTwo returns false for 0");
  check(!math::is_power_of_two(-1) && !math::is_power_of_two(-4), "isPowerOfTwo returns false for negative numbers");

  check(same(math::next_multiple_of(7, 4), 8), "nextMultipleOf rounds up to the next multiple");
  check(same(math::next_multiple_of(8, 4), 8), "nextMultipleOf leaves an exact multiple unchanged");
  check(same(math::next_multiple_of(7, 0), 7), "nextMultipleOf returns value when multiple is 0");
  check(same(math::next_multiple_of(1, 16), 16) && same(math::next_multiple_of(0, 4), 0), "nextMultipleOf works with small values");

  check(same(math::next_power_of_two(0), 1), "nextPowerOfTwo returns 1 for 0");
  check(same(math::next_power_of_two(1), 1), "nextPowerOfTwo returns 1 for 1");
  check(same(math::next_power_of_two(2), 2) && same(math::next_power_of_two(64), 64) && same(math::next_power_of_two(128), 128) && same(math::next_power_of_two(1024), 1024),
        "nextPowerOfTwo returns an exact power of two unchanged");
  check(same(math::next_power_of_two(3), 4) && same(math::next_power_of_two(5), 8) && same(math::next_power_of_two(100), 128) && same(math::next_power_of_two(129), 256) &&
            same(math::next_power_of_two(1000), 1024),
        "nextPowerOfTwo rounds up to the next power of two");
  check(same(math::next_power_of_two(-1), 1), "nextPowerOfTwo handles negative numbers");

  check(same(math::previous_power_of_two(1), 1), "previousPowerOfTwo returns 1 for 1");
  check(same(math::previous_power_of_two(0), 1) && same(math::previous_power_of_two(-1), 1), "previousPowerOfTwo returns 1 for values <= 1");
  check(same(math::previous_power_of_two(4), 4) && same(math::previous_power_of_two(64), 64), "previousPowerOfTwo returns an exact power of two unchanged");
  check(same(math::previous_power_of_two(6), 4) && same(math::previous_power_of_two(100), 64) && same(math::previous_power_of_two(200), 128) &&
            same(math::previous_power_of_two(1025), 1024),
        "previousPowerOfTwo rounds down to the previous power of two");
  check(same(math::previous_power_of_two(1073741824), 1073741824) && same(math::previous_power_of_two(2147483647), 1073741824) &&
            same(math::next_power_of_two(1073741824), 1073741824),
        "previousPowerOfTwo returns a positive power of two at and above 2^30, where a signed shift sign-extended");
}

void test_number_theory() {
  check(same(math::factorial(0), 1), "factorial returns 1 for 0");
  check(same(math::factorial(1), 1), "factorial returns 1 for 1");
  check(same(math::factorial(5), 120) && same(math::factorial(10), 3628800), "factorial computes common factorials");
  check(throws_range_error([] { math::factorial(-1); }), "factorial throws for negative integers");
  check(throws_range_error([] { math::factorial(1.5); }), "factorial throws for non-integers");

  check(same(math::gcd(12, 8), 4) && same(math::gcd(100, 75), 25), "gcd returns the gcd of two positive integers");
  check(same(math::gcd(6, 3), 3), "gcd returns the larger number when one is a multiple of the other");
  check(same(math::gcd(7, 13), 1), "gcd returns 1 for coprime numbers");
  check(same(math::gcd(-12, 8), 4), "gcd accepts negative inputs");
  check(same(math::gcd(0, 5), 5) && same(math::gcd(7, 0), 7), "gcd accepts zero as one argument");
  check(throws_range_error([] { math::gcd(0, 0); }), "gcd throws when both arguments are 0");
  check(throws_range_error([] { math::gcd(infinity, 0); }) && throws_range_error([] { math::gcd(0, -infinity); }) &&
            throws_range_error([] { math::gcd(nan, 0); }),
        "gcd rejects non-finite inputs instead of returning a non-finite value or looping");

  check(same(math::hypot2(3, 4), 25), "hypot2 returns x squared plus y squared");
  check(same(math::hypot2(0, 0), 0), "hypot2 returns 0 for zero inputs");
  {
    const double h = std::hypot(5.0, 12.0);
    check(close(math::hypot2(5, 12), h * h, 10), "hypot2 matches Math.hypot squared");
  }

  check(math::is_even(0) && math::is_even(2) && math::is_even(100), "isEven returns true for even numbers");
  check(!math::is_even(1) && !math::is_even(3) && !math::is_even(99), "isEven returns false for odd numbers");
  check(math::is_odd(1) && math::is_odd(3) && math::is_odd(99), "isOdd returns true for odd numbers");
  check(!math::is_odd(0) && !math::is_odd(2) && !math::is_odd(100), "isOdd returns false for even numbers");

  check(same(math::lcm(4, 6), 12) && same(math::lcm(3, 5), 15), "lcm returns the lcm of two positive integers");
  check(same(math::lcm(4, 8), 8), "lcm returns the larger number when one is a multiple of the other");
  check(same(math::lcm(-4, 6), 12), "lcm accepts negative inputs");
  check(throws_range_error([] { math::lcm(infinity, 0); }) && throws_range_error([] { math::lcm(0, nan); }),
        "lcm rejects non-finite inputs through the shared GCD validation");
}

void test_random() {
  {
    auto rng = math::create_random_source(1);
    bool ok = true;
    for (int i = 0; i < 1000; ++i) {
      const double v = rng();
      ok = ok && v >= 0 && v < 1;
    }
    check(ok, "createRandomSource produces values in [0, 1)");
  }
  {
    auto a = math::create_random_source(0xc0ffee);
    auto b = math::create_random_source(0xc0ffee);
    bool ok = true;
    for (int i = 0; i < 100; ++i) ok = ok && a() == b();
    check(ok, "createRandomSource is deterministic: same seed yields the same sequence");
  }
  {
    auto a = math::create_random_source(1);
    auto b = math::create_random_source(2);
    bool differs = false;
    for (int i = 0; i < 10; ++i)
      if (a() != b()) differs = true;
    check(differs, "createRandomSource: different seeds yield different sequences");
  }
  {
    auto rng = math::create_random_source(0xc0ffee);
    const double v0 = rng(), v1 = rng(), v2 = rng(), v3 = rng();
    check(v0 == 0.021141508361324668 && v1 == 0.6661099966149777 && v2 == 0.7799714196007699 && v3 == 0.7395844468846917,
          "createRandomSource pins the mulberry32 transform with a known-answer sequence");
  }
  {
    auto rng = math::create_random_source(nan);
    check(std::isfinite(rng()), "createRandomSource tolerates non-finite seeds without producing NaN");
  }
}

flight::types::RandomSource seeded() { return math::create_random_source(0xabcdef); }
flight::types::RandomSource constant(double value) {
  return [value] { return value; };
}

void test_random_range() {
  static_assert(std::is_same_v<decltype(math::random_bool(seeded())), bool>, "randomBool returns only booleans");
  {
    auto random = seeded();
    bool ok = true;
    for (int i = 0; i < 10; ++i) ok = ok && math::random_bool(random, 1) == true;
    check(ok, "randomBool returns true with probability 1");
  }
  {
    auto random = seeded();
    bool ok = true;
    for (int i = 0; i < 10; ++i) ok = ok && math::random_bool(random, 0) == false;
    check(ok, "randomBool returns false with probability 0");
  }
  check(math::random_bool(constant(0.5), 0.5) == false, "randomBool uses a half-open comparison at the probability boundary");
  {
    auto a = seeded();
    auto b = seeded();
    bool ok = true;
    for (int i = 0; i < 20; ++i) ok = ok && math::random_bool(a) == math::random_bool(b);
    check(ok, "randomBool is deterministic for the same seed");
  }

  {
    auto random = seeded();
    bool ok = true;
    for (int i = 0; i < 200; ++i) {
      const double v = math::random_int(random, 0, 9);
      ok = ok && is_integer(v) && v >= 0 && v <= 9;
    }
    check(ok, "randomInt returns integers within [min, max]");
  }
  check(same(math::random_int(seeded(), 5, 5), 5), "randomInt returns min when min equals max");
  check(throws_range_error([] { math::random_int(seeded(), 10, 5); }), "randomInt throws when min > max");
  check(same(math::random_int(constant(0.999999), 2, 4), 4), "randomInt can select the inclusive upper endpoint");
  check(same(math::random_int(constant(0), -1.2, -0.2), -2), "randomInt rounds negative fractional bounds down");
  {
    auto a = seeded();
    auto b = seeded();
    bool ok = true;
    for (int i = 0; i < 20; ++i) ok = ok && math::random_int(a, 0, 100) == math::random_int(b, 0, 100);
    check(ok, "randomInt is deterministic for the same seed");
  }

  {
    auto random = seeded();
    bool ok = true;
    for (int i = 0; i < 200; ++i) {
      const double v = math::random_range(random, 5, 10);
      ok = ok && v >= 5 && v < 10;
    }
    check(ok, "randomRange returns values within [min, max)");
  }
  check(same(math::random_range(seeded(), 3, 3), 3), "randomRange returns min when min equals max");
  {
    auto a = seeded();
    auto b = seeded();
    bool ok = true;
    for (int i = 0; i < 20; ++i) ok = ok && math::random_range(a, 0, 100) == math::random_range(b, 0, 100);
    check(ok, "randomRange is deterministic for the same seed");
  }

  {
    auto random = seeded();
    bool ok = true;
    for (int i = 0; i < 100; ++i) {
      const double v = math::random_sign(random);
      ok = ok && (v == 1 || v == -1);
    }
    check(ok, "randomSign returns only 1 or -1");
  }
  {
    auto random = seeded();
    bool saw_positive = false, saw_negative = false;
    for (int i = 0; i < 100; ++i) {
      const double v = math::random_sign(random);
      if (v == 1) saw_positive = true;
      if (v == -1) saw_negative = true;
    }
    check(saw_positive && saw_negative, "randomSign produces both signs");
  }
  check(same(math::random_sign(constant(0.5)), 1), "randomSign selects the positive sign at the half-open boundary");
  {
    auto a = seeded();
    auto b = seeded();
    bool ok = true;
    for (int i = 0; i < 20; ++i) ok = ok && math::random_sign(a) == math::random_sign(b);
    check(ok, "randomSign is deterministic for the same seed");
  }
}

const double max_value = std::numeric_limits<double>::max();
const double min_value = std::numeric_limits<double>::denorm_min();
const double number_epsilon = std::numeric_limits<double>::epsilon();

flight::Array<double> values(std::initializer_list<double> items) {
  flight::Array<double> out;
  for (const double item : items) out.push(item);
  return out;
}

void test_rounding() {
  check(same(math::ceil_to(7, 5), 10), "ceilTo rounds up to the nearest step");
  check(same(math::ceil_to(10, 5), 10), "ceilTo leaves an exact multiple unchanged");
  check(same(math::ceil_to(7, 0), 7), "ceilTo returns value when step is 0");
  check(same(math::ceil_to(-7, 5), -5), "ceilTo works with negative values");

  check(same(math::euclidean_mod(7, 4), 3), "euclideanMod returns the remainder for positive inputs");
  check(same(math::euclidean_mod(-1, 4), 3) && same(math::euclidean_mod(-5, 4), 3), "euclideanMod returns a non-negative result for negative inputs");
  check(same(math::euclidean_mod(8, 4), 0), "euclideanMod returns 0 when value is a multiple of divisor");
  check(throws_range_error([] { math::euclidean_mod(7, 0); }), "euclideanMod throws for a zero divisor");

  check(same(math::floor_to(7, 5), 5), "floorTo rounds down to the nearest step");
  check(same(math::floor_to(10, 5), 10), "floorTo leaves an exact multiple unchanged");
  check(same(math::floor_to(7, 0), 7), "floorTo returns value when step is 0");
  check(same(math::floor_to(-7, 5), -10), "floorTo works with negative values");

  check(close(math::fract(3.75), 0.75, 10), "fract returns the fractional part of a positive number");
  check(same(math::fract(3), 0), "fract returns 0 for an integer");
  check(close(math::fract(-1.3), -0.3, 10), "fract preserves sign for negative numbers");
  check(same(math::fract(0), 0), "fract returns 0 for 0");

  check(same(math::round_to(5, 10), 10) && same(math::round_to(-5, 10), -10) && !same(math::round_to(-4, 10), -0.0),
        "roundTo rounds halfway ties away from zero symmetrically");
  check(same(math::round_to(7, 5), 5) && same(math::round_to(8, 5), 10), "roundTo rounds to the nearest step");
  check(same(math::round_to(10, 5), 10), "roundTo leaves an exact multiple unchanged");
  check(same(math::round_to(7, 0), 7), "roundTo returns value when step is 0");
  check(close(math::round_to(0.3, 0.1), 0.3, 10), "roundTo works with floating-point steps");
  check(same(math::round_to(-7, 5), -5), "roundTo works with negative values");
}

void test_scalar() {
  check(same(math::ceil_power_of_two(5), 8) && same(math::ceil_power_of_two(9), 16), "ceilPowerOfTwo rounds up to the next power of two");
  check(same(math::ceil_power_of_two(8), 8) && same(math::ceil_power_of_two(256), 256), "ceilPowerOfTwo leaves an exact power of two unchanged");
  check(same(math::floor_power_of_two(6), 4) && same(math::floor_power_of_two(100), 64), "floorPowerOfTwo rounds down to the previous power of two");
  check(same(math::floor_power_of_two(8), 8), "floorPowerOfTwo leaves an exact power of two unchanged");

  check(close(math::quantize(0.7, 4, 0, 1), 0.75, 10), "quantize quantizes to the nearest step");
  check(close(math::quantize(0, 4, 0, 1), 0, 10), "quantize returns min at the lower bound");
  check(close(math::quantize(1, 4, 0, 1), 1, 10), "quantize returns max at the upper bound");
  check(same(math::quantize(0.5, 0, 0, 1), 0), "quantize returns min when steps is 0");
  check(same(math::quantize(5, 4, 5, 5), 5), "quantize returns min when min equals max");
  check(close(math::quantize(-1, 4, 0, 1), 0, 10) && close(math::quantize(2, 4, 0, 1), 1, 10), "quantize clamps values outside the range");

  check(same(math::sign(5), 1) && same(math::sign(0.001), 1), "sign returns 1 for positive values");
  check(same(math::sign(-5), -1) && same(math::sign(-0.001), -1), "sign returns -1 for negative values");
  check(same(math::sign(0), 0), "sign returns 0 for zero");
}

void test_statistics() {
  check(same(math::mean(values({1, 2, 3, 4, 5})), 3), "mean returns the arithmetic mean");
  check(same(math::mean(values({42})), 42), "mean returns the single element for a one-element array");
  check(std::isnan(math::mean(values({}))), "mean returns NaN for an empty array");
  check(same(math::mean(values({-2, 0, 2})), 0), "mean handles negative values");
  check(same(math::mean(values({max_value, max_value})), max_value), "mean does not overflow when the finite mean is representable");
  check(same(math::mean(values({1, number_epsilon / 2, -1})), number_epsilon / 6), "mean retains a small term across cancellation");
  check(same(math::mean(values({0, 0})), 0) && same(math::mean(values({infinity, infinity})), infinity) &&
            std::isnan(math::mean(values({infinity, -infinity}))),
        "mean preserves zero and non-finite propagation");

  check(same(math::median(values({3, 1, 2})), 2), "median returns the middle value for an odd-length array");
  check(same(math::median(values({1, 2, 3, 4})), 2.5), "median returns the average of the two middle values for an even-length array");
  check(std::isnan(math::median(values({}))), "median returns NaN for an empty array");
  {
    // flight::Array is a reference: the callee receives the caller's array, as a JS function does, so
    // an in-place sort inside median would be visible here.
    const auto input = values({5, 3, 1, 4, 2});
    math::median(input);
    check(input.size() == 5 && input[0] == 5 && input[1] == 3 && input[2] == 1 && input[3] == 4 && input[4] == 2, "median does not mutate the input");
  }
  check(same(math::median(values({max_value, max_value})), max_value), "median does not overflow when averaging identical finite middle values");
  check(same(math::median(values({min_value * 2, min_value * 3})), min_value * 2), "median does not prematurely underflow when averaging subnormal middle values");
  check(same(math::median(values({-4, -2})), -3) && same(math::median(values({-max_value, max_value})), 0) &&
            same(math::median(values({infinity, infinity})), infinity),
        "median averages negative and non-finite middle values without changing their semantics");

  check(same(math::standard_deviation(values({5})), 0), "standardDeviation returns 0 for a single-element array");
  check(std::isnan(math::standard_deviation(values({}))), "standardDeviation returns NaN for an empty array");
  check(close(math::standard_deviation(values({2, 4, 4, 4, 5, 5, 7, 9})), 2, 5), "standardDeviation computes the population standard deviation");
  check(same(math::standard_deviation(values({max_value, max_value})), 0), "standardDeviation returns zero for identical maximum finite values");
  check(same(math::standard_deviation(values({0, 0})), 0) && std::isnan(math::standard_deviation(values({infinity}))),
        "standardDeviation handles zero and preserves non-finite propagation");

  check(same(math::variance(values({5})), 0), "variance returns 0 for a single-element array");
  check(std::isnan(math::variance(values({}))), "variance returns NaN for an empty array");
  check(close(math::variance(values({2, 4, 4, 4, 5, 5, 7, 9})), 4, 5), "variance computes the population variance");
  check(same(math::variance(values({max_value, max_value})), 0), "variance returns zero for identical maximum finite values");
  check(same(math::variance(values({-1, 1000000000000, 1000000000000})), 2.2222222222266665e23), "variance compensates the sum of squared deviations");
  check(same(math::variance(values({0, 0})), 0) && std::isnan(math::variance(values({infinity}))), "variance handles zero and preserves non-finite propagation");

  check(close(math::weighted_average(values({1, 2, 3}), values({1, 1, 1})), 2, 10) && close(math::weighted_average(values({0, 10}), values({1, 3})), 7.5, 10),
        "weightedAverage returns the weighted average");
  check(std::isnan(math::weighted_average(values({}), values({}))), "weightedAverage returns NaN for an empty array");
  check(std::isnan(math::weighted_average(values({1, 2, 3}), values({0, 0, 0}))), "weightedAverage returns NaN when all weights are 0");
  check(throws_range_error([] { math::weighted_average(values({1, 2}), values({1})); }), "weightedAverage throws when arrays have different lengths");
  check(same(math::weighted_average(values({max_value, max_value}), values({1, 1})), max_value),
        "weightedAverage does not overflow when the finite weighted average is representable");
  check(same(math::weighted_average(values({2, 10}), values({1, 3})), 8), "weightedAverage uses normalized weights in the weighted product");
  check(same(math::weighted_average(values({10000000000000000, -1000000000000000, -10000000000000000}), values({2, 1, 1})), 2250000000000000),
        "weightedAverage retains small weighted terms across cancellation");
  check(same(math::weighted_average(values({0, 0}), values({1, 1})), 0) && std::isnan(math::weighted_average(values({1, 2}), values({1, -1}))),
        "weightedAverage handles zero values and cancelling non-zero weights");
  check(same(math::weighted_average(values({infinity}), values({1})), infinity) && std::isnan(math::weighted_average(values({infinity, 1}), values({1, -1}))) &&
            std::isnan(math::weighted_average(values({1}), values({infinity}))),
        "weightedAverage preserves non-finite weighted-average propagation");
}

// Upstream's sequenceRandom: replays the given samples in order and throws once they run out.
flight::types::RandomSource sequence_random(std::vector<double> samples) {
  auto index = std::make_shared<std::size_t>(0);
  return [samples = std::move(samples), index] {
    if (*index >= samples.size()) throw std::runtime_error("sequenceRandom exhausted");
    return samples[(*index)++];
  };
}

flight::Ref<flight::types::Vector2> vec2() { return flight::make_ref<flight::types::Vector2>(flight::types::Vector2{.entity_runtime_key = std::nullopt, .x = 0, .y = 0}); }
flight::Ref<flight::types::Vector3> vec3() { return flight::make_ref<flight::types::Vector3>(flight::types::Vector3{.entity_runtime_key = std::nullopt, .x = 0, .y = 0, .z = 0}); }

flight::Array<flight::String> strings(std::initializer_list<const char*> items) {
  flight::Array<flight::String> out;
  for (const char* item : items) out.push(flight::String(item));
  return out;
}

bool equal_arrays(const flight::Array<double>& a, const flight::Array<double>& b) {
  if (a.size() != b.size()) return false;
  for (std::size_t i = 0; i < a.size(); ++i)
    if (!same(a[i], b[i])) return false;
  return true;
}

flight::Array<double> sorted(const flight::Array<double>& items) {
  return items.slice().sort([](double a, double b) { return a - b; });
}

void test_random_distributions() {
  {
    const auto items = values({1, 2, 3, 4, 5});
    auto random = seeded();
    bool ok = true;
    for (int i = 0; i < 50; ++i) {
      const auto picked = math::pick(random, items);
      ok = ok && picked.has_value() && items.includes(*picked);
    }
    check(ok, "pick returns an element from the array");
  }
  check(!math::pick(seeded(), values({})).has_value(), "pick returns undefined for an empty array");
  {
    auto random = seeded();
    bool ok = true;
    for (int i = 0; i < 20; ++i) ok = ok && math::pick(random, values({42})) == 42.0;
    check(ok, "pick always returns the only element in a single-item array");
  }
  check(math::pick(constant(0.75), strings({"a", "b", "c", "d"})) == flight::String("d"), "pick selects the bucket named by an exact deterministic sample");
  {
    const auto items = strings({"a", "b", "c", "d"});
    auto a = seeded();
    auto b = seeded();
    bool ok = true;
    for (int i = 0; i < 20; ++i) ok = ok && math::pick(a, items) == math::pick(b, items);
    check(ok, "pick is deterministic for the same seed");
  }

  {
    auto random = seeded();
    bool ok = true;
    for (int i = 0; i < 200; ++i) ok = ok && math::random_exponential(random) >= 0;
    check(ok, "randomExponential returns a non-negative value");
  }
  {
    auto random = seeded();
    double sum = 0;
    const int n = 5000;
    for (int i = 0; i < n; ++i) sum += math::random_exponential(random, 1);
    check(close(sum / n, 1, 0), "randomExponential mean approximates 1/rate for rate=1");
  }
  {
    auto random = seeded();
    double sum = 0;
    const int n = 5000;
    for (int i = 0; i < n; ++i) sum += math::random_exponential(random, 2);
    check(close(sum / n, 0.5, 0), "randomExponential mean approximates 1/rate for rate=2");
  }
  check(same(math::random_exponential(constant(0)), -std::log(number_epsilon)), "randomExponential substitutes epsilon when the random source returns zero");
  check(throws_range_error([] { math::random_exponential(seeded(), 0); }) && throws_range_error([] { math::random_exponential(seeded(), -1); }),
        "randomExponential throws for rate <= 0");
  check(throws_range_error([] { math::random_exponential(seeded(), nan); }) && throws_range_error([] { math::random_exponential(seeded(), infinity); }),
        "randomExponential rejects non-finite rates");
  {
    auto a = seeded();
    auto b = seeded();
    bool ok = true;
    for (int i = 0; i < 20; ++i) ok = ok && same(math::random_exponential(a), math::random_exponential(b));
    check(ok, "randomExponential is deterministic for the same seed");
  }

  {
    auto random = seeded();
    bool ok = true;
    for (int i = 0; i < 100; ++i) ok = ok && std::isfinite(math::random_gaussian(random));
    check(ok, "randomGaussian produces finite values");
  }
  {
    auto random = seeded();
    double sum = 0;
    const int n = 2000;
    for (int i = 0; i < n; ++i) sum += math::random_gaussian(random, 5, 1);
    check(close(sum / n, 5, 0), "randomGaussian centers around the mean");
  }
  check(close(math::random_gaussian(sequence_random({0, 0.25})), 0, 10), "randomGaussian substitutes epsilon when the first random sample is zero");
  {
    const double standard = std::sqrt(-2 * std::log(0.5));
    check(close(math::random_gaussian(sequence_random({0.5, 0}), 10, 2), 10 + standard * 2, 12), "randomGaussian matches a deterministic Box-Muller vector with mean and scale");
  }
  {
    auto a = seeded();
    auto b = seeded();
    bool ok = true;
    for (int i = 0; i < 20; ++i) ok = ok && same(math::random_gaussian(a), math::random_gaussian(b));
    check(ok, "randomGaussian is deterministic for the same seed");
  }

  {
    const auto pair = math::random_gaussian_pair(seeded());
    check(pair.size() == 2 && std::isfinite(pair[0]) && std::isfinite(pair[1]), "randomGaussianPair returns a tuple of two finite values");
  }
  {
    auto random = seeded();
    double sum = 0;
    const int n = 2000;
    for (int i = 0; i < n; ++i) {
      const auto pair = math::random_gaussian_pair(random, 10, 1);
      sum += pair[0] + pair[1];
    }
    check(close(sum / (n * 2), 10, 0), "randomGaussianPair produces values matching the requested mean");
  }
  {
    const auto pair = math::random_gaussian_pair(sequence_random({0, 0.25}));
    check(close(pair[0], 0, 10) && close(pair[1], std::sqrt(-2 * std::log(number_epsilon)), 10), "randomGaussianPair substitutes epsilon when the first random sample is zero");
  }
  {
    const double component = std::sqrt(-2 * std::log(0.5)) * M_SQRT1_2;
    check(equal_arrays(math::random_gaussian_pair(sequence_random({0.5, 0.125}), 3, 2), values({3 + component * 2, 3 + component * 2})),
          "randomGaussianPair matches both values of a deterministic Box-Muller pair");
  }

  {
    auto random = seeded();
    const auto out = vec2();
    bool ok = true;
    for (int i = 0; i < 200; ++i) {
      math::random_inside_unit_disc(random, out);
      ok = ok && out->x * out->x + out->y * out->y <= 1 + 1e-9;
    }
    check(ok, "randomInsideUnitDisc produces points inside the unit disc");
  }
  {
    const auto out = vec2();
    math::random_inside_unit_disc(seeded(), out);
    check(std::isfinite(out->x) && std::isfinite(out->y), "randomInsideUnitDisc is alias-safe when out is the same object as input");
  }
  {
    auto a = seeded();
    auto b = seeded();
    const auto out_a = vec2();
    const auto out_b = vec2();
    bool ok = true;
    for (int i = 0; i < 20; ++i) {
      math::random_inside_unit_disc(a, out_a);
      math::random_inside_unit_disc(b, out_b);
      ok = ok && same(out_a->x, out_b->x) && same(out_a->y, out_b->y);
    }
    check(ok, "randomInsideUnitDisc is deterministic for the same seed");
  }
  {
    const auto out = vec2();
    math::random_inside_unit_disc(sequence_random({1, 0.5, 0.5, 0.5}), out);
    check(out->x == 1 && out->y == 0, "randomInsideUnitDisc accepts and preserves a deterministic boundary point");
  }

  {
    auto random = seeded();
    const auto out = vec3();
    bool ok = true;
    for (int i = 0; i < 200; ++i) {
      math::random_inside_unit_sphere(random, out);
      ok = ok && out->x * out->x + out->y * out->y + out->z * out->z <= 1 + 1e-9;
    }
    check(ok, "randomInsideUnitSphere produces points with radius <= 1");
  }
  {
    const auto out = vec3();
    math::random_inside_unit_sphere(seeded(), out);
    check(std::isfinite(out->x) && std::isfinite(out->y) && std::isfinite(out->z), "randomInsideUnitSphere is alias-safe when out is the same object as input");
  }
  {
    auto a = seeded();
    auto b = seeded();
    const auto out_a = vec3();
    const auto out_b = vec3();
    bool ok = true;
    for (int i = 0; i < 20; ++i) {
      math::random_inside_unit_sphere(a, out_a);
      math::random_inside_unit_sphere(b, out_b);
      ok = ok && same(out_a->x, out_b->x) && same(out_a->y, out_b->y) && same(out_a->z, out_b->z);
    }
    check(ok, "randomInsideUnitSphere is deterministic for the same seed");
  }
  {
    const auto out = vec3();
    math::random_inside_unit_sphere(sequence_random({1, 0.5, 0.5, 0.5, 0.5, 0.5}), out);
    check(out->x == 1 && out->y == 0 && out->z == 0, "randomInsideUnitSphere accepts and preserves a deterministic boundary point");
  }

  {
    auto random = seeded();
    const auto out = vec2();
    bool ok = true;
    for (int i = 0; i < 100; ++i) {
      math::random_on_unit_circle(random, out);
      ok = ok && close(out->x * out->x + out->y * out->y, 1, 8);
    }
    check(ok, "randomOnUnitCircle produces points on the unit circle");
  }
  {
    const auto out = vec2();
    math::random_on_unit_circle(seeded(), out);
    check(std::isfinite(out->x) && std::isfinite(out->y), "randomOnUnitCircle is alias-safe when out is the same object as input");
  }
  {
    auto a = seeded();
    auto b = seeded();
    const auto out_a = vec2();
    const auto out_b = vec2();
    bool ok = true;
    for (int i = 0; i < 20; ++i) {
      math::random_on_unit_circle(a, out_a);
      math::random_on_unit_circle(b, out_b);
      ok = ok && same(out_a->x, out_b->x) && same(out_a->y, out_b->y);
    }
    check(ok, "randomOnUnitCircle is deterministic for the same seed");
  }
  {
    const auto out = vec2();
    math::random_on_unit_circle(constant(0.25), out);
    check(close(out->x, 0, 12) && close(out->y, 1, 12), "randomOnUnitCircle maps a quarter-turn sample to the top of the circle");
  }

  {
    auto random = seeded();
    const auto out = vec3();
    bool ok = true;
    for (int i = 0; i < 100; ++i) {
      math::random_on_unit_sphere(random, out);
      ok = ok && close(out->x * out->x + out->y * out->y + out->z * out->z, 1, 8);
    }
    check(ok, "randomOnUnitSphere produces points on the unit sphere");
  }
  {
    const auto out = vec3();
    math::random_on_unit_sphere(seeded(), out);
    check(std::isfinite(out->x) && std::isfinite(out->y) && std::isfinite(out->z), "randomOnUnitSphere is alias-safe when out is the same object as input");
  }
  {
    auto a = seeded();
    auto b = seeded();
    const auto out_a = vec3();
    const auto out_b = vec3();
    bool ok = true;
    for (int i = 0; i < 20; ++i) {
      math::random_on_unit_sphere(a, out_a);
      math::random_on_unit_sphere(b, out_b);
      ok = ok && same(out_a->x, out_b->x) && same(out_a->y, out_b->y) && same(out_a->z, out_b->z);
    }
    check(ok, "randomOnUnitSphere is deterministic for the same seed");
  }
  {
    const auto out = vec3();
    math::random_on_unit_sphere(sequence_random({0.75, 0.5}), out);
    check(close(out->x, std::sqrt(3.0) / 2, 12) && same(out->y, 0) && same(out->z, 0.5), "randomOnUnitSphere projects a deterministic disc sample onto the sphere");
  }
  {
    const auto out = vec3();
    math::random_on_unit_sphere(sequence_random({1, 0.5, 0.5, 0.5}), out);
    check(out->x == 0 && out->y == 0 && out->z == 1, "randomOnUnitSphere rejects a disc-boundary sample before projecting");
  }

  {
    auto random = seeded();
    bool ok = true;
    for (int i = 0; i < 200; ++i) {
      const double k = math::random_poisson(random, 3);
      ok = ok && is_integer(k) && k >= 0;
    }
    check(ok, "randomPoisson returns a non-negative integer");
  }
  {
    auto random = seeded();
    double sum = 0;
    const int n = 5000;
    for (int i = 0; i < n; ++i) sum += math::random_poisson(random, 1);
    check(close(sum / n, 1, 0), "randomPoisson mean approximates lambda=1");
  }
  {
    auto random = seeded();
    double sum = 0;
    const int n = 5000;
    for (int i = 0; i < n; ++i) sum += math::random_poisson(random, 5);
    check(close(sum / n, 5, 0), "randomPoisson mean approximates lambda=5");
  }
  check(throws_range_error([] { math::random_poisson(seeded(), 0); }) && throws_range_error([] { math::random_poisson(seeded(), -1); }), "randomPoisson throws for lambda <= 0");
  check(throws_range_error([] { math::random_poisson(seeded(), nan); }) && throws_range_error([] { math::random_poisson(seeded(), infinity); }),
        "randomPoisson rejects non-finite means");
  check(same(math::random_poisson(sequence_random({0.5, 0.5}), std::log(2.0)), 0), "randomPoisson stops when the product exactly reaches the limit");
  {
    auto a = seeded();
    auto b = seeded();
    bool ok = true;
    for (int i = 0; i < 20; ++i) ok = ok && same(math::random_poisson(a, 3), math::random_poisson(b, 3));
    check(ok, "randomPoisson is deterministic for the same seed");
  }

  {
    auto random = seeded();
    const auto weights = values({1, 2, 3});
    bool ok = true;
    for (int i = 0; i < 100; ++i) {
      const double index = math::random_weighted(random, weights);
      ok = ok && index >= 0 && index < static_cast<double>(weights.size());
    }
    check(ok, "randomWeighted returns indices within the weights array");
  }
  check(same(math::random_weighted(seeded(), values({})), -1), "randomWeighted returns -1 for an empty array");
  check(same(math::random_weighted(seeded(), values({0, 0, 0})), -1), "randomWeighted returns -1 for an all-zero weight array");
  {
    auto random = seeded();
    bool ok = true;
    for (int i = 0; i < 20; ++i) ok = ok && same(math::random_weighted(random, values({0, 10, 0})), 1);
    check(ok, "randomWeighted always returns the only non-zero index when all others are zero");
  }
  check(same(math::random_weighted(constant(1.1), values({1, 2, 7})), 2), "randomWeighted falls back to the final bucket for an out-of-range random source");
  check(same(math::random_weighted(constant(1.0 / 3.0), values({1, 2})), 0), "randomWeighted selects the earlier bucket when the sample lands exactly on its boundary");
  {
    auto a = seeded();
    auto b = seeded();
    const auto weights = values({1, 2, 3, 4});
    bool ok = true;
    for (int i = 0; i < 20; ++i) ok = ok && same(math::random_weighted(a, weights), math::random_weighted(b, weights));
    check(ok, "randomWeighted is deterministic for the same seed");
  }

  check(math::shuffle(seeded(), values({1, 2, 3, 4, 5})).size() == 5, "shuffle returns an array of the same length");
  {
    const auto items = values({1, 2, 3, 4, 5});
    check(equal_arrays(sorted(math::shuffle(seeded(), items)), sorted(items)), "shuffle contains all original elements");
  }
  {
    const auto items = values({1, 2, 3, 4, 5});
    math::shuffle(seeded(), items);
    check(equal_arrays(items, values({1, 2, 3, 4, 5})), "shuffle does not mutate the original array");
  }
  {
    const auto items = values({1, 2, 3, 4, 5, 6, 7, 8});
    check(equal_arrays(math::shuffle(seeded(), items), math::shuffle(seeded(), items)), "shuffle is deterministic for the same seed");
  }

  {
    const auto items = values({1, 2, 3, 4, 5});
    math::shuffle_in_place(seeded(), items);
    check(equal_arrays(sorted(items), values({1, 2, 3, 4, 5})), "shuffleInPlace retains all original elements");
  }
  {
    const auto items = values({1, 2, 3, 4, 5, 6, 7, 8});
    math::shuffle_in_place(seeded(), items);
    check(items.size() == 8, "shuffleInPlace mutates the original array");
  }
  {
    const auto a = values({1, 2, 3, 4, 5, 6, 7, 8});
    const auto b = values({1, 2, 3, 4, 5, 6, 7, 8});
    math::shuffle_in_place(seeded(), a);
    math::shuffle_in_place(seeded(), b);
    check(equal_arrays(a, b), "shuffleInPlace is deterministic for the same seed");
  }
  {
    const std::vector<double> samples = {0.75, 0.5, 0.25};
    auto calls = std::make_shared<int>(0);
    const auto items = strings({"a", "b", "c", "d"});
    math::shuffle_in_place<flight::String>(
        [samples, calls] {
          ++*calls;
          return samples[*calls - 1];
        },
        items);
    check(items.size() == 4 && items[0] == flight::String("c") && items[1] == flight::String("a") && items[2] == flight::String("b") &&
              items[3] == flight::String("d") && *calls == 3,
          "shuffleInPlace pins the swaps and consumes exactly one sample per Fisher-Yates step");
  }
}


}  // namespace

int main() {
  test_angle();
  test_clamp();
  test_comparison();
  test_constants();
  test_hash();
  test_interpolation();
  test_interpolation_advanced();
  test_next_power_of_two();
  test_number_theory();
  test_random();
  test_random_range();
  test_rounding();
  test_scalar();
  test_statistics();
  test_random_distributions();
  return failures == 0 ? 0 : 1;
}
