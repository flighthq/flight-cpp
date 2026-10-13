// Re-derived from the generated tween update module at a627849. Every function is carried verbatim except the
// writes of tweened values onto the erased target, which go through the object's own row owner, and the walk
// over the manager's map, which uses its live cursor. See
// overrides/manifest.json (tween-update-erased-target-writes) for what changed, why, and what remains.
#pragma once
#include <cmath>
#include <cstdint>
#include <flight/any.hpp>
#include <flight/record.hpp>
#include <functional>
#include <random>
#include <tuple>
#include <flight/runtime.hpp>

static_assert(flight::runtime_contract.compiler_contract == "flight-runtime-contract/2", "Flight compiler/runtime contract mismatch");
static_assert(flight::runtime_contract.cpp_abi == 1, "Flight C++ runtime ABI mismatch");

namespace flight::types { struct TweenManager; }
namespace flight::types { struct TweenPropertyDetail; }
namespace flight::types { template <typename T> struct Signal; }
namespace flight::types { template <typename T> struct Tween; }

#include <flight/signals/emitter.hpp>
#include <flight/types/easing_function.hpp>
#include <flight/types/signal.hpp>
#include <flight/types/tween.hpp>
#include <flight/types/tween_manager.hpp>
#include <flight/types/tween_property_detail.hpp>
#include <flight/tween/_internal_internal.hpp>

namespace flight::tween {

using flight::types::Tween;

template <typename T>
inline void complete_tween(flight::Ref<flight::types::Tween<T>> tween) {
  if (tween->complete) {
    return;
  }
  if (!tween->initialized) {
    flight::tween::initialize_tween<T>(tween);
  }
  const double effective_t = (tween->reverse ? 0.0 : 1.0);
  const double eased_t = tween->ease(effective_t);
  const auto target = override_detail::numeric_record(tween->target);
  for (auto detail : tween->properties) {
    double value = (detail->start + (detail->change * eased_t));
    if (tween->snapping) {
      (value = flight::round(value));
    }
    target.write(detail->key, value);
  }
  (tween->complete = true);
  flight::signals::emit_signal<std::function<void()>>(tween->on_complete);
}

template <typename T>
inline void update_tween(flight::Ref<flight::types::Tween<T>> tween, double delta_time) {
  if ((tween->paused || tween->complete)) {
    return;
  }
  (tween->elapsed += delta_time);
  const double active_elapsed = (tween->elapsed - tween->delay);
  if ((active_elapsed <= 0.0)) {
    return;
  }
  if (!tween->initialized) {
    flight::tween::initialize_tween<T>(tween);
  }
  const double t = flight::minimum((active_elapsed / tween->duration), 1.0);
  const double effective_t = (tween->reverse ? (1.0 - t) : t);
  const double eased_t = tween->ease(effective_t);
  const auto target = override_detail::numeric_record(tween->target);
  for (auto detail : tween->properties) {
    double value = (detail->start + (detail->change * eased_t));
    if (tween->snapping) {
      (value = flight::round(value));
    }
    target.write(detail->key, value);
  }
  flight::signals::emit_signal<std::function<void()>>(tween->on_update);
  if ((t >= 1.0)) {
    if ((tween->repeat == 0.0)) {
      (tween->complete = true);
      flight::signals::emit_signal<std::function<void()>>(tween->on_complete);
    }
    else {
      if (tween->reflect) {
        (tween->reverse = !tween->reverse);
        flight::signals::emit_signal<std::function<void()>>(tween->on_yoyo);
      }
      (tween->elapsed = tween->delay);
      if ((tween->repeat > 0.0)) {
        tween->repeat--;
      }
      flight::signals::emit_signal<std::function<void()>>(tween->on_repeat);
    }
  }
}

// OVERRIDE: the loop walks the map through its live `entries()` cursor rather than a range-for over the
// map's storage (docs/compiler-requests/map-for-of-live-iteration.md). The body deletes the entry it is
// visiting once that target's list empties, which a JavaScript Map iteration allows -- the walk continues
// with the next entry -- and which invalidates a range-for over the records underneath it. The cursor
// visits entries in insertion order, skips any deleted before it reaches them, and also visits any a
// signal handler adds during the walk, as the source's `for...of` does.
inline void update_tweens(flight::Ref<flight::types::TweenManager> manager, double delta_time) {
  for (auto array_pattern_value : manager->tweens.entries()) {
    auto target = std::get<0>(array_pattern_value);
    flight::Array<flight::Ref<flight::types::Tween<flight::Any>>> list = std::get<1>(array_pattern_value);
    double i = (static_cast<double>(list.size()) - 1.0);
    while ((i >= 0.0)) {
      if (list.element(i)->complete) {
        list.splice(i, 1.0);
      }
      else {
        update_tween<flight::Any>(list.element(i), delta_time);
      }
      i--;
    }
    if ((static_cast<double>(list.size()) == 0.0)) {
      manager->tweens.erase(target);
    }
  }
}

} // namespace flight::tween
