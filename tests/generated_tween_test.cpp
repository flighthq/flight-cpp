// The @flighthq/tween package at a627849 -- its emitted modules with their supplying overrides -- exercised
// natively. Ported from packages/tween/src/*.test.ts:
// - interpolation: linear progress, multiple properties, the manager's default ease and an explicit one,
//   snapping, smart rotation's short path, a zero duration;
// - delay, repeat and yoyo: the delay window, a finite repeat budget, infinite repeat, reflect reversing
//   direction on every cycle, reverse playback;
// - settlement: onComplete exactly once, completeTween, stopTween / stopTweens with and without
//   { complete, sendEvent }, seek and progress landing on the end;
// - ordering and identity: overwrite of overlapping properties only, per-target registration, the
//   manager's map keyed by the target object itself, cleanup on the pass after completion;
// - lifecycle: defaults, pause / resume, reset, restart / invalidate, the default manager overload,
//   timers with their own sentinel target, staggers and colour tweens registered under the user's record.
//
// A target here is any object the tween reads through its named row -- a struct of numeric members boxed
// with `Any::object` -- or a `Record<String, number>` boxed with `Any::external`, which is how an erased
// value carries a record. A property map is the same: a struct declaring exactly the keys to tween.
#include <flight/tween/color_tween.hpp>
#include <flight/tween/timer.hpp>
#include <flight/tween/tween.hpp>
#include <flight/tween/tween_manager.hpp>
#include <flight/tween/tween_progress.hpp>
#include <flight/tween/tween_stagger.hpp>
#include <flight/tween/update_tweens.hpp>
#include <flight/signals/slot.hpp>

#include <cmath>
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

bool close(double actual, double expected) { return std::abs(actual - expected) < 0.005; }

template <typename Pointer>
auto view(const Pointer& value) {
  using Element = typename Pointer::element_type;
  return flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<Element>>>>(value);
}

namespace tw = flight::tween;
using flight::Any;
using flight::Ref;
using flight::types::TweenManager;
using TweenRef = Ref<flight::types::Tween<Any>>;
using OptionsView = flight::StructuralRef<flight::RowReadonly<flight::RowOf<Ref<flight::types::TweenOptions>>>>;
using StopView = flight::StructuralRef<flight::RowReadonly<flight::RowOf<Ref<flight::types::StopTweenOptions>>>>;
using PropsOrOptions = std::variant<flight::types::NumericProps<Any>, OptionsView>;

// Targets and property maps: plain objects whose members are the keys they declare.
struct X : flight::ReferenceEnabled {
  double x = 0;
};
struct Y : flight::ReferenceEnabled {
  double y = 0;
};
struct XY : flight::ReferenceEnabled {
  double x = 0;
  double y = 0;
};
struct Rotation : flight::ReferenceEnabled {
  double rotation = 0;
};
struct Empty : flight::ReferenceEnabled {};

Ref<X> xTarget(double x) { return flight::make_ref<X>(X{.x = x}); }
Any xProps(double x) { return Any::object(xTarget(x)); }
Any yProps(double y) { return Any::object(flight::make_ref<Y>(Y{.y = y})); }

double linear(double t) { return t; }

struct Opts {
  std::optional<double> delay;
  bool linear = false;
  std::optional<flight::types::EasingFunction> ease;
  std::optional<bool> overwrite;
  std::optional<bool> reflect;
  std::optional<double> repeat;
  std::optional<bool> reverse;
  std::optional<bool> smartRotation;
  std::optional<bool> snapping;
};

std::optional<OptionsView> options(const Opts& o) {
  auto out = flight::make_ref<flight::types::TweenOptions>();
  out->delay = o.delay;
  out->ease = o.linear ? std::optional<flight::types::EasingFunction>(linear) : o.ease;
  out->overwrite = o.overwrite;
  out->reflect = o.reflect;
  out->repeat = o.repeat;
  out->reverse = o.reverse;
  out->smart_rotation = o.smartRotation;
  out->snapping = o.snapping;
  return view(out);
}

// createTween(manager, target, duration, propertyMap, options?) -- the manager overload.
TweenRef tween(const Ref<TweenManager>& manager, const Any& target, double duration, const Any& props, std::optional<Opts> o = std::nullopt) {
  return tw::create_tween<Any>(std::variant<Ref<TweenManager>, Any>{std::in_place_type<Ref<TweenManager>>, manager},
                               std::variant<Any, double>{std::in_place_type<Any>, target},
                               std::variant<Any, double>{std::in_place_type<double>, duration},
                               PropsOrOptions{std::in_place_index<0>, props}, o ? options(*o) : std::nullopt);
}

TweenRef tween(const Ref<TweenManager>& manager, const Ref<X>& target, double duration, double toX, std::optional<Opts> o = std::nullopt) {
  return tween(manager, Any::object(target), duration, xProps(toX), o);
}

flight::ErasedRef key(const Any& target) { return target.erased_ref(); }

std::optional<StopView> stopOptions(std::optional<bool> complete, std::optional<bool> sendEvent) {
  auto out = flight::make_ref<flight::types::StopTweenOptions>();
  out->complete = complete;
  out->send_event = sendEvent;
  return view(out);
}

int& count(const std::shared_ptr<flight::types::Signal<std::function<void()>>>& signal) {
  static std::vector<std::unique_ptr<int>> counters;
  counters.push_back(std::make_unique<int>(0));
  int* counter = counters.back().get();
  flight::signals::connect_signal(signal, std::function<void()>([counter] { ++*counter; }), std::nullopt);
  return *counter;
}

