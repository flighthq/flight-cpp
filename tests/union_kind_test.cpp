// Focused tests for the shared carrier-kind mechanism: `flight::union_kind_if` and its throwing twin.
//
// The emitter stores a union alternative a source spelled `unknown` -- or one it can only erase -- as the
// erased carrier BESIDE the named ones, so `x instanceof ArrayBuffer` may be true for a value sitting in
// its own alternative OR inside an `Any` the host boxed. The guard and the narrowed projection must agree
// about which values answer, or a guard can admit a value the projection cannot produce. One function
// answers both, and this file pins every arm it must recognize, every arm it must refuse silently, and the
// identity it must preserve.
//
// Measured sites: `@flighthq/audio`'s `audioResourceFrom.ts` and `@flighthq/loader`'s `load.ts`, whose
// `NetResponseBody` is `string | unknown | ArrayBuffer | Blob | null`. The mechanism is filed and
// described in docs/compiler-requests/property-union-evidence-lost.md.
#include <flight/any.hpp>
#include <flight/array_buffer.hpp>
#include <flight/optional_variant.hpp>
#include <flight/reference.hpp>
#include <flight/string.hpp>

#include <cstdint>
#include <iostream>
#include <optional>
#include <stdexcept>
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

struct Subject : flight::ReferenceEnabled {
  int marker = 0;
};

// The measured carrier: a named alternative, a second named alternative, and the erased arm a source
// segment spelled `unknown` becomes.
using Body = std::optional<std::variant<flight::ArrayBuffer, flight::String, flight::Any>>;

flight::ArrayBuffer buffer_of(std::size_t length) { return flight::ArrayBuffer::allocate(length); }

void test_explicit_alternative() {
  Body body = std::variant<flight::ArrayBuffer, flight::String, flight::Any>{buffer_of(4)};
  const flight::ArrayBuffer* found = flight::union_kind_if<flight::ArrayBuffer>(body);
  check(found != nullptr, "an explicit alternative is found");
  check(found != nullptr && found->byte_length() == 4, "the found alternative is the one the carrier holds");

  // The pointer is INTO the carrier: writing through the carrier is visible through the answer, which a
  // copy could not show.
  const flight::ArrayBuffer* again = flight::union_kind_if<flight::ArrayBuffer>(body);
  check(found == again, "the answer is a view of the carrier, not a fresh object");
}

void test_boxed_in_erased_arm() {
  flight::ArrayBuffer original = buffer_of(7);
  Body body = std::variant<flight::ArrayBuffer, flight::String, flight::Any>{
      flight::Any::external<flight::ArrayBuffer>(original)};
  const flight::ArrayBuffer* found = flight::union_kind_if<flight::ArrayBuffer>(body);
  check(found != nullptr, "an ArrayBuffer boxed in the erased arm is found");
  check(found != nullptr && found->byte_length() == 7, "the boxed buffer keeps its own bytes");
  check(found != nullptr && std::get<flight::Any>(body.value()).external_if<flight::ArrayBuffer>() == found,
        "the answer is the box that was there, not a second one");
}

void test_refuses_every_other_arm_without_throwing() {
  Body string_arm = std::variant<flight::ArrayBuffer, flight::String, flight::Any>{flight::String("text")};
  check(flight::union_kind_if<flight::ArrayBuffer>(string_arm) == nullptr,
        "a string alternative answers null rather than throwing");

  Body absent = std::nullopt;
  check(flight::union_kind_if<flight::ArrayBuffer>(absent) == nullptr,
        "an absent carrier answers null");

  Body other_primitive = std::variant<flight::ArrayBuffer, flight::String, flight::Any>{flight::Any(true)};
  check(flight::union_kind_if<flight::ArrayBuffer>(other_primitive) == nullptr,
        "an Any holding a primitive answers null");

  Body other_string = std::variant<flight::ArrayBuffer, flight::String, flight::Any>{
      flight::Any(flight::String("boxed text"))};
  check(flight::union_kind_if<flight::ArrayBuffer>(other_string) == nullptr,
        "an Any holding a string answers null");

  Body other_external = std::variant<flight::ArrayBuffer, flight::String, flight::Any>{
      flight::Any::external<flight::String>(flight::String("external"))};
  check(flight::union_kind_if<flight::ArrayBuffer>(other_external) == nullptr,
        "an Any boxing a different type answers null");

  flight::Ref<Subject> subject = flight::make_ref<Subject>();
  Body boxed_object = std::variant<flight::ArrayBuffer, flight::String, flight::Any>{flight::Any::object(subject)};
  check(flight::union_kind_if<flight::ArrayBuffer>(boxed_object) == nullptr,
        "an Any boxing an object answers null for a value-shaped target");
  check(subject->marker == 0, "asking leaves the boxed object alone");
}

