// The generated @flighthq/motionpath package at a627849, exercised natively. Expectations are ported from the
// package's own tests (packages/motionpath/src/motionPath.test.ts): construction defaults and the cached
// arc length, sampling position, tangent and heading along real paths, progress, seeking and its clamps,
// the direction-aware end query, and every loop mode of the per-frame update, including single large steps.
#include <flight/geometry/vector2.hpp>
#include <flight/motionpath/motion_path.hpp>
#include <flight/path/path.hpp>

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
bool close(double actual, double expected, int digits = 6) {
  return std::abs(actual - expected) < std::pow(10.0, -digits) / 2.0;
}

// The readonly row view the generated API takes for a `Readonly<T>` parameter.
template <typename Pointer>
auto view(const Pointer& value) {
  using Element = typename Pointer::element_type;
  return flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<Element>>>>(value);
}

namespace motionpath = flight::motionpath;
using flight::types::MotionPath;
using flight::types::Path;

// A straight horizontal line from (0,0) to (100,0): arc length 100, tangent (1,0) everywhere.
flight::Ref<Path> line() {
  auto path = flight::path::create_path();
  flight::path::append_path_move_to(path, 0, 0);
  flight::path::append_path_line_to(path, 100, 0);
  return path;
}

// Right 100, then down 100: heading 0 on the first leg and +pi/2 on the second.
flight::Ref<Path> corner() {
  auto path = line();
  flight::path::append_path_line_to(path, 100, 100);
  return path;
}

flight::Ref<MotionPath> make(const flight::Ref<Path>& path, std::optional<double> speed = std::nullopt, const char* mode = nullptr) {
  return motionpath::create_motion_path(path, speed,
                                        mode != nullptr ? std::optional<flight::String>(flight::String(mode)) : std::nullopt);
}

void construction() {
  auto mp = make(line());
  check(close(mp->length, 100), "caches the arc length measured from the path");
  check(mp->distance == 0 && mp->direction == 1 && mp->speed == 0 && mp->loop_mode == flight::String("clamp"),
        "starts at the path start, forward, with clamp defaults");
  auto custom = make(line(), 50, "pingpong");
  check(custom->speed == 50 && custom->loop_mode == flight::String("pingpong"), "carries the given speed and loop mode");
  auto path = line();
  check(make(path)->path == path, "retains the same path reference");
}

void sampling() {
  {
    auto mp = make(line());
    motionpath::set_motion_path_distance(mp, 50);
    check(close(motionpath::get_motion_path_heading(view(mp)), 0), "heading 0 along a rightward line");
  }
  {
    auto mp = make(corner());
    motionpath::set_motion_path_distance(mp, 50);
    const double before = motionpath::get_motion_path_heading(view(mp));
    motionpath::set_motion_path_distance(mp, 150);
    const double after = motionpath::get_motion_path_heading(view(mp));
    check(close(before, 0) && close(after, std::numbers::pi / 2), "heading turns at the corner");
  }
  {
    auto mp = make(line());
    motionpath::set_motion_path_distance(mp, 50);
    auto point = flight::geometry::create_vector2();
    auto tangent = flight::geometry::create_vector2();
    const bool ok = motionpath::get_motion_path_position(view(mp), point, tangent);
    check(ok && close(point->x, 50) && close(point->y, 0) && close(tangent->x, 1) && close(tangent->y, 0),
          "samples point and tangent at the current distance");
  }
  {
    auto mp = make(flight::path::create_path());
    auto point = flight::geometry::create_vector2(7, 8);
    auto tangent = flight::geometry::create_vector2(9, 9);
    const bool ok = motionpath::get_motion_path_position(view(mp), point, tangent);
    check(!ok && point->x == 7 && point->y == 8, "an empty path samples false and leaves the point unchanged");
    // Upstream's comment says both outputs are left unchanged, but its test checks only the point, and the
    // source samples the tangent unconditionally: @flighthq/path writes its documented (1, 0) fallback.
    check(tangent->x == 1 && tangent->y == 0, "the tangent takes path's (1, 0) fallback, as the source does");
    check(motionpath::get_motion_path_heading(view(mp)) == 0, "an empty path's heading falls back to the (1, 0) tangent: 0 radians");
  }
  {
    auto mp = make(line());
    motionpath::set_motion_path_distance(mp, 25);
    check(close(motionpath::get_motion_path_progress(view(mp)), 0.25), "progress is distance / length");
    check(motionpath::get_motion_path_progress(view(make(flight::path::create_path()))) == 0, "a zero-length path reports progress 0");
  }
}

