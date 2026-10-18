#include <flight/runtime.hpp>

#include <iostream>
#include <optional>

struct OptionalReferenceObject : flight::ReferenceEnabled { double value = 0; };
struct OtherReferenceObject : flight::ReferenceEnabled {};

int main() {
  int checks = 0;
  int failures = 0;
  const auto check = [&](bool ok) { ++checks; if (!ok) ++failures; };
  auto original = flight::make_ref<OptionalReferenceObject>();
  original->value = 3;
  std::optional<flight::Ref<OptionalReferenceObject>> slot = original;
  const flight::Any boxed(slot);
  check(boxed.kind() == flight::AnyKind::object);
  check(boxed.identity() == original.get());
  check(boxed.object_if<OptionalReferenceObject>() == original);
  check(!boxed.object_if<OtherReferenceObject>());
  original->value = 4;
  check(boxed.object_if<OptionalReferenceObject>()->value == 4);
  boxed.object_if<OptionalReferenceObject>()->value = 5;
  check(original->value == 5);
  slot = std::nullopt;
  check(flight::Any(slot).kind() == flight::AnyKind::undefined);
  check(boxed.object_if<OptionalReferenceObject>()->value == 5);
  std::weak_ptr<OptionalReferenceObject> lifetime = original;
  original.reset();
  check(!lifetime.expired());
  check(boxed.object_if<OptionalReferenceObject>()->value == 5);
  slot = flight::Ref<OptionalReferenceObject>{};
  check(slot.has_value() && flight::Any(slot).kind() == flight::AnyKind::null);
  check(flight::Any(std::optional<double>{0}).as_number() == 0);
  check(flight::Any(std::optional<bool>{false}).as_boolean() == false);
  check(flight::Any(std::optional<flight::String>{flight::String("kept")}).as_string() == flight::String("kept"));
  check(flight::Any(std::optional<double>{}).kind() == flight::AnyKind::undefined);
  std::cout << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
