// A consumer of the installed @flighthq/skeleton2d-formats Spine parse helpers: it includes the
// generated public header and calls the exported clampUnit. Ported from
// packages/skeleton2d-formats/src/spineParseHelpers.ts:124-126, `value < 0 ? 0 : value > 1 ? 1 : value`.
// The comparisons are strict, so exactly 0 and 1 are identity (including the sign of -0), the doubles
// immediately on either side stay unchanged, infinities go to the matching bound, and NaN fails both
// comparisons and passes through as NaN rather than being coerced to a bound. The Spine importer
// depends on this when it clamps only the x components of an ease cubic bezier before inverting them.
#include <flight/skeleton2d_formats/spine_parse_helpers.hpp>

#include <cmath>
#include <iostream>
#include <limits>

namespace {

int failures = 0;

void check(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}

void clamp_unit_bounds() {
  namespace formats = flight::skeleton2d_formats;
  const double infinity = std::numeric_limits<double>::infinity();
  check(formats::clamp_unit(-1.0) == 0.0, "a negative value clamps to 0");
  check(formats::clamp_unit(-infinity) == 0.0, "negative infinity clamps to 0");
  check(formats::clamp_unit(1.0001) == 1.0, "a value just above 1 clamps to 1");
  check(formats::clamp_unit(2.0) == 1.0, "a value above 1 clamps to 1");
  check(formats::clamp_unit(infinity) == 1.0, "positive infinity clamps to 1");
}

void clamp_unit_identity() {
  namespace formats = flight::skeleton2d_formats;
  check(formats::clamp_unit(0.0) == 0.0, "+0 is returned unchanged");
  const double negative_zero = formats::clamp_unit(-0.0);
  check(negative_zero == 0.0, "-0 stays inside the unit interval");
  check(std::signbit(negative_zero), "-0 keeps its sign because both comparisons are strict");
  check(formats::clamp_unit(0.25) == 0.25, "an interior value is unchanged");
  check(formats::clamp_unit(1.0) == 1.0, "exactly 1 is unchanged");
  const double above_zero = std::nextafter(0.0, 1.0);
  const double below_one = std::nextafter(1.0, 0.0);
  check(formats::clamp_unit(above_zero) == above_zero, "the smallest value above 0 is unchanged");
  check(formats::clamp_unit(below_one) == below_one, "the largest value below 1 is unchanged");
}

void clamp_unit_nan() {
  namespace formats = flight::skeleton2d_formats;
  const double nan = std::numeric_limits<double>::quiet_NaN();
  const double result = formats::clamp_unit(nan);
  check(std::isnan(result), "NaN fails both strict comparisons and passes through as NaN");
  check(!(result == 0.0) && !(result == 1.0), "NaN is not coerced to either bound");
}

}  // namespace

int main() {
  clamp_unit_bounds();
  clamp_unit_identity();
  clamp_unit_nan();
  if (failures == 0) std::cout << "clampUnit matches the pinned source at its boundaries\n";
  return failures == 0 ? 0 : 1;
}
