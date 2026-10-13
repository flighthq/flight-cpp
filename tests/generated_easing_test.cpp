// The @flighthq/easing package at a627849 -- its 22 emitted modules and the createEasingSamples override --
// exercised natively. Ported from packages/easing/src/*.test.ts:
// - cross-family invariants (easingFamilies.test.ts): pairwise distinctness, exact endpoints with the two
//   named one-ulp exceptions, easeInOut as scaled halves (and the two deliberate exceptions), midpoint
//   continuity, easeOut as the 180-degree rotation of easeIn, monotonicity where promised, overshoot where it
//   is the family's point, and Bounce staying inside the unit interval;
// - per-curve values, the parameterized families (power, damped sine, cubic Bezier, smoothstep range, steps,
//   piecewise) against the fixed curves they generalize, the combinators, sampling, the numeric derivative,
//   and the degenerate-steps guard with its log warning.
//
// Tolerances are upstream's. `close(actual, expected, digits)` is Vitest's toBeCloseTo, whose default is 2
// digits, and each call uses the digit count of the upstream assertion it ports. The family bounds -- 1e-12
// for identities, 0.005 for distinctness, 0.01 for the deliberate InOut differences, 0.05 for overshoot --
// are upstream's constants, each justified there against the measured worst case.
#include <flight/easing/create_easing_samples.hpp>
#include <flight/easing/ease_back.hpp>
#include <flight/easing/ease_bounce.hpp>
#include <flight/easing/ease_circular.hpp>
#include <flight/easing/ease_combinators.hpp>
#include <flight/easing/ease_cubic.hpp>
#include <flight/easing/ease_cubic_bezier.hpp>
#include <flight/easing/ease_damped_sine.hpp>
#include <flight/easing/ease_elastic.hpp>
#include <flight/easing/ease_exponential.hpp>
#include <flight/easing/ease_linear.hpp>
#include <flight/easing/ease_piecewise.hpp>
#include <flight/easing/ease_power.hpp>
#include <flight/easing/ease_quadratic.hpp>
#include <flight/easing/ease_quartic.hpp>
#include <flight/easing/ease_quintic.hpp>
#include <flight/easing/ease_sine.hpp>
#include <flight/easing/ease_smoothstep.hpp>
#include <flight/easing/ease_steps.hpp>
#include <flight/easing/enable_easing_guards.hpp>
#include <flight/easing/get_easing_derivative.hpp>
#include <flight/log/log.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}

// Vitest's toBeCloseTo(expected, digits): |actual - expected| < 10^-digits / 2.
bool close(double actual, double expected, int digits = 2) {
  return std::abs(actual - expected) < std::pow(10.0, -digits) / 2.0;
}

namespace ez = flight::easing;
using Ease = flight::types::EasingFunction;

struct Family {
  Ease easeIn;
  Ease easeInOut;
  Ease easeOut;
  bool halvesMatch;
  bool monotonic;
  const char* name;
};

const std::vector<Family>& families() {
  static const std::vector<Family> list{
      {ez::ease_in_back, ez::ease_in_out_back, ez::ease_out_back, false, false, "Back"},
      {ez::ease_in_bounce, ez::ease_in_out_bounce, ez::ease_out_bounce, true, false, "Bounce"},
      {ez::ease_in_circular, ez::ease_in_out_circular, ez::ease_out_circular, true, true, "Circular"},
      {ez::ease_in_cubic, ez::ease_in_out_cubic, ez::ease_out_cubic, true, true, "Cubic"},
      {ez::ease_in_elastic, ez::ease_in_out_elastic, ez::ease_out_elastic, false, false, "Elastic"},
      {ez::ease_in_exponential, ez::ease_in_out_exponential, ez::ease_out_exponential, true, true, "Exponential"},
      {ez::ease_in_quadratic, ez::ease_in_out_quadratic, ez::ease_out_quadratic, true, true, "Quadratic"},
      {ez::ease_in_quartic, ez::ease_in_out_quartic, ez::ease_out_quartic, true, true, "Quartic"},
      {ez::ease_in_quintic, ez::ease_in_out_quintic, ez::ease_out_quintic, true, true, "Quintic"},
      {ez::ease_in_sine, ez::ease_in_out_sine, ez::ease_out_sine, true, true, "Sine"},
  };
  return list;
}