bool contains(const flight::Array<TweenRef>& list, const TweenRef& tween) {
  for (const auto& entry : list) {
    if (entry == tween) return true;
  }
  return false;
}

void interpolation() {
  {
    auto m = tw::create_tween_manager(std::nullopt);
    auto target = xTarget(0);
    tween(m, target, 1000, 100, Opts{.linear = true});
    tw::update_tweens(m, 500);
    check(close(target->x, 50), "interpolates the property toward the target value");
    tw::update_tweens(m, 500);
    check(target->x == 100, "reaches the target value at the end of the duration");
  }
  {
    auto m = tw::create_tween_manager(std::nullopt);
    auto target = flight::make_ref<XY>();
    auto props = flight::make_ref<XY>(XY{.x = 100, .y = 200});
    auto t = tween(m, Any::object(target), 1000, Any::object(props), Opts{.linear = true});
    check(t->properties.size() == 2 && t->properties.element(0.0)->key == flight::String("x") &&
              t->properties.element(1.0)->key == flight::String("y"),
          "properties are pre-allocated from the property map, one per key");
    tw::update_tweens(m, 1000);
    check(target->x == 100 && target->y == 200, "multiple properties reach their own targets");
  }
  {
    auto m = tw::create_tween_manager(std::nullopt);
    auto target = xTarget(25);
    tween(m, target, 1000, 125, Opts{.linear = true});
    tw::update_tweens(m, 500);
    check(close(target->x, 75), "the start value is captured from the target on the first active update");
  }
  {
    auto m = tw::create_tween_manager(std::nullopt);
    check(m->default_ease(0) == 0 && close(m->default_ease(1), 1), "the default ease is easeOutExponential: 0 at 0, 1 at 1");
    auto target = xTarget(0);
    tween(m, target, 1000, 100);
    tw::update_tweens(m, 500);
    check(target->x > 90 && target->x < 100, "tweens without an ease use the manager's default (ease-out, ahead of linear)");
  }
  {
    auto options = flight::make_ref<flight::types::TweenManagerOptions>();
    options->default_ease = linear;
    auto m = tw::create_tween_manager(view(options));
    auto target = xTarget(0);
    tween(m, target, 1000, 100);
    tw::update_tweens(m, 500);
    check(close(target->x, 50), "a manager's defaultEase applies to tweens that omit ease");
    auto other = xTarget(0);
    tween(m, other, 1000, 100, Opts{.ease = [](double) { return 1.0; }});
    tw::update_tweens(m, 1);
    check(other->x == 100, "an explicit ease overrides the manager's default");
  }
  {
    auto m = tw::create_tween_manager(std::nullopt);
    auto target = xTarget(0);
    tween(m, target, 1000, 100, Opts{.linear = true, .snapping = true});
    tw::update_tweens(m, 333);
    check(target->x == 33, "snapping rounds every written value");
  }
  {
    auto m = tw::create_tween_manager(std::nullopt);
    auto target = flight::make_ref<Rotation>();
    auto to = flight::make_ref<Rotation>(Rotation{.rotation = 350});
    tween(m, Any::object(target), 1000, Any::object(to), Opts{.linear = true, .smartRotation = true});
    tw::update_tweens(m, 500);
    check(close(target->rotation, -5), "smart rotation takes the short path for a large positive delta");
  }
  {
    auto m = tw::create_tween_manager(std::nullopt);
    auto target = flight::make_ref<Rotation>();
    auto to = flight::make_ref<Rotation>(Rotation{.rotation = -350});
    tween(m, Any::object(target), 1000, Any::object(to), Opts{.linear = true, .smartRotation = true});
    tw::update_tweens(m, 500);
    check(close(target->rotation, 5), "and for a large negative delta");
  }
  {
    auto m = tw::create_tween_manager(std::nullopt);
    auto target = flight::make_ref<Rotation>(Rotation{.rotation = 350});
    auto to = flight::make_ref<Rotation>(Rotation{.rotation = 10});
    tween(m, Any::object(target), 1000, Any::object(to), Opts{.linear = true, .smartRotation = true});
    tw::update_tweens(m, 500);
    check(close(target->rotation, 360), "from a non-zero start, the short path goes forward");
    tw::update_tweens(m, 500);
    check(close(target->rotation, 370), "and ends at start plus the short change, not at the requested angle");
  }
  {
    auto m = tw::create_tween_manager(std::nullopt);
    auto target = flight::make_ref<Rotation>();
    auto to = flight::make_ref<Rotation>(Rotation{.rotation = 90});
    tween(m, Any::object(target), 1000, Any::object(to), Opts{.linear = true, .smartRotation = true});
    tw::update_tweens(m, 500);
    check(close(target->rotation, 45), "small deltas are unchanged by smart rotation");
  }
  {
    auto m = tw::create_tween_manager(std::nullopt);
    auto target = xTarget(0);
    auto t = tween(m, target, 0, 100, Opts{.linear = true});
    tw::update_tweens(m, 1);
    check(target->x == 100 && t->complete, "a zero duration completes at the end value without NaN");
  }
  {
    auto m = tw::create_tween_manager(std::nullopt);
    auto target = xTarget(0);
    tween(m, target, 1000, 100, Opts{.linear = true});
    tw::update_tweens(m, 500);
    tw::update_tweens(m, -200);
    check(std::isfinite(target->x) && close(target->x, 30), "a negative delta moves elapsed back without NaN");
  }
}

