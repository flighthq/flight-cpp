#pragma once

#include <algorithm>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

#include <flight/math.hpp>
#include <flight/string.hpp>

namespace flight {

namespace detail {

inline bool number_whitespace(char value) noexcept {
  return value == ' ' || value == '\t' || value == '\n' || value == '\r' || value == '\f' || value == '\v';
}

inline bool number_whitespace(char16_t value) noexcept {
  return (value >= 0x0009 && value <= 0x000D) || value == 0x0020 || value == 0x00A0 ||
         value == 0x1680 || (value >= 0x2000 && value <= 0x200A) || value == 0x2028 ||
         value == 0x2029 || value == 0x202F || value == 0x205F || value == 0x3000 ||
         value == 0xFEFF;
}

inline std::string trim_leading_number_text(const String& input) {
  auto begin = input.begin();
  while (begin != input.end() && number_whitespace(*begin)) ++begin;
  return String(std::u16string(begin, input.end())).to_utf8();
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

inline int number_exponent(double value) noexcept {
  const auto stored = static_cast<int>((std::bit_cast<std::uint64_t>(value) >> 52) & 0x7FFU);
  return stored == 0 ? -1074 : stored - 1023 - 52;
}

inline char radix_character(int digit) noexcept {
  return digit < 10 ? static_cast<char>('0' + digit) : static_cast<char>('a' + digit - 10);
}

// ECMAScript's non-decimal Number string conversion is intentionally based on the input double's
// precision rather than an exact expansion of its rational value. Fractional digits stop at half
// the distance to the next representable double; large integer places beyond binary precision are
// emitted as zeroes. This also supplies the specified round-to-even carry behavior.
inline std::string finite_number_to_radix(double value, int radix) {
  double integer = std::floor(value);
  double fraction = value - integer;
  double delta = 0.5 * (std::nextafter(value, std::numeric_limits<double>::infinity()) - value);
  if (delta <= 0.0) delta = std::numeric_limits<double>::denorm_min();

  std::string fractional_digits;
  if (fraction >= delta) {
    do {
      fraction *= static_cast<double>(radix);
      delta *= static_cast<double>(radix);
      int digit = static_cast<int>(fraction);
      fractional_digits.push_back(radix_character(digit));
      fraction -= static_cast<double>(digit);
      if ((fraction > 0.5 || (fraction == 0.5 && (digit & 1) != 0)) &&
          fraction + delta > 1.0) {
        while (!fractional_digits.empty()) {
          const char character = fractional_digits.back();
          digit = character > '9' ? character - 'a' + 10 : character - '0';
          if (digit + 1 < radix) {
            fractional_digits.back() = radix_character(digit + 1);
            break;
          }
          fractional_digits.pop_back();
        }
        if (fractional_digits.empty()) integer += 1.0;
        break;
      }
    } while (fraction >= delta);
  }

  std::size_t zero_suffix = 0;
  while (number_exponent(integer / static_cast<double>(radix)) > 0) {
    integer /= static_cast<double>(radix);
    ++zero_suffix;
  }
  std::string integer_digits;
  do {
    const double remainder = std::fmod(integer, static_cast<double>(radix));
    integer_digits.push_back(radix_character(static_cast<int>(remainder)));
    integer = (integer - remainder) / static_cast<double>(radix);
  } while (integer > 0.0);
  std::reverse(integer_digits.begin(), integer_digits.end());
  integer_digits.append(zero_suffix, '0');
  if (!fractional_digits.empty()) {
    integer_digits.push_back('.');
    integer_digits.append(fractional_digits);
  }
  return integer_digits;
}

} // namespace detail

inline String number_to_string(double value, double radix_value = 10.0) {
  if (!std::isfinite(radix_value)) {
    throw std::range_error("flight::number_to_string radix must be between 2 and 36");
  }
  const auto radix = static_cast<int>(std::trunc(radix_value));
  if (radix < 2 || radix > 36) {
    throw std::range_error("flight::number_to_string radix must be between 2 and 36");
  }
  if (radix == 10 || !std::isfinite(value) || value == 0.0) return String::from_number(value);

  const bool negative = value < 0.0;
  const double magnitude = negative ? -value : value;
  std::string result = detail::finite_number_to_radix(magnitude, radix);
  if (negative) result.insert(result.begin(), '-');
  return String::from_utf8(result);
}

// Number.prototype.toFixed. The rounding is done on the double's exact decimal expansion rather
// than by asking printf for the final precision, because the two disagree: printf rounds a tie to
// even, and ECMAScript rounds it away from zero. `(2.5).toFixed(0)` is "3" here, as it is in
// JavaScript, and `(1.005).toFixed(2)` is still "1.00" because 1.005 is really 1.00499999999999989.
inline String number_to_fixed(double value, double digits_value = 0.0) {
  const double digits = std::isnan(digits_value) ? 0.0 : std::trunc(digits_value);
  if (!(digits >= 0.0) || digits > 100.0) {
    throw std::range_error("flight::number_to_fixed digits must be between 0 and 100");
  }
  if (std::isnan(value)) return String("NaN");

  // The sign comes from the comparison, not from the sign bit: negative zero has no sign here,
  // while a negative value that rounds to zero keeps one, exactly as the specification states.
  const bool negative = value < 0.0;
  // fabs rather than negation: negative zero has no sign in this result, and printing it
  // through the expansion below would reintroduce one.
  const double magnitude = std::fabs(value);
  if (!(magnitude < 1e21)) return number_to_string(value);

  const auto fraction_digits = static_cast<std::size_t>(digits);

  // A double under 1e21 has at most 1074 fractional digits, so this expansion is exact rather than
  // rounded, which is what makes the tie above decidable.
  std::string exact(1200, '\0');
  const int written = std::snprintf(exact.data(), exact.size(), "%.*f", 1100, magnitude);
  if (written <= 0) throw std::range_error("flight::number_to_fixed could not expand the value");
  exact.resize(static_cast<std::size_t>(written));

  const auto separator = exact.find('.');
  std::string integer_text = exact.substr(0, separator);
  std::string fraction_text =
      separator == std::string::npos ? std::string() : exact.substr(separator + 1);

  bool carry = false;
  if (fraction_text.size() > fraction_digits) {
    carry = fraction_text[fraction_digits] >= '5';
    fraction_text.resize(fraction_digits);
  } else {
    fraction_text.append(fraction_digits - fraction_text.size(), '0');
  }

  for (auto digit = fraction_text.rbegin(); carry && digit != fraction_text.rend(); ++digit) {
    if (*digit == '9') {
      *digit = '0';
    } else {
      ++*digit;
      carry = false;
    }
  }
  for (auto digit = integer_text.rbegin(); carry && digit != integer_text.rend(); ++digit) {
    if (*digit == '9') {
      *digit = '0';
    } else {
      ++*digit;
      carry = false;
    }
  }
  if (carry) integer_text.insert(integer_text.begin(), '1');

  std::string result;
  if (negative) result.push_back('-');
  result.append(integer_text);
  if (!fraction_text.empty()) {
    result.push_back('.');
    result.append(fraction_text);
  }
  return String(result);
}

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

inline double parse_float(const String& input) {
  const std::string encoded = detail::trim_leading_number_text(input);
  std::string_view text = encoded;

  bool negative = false;
  if (!text.empty() && (text.front() == '+' || text.front() == '-')) {
    negative = text.front() == '-';
    text.remove_prefix(1);
  }
  if (text.starts_with("Infinity")) {
    return negative ? -std::numeric_limits<double>::infinity()
                    : std::numeric_limits<double>::infinity();
  }

  const char* const begin = text.data();
  const char* cursor = begin;
  const char* const limit = begin + text.size();
  bool has_digits = false;
  while (cursor != limit && *cursor >= '0' && *cursor <= '9') {
    has_digits = true;
    ++cursor;
  }
  if (cursor != limit && *cursor == '.') {
    ++cursor;
    while (cursor != limit && *cursor >= '0' && *cursor <= '9') {
      has_digits = true;
      ++cursor;
    }
  }
  if (!has_digits) return std::numeric_limits<double>::quiet_NaN();

  if (cursor != limit && (*cursor == 'e' || *cursor == 'E')) {
    const char* const exponent = cursor;
    ++cursor;
    if (cursor != limit && (*cursor == '+' || *cursor == '-')) ++cursor;
    const char* const exponent_digits = cursor;
    while (cursor != limit && *cursor >= '0' && *cursor <= '9') ++cursor;
    if (cursor == exponent_digits) cursor = exponent;
  }

  double result = 0.0;
  const auto parsed = std::from_chars(begin, cursor, result, std::chars_format::general);
  if (parsed.ec == std::errc::result_out_of_range) {
    const auto prefix = text.substr(0, static_cast<std::size_t>(cursor - begin));
    const auto exponent = prefix.find_last_of("eE");
    const auto significand = prefix.substr(0, exponent);
    std::ptrdiff_t digits_before_decimal = 0;
    std::ptrdiff_t first_nonzero = -1;
    std::ptrdiff_t digit_index = 0;
    bool before_decimal = true;
    for (const char character : significand) {
      if (character == '.') {
        before_decimal = false;
      } else {
        if (before_decimal) ++digits_before_decimal;
        if (first_nonzero < 0 && character != '0') first_nonzero = digit_index;
        ++digit_index;
      }
    }
    if (first_nonzero < 0) {
      result = 0.0;
    } else {
      std::int64_t explicit_exponent = 0;
      if (exponent != std::string_view::npos) {
        auto exponent_text = prefix.substr(exponent + 1);
        bool exponent_negative = false;
        if (!exponent_text.empty() && (exponent_text.front() == '+' || exponent_text.front() == '-')) {
          exponent_negative = exponent_text.front() == '-';
          exponent_text.remove_prefix(1);
        }
        for (const char character : exponent_text) {
          explicit_exponent = std::min<std::int64_t>(
              explicit_exponent * 10 + static_cast<std::int64_t>(character - '0'), 1000000);
        }
        if (exponent_negative) explicit_exponent = -explicit_exponent;
      }
      const auto magnitude = static_cast<std::int64_t>(digits_before_decimal) -
                             static_cast<std::int64_t>(first_nonzero) - 1 + explicit_exponent;
      result = magnitude > 308 ? std::numeric_limits<double>::infinity() : 0.0;
    }
  } else if (parsed.ec != std::errc{} || parsed.ptr != cursor) {
    return std::numeric_limits<double>::quiet_NaN();
  }
  return negative ? -result : result;
}

inline double parse_float(const char* input) { return parse_float(String(input)); }
inline double parse_float(std::string_view input) { return parse_float(String(input)); }

inline bool is_safe_integer(double value) noexcept {
  constexpr double maximum_safe_integer = 9007199254740991.0;
  return std::isfinite(value) && std::trunc(value) == value &&
         std::abs(value) <= maximum_safe_integer;
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

inline double to_number(const char* input) { return to_number(String(input)); }
inline double to_number(std::string_view input) { return to_number(String(input)); }
inline double to_number(double value) noexcept { return value; }
inline double to_number(bool value) noexcept { return value ? 1.0 : 0.0; }

} // namespace flight
