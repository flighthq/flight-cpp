#pragma once

#include <cmath>
#include <concepts>
#include <cstddef>
#include <utility>

#include <flight/array.hpp>
#include <flight/date.hpp>
#include <flight/string.hpp>

namespace flight {

// This initial implementation is a deterministic, locale-neutral baseline. Locale-sensitive
// collation and option records remain part of the planned internationalization capability.
struct IntlCollatorOptions {};
struct IntlDateTimeFormatOptions {};
struct IntlListFormatOptions {};
struct IntlNumberFormatOptions {};
struct IntlPluralRulesOptions {};
struct IntlRelativeTimeFormatOptions {};

using IntlPluralRule = String;
using IntlRelativeTimeFormatUnit = String;

class IntlCollator {
 public:
  IntlCollator() = default;

  template <typename... Arguments>
    requires(sizeof...(Arguments) >= 1 && sizeof...(Arguments) <= 2)
  explicit IntlCollator(Arguments&&...) {}

  [[nodiscard]] double compare(const String& left, const String& right) const noexcept {
    if (left < right) return -1.0;
    if (right < left) return 1.0;
    return 0.0;
  }
};

class IntlDateTimeFormat {
 public:
  IntlDateTimeFormat() = default;

  template <typename... Arguments>
    requires(sizeof...(Arguments) >= 1 && sizeof...(Arguments) <= 2)
  explicit IntlDateTimeFormat(Arguments&&...) {}

  [[nodiscard]] String format(const Date& value) const { return value.to_isostring(); }
  [[nodiscard]] String format(double value) const { return Date(value).to_isostring(); }
};

class IntlListFormat {
 public:
  IntlListFormat() = default;

  template <typename... Arguments>
    requires(sizeof...(Arguments) >= 1 && sizeof...(Arguments) <= 2)
  explicit IntlListFormat(Arguments&&...) {}

  [[nodiscard]] String format(const Array<String>& values) const {
    if (values.empty()) return String();
    if (values.size() == 1) return values[0];
    if (values.size() == 2) return values[0].concat(String(" and "), values[1]);

    String result;
    for (std::size_t index = 0; index < values.size(); ++index) {
      if (index != 0) result = result.concat(index + 1 == values.size() ? String(", and ") : String(", "));
      result = result.concat(values[index]);
    }
    return result;
  }
};

class IntlNumberFormat {
 public:
  IntlNumberFormat() = default;

  template <typename... Arguments>
    requires(sizeof...(Arguments) >= 1 && sizeof...(Arguments) <= 2)
  explicit IntlNumberFormat(Arguments&&...) {}

  [[nodiscard]] String format(double value) const { return String::from_number(value); }
};

class IntlPluralRules {
 public:
  IntlPluralRules() = default;

  template <typename... Arguments>
    requires(sizeof...(Arguments) >= 1 && sizeof...(Arguments) <= 2)
  explicit IntlPluralRules(Arguments&&...) {}

  [[nodiscard]] IntlPluralRule select(double value) const {
    return String(std::abs(value) == 1.0 ? "one" : "other");
  }
};

class IntlRelativeTimeFormat {
 public:
  IntlRelativeTimeFormat() = default;

  template <typename... Arguments>
    requires(sizeof...(Arguments) >= 1 && sizeof...(Arguments) <= 2)
  explicit IntlRelativeTimeFormat(Arguments&&...) {}

  [[nodiscard]] String format(double value, const IntlRelativeTimeFormatUnit& unit) const {
    const auto magnitude = String::from_number(std::abs(value));
    const auto plural = std::abs(value) == 1.0 ? String() : String("s");
    const auto quantity = magnitude.concat(String(" "), unit, plural);
    return value < 0.0 ? quantity.concat(String(" ago")) : String("in ").concat(quantity);
  }
};

struct Intl {};

} // namespace flight
