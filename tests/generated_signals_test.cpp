#include <flight/signals/_internal_internal.hpp>
#include <flight/signals/emitter.hpp>
#include <flight/signals/safe.hpp>
#include <flight/signals/scope.hpp>
#include <flight/signals/signal.hpp>
#include <flight/signals/slot.hpp>

#include <algorithm>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

// Native port of packages/signals/src/{signal,internal,slot,emitter,safe,scope}.test.ts at a627849, against
// the generated headers (signals' one override covers throttle.ts, whose cases are in
// generated_signals_throttle_test.cpp).
//
// Slots are flight::Function values carried in the std::function the emitter spells. JavaScript compares
// functions by reference, and flight::callable_identity_equal answers that for exactly this carrier (and
// for plain function pointers); a bare lambda has no identity and disconnect refuses it rather than
// guessing. Every slot below is created once and passed by copy, which shares its identity as a JS
// reference would.
//
// Excluded, with their blockers:
// - every connection.test.ts case (11), and every scope.test.ts case that registers a member (12): each
//   creates its handle with connectSignalTracked, which the emitter refuses at this pin
//   (generated/include/flight/signals/connection.hpp:39, cpp-generic-owner-argument-assertion-unproven)
//   and no override supplies. disconnect/pause/resumeSignalConnection and disconnectSignalScope over
//   members are emitted, but their only inputs are handles that function constructs; building a
//   SignalConnection by hand would fabricate the wrapper slot those cases test.
// - "createSignalScope starts empty and collects only the connections that name it": its emptiness half is
//   ported; the collection half needs connectSignalTracked (above).
// - initializeSignal / initializeSignalScope / nullSignalEmit `typeof === 'function'` and "returns
//   undefined" halves are established at compile time (static_assert on the declared void result).
//
// Three source patches carry behaviour these cases assert. Each case fails with its patch reverted:
// - compiler-indexed-element-presence-test: the tombstone compaction and live-slot cases;
// - compiler-constrained-type-parameter-union-slot: every emitSignalSafe case, since no instantiation
//   compiled before it;
// - compiler-dependent-callable-pack-reuse: dispatch hands each slot the same arguments, which "emitSignalSafe
//   gives a nested safe emission its own snapshot and arguments" observes upstream (String arguments) and
//   test_temporary_argument_delivery pins directly for String and Ref temporaries.

