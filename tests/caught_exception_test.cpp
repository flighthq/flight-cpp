// Focused tests for the owned caught-value carrier: `flight::caught_exception`.
//
// A catch body that awaits is hoisted out of its handler, and a body that reads its binding cannot be hoisted
// as a flag alone -- the handler's `const std::exception&` is gone by the time the body runs. The handler
// records an OWNED `std::exception_ptr`, which keeps the thrown object alive across the suspension, and the
// hoisted body re-binds that object from it. What this file pins is that the re-binding is the OBJECT and not
// a copy: the same address the handler saw, surviving a rethrow and a task rejection, with the subclass still
// the subclass. The emitter's side is pinned in
// packages/compiler-backend-cpp/src/cppCatchAwaitCarrier.test.ts.
//
// Measured site: `@flighthq/loader`'s `resourceLoader.ts` `runEntry`, which retries after
// `await delay(delayMs)` and passes the caught `error` to `entry.reject(error)`.
#include <flight/error.hpp>
#include <flight/rejection.hpp>

#include <exception>
#include <iostream>
#include <stdexcept>

namespace {

int failures = 0;

bool check(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
  return condition;
}

// What the handler records: the pointer it captured, and the address of the object it caught.
struct Caught {
  std::exception_ptr pointer;
  const void* address = nullptr;
};

Caught catch_rang_error() {
  Caught caught;
  try {
    throw flight::RangeError(flight::String("out of range"));
  } catch (const std::exception& error) {
    caught.pointer = std::current_exception();
    caught.address = &error;
  }
  return caught;
}

void test_rebinding_is_the_same_object() {
  const Caught caught = catch_rang_error();
  const std::exception& rebound = flight::caught_exception(caught.pointer);
  check(&rebound == caught.address, "the re-bound value is the object the handler caught, not a copy");
}

void test_rebinding_is_stable_across_calls() {
  const Caught caught = catch_rang_error();
  const std::exception& first = flight::caught_exception(caught.pointer);
  const std::exception& second = flight::caught_exception(caught.pointer);
  check(&first == &second, "asking twice gives the same object, as a second catch would");
}

void test_subclass_survives_the_carrier() {
  const Caught caught = catch_rang_error();
  const std::exception& rebound = flight::caught_exception(caught.pointer);
  const auto* range_error = dynamic_cast<const flight::RangeError*>(&rebound);
  check(range_error != nullptr, "the subclass is still the subclass after the carrier");
  check(range_error != nullptr && range_error->name() == flight::String("RangeError"),
        "the re-bound object answers its own name");
}

void test_rethrow_keeps_the_object() {
  const Caught caught = catch_rang_error();
  const void* again = nullptr;
  try {
    std::rethrow_exception(caught.pointer);
  } catch (const std::exception& error) {
    again = &error;
  }
  check(again == caught.address, "a rethrow of the carrier delivers the object, not a copy");
}

void test_settlement_carries_the_object() {
  const Caught caught = catch_rang_error();
  // A rejection is how a task settlement carries a failure out of the frame that caught it, which is the
  // path `resourceLoader`'s `entry.reject(error)` takes.
  const flight::Rejection rejection = flight::Rejection::from_exception(caught.pointer);
  check(rejection.has_exception(), "the rejection carries the caught exception");
  const void* settled = nullptr;
  try {
    rejection.rethrow();
  } catch (const std::exception& error) {
    settled = &error;
  }
  check(settled == caught.address, "the settlement delivers the same object the handler caught");
}

void test_absent_carrier_fails_loudly() {
  bool threw = false;
  bool threw_logic_error = false;
  bool threw_flight_error = false;
  try {
    static_cast<void>(flight::caught_exception(std::exception_ptr{}));
  } catch (const flight::Error&) {
    threw = true;
    threw_flight_error = true;
  } catch (const std::logic_error&) {
    threw = true;
    threw_logic_error = true;
  } catch (...) {
    threw = true;
  }
  check(threw, "an absent carrier fails rather than inventing an exception");
  // A compiler defect, not a source-observable failure: a `catch` in the source must not be able to take
  // this for an application error.
  check(threw_logic_error && !threw_flight_error, "the failure is a std::logic_error, not a flight::Error");
}

}  // namespace

int main() {
  test_rebinding_is_the_same_object();
  test_rebinding_is_stable_across_calls();
  test_subclass_survives_the_carrier();
  test_rethrow_keeps_the_object();
  test_settlement_carries_the_object();
  test_absent_carrier_fails_loudly();
  if (failures != 0) {
    std::cerr << failures << " caught-exception check(s) failed\n";
    return 1;
  }
  std::cout << "caught exception: all checks passed\n";
  return 0;
}