// 201 points across [0, 1], as upstream samples.
std::vector<double> samples() {
  std::vector<double> out;
  for (int i = 0; i <= 200; ++i) out.push_back(i / 200.0);
  return out;
}

constexpr double kEpsilon = 1e-12;

void family_distinctness() {
  const auto ts = samples();
  const auto pick = [](const Family& f, int direction) -> const Ease& {
    return direction == 0 ? f.easeIn : (direction == 1 ? f.easeInOut : f.easeOut);
  };
  const char* directions[] = {"easeIn", "easeInOut", "easeOut"};
  for (int d = 0; d < 3; ++d) {
    const auto& list = families();
    for (std::size_t a = 0; a < list.size(); ++a) {
      for (std::size_t b = a + 1; b < list.size(); ++b) {
        double worst = 0;
        for (double t : ts) worst = std::max(worst, std::abs(pick(list[a], d)(t) - pick(list[b], d)(t)));
        check(worst > 0.005, std::string(directions[d]) + " " + list[a].name + "/" + list[b].name + " are distinct");
      }
    }
  }
}

void family_endpoints() {
  for (const auto& f : families()) {
    const std::string name = f.name;
    // -0 == 0 in C++ as with JS ===; upstream normalizes negative zero before its Object.is comparison.
    check(f.easeIn(0) == 0 && f.easeInOut(0) == 0 && std::abs(f.easeOut(0)) < kEpsilon, name + ": starts at exactly 0");
    const bool inexactAtOne = name == "Back" || name == "Sine";
    check(f.easeOut(1) == 1 && f.easeInOut(1) == 1 && (inexactAtOne ? close(f.easeIn(1), 1, 15) : f.easeIn(1) == 1),
          name + ": ends at 1, exactly where the arithmetic allows");
  }
  check(ez::ease_in_back(1) != 1 && ez::ease_in_sine(1) != 1, "the two named one-ulp exceptions really are inexact at 1");
}

void family_halves() {
  const auto ts = samples();
  for (const auto& f : families()) {
    const std::string name = f.name;
    if (f.halvesMatch) {
      bool matches = true;
      for (double t : ts) {
        const double expected = t <= 0.5 ? f.easeIn(t * 2) / 2 : 0.5 + f.easeOut(t * 2 - 1) / 2;
        matches = matches && std::abs(f.easeInOut(t) - expected) < kEpsilon;
      }
      check(matches, name + ": easeInOut is easeIn on the first half and easeOut on the second");
    } else {
      double worst = 0;
      for (double t : ts) {
        if (t <= 0.5) worst = std::max(worst, std::abs(f.easeInOut(t) - f.easeIn(t * 2) / 2));
      }
      check(worst > 0.01, name + ": easeInOut deliberately differs from the scaled halves");
    }
    const double wide = std::abs(f.easeInOut(0.5 + 1e-4) - f.easeInOut(0.5 - 1e-4));
    const double narrow = std::abs(f.easeInOut(0.5 + 1e-6) - f.easeInOut(0.5 - 1e-6));
    check(close(f.easeInOut(0.5), 0.5, 12) && narrow < std::max(wide / 2, 1e-9), name + ": easeInOut passes the midpoint without a step");
  }
}

void family_reflection_and_shape() {
  const auto ts = samples();
  for (const auto& f : families()) {
    const std::string name = f.name;
    bool reflected = true;
    for (double t : ts) reflected = reflected && std::abs(f.easeOut(t) - (1 - f.easeIn(1 - t))) < kEpsilon;
    check(reflected, name + ": easeOut(t) is 1 - easeIn(1 - t)");
    if (f.monotonic) {
      bool rising = true;
      for (std::size_t i = 1; i < ts.size(); ++i) rising = rising && f.easeIn(ts[i]) >= f.easeIn(ts[i - 1]);
      check(rising, name + ": easeIn never steps back");
    }
  }
  for (const auto& f : families()) {
    const std::string name = f.name;
    if (name != "Back" && name != "Elastic") continue;
    double low = 1, high = 0;
    for (double t : ts) {
      low = std::min(low, f.easeIn(t));
      high = std::max(high, f.easeOut(t));
    }
    check(low < -0.05 && high > 1.05, name + ": easeIn dips below 0 and easeOut rises above 1");
  }
  bool inside = true, reverses = false;
  for (std::size_t i = 0; i < ts.size(); ++i) {
    const double value = ez::ease_in_bounce(ts[i]);
    inside = inside && value >= 0 && value <= 1;
    if (i > 0) reverses = reverses || value < ez::ease_in_bounce(ts[i - 1]);
  }
  check(inside && reverses, "Bounce reverses without ever leaving the unit interval");
}

