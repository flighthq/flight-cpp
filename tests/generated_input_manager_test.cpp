// Native parity test for @flighthq/input's dead-zone contract -- complementary to the existing input test.
//
// tests/generated_input_test.cpp already covers the key-repeat timer, and it is untouched here. This file covers the
// other closed-form half of the module: the axis and radial stick dead zones, ported from inputManager.test.ts's own
// cases for them. They are pure, documented, and were otherwise uncovered.
//
//   * applyGamepadAxisDeadZone: a value inside [-deadZone, deadZone] maps to 0, INCLUDING exactly at the boundary; a
//     value outside is rescaled linearly onto [-1, 1] so the live range stays continuous -- full deflection is still
//     exactly 1, and a dead zone of 0 returns the raw value rather than clamping it.
//   * applyGamepadStickDeadZone: the RADIAL magnitude is what is compared, not either axis, so a diagonal input just
//     outside the zone is live while an axis-aligned one of the same magnitude is not; the direction is preserved and
//     the magnitude is rescaled onto [0, 1]. It is documented as alias-safe, which is asserted rather than assumed:
//     passing the output object's own fields as the inputs must give the answer the inputs imply.
//
// What is NOT covered here, and is not claimed: the manager's event/state transitions, pointer and touch ingress,
// focus and capture, multiple devices, and frame boundaries. The upstream suite has 63 cases for those; this unit takes
// the closed-form contract and says so, rather than implying the package is covered.
#include <flight/input/input_manager.hpp>

#include <cmath>
#include <iostream>

namespace {

namespace input = flight::input;
namespace types = flight::types;

int failures = 0;

void check(bool condition, const char* message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

bool close(double value, double expected, double tolerance = 1e-12) {
  return std::abs(value - expected) <= tolerance * std::max(1.0, std::abs(expected));
}

// The axis dead zone: zero inside, a continuous linear rescale outside.
void test_axis_dead_zone() {
  check(input::apply_gamepad_axis_dead_zone(0.0, 0.2) == 0.0, "an axis value inside the zone is zero");
  check(input::apply_gamepad_axis_dead_zone(0.2, 0.2) == 0.0, "exactly at the boundary is still inside it");
  check(input::apply_gamepad_axis_dead_zone(-0.2, 0.2) == 0.0, "and so is the negative boundary");
  check(input::apply_gamepad_axis_dead_zone(0.1, 0.2) == 0.0, "and anything between them");

  check(close(input::apply_gamepad_axis_dead_zone(1.0, 0.2), 1.0), "full deflection stays full deflection");
  check(close(input::apply_gamepad_axis_dead_zone(0.6, 0.2), 0.5),
        "and the live range is rescaled linearly between the zone and 1");
  check(close(input::apply_gamepad_axis_dead_zone(-0.6, 0.2), -0.5), "the rescale preserves the sign");
  check(input::apply_gamepad_axis_dead_zone(0.5, 0.2) > 0.0 && input::apply_gamepad_axis_dead_zone(0.5, 0.2) < 1.0,
        "a value just outside the zone lands strictly inside the live range");

  check(input::apply_gamepad_axis_dead_zone(0.7, 0.0) == 0.7, "a dead zone of zero returns the raw value");
  check(input::apply_gamepad_axis_dead_zone(-0.35, -1.0) == -0.35, "and so does a nonsensical negative zone");
}

// The stick dead zone is radial: the magnitude decides, not either axis.
void test_stick_dead_zone_is_radial() {
  const auto out = flight::make_ref<types::x_y_8365950bd60f783f>(types::x_y_8365950bd60f783f{.x = 0.0, .y = 0.0});
  const double zone = 0.2;

  input::apply_gamepad_stick_dead_zone(out, 0.1, 0.1, zone);
  check(out->x == 0.0 && out->y == 0.0, "a stick inside the radial zone reads as centred");

  // Magnitude 0.3, axis-aligned: live, and rescaled to (0, 1).
  input::apply_gamepad_stick_dead_zone(out, 0.3, 0.0, zone);
  check(out->x > 0.0 && out->y == 0.0, "an axis-aligned push outside the zone is live on that axis only");
  check(close(out->x, (0.3 - zone) / (1.0 - zone)), "and its magnitude is the linear rescale");

  // The same magnitude on the diagonal is live too -- radial, not per-axis.
  const double diagonal = 0.3 / std::sqrt(2.0);
  input::apply_gamepad_stick_dead_zone(out, diagonal, diagonal, zone);
  check(out->x > 0.0 && out->y > 0.0, "a diagonal push of the same magnitude is live as well");
  check(close(std::sqrt(out->x * out->x + out->y * out->y), (0.3 - zone) / (1.0 - zone)),
        "and its rescaled magnitude matches the axis-aligned one, which per-axis zones would not");

  input::apply_gamepad_stick_dead_zone(out, 1.0, 0.0, zone);
  check(close(out->x, 1.0) && out->y == 0.0, "full deflection reaches exactly 1");
  input::apply_gamepad_stick_dead_zone(out, 0.4, 0.3, 0.0);
  check(close(out->x, 0.4) && close(out->y, 0.3), "a dead zone of zero passes the input through");
}

// The documented alias-safety: the result must come from the inputs, not from the object being written.
void test_stick_dead_zone_is_alias_safe() {
  const auto value = flight::make_ref<types::x_y_8365950bd60f783f>(types::x_y_8365950bd60f783f{.x = 0.6, .y = 0.0});
  const double zone = 0.2;
  const double expected = (0.6 - zone) / (1.0 - zone);

  // The output object IS the input: its own fields are read for x and y before anything is written.
  input::apply_gamepad_stick_dead_zone(value, value->x, value->y, zone);
  check(close(value->x, expected) && value->y == 0.0,
        "passing the output's own fields as the inputs still answers from those inputs");
}

}  // namespace

int main() {
  test_axis_dead_zone();
  test_stick_dead_zone_is_radial();
  test_stick_dead_zone_is_alias_safe();
  if (failures == 0) std::cout << "generated input manager: all checks passed\n";
  return failures == 0 ? 0 : 1;
}
