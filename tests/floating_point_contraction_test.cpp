#include <flight/contract.hpp>

#include <cmath>
#include <iostream>
#include <string>

// Guards the floating-point profile every build of the runtime applies (the Flight::Cpp interface, each
// Bazel platform, the header gate): no contraction. JavaScript rounds every multiply and every add
// separately, while a fused multiply-add rounds once, so an expression TypeScript evaluates to exactly 0
// can leave a residue natively. See docs/floating-point-contraction.md.
//
// The inputs are volatile so the compiler cannot fold the expressions at compile time; they are evaluated
// by the same arithmetic emitted code uses. On a target without FMA these pass regardless, and on an FMA
// target they fail unless contraction is off.

namespace {

int failures = 0;

void check(bool condition, const std::string& message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

bool is_positive_zero(double value) { return value == 0.0 && !std::signbit(value); }

volatile double forward[3] = {2.0 / 7.0, 3.0 / 7.0, 6.0 / 7.0};
volatile double tenth = 0.1;
volatile double ten = 10.0;
volatile double one = 1.0;

}  // namespace

int main() {
  {
    // The cross-product component setQuaternionLookRotation tests against exactly 0 when up is parallel
    // to forward. JavaScript: (3/7)*(6/7) - (6/7)*(3/7) === 0.
    const double fy = forward[1];
    const double fz = forward[2];
    const double uy = forward[1];
    const double uz = forward[2];
    const double residue = uy * fz - uz * fy;
    check(is_positive_zero(residue), "a cross-product component of parallel vectors is exactly 0, as in JavaScript");
  }
  {
    // JavaScript: 0.1 * 10 - 1 === 0, because 0.1 * 10 rounds to exactly 1 before the subtraction. Fused,
    // the unrounded product leaves 2^-54 * 10 behind.
    const double result = tenth * ten - one;
    check(is_positive_zero(result), "0.1 * 10 - 1 is exactly 0, as in JavaScript");
  }
  {
    // The same expression split across statements: GCC's GNU dialects contract across them.
    const double product = tenth * ten;
    const double result = product - one;
    check(is_positive_zero(result), "a product rounded into a local before the subtraction is exactly 0");
  }
  return failures == 0 ? 0 : 1;
}