void per_curve_values() {
  check(ez::ease_in_back(0.3) < 0 && ez::ease_out_back(0.7) > 1, "Back undershoots on the way in and overshoots on the way out");
  check(ez::ease_in_bounce(0.5) < 0.5 && ez::ease_out_bounce(0.5) > 0.5, "Bounce in is slow and out is fast at the midpoint");
  check(close(ez::ease_in_circular(0.5), 1 - std::sqrt(0.75)), "easeInCircular(0.5) is 1 - sqrt(0.75)");
  check(ez::ease_in_elastic(0.8) < 0 && ez::ease_out_elastic(0.6) > 1, "Elastic oscillates past its bounds");
  check(ez::ease_in_exponential(0) == 0 && ez::ease_out_exponential(1) == 1 && ez::ease_in_out_exponential(0) == 0 &&
            ez::ease_in_out_exponential(1) == 1,
        "Exponential's special-cased endpoints are exact");
  check(ez::ease_linear(0.25) == 0.25 && ez::ease_linear(0.5) == 0.5 && ez::ease_linear(0.75) == 0.75, "easeLinear returns t unchanged");
  for (const auto& f : families()) {
    const std::string name = f.name;
    if (name == "Back" || name == "Elastic") continue;
    check(f.easeIn(0.5) < 0.5 && f.easeOut(0.5) > 0.5, name + ": easeIn is slow and easeOut fast at the midpoint");
  }
  check(close(ez::ease_smoothstep(0.25), 0.15625) && close(ez::ease_smoothstep(0.5), 0.5) && close(ez::ease_smootherstep(0.5), 0.5),
        "smoothstep's known cubic value, and both steps pass the midpoint");
  check(ez::ease_smoothstep(0) == 0 && ez::ease_smoothstep(1) == 1 && ez::ease_smootherstep(0) == 0 && ez::ease_smootherstep(1) == 1,
        "smoothstep and smootherstep have exact endpoints");
  check(ez::ease_smootherstep(0.25) < ez::ease_smoothstep(0.25), "smootherstep approaches the endpoints more gently");
  bool rising = true;
  for (int i = 1; i <= 20; ++i) {
    rising = rising && ez::ease_smoothstep(i / 20.0) >= ez::ease_smoothstep((i - 1) / 20.0) &&
             ez::ease_smootherstep(i / 20.0) >= ez::ease_smootherstep((i - 1) / 20.0);
  }
  check(rising, "smoothstep and smootherstep are monotonically increasing");
}

void power() {
  bool endpoints = true;
  for (double exponent : {0.5, 1.0, 2.0, 3.0, 5.0}) {
    const auto f = ez::ease_in_power(exponent);
    endpoints = endpoints && close(f(0), 0) && close(f(1), 1);
  }
  check(endpoints, "easeInPower runs 0 to 1 for any exponent");
  check(close(ez::ease_in_power(2)(0.5), ez::ease_in_quadratic(0.5), 10) && close(ez::ease_in_power(2)(0.3), ez::ease_in_quadratic(0.3), 10),
        "easeInPower(2) is easeInQuadratic");
  check(close(ez::ease_in_power(5)(0.5), ez::ease_in_quintic(0.5), 10) && close(ez::ease_in_power(5)(0.7), ez::ease_in_quintic(0.7), 10),
        "easeInPower(5) is easeInQuintic");
  check(close(ez::ease_in_power(0.5)(0.25), 0.5, 10), "easeInPower is sub-linear below exponent 1");
  check(close(ez::ease_out_power(2)(0.5), ez::ease_out_quadratic(0.5), 10) && close(ez::ease_out_power(2)(0.8), ez::ease_out_quadratic(0.8), 10),
        "easeOutPower(2) is easeOutQuadratic");
  const auto inOut = ez::ease_in_out_power(3);
  check(close(inOut(0.3), ez::ease_in_out_cubic(0.3), 10) && close(inOut(0.7), ez::ease_in_out_cubic(0.7), 10) && close(inOut(0.5), 0.5) &&
            close(inOut(0), 0) && close(inOut(1), 1),
        "easeInOutPower(3) is easeInOutCubic");
  const auto four = ez::ease_in_out_power(4);
  check(close(four(0.3) + four(0.7), 1) && close(four(0.1) + four(0.9), 1), "easeInOutPower is point-symmetric");
  bool strictIn = true, strictOut = true;
  for (int i = 1; i <= 20; ++i) {
    strictIn = strictIn && ez::ease_in_power(2.5)(i / 20.0) > ez::ease_in_power(2.5)((i - 1) / 20.0);
    strictOut = strictOut && ez::ease_out_power(3)(i / 20.0) > ez::ease_out_power(3)((i - 1) / 20.0);
  }
  check(strictIn && strictOut, "easeInPower and easeOutPower are strictly increasing");
}