namespace {

namespace sig = flight::signals;
using Void = std::function<void()>;

int failures = 0;

void check(bool condition, const std::string& message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

// A JS function value: one identity, shared by every copy.
template <typename Signature, typename Implementation>
std::function<Signature> fn(Implementation implementation) {
  return std::function<Signature>(flight::Function<Signature>(std::move(implementation)));
}

flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::SignalConnectOptions>>>> options(std::optional<bool> once,
                                                                                                                     std::optional<double> priority) {
  auto out = flight::make_ref<flight::types::SignalConnectOptions>();
  out->once = once;
  out->priority = priority;
  return flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::SignalConnectOptions>>>>(out);
}

template <typename Call>
bool does_not_throw(Call call) {
  try {
    call();
    return true;
  } catch (...) {
    return false;
  }
}

using Strings = std::vector<std::string>;

void test_signal_and_internal() {
  {
    auto signal = sig::create_signal<Void>();
    check(!signal->data.has_value(), "createSignal initializes with data=null");
  }
  {
    auto signal = sig::create_signal<Void>();
    static_assert(std::is_void_v<decltype(signal->emit())>, "createSignal's emit returns undefined");
    check(static_cast<bool>(signal->emit) && does_not_throw([&] { signal->emit(); }), "createSignal initializes with a no-op emit");
  }
  static_assert(std::is_void_v<decltype(sig::null_signal_emit())>, "nullSignalEmit returns undefined");
  check(does_not_throw([] { sig::null_signal_emit(); }), "nullSignalEmit returns undefined and does nothing");
}

void test_slot() {
  {
    auto signal = sig::create_signal<Void>();
    int count = 0;
    sig::connect_signal(signal, fn<void()>([&] { ++count; }));
    sig::connect_signal(signal, fn<void()>([&] { ++count; }));
    sig::clear_signal(signal);
    sig::emit_signal(signal);
    check(count == 0, "clearSignal removes all slots");
  }

  {
    auto signal = sig::create_signal<Void>();
    bool called = false;
    sig::connect_signal(signal, fn<void()>([&] { called = true; }));
    sig::emit_signal(signal);
    check(called, "connectSignal connects a slot that receives emits");
  }
  {
    auto signal = sig::create_signal<Void>();
    std::vector<int> order;
    sig::connect_signal(signal, fn<void()>([&] { order.push_back(1); }));
    sig::connect_signal(signal, fn<void()>([&] { order.push_back(2); }));
    sig::connect_signal(signal, fn<void()>([&] { order.push_back(3); }));
    sig::emit_signal(signal);
    check(order == std::vector<int>{1, 2, 3}, "connectSignal connects multiple slots emitted in insertion order");
  }
  {
    auto signal = sig::create_signal<std::function<void(double, double)>>();
    std::vector<double> received;
    sig::connect_signal(signal, fn<void(double, double)>([&](double x, double y) { received = {x, y}; }));
    sig::emit_signal(signal, 3.0, 7.0);
    check(received == std::vector<double>{3, 7}, "connectSignal passes typed arguments to slot");
  }
  {
    auto signal = sig::create_signal<Void>();
    int count = 0;
    sig::connect_signal(signal, fn<void()>([&] { ++count; }), options(true, std::nullopt));
    sig::emit_signal(signal);
    sig::emit_signal(signal);
    check(count == 1, "connectSignal removes slot after first emit when once=true");
  }
  {
    auto signal = sig::create_signal<Void>();
    int count = 0;
    sig::connect_signal(signal, fn<void()>([&] { ++count; }));
    sig::emit_signal(signal);
    sig::emit_signal(signal);
    check(count == 2, "connectSignal keeps slot across emits when once is not set");
  }
  {
    auto signal = sig::create_signal<Void>();
    std::vector<int> order;
    sig::connect_signal(signal, fn<void()>([&] { order.push_back(1); }), options(std::nullopt, 0.0));
    sig::connect_signal(signal, fn<void()>([&] { order.push_back(2); }), options(std::nullopt, 10.0));
    sig::connect_signal(signal, fn<void()>([&] { order.push_back(3); }), options(std::nullopt, 5.0));
    sig::emit_signal(signal);
    check(order == std::vector<int>{2, 3, 1}, "connectSignal emits higher priority slots first");
  }
  {
    auto signal = sig::create_signal<Void>();
    Strings calls;
    bool reentered = false;
    sig::connect_signal(signal, fn<void()>([&] {
                          calls.push_back("a");
                          if (!reentered) {
                            reentered = true;
                            sig::emit_signal(signal);
                          }
                        }));
    sig::connect_signal(signal, fn<void()>([&] { calls.push_back("b"); }));
    sig::emit_signal(signal);
    const auto contains = [&](const char* name) { return std::find(calls.begin(), calls.end(), name) != calls.end(); };
    check(contains("a") && contains("b"), "connectSignal delivers to every slot when a slot re-emits the same signal");
  }
  {
    auto signal = sig::create_signal<Void>();
    int once_count = 0;
    sig::connect_signal(signal, fn<void()>([&] { ++once_count; }), options(true, std::nullopt));
    bool reentered = false;
    sig::connect_signal(signal, fn<void()>([&] {
                          if (!reentered) {
                            reentered = true;
                            sig::emit_signal(signal);
                          }
                        }));
    sig::emit_signal(signal);
    sig::emit_signal(signal);
    check(once_count == 1, "connectSignal removes a once slot exactly once across a re-entrant emit");
  }

  {
    auto signal = sig::create_signal<Void>();
    check(does_not_throw([&] { sig::disconnect_signal(signal, fn<void()>([] {})); }), "disconnectSignal does nothing when no slots are connected");
  }
  {
    auto signal = sig::create_signal<Void>();
    int count = 0;
    const auto slot = fn<void()>([&] { ++count; });
    sig::connect_signal(signal, slot);
    sig::disconnect_signal(signal, slot);
    sig::emit_signal(signal);
    check(count == 0, "disconnectSignal removes a specific slot");
  }
  {
    auto signal = sig::create_signal<Void>();
    int a = 0, b = 0;
    const auto slot_a = fn<void()>([&] { ++a; });
    sig::connect_signal(signal, slot_a);
    sig::connect_signal(signal, fn<void()>([&] { ++b; }));
    sig::disconnect_signal(signal, slot_a);
    sig::emit_signal(signal);
    check(a == 0 && b == 1, "disconnectSignal does not remove other slots");
  }
}

void test_dispatch_mutation() {
  {
    auto signal = sig::create_signal<Void>();
    Strings calls;
    const auto a = fn<void()>([&] { calls.push_back("a"); });
    sig::connect_signal(signal, a);
    sig::connect_signal(signal, fn<void()>([&] {
                          calls.push_back("b");
                          sig::disconnect_signal(signal, a);
                        }));
    sig::connect_signal(signal, fn<void()>([&] { calls.push_back("c"); }));
    sig::emit_signal(signal);
    check(calls == Strings{"a", "b", "c"}, "emitSignal runs the slot that shifts into a disconnected earlier position");
  }
  {
    auto signal = sig::create_signal<Void>();
    Strings calls;
    auto a = std::make_shared<Void>();
    *a = fn<void()>([&, a_weak = std::weak_ptr<Void>(a)] {
      calls.push_back("a");
      sig::disconnect_signal(signal, *a_weak.lock());
    });
    sig::connect_signal(signal, *a);
    sig::connect_signal(signal, fn<void()>([&] { calls.push_back("b"); }));
    sig::emit_signal(signal);
    check(calls == Strings{"a", "b"}, "emitSignal keeps dispatching to later slots after a slot disconnects itself");
  }
  {
    auto signal = sig::create_signal<Void>();
    Strings calls;
    const auto c = fn<void()>([&] { calls.push_back("c"); });
    sig::connect_signal(signal, fn<void()>([&] {
                          calls.push_back("a");
                          sig::disconnect_signal(signal, c);
                        }));
    sig::connect_signal(signal, fn<void()>([&] { calls.push_back("b"); }));
    sig::connect_signal(signal, c);
    sig::emit_signal(signal);
    check(calls == Strings{"a", "b"}, "emitSignal does not deliver to a slot disconnected earlier in the same dispatch");
  }
  {
    auto signal = sig::create_signal<Void>();
    Strings nested;
    int depth = 0;
    bool reentered = false;
    const auto record = [&](const char* name) {
      if (depth > 0) nested.push_back(name);
    };
    auto a = std::make_shared<Void>();
    *a = fn<void()>([&] {
      record("a");
      if (!reentered) {
        reentered = true;
        ++depth;
        sig::emit_signal(signal);
        --depth;
      }
    });
    sig::connect_signal(signal, *a);
    sig::connect_signal(signal, fn<void()>([&] {
                          record("b");
                          sig::disconnect_signal(signal, *a);
                        }));
    sig::connect_signal(signal, fn<void()>([&] { record("c"); }));
    sig::emit_signal(signal);
    check(nested == Strings{"a", "b", "c"}, "emitSignal gives a nested emit its own cursor, unskewed by a disconnect inside it");
  }
  {
    auto signal = sig::create_signal<Void>();
    Strings calls;
    sig::connect_signal(signal, fn<void()>([&] { calls.push_back("once"); }), options(true, std::nullopt));
    bool reentered = false;
    sig::connect_signal(signal, fn<void()>([&] {
                          calls.push_back("outer");
                          if (!reentered) {
                            reentered = true;
                            sig::emit_signal(signal);
                          }
                        }));
    sig::connect_signal(signal, fn<void()>([&] { calls.push_back("tail"); }));
    sig::emit_signal(signal);
    check(std::count(calls.begin(), calls.end(), "once") == 1 && std::count(calls.begin(), calls.end(), "tail") == 2,
          "emitSignal fires a once slot exactly once when a nested emit walks past it");
  }
  {
    auto signal = sig::create_signal<Void>();
    Strings calls;
    const auto b = fn<void()>([&] { calls.push_back("b"); });
    sig::connect_signal(signal, fn<void()>([&] {
                          calls.push_back("a");
                          sig::disconnect_signal(signal, b);
                        }));
    sig::connect_signal(signal, b);
    sig::emit_signal(signal);
    const auto& data = signal->data;
    const bool compacted = data.has_value() && data.value()->slots.size() == 1 && data.value()->priorities.size() == 1 && data.value()->repeat.size() == 1;
    const bool states = !sig::is_slot_connected<Void>(signal, b) && sig::has_signal_slots<Void>(signal);
    calls.clear();
    sig::emit_signal(signal);
    check(states && compacted && calls == Strings{"a"}, "emitSignal purges disconnected entries once the outermost emit exits");
  }
  {
    auto signal = sig::create_signal<Void>();
    auto a = std::make_shared<Void>();
    *a = fn<void()>([&, a_weak = std::weak_ptr<Void>(a)] { sig::disconnect_signal(signal, *a_weak.lock()); });
    sig::connect_signal(signal, *a);
    sig::emit_signal(signal);
    check(!sig::has_signal_slots<Void>(signal) && !sig::is_slot_connected<Void>(signal, *a) && !signal->data.has_value(),
          "emitSignal returns the signal to its empty state when every slot disconnects during dispatch");
  }
  {
    auto signal = sig::create_signal<Void>();
    std::vector<bool> observed;
    auto a = std::make_shared<Void>();
    *a = fn<void()>([&, a_weak = std::weak_ptr<Void>(a)] {
      sig::disconnect_signal(signal, *a_weak.lock());
      observed.push_back(sig::has_signal_slots<Void>(signal));
    });
    sig::connect_signal(signal, *a);
    sig::emit_signal(signal);
    check(observed == std::vector<bool>{false}, "emitSignal reports no slots from inside the dispatch that disconnected the last one");
  }
  {
    auto signal = sig::create_signal<Void>();
    std::vector<bool> observed;
    const auto b = fn<void()>([] {});
    sig::connect_signal(signal, fn<void()>([&] {
                          sig::disconnect_signal(signal, b);
                          observed.push_back(sig::is_slot_connected<Void>(signal, b));
                          observed.push_back(sig::has_signal_slots<Void>(signal));
                        }));
    sig::connect_signal(signal, b);
    sig::emit_signal(signal);
    check(observed == std::vector<bool>{false, true}, "emitSignal reports a slot disconnected mid-dispatch as disconnected from inside the same dispatch");
  }
}

void test_queries() {
  {
    auto signal = sig::create_signal<Void>();
    check(!sig::has_signal_slots<Void>(signal), "hasSignalSlots returns false when no slot is connected");
  }
  {
    auto signal = sig::create_signal<Void>();
    sig::connect_signal(signal, fn<void()>([] {}));
    check(sig::has_signal_slots<Void>(signal), "hasSignalSlots returns true when at least one slot is connected");
  }
  {
    auto signal = sig::create_signal<Void>();
    const auto slot = fn<void()>([] {});
    sig::connect_signal(signal, slot);
    sig::disconnect_signal(signal, slot);
    check(!sig::has_signal_slots<Void>(signal), "hasSignalSlots returns false after the last slot is disconnected");
  }
  {
    auto signal = sig::create_signal<Void>();
    const auto slot = fn<void()>([] {});
    sig::connect_signal(signal, slot);
    check(sig::is_slot_connected<Void>(signal, slot), "isSlotConnected returns true when slot is connected");
  }
  {
    auto signal = sig::create_signal<Void>();
    check(!sig::is_slot_connected<Void>(signal, fn<void()>([] {})), "isSlotConnected returns false when slot is not connected");
  }
  {
    auto signal = sig::create_signal<Void>();
    const auto slot = fn<void()>([] {});
    sig::connect_signal(signal, slot);
    sig::disconnect_signal(signal, slot);
    check(!sig::is_slot_connected<Void>(signal, slot), "isSlotConnected returns false after slot is disconnected");
  }
}

void test_emitter() {
  {
    auto signal = sig::create_signal<Void>();
    check(does_not_throw([&] { sig::cancel_signal(signal); }), "cancelSignal does nothing when no slots are connected");
  }
  {
    auto signal = sig::create_signal<Void>();
    std::vector<int> order;
    sig::connect_signal(signal, fn<void()>([&] {
                          order.push_back(1);
                          sig::cancel_signal(signal);
                        }));
    sig::connect_signal(signal, fn<void()>([&] { order.push_back(2); }));
    sig::emit_signal(signal);
    check(order == std::vector<int>{1}, "cancelSignal stops emit after the canceling slot");
  }
  {
    auto signal = sig::create_signal<Void>();
    int count = 0;
    sig::connect_signal(signal, fn<void()>([&] {
                          ++count;
                          sig::cancel_signal(signal);
                        }));
    sig::connect_signal(signal, fn<void()>([&] { ++count; }));
    sig::emit_signal(signal);
    sig::emit_signal(signal);
    check(count == 2, "cancelSignal does not prevent subsequent emits");
  }

  {
    auto signal = sig::create_signal<Void>();
    int count = 0;
    sig::connect_signal(signal, fn<void()>([&] { ++count; }));
    sig::connect_signal(signal, fn<void()>([&] { ++count; }));
    sig::emit_signal(signal);
    check(count == 2, "emitSignal calls all connected slots");
  }
  {
    auto signal = sig::create_signal<std::function<void(double)>>();
    std::vector<double> received;
    sig::connect_signal(signal, fn<void(double)>([&](double v) { received.push_back(v); }));
    sig::emit_signal(signal, 42.0);
    check(received == std::vector<double>{42}, "emitSignal passes typed arguments to each slot");
  }
  {
    auto signal = sig::create_signal<Void>();
    int count = 0;
    sig::connect_signal(signal, fn<void()>([&] {
                          ++count;
                          sig::cancel_signal(signal);
                        }));
    sig::connect_signal(signal, fn<void()>([&] { ++count; }));
    sig::emit_signal(signal);
    const bool first = count == 1;
    sig::emit_signal(signal);
    check(first && count == 2, "emitSignal resets canceled=false before each emit");
  }
  {
    auto signal = sig::create_signal<Void>();
    int count = 0;
    sig::connect_signal(signal, fn<void()>([&] { ++count; }), options(true, std::nullopt));
    sig::emit_signal(signal);
    sig::emit_signal(signal);
    check(count == 1, "emitSignal removes once slots after they fire");
  }
  {
    auto signal = sig::create_signal<Void>();
    check(does_not_throw([&] { sig::emit_signal(signal); }), "emitSignal does nothing when no slots are connected");
  }
}

void test_safe() {
  {
    auto signal = sig::create_signal<std::function<void(double)>>();
    Strings received;
    const auto label = [](const char* name, double value) { return std::string(name) + ":" + std::to_string(static_cast<int>(value)); };
    sig::connect_signal(signal, fn<void(double)>([&](double v) { received.push_back(label("low", v)); }), options(std::nullopt, -1.0));
    sig::connect_signal(signal, fn<void(double)>([&](double v) { received.push_back(label("high", v)); }), options(std::nullopt, 10.0));
    sig::connect_signal(signal, fn<void(double)>([&](double v) { received.push_back(label("normal", v)); }));
    sig::emit_signal_safe(signal, 42.0);
    check(received == Strings{"high:42", "normal:42", "low:42"}, "emitSignalSafe passes typed arguments in priority order");
  }
  {
    auto signal = sig::create_signal<Void>();
    Strings calls;
    bool added = false;
    sig::connect_signal(signal, fn<void()>([&] {
                          calls.push_back("a");
                          if (!added) {
                            added = true;
                            sig::connect_signal(signal, fn<void()>([&] { calls.push_back("added"); }));
                          }
                        }));
    sig::connect_signal(signal, fn<void()>([&] { calls.push_back("b"); }));
    sig::emit_signal_safe(signal);
    const bool first = calls == Strings{"a", "b"};
    calls.clear();
    sig::emit_signal_safe(signal);
    check(first && calls == Strings{"a", "b", "added"}, "emitSignalSafe does not call a slot added during the current safe dispatch");
  }
  {
    auto signal = sig::create_signal<Void>();
    Strings calls;
    const auto removed = fn<void()>([&] { calls.push_back("removed"); });
    sig::connect_signal(signal, fn<void()>([&] {
                          calls.push_back("first");
                          sig::disconnect_signal(signal, removed);
                        }));
    sig::connect_signal(signal, removed);
    sig::emit_signal_safe(signal);
    const bool first = calls == Strings{"first", "removed"};
    calls.clear();
    sig::emit_signal_safe(signal);
    check(first && calls == Strings{"first"}, "emitSignalSafe still calls a copied slot removed during the current safe dispatch");
  }
  {
    auto signal = sig::create_signal<std::function<void(flight::String)>>();
    Strings calls;
    bool nested = false;
    sig::connect_signal(signal, fn<void(flight::String)>([&](flight::String value) {
                          calls.push_back("a:" + value.to_utf8());
                          if (!nested) {
                            nested = true;
                            sig::emit_signal_safe(signal, flight::String("nested"));
                          }
                        }));
    sig::connect_signal(signal, fn<void(flight::String)>([&](flight::String value) { calls.push_back("b:" + value.to_utf8()); }));
    sig::emit_signal_safe(signal, flight::String("outer"));
    check(calls == Strings{"a:outer", "a:nested", "b:nested", "b:outer"}, "emitSignalSafe gives a nested safe emission its own snapshot and arguments");
  }
  {
    auto signal = sig::create_signal<Void>();
    Strings calls;
    bool nested = false;
    sig::connect_signal(signal, fn<void()>([&] {
                          calls.push_back("once");
                          if (!nested) {
                            nested = true;
                            sig::emit_signal_safe(signal);
                          }
                        }),
                        options(true, std::nullopt));
    sig::connect_signal(signal, fn<void()>([&] { calls.push_back("tail"); }));
    sig::emit_signal_safe(signal);
    sig::emit_signal_safe(signal);
    check(calls == Strings{"once", "tail", "tail", "tail"}, "emitSignalSafe removes a once slot before invoking it so nested safe emission cannot repeat it");
  }
  {
    auto signal = sig::create_signal<Void>();
    Strings calls;
    bool cancel = true;
    sig::connect_signal(signal, fn<void()>([&] {
                          calls.push_back("first");
                          if (cancel) sig::cancel_signal(signal);
                        }));
    sig::connect_signal(signal, fn<void()>([&] { calls.push_back("second"); }));
    sig::emit_signal_safe(signal);
    const bool first = calls == Strings{"first"};
    cancel = false;
    sig::emit_signal_safe(signal);
    check(first && calls == Strings{"first", "first", "second"}, "emitSignalSafe stops after cancellation and resets cancellation before the next safe emission");
  }
  {
    auto signal = sig::create_signal<Void>();
    check(does_not_throw([&] { sig::emit_signal_safe(signal); }), "emitSignalSafe does nothing when no slots are connected");
  }
}

// Not an upstream case: the delivery guarantee the forwarding fix restores, asserted directly. Two slots
// receive the same temporary, and a Ref temporary reaches both as the same live object.
struct Payload : flight::ReferenceEnabled {
  double value = 7;
};

void test_temporary_argument_delivery() {
  {
    auto signal = sig::create_signal<std::function<void(flight::String)>>();
    Strings received;
    sig::connect_signal(signal, fn<void(flight::String)>([&](flight::String v) { received.push_back("a:" + v.to_utf8()); }));
    sig::connect_signal(signal, fn<void(flight::String)>([&](flight::String v) { received.push_back("b:" + v.to_utf8()); }));
    sig::emit_signal(signal, flight::String("x"));
    check(received == Strings{"a:x", "b:x"}, "emitSignal delivers a temporary String to every slot");
    received.clear();
    sig::emit_signal_safe(signal, flight::String("y"));
    check(received == Strings{"a:y", "b:y"}, "emitSignalSafe delivers a temporary String to every slot");
  }
  {
    auto signal = sig::create_signal<std::function<void(flight::Ref<Payload>)>>();
    std::vector<flight::Ref<Payload>> received;
    sig::connect_signal(signal, fn<void(flight::Ref<Payload>)>([&](flight::Ref<Payload> p) { received.push_back(p); }));
    sig::connect_signal(signal, fn<void(flight::Ref<Payload>)>([&](flight::Ref<Payload> p) { received.push_back(p); }));
    sig::emit_signal(signal, flight::make_ref<Payload>());
    check(received.size() == 2 && received[0] != nullptr && received[0] == received[1] && received[1]->value == 7,
          "emitSignal delivers one temporary Ref, the same live object, to every slot");
    received.clear();
    sig::emit_signal_safe(signal, flight::make_ref<Payload>());
    check(received.size() == 2 && received[0] != nullptr && received[0] == received[1],
          "emitSignalSafe delivers one temporary Ref, the same live object, to every slot");
  }
}

void test_scope() {
  {
    auto scope = sig::create_signal_scope();
    check(scope->connections.size() == 0, "createSignalScope starts empty (the collection half needs connectSignalTracked)");
  }
  {
    auto scope = sig::create_signal_scope();
    check(does_not_throw([&] { sig::disconnect_signal_scope(scope); }) && scope->connections.size() == 0, "disconnectSignalScope accepts an empty scope");
  }
  static_assert(std::is_invocable_v<decltype(&sig::initialize_signal_scope), flight::types::EntityConstruction<flight::Ref<flight::types::SignalScope>>>,
                "initializeSignalScope is the construction initializer of createSignalScope");
}

}  // namespace

int main() {
  test_signal_and_internal();
  test_slot();
  test_dispatch_mutation();
  test_queries();
  test_emitter();
  test_safe();
  test_temporary_argument_delivery();
  test_scope();
  return failures == 0 ? 0 : 1;
}
