#include "generated/tween.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>

int main() {
  constexpr double progress = 0.35;
  constexpr std::size_t track_width = 36;
  constexpr std::array<std::string_view, 15> names{
      "easeInQuadratic",     "easeOutQuadratic",   "easeInOutQuadratic",
      "easeInCubic",         "easeOutCubic",       "easeInOutCubic",
      "easeInSine",          "easeOutSine",        "easeInOutSine",
      "easeInExponential",   "easeOutExponential", "easeInOutExponential",
      "easeInElastic",       "easeOutElastic",     "easeOutBounce",
  };

  const auto values = flighthq_examples_tween::sample_tween_curves(progress);
  const auto start_values = flighthq_examples_tween::sample_tween_curves(0.0);
  const auto end_values = flighthq_examples_tween::sample_tween_curves(1.0);
  if (values.size() != names.size() || start_values.size() != names.size() ||
      end_values.size() != names.size()) {
    std::cerr << "The generated tween curve count does not match the native host.\n";
    return 1;
  }
  for (std::size_t index = 0; index < names.size(); ++index) {
    if (std::abs(start_values[index]) > 1e-12 || std::abs(end_values[index] - 1.0) > 1e-12) {
      std::cerr << names[index] << " does not preserve the tween endpoints.\n";
      return 1;
    }
  }

  std::cout << "Flight tween example at t=" << progress << "\n\n";
  for (std::size_t index = 0; index < names.size(); ++index) {
    const double value = values[index];
    const double track_value = std::clamp(value, 0.0, 1.0);
    const auto position = static_cast<std::size_t>(
        std::lround(track_value * static_cast<double>(track_width)));
    std::string track(track_width + 1, '-');
    track[position] = 'o';
    std::cout << std::left << std::setw(22) << names[index] << " |" << track << "| "
              << std::right << std::fixed << std::setprecision(3) << value << '\n';
  }

  return 0;
}
