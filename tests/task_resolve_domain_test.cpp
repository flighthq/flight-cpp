// Focused tests for the domain a promise settlement carries: `flight::resolve_task`.
//
// The runtime resolver deduces its task type from the value it is handed, so a literal narrower than the
// declaration would settle the narrower task. The emitter now gives the resolver the DECLARED domain
// explicitly (`resolve_task<flight::Any>(...)`), exactly as a source-written `Promise.resolve<T>(x)` does.
// What this file pins is that the explicit domain changes nothing but the carrier: the same value, the same
// object, the same settlement, and an adopted rejection intact. The emitter's side is pinned in
// packages/compiler-backend-cpp/src/cppPromiseResolveDomain.test.ts.
//
// Measured site: `@flighthq/loader`'s `resourceLoader.ts` `_noopLoad`, whose `Promise.resolve(undefined)` in a
// `Promise<unknown>` function is the only reason that module's header does not compile.
#include <flight/any.hpp>
#include <flight/error.hpp>
#include <flight/task.hpp>

#include <iostream>
#include <memory>

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

void test_the_declared_domain_holds_the_literal() {
  flight::Task<flight::Any> task = flight::resolve_task<flight::Any>(flight::undefined);
  check(task.status() == flight::TaskStatus::fulfilled, "the settlement is fulfilled");
  check(task.get().is_undefined(), "the value is the undefined that was resolved, in the declared domain");
}

void test_the_declared_domain_keeps_the_object() {
  flight::Ref<Subject> subject = flight::make_ref<Subject>();
  subject->marker = 7;
  flight::Task<flight::Any> task = flight::resolve_task<flight::Any>(flight::Any::object(subject));
  const flight::Any settled = task.get();
  const flight::Ref<Subject> recovered = settled.object_if<Subject>();
  check(recovered.get() == subject.get(), "the settled value is the same object, not a copy of it");
  check(recovered->marker == 7, "and it is the object that was resolved");
}

void test_the_deduced_domain_still_works() {
  flight::Task<double> task = flight::resolve_task(2.0);
  check(task.status() == flight::TaskStatus::fulfilled, "a value already in the declared domain settles");
  check(task.get() == 2.0, "with its own value");
}

void test_adoption_keeps_the_settlement() {
  flight::Task<double> inner = flight::resolve_task(3.0);
  flight::Task<double> adopted = flight::resolve_task(inner);
  check(adopted.status() == flight::TaskStatus::fulfilled, "an adopted task keeps the settlement it had");
  check(adopted.get() == 3.0, "and its value, not a re-settlement");
  check(inner.status() == flight::TaskStatus::fulfilled, "adoption does not disturb the source task");
}

void test_adoption_keeps_a_rejection() {
  flight::Task<double> rejected = flight::Task<double>::reject(flight::Error(flight::String("boom")));
  flight::Task<double> adopted = flight::resolve_task(rejected);
  check(adopted.status() == flight::TaskStatus::rejected, "an adopted rejection stays rejected");
  // A rejection carrying a VALUE (rather than an exception) is delivered as an unhandled-rejection shape
  // when it is read outside a `catch`, which is that value's own contract rather than anything adoption
  // changes; what adoption must preserve is the rejection itself.
  bool threw = false;
  try {
    static_cast<void>(adopted.get());
  } catch (...) {
    threw = true;
  }
  check(threw, "and reading it raises rather than returning a value");
}

void test_adoption_keeps_an_exception_rejection() {
  flight::Task<double> rejected =
      flight::Task<double>::reject(std::make_exception_ptr(flight::Error(flight::String("original"))));
  flight::Task<double> adopted = flight::resolve_task(rejected);
  check(adopted.status() == flight::TaskStatus::rejected, "an adopted exception rejection stays rejected");
  bool threw = false;
  try {
    static_cast<void>(adopted.get());
  } catch (const flight::Error& error) {
    threw = error.message() == flight::String("original");
  } catch (...) {
    threw = false;
  }
  check(threw, "and reading it raises that same exception, not a copy of a message");
}

}  // namespace

int main() {
  test_the_declared_domain_holds_the_literal();
  test_the_declared_domain_keeps_the_object();
  test_the_deduced_domain_still_works();
  test_adoption_keeps_the_settlement();
  test_adoption_keeps_a_rejection();
  test_adoption_keeps_an_exception_rejection();
  if (failures != 0) {
    std::cerr << failures << " task resolve-domain check(s) failed\n";
    return 1;
  }
  std::cout << "task resolve domain: all checks passed\n";
  return 0;
}