void damped_sine() {
  const auto in = ez::ease_in_damped_sine(1, 0.4), out = ez::ease_out_damped_sine(1, 0.4), inOut = ez::ease_in_out_damped_sine(1, 0.45);
  check(in(0) == 0 && in(1) == 1 && out(0) == 0 && out(1) == 1 && inOut(0) == 0 && inOut(1) == 1, "damped sine endpoints are exact");
  check(in(0.8) < 0 && out(0.6) > 1, "damped sine winds up below 0 and overshoots past 1");
  bool matchesIn = true, matchesOut = true, matchesInOut = true;
  for (double t : {0.1, 0.25, 0.4, 0.6, 0.75, 0.9}) {
    matchesIn = matchesIn && close(in(t), ez::ease_in_elastic(t), 10);
    matchesOut = matchesOut && close(out(t), ez::ease_out_elastic(t), 10);
  }
  for (double t : {0.1, 0.3, 0.5, 0.7, 0.9}) matchesInOut = matchesInOut && close(inOut(t), ez::ease_in_out_elastic(t), 10);
  check(matchesIn && matchesOut && matchesInOut, "damped sine at the fixed constants is the Elastic family");
  const auto wide = ez::ease_out_damped_sine(1, 0.9), narrow = ez::ease_out_damped_sine(1, 0.2);
  double widest = 0;
  for (int i = 1; i <= 99; ++i) widest = std::max(widest, std::abs(wide(i / 100.0) - narrow(i / 100.0)));
  check(widest > 0.5, "the period separates curves the fixed form collapses together");
  const auto peak = [](double amplitude) {
    double best = -1e9;
    for (int i = 1; i <= 99; ++i) best = std::max(best, ez::ease_out_damped_sine(amplitude, 0.4)(i / 100.0));
    return best;
  };
  check(peak(3) > peak(1), "overshoot grows with amplitude");
  const auto clamped = ez::ease_out_damped_sine(0.5, 0.4);
  check(close(clamped(0.2), out(0.2), 10) && close(clamped(0.5), out(0.5), 10) && close(clamped(0.8), out(0.8), 10) && std::isfinite(clamped(0.3)),
        "an amplitude below 1 is raised to the smallest defined curve");
  bool fallback = true;
  for (double period : {0.0, -1.0}) {
    const auto f = ez::ease_out_damped_sine(1, period);
    fallback = fallback && std::isfinite(f(0.5)) && close(f(0.5), out(0.5), 10);
  }
  check(fallback, "a non-positive period falls back to 0.4");
}

void cubic_bezier() {
  const auto css = ez::ease_cubic_bezier(0.25, 0.1, 0.25, 1.0);
  check(css(0) == 0 && css(1) == 1, "cubic Bezier endpoints are exact");
  const auto inOut = ez::ease_cubic_bezier(0.42, 0, 0.58, 1);
  check(inOut(-0.5) == 0 && inOut(1.5) == 1, "cubic Bezier clamps out-of-range input to the endpoints");
  bool identity = true;
  const auto diagonal = ez::ease_cubic_bezier(0, 0, 1, 1);
  for (int i = 0; i <= 10; ++i) identity = identity && close(diagonal(i / 10.0), i / 10.0, 3);
  check(identity, "the diagonal control points approximate the identity");
  check(close(css(0.5), 0.8024, 2) && css(0.5) > 0.5, "matches the CSS ease curve at its midpoint");
  bool rising = true;
  for (int i = 1; i <= 50; ++i) rising = rising && css(i / 50.0) >= css((i - 1) / 50.0) - 1e-6;
  check(rising, "cubic Bezier is monotonically non-decreasing");
  check(close(inOut(0.5), 0.5, 2), "ease-in-out inverts x to pass through the midpoint");
}

