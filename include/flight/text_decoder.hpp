#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

#include <flight/string.hpp>
#include <flight/typed_array.hpp>

namespace flight {

class TextDecoder {
 public:
  TextDecoder() = default;

  explicit TextDecoder(const String& encoding) {
    const auto name = encoding.to_lower();
    if (name != String("utf-8") && name != String("utf8")) {
      throw std::invalid_argument("flight::TextDecoder supports UTF-8 only");
    }
  }

  [[nodiscard]] String decode() const { return String(); }

  [[nodiscard]] String decode(const Uint8Array& input) const {
    std::u16string result;
    const auto bytes = input.span();
    std::size_t index = bytes.size() >= 3 && bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF ? 3 : 0;
    for (; index < bytes.size();) {
      const std::uint8_t first = bytes[index];
      std::uint32_t code_point = 0;
      std::size_t continuation_count = 0;
      if (first <= 0x7F) {
        code_point = first;
      } else if (first >= 0xC2 && first <= 0xDF) {
        code_point = first & 0x1F;
        continuation_count = 1;
      } else if (first >= 0xE0 && first <= 0xEF) {
        code_point = first & 0x0F;
        continuation_count = 2;
      } else if (first >= 0xF0 && first <= 0xF4) {
        code_point = first & 0x07;
        continuation_count = 3;
      } else {
        result.push_back(u'\uFFFD');
        ++index;
        continue;
      }

      std::size_t consumed = 0;
      while (consumed < continuation_count && index + consumed + 1 < bytes.size()) {
        const auto continuation = bytes[index + consumed + 1];
        std::uint8_t lower = 0x80;
        std::uint8_t upper = 0xBF;
        if (consumed == 0 && first == 0xE0) lower = 0xA0;
        if (consumed == 0 && first == 0xED) upper = 0x9F;
        if (consumed == 0 && first == 0xF0) lower = 0x90;
        if (consumed == 0 && first == 0xF4) upper = 0x8F;
        if (continuation < lower || continuation > upper) break;
        code_point = (code_point << 6) | (continuation & 0x3F);
        ++consumed;
      }
      const bool complete = consumed == continuation_count;
      if (!complete) {
        result.push_back(u'\uFFFD');
        index += consumed + 1;
        continue;
      }
      if (code_point <= 0xFFFF) {
        result.push_back(static_cast<char16_t>(code_point));
      } else {
        code_point -= 0x10000;
        result.push_back(static_cast<char16_t>(0xD800 + (code_point >> 10)));
        result.push_back(static_cast<char16_t>(0xDC00 + (code_point & 0x3FF)));
      }
      index += continuation_count + 1;
    }
    return String(std::move(result));
  }
};

} // namespace flight