void delay_repeat_yoyo() {
  {
    auto m = tw::create_tween_manager(std::nullopt);
    auto target = xTarget(0);
    tween(m, target, 1000, 100, Opts{.delay = 500, .linear = true});
    tw::update_tweens(m, 500);
    check(target->x == 0, "nothing is written during the delay");
    tw::update_tweens(m, 500);
    check(close(target->x, 50), "the tween starts once the delay has elapsed");
  }
  {
    auto m = tw::create_tween_manager(std::nullopt);
    auto target = xTarget(0);
    auto t = tween(m, target, 1000, 100, Opts{.linear = true, .repeat = 2});
    auto& repeats = count(t->on_repeat);
    auto& completes = count(t->on_complete);
    tw::update_tweens(m, 1000);
    tw::update_tweens(m, 1000);
    check(repeats == 2 && !t->complete && t->repeat == 0, "each finished cycle spends one repeat and fires onRepeat");
    tw::update_tweens(m, 1000);
    check(repeats == 2 && t->complete && completes == 1, "the final cycle completes once the budget is spent");
  }
  {
    auto m = tw::create_tween_manager(std::nullopt);
    auto target = xTarget(0);
    auto t = tween(m, target, 1000, 100, Opts{.linear = true, .repeat = -1});
    auto& repeats = count(t->on_repeat);
    tw::update_tweens(m, 1000);
    tw::update_tweens(m, 1000);
    tw::update_tweens(m, 1000);
    check(repeats == 3 && !t->complete && t->repeat == -1, "repeat -1 loops without completing or spending");
  }
  {
    auto m = tw::create_tween_manager(std::nullopt);
    auto target = xTarget(0);
    tween(m, target, 1000, 100, Opts{.linear = true, .reverse = true});
    tw::update_tweens(m, 100);
    check(close(target->x, 90), "reverse plays from the end value back toward the start");
    tw::update_tweens(m, 900);
    check(target->x == 0, "and lands on the start");
  }
  {
    auto m = tw::create_tween_manager(std::nullopt);
    auto target = xTarget(0);
    auto t = tween(m, target, 1000, 100, Opts{.linear = true, .reflect = true, .repeat = 1});
    auto& yoyos = count(t->on_yoyo);
    tw::update_tweens(m, 1000);
    check(target->x == 100 && yoyos == 1 && t->reverse, "reflect flips direction at the end of a cycle and fires onYoyo");
    tw::update_tweens(m, 250);
    check(close(target->x, 75), "the reflected cycle plays backward");
    tw::update_tweens(m, 750);
    check(target->x == 0 && t->complete, "and arrives back at the start");
  }
  {
    auto m = tw::create_tween_manager(std::nullopt);
    auto target = xTarget(0);
    auto t = tween(m, target, 1000, 100, Opts{.linear = true, .reflect = true, .repeat = 3});
    auto& yoyos = count(t->on_yoyo);
    tw::update_tweens(m, 1000);
    tw::update_tweens(m, 1000);
    tw::update_tweens(m, 1000);
    check(yoyos == 3, "onYoyo fires on every reflected cycle");
  }
  {
    auto m = tw::create_tween_manager(std::nullopt);
    auto target = xTarget(0);
    auto t = tween(m, target, 1000, 100, Opts{.linear = true, .repeat = 1});
    auto& yoyos = count(t->on_yoyo);
    tw::update_tweens(m, 1000);
    tw::update_tweens(m, 1000);
    check(yoyos == 0 && !t->reverse, "without reflect there is no yoyo and no direction change");
  }
}

