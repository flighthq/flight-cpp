// The generated @flighthq/spring package at a627849, exercised natively. Expectations are ported from the
// package's own tests (packages/spring/src/*.test.ts): the analytic damped-harmonic step, settle
// detection, the angle seam, the vector springs matching the scalar solver per axis, and the config
// constructors.
//
// Tolerances are upstream's. `close(actual, expected, digits)` is Vitest's toBeCloseTo, and every call
// uses the digit count the upstream assertion uses; the one-sided bounds (1e-9 monotonicity slack, the
// 0.05 overshoot margin, |value| < 100) are upstream's literals. The closed-form references below are this
// port's own addition: they solve the same differential equation from its displacement form rather than
// the solver's coefficient matrix, to 12 digits -- values here are O(10), so that leaves about three
// orders of magnitude above double-precision rounding for the dozen operations either side performs.
#include <flight/spring/spring.hpp>
#include <flight/spring/spring_2d.hpp>
#include <flight/spring/spring_3d.hpp>
#include <flight/spring/spring_config.hpp>

#include <cmath>
#include <limits>
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

template <typename Pointer>
constexpr bool declares_runtime(const Pointer&) {
  return std::is_same_v<decltype(std::declval<typename Pointer::element_type&>().entity_runtime_key),
                        std::optional<flight::Ref<flight::types::EntityRuntime>>>;
}

