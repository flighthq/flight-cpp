// The generated @flighthq/velocity package at a627849, exercised natively. Expectations are ported from the
// package's own tests (packages/velocity/src/velocityField.test.ts and velocitySample.test.ts): the vector
// helpers and their out-aliasing, the per-frame field with identity-keyed samples, explicit contribution
// and suppression, staleness, the explanation query, and affine reprojection of a sample.
//
// contributeTransformVelocity (transformVelocity.test.ts) is not covered, and not because a native node is
// missing: scene2d's create_display_object supplies one. The emitted traversal does not instantiate for any real
// node at a627849 -- its node calls lack their conversion and template argument
// (docs/compiler-requests/generic-intersection-argument-widening.md), its sample key reads shared_object() from
// a merged row (generic-node-row-into-object-parameter.md), and @flighthq/node's own world-matrix queries fail
// to instantiate (node-transform-runtime-owner-argument.md).
//
// Tolerances: `close` is upstream's toBeCloseTo default (two digits) wherever upstream uses it; everything else
// is exact, because the arithmetic is exact in binary for the values used (small integers, halves, and the
// 3-4-5 triangle's 0.6/0.8 only through `close`).
#include <flight/geometry/matrix.hpp>
#include <flight/velocity/velocity_field.hpp>
#include <flight/velocity/velocity_sample.hpp>

#include <cmath>
#include <iostream>