void settlement() {
  {
    auto m = tw::create_tween_manager(std::nullopt);
    auto target = xTarget(0);
    auto t = tween(m, target, 1000, 100);
    auto& completes = count(t->on_complete);
    auto& updates = count(t->on_update);
    tw::update_tweens(m, 250);
    tw::update_tweens(m, 250);
    check(updates == 2 && completes == 0, "onUpdate fires on each active tick");
    tw::update_tweens(m, 500);
    check(t->complete && completes == 1 && updates == 3, "the end of the duration completes and fires onComplete once");
    tw::update_tweens(m, 500);
    check(completes == 1 && updates == 3, "a complete tween is never updated again");
  }
  {
    auto m = tw::create_tween_manager(std::nullopt);
    auto target = xTarget(25);
    auto t = tween(m, target, 1000, 100, Opts{.linear = true});
    auto& completes = count(t->on_complete);
    check(!t->initialized, "a tween is uninitialized before its first update");
    tw::complete_tween<Any>(t);
    check(target->x == 100 && t->complete && completes == 1, "completeTween initializes, jumps to the end, and fires onComplete");
    target->x = 0;
    tw::complete_tween<Any>(t);
    check(target->x == 0 && completes == 1, "and is a no-op on a complete tween");
  }
  {
    auto m = tw::create_tween_manager(std::nullopt);
    auto target = xTarget(0);
    auto t = tween(m, target, 1000, 100, Opts{.linear = true, .reverse = true});
    tw::complete_tween<Any>(t);
    check(target->x == 0, "completing a reversed tween lands on the start");
  }
  {
    auto m = tw::create_tween_manager(std::nullopt);
    auto target = xTarget(0);
    auto t = tween(m, target, 1000, 100, Opts{.linear = true});
    auto& completes = count(t->on_complete);
    tw::update_tweens(m, 500);
    tw::stop_tween(t);
    check(t->complete && close(target->x, 50) && completes == 0, "stopTween leaves the value where it is and fires nothing");
  }
  {
    auto m = tw::create_tween_manager(std::nullopt);
    auto target = xTarget(0);
    auto t = tween(m, target, 1000, 100, Opts{.linear = true});
    auto& completes = count(t->on_complete);
    tw::update_tweens(m, 500);
    tw::stop_tween(t, stopOptions(true, std::nullopt));
    check(target->x == 100 && completes == 1, "complete: true jumps to the end and fires onComplete by default");
  }
  {
    auto m = tw::create_tween_manager(std::nullopt);
    auto target = xTarget(0);
    auto t = tween(m, target, 1000, 100);
    auto& completes = count(t->on_complete);
    tw::stop_tween(t, stopOptions(true, false));
    check(target->x == 100 && completes == 0 && t->complete, "sendEvent: false still applies the end values but stays silent");
  }
  {
    auto m = tw::create_tween_manager(std::nullopt);
    auto target = xTarget(0);
    auto t = tween(m, target, 1000, 100, Opts{.linear = true});
    tw::update_tweens(m, 500);
    tw::stop_tweens(m, key(Any::object(target)));
    check(t->complete && close(target->x, 50), "stopTweens leaves the value where it is");
  }
  {
    auto m = tw::create_tween_manager(std::nullopt);
    auto target = xTarget(0);
    auto t = tween(m, target, 1000, 100, Opts{.linear = true});
    auto& completes = count(t->on_complete);
    tw::update_tweens(m, 500);
    tw::stop_tweens(m, key(Any::object(target)), std::nullopt, stopOptions(true, std::nullopt));
    check(target->x == 100 && completes == 1, "stopTweens complete: true jumps to the end and fires onComplete");
  }
  {
    auto m = tw::create_tween_manager(std::nullopt);
    auto target = xTarget(0);
    auto t = tween(m, target, 1000, 100);
    auto& completes = count(t->on_complete);
    tw::stop_tweens(m, key(Any::object(target)), std::nullopt, stopOptions(true, false));
    check(target->x == 100 && completes == 0, "stopTweens sendEvent: false applies the values silently");
  }
  {
    auto m = tw::create_tween_manager(std::nullopt);
    auto target = xTarget(0);
    auto t = tween(m, target, 1000, 100, Opts{.linear = true});
    tw::seek_tween(t, 500);
    check(close(target->x, 50), "seekTween applies the value at that time immediately");
    tw::seek_tween(t, 750);
    check(close(target->x, 75), "and again, consistently, reading start values before writing");
    tw::seek_tween(t, -100);
    check(t->elapsed == 0 && close(target->x, 75), "a negative time clamps elapsed to 0, where nothing is active to write");
    tw::seek_tween(t, 9999);
    check(target->x == 100 && t->complete, "a time past the end clamps to the end and completes");
  }
  {
    auto m = tw::create_tween_manager(std::nullopt);
    auto target = xTarget(0);
    auto t = tween(m, target, 1000, 100, Opts{.delay = 200, .linear = true});
    auto& completes = count(t->on_complete);
    auto& updates = count(t->on_update);
    tw::seek_tween(t, 700);
    check(close(target->x, 50) && updates == 1, "seek measures from before the delay and fires onUpdate");
    tw::seek_tween(t, 1199.999);
    check(completes == 0 && !t->complete, "seeking just before delay + duration does not complete");
    tw::seek_tween(t, 1200);
    check(completes == 1 && t->complete, "seeking to exactly delay + duration completes and fires onComplete");
    tw::seek_tween(t, 1200);
    check(completes == 1, "and seeking there again does not fire it twice");
  }
  {
    auto m = tw::create_tween_manager(std::nullopt);
    auto target = xTarget(0);
    auto t = tween(m, target, 1000, 100, Opts{.linear = true});
    auto& completes = count(t->on_complete);
    tw::set_tween_progress(t, 0.25);
    check(close(target->x, 25), "setTweenProgress applies the fraction of the range");
    tw::set_tween_progress(t, -1);
    check(t->elapsed == 0 && close(target->x, 25), "and clamps below 0 to the start of the delay, writing nothing");
    tw::set_tween_progress(t, 2);
    check(target->x == 100 && t->complete && completes == 1, "and clamps to 1, completing the tween");
  }
}

