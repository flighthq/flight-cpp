// The generated @flighthq/clock package at a627849, exercised natively. Expectations are ported from the
// package's own tests (packages/clock/src/clock.test.ts and clockSignals.test.ts): construction defaults
// and options, hierarchy membership and reparenting by reference identity, the advance cascade with scale
// and pause composing down the tree, signed deltas and scales, the opt-in onTick signal, the
// effective-scale and effective-pause queries, reset, and disposal of links and listeners.
#include <flight/clock/clock.hpp>
#include <flight/clock/clock_signals.hpp>
#include <flight/signals/slot.hpp>

#include <iostream>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}

// The readonly row view the generated API takes for a `Readonly<T>` parameter.
template <typename Pointer>
auto view(const Pointer& value) {
  using Element = typename Pointer::element_type;
  return flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<Element>>>>(value);
}

namespace clock = flight::clock;
using flight::types::Clock;
using flight::types::ClockOptions;

flight::Ref<Clock> make(std::optional<double> scale = std::nullopt, std::optional<bool> paused = std::nullopt) {
  if (!scale.has_value() && !paused.has_value()) return clock::create_clock();
  auto options = flight::make_ref<ClockOptions>();
  options->scale = scale;
  options->paused = paused;
  return clock::create_clock(view(options));
}

flight::Ref<Clock> child_of(const flight::Ref<Clock>& parent, std::optional<double> scale = std::nullopt) {
  auto options = flight::make_ref<ClockOptions>();
  options->scale = scale;
  return clock::create_child_clock(parent, view(options));
}

bool children_are(const flight::Ref<Clock>& clock, std::vector<flight::Ref<Clock>> expected) {
  if (clock->children.size() != expected.size()) return false;
  for (std::size_t i = 0; i < expected.size(); ++i) {
    if (clock->children.element(i) != expected[i]) return false;
  }
  return true;
}

bool parent_is(const flight::Ref<Clock>& clock, const flight::Ref<Clock>& parent) {
  return clock->parent.has_value() && clock->parent.value() == parent;
}

void construction() {
  auto c = make();
  check(c->scale == 1 && !c->paused && c->delta_time == 0 && c->elapsed == 0 && !c->parent.has_value() &&
            c->children.size() == 0 && !c->on_tick.has_value(),
        "defaults to realtime, running, zeroed, rootless and signal-free");
  auto options = make(3.0, true);
  check(options->scale == 3 && options->paused, "applies scale and paused options");

  auto parent = make();
  auto child = clock::create_child_clock(parent);
  check(parent_is(child, parent) && children_are(parent, {child}), "createChildClock attaches the child");
  auto configured = flight::make_ref<ClockOptions>();
  configured->scale = 0.5;
  configured->paused = true;
  auto optioned = clock::create_child_clock(parent, view(configured));
  check(optioned->scale == 0.5 && optioned->paused, "createChildClock applies options");
}

void hierarchy() {
  auto parent = make(), child = make();
  clock::add_clock_child(parent, child);
  check(parent_is(child, parent) && children_are(parent, {child}), "addClockChild attaches and sets parent");
  clock::add_clock_child(parent, child);
  check(children_are(parent, {child}), "re-adding to the same parent is a no-op");

  auto a = make(), b = make(), moving = make();
  clock::add_clock_child(a, moving);
  clock::add_clock_child(b, moving);
  check(children_are(a, {}) && children_are(b, {moving}) && parent_is(moving, b), "reparenting detaches from the old parent first");

  clock::remove_clock_child(b, moving);
  check(children_are(b, {}) && !moving->parent.has_value(), "removeClockChild detaches and clears parent");
  clock::remove_clock_child(b, moving);
  check(children_are(b, {}), "removing a non-child is a no-op");

  check(!clock::get_clock_parent(view(make())).has_value(), "getClockParent is null for a root");
  auto p = make();
  auto c = clock::create_child_clock(p);
  auto got = clock::get_clock_parent(view(c));
  check(got.has_value() && got.value() == p, "getClockParent returns the same parent reference");
}

