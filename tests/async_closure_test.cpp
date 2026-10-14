// Async closures as the compiler emits them (compiler-async-closure-coroutine patch), checked against node.
//
// The functions in namespace emitted are the compiler's output for these sources, verbatim:
//
//   export interface Box { value: string }
//   export interface Holder { box: Box; read(): Promise<string> }
//   export function chain(tail: Promise<void>, log: string[]): Promise<void> {
//     return tail.then(async () => { log.push('before'); await tail; log.push('after'); });
//   }
//   export function counter(gate: Promise<void>): () => Promise<number> {
//     let n = 0;
//     return async () => { n += 1; await gate; return n; };
//   }
//   export function doubler(): (x: number) => Promise<number> { return async (x: number) => x * 2; }
//   export function reader(box: Box, gate: Promise<void>): () => Promise<string> {
//     return async () => { await gate; return box.value; };
//   }
//   export function holder(box: Box, gate: Promise<void>): Holder {
//     return { box, async read(): Promise<string> { await gate; return box.value; } };
//   }
//
// A coroutine lambda's frame reads its captures through the closure object, so the closure is held in a shared
// owner that flight::retain_closure keeps alive for the life of the call's coroutine frame. The lifetime cases
// below drop every other owner of the closure while its coroutine is suspended.
#include <flight/runtime.hpp>
#include <flight/executor.hpp>
#include <flight/task.hpp>

#include <functional>
#include <iostream>
#include <memory>
#include <optional>

namespace emitted {

struct Box;
struct Holder;

struct Box : public flight::ReferenceEnabled {
  flight::String value;
};

struct Holder : public flight::ReferenceEnabled {
  flight::Ref<Box> box;
  std::function<flight::Task<flight::String>()> read;
};

inline flight::Task<void> chain(flight::Task<void> tail, flight::Array<flight::String> log) {
  return tail.then([async_closure = std::make_shared<const std::function<flight::Task<void>()>>([=]() -> flight::Task<void> {
  log.push(flight::String("before"));
  co_await tail;
  log.push(flight::String("after"));
  co_return;
})]() -> flight::Task<void> { return flight::retain_closure((*async_closure)(), async_closure); });
}

inline std::function<flight::Task<double>()> counter(flight::Task<void> gate) {
  const auto n_capture = flight::make_binding_cell(double{0.0});
  return [async_closure_2 = std::make_shared<const std::function<flight::Task<double>()>>([=]() -> flight::Task<double> {
  n_capture.update_binding([&](auto& binding_value) { binding_value += 1.0; return binding_value; });
  co_await gate;
  co_return n_capture.read_binding();
})]() -> flight::Task<double> { return flight::retain_closure((*async_closure_2)(), async_closure_2); };
}

inline std::function<flight::Task<double>(double)> doubler() {
  return [async_closure_3 = std::make_shared<const std::function<flight::Task<double>(double)>>([=](double x) -> flight::Task<double> {
  co_return (x * 2.0);
})](double x) -> flight::Task<double> { return flight::retain_closure((*async_closure_3)(x), async_closure_3); };
}

inline std::function<flight::Task<flight::String>()> reader(flight::Ref<Box> box, flight::Task<void> gate) {
  return [async_closure_4 = std::make_shared<const std::function<flight::Task<flight::String>()>>([=]() -> flight::Task<flight::String> {
  co_await gate;
  co_return box->value;
})]() -> flight::Task<flight::String> { return flight::retain_closure((*async_closure_4)(), async_closure_4); };
}

inline flight::Ref<Holder> holder(flight::Ref<Box> box, flight::Task<void> gate) {
  return flight::make_ref<Holder>(Holder{.box = box, .read = [async_closure_5 = std::make_shared<const std::function<flight::Task<flight::String>()>>([=]() -> flight::Task<flight::String> {
  co_await gate;
  co_return box->value;
})]() -> flight::Task<flight::String> { return flight::retain_closure((*async_closure_5)(), async_closure_5); }});
}

} // namespace emitted

namespace {

int failures = 0;

void check(bool ok, const char* what) {
  if (ok) return;
  std::cerr << "FAIL: " << what << '\n';
  ++failures;
}

using namespace emitted;

struct Gate {
  flight::Task<void>::Resolver resolve;
  flight::Task<void> task;
};
Gate gate() {
  std::optional<flight::Task<void>::Resolver> resolver;
  auto task = flight::Task<void>::create([&](flight::Task<void>::Resolver r) { resolver = r; });
  return Gate{*resolver, task};
}
} // namespace

int main() {
  const auto executor = std::make_shared<flight::QueueExecutor>();
  const flight::ExecutorScope scope(executor);

  {  // then(async () => ...): starts after tail, runs to its await synchronously, settles when the body completes.
    auto g = gate();
    flight::Array<flight::String> log;
    auto done = chain(g.task, log);
    check(log.size() == 0, "the callback has not run before tail settles");
    g.resolve();
    executor->run_until_idle();
    check(log.size() == 2 && log.get(0) == flight::String("before") && log.get(1) == flight::String("after"),
          "the callback ran its whole body in order");
    check(done.status() == flight::TaskStatus::fulfilled, "then settles with the async callback's completion");
  }
  {  // A shared mutable capture: each call increments the one binding, synchronously before its await.
    auto g = gate();
    auto next = counter(g.task);
    auto first = next();
    auto second = next();
    check(first.status() == flight::TaskStatus::pending, "a call is pending at its first await");
    g.resolve();
    executor->run_until_idle();
    check(first.get() == 2.0 && second.get() == 2.0, "both calls read the shared binding after both increments");
  }
  {  // An expression-bodied async arrow settles with its value.
    auto twice = doubler();
    auto result = twice(21.0);
    executor->run_until_idle();
    check(result.status() == flight::TaskStatus::fulfilled && result.get() == 42.0, "async (x) => x * 2");
  }
  {  // Lifetime: the closure is dropped while its coroutine is suspended.
    auto g = gate();
    auto box = flight::make_ref<Box>();
    box->value = flight::String("kept");
    std::weak_ptr<Box> weak = box;
    std::optional<flight::Task<flight::String>> pending;
    {
      auto read = reader(box, g.task);
      box.reset();
      pending = read();
    }  // the only owner of the closure is gone; the frame is suspended at `await gate`
    check(!weak.expired(), "the suspended frame keeps the closure, and the capture, alive");
    g.resolve();
    executor->run_until_idle();
    check(pending->status() == flight::TaskStatus::fulfilled && pending->get() == flight::String("kept"),
          "the resumed frame reads the capture it started with");
    pending.reset();
    executor->run_until_idle();
    check(weak.expired(), "releasing the task releases the closure and its capture");
  }
  {  // An async object method: the object goes away while its method is suspended.
    auto g = gate();
    auto box = flight::make_ref<Box>();
    box->value = flight::String("method");
    std::optional<flight::Task<flight::String>> pending;
    std::weak_ptr<Holder> holder_weak;
    {
      auto h = holder(box, g.task);
      check(h->box == box, "the object holds the same box");
      holder_weak = h;
      pending = h->read();
    }
    check(holder_weak.expired(), "the object is gone while its method is suspended");
    box->value = flight::String("method, updated");
    g.resolve();
    executor->run_until_idle();
    check(pending->get() == flight::String("method, updated"), "the method reads the live object it captured, after its holder is gone");
  }
  if (failures == 0) std::cout << "async closure tests passed\n";
  return failures == 0 ? 0 : 1;
}
