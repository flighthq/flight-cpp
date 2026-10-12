// The generated @flighthq/spring package at a627849, exercised natively. Expectations are ported from the
// package's own tests (packages/spring/src/*.test.ts): the analytic damped-harmonic step, settle
// detection, the angle seam, the vector springs matching the scalar solver per axis, and the config
// constructors.
#include <flight/spring/spring.hpp>
#include <flight/spring/spring2_d.hpp>
#include <flight/spring/spring3_d.hpp>
#include <flight/spring/spring_config.hpp>

#include <cmath>
#include <iostream>
#include <numbers>

namespace {

int failures = 0;

void check(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}

// Vitest's toBeCloseTo(expected, digits): |actual - expected| < 10^-digits / 2.
bool close(double actual, double expected, int digits) {
  return std::abs(actual - expected) < std::pow(10.0, -digits) / 2.0;
}

// The readonly row view the generated API takes for a `Readonly<T>` parameter.
template <typename Pointer>
auto view(const Pointer& value) {
  using Element = typename Pointer::element_type;
  return flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<Element>>>>(value);
}

namespace spring = flight::spring;
using flight::types::Spring;

void step(const flight::Ref<Spring>& s, double target, double frequency, double damping, double dt) {
  spring::update_spring(s, target, view(spring::create_spring_config(frequency, damping)), dt);
}

void scalar_state() {
  auto s = spring::create_spring(12.0, 3.0);
  spring::add_spring_impulse(s, 4.0);
  spring::add_spring_impulse(s, -1.5);
  check(s->value == 12.0 && s->velocity == 5.5, "impulses add velocity and compose, value unchanged");

  auto zero = spring::create_spring();
  check(zero->value == 0.0 && zero->velocity == 0.0, "create_spring defaults to 0, 0");

  auto reset = spring::create_spring(3.0, 9.0);
  spring::reset_spring(reset, 100.0);
  check(reset->value == 100.0 && reset->velocity == 0.0, "reset defaults velocity to 0");
  spring::reset_spring(reset, -5.0, 12.0);
  check(reset->value == -5.0 && reset->velocity == 12.0, "reset sets an explicit velocity");
}

void settling() {
  auto moving = spring::create_spring(0.0);
  step(moving, 10.0, 1.0, 1.0, 1.0 / 60.0);
  check(!spring::is_spring_settled(view(moving), 10.0), "not settled while moving");

  auto resting = spring::create_spring(0.0);
  for (int i = 0; i < 600; ++i) step(resting, 10.0, 4.0, 1.0, 1.0 / 60.0);
  check(spring::is_spring_settled(view(resting), 10.0), "settled once at rest at the target");

  check(!spring::is_spring_settled(view(spring::create_spring(10.0, 50.0)), 10.0), "fast at the target is not settled");
  check(!spring::is_spring_settled(view(spring::create_spring(3.0, 0.0)), 10.0), "at rest elsewhere is not settled");

  auto undamped = spring::create_spring(0.0);
  bool everSettled = false;
  for (int i = 0; i < 600; ++i) {
    step(undamped, 10.0, 1.0, 0.0, 1.0 / 60.0);
    everSettled = everSettled || spring::is_spring_settled(view(undamped), 10.0);
  }
  check(!everSettled, "an undamped oscillating spring never settles");

  auto loose = spring::create_spring(10.4, 0.4);
  check(!spring::is_spring_settled(view(loose), 10.0), "default epsilons reject 0.4 off");
  check(spring::is_spring_settled(view(loose), 10.0, 0.5, 0.5), "custom epsilons are honored");
}

void dynamics() {
  {
    const double frequency = 2.0, target = 3.0, omega = std::numbers::pi * 2.0 * frequency;
    auto s = spring::create_spring(7.0, -4.0);
    const auto energy = [&] { return s->velocity * s->velocity + omega * omega * std::pow(s->value - target, 2.0); };
    const double initial = energy();
    for (int i = 0; i < 600; ++i) step(s, target, frequency, 0.0, 1.0 / 60.0);
    check(close(energy(), initial, 8), "zero damping preserves oscillation energy");
  }
  {
    const double band = 1e-4;
    const auto at = [](double damping) {
      auto s = spring::create_spring(4.0, -3.0);
      step(s, -2.0, 3.0, damping, 0.2);
      return std::pair{s->value, s->velocity};
    };
    const auto critical = at(1.0);
    check(at(1.0 - band) == critical && at(1.0 - band * 0.5) == critical && at(1.0 + band * 0.5) == critical &&
              at(1.0 + band) == critical,
          "inside CRITICAL_BAND the critical solution is used exactly");
    const auto under = at(1.0 - band * 2.0), over = at(1.0 + band * 2.0);
    check(under != critical && over != critical, "outside the band the other solutions apply");
    check(close(under.first, critical.first, 3) && close(over.first, critical.first, 3), "no discontinuity at the band edge");
  }
  {
    auto s = spring::create_spring(0.0);
    double previous = s->value;
    bool monotone = true;
    for (int i = 0; i < 600; ++i) {
      step(s, 10.0, 1.0, 1.0, 1.0 / 60.0);
      monotone = monotone && s->value >= previous - 1e-9 && s->value <= 10.0 + 1e-9;
      previous = s->value;
    }
    check(monotone && close(s->value, 10.0, 3), "critical damping converges monotonically without overshoot");
  }
  {
    auto s = spring::create_spring(0.0);
    bool overshot = false;
    for (int i = 0; i < 2000; ++i) {
      step(s, 10.0, 1.0, 0.3, 1.0 / 60.0);
      overshot = overshot || s->value > 10.05;
    }
    check(overshot && spring::is_spring_settled(view(s), 10.0), "underdamped overshoots then settles");
  }
  {
    auto over = spring::create_spring(0.0), critical = spring::create_spring(0.0);
    for (int i = 0; i < 120; ++i) {
      step(over, 10.0, 1.0, 2.0, 1.0 / 60.0);
      step(critical, 10.0, 1.0, 1.0, 1.0 / 60.0);
    }
    check(over->value < critical->value, "overdamped lags behind critical over the same time");
  }
  {
    auto s = spring::create_spring(0.0);
    step(s, 10.0, 1000.0, 1.0, 1.0);
    check(std::isfinite(s->value) && std::isfinite(s->velocity) && close(s->value, 10.0, 6) && close(s->velocity, 0.0, 6),
          "a stiff spring at a huge deltaTime lands on the target");
  }
  {
    auto fine = spring::create_spring(0.0, 3.0), coarse = spring::create_spring(0.0, 3.0);
    for (int i = 0; i < 60; ++i) step(fine, 7.0, 2.0, 0.5, 1.0 / 60.0);
    step(coarse, 7.0, 2.0, 0.5, 1.0);
    check(close(fine->value, coarse->value, 6) && close(fine->velocity, coarse->velocity, 6), "frame-rate independent");
  }
  {
    auto s = spring::create_spring(4.0, 2.0);
    step(s, 100.0, 1.0, 1.0, 0.0);
    step(s, 100.0, 1.0, 1.0, -1.0);
    check(s->value == 4.0 && s->velocity == 2.0, "deltaTime <= 0 is a no-op");
    step(s, 100.0, 0.0, 1.0, 1.0 / 60.0);
    check(s->value == 4.0 && s->velocity == 2.0, "a non-positive frequency is inert");
  }
}

void angle() {
  const auto config = view(spring::create_spring_config(2.0, 1.0));
  auto forward = spring::create_spring(359.0), forwardReference = spring::create_spring(359.0);
  spring::update_spring_angle(forward, 1.0, 360.0, config, 1.0 / 60.0);
  spring::update_spring(forwardReference, 361.0, config, 1.0 / 60.0);
  check(close(forward->value, forwardReference->value, 12) && close(forward->velocity, forwardReference->velocity, 12),
        "angle takes the short path forward across the seam");
  auto reverse = spring::create_spring(1.0), reverseReference = spring::create_spring(1.0);
  spring::update_spring_angle(reverse, 359.0, 360.0, config, 1.0 / 60.0);
  spring::update_spring(reverseReference, -1.0, config, 1.0 / 60.0);
  check(close(reverse->value, reverseReference->value, 12) && close(reverse->velocity, reverseReference->velocity, 12),
        "angle takes the short path backward across the seam");
}

void vectors() {
  const auto config = view(spring::create_spring_config(3.0, 0.6));
  auto v2 = spring::create_spring2_d(1.0, -2.0, 0.5, 4.0);
  auto x = spring::create_spring(1.0, 0.5), y = spring::create_spring(-2.0, 4.0);
  spring::update_spring2_d(v2, 6.0, 9.0, config, 1.0 / 30.0);
  spring::update_spring(x, 6.0, config, 1.0 / 30.0);
  spring::update_spring(y, 9.0, config, 1.0 / 30.0);
  check(v2->x->value == x->value && v2->x->velocity == x->velocity && v2->y->value == y->value && v2->y->velocity == y->velocity,
        "Spring2D matches the scalar solver per axis");

  auto v3 = spring::create_spring3_d(1.0, 2.0, 3.0, 0.0, 0.0, 0.0);
  auto z = spring::create_spring(3.0, 0.0);
  spring::update_spring3_d(v3, 4.0, 5.0, -6.0, config, 1.0 / 30.0);
  spring::update_spring(z, -6.0, config, 1.0 / 30.0);
  check(v3->z->value == z->value && v3->z->velocity == z->velocity, "Spring3D matches the scalar solver per axis");
}

void configs() {
  auto config = spring::create_spring_config(2.5, 0.4);
  check(config->frequency == 2.5 && config->damping_ratio == 0.4, "create_spring_config stores verbatim");
  const double tau = std::numbers::pi * 2.0;
  auto critical = spring::create_spring_config_from_physical(100.0, 20.0, 1.0);
  check(close(critical->frequency, 10.0 / tau, 9) && close(critical->damping_ratio, 1.0, 12), "physical k=100 c=20 m=1");
  auto massive = spring::create_spring_config_from_physical(100.0, 20.0, 4.0);
  check(close(massive->frequency, 5.0 / tau, 9) && close(massive->damping_ratio, 0.5, 12), "mass enters both terms");

  const auto read = [](const auto& preset) {
    return std::pair{flight::row_get<flight::RowKey<"frequency">>(preset), flight::row_get<flight::RowKey<"dampingRatio">>(preset)};
  };
  const auto bouncy = read(spring::spring_preset_bouncy), gentle = read(spring::spring_preset_gentle),
             stiff = read(spring::spring_preset_stiff);
  check(bouncy.second < gentle.second && gentle.first < stiff.first && stiff.second >= 1.0,
        "presets are distinct bouncy, gentle and stiff profiles");
}

}  // namespace

int main() {
  scalar_state();
  settling();
  dynamics();
  angle();
  vectors();
  configs();
  return failures == 0 ? 0 : 1;
}
