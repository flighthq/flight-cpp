#pragma once

#include <chrono>

namespace flight::host {

struct Performance final {};

inline constexpr Performance performance;

[[nodiscard]] inline double performance_now() noexcept {
  using Clock = std::chrono::steady_clock;
  static const auto origin = Clock::now();
  return std::chrono::duration<double, std::milli>(Clock::now() - origin).count();
}

} // namespace flight::host
