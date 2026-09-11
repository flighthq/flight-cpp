#pragma once

#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>

#include <flight/math.hpp>
#include <flight/string.hpp>

namespace flight {

namespace detail {

inline bool number_whitespace(char value) noexcept {
  return value == ' ' || value == '\t' || value == '\n' || value == '\r' || value == '\f' || value == '\v';
}

inline std::string_view trim_number_text(std::string_view value) noexcept {
  while (!value.empty() && number_whitespace(value.front())) value.remove_prefix(1);
  while (!value.empty() && number_whitespace(value.back())) value.remove_suffix(1);
  return value;
}

inline int radix_digit(char value) noexcept {
  if (value >= '0' && value <= '9') return value - '0';
  if (value >= 'a' && value <= 'z') return value - 'a' + 10;
  if (value >= 'A' && value <= 'Z') return value - 'A' + 10;
  return -1;
}

inline double prefixed_integer(std::string_view value, int radix) noexcept {
  double result = 0.0;
  for (const char character : value) {
    const int digit = radix_digit(character);
    if (digit < 0 || digit >= radix) return std::numeric_limits<double>::quiet_NaN();
    result = result * static_cast<double>(radix) + static_cast<double>(digit);
  }
  return result;
}

} // namespace detail

inline double parse_int(const String& input, double radix_value = 0.0) {
  const std::string encoded = input.to_utf8();
  std::string_view text = detail::trim_number_text(encoded);
  bool negative = false;
  if (!text.empty() && (text.front() == '+' || text.front() == '-')) {
    negative = text.front() == '-';
    text.remove_prefix(1);
  }

  int radix = 0;
  if (std::isfinite(radix_value) && radix_value != 0.0) {
    constexpr double modulus = 4294967296.0;
    double normalized = std::fmod(std::trunc(radix_value), modulus);
    if (normalized < 0.0) normalized += modulus;
    const auto unsigned_radix = static_cast<std::uint32_t>(normalized);
    radix = unsigned_radix < 0x80000000U
                ? static_cast<int>(unsigned_radix)
                : static_cast<int>(static_cast<std::int64_t>(unsigned_radix) - 0x100000000LL);
  }
  if (radix != 0 && (radix < 2 || radix > 36)) {
    return std::numeric_limits<double>::quiet_NaN();
  }
  if ((radix == 0 || radix == 16) && text.size() >= 2 && text[0] == '0' &&
      (text[1] == 'x' || text[1] == 'X')) {
    radix = 16;
    text.remove_prefix(2);
  } else if (radix == 0) {
    radix = 10;
  }

  double result = 0.0;
  std::size_t digits = 0;
  for (const char character : text) {
    const int digit = detail::radix_digit(character);
    if (digit < 0 || digit >= radix) break;
    result = result * static_cast<double>(radix) + static_cast<double>(digit);
    ++digits;
  }
  if (digits == 0) return std::numeric_limits<double>::quiet_NaN();
  return negative ? -result : result;
}

inline double to_number(const String& input) {
  const std::string encoded = input.to_utf8();
  std::string_view text = detail::trim_number_text(encoded);
  if (text.empty()) return 0.0;
  if (text == "Infinity" || text == "+Infinity") return std::numeric_limits<double>::infinity();
  if (text == "-Infinity") return -std::numeric_limits<double>::infinity();

  bool has_sign = false;
  bool negative = false;
  if (text.front() == '+' || text.front() == '-') {
    has_sign = true;
    negative = text.front() == '-';
    text.remove_prefix(1);
  }
  if (text.size() >= 2 && text[0] == '0') {
    int radix = 0;
    if (text[1] == 'x' || text[1] == 'X') radix = 16;
    if (text[1] == 'b' || text[1] == 'B') radix = 2;
    if (text[1] == 'o' || text[1] == 'O') radix = 8;
    if (radix != 0) {
      if (has_sign || text.size() == 2) return std::numeric_limits<double>::quiet_NaN();
      const double parsed = detail::prefixed_integer(text.substr(2), radix);
      return parsed;
    }
  }

  if (negative) text = std::string_view(text.data() - 1, text.size() + 1);
  for (const char character : text) {
    if ((character >= 'A' && character <= 'Z' && character != 'E') ||
        (character >= 'a' && character <= 'z' && character != 'e')) {
      return std::numeric_limits<double>::quiet_NaN();
    }
  }
  double result = 0.0;
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), result, std::chars_format::general);
  return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size()
             ? result
             : std::numeric_limits<double>::quiet_NaN();
}

inline double to_number(double value) noexcept { return value; }
inline double to_number(bool value) noexcept { return value ? 1.0 : 0.0; }

} // namespace flight
