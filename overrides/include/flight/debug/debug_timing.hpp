// Hand-written timing boundary for the log module's refused endLogTimer implementation.
#pragma once

#include <functional>
#include <optional>
#include <type_traits>
#include <utility>

#include <flight/runtime.hpp>
#include <flight/debug/debug.hpp>
#include <flight/log/log.hpp>
#include <flight/types/log.hpp>

namespace flight::debug {

inline std::optional<flight::Ref<flight::types::LogTimer>> begin_debug_span(
    flight::String name,
    std::optional<std::optional<flight::String>> channel = std::nullopt) {
  channel = channel.value_or(std::nullopt);
  if (!is_debug_enabled()) return std::nullopt;
  return flight::log::start_log_timer(std::move(name), channel.value());
}

inline double end_debug_span(
    std::optional<flight::Ref<flight::types::LogTimer>> timer) {
  return timer.has_value() ? flight::log::end_log_timer(timer.value()) : -1.0;
}

template <typename T>
inline T measure_debug_span(
    flight::String name,
    std::function<T()> fn,
    std::optional<std::optional<flight::String>> channel = std::nullopt) {
  channel = channel.value_or(std::nullopt);
  auto timer = begin_debug_span(std::move(name), channel.value());
  try {
    if constexpr (std::is_void_v<T>) {
      fn();
      end_debug_span(timer);
      return;
    } else {
      T result = fn();
      end_debug_span(timer);
      return result;
    }
  } catch (...) {
    end_debug_span(timer);
    throw;
  }
}

inline double debug_frame_number = 0.0;

inline void mark_debug_frame(
    std::optional<flight::String> label = std::nullopt,
    std::optional<std::optional<flight::String>> channel = std::nullopt) {
  channel = channel.value_or(std::nullopt);
  if (!is_debug_enabled()) return;
  const flight::Any frame = label.has_value()
      ? flight::Any(label.value())
      : flight::Any(++debug_frame_number);
  flight::log::log_debug(
      flight::Record<flight::String, flight::Any>{{flight::String("frame"), frame}},
      channel);
}

}  // namespace flight::debug
