#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

#include <flight/string.hpp>

namespace flight {

class UriError final : public std::runtime_error {
 public:
  UriError() : std::runtime_error("malformed URI sequence") {}
};

namespace detail {

inline bool is_uri_component_unescaped(std::uint8_t byte) noexcept {
  return (byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z') ||
         (byte >= '0' && byte <= '9') || byte == '-' || byte == '_' || byte == '.' ||
         byte == '!' || byte == '~' || byte == '*' || byte == '\'' || byte == '(' || byte == ')';
}

inline void append_uri_hex(std::string& output, std::uint8_t byte) {
  static constexpr std::string_view digits = "0123456789ABCDEF";
  output.push_back('%');
  output.push_back(digits[(byte >> 4) & 0x0F]);
  output.push_back(digits[byte & 0x0F]);
}

inline int uri_hex_value(char16_t unit) noexcept {
  if (unit >= u'0' && unit <= u'9') return unit - u'0';
  if (unit >= u'A' && unit <= u'F') return unit - u'A' + 10;
  if (unit >= u'a' && unit <= u'f') return unit - u'a' + 10;
  return -1;
}

inline std::uint8_t decode_uri_byte(std::u16string_view input, std::size_t offset) {
  if (offset + 2 >= input.size() || input[offset] != u'%') throw UriError();
  const auto high = uri_hex_value(input[offset + 1]);
  const auto low = uri_hex_value(input[offset + 2]);
  if (high < 0 || low < 0) throw UriError();
  return static_cast<std::uint8_t>((high << 4) | low);
}

inline void append_uri_scalar(std::string& output, std::uint32_t scalar) {
  std::array<std::uint8_t, 4> bytes{};
  std::size_t length = 0;
  if (scalar <= 0x7F) {
    bytes[0] = static_cast<std::uint8_t>(scalar);
    length = 1;
  } else if (scalar <= 0x7FF) {
    bytes[0] = static_cast<std::uint8_t>(0xC0 | (scalar >> 6));
    bytes[1] = static_cast<std::uint8_t>(0x80 | (scalar & 0x3F));
    length = 2;
  } else if (scalar <= 0xFFFF) {
    bytes[0] = static_cast<std::uint8_t>(0xE0 | (scalar >> 12));
    bytes[1] = static_cast<std::uint8_t>(0x80 | ((scalar >> 6) & 0x3F));
    bytes[2] = static_cast<std::uint8_t>(0x80 | (scalar & 0x3F));
    length = 3;
  } else {
    bytes[0] = static_cast<std::uint8_t>(0xF0 | (scalar >> 18));
    bytes[1] = static_cast<std::uint8_t>(0x80 | ((scalar >> 12) & 0x3F));
    bytes[2] = static_cast<std::uint8_t>(0x80 | ((scalar >> 6) & 0x3F));
    bytes[3] = static_cast<std::uint8_t>(0x80 | (scalar & 0x3F));
    length = 4;
  }
  for (std::size_t index = 0; index < length; ++index) {
    if (is_uri_component_unescaped(bytes[index])) {
      output.push_back(static_cast<char>(bytes[index]));
    } else {
      append_uri_hex(output, bytes[index]);
    }
  }
}

} // namespace detail

[[nodiscard]] inline String encode_uri_component(const String& value) {
  std::string output;
  output.reserve(value.length());
  const auto& input = value.native();
  for (std::size_t index = 0; index < input.size(); ++index) {
    std::uint32_t scalar = input[index];
    if (scalar >= 0xD800 && scalar <= 0xDBFF) {
      if (index + 1 >= input.size()) throw UriError();
      const auto low = static_cast<std::uint32_t>(input[index + 1]);
      if (low < 0xDC00 || low > 0xDFFF) throw UriError();
      scalar = 0x10000 + ((scalar - 0xD800) << 10) + (low - 0xDC00);
      ++index;
    } else if (scalar >= 0xDC00 && scalar <= 0xDFFF) {
      throw UriError();
    }
    detail::append_uri_scalar(output, scalar);
  }
  return String::from_utf8(output);
}

[[nodiscard]] inline String decode_uri_component(const String& value) {
  std::u16string output;
  const auto& input = value.native();
  output.reserve(input.size());
  for (std::size_t index = 0; index < input.size();) {
    if (input[index] != u'%') {
      output.push_back(input[index]);
      ++index;
      continue;
    }

    const auto first = detail::decode_uri_byte(input, index);
    if (first <= 0x7F) {
      output.push_back(static_cast<char16_t>(first));
      index += 3;
      continue;
    }

    std::size_t continuation_count = 0;
    std::uint32_t scalar = 0;
    if (first >= 0xC2 && first <= 0xDF) {
      continuation_count = 1;
      scalar = first & 0x1F;
    } else if (first >= 0xE0 && first <= 0xEF) {
      continuation_count = 2;
      scalar = first & 0x0F;
    } else if (first >= 0xF0 && first <= 0xF4) {
      continuation_count = 3;
      scalar = first & 0x07;
    } else {
      throw UriError();
    }

    for (std::size_t continuation = 0; continuation < continuation_count; ++continuation) {
      const auto offset = index + (continuation + 1) * 3;
      const auto byte = detail::decode_uri_byte(input, offset);
      std::uint8_t lower = 0x80;
      std::uint8_t upper = 0xBF;
      if (continuation == 0 && first == 0xE0) lower = 0xA0;
      if (continuation == 0 && first == 0xED) upper = 0x9F;
      if (continuation == 0 && first == 0xF0) lower = 0x90;
      if (continuation == 0 && first == 0xF4) upper = 0x8F;
      if (byte < lower || byte > upper) throw UriError();
      scalar = (scalar << 6) | (byte & 0x3F);
    }
    index += (continuation_count + 1) * 3;
    if (scalar <= 0xFFFF) {
      output.push_back(static_cast<char16_t>(scalar));
    } else {
      scalar -= 0x10000;
      output.push_back(static_cast<char16_t>(0xD800 + (scalar >> 10)));
      output.push_back(static_cast<char16_t>(0xDC00 + (scalar & 0x3FF)));
    }
  }
  return String(std::move(output));
}

} // namespace flight
