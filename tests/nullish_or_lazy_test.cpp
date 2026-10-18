// flight::nullish_or (flight/presence.hpp) in the forms the generic `??` lowering emits
// (compiler-lazy-nullish-fallback): an explicit Target that is the left's payload, or that payload's optional
// when the fallback can itself be absent, and a thunk for the right operand.
//
// The source semantics are the contract: the left operand is evaluated once; the right operand is evaluated
// only when the left is null or undefined, so its calls, getters and throws happen only then; a present left
// is the result whatever its value (false, 0, an empty string) and whatever its identity (the same Ref); and
// a fallback that can itself be absent keeps the result absent rather than inventing a value.
#include <flight/presence.hpp>
#include <flight/reference.hpp>
#include <flight/string.hpp>

#include <cstdio>
#include <optional>
#include <stdexcept>
#include <type_traits>

namespace {

struct Box : flight::ReferenceEnabled {
  double value{0.0};
};

int failures = 0;
void check(bool ok, const char* what) {
  if (!ok) {
    std::printf("FAIL %s\n", what);
    ++failures;
  }
}

} // namespace

int main() {
  int fallback_calls = 0;
  const auto counted = [&](double value) {
    return [&fallback_calls, value]() {
      ++fallback_calls;
      return value;
    };
  };

  // RHS only when nullish.
  check(flight::nullish_or<double>(std::optional<double>{2.0}, counted(9.0)) == 2.0 && fallback_calls == 0,
        "a present left never evaluates the fallback");
  check(flight::nullish_or<double>(std::optional<double>{}, counted(9.0)) == 9.0 && fallback_calls == 1,
        "an absent left evaluates the fallback exactly once");

  // Falsy present values are present.
  fallback_calls = 0;
  check(flight::nullish_or<double>(std::optional<double>{0.0}, counted(9.0)) == 0.0, "0 is not nullish");
  check(flight::nullish_or<bool>(std::optional<bool>{false}, [&]() { ++fallback_calls; return true; }) == false,
        "false is not nullish");
  check(flight::nullish_or<flight::String>(std::optional<flight::String>{flight::String("")},
                           [&]() { ++fallback_calls; return flight::String("x"); }) == flight::String(""),
        "an empty string is not nullish");
  check(fallback_calls == 0, "no falsy present value ran its fallback");

  // The left operand is evaluated exactly once, by the caller.
  int left_evaluations = 0;
  const auto left = [&]() {
    ++left_evaluations;
    return std::optional<double>{3.0};
  };
  (void)flight::nullish_or<double>(left(), counted(1.0));
  check(left_evaluations == 1, "the left operand is evaluated exactly once");

  // Identity: a present Ref is the same object.
  const auto box = flight::make_ref<Box>(Box{.value = 4.0});
  const auto same = flight::nullish_or<flight::Ref<Box>>(std::optional<flight::Ref<Box>>{box}, [&]() { return flight::make_ref<Box>(Box{}); });
  check(same == box, "a present reference is returned as the same object");

  // A throwing fallback throws only when it runs.
  const auto throwing = []() -> double { throw std::runtime_error("fallback"); };
  bool threw = false;
  check(flight::nullish_or<double>(std::optional<double>{5.0}, throwing) == 5.0, "a present left never reaches a throwing fallback");
  try {
    (void)flight::nullish_or<double>(std::optional<double>{}, throwing);
  } catch (const std::runtime_error&) {
    threw = true;
  }
  check(threw, "an absent left propagates the fallback's own exception");

  // A possibly-absent fallback keeps the result optional, never a fabricated value.
  const auto absent = flight::nullish_or<std::optional<double>>(std::optional<double>{}, []() { return std::optional<double>{}; });
  check(!absent.has_value(), "an absent left with an absent fallback stays absent");
  check(flight::nullish_or<std::optional<double>>(std::optional<double>{6.0}, []() { return std::optional<double>{}; }) == std::optional<double>{6.0},
        "an absent-capable fallback still yields a present left");

  std::printf("%s (%d failures)\n", failures ? "FAIL" : "nullish_or lazy forms ok", failures);
  return failures;
}