void ordering_and_identity() {
  {
    auto m = tw::create_tween_manager(std::nullopt);
    auto target = xTarget(0);
    auto boxed = Any::object(target);
    auto t = tween(m, target, 1000, 100);
    check(m->tweens.has(key(boxed)) && t->target.erased_ref() == key(boxed), "a tween is registered under its target object");
  }
  {
    auto m = tw::create_tween_manager(std::nullopt);
    auto target = xTarget(0);
    auto first = tween(m, target, 1000, 100);
    auto second = tween(m, target, 1000, 200);
    check(first->complete && !second->complete, "a new tween overwrites an existing tween of the same property");
  }
  {
    auto m = tw::create_tween_manager(std::nullopt);
    auto target = xTarget(0);
    auto first = tween(m, target, 1000, 100);
    tween(m, target, 1000, 200, Opts{.overwrite = false});
    check(!first->complete, "overwrite: false leaves it running");
  }
  {
    auto m = tw::create_tween_manager(std::nullopt);
    auto target = Any::object(flight::make_ref<XY>());
    auto first = tween(m, target, 1000, xProps(100));
    tween(m, target, 1000, yProps(200));
    check(!first->complete, "non-overlapping properties do not overwrite");
  }
  {
    auto m = tw::create_tween_manager(std::nullopt);
    auto target = Any::object(xTarget(0));
    tween(m, target, 1000, xProps(100));
    tween(m, Any::object(xTarget(0)), 1000, xProps(100));
    check(tw::get_active_tween_count(m) == 2 && m->tweens.size() == 2, "tweens of different targets are counted and keyed separately");
  }
  {
    auto m = tw::create_tween_manager(std::nullopt);
    auto target = Any::object(flight::make_ref<XY>());
    auto t1 = tween(m, target, 1000, xProps(100));
    auto t2 = tween(m, target, 1000, yProps(100), Opts{.overwrite = false});
    auto list = tw::get_tweens_of(m, key(target));
    check(list.size() == 2 && list.element(0.0) == t1 && list.element(1.0) == t2, "getTweensOf returns the target's tweens in creation order");
    check(tw::get_tweens_of(m, flight::ErasedRef(flight::make_ref<Empty>())).size() == 0 &&
              !tw::has_tweens_of(m, flight::ErasedRef(flight::make_ref<Empty>())),
          "an unknown target has no tweens");
    check(tw::has_tweens_of(m, key(target)), "hasTweensOf answers for a registered target");
  }
  {
    auto m = tw::create_tween_manager(std::nullopt);
    auto a = Any::object(xTarget(0));
    auto b = Any::object(xTarget(0));
    tween(m, a, 1000, xProps(100));
    tween(m, b, 500, xProps(100));
    tw::update_tweens(m, 500);
    check(tw::get_active_tween_count(m) == 2 && tw::has_tweens_of(m, key(b)), "a completed tween stays registered until the next pass");
    tw::update_tweens(m, 0);
    check(tw::get_active_tween_count(m) == 1 && !tw::has_tweens_of(m, key(b)) && tw::has_tweens_of(m, key(a)),
          "the next pass removes it, and its emptied target, without disturbing the others");
    tw::update_tweens(m, 500);
    tw::update_tweens(m, 0);
    check(tw::get_active_tween_count(m) == 0 && m->tweens.size() == 0, "and the manager empties once every tween is gone");
  }
  {
    auto m = tw::create_tween_manager(std::nullopt);
    auto first = xTarget(0);
    auto second = xTarget(0);
    auto third = xTarget(0);
    tween(m, first, 100, 100, Opts{.linear = true});
    tween(m, second, 1000, 100, Opts{.linear = true});
    tween(m, third, 1000, 100, Opts{.linear = true});
    tw::update_tweens(m, 100);
    tw::update_tweens(m, 100);
    check(!tw::has_tweens_of(m, key(Any::object(first))) && close(second->x, 20) && close(third->x, 20),
          "deleting an emptied target mid-pass does not skip or repeat the targets after it");
  }
  {
    auto m = tw::create_tween_manager(std::nullopt);
    auto target = Any::object(flight::make_ref<XY>());
    auto a = tween(m, target, 1000, xProps(100));
    auto b = tween(m, target, 1000, yProps(100), Opts{.overwrite = false});
    auto c = tween(m, Any::object(xTarget(0)), 1000, xProps(200));
    tw::kill_tweens_of_property(m, flight::String("x"));
    check(a->complete && !b->complete && c->complete, "killTweensOfProperty completes every tween of that property, on every target");
    tw::kill_tweens_of_property(m, flight::String("z"));
    check(!b->complete, "and is a no-op for a property no tween has");
  }
  {
    auto m = tw::create_tween_manager(std::nullopt);
    auto target = Any::object(flight::make_ref<XY>());
    auto a = tween(m, target, 1000, xProps(100));
    auto b = tween(m, target, 1000, yProps(100), Opts{.overwrite = false});
    tw::stop_tweens(m, key(target), xProps(0));
    check(a->complete && !b->complete, "stopTweens with a property map stops only overlapping tweens");
    tw::stop_tweens(m, key(target));
    check(b->complete, "and without one stops them all");
  }
  {
    auto m = tw::create_tween_manager(std::nullopt);
    auto target = xTarget(0);
    auto t = tween(m, target, 1000, 100);
    tw::apply_tween<Any>(m, Any::object(target), xProps(50));
    check(t->complete && target->x == 50, "applyTween stops conflicting tweens and writes the values immediately");
  }
}