void smoothstep_range() {
  const auto f = ez::ease_smoothstep_range(10, 20);
  check(f(10) == 0 && f(20) == 1 && f(5) == 0 && f(25) == 1, "smoothstepRange maps the edges and clamps outside them");
  check(close(ez::ease_smoothstep_range(0, 100)(50), 0.5), "smoothstepRange applies smoothstep at the midpoint");
  const auto inverted = ez::ease_smoothstep_range(20, 10);
  check(inverted(20) == 0 && inverted(10) == 1 && close(inverted(15), 0.5), "edge0 > edge1 inverts the remap");
  bool unit = true;
  for (double t : {0.0, 0.25, 0.5, 0.75, 1.0}) unit = unit && close(ez::ease_smoothstep_range(0, 1)(t), ez::ease_smoothstep(t));
  check(unit, "smoothstepRange(0, 1) is smoothstep");
  const auto degenerate = ez::ease_smoothstep_range(5, 5);
  check(std::isnan(degenerate(5)) && degenerate(6) == 1 && degenerate(4) == 0, "a zero-width range is NaN at the edge and clamps either side");
}

void steps() {
  const auto end = ez::ease_steps(4, flight::String("jumpEnd"));
  check(end(0) == 0 && end(0.1) == 0 && close(end(0.25), 0.25) && close(end(0.6), 0.5) && close(end(0.99), 0.75) && end(1) == 1,
        "jumpEnd holds at 0 first and reaches 1 only at t=1");
  const auto implicit = ez::ease_steps(4);
  bool same = true;
  for (int i = 0; i <= 10; ++i) same = same && implicit(i / 10.0) == end(i / 10.0);
  check(same, "jumpEnd is the default position");
  const auto start = ez::ease_steps(4, flight::String("jumpStart"));
  check(close(start(0), 0.25) && close(start(0.1), 0.25) && close(start(0.5), 0.75) && start(0.99) == 1 && start(1) == 1,
        "jumpStart jumps immediately and reaches 1 before t=1");
  const auto none = ez::ease_steps(4, flight::String("jumpNone"));
  check(none(0) == 0 && close(none(0.25), 1.0 / 3) && close(none(0.5), 2.0 / 3) && none(0.99) == 1 && none(1) == 1,
        "jumpNone spans the closed range with count - 1 jumps");
  const auto one = ez::ease_steps(1, flight::String("jumpNone"));
  check(std::isnan(one(0)) && std::isnan(one(0.5)) && std::isnan(one(1)), "jumpNone with count 1 is NaN, as CSS forbids it");
  const auto both = ez::ease_steps(4, flight::String("jumpBoth"));
  check(close(both(0), 0.2) && close(both(0.1), 0.2) && close(both(0.25), 0.4) && close(both(0.99), 0.8) && both(1) == 1,
        "jumpBoth never outputs 0, with interior 1/(n+1) steps");
  bool rising = true;
  const auto five = ez::ease_steps(5, flight::String("jumpEnd"));
  for (int i = 1; i <= 50; ++i) rising = rising && five(i / 50.0) >= five((i - 1) / 50.0);
  check(rising, "jumpEnd is monotonically non-decreasing");
}

void steps_guard() {
  std::vector<std::pair<double, std::string>> seen;
  ez::set_easing_steps_guard(std::function<void(double, flight::types::StepPosition)>(
      [&seen](double count, flight::types::StepPosition position) { seen.emplace_back(count, position.to_utf8()); }));
  check(std::isnan(ez::ease_steps(1, flight::String("jumpNone"))(0.5)), "the reported degenerate call still returns NaN");
  ez::set_easing_steps_guard(std::nullopt);
  check(seen.size() == 1 && seen[0].first == 1 && seen[0].second == "jumpNone", "the guard reports the degenerate jumpNone call");

  seen.clear();
  ez::set_easing_steps_guard(std::function<void(double, flight::types::StepPosition)>(
      [&seen](double count, flight::types::StepPosition position) { seen.emplace_back(count, position.to_utf8()); }));
  static_cast<void>(ez::ease_steps(2, flight::String("jumpNone"))(0.5));
  static_cast<void>(ez::ease_steps(1, flight::String("jumpEnd"))(0.5));
  static_cast<void>(ez::ease_steps(1, flight::String("jumpBoth"))(0.5));
  ez::set_easing_steps_guard(std::nullopt);
  static_cast<void>(ez::ease_steps(1, flight::String("jumpNone"))(0.5));
  check(seen.empty(), "the guard stays silent for well-defined calls, and once uninstalled");
}