template <typename Schema>
constexpr bool preset_is_readonly(const flight::StructuralRef<Schema>&) {
  return flight::detail::schema_readonly<Schema>;
}
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
  // SPRING_SETTLE_EPSILON is upstream's 1e-3, applied inclusively (approxEqual is |a - b| <= epsilon).
  check(!spring::is_spring_settled(view(spring::create_spring(10.002, 0.0)), 10.0) &&
            !spring::is_spring_settled(view(spring::create_spring(10.0, 0.002)), 10.0) &&
            spring::is_spring_settled(view(spring::create_spring(10.0009, 0.0009)), 10.0),
        "the default epsilon is 1e-3 for both position and velocity");
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
    check(close(under.first, critical.first, 3) && close(over.first, critical.first, 3) && close(under.second, critical.second, 2) &&
              close(over.second, critical.second, 2),
          "no discontinuity at the band edge, in value (3 digits) or velocity (2 digits)");
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
    check(monotone && close(s->value, 10.0, 3) && spring::is_spring_settled(view(s), 10.0),
          "critical damping converges monotonically without overshoot and settles");
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
    double previous = over->value;
    bool monotone = true;
    for (int i = 0; i < 120; ++i) {
      step(over, 10.0, 1.0, 2.0, 1.0 / 60.0);
      step(critical, 10.0, 1.0, 1.0, 1.0 / 60.0);
      monotone = monotone && over->value >= previous - 1e-9 && over->value <= 10.0 + 1e-9;
      previous = over->value;
    }
    check(monotone, "overdamped converges without overshoot");
    check(over->value < critical->value, "overdamped lags behind critical over the same time");
  }
  {
    auto s = spring::create_spring(0.0);
    bool bounded = true;
    for (int i = 0; i < 50; ++i) {
      step(s, 10.0, 1000.0, 0.5, 1.0);
      bounded = bounded && std::isfinite(s->value) && std::isfinite(s->velocity) && std::abs(s->value) < 100.0;
    }
    check(bounded && spring::is_spring_settled(view(s), 10.0), "a stiff underdamped spring stepped repeatedly at a huge deltaTime stays bounded and settles");
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
    // 0.7 + (0.1 - 0.7) is not 0.1 in binary floating point, so a zero step that ran the solver would show.
    auto inexact = spring::create_spring(0.1, 0.3);
    step(inexact, 0.7, 2.0, 1.0, 0.0);
    check(inexact->value == 0.1 && inexact->velocity == 0.3, "deltaTime == 0 returns before any arithmetic");
    step(s, 100.0, 0.0, 1.0, 1.0 / 60.0);
    step(s, 100.0, -3.0, 1.0, 1.0 / 60.0);
    check(s->value == 4.0 && s->velocity == 2.0, "a non-positive frequency is inert");
  }
  {
    auto clamped = spring::create_spring(5.0, 2.0), undamped = spring::create_spring(5.0, 2.0);
    step(clamped, 0.0, 3.0, -0.5, 1.0 / 60.0);
    step(undamped, 0.0, 3.0, 0.0, 1.0 / 60.0);
    check(clamped->value == undamped->value && clamped->velocity == undamped->velocity, "a negative dampingRatio is clamped to 0");
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

  auto tie = spring::create_spring(0.0), tieReference = spring::create_spring(0.0);
  spring::update_spring_angle(tie, -0.5, 1.0, config, 1.0 / 60.0);
  spring::update_spring(tieReference, 0.5, config, 1.0 / 60.0);
  check(close(tie->value, tieReference->value, 12) && close(tie->velocity, tieReference->velocity, 12) && tie->value > 0.0,
        "caller-selected units (turns), and an exact half-turn tie resolves positively");

  auto far = spring::create_spring(725.0), farReference = spring::create_spring(725.0);
  spring::update_spring_angle(far, 10.0, 360.0, config, 1.0 / 60.0);
  spring::update_spring(farReference, 730.0, config, 1.0 / 60.0);
  check(close(far->value, farReference->value, 12), "the spring stays on its unwrapped branch, several turns out");

  bool inert = true;
  for (const double fullTurn : {0.0, -360.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
    auto s = spring::create_spring(10.0, 1.0);
    spring::update_spring_angle(s, 20.0, fullTurn, config, 1.0 / 60.0);
    inert = inert && s->value == 10.0 && s->velocity == 1.0;
  }
  check(inert, "a non-positive or non-finite fullTurn is an inert sentinel");
}

// The damped-harmonic equation solved from its displacement form, c(t) = value - target, independently of
// the solver's coefficient matrix: one step from (c0, v0) over dt.
std::pair<double, double> closed_form(double c0, double v0, double frequency, double zeta, double dt) {
  const double omega = 2.0 * std::numbers::pi * frequency;
  if (zeta < 1.0) {
    const double alpha = zeta * omega, beta = omega * std::sqrt(1.0 - zeta * zeta);
    const double a = c0, b = (v0 + alpha * c0) / beta, decay = std::exp(-alpha * dt);
    const double c = decay * (a * std::cos(beta * dt) + b * std::sin(beta * dt));
    const double v = decay * ((b * beta - alpha * a) * std::cos(beta * dt) - (a * beta + alpha * b) * std::sin(beta * dt));
    return {c, v};
  }
  if (zeta > 1.0) {
    const double root = omega * std::sqrt(zeta * zeta - 1.0);
    const double r1 = -zeta * omega - root, r2 = -zeta * omega + root;
    const double b = (v0 - r1 * c0) / (r2 - r1), a = c0 - b;
    return {a * std::exp(r1 * dt) + b * std::exp(r2 * dt), a * r1 * std::exp(r1 * dt) + b * r2 * std::exp(r2 * dt)};
  }
  const double decay = std::exp(-omega * dt), b = v0 + omega * c0;
  return {(c0 + b * dt) * decay, (b - omega * (c0 + b * dt)) * decay};
}

void closed_form_reference() {
  struct Case {
    double zeta;
    const char* message;
  };
  for (const Case c : {Case{0.0, "undamped matches the closed form"}, Case{0.7, "underdamped matches the closed form"},
                       Case{1.0, "critical matches the closed form"}, Case{2.5, "overdamped matches the closed form"}}) {
    auto s = spring::create_spring(5.0, 2.0);
    step(s, 1.5, 3.0, c.zeta, 1.0 / 60.0);
    const auto [displacement, velocity] = closed_form(5.0 - 1.5, 2.0, 3.0, c.zeta, 1.0 / 60.0);
    check(close(s->value, 1.5 + displacement, 12) && close(s->velocity, velocity, 12), c.message);
  }
  auto s = spring::create_spring(5.0, 2.0);
  step(s, 0.0, 3.0, 0.7, 1.0 / 60.0);
  check(s->value != 5.0 && s->velocity != 2.0, "a meaningful step moves the value and changes the velocity");
}

void vector_lifecycle() {
  {
    auto s = spring::create_spring_2d();
    // Upstream asserts Object.hasOwn(spring, EntityRuntimeKey): allocateEntity writes the key with the value
    // undefined. An optional member cannot tell "own key holding undefined" from "absent" -- the runtime
    // answers presence of a computed cell by its engagement -- so the port asserts what is observable: every
    // spring declares the runtime cell, and it holds undefined until a runtime is attached.
    check(declares_runtime(s) && declares_runtime(s->x) && declares_runtime(s->y) && !s->entity_runtime_key.has_value() &&
              !s->x->entity_runtime_key.has_value() && !s->y->entity_runtime_key.has_value(),
          "Spring2D and each axis are entities whose runtime cell is still undefined");
    check(s->x->value == 0 && s->x->velocity == 0 && s->y->value == 0 && s->y->velocity == 0, "Spring2D defaults to rest at 0");
    auto set = spring::create_spring_2d(1.0, 2.0, 3.0, 4.0);
    check(set->x->value == 1 && set->y->value == 2 && set->x->velocity == 3 && set->y->velocity == 4, "Spring2D takes per-axis value and velocity");
    auto impulse = spring::create_spring_2d(10.0, 20.0, 1.0, 2.0);
    spring::add_spring_impulse_2d(impulse, 3.0, -5.0);
    check(impulse->x->value == 10 && impulse->x->velocity == 4 && impulse->y->value == 20 && impulse->y->velocity == -3,
          "addSpringImpulse2D adds each velocity component, values unchanged");
    auto reset = spring::create_spring_2d(1.0, 2.0, 3.0, 4.0);
    const auto x = reset->x, y = reset->y;
    spring::reset_spring_2d(reset, 10.0, 20.0, 30.0, 40.0);
    check(reset->x == x && reset->y == y && x->value == 10 && x->velocity == 30 && y->value == 20 && y->velocity == 40,
          "resetSpring2D writes into the same axis springs");
    spring::reset_spring_2d(reset, -10.0, -20.0);
    check(x->value == -10 && x->velocity == 0 && y->value == -20 && y->velocity == 0, "and defaults the velocities to 0");
  }
  {
    auto s = spring::create_spring_2d(0.0, 0.0);
    const auto config = view(spring::create_spring_config(4.0, 1.0));
    check(!spring::is_spring_2d_settled(view(s), 10.0, -6.0), "Spring2D is not settled before moving");
    for (int i = 0; i < 600; ++i) spring::update_spring_2d(s, 10.0, -6.0, config, 1.0 / 60.0);
    check(spring::is_spring_2d_settled(view(s), 10.0, -6.0), "and settles once both axes rest at their targets");
    s->y->velocity = 0.4;
    check(!spring::is_spring_2d_settled(view(s), 10.0, -6.0) && spring::is_spring_2d_settled(view(s), 10.0, -6.0, 0.5, 0.5),
          "one moving axis keeps it unsettled, and the epsilons reach both axes");
  }
  {
    auto s = spring::create_spring_2d(0.0, 0.0);
    spring::update_spring_2d(s, 10.0, -20.0, view(spring::create_spring_config(2.0, 1.0)), 1.0 / 60.0);
    check(s->x->value > 0 && s->y->value < 0, "updateSpring2D moves both components toward their targets");
    auto independent = spring::create_spring_2d(0.0, 0.0);
    for (int i = 0; i < 30; ++i) spring::update_spring_2d(independent, 10.0, 5.0, view(spring::create_spring_config(2.0, 0.8)), 1.0 / 60.0);
    check(!close(independent->x->value, independent->y->value, 3), "the axes are driven independently");
  }
  {
    auto s = spring::create_spring_3d();
    check(declares_runtime(s) && declares_runtime(s->z) && !s->entity_runtime_key.has_value() && !s->x->entity_runtime_key.has_value() &&
              !s->y->entity_runtime_key.has_value() && !s->z->entity_runtime_key.has_value(),
          "Spring3D and each axis are entities whose runtime cell is still undefined");
    check(s->x->value == 0 && s->y->value == 0 && s->z->value == 0 && s->x->velocity == 0 && s->y->velocity == 0 && s->z->velocity == 0,
          "Spring3D defaults to rest at 0");
    auto set = spring::create_spring_3d(1.0, 2.0, 3.0, 4.0, 5.0, 6.0);
    check(set->x->value == 1 && set->y->value == 2 && set->z->value == 3 && set->x->velocity == 4 && set->y->velocity == 5 &&
              set->z->velocity == 6,
          "Spring3D takes per-axis value and velocity");
    auto impulse = spring::create_spring_3d(10.0, 20.0, 30.0, 1.0, 2.0, 3.0);
    spring::add_spring_impulse_3d(impulse, 3.0, -5.0, 7.0);
    check(impulse->x->velocity == 4 && impulse->y->velocity == -3 && impulse->z->velocity == 10 && impulse->x->value == 10 &&
              impulse->y->value == 20 && impulse->z->value == 30,
          "addSpringImpulse3D adds each velocity component, values unchanged");
    auto reset = spring::create_spring_3d(1.0, 2.0, 3.0, 4.0, 5.0, 6.0);
    const auto x = reset->x, y = reset->y, z = reset->z;
    spring::reset_spring_3d(reset, 10.0, 20.0, 30.0, 40.0, 50.0, 60.0);
    check(reset->x == x && reset->y == y && reset->z == z && z->value == 30 && z->velocity == 60 && x->velocity == 40,
          "resetSpring3D writes into the same axis springs");
    spring::reset_spring_3d(reset, -10.0, -20.0, -30.0);
    check(x->velocity == 0 && y->velocity == 0 && z->velocity == 0 && z->value == -30, "and defaults the velocities to 0");
  }
  {
    auto s = spring::create_spring_3d(0.0, 0.0, 0.0);
    const auto config = view(spring::create_spring_config(4.0, 1.0));
    check(!spring::is_spring_3d_settled(view(s), 10.0, -6.0, 3.0), "Spring3D is not settled before moving");
    spring::update_spring_3d(s, 10.0, -20.0, 30.0, view(spring::create_spring_config(2.0, 1.0)), 1.0 / 60.0);
    check(s->x->value > 0 && s->y->value < 0 && s->z->value > 0, "updateSpring3D moves all three components toward their targets");
    spring::reset_spring_3d(s, 0.0, 0.0, 0.0);
    for (int i = 0; i < 600; ++i) spring::update_spring_3d(s, 10.0, -6.0, 3.0, config, 1.0 / 60.0);
    check(spring::is_spring_3d_settled(view(s), 10.0, -6.0, 3.0), "and settles once all three axes rest at their targets");
    s->z->value = 3.4;
    check(!spring::is_spring_3d_settled(view(s), 10.0, -6.0, 3.0), "one displaced axis keeps it unsettled");
  }
}

void vectors() {
  const auto config = view(spring::create_spring_config(3.0, 0.6));
  auto v2 = spring::create_spring_2d(1.0, -2.0, 0.5, 4.0);
  auto x = spring::create_spring(1.0, 0.5), y = spring::create_spring(-2.0, 4.0);
  spring::update_spring_2d(v2, 6.0, 9.0, config, 1.0 / 30.0);
  spring::update_spring(x, 6.0, config, 1.0 / 30.0);
  spring::update_spring(y, 9.0, config, 1.0 / 30.0);
  check(v2->x->value == x->value && v2->x->velocity == x->velocity && v2->y->value == y->value && v2->y->velocity == y->velocity,
        "Spring2D matches the scalar solver per axis");

  auto v3 = spring::create_spring_3d(1.0, 2.0, 3.0, 0.0, 0.0, 0.0);
  auto z = spring::create_spring(3.0, 0.0);
  spring::update_spring_3d(v3, 4.0, 5.0, -6.0, config, 1.0 / 30.0);
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
  check(bouncy == std::pair{2.0, 0.35} && gentle == std::pair{1.5, 0.8} && stiff == std::pair{4.0, 1.0},
        "presets carry their declared frequency and dampingRatio");

  // Object.freeze: the presets are read-only row views, and `row_set` refuses a read-only schema with a
  // static_assert in its body -- a hard error rather than a substitution failure, so the property is checked
  // through the same trait that guard reads. A customized copy leaves the shared profile untouched.
  static_assert(preset_is_readonly(spring::spring_preset_bouncy) && preset_is_readonly(spring::spring_preset_gentle) &&
                    preset_is_readonly(spring::spring_preset_stiff),
                "a frozen preset rejects writes");
  auto customized = spring::create_spring_config(3.0, gentle.second);
  check(customized->damping_ratio == 0.8 && customized->frequency == 3.0 && read(spring::spring_preset_gentle).first == 1.5,
        "customizing a copy keeps the preset's other fields and leaves the preset as it was");
  check(spring::create_spring(5.0, -2.0)->value == 5.0 && spring::create_spring(5.0, -2.0)->velocity == -2.0,
        "create_spring stores the provided value and velocity");
}

}  // namespace

int main() {
  scalar_state();
  settling();
  dynamics();
  angle();
  closed_form_reference();
  vector_lifecycle();
  vectors();
  configs();
  return failures == 0 ? 0 : 1;
}