void lifecycle() {
  {
    auto m = tw::create_tween_manager(std::nullopt);
    auto t = tween(m, xTarget(0), 1000, 100);
    check(!t->complete && t->elapsed == 0 && !t->paused && t->repeat == 0 && !t->reflect && !t->reverse && !t->smart_rotation &&
              !t->snapping && t->delay == 0 && t->duration == 1000,
          "createTween defaults");
    check(m->tweens.size() == 1 && tw::create_tween_manager(std::nullopt) != m, "each manager is its own instance and starts empty");
  }
  {
    auto m = tw::create_tween_manager(std::nullopt);
    auto target = xTarget(0);
    auto t = tween(m, target, 1000, 100, Opts{.linear = true});
    tw::pause_tween(t);
    tw::update_tweens(m, 500);
    check(t->paused && target->x == 0 && t->elapsed == 0, "a paused tween neither writes nor accumulates elapsed time");
    tw::resume_tween(t);
    tw::update_tweens(m, 500);
    check(!t->paused && close(target->x, 50), "resuming continues from where it paused");
  }
  {
    auto m = tw::create_tween_manager(std::nullopt);
    auto target = Any::object(xTarget(0));
    auto other = tween(m, Any::object(xTarget(0)), 1000, xProps(100));
    auto t = tween(m, target, 1000, xProps(100));
    tw::pause_tweens(m, key(target));
    check(t->paused && !other->paused, "pauseTweens pauses only that target's tweens");
    tw::resume_tweens(m, key(target));
    check(!t->paused, "resumeTweens resumes them");
    tw::pause_all_tweens(m);
    check(t->paused && other->paused, "pauseAllTweens pauses every tween");
    tw::resume_all_tweens(m);
    check(!t->paused && !other->paused, "resumeAllTweens resumes every tween");
    tw::stop_all_tweens(m);
    check(t->complete && other->complete, "stopAllTweens completes every tween");
  }
  {
    auto m = tw::create_tween_manager(std::nullopt);
    auto a = tween(m, xTarget(0), 1000, 100);
    tw::reset_all_tweens(m);
    check(m->tweens.size() == 0 && !a->complete, "resetAllTweens forgets every tween without completing it");
  }
  {
    auto m = tw::create_tween_manager(std::nullopt);
    auto t = tween(m, xTarget(0), 1000, 100, Opts{.delay = 500, .linear = true});
    check(tw::get_tween_progress(t) == 0, "progress is 0 before any update");
    tw::update_tweens(m, 400);
    check(tw::get_tween_progress(t) == 0, "and during the delay");
    tw::update_tweens(m, 600);
    check(close(tw::get_tween_progress(t), 0.5), "and the active fraction after it");
    tw::update_tweens(m, 500);
    check(tw::get_tween_progress(t) == 1 && t->complete, "and 1 once complete");
    tw::restart_tween(t);
    check(!t->complete && !t->initialized && t->elapsed == 0, "restartTween rewinds, keeping the delay");
    tw::restart_tween(t, false);
    check(t->elapsed == 500, "or skipping it");
    tw::update_tweens(m, 100);
    tw::invalidate_tween(t);
    check(!t->initialized && !t->complete && t->elapsed == 0, "invalidateTween drops captured state");
  }
  {
    auto m = tw::create_tween_manager(std::nullopt);
    auto target = xTarget(0);
    auto boxed = Any::object(target);
    auto t = tween(m, target, 1000, 100, Opts{.linear = true});
    tw::update_tweens(m, 1000);
    tw::invalidate_tween(t);
    target->x = 40;
    check(contains(tw::get_tweens_of(m, key(boxed)), t), "a completed tween is still registered until the next pass");
    tw::update_tweens(m, 500);
    check(close(target->x, 70), "after invalidating, start values are re-captured from the target's current state");
  }
  {
    auto target = xTarget(0);
    auto boxed = Any::object(target);
    auto t = tw::create_tween<Any>(std::variant<Ref<TweenManager>, Any>{std::in_place_type<Any>, boxed},
                                   std::variant<Any, double>{std::in_place_type<double>, 1000},
                                   std::variant<Any, double>{std::in_place_type<Any>, xProps(100)},
                                   PropsOrOptions{std::in_place_index<1>, *options(Opts{.linear = true})});
    check(t->target.erased_ref() == key(boxed) && tw::has_tweens_of(tw::default_manager, key(boxed)),
          "without a manager, createTween registers the target with the default manager");
    tw::update_tweens(tw::default_manager, 500);
    check(close(target->x, 50), "and the options in third position apply");
    tw::reset_all_tweens(tw::default_manager);
  }
  {
    auto m = tw::create_tween_manager(std::nullopt);
    auto t1 = tw::create_tween_timer(m, 1000);
    auto t2 = tw::create_tween_timer(m, 1000);
    check(!(t1->target.erased_ref() == t2->target.erased_ref()) && m->tweens.size() == 2 && t1->properties.size() == 0,
          "each timer has its own sentinel target and tweens nothing");
    auto& fired = count(t1->on_complete);
    tw::update_tweens(m, 500);
    check(fired == 0, "a timer does not fire before its duration");
    tw::update_tweens(m, 500);
    check(fired == 1, "and fires onComplete at its duration");
  }
  {
    auto m = tw::create_tween_manager(std::nullopt);
    auto delayed = tw::create_tween_timer(m, 1000, options(Opts{.delay = 500}));
    auto& fired = count(delayed->on_complete);
    tw::update_tweens(m, 1000);
    check(fired == 0, "a timer's delay postpones it");
    tw::update_tweens(m, 500);
    check(fired == 1, "by exactly the delay");
    auto looping = tw::create_tween_timer(m, 1000, options(Opts{.repeat = -1}));
    auto& ticks = count(looping->on_repeat);
    tw::update_tweens(m, 1000);
    tw::update_tweens(m, 1000);
    tw::update_tweens(m, 1000);
    check(ticks == 3 && !looping->complete, "a repeating timer ticks onRepeat and keeps running");
  }
}

