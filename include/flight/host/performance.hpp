#pragma once

#include <chrono>

#include <flight/array.hpp>
#include <flight/json.hpp>
#include <flight/string.hpp>

namespace flight::host {

struct Performance final {};

struct PerformanceEntry {
  String name;
  String entry_type;
  double start_time = 0.0;
  double duration = 0.0;

  [[nodiscard]] JsonValue to_json() const {
    return JsonObject{
        {String("name"), name},
        {String("entryType"), entry_type},
        {String("startTime"), start_time},
        {String("duration"), duration},
    };
  }
};

struct PerformanceNavigationTiming final : PerformanceEntry {
  String type;
};

inline constexpr Performance performance;

[[nodiscard]] inline double performance_now() noexcept {
  using Clock = std::chrono::steady_clock;
  static const auto origin = Clock::now();
  return std::chrono::duration<double, std::milli>(Clock::now() - origin).count();
}

// A native process has no browser navigation history. The empty result makes Flight's Web
// lifecycle adapter choose its specified cold-launch fallback while a native lifecycle backend can
// still provide richer platform state directly.
[[nodiscard]] inline Array<PerformanceNavigationTiming> performance_get_entries_by_type(
    const String&) {
  return {};
}

} // namespace flight::host