namespace {

int failures = 0;

void check(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}

// Vitest's toBeCloseTo(expected) with its default two digits.
bool close(double actual, double expected) { return std::abs(actual - expected) < 0.005; }

// The readonly row view the generated API takes for a `Readonly<T>` parameter.
template <typename Pointer>
auto view(const Pointer& value) {
  using Element = typename Pointer::element_type;
  return flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<Element>>>>(value);
}

namespace velocity = flight::velocity;
using flight::types::Velocity2D;

flight::Ref<Velocity2D> v(double x, double y) { return flight::make_ref<Velocity2D>(Velocity2D{.x = x, .y = y}); }

bool equals(const flight::Ref<Velocity2D>& value, double x, double y) { return value->x == x && value->y == y; }

// A fresh object identity to key the field by; the source type is `object`, so any reference serves.
flight::Ref<void> source() { return v(0.0, 0.0); }

void arithmetic() {
  auto out = v(0, 0);
  velocity::add_velocity(out, view(v(1, 2)), view(v(3, 4)));
  check(equals(out, 4, 6), "add");
  auto a = v(5, 3);
  velocity::add_velocity(a, view(a), view(v(1, 1)));
  check(equals(a, 6, 4), "add is alias-safe");

  velocity::subtract_velocity(out, view(v(5, 3)), view(v(2, 1)));
  check(equals(out, 3, 2), "subtract");
  auto s = v(5, 3);
  velocity::subtract_velocity(s, view(s), view(v(2, 1)));
  check(equals(s, 3, 2), "subtract is alias-safe");

  velocity::scale_velocity(out, view(v(3, -2)), 2);
  check(equals(out, 6, -4), "scale");
  auto k = v(4, 2);
  velocity::scale_velocity(k, view(k), 0.5);
  check(equals(k, 2, 1), "scale is alias-safe");

  velocity::copy_velocity(out, view(v(5, -3)));
  check(equals(out, 5, -3), "copy");
  auto c = v(7, 2);
  velocity::copy_velocity(c, view(c));
  check(equals(c, 7, 2), "copy is alias-safe");

  auto z = v(1, 1);
  check(velocity::zero_velocity(z) == z && equals(z, 0, 0), "zero returns the out reference, zeroed");
}

void magnitude() {
  auto out = v(0, 0);
  velocity::clamp_velocity(out, view(v(3, 0)), 5);
  check(equals(out, 3, 0), "clamp leaves a short velocity unchanged");
  velocity::clamp_velocity(out, view(v(10, 0)), 5);
  check(close(out->x, 5) && close(out->y, 0), "clamp shortens to maxLength");
  velocity::clamp_velocity(out, view(v(3, 4)), 2.5);
  check(close(velocity::length_of_velocity(view(out)), 2.5), "clamp a diagonal to maxLength");
  auto c = v(10, 0);
  velocity::clamp_velocity(c, view(c), 5);
  check(close(c->x, 5) && close(c->y, 0), "clamp is alias-safe");
  velocity::clamp_velocity(out, view(v(0, 0)), 5);
  check(equals(out, 0, 0), "clamp of zero has no division error");

  velocity::normalize_velocity(out, view(v(3, 4)));
  check(close(velocity::length_of_velocity(view(out)), 1) && close(out->x, 0.6) && close(out->y, 0.8), "normalize");
  velocity::normalize_velocity(out, view(v(0, 0)));
  check(equals(out, 0, 0), "normalize of zero is zero");
  auto n = v(5, 0);
  velocity::normalize_velocity(n, view(n));
  check(equals(n, 1, 0), "normalize is alias-safe");

  check(close(velocity::length_of_velocity(view(v(3, 4))), 5) && velocity::length_of_velocity(view(v(0, 0))) == 0, "length");
  check(velocity::is_velocity_zero(view(v(0, 0))) && !velocity::is_velocity_zero(view(v(0.001, 0))), "isZero exact");
  check(velocity::is_velocity_zero(view(v(0.001, 0)), 0.01) && !velocity::is_velocity_zero(view(v(0.1, 0)), 0.01), "isZero epsilon");
}

void blending() {
  auto out = v(0, 0);
  velocity::damp_velocity(out, view(v(10, 5)), view(v(0, 0)), 1);
  check(equals(out, 10, 5), "damp factor 1 is current");
  velocity::damp_velocity(out, view(v(10, 5)), view(v(2, 3)), 0);
  check(equals(out, 2, 3), "damp factor 0 is previous");
  velocity::damp_velocity(out, view(v(10, 0)), view(v(0, 0)), 0.5);
  check(equals(out, 5, 0), "damp blends");
  auto d = v(10, 0);
  velocity::damp_velocity(d, view(d), view(v(0, 0)), 0.5);
  check(equals(d, 5, 0), "damp is alias-safe");

  velocity::lerp_velocity(out, view(v(1, 2)), view(v(9, 8)), 0);
  check(equals(out, 1, 2), "lerp t=0 is a");
  velocity::lerp_velocity(out, view(v(1, 2)), view(v(9, 8)), 1);
  check(equals(out, 9, 8), "lerp t=1 is b");
  velocity::lerp_velocity(out, view(v(0, 0)), view(v(10, 4)), 0.5);
  check(equals(out, 5, 2), "lerp interpolates");
  auto l = v(0, 0);
  velocity::lerp_velocity(l, view(l), view(v(10, 0)), 0.5);
  check(equals(l, 5, 0), "lerp is alias-safe");
}

void field() {
  {
    auto f = velocity::create_velocity_field();
    check(f->frame_id == 0 && equals(velocity::get_velocity(f, source(), v(9, 9)), 0, 0), "a new field is at frame 0 and unknown sources read zero");
    velocity::begin_velocity_frame(f);
    check(f->frame_id == 1, "beginVelocityFrame advances the frame id");
  }
  {
    auto f = velocity::create_velocity_field();
    auto a = source(), b = source();
    check(velocity::ensure_velocity_sample(f, a) == velocity::ensure_velocity_sample(f, a), "ensure returns the same sample per source");
    check(velocity::ensure_velocity_sample(f, a) != velocity::ensure_velocity_sample(f, b), "distinct sources have distinct samples");
  }
  {
    auto f = velocity::create_velocity_field();
    auto a = source();
    velocity::contribute_velocity(f, a, 3, -4);
    check(equals(velocity::get_velocity(f, a, v(0, 0)), 3, -4), "an explicit contribution is readable this frame");
    velocity::begin_velocity_frame(f);
    check(equals(velocity::get_velocity(f, a, v(9, 9)), 0, 0), "a stale sample reads zero");
  }
  {
    auto f = velocity::create_velocity_field();
    auto a = source();
    velocity::contribute_velocity(f, a, 9, 9);
    velocity::suppress_velocity(f, a);
    check(equals(velocity::get_velocity(f, a, v(1, 1)), 0, 0), "suppress zeroes a contribution");
  }
  {
    auto f = velocity::create_velocity_field();
    auto moving = source(), still = source();
    velocity::contribute_velocity(f, moving, 1, 0);
    velocity::suppress_velocity(f, still);
    check(velocity::has_velocity(f, moving) && !velocity::has_velocity(f, still), "hasVelocity is true when moving, false when suppressed");
  }
}

void explanation() {
  const auto reason = [](const auto& result, const char* expected) { return result->reason == flight::String(expected); };
  {
    auto f = velocity::create_velocity_field();
    check(reason(velocity::explain_velocity(f, source()), "no-sample"), "explain: no-sample");
  }
  {
    auto f = velocity::create_velocity_field();
    auto a = source();
    velocity::contribute_velocity(f, a, 5, 5);
    velocity::begin_velocity_frame(f);
    check(reason(velocity::explain_velocity(f, a), "stale"), "explain: stale");
  }
  {
    auto f = velocity::create_velocity_field();
    auto a = source();
    velocity::suppress_velocity(f, a);
    auto result = velocity::explain_velocity(f, a);
    check(reason(result, "explicit-zero") && result->explicit_, "explain: explicit-zero");
  }
  {
    auto f = velocity::create_velocity_field();
    auto a = source();
    auto sample = velocity::ensure_velocity_sample(f, a);
    sample->velocity->x = 0;
    sample->velocity->y = 0;
    sample->last_frame_id = f->frame_id;
    auto result = velocity::explain_velocity(f, a);
    check(reason(result, "derived-zero") && !result->explicit_, "explain: derived-zero");
  }
  {
    auto f = velocity::create_velocity_field();
    auto a = source();
    velocity::contribute_velocity(f, a, 3, -4);
    auto result = velocity::explain_velocity(f, a);
    check(reason(result, "ok") && result->x == 3 && result->y == -4 && result->explicit_, "explain: ok, explicit");
  }
  {
    auto f = velocity::create_velocity_field();
    auto a = source();
    auto sample = velocity::ensure_velocity_sample(f, a);
    sample->velocity->x = 7;
    sample->velocity->y = -2;
    sample->last_frame_id = f->frame_id;
    auto result = velocity::explain_velocity(f, a);
    check(reason(result, "ok") && result->x == 7 && result->y == -2 && !result->explicit_, "explain: ok, derived");
  }
}

void reprojection() {
  using flight::geometry::create_matrix;
  const auto at = [](std::optional<flight::Ref<flight::types::Matrix>> previous, const flight::Ref<flight::types::Matrix>& current,
                     double x, double y) {
    auto f = velocity::create_velocity_field();
    auto sample = velocity::ensure_velocity_sample(f, source());
    sample->previous_world_transform = previous;
    auto out = v(99, 99);
    velocity::get_velocity_sample_at(view(sample), view(current), x, y, out);
    return out;
  };
  check(equals(at(std::nullopt, create_matrix(), 0, 0), 0, 0), "no previous transform: zero");
  check(equals(at(create_matrix(1, 0, 0, 1, 3, 4), create_matrix(1, 0, 0, 1, 8, 9), 0, 0), 5, 5), "translation delta at the origin");
  check(equals(at(create_matrix(), create_matrix(1, 0, 0, 1, 2, 3), 1, 0), 2, 3), "affine reprojection off the origin");
  auto rotated = at(create_matrix(), create_matrix(0, 1, -1, 0, 0, 0), 1, 0);
  check(close(rotated->x, -1) && close(rotated->y, 1), "a rotating transform off the origin");
  check(equals(at(create_matrix(1, 0, 0, 1, 1, 2), create_matrix(1, 0, 0, 1, 4, 6), 0, 0), 3, 4), "a translated pair");
  // Not in upstream: a rotated PREVIOUS transform. (1, 0) was at (0, 1) under a quarter turn and is at (1, 0)
  // under the identity, so the point moved by (1, -1); every term is an exact small integer.
  check(equals(at(create_matrix(0, 1, -1, 0, 0, 0), create_matrix(), 1, 0), 1, -1), "a rotated previous transform reprojects too");

  auto f = velocity::create_velocity_field();
  auto a = source();
  auto sample = velocity::ensure_velocity_sample(f, a);
  sample->previous_world_transform = create_matrix(1, 0, 0, 1, 0, 0);
  velocity::contribute_velocity(f, a, 0, 0);
  velocity::suppress_velocity(f, a);
  auto out = v(0, 0);
  velocity::get_velocity_sample_at(view(sample), view(create_matrix(1, 0, 0, 1, 7, 3)), 0, 0, out);
  check(equals(out, 7, 3), "reprojection is independent of contribute and suppress");
}

}  // namespace

int main() {
  arithmetic();
  magnitude();
  blending();
  field();
  explanation();
  reprojection();
  return failures == 0 ? 0 : 1;
}
