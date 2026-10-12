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
    check(!ok && point->x == 7 && point->y == 8, "an empty path samples false and leaves outputs unchanged");
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

}  // namespace

int main() {
  construction();
  sampling();
  seeking();
  updates();
  return failures == 0 ? 0 : 1;
}