std::optional<flight::StructuralRef<flight::RowReadonly<flight::RowOf<Ref<flight::types::TweenStaggerOptions>>>>> stagger(
    std::optional<double> each, std::optional<std::variant<double, flight::String>> from) {
  auto out = flight::make_ref<flight::types::TweenStaggerOptions>();
  out->each = each;
  out->from = from;
  return view(out);
}

void staggers() {
  using From = std::variant<double, flight::String>;
  auto targets = [](int n, std::vector<Ref<X>>& refs) {
    flight::Array<Any> list;
    for (int i = 0; i < n; ++i) {
      refs.push_back(xTarget(0));
      list.push(Any::object(refs.back()));
    }
    return list;
  };
  {
    auto m = tw::create_tween_manager(std::nullopt);
    check(tw::create_tween_stagger<Any>(m, flight::Array<Any>{}, 1000, Any::object(flight::make_ref<Empty>())).size() == 0,
          "no targets, no tweens");
  }
  {
    auto m = tw::create_tween_manager(std::nullopt);
    std::vector<Ref<X>> refs;
    auto list = targets(3, refs);
    auto tweens = tw::create_tween_stagger<Any>(m, list, 2000, xProps(100), stagger(200, std::nullopt));
    check(tweens.size() == 3 && tweens.element(0.0)->target.erased_ref() == list.element(0.0).erased_ref() &&
              tweens.element(2.0)->target.erased_ref() == list.element(2.0).erased_ref(),
          "one tween per target, each targeting its own element");
    check(close(tweens.element(0.0)->delay, 0) && close(tweens.element(1.0)->delay, 200) && close(tweens.element(2.0)->delay, 400) &&
              tweens.element(1.0)->duration == 2000,
          "from start: delays step by each, and every tween gets the duration");
  }
  {
    auto m = tw::create_tween_manager(std::nullopt);
    std::vector<Ref<X>> refs;
    auto tweens = tw::create_tween_stagger<Any>(m, targets(3, refs), 1000, xProps(100), stagger(100, From{flight::String("end")}));
    check(close(tweens.element(2.0)->delay, 0) && close(tweens.element(1.0)->delay, 100) && close(tweens.element(0.0)->delay, 200),
          "from end reverses the order");
  }
  {
    auto m = tw::create_tween_manager(std::nullopt);
    std::vector<Ref<X>> refs;
    auto tweens = tw::create_tween_stagger<Any>(m, targets(5, refs), 1000, xProps(100), stagger(100, From{flight::String("center")}));
    check(close(tweens.element(2.0)->delay, 0) && close(tweens.element(1.0)->delay, 200) && close(tweens.element(0.0)->delay, 400) &&
              close(tweens.element(4.0)->delay, 400),
          "from center spreads outward from the middle");
  }
  {
    auto m = tw::create_tween_manager(std::nullopt);
    std::vector<Ref<X>> refs;
    auto tweens = tw::create_tween_stagger<Any>(m, targets(3, refs), 1000, xProps(100), stagger(100, From{0.0}));
    check(close(tweens.element(0.0)->delay, 0) && close(tweens.element(1.0)->delay, 100) && close(tweens.element(2.0)->delay, 200),
          "a numeric origin spreads outward from that index");
    std::vector<Ref<X>> more;
    auto middle = tw::create_tween_stagger<Any>(m, targets(5, more), 1000, xProps(100), stagger(100, From{3.0}));
    check(close(middle.element(3.0)->delay, 0) && close(middle.element(0.0)->delay, 400) && close(middle.element(4.0)->delay, 400.0 / 3),
          "normalizing by the farther side of the origin");
    std::vector<Ref<X>> clamped;
    auto past = tw::create_tween_stagger<Any>(m, targets(3, clamped), 1000, xProps(100), stagger(100, From{9.0}));
    check(close(past.element(2.0)->delay, 0) && close(past.element(0.0)->delay, 200), "an origin past the end clamps to the last index");
  }
  {
    auto m = tw::create_tween_manager(std::nullopt);
    std::vector<Ref<X>> refs;
    auto eased = flight::make_ref<flight::types::TweenStaggerOptions>();
    eased->each = 100;
    eased->stagger_ease = [](double t) { return t * t; };
    auto tweens = tw::create_tween_stagger<Any>(m, targets(3, refs), 1000, xProps(100), view(eased));
    check(close(tweens.element(1.0)->delay, 50) && close(tweens.element(2.0)->delay, 200), "staggerEase shapes the delay distribution");
  }
  {
    auto m = tw::create_tween_manager(std::nullopt);
    std::vector<Ref<X>> refs;
    auto tweens = tw::create_tween_stagger<Any>(m, targets(2, refs), 1000, xProps(100), stagger(100, std::nullopt), options(Opts{.delay = 500}));
    check(close(tweens.element(0.0)->delay, 500) && close(tweens.element(1.0)->delay, 600), "options.delay is added to every stagger delay");
  }
  {
    auto m = tw::create_tween_manager(std::nullopt);
    std::vector<Ref<X>> refs;
    tw::create_tween_stagger<Any>(m, targets(2, refs), 1000, xProps(100), stagger(200, std::nullopt), options(Opts{.linear = true}));
    tw::update_tweens(m, 100);
    check(close(refs[0]->x, 10) && refs[1]->x == 0, "staggered tweens animate on their own schedule");
  }
  {
    auto m = tw::create_tween_manager(std::nullopt);
    std::vector<Ref<X>> refs;
    auto tweens = tw::create_tween_stagger<Any>(m, targets(1, refs), 1000, xProps(100), stagger(100, std::nullopt));
    check(tweens.element(0.0)->delay == 0, "a single target has no stagger");
  }
}

