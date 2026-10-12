// Focused tests for identity in a union of reference-backed subjects.
//
// The emitter builds `flight::Set<std::variant<ViewA, ViewB>>` when one call takes two capability slots
// (`@flighthq/mediasession`'s `destroyMediaSession` is the measured one). The source's `new Set([a, b])`
// holds the two *objects* and counts one subject once even when a caller passes it through both slots; the
// C++ carrier holds two views, and `std::variant`'s `==` compares the index before the value, so the same
// subject through two arms was counted twice -- a double release of whatever the subject owns. That is
// filed as docs/compiler-requests/union-set-identity-across-alternatives.md.
//
// `flight::SameValueZero` now answers identity across the arms of a variant whose active alternatives are
// reference-backed carriers, and leaves every other variant to `==`. This file pins that rule, its three
// required cases, and the boundaries it deliberately does not cross.
#include <flight/equality.hpp>
#include <flight/set.hpp>
#include <flight/structural_ref.hpp>

#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <string>
#include <variant>

namespace {

int failures = 0;

bool check(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
  return condition;
}

// A subject that satisfies two capability-shaped interfaces, and the two row schemas a caller would hand
// over for them. The schemas are unrelated types, which is exactly the emitted shape: the row owner is
// what remembers the object, not inheritance.
struct Both : flight::ReferenceEnabled {
  std::function<void()> destroy;
  flight::String label;
};
struct Other : flight::ReferenceEnabled {
  flight::String label;
};

using RowOfBoth = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<Both>>>>;
using RowOfOther = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<Other>>>>;
using Union = std::variant<RowOfBoth, RowOfOther>;

RowOfOther other_arm(const flight::Ref<Both>& subject) {
  const RowOfBoth first(subject);
  return flight::structural_ref_cast<RowOfOther>(first);
}

}  // namespace

int main() {
  const auto subject = flight::make_ref<Both>();
  subject->destroy = []() {};
  const auto second_subject = flight::make_ref<Both>();

  const RowOfBoth subject_row(subject);
  const RowOfOther other_view = other_arm(subject);
  const RowOfBoth other_subject_row(second_subject);

  // The two views of one subject really do share the subject: the cast keeps the live owner rather than
  // materializing a second one. Without this the rule below would be answering about two objects.
  check(subject_row.identity() == other_view.identity(),
        "a structural cast between schemas keeps the subject's owner");

  // same subject, cross-arm: equal by the rule, unequal by the carrier's own comparison -- which is the
  // defect, and the reason the rule exists.
  {
    const Union left{std::in_place_type<RowOfBoth>, subject_row};
    const Union right{std::in_place_type<RowOfOther>, other_view};
    check(flight::same_value_zero(left, right),
          "one subject through two arms is one subject");
    check(!(left == right),
          "the variant's own comparison still answers index-then-value -- the rule is what answers");
  }

  // same subject, same arm
  {
    const Union left{std::in_place_type<RowOfBoth>, subject_row};
    const Union right{std::in_place_type<RowOfBoth>, subject_row};
    check(flight::same_value_zero(left, right), "one subject through one arm is one subject");
  }

  // distinct subjects, same arm, and across arms
  {
    const Union left{std::in_place_type<RowOfBoth>, subject_row};
    const Union right{std::in_place_type<RowOfBoth>, other_subject_row};
    check(!flight::same_value_zero(left, right), "two subjects are two subjects");
    const Union crossed{std::in_place_type<RowOfOther>, other_arm(second_subject)};
    check(!flight::same_value_zero(left, crossed),
          "and two subjects are still two when one of them arrives through the other arm");
  }

  // Ordinary variant and value equality is untouched.
  {
    using Values = std::variant<double, flight::String>;
    check(flight::same_value_zero(Values{1.0}, Values{1.0}), "equal values stay equal");
    check(!flight::same_value_zero(Values{1.0}, Values{flight::String("1")}),
          "the index still separates alternatives");
    check(!flight::same_value_zero(Values{1.0}, Values{2.0}), "and unequal values stay unequal");
    const double nan = std::numeric_limits<double>::quiet_NaN();
    check(flight::same_value_zero(Values{nan}, Values{nan}), "SameValueZero stays NaN-matched");
  }

  // The Set itself: what the emitted module actually builds.
  {
    flight::Set<Union> subjects{};
    subjects.add(Union{std::in_place_type<RowOfBoth>, subject_row});
    subjects.add(Union{std::in_place_type<RowOfOther>, other_view});
    check(subjects.size() == 1, "a Set counts one subject once, however it arrived");
    check(subjects.has(Union{std::in_place_type<RowOfOther>, other_view}),
          "and a lookup through the other arm finds it");

    subjects.add(Union{std::in_place_type<RowOfBoth>, other_subject_row});
    check(subjects.size() == 2, "a second subject is a second member");

    flight::Array<Union> listed = flight::Array<Union>{};
    for (const auto& member : subjects) listed.push(member);
    check(listed.size() == 2.0, "iteration sees both members");
    check(std::get<RowOfBoth>(listed.element(0.0)).identity() == subject_row.identity(),
          "in insertion order, and the member keeps the arm it was first inserted with");
  }

  // Array::includes goes through the same comparison.
  {
    flight::Array<Union> listed = flight::Array<Union>{};
    listed.push(Union{std::in_place_type<RowOfBoth>, subject_row});
    check(listed.includes(Union{std::in_place_type<RowOfOther>, other_view}),
          "includes answers over the subject, not the arm");
  }

  // THE BOUNDARY, deliberately not crossed. Two carriers of *different kinds* for one subject -- a shared
  // reference and a row view -- answer with addresses of different shapes (the object, the row owner), so
  // the rule leaves them to `==` and they are two members here. Emitted unions of this shape carry two row
  // views, so the mixed pair does not arise from the emitter today; if a union ever mixes a `Ref` arm with
  // a row arm, this is where it would be counted twice, and that is a carrier decision rather than an
  // equality one.
  {
    using Mixed = std::variant<flight::Ref<Both>, RowOfBoth>;
    check(!flight::same_value_zero(Mixed{subject}, Mixed{RowOfBoth(subject)}),
          "a shared reference and a row view of one subject are different carriers, not one member");
  }

  if (failures == 0) std::cout << "variant set identity: all checks passed\n";
  return failures == 0 ? 0 : 1;
}