void seeking() {
  auto mp = make(line());
  check(!motionpath::is_motion_path_at_end(view(mp)), "not at the end at the start going forward");
  motionpath::set_motion_path_distance(mp, 100);
  check(motionpath::is_motion_path_at_end(view(mp)), "at the end at length going forward");
  motionpath::set_motion_path_direction(mp, -1);
  check(!motionpath::is_motion_path_at_end(view(mp)), "the end follows the direction");
  motionpath::set_motion_path_distance(mp, 0);
  check(motionpath::is_motion_path_at_end(view(mp)), "at the end at 0 going backward");

  auto d = make(line());
  motionpath::set_motion_path_distance(d, 25);
  motionpath::set_motion_path_direction(d, -1);
  check(d->direction == -1 && d->distance == 25, "direction changes without moving");
  motionpath::set_motion_path_distance(d, 150);
  check(d->distance == 100, "distance clamps above the length");
  motionpath::set_motion_path_distance(d, -10);
  check(d->distance == 0, "distance clamps below 0");

  auto p = make(line());
  motionpath::set_motion_path_progress(p, 0.25);
  check(close(p->distance, 25), "progress seeks a normalized distance");
  motionpath::set_motion_path_progress(p, 1.5);
  check(close(p->distance, 100), "progress clamps above 1");
  motionpath::set_motion_path_progress(p, -0.5);
  check(p->distance == 0, "progress clamps below 0");
}

void updates() {
  {
    auto mp = make(line(), 50);
    motionpath::update_motion_path(mp, 1);
    check(close(mp->distance, 50) && close(motionpath::get_motion_path_progress(view(mp)), 0.5), "advances speed * deltaTime");
  }
  {
    auto mp = make(line(), 50, "clamp");
    motionpath::update_motion_path(mp, 10);
    check(close(mp->distance, 100) && close(motionpath::get_motion_path_progress(view(mp)), 1), "clamp stops at the end");
    motionpath::update_motion_path(mp, 5);
    check(close(mp->distance, 100), "clamp stays at the end");
  }
  {
    auto mp = make(line(), 30, "loop");
    motionpath::update_motion_path(mp, 4);
    check(close(mp->distance, 20), "loop wraps past the end");
  }
  {
    auto mp = make(line(), 30, "loop");
    mp->direction = -1;
    motionpath::update_motion_path(mp, 4);
    check(close(mp->distance, 80), "loop wraps backward into [0, length)");
  }
  {
    auto mp = make(line(), 30, "pingpong");
    mp->distance = 90;
    motionpath::update_motion_path(mp, 1);
    check(close(mp->distance, 80) && mp->direction == -1, "pingpong reflects at the end and flips to -1");
    motionpath::update_motion_path(mp, 1);
    check(close(mp->distance, 50) && mp->direction == -1, "pingpong then travels back");
  }
  {
    auto mp = make(line(), 250, "loop");
    motionpath::update_motion_path(mp, 1);
    check(close(mp->distance, 50), "loop resolves a step that crosses the path several times");
  }
  {
    auto mp = make(line(), 250, "pingpong");
    motionpath::update_motion_path(mp, 1);
    check(close(mp->distance, 50) && mp->direction == 1, "pingpong resolves a step that crosses the path several times");
  }
  {
    auto mp = make(line(), 80, "pingpong");
    mp->distance = 50;
    mp->direction = -1;
    motionpath::update_motion_path(mp, 1);
    check(close(mp->distance, 30) && mp->direction == 1, "pingpong reflects at the start and flips to +1");
  }
  {
    auto mp = make(line(), 50);
    motionpath::update_motion_path(mp, 0);
    motionpath::update_motion_path(mp, -1);
    check(mp->distance == 0, "deltaTime <= 0 is a no-op");
  }
  {
    auto mp = make(flight::path::create_path(), 50);
    motionpath::update_motion_path(mp, 1);
    check(mp->length == 0 && mp->distance == 0, "a zero-length path is a no-op");
  }
}

// Not in upstream, which samples only straight lines: a curve, where flattening tolerance matters. A
// semicircle of radius 50 from (100, 0) around (50, 0) has arc length 50 * pi. `append_path_arc` approximates
// the arc with cubics and `getPathLength` flattens those to chords, so every bound below is derived from those
// two errors rather than picked to pass:
// - a chord whose sagitta is at most s, on radius r, spans at most 2 * acos(1 - s / r); its direction differs
//   from the true tangent by at most half that, acos(1 - 0.25 / 50) = 0.0999 rad at the default s = 0.25;
// - a sampled point lies within s of the circle, plus the cubic approximation's radial error (2.7e-4 * r,
//   0.0136 here): 0.25 + 0.0136 < 0.3;
// - the length is within the chord deficit (s / (3 r) of the arc, 0.26) plus the cubic overshoot (1.4e-4 of the
//   arc, 0.022) of 50 * pi: under 0.3.
flight::Ref<Path> semicircle() {
  auto path = flight::path::create_path();
  flight::path::append_path_move_to(path, 100, 0);
  flight::path::append_path_arc(path, 50, 0, 50, 0, std::numbers::pi);
  return path;
}