flight::Record<flight::String, double> colorRecord(double color) {
  flight::Record<flight::String, double> record;
  record.set(flight::String("color"), color);
  return record;
}

double color(const flight::Record<flight::String, double>& record) { return record.get(flight::String("color")).value_or(-1); }

void color_tweens() {
  {
    auto m = tw::create_tween_manager(std::nullopt);
    auto target = colorRecord(0xff0000);
    tw::create_color_tween(m, target, flight::String("color"), 1000, 0x0000ff);
    check(color(target) == 0xff0000, "nothing is written before the first update");
    tw::update_tweens(m, 1000);
    check(color(target) == 0x0000ff, "the caller's record reaches the target colour");
  }
  {
    auto m = tw::create_tween_manager(std::nullopt);
    auto target = colorRecord(0x00ff00);
    tw::create_color_tween(m, target, flight::String("color"), 1000, 0xff0000, options(Opts{.linear = true}));
    tw::update_tweens(m, 1000);
    check(color(target) == 0xff0000, "starting from the record's current colour");
  }
  {
    auto m = tw::create_tween_manager(std::nullopt);
    auto target = colorRecord(0xff0000);
    tw::create_color_tween(m, target, flight::String("color"), 1000, 0x0000ff, options(Opts{.linear = true}));
    tw::update_tweens(m, 500);
    const double value = color(target);
    check(value == std::floor(value), "every written colour is an integer");
    check(value == 0x800080, "components interpolate independently and are rounded: r 128, g 0, b 128");
  }
  {
    auto m = tw::create_tween_manager(std::nullopt);
    auto target = colorRecord(0x000000);
    tw::create_color_tween(m, target, flight::String("color"), 1000, 0xffffff, options(Opts{.linear = true}));
    tw::update_tweens(m, 250);
    check(color(target) == 0x404040, "each channel is written in its own byte");
  }
  {
    auto m = tw::create_tween_manager(std::nullopt);
    flight::Record<flight::String, double> empty;
    tw::create_color_tween(m, empty, flight::String("color"), 1000, 0x0000ff, options(Opts{.linear = true}));
    tw::update_tweens(m, 1000);
    check(color(empty) == 0x0000ff, "an absent property starts from 0 and is added by the write");
  }
  {
    auto m = tw::create_tween_manager(std::nullopt);
    auto target = colorRecord(0xff0000);
    auto t = tw::create_color_tween(m, target, flight::String("color"), 1000, 0x0000ff);
    check(contains(tw::get_tweens_of(m, target.erased_ref()), t) && tw::has_tweens_of(m, target.erased_ref()),
          "the tween is registered against the user's record, not its component object");
    tw::stop_tweens(m, target.erased_ref());
    check(t->complete, "so stopping the record stops it");
  }
  {
    auto m = tw::create_tween_manager(std::nullopt);
    auto target = colorRecord(0xff0000);
    auto boxed = Any::external(target);
    auto numeric = flight::make_ref<flight::types::Tween<Any>>();
    numeric = tween(m, boxed, 1000, Any::external(colorRecord(0x00ff00)));
    auto colorTween = tw::create_color_tween(m, target, flight::String("color"), 1000, 0x0000ff);
    check(numeric->complete && !colorTween->complete, "a colour tween overwrites a numeric tween of the same record property");
    tw::stop_tweens(m, target.erased_ref(), Any::external(colorRecord(0)));
    check(colorTween->complete, "a targeted stop by that property reaches the colour tween");
    auto next = tw::create_color_tween(m, target, flight::String("color"), 1000, 0xffffff);
    tw::kill_tweens_of_property(m, flight::String("color"));
    check(next->complete, "and killTweensOfProperty does too");
  }
  {
    auto m = tw::create_tween_manager(std::nullopt);
    auto target = colorRecord(10);
    tween(m, Any::external(target), 1000, Any::external(colorRecord(110)), Opts{.linear = true});
    tw::update_tweens(m, 500);
    check(close(color(target), 60), "a plain tween writes through to a record target");
  }
}

// A struct target cannot grow a property the way a JavaScript object does. The tween reports that rather
// than dropping the write, and a record target, which can grow, takes it.
void undeclared_properties() {
  {
    auto m = tw::create_tween_manager(std::nullopt);
    auto target = xTarget(0);
    tween(m, Any::object(target), 1000, yProps(100), Opts{.linear = true});
    bool threw = false;
    try {
      tw::update_tweens(m, 500);
    } catch (const flight::TypeError&) {
      threw = true;
    }
    check(threw && target->x == 0, "writing a key the target object does not declare throws instead of being dropped");
  }
  {
    auto m = tw::create_tween_manager(std::nullopt);
    flight::Record<flight::String, double> target;
    tween(m, Any::external(target), 1000, Any::external(colorRecord(100)), Opts{.linear = true});
    tw::update_tweens(m, 500);
    check(close(color(target), 50), "a record target gains the key, starting from 0");
  }
}

}  // namespace

int main() {
  interpolation();
  delay_repeat_yoyo();
  settlement();
  ordering_and_identity();
  lifecycle();
  staggers();
  color_tweens();
  undeclared_properties();
  return failures == 0 ? 0 : 1;
}
