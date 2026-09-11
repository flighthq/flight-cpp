#pragma once

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>
#include <string_view>

#include <flight/string.hpp>

namespace flight {

class Url {
 public:
  explicit Url(const String& value) { assign(value, nullptr); }
  Url(const String& value, const String& base) { assign(value, &base); }
  Url(const String& value, const Url& base) { assign(value, &base.protocol); }

  String protocol;

 private:
  void assign(const String& value, const String* base) {
    const std::string encoded = value.to_utf8();
    const auto separator = encoded.find(':');
    if (separator != std::string::npos && separator != 0 && valid_scheme(encoded.substr(0, separator))) {
      auto scheme = encoded.substr(0, separator + 1);
      std::transform(scheme.begin(), scheme.end(), scheme.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
      });
      protocol = String::from_utf8(scheme);
      return;
    }
    if (base != nullptr) {
      const Url parsed_base(*base);
      protocol = parsed_base.protocol;
      return;
    }
    throw std::invalid_argument("flight::Url requires an absolute URL or a base URL");
  }

  [[nodiscard]] static bool valid_scheme(std::string_view value) noexcept {
    if (value.empty() || std::isalpha(static_cast<unsigned char>(value.front())) == 0) return false;
    for (const char character : value.substr(1)) {
      const auto byte = static_cast<unsigned char>(character);
      if (std::isalnum(byte) == 0 && character != '+' && character != '-' && character != '.') return false;
    }
    return true;
  }
};

} // namespace flight