// The memory sink's constructor is not generated for this profile (overrides/include/flight/log/log.hpp), so
// the warning is captured by a sink installed directly: a LogSink is a callable, as in the source.
std::vector<std::string> capture_log(const std::function<void()>& run) {
  std::vector<std::string> messages;
  auto sink = flight::make_ref<flight::log::LogSink>([&messages](flight::Ref<flight::types::LogEntry> entry) {
    const auto& data = entry->data;
    if (const auto* text = std::get_if<flight::String>(&data)) {
      messages.push_back(text->to_utf8());
    } else {
      const auto message = std::get<flight::Record<flight::String, flight::Any>>(data).get(flight::String("message"));
      messages.push_back(message.has_value() && message->kind() == flight::AnyKind::string ? message->as_string().to_utf8() : std::string());
    }
  });
  flight::log::clear_log_once_keys();
  flight::log::add_log_sink(sink);
  // Removed on every exit: the sink writes into this frame's `messages`.
  struct Remove {
    flight::Ref<flight::log::LogSink> sink;
    ~Remove() { static_cast<void>(flight::log::remove_log_sink(sink)); }
  } remove{sink};
  run();
  return messages;
}

// The warning goes through flight::log::log_once. At this base the log package is partial
// (overrides/manifest.json, log-module-qualification-and-operators): merge_span_fields is a stub on every
// emission path, so any entry that passes the level gate throws instead of reaching a sink. That is a log
// gap, not an easing one, so the warning case asserts what each state can show: with an emitting log, the
// one warning upstream asserts; with the stub, that the guard fired and reached the log's emission path,
// which the stub's own error names. The silent cases emit nothing and keep their full upstream assertions.
void easing_guards() {
  bool reachedStub = false;
  std::vector<std::string> warned;
  try {
    warned = capture_log([] {
      ez::enable_easing_guards();
      struct Disable {
        ~Disable() { ez::disable_easing_guards(); }
      } disable;
      check(std::isnan(ez::ease_steps(1, flight::String("jumpNone"))(0.5)), "with guards enabled the degenerate call is still NaN");
    });
  } catch (const flight::Error& error) {
    reachedStub = std::string(error.what()).find("merge_span_fields") != std::string::npos;
  }
  check(reachedStub || (warned.size() == 1 && warned[0].find("count >= 2") != std::string::npos),
        "enableEasingGuards warns once on the degenerate call, naming count >= 2");

  const auto quiet = capture_log([] {
    ez::enable_easing_guards();
    static_cast<void>(ez::ease_steps(2, flight::String("jumpNone"))(0.5));
    static_cast<void>(ez::ease_steps(1, flight::String("jumpEnd"))(0.5));
    static_cast<void>(ez::ease_steps(1, flight::String("jumpStart"))(0.5));
    static_cast<void>(ez::ease_steps(1, flight::String("jumpBoth"))(0.5));
    ez::disable_easing_guards();
  });
  check(quiet.empty(), "the guard stays silent for well-defined calls");

  const auto disabled = capture_log([] {
    ez::enable_easing_guards();
    ez::disable_easing_guards();
    check(std::isnan(ez::ease_steps(1, flight::String("jumpNone"))(0.5)), "disabling removes the report, not the NaN");
  });
  const auto unguarded = capture_log([] { static_cast<void>(ez::ease_steps(1, flight::String("jumpNone"))(0.5)); });
  check(disabled.empty() && unguarded.empty(), "disabling, or never enabling, leaves the degenerate call silent");
}

