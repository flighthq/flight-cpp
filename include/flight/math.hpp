#pragma once

#include <cmath>
#include <concepts>
#include <cstdint>
#include <iterator>
#include <limits>

namespace flight {

namespace detail {

inline std::uint32_t number_to_uint32(double value) noexcept {
  if (!std::isfinite(value) || value == 0.0) return 0;

  constexpr double modulus = 4294967296.0;
  double remainder = std::fmod(std::trunc(value), modulus);
  if (remainder < 0.0) remainder += modulus;
  return static_cast<std::uint32_t>(remainder);
}

inline double int32_bits_to_number(std::uint32_t value) noexcept {
  constexpr std::uint32_t sign_bit = 0x80000000U;
  constexpr std::int64_t modulus = 0x100000000LL;
  return value < sign_bit ? static_cast<double>(value)
                          : static_cast<double>(static_cast<std::int64_t>(value) - modulus);
}

inline std::uint32_t shift_count(double value) noexcept {
  return number_to_uint32(value) & 31U;
}

} // namespace detail

inline constexpr double e = 2.718281828459045235360287471352662498;
inline constexpr double pi = 3.141592653589793238462643383279502884;

inline double power(double base, double exponent) noexcept {
  return std::pow(base, exponent);
}

inline double minimum(double left, double right) noexcept {
  if (std::isnan(left) || std::isnan(right)) return std::numeric_limits<double>::quiet_NaN();
  if (left == 0.0 && right == 0.0) return std::signbit(left) || std::signbit(right) ? -0.0 : 0.0;
  return left < right ? left : right;
}

template <typename... Values>
  requires(sizeof...(Values) > 0 && (std::convertible_to<Values, double> && ...))
inline double minimum(double first, double second, Values... remaining) noexcept {
  double result = minimum(first, second);
  ((result = minimum(result, static_cast<double>(remaining))), ...);
  return result;
}

inline double maximum(double left, double right) noexcept {
  if (std::isnan(left) || std::isnan(right)) return std::numeric_limits<double>::quiet_NaN();
  if (left == 0.0 && right == 0.0) return std::signbit(left) && std::signbit(right) ? -0.0 : 0.0;
  return left > right ? left : right;
}

template <typename... Values>
  requires(sizeof...(Values) > 0 && (std::convertible_to<Values, double> && ...))
inline double maximum(double first, double second, Values... remaining) noexcept {
  double result = maximum(first, second);
  ((result = maximum(result, static_cast<double>(remaining))), ...);
  return result;
}

template <typename Range>
  requires requires(const Range& values) {
    std::begin(values);
    std::end(values);
  }
inline double minimum(const Range& values) {
  double result = std::numeric_limits<double>::infinity();
  for (const auto& value : values) result = minimum(result, static_cast<double>(value));
  return result;
}

template <typename Range>
  requires requires(const Range& values) {
    std::begin(values);
    std::end(values);
  }
inline double maximum(const Range& values) {
  double result = -std::numeric_limits<double>::infinity();
  for (const auto& value : values) result = maximum(result, static_cast<double>(value));
  return result;
}

inline bool is_integer(double value) noexcept {
  return std::isfinite(value) && std::trunc(value) == value;
}

// ECMAScript Math.fround returns the nearest IEEE-754 binary32 value as a Number. The supported
// native toolchains use IEC 559 floats; handle overflow explicitly so the conversion never depends
// on out-of-range C++ floating-conversion behavior.
inline double fround(double value) noexcept {
  if (!std::isfinite(value) || value == 0.0) return value;
  constexpr double maximum = static_cast<double>(std::numeric_limits<float>::max());
  constexpr double overflow_threshold = maximum + 0x1p103;
  if (std::abs(value) >= overflow_threshold) {
    return std::copysign(std::numeric_limits<double>::infinity(), value);
  }
  if (std::abs(value) > maximum) return std::copysign(maximum, value);
  return static_cast<double>(static_cast<float>(value));
}

inline double bitwise_and(double left, double right) noexcept {
  return detail::int32_bits_to_number(detail::number_to_uint32(left) & detail::number_to_uint32(right));
}

inline double bitwise_or(double left, double right) noexcept {
  return detail::int32_bits_to_number(detail::number_to_uint32(left) | detail::number_to_uint32(right));
}

inline double bitwise_xor(double left, double right) noexcept {
  return detail::int32_bits_to_number(detail::number_to_uint32(left) ^ detail::number_to_uint32(right));
}

inline double bitwise_not(double value) noexcept {
  return detail::int32_bits_to_number(~detail::number_to_uint32(value));
}

inline double left_shift(double value, double count) noexcept {
  const auto result = detail::number_to_uint32(value) << detail::shift_count(count);
  return detail::int32_bits_to_number(result);
}

inline double signed_right_shift(double value, double count) noexcept {
  const auto bits = detail::number_to_uint32(value);
  const auto width = detail::shift_count(count);
  if (width == 0) return detail::int32_bits_to_number(bits);

  auto result = bits >> width;
  if ((bits & 0x80000000U) != 0) result |= ~std::uint32_t{0} << (32U - width);
  return detail::int32_bits_to_number(result);
}

inline double unsigned_right_shift(double value, double count) noexcept {
  return static_cast<double>(detail::number_to_uint32(value) >> detail::shift_count(count));
}

inline double round(double value) noexcept {
  if (!std::isfinite(value) || value == 0.0) return value;
  if (value < 0.0 && value >= -0.5) return -0.0;

  const double lower = std::floor(value);
  return value - lower < 0.5 ? lower : lower + 1.0;
}

inline double sign(double value) noexcept {
  if (value == 0.0 || std::isnan(value)) return value;
  return value < 0.0 ? -1.0 : 1.0;
}

} // namespace flight
