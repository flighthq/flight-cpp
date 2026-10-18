#include <flight/callable.hpp>

#include <iostream>
#include <memory>

namespace {
int failures = 0;
int calls = 0;
const auto sentinel = std::make_shared<int>(42);
int first(int value) { ++calls; if (value < 0) throw sentinel; return value + 1; }
int second(int value) { ++calls; if (value < 0) throw sentinel; return value + 1; }
void check(bool condition, const char* message) {
  if (!condition) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}
}

int main() {
  using Native = int(int);
  using Contextual = int(int, bool);
  const auto original = flight::NamedFunction<Native>::from<&first>(first);
  const auto adapted = flight::NamedFunction<Contextual>::from<&first>(
      [](int value, bool) { return first(value); });
  const auto different = flight::NamedFunction<Contextual>::from<&second>(
      [](int value, bool) { return second(value); });
  const std::function<Native> native = original;
  const std::function<Contextual> slot = adapted;
  const std::function<Contextual> other = different;
  check(flight::callable_identity_equal(native, slot) && flight::callable_identity_equal(slot, native),
        "same named declaration survives different callback signatures");
  check(!flight::callable_identity_equal(native, other) && !flight::callable_identity_equal(slot, other),
        "distinct declarations with identical bodies remain distinct");
  const auto copy = slot;
  const std::function<Contextual> independent = flight::NamedFunction<Contextual>::from<&first>(
      [](int value, bool) { return first(value); });
  check(flight::callable_identity_equal(slot, copy) && flight::callable_identity_equal(slot, independent),
        "copies and repeated contextual stores retain original declaration identity");
  check(flight::callable_identity_equal_named<&first>(slot) &&
        !flight::callable_identity_equal_named<&second>(slot), "bare source declaration comparison");
  const std::function<Native> raw = first;
  check(flight::callable_identity_equal_named<&first>(raw) &&
        !flight::callable_identity_equal_named<&second>(raw), "native pointer compatibility without casts");
  constexpr auto alias = &first;
  check(flight::NamedFunctionOrigin::of<alias>() == original.origin(), "source alias keeps declaration token");
  calls = 0;
  check(copy(2, false) == 3 && native(3) == 4 && other(4, true) == 5 && calls == 3,
        "each callback invokes its original body exactly once");
  bool same_error = false;
  try { (void)slot(-1, true); }
  catch (const std::shared_ptr<int>& error) { same_error = error == sentinel; }
  check(same_error && calls == 4, "adapted call preserves exact exception owner and effect count");
  const std::function<Contextual> anonymous = flight::Function<Contextual>{[](int value, bool) { return value + 1; }};
  check(!flight::callable_identity_equal(slot, anonymous) &&
        !flight::callable_identity_equal(anonymous, slot), "represented anonymous closure has distinct identity");
  const std::function<Contextual> erased = [](int value, bool) { return value + 1; };
  bool rejected = false;
  try { (void)flight::callable_identity_equal(native, erased); }
  catch (const std::logic_error&) { rejected = true; }
  check(rejected, "unknown cross-signature closure identity stays unavailable");
  return failures == 0 ? 0 : 1;
}