void advancing() {
  {
    auto c = make();
    clock::advance_clock(c, 500);
    check(c->delta_time == 500 && c->elapsed == 500, "a realtime root takes the real delta");
    clock::advance_clock(c, 250);
    check(c->delta_time == 250 && c->elapsed == 750, "elapsed accumulates");
  }
  {
    auto c = make(2.0);
    clock::advance_clock(c, 1000);
    check(c->delta_time == 2000 && c->elapsed == 2000, "the local scale applies");
  }
  {
    auto c = make(std::nullopt, true);
    clock::advance_clock(c, 1000);
    check(c->delta_time == 0 && c->elapsed == 0, "a paused clock takes a zero delta");
  }
  {
    auto grandparent = make(2.0);
    auto parent = child_of(grandparent, 3.0);
    auto child = child_of(parent, 0.5);
    clock::advance_clock(grandparent, 1000);
    check(grandparent->delta_time == 2000 && parent->delta_time == 6000 && child->delta_time == 3000 && child->elapsed == 3000,
          "scale composes down three levels");
  }
  {
    auto parent = make(std::nullopt, true);
    auto child = clock::create_child_clock(parent);
    clock::advance_clock(parent, 1000);
    check(parent->delta_time == 0 && child->delta_time == 0 && child->elapsed == 0, "a paused ancestor freezes a running child");
  }
  {
    auto c = make();
    clock::advance_clock(c, 1000);
    clock::advance_clock(c, -500);
    check(c->delta_time == -500 && c->elapsed == 500, "a negative delta decreases elapsed");
  }
  {
    auto c = make(-1.0);
    clock::advance_clock(c, 1000);
    check(c->delta_time == -1000 && c->elapsed == -1000, "a negative scale reverses direction");
  }
  {
    auto c = make();
    clock::advance_clock(c, 1000);
    check(!c->on_tick.has_value(), "advancing without signals allocates none");
  }
}

void signals() {
  {
    auto c = make();
    auto on_tick = clock::enable_clock_signals(c);
    check(on_tick != nullptr && c->on_tick.has_value() && c->on_tick.value() == on_tick, "enable allocates and stores onTick");
    check(clock::enable_clock_signals(c) == on_tick, "enable is idempotent");
  }
  {
    auto c = make(2.0);
    std::vector<double> received;
    flight::signals::connect_signal(clock::enable_clock_signals(c), std::function<void(double)>([&](double dt) { received.push_back(dt); }),
                                    std::nullopt);
    clock::advance_clock(c, 500);
    check(received == std::vector<double>{1000}, "onTick emits the scaled delta");
  }
  {
    auto parent = make();
    auto child = clock::create_child_clock(parent);
    std::vector<int> order;
    flight::signals::connect_signal(clock::enable_clock_signals(parent), std::function<void(double)>([&](double) { order.push_back(1); }),
                                    std::nullopt);
    flight::signals::connect_signal(clock::enable_clock_signals(child), std::function<void(double)>([&](double) { order.push_back(2); }),
                                    std::nullopt);
    clock::advance_clock(parent, 16);
    check(order == std::vector<int>{1, 2}, "a parent ticks before its children (pre-order)");
  }
}

void queries() {
  check(clock::get_clock_effective_scale(view(make(2.0))) == 2, "a root's effective scale is its own");
  auto a = make(2.0);
  auto b = child_of(a, 0.5);
  auto c = child_of(b, 3.0);
  check(clock::get_clock_effective_scale(view(c)) == 3, "effective scale multiplies through the ancestors");

  check(!clock::is_clock_effectively_paused(view(make())), "a running root is not paused");
  check(clock::is_clock_effectively_paused(view(make(std::nullopt, true))), "a paused clock is paused");
  auto parent = make(std::nullopt, true);
  auto child = clock::create_child_clock(parent);
  check(!child->paused && clock::is_clock_effectively_paused(view(child)), "a paused ancestor pauses a running child");

  auto p = make();
  clock::pause_clock(p);
  check(p->paused, "pause sets paused");
  clock::resume_clock(p);
  check(!p->paused, "resume clears paused");
  clock::set_clock_scale(p, 0.25);
  check(p->scale == 0.25, "setClockScale sets the local scale");

  auto r = make(2.0);
  clock::advance_clock(r, 100);
  clock::reset_clock(r);
  check(r->elapsed == 0 && r->delta_time == 0 && r->scale == 2 && !r->paused, "reset clears time without touching scale or pause");
}

void disposal() {
  {
    auto parent = make();
    auto c = clock::create_child_clock(parent);
    clock::dispose_clock(c);
    check(children_are(parent, {}) && !c->parent.has_value(), "dispose detaches from the parent");
  }
  {
    auto c = make();
    auto child = clock::create_child_clock(c);
    clock::dispose_clock(c);
    check(children_are(c, {}) && !child->parent.has_value(), "dispose releases children as roots");
  }
  {
    auto c = make();
    auto on_tick = clock::enable_clock_signals(c);
    flight::signals::connect_signal(on_tick, std::function<void(double)>([](double) {}), std::nullopt);
    check(flight::signals::has_signal_slots(view(on_tick)), "a connected onTick has slots");
    clock::dispose_clock(c);
    check(!flight::signals::has_signal_slots(view(on_tick)), "dispose clears onTick listeners");
  }
  clock::dispose_clock(make());
}

}  // namespace

int main() {
  construction();
  hierarchy();
  advancing();
  signals();
  queries();
  disposal();
  return failures == 0 ? 0 : 1;
}