void test_every_carrier_shape() {
  std::variant<flight::ArrayBuffer, flight::String> plain{buffer_of(2)};
  check(flight::union_kind_if<flight::ArrayBuffer>(plain) != nullptr, "a bare variant is answered");

  std::optional<flight::ArrayBuffer> single = buffer_of(3);
  check(flight::union_kind_if<flight::ArrayBuffer>(single) != nullptr, "an optional single is answered");

  std::optional<flight::ArrayBuffer> empty_single = std::nullopt;
  check(flight::union_kind_if<flight::ArrayBuffer>(empty_single) == nullptr, "an empty optional answers null");

  flight::Any erased = flight::Any::external<flight::ArrayBuffer>(buffer_of(5));
  check(flight::union_kind_if<flight::ArrayBuffer>(erased) != nullptr, "a bare erased carrier is answered");

  flight::ArrayBuffer direct = buffer_of(1);
  check(flight::union_kind_if<flight::ArrayBuffer>(direct) == &direct, "a carrier that IS the target answers itself");
}

void test_identity_is_preserved() {
  flight::Ref<Subject> subject = flight::make_ref<Subject>();
  subject->marker = 41;
  std::optional<std::variant<flight::Ref<Subject>, flight::String>> carried{subject};

  const flight::Ref<Subject>* found = flight::union_kind_if<flight::Ref<Subject>>(carried);
  check(found != nullptr, "a reference alternative is found");
  check(found != nullptr && found->get() == subject.get(), "the reference is the same object, not a second one");
  check(found != nullptr && (*found)->marker == 41, "the object behind the reference is the original");
}

void test_throwing_twin() {
  Body present = std::variant<flight::ArrayBuffer, flight::String, flight::Any>{buffer_of(9)};
  const flight::ArrayBuffer& got = flight::union_kind_or_throw<flight::ArrayBuffer>(present);
  check(got.byte_length() == 9, "the throwing twin returns the value the guard proved");

  Body boxed = std::variant<flight::ArrayBuffer, flight::String, flight::Any>{
      flight::Any::external<flight::ArrayBuffer>(buffer_of(11))};
  check(flight::union_kind_or_throw<flight::ArrayBuffer>(boxed).byte_length() == 11,
        "the throwing twin reaches the boxed arm the guard admits");

  // Divergence -- a projection emitted without the guard that proves it -- is a compiler defect with no
  // TypeScript shape, so it is a logic error and NOT a `flight::Error` a source `catch` could take for an
  // application failure.
  bool threw = false;
  bool threw_logic_error = false;
  try {
    const flight::ArrayBuffer& missing =
        flight::union_kind_or_throw<flight::ArrayBuffer>(Body{flight::String("text")});
    static_cast<void>(missing);
  } catch (const std::logic_error&) {
    threw = true;
    threw_logic_error = true;
  } catch (const flight::Error&) {
    threw = true;
  } catch (...) {
    threw = true;
  }
  check(threw, "a divergence fails loudly rather than inventing a value");
  check(threw_logic_error, "the failure is a std::logic_error, not a flight::Error");
}

}  // namespace

int main() {
  test_explicit_alternative();
  test_boxed_in_erased_arm();
  test_refuses_every_other_arm_without_throwing();
  test_every_carrier_shape();
  test_identity_is_preserved();
  test_throwing_twin();
  if (failures != 0) {
    std::cerr << failures << " union kind check(s) failed\n";
    return 1;
  }
  std::cout << "union kind: all checks passed\n";
  return 0;
}
