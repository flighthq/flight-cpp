#include <flight/path_boolean/martinez_kernel.hpp>

#include <iostream>
#include <sstream>
#include <string>

// Focused native coverage for packages/path-boolean/src/martinezKernel.ts at a627849. The upstream
// suite covers 88 cases across nine files: operation dispatch and aliases, Martinez degeneracies and
// fill rules, offset joins/caps, simplification and region resolution, contour writing, and seeded
// invariants. This file concentrates on the lower-level behavior that proves the carried DirectedGraph
// override faithful: exact coordinates and ring order, sweep ordering, reference equality, value
// independence, all operation/fill-rule branches, and the source's explicit malformed-event failure.
//
// MUTATION SENSITIVITY, measured 2026-10-13: reversing the clockwise angular gap fails the shared-
// vertex branch-order check; changing XOR to equality fails all four truth-table checks and the exact
// XOR contours; changing the unpaired-event message fails the exact-error check. Each mutation was
// applied separately to the override, rebuilt and observed failing, then restored.

namespace {

namespace pb = flight::path_boolean;
using Contour = flight::types::PathBooleanContour;

int failures = 0;

void check(bool condition, const std::string& message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

bool same(const Contour& actual, const Contour& expected) {
  if (actual.size() != expected.size()) return false;
  for (std::size_t i = 0; i < actual.size(); ++i) {
    if (actual.element(i) != expected.element(i)) return false;
  }
  return true;
}

std::string show(const flight::Array<Contour>& rings) {
  std::ostringstream out;
  out << '[';
  for (std::size_t r = 0; r < rings.size(); ++r) {
    if (r != 0) out << ", ";
    out << '[';
    const auto ring = rings.element(r);
    for (std::size_t i = 0; i < ring.size(); ++i) {
      if (i != 0) out << ',';
      out << ring.element(i);
    }
    out << ']';
  }
  out << ']';
  return out.str();
}

void check_rings(const flight::Array<Contour>& actual, const flight::Array<Contour>& expected,
                 const std::string& message) {
  bool equal = actual.size() == expected.size();
  for (std::size_t i = 0; equal && i < actual.size(); ++i) equal = same(actual.element(i), expected.element(i));
  check(equal, message + "; actual " + show(actual));
}

Contour square(double x, double y, double size) {
  return Contour{x, y, x + size, y, x + size, y + size, x, y + size};
}

flight::Ref<pb::SweepEvent> event(double x, double y, bool subject = true) {
  return pb::create_event(x, y, subject);
}

void link(const flight::Ref<pb::SweepEvent>& left, const flight::Ref<pb::SweepEvent>& right) {
  left->left = true;
  right->left = false;
  left->other_event = right;
  right->other_event = left;
}

void test_scalar_rules() {
  check(pb::point_order(0, 5, 1, -4) == -1, "point order compares x before y");
  check(pb::point_order(1, -4, 1, 7) == -1, "point order compares y when x ties");
  check(pb::point_order(2, 3, 2, 3) == 0, "point order reports equal coordinates");

  for (const bool subject : {false, true}) {
    for (const bool clip : {false, true}) {
      check(pb::combine(flight::String("union"), subject, clip) == (subject || clip),
            "union truth table is exact");
      check(pb::combine(flight::String("intersection"), subject, clip) == (subject && clip),
            "intersection truth table is exact");
      check(pb::combine(flight::String("difference"), subject, clip) == (subject && !clip),
            "difference truth table is exact");
      check(pb::combine(flight::String("xor"), subject, clip) == (subject != clip),
            "xor truth table is exact");
    }
  }

  check(pb::is_inside(2, flight::String("nonZero")), "non-zero fill retains winding two");
  check(!pb::is_inside(2, flight::String("evenOdd")), "even-odd fill removes winding two");
  check(pb::is_inside(1, flight::String("positive")), "positive fill retains positive winding");
  check(!pb::is_inside(-1, flight::String("positive")), "positive fill rejects negative winding");
  check(pb::is_inside(-1, flight::String("negative")), "negative fill retains negative winding");
  check(!pb::is_inside(1, flight::String("negative")), "negative fill rejects positive winding");
}

void test_intersections_and_graph_order() {
  const auto overlap = pb::segment_intersection(0, 0, 3, 0, 1, 0, 2, 0);
  check(overlap.has_value(), "coincident overlap has an intersection result");
  if (overlap.has_value()) {
    check_rings(overlap.value(), flight::Array<Contour>{Contour{1, 0}, Contour{2, 0}},
                "coincident overlap endpoints keep exact source order");
  }

  const flight::Array<flight::Array<double>> edges{
      {-1, 0, 0, 0}, {0, 0, 0, 1}, {0, 1, -1, 0}, {0, 0, 0, -1}, {0, -1, -1, 0},
  };
  check_rings(pb::trace_rings(edges),
              flight::Array<Contour>{Contour{-1, 0, 0, 0, 0, 1}},
              "clockwise next-edge selection chooses the exact ordered branch at a shared vertex");
}

void test_sweep_order_and_identity() {
  const auto high = event(0, 1);
  const auto high_right = event(2, 1);
  link(high, high_right);
  const auto low = event(0, 0);
  const auto low_right = event(2, 0);
  link(low, low_right);

  pb::EventHeap heap;
  heap.push(high);
  heap.push(low);
  check(heap.pop() == low, "event heap orders the lower y first when x ties");
  check(heap.pop() == high, "event heap retains the remaining reference identity");

  const auto subject = event(4, 4, true);
  const auto subject_right = event(5, 4, true);
  link(subject, subject_right);
  const auto clip = event(4, 4, false);
  const auto clip_right = event(5, 4, false);
  link(clip, clip_right);
  check(pb::compare_events(subject, clip) < 0, "subject events precede clip events on the same segment");

  const auto incoming_right = event(7, 0);
  const auto incoming_left = event(6, 0);
  link(incoming_left, incoming_right);
  const auto outgoing_left = event(7, 0);
  const auto outgoing_right = event(8, 0);
  link(outgoing_left, outgoing_right);
  check(pb::compare_events(incoming_right, outgoing_left) < 0,
        "right endpoints precede left endpoints at an equal coordinate");

  flight::Array<flight::Ref<pb::SweepEvent>> status{subject};
  const auto equal_value = event(subject->x, subject->y, subject->is_subject);
  equal_value->left = subject->left;
  equal_value->other_event = subject->other_event;
  check(pb::find_status(status, subject) == 0, "status lookup finds the same event reference");
  check(pb::find_status(status, equal_value) == -1, "status lookup does not equate a distinct equal-valued event");
}

void test_exact_boolean_geometry_and_value_independence() {
  const Contour subject = square(0, 0, 2);
  const Contour clip = square(1, 0, 2);
  const Contour subject_before = subject.slice();
  const Contour clip_before = clip.slice();

  auto united = pb::compute_martinez_boolean({subject}, {clip}, flight::String("union"), flight::String("nonZero"));
  check_rings(united,
              flight::Array<Contour>{Contour{0, 2, 0, 0, 1, 0, 2, 0, 3, 0, 3, 2, 2, 2, 1, 2}},
              "overlapping square union has exact deterministic contour geometry");
  check_rings(pb::compute_martinez_boolean({subject}, {clip}, flight::String("intersection"),
                                           flight::String("nonZero")),
              flight::Array<Contour>{Contour{1, 2, 1, 0, 2, 0, 2, 2}},
              "overlapping square intersection has exact deterministic contour geometry");
  check_rings(pb::compute_martinez_boolean({subject}, {clip}, flight::String("difference"),
                                           flight::String("nonZero")),
              flight::Array<Contour>{Contour{0, 2, 0, 0, 1, 0, 1, 2}},
              "overlapping square difference has exact deterministic contour geometry");
  check_rings(pb::compute_martinez_boolean({subject}, {clip}, flight::String("xor"),
                                           flight::String("nonZero")),
              flight::Array<Contour>{Contour{0, 2, 0, 0, 1, 0, 1, 2},
                                     Contour{2, 2, 2, 0, 3, 0, 3, 2}},
              "overlapping square xor has exact deterministic ring and contour order");

  check(same(subject, subject_before) && same(clip, clip_before), "boolean evaluation leaves both inputs unchanged");
  if (!united.empty()) united.element(0).element(0) = 99;
  check(same(subject, subject_before) && same(clip, clip_before),
        "mutating a result contour cannot mutate either input contour");
}

void test_failure_contract() {
  bool exact_error = false;
  try {
    static_cast<void>(pb::get_other_event(event(0, 0)));
  } catch (const flight::Error& error) {
    exact_error = error.message() ==
                  flight::String("Sweep event entered the Martinez queue before its endpoint pair was linked");
  }
  check(exact_error, "an unpaired sweep event throws the exact source error");
}

}  // namespace

int main() {
  test_scalar_rules();
  test_intersections_and_graph_order();
  test_sweep_order_and_identity();
  test_exact_boolean_geometry_and_value_independence();
  test_failure_contract();
  return failures == 0 ? 0 : 1;
}