void curves() {
  const double arc = 50 * std::numbers::pi;
  auto mp = make(semicircle());
  check(std::abs(mp->length - arc) < 0.3, "a semicircle's cached length is 50 * pi within the flattening error");

  const auto withTolerance = [](double tolerance) {
    return motionpath::create_motion_path(semicircle(), std::nullopt, std::nullopt, tolerance)->length;
  };
  const double coarse = withTolerance(1.0), fine = withTolerance(0.01);
  check(coarse != fine && std::abs(fine - arc) < std::abs(coarse - arc),
        "the tolerance reaches the arc-length measurement, and a tighter one measures closer to 50 * pi");

  bool onCircle = true;
  for (double t : {0.0, 0.1, 0.25, 0.5, 0.75, 0.9, 1.0}) {
    motionpath::set_motion_path_progress(mp, t);
    auto point = flight::geometry::create_vector2(), tangent = flight::geometry::create_vector2();
    onCircle = onCircle && motionpath::get_motion_path_position(view(mp), point, tangent) &&
               std::abs(std::hypot(point->x - 50, point->y) - 50) < 0.3 && close(std::hypot(tangent->x, tangent->y), 1, 9);
  }
  check(onCircle, "sampled points stay on the circle and tangents are unit length");

  motionpath::set_motion_path_distance(mp, 0);
  check(std::abs(motionpath::get_motion_path_heading(view(mp)) - std::numbers::pi / 2) < 0.0999,
        "the heading leaves (100, 0) along +y, a quarter turn");
  motionpath::set_motion_path_progress(mp, 1);
  check(std::abs(motionpath::get_motion_path_heading(view(mp)) + std::numbers::pi / 2) < 0.0999,
        "and arrives at (0, 0) along -y");
  motionpath::set_motion_path_progress(mp, 0.5);
  const double middle = motionpath::get_motion_path_heading(view(mp));
  check(std::abs(std::abs(middle) - std::numbers::pi) < 0.0999, "at the top it points along -x");

  // Heading is atan2 of the sampled tangent, so it and getMotionPathPosition agree everywhere.
  bool agrees = true;
  for (double t : {0.05, 0.3, 0.6, 0.95}) {
    motionpath::set_motion_path_progress(mp, t);
    auto point = flight::geometry::create_vector2(), tangent = flight::geometry::create_vector2();
    static_cast<void>(motionpath::get_motion_path_position(view(mp), point, tangent));
    agrees = agrees && motionpath::get_motion_path_heading(view(mp)) == std::atan2(tangent->y, tangent->x);
  }
  check(agrees, "heading is exactly atan2 of the tangent getMotionPathPosition samples");

  {
    auto exact = make(line(), 1, "pingpong");
    motionpath::update_motion_path(exact, 100);
    check(exact->distance == 100 && exact->direction == 1,
          "a pingpong step landing exactly on the end stays on the outbound leg (advanced <= length)");
    auto backward = make(line(), 1, "clamp");
    motionpath::set_motion_path_distance(backward, 60);
    motionpath::set_motion_path_direction(backward, -1);
    motionpath::update_motion_path(backward, 25);
    check(backward->distance == 35, "clamp travels backward along a reversed direction");
    motionpath::update_motion_path(backward, 1000);
    check(backward->distance == 0 && motionpath::is_motion_path_at_end(view(backward)), "and stops at 0");
    bool inert = true;
    for (const char* mode : {"loop", "pingpong"}) {
      auto empty = make(flight::path::create_path(), 5, mode);
      motionpath::update_motion_path(empty, 16);
      inert = inert && empty->distance == 0 && empty->direction == 1;
    }
    check(inert, "a zero-length path is inert in loop and pingpong too, where wrapping would divide by zero");
  }

  auto moving = make(semicircle(), 0.05, "loop");
  motionpath::update_motion_path(moving, 1000);
  check(close(moving->distance, 50, 9) && close(motionpath::get_motion_path_progress(view(moving)), 50 / moving->length, 12),
        "timing is speed * deltaTime path units, the same on a curve");
}

}  // namespace

int main() {
  construction();
  sampling();
  seeking();
  updates();
  curves();
  return failures == 0 ? 0 : 1;
}