void combinators() {
  const auto clamp = ez::ease_clamp(ez::ease_in_cubic);
  check(close(clamp(0.5), ez::ease_in_cubic(0.5)) && close(clamp(-0.5), 0) && close(clamp(1.5), 1), "easeClamp clamps t into [0, 1]");
  const auto twice = ez::ease_clamp(ez::ease_clamp(ez::ease_in_cubic));
  check(close(twice(-1), 0) && close(twice(2), 1), "easeClamp composes");
  check(close(ez::ease_clamp_output(ez::ease_in_cubic, 0, 1)(0.5), ez::ease_in_cubic(0.5)), "easeClampOutput passes in-range output");
  check(ez::ease_clamp_output([](double) { return -0.5; }, 0, 1)(0.5) == 0 && ez::ease_clamp_output([](double) { return 1.5; }, 0, 1)(0.5) == 1,
        "easeClampOutput clamps below and above");
  const auto ranged = ez::ease_clamp_output(ez::ease_in_cubic, 0.2, 0.8);
  check(ranged(0) == 0.2 && ranged(1) == 0.8, "easeClampOutput respects a custom range");
  const auto invert = ez::ease_invert(ez::ease_in_cubic);
  check(close(invert(0), 1) && close(invert(1), 0) && close(invert(0.3), 1 - ez::ease_in_cubic(0.3)) &&
            close(ez::ease_invert(invert)(0.4), ez::ease_in_cubic(0.4)),
        "easeInvert is 1 - f(t), and its own inverse");
  const auto mirror = ez::ease_mirror(ez::ease_in_cubic);
  check(close(mirror(0), 0) && close(mirror(1), 1) && close(mirror(0.5), 0.5) && close(mirror(0.3) + mirror(0.7), 1) &&
            close(mirror(0.1) + mirror(0.9), 1),
        "easeMirror is point-symmetric through the midpoint");
  const auto reverse = ez::ease_reverse(ez::ease_in_cubic);
  check(close(reverse(0.3), 1 - ez::ease_in_cubic(0.7)) && close(reverse(0), 0) && close(reverse(1), 1) &&
            close(reverse(0.5), ez::ease_out_cubic(0.5), 10) && close(reverse(0.3), ez::ease_out_cubic(0.3), 10) &&
            close(ez::ease_reverse(reverse)(0.4), ez::ease_in_cubic(0.4)),
        "easeReverse turns In into Out, and is its own inverse");
  const auto scaled = ez::ease_scale_output(ez::ease_in_cubic, 100, 300);
  check(close(scaled(0), 100) && close(scaled(1), 300) && close(scaled(0.5), 100 + ez::ease_in_cubic(0.5) * 200), "easeScaleOutput maps onto a range");
  const auto down = ez::ease_scale_output(ez::ease_in_cubic, 300, 100);
  check(close(down(0), 300) && close(down(1), 100) && close(ez::ease_scale_output(ez::ease_in_cubic, 0, 1)(0.5), ez::ease_in_cubic(0.5)),
        "easeScaleOutput handles inverted and unit ranges");
}

flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::EasingSegment>>>> segment(Ease ease, std::optional<double> weight = std::nullopt) {
  auto out = flight::make_ref<flight::types::EasingSegment>();
  out->ease = std::move(ease);
  out->weight = weight;
  return flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::EasingSegment>>>>(out);
}

template <typename Function>
bool throws(Function function) {
  try {
    function();
  } catch (const flight::Error&) {
    return true;
  }
  return false;
}

void piecewise() {
  check(throws([] { static_cast<void>(ez::ease_piecewise({})); }), "an empty segment list throws");
  check(throws([] { static_cast<void>(ez::ease_piecewise({segment(ez::ease_linear, 0.0)})); }), "a zero total weight throws");
  const auto single = ez::ease_piecewise({segment(ez::ease_in_cubic)});
  check(close(single(0), 0) && close(single(0.5), ez::ease_in_cubic(0.5)) && close(single(1), 1), "one segment is the wrapped function");
  const auto halves = ez::ease_piecewise({segment(ez::ease_linear), segment([](double) { return 0.0; })});
  check(close(halves(0.25), 0.5) && close(halves(0.75), 0), "two equal segments split the range at 0.5");
  const auto weighted = ez::ease_piecewise({segment(ez::ease_linear, 1.0), segment([](double) { return 1.0; }, 2.0)});
  check(close(weighted(1.0 / 6), 0.5) && close(weighted(2.0 / 3), 1), "a weight of 2 takes twice the range");
  const auto ends = ez::ease_piecewise({segment(ez::ease_linear), segment(ez::ease_in_cubic)});
  const auto ends2 = ez::ease_piecewise({segment(ez::ease_in_cubic), segment(ez::ease_linear)});
  check(close(ends(0), 0) && close(ends(1), 1) && close(ends2(0), 0) && close(ends2(1), 1), "t = 0 and t = 1 are boundary-safe");
  const auto explicitWeights = ez::ease_piecewise({segment(ez::ease_linear, 1.0), segment(ez::ease_in_cubic, 1.0)});
  bool same = true;
  for (int i = 0; i <= 10; ++i) same = same && close(ends(i / 10.0), explicitWeights(i / 10.0), 10);
  check(same, "an absent weight is 1");
}

