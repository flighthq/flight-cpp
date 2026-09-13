#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

#include <flight/string.hpp>

namespace flight {

class InvalidCharacterError final : public std::runtime_error {
 public:
  InvalidCharacterError() : std::runtime_error("invalid character") {}
};

namespace detail {

inline bool is_base64_whitespace(char16_t unit) noexcept {
  return unit == u'\t' || unit == u'\n' || unit == u'\f' || unit == u'\r' || unit == u' ';
}

inline int base64_value(char16_t unit) noexcept {
  if (unit >= u'A' && unit <= u'Z') return unit - u'A';
  if (unit >= u'a' && unit <= u'z') return unit - u'a' + 26;
  if (unit >= u'0' && unit <= u'9') return unit - u'0' + 52;
  if (unit == u'+') return 62;
  if (unit == u'/') return 63;
  return -1;
}

} // namespace detail

[[nodiscard]] inline String atob(const String& encoded) {
  std::u16string input;
  input.reserve(encoded.length());
  for (const auto unit : encoded.native()) {
    if (!detail::is_base64_whitespace(unit)) input.push_back(unit);
  }
  if (input.size() % 4 == 0) {
    if (!input.empty() && input.back() == u'=') input.pop_back();
    if (!input.empty() && input.back() == u'=') input.pop_back();
  }
  if (input.size() % 4 == 1) throw InvalidCharacterError();

  std::u16string output;
  output.reserve((input.size() * 3) / 4);
  std::uint32_t accumulator = 0;
  int bits = 0;
  for (const auto unit : input) {
    const auto value = detail::base64_value(unit);
    if (value < 0) throw InvalidCharacterError();
    accumulator = (accumulator << 6) | static_cast<std::uint32_t>(value);
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      output.push_back(static_cast<char16_t>((accumulator >> bits) & 0xFF));
    }
  }
  return String(std::move(output));
}

[[nodiscard]] inline String btoa(const String& decoded) {
  static constexpr std::array<char, 64> alphabet{
      'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
      'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
      'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
      'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
      '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/'};
  std::string output;
  output.reserve(((decoded.length() + 2) / 3) * 4);
  std::uint32_t accumulator = 0;
  int bits = 0;
  for (const auto unit : decoded.native()) {
    if (unit > 0xFF) throw InvalidCharacterError();
    accumulator = (accumulator << 8) | unit;
    bits += 8;
    while (bits >= 6) {
      bits -= 6;
      output.push_back(alphabet[(accumulator >> bits) & 0x3F]);
    }
  }
  if (bits > 0) output.push_back(alphabet[(accumulator << (6 - bits)) & 0x3F]);
  while (output.size() % 4 != 0) output.push_back('=');
  return String::from_utf8(output);
}

} // namespace flight
