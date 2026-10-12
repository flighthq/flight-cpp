// Derived from @flighthq/signals/packages/signals/src/throttle.ts at a62784923f3be814463286c9fb28edfe6ab15789.
#pragma once

// OVERRIDE flight/signals/throttle.hpp (overrides/manifest.json: signals-throttle-typed-argument-buffers).
// Supplies connectSignalDebounced and connectSignalThrottled, which the emitter refused because their
// `any[]` argument buffer names a callable parameter pack as storage, and corrects two defects in the
// emitted connectSignalAtFrameRate: `elapsed %= period` was written as a C++ double remainder, and its
// handler was a bare lambda, whose identity std::function erasure loses so disconnect threw.
//
// Each buffer holds the slot's own parameter tuple, never an erased copy, and every handler is a
// flight::Function so the cleanup closure disconnects the exact callable it connected.

#include <cmath>
#include <flight/any.hpp>
#include <flight/callable.hpp>
#include <flight/date.hpp>
#include <flight/error.hpp>
#include <flight/host/timers.hpp>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <tuple>
#include <type_traits>
#include <flight/runtime.hpp>

static_assert(flight::runtime_contract.compiler_contract == "flight-runtime-contract/2", "Flight compiler/runtime contract mismatch");
static_assert(flight::runtime_contract.cpp_abi == 1, "Flight C++ runtime ABI mismatch");

#include <flight/types/signal.hpp>
#include <flight/types/signal_throttle_options.hpp>

#include <flight/signals/slot.hpp>

namespace flight::signals {

using flight::types::Signal;

using flight::types::SignalThrottleOptions;

using SignalThrottleOptionsArgument =
    std::optional<flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<SignalThrottleOptions>>>>>;

namespace detail {

// `options?.<key> ?? fallback`, as the emitter spells it for connectSignal's options.
template <typename Key>
inline bool signal_throttle_option(const SignalThrottleOptionsArgument& options, bool fallback) {
  if (!options.has_value()) return fallback;
  const std::optional<bool> value = flight::row_get<Key>(options.value());
  return value.has_value() ? value.value() : fallback;
}

} // namespace detail

inline std::function<void()> connect_signal_at_frame_rate(std::shared_ptr<Signal<std::function<void(double)>>> source, double fps, std::function<void(double)> slot) {
  const double period = (1000.0 / fps);
  auto elapsed = std::make_shared<double>(0.0);
  std::function<void(double)> handler = flight::Function<void(double)>([=](double delta) {
    *elapsed += delta;
    if (*elapsed >= period) {
      slot(*elapsed);
      // JavaScript `%` on numbers truncates toward zero and keeps the dividend's sign: std::fmod.
      *elapsed = std::fmod(*elapsed, period);
    }
  });
  connect_signal(source, handler, std::nullopt);
  return [=]() { return disconnect_signal(source, handler); };
}

template <typename... Args>
inline std::function<void()> connect_signal_debounced(
    std::shared_ptr<Signal<std::function<void(Args...)>>> source,
    double delay_ms,
    std::type_identity_t<std::function<void(Args...)>> slot,
    SignalThrottleOptionsArgument options = std::nullopt) {
  const bool leading = detail::signal_throttle_option<flight::RowKey<"leading">>(options, false);
  const bool trailing = detail::signal_throttle_option<flight::RowKey<"trailing">>(options, true);
  struct State {
    std::optional<flight::host::TimerHandle> timer;
    std::optional<std::tuple<Args...>> last_args;
    bool leading_fired = false;
  };
  auto state = std::make_shared<State>();
  const auto clear_timer = [state]() {
    if (state->timer.has_value()) {
      flight::host::clear_timeout(state->timer.value());
      state->timer.reset();
    }
  };
  std::function<void(Args...)> handler = flight::Function<void(Args...)>([=](Args... args) {
    state->last_args.emplace(args...);
    if (leading && !state->timer.has_value() && !state->leading_fired) {
      state->leading_fired = true;
      slot(args...);
    }
    clear_timer();
    state->timer = flight::host::set_timeout(
        [=]() {
          state->timer.reset();
          state->leading_fired = false;
          if (trailing && state->last_args.has_value()) {
            const std::tuple<Args...> last_args = state->last_args.value();
            std::apply(slot, last_args);
            state->last_args.reset();
          }
        },
        delay_ms);
  });
  connect_signal(source, handler, std::nullopt);
  return [=]() {
    disconnect_signal(source, handler);
    clear_timer();
  };
}

template <typename... Args>
inline std::function<void()> connect_signal_throttled(
    std::shared_ptr<Signal<std::function<void(Args...)>>> source,
    double interval_ms,
    std::type_identity_t<std::function<void(Args...)>> slot,
    SignalThrottleOptionsArgument options = std::nullopt) {
  const bool leading = detail::signal_throttle_option<flight::RowKey<"leading">>(options, true);
  const bool trailing = detail::signal_throttle_option<flight::RowKey<"trailing">>(options, true);
  struct State {
    double last_fired_at = -std::numeric_limits<double>::infinity();
    std::optional<flight::host::TimerHandle> trailing_timer;
    std::optional<std::tuple<Args...>> last_args;
  };
  auto state = std::make_shared<State>();
  const auto clear_trailing = [state]() {
    if (state->trailing_timer.has_value()) {
      flight::host::clear_timeout(state->trailing_timer.value());
      state->trailing_timer.reset();
    }
  };
  const auto schedule_trailing = [=](double delay) {
    state->trailing_timer = flight::host::set_timeout(
        [=]() {
          state->last_fired_at = flight::Date::now();
          state->trailing_timer.reset();
          // `slot(...lastArgs!)`: spreading a null buffer is a TypeError in JavaScript.
          if (!state->last_args.has_value()) throw flight::TypeError(flight::String("lastArgs is not iterable"));
          const std::tuple<Args...> last_args = state->last_args.value();
          std::apply(slot, last_args);
          state->last_args.reset();
        },
        delay);
  };
  std::function<void(Args...)> handler = flight::Function<void(Args...)>([=](Args... args) {
    const double now = flight::Date::now();
    const double remaining = interval_ms - (now - state->last_fired_at);
    if (remaining <= 0.0 || remaining > interval_ms) {
      clear_trailing();
      state->last_fired_at = now;
      if (leading) {
        slot(args...);
      } else if (trailing) {
        state->last_args.emplace(args...);
        schedule_trailing(interval_ms);
      }
    } else if (trailing) {
      clear_trailing();
      state->last_args.emplace(args...);
      schedule_trailing(remaining);
    }
  });
  connect_signal(source, handler, std::nullopt);
  return [=]() {
    disconnect_signal(source, handler);
    clear_trailing();
  };
}

} // namespace flight::signals
