#include <flight/presence.hpp>

#include <iostream>
#include <memory>
#include <optional>

// `flight::nullish_or` is `left ?? fallback` for a left whose storage is only known at template
// instantiation. Node: undefined ?? f() and null ?? f() call f; an object is returned itself without
// calling f; 0 ?? f() is 0 -- `??` tests nullishness, not truthiness.
namespace {

int failures = 0;

void check(bool condition, const char* message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

struct Group {
  int id = 0;
};

} // namespace

int main() {
  int calls = 0;
  const auto fallback = [&] {
    ++calls;
    return std::make_shared<Group>(Group{.id = 7});
  };
  using Ref = std::shared_ptr<Group>;

  const std::optional<Ref> empty;
  check(flight::nullish_or<Ref>(empty, fallback)->id == 7 && calls == 1, "an empty optional falls back");

  const auto present = std::make_shared<Group>(Group{.id = 1});
  const std::optional<Ref> held = present;
  check(flight::nullish_or<Ref>(held, fallback).get() == present.get() && calls == 1,
        "a present optional is returned itself and the fallback is not evaluated");

  const Ref null_reference;
  check(flight::nullish_or<Ref>(null_reference, fallback)->id == 7 && calls == 2, "a null reference falls back");
  check(flight::nullish_or<Ref>(present, fallback).get() == present.get() && calls == 2,
        "a present reference is the very object and the fallback is not evaluated");

  int number_calls = 0;
  check(flight::nullish_or<double>(0.0, [&] { ++number_calls; return 5.0; }) == 0.0 && number_calls == 0,
        "a non-nullish falsy value is kept, as ?? keeps it");
  const std::optional<double> no_number;
  check(flight::nullish_or<double>(no_number, [&] { ++number_calls; return 5.0; }) == 5.0 && number_calls == 1,
        "an empty optional number falls back");

  if (failures == 0) std::cout << "nullish_or matches ?? semantics\n";
  return failures == 0 ? 0 : 1;
}