void sampling() {
  check(throws([] { static_cast<void>(ez::create_easing_samples(ez::ease_linear, 0)); }) &&
            throws([] { static_cast<void>(ez::create_easing_samples(ez::ease_linear, -1)); }),
        "a count below 1 throws");
  check(throws([] { static_cast<void>(ez::create_easing_samples(ez::ease_linear, std::numeric_limits<double>::infinity())); }) &&
            throws([] { static_cast<void>(ez::create_easing_samples(ez::ease_linear, std::numeric_limits<double>::quiet_NaN())); }),
        "a non-finite count throws");
  check(ez::create_easing_samples(ez::ease_linear, 5).size() == 5, "a fresh Float32Array of count entries when out is omitted");
  flight::Float32Array out(5);
  check(ez::create_easing_samples(ez::ease_linear, 5, out) == out, "the provided out array is written and returned, by identity");
  const auto cubic = ez::create_easing_samples(ez::ease_in_cubic, 11);
  check(close(cubic.get_index(0), ez::ease_in_cubic(0)) && close(cubic.get_index(10), ez::ease_in_cubic(1)), "first and last samples are ease(0) and ease(1)");
  const auto linear = ez::create_easing_samples(ez::ease_linear, 6);
  bool uniform = true;
  for (int i = 0; i < 6; ++i) uniform = uniform && close(linear.get_index(i), i / 5.0, 5);
  check(uniform, "easeLinear samples uniformly");
  check(close(ez::create_easing_samples(ez::ease_in_cubic, 1).get_index(0), ez::ease_in_cubic(0.5)), "count 1 samples the midpoint");
  flight::Float32Array wide(10);
  const auto written = ez::create_easing_samples(ez::ease_linear, 5, wide);
  check(close(written.get_index(4), 1) && written.get_index(5) == 0, "only count entries of a longer out array are written");
  check(ez::create_easing_samples(ez::ease_linear, 5.9).size() == 5, "a fractional count is floored");
}

void derivative() {
  bool flat = true;
  for (double t : {0.0, 0.25, 0.5, 0.75, 1.0}) flat = flat && close(ez::get_easing_derivative(ez::ease_linear, t), 1, 4);
  check(flat, "easeLinear's slope is 1 everywhere");
  check(close(ez::get_easing_derivative(ez::ease_in_cubic, 0), 0, 3) && close(ez::get_easing_derivative(ez::ease_in_cubic, 1), 3, 3),
        "easeInCubic's slope is 0 at the start and 3 at the end (one-sided at the boundaries)");
  check(close(ez::get_easing_derivative(ez::ease_out_cubic, 0), 3, 3) && close(ez::get_easing_derivative(ez::ease_out_cubic, 1), 0, 3),
        "easeOutCubic's slope is 3 at the start and 0 at the end");
  check(close(ez::get_easing_derivative(ez::ease_in_cubic, 0.5), 0.75, 3), "the centered difference at an interior point");
  check(close(ez::get_easing_derivative(ez::ease_linear, 0.5, 1e-4), 1, 3), "a custom epsilon");
  // The boundaries are one-sided, never sampling outside [0, 1]. A polynomial cannot show that -- its centered
  // difference at 0 or 1 is just as good -- but a curve clamped at the interval can: a centered difference
  // there would read the flat outside and report half the slope.
  const auto clamped = ez::ease_clamp(ez::ease_linear);
  check(close(ez::get_easing_derivative(clamped, 0), 1, 4) && close(ez::get_easing_derivative(clamped, 1), 1, 4),
        "the boundary difference is one-sided, so a clamped curve keeps its slope at 0 and 1");
}

}  // namespace

int main() {
  family_distinctness();
  family_endpoints();
  family_halves();
  family_reflection_and_shape();
  per_curve_values();
  power();
  damped_sine();
  cubic_bezier();
  smoothstep_range();
  steps();
  steps_guard();
  easing_guards();
  combinators();
  piecewise();
  sampling();
  derivative();
  return failures == 0 ? 0 : 1;
}
