#pragma once

#include <cstdint>
#include <vector>

#include <flight/string.hpp>
#include <flight/typed_array.hpp>

namespace flight {

class TextEncoder final {
 public:
  TextEncoder() = default;

  [[nodiscard]] Uint8Array encode(const String& input = {}) const {
    std::vector<std::uint8_t> output;
    output.reserve(input.length());
    const auto& units = input.native();
    for (std::size_t index = 0; index < units.size(); ++index) {
      std::uint32_t scalar = units[index];
      if (scalar >= 0xD800 && scalar <= 0xDBFF) {
        if (index + 1 < units.size()) {
          const auto low = static_cast<std::uint32_t>(units[index + 1]);
          if (low >= 0xDC00 && low <= 0xDFFF) {
            scalar = 0x10000 + ((scalar - 0xD800) << 10) + (low - 0xDC00);
            ++index;
          } else {
            scalar = 0xFFFD;
          }
        } else {
          scalar = 0xFFFD;
        }
      } else if (scalar >= 0xDC00 && scalar <= 0xDFFF) {
        scalar = 0xFFFD;
      }
      append_utf8(output, scalar);
    }
    return Uint8Array(output);
  }

  String encoding{"utf-8"};

 private:
  static void append_utf8(std::vector<std::uint8_t>& output, std::uint32_t scalar) {
    if (scalar <= 0x7F) {
      output.push_back(static_cast<std::uint8_t>(scalar));
    } else if (scalar <= 0x7FF) {
      output.push_back(static_cast<std::uint8_t>(0xC0 | (scalar >> 6)));
      output.push_back(static_cast<std::uint8_t>(0x80 | (scalar & 0x3F)));
    } else if (scalar <= 0xFFFF) {
      output.push_back(static_cast<std::uint8_t>(0xE0 | (scalar >> 12)));
      output.push_back(static_cast<std::uint8_t>(0x80 | ((scalar >> 6) & 0x3F)));
      output.push_back(static_cast<std::uint8_t>(0x80 | (scalar & 0x3F)));
    } else {
      output.push_back(static_cast<std::uint8_t>(0xF0 | (scalar >> 18)));
      output.push_back(static_cast<std::uint8_t>(0x80 | ((scalar >> 12) & 0x3F)));
      output.push_back(static_cast<std::uint8_t>(0x80 | ((scalar >> 6) & 0x3F)));
      output.push_back(static_cast<std::uint8_t>(0x80 | (scalar & 0x3F)));
    }
  }
};

} // namespace flight
