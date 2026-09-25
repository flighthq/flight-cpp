#pragma once

#include <cmath>
#include <concepts>
#include <cstddef>
#include <functional>
#include <optional>
#include <utility>
#include <variant>

#include <flight/array.hpp>
#include <flight/date.hpp>
#include <flight/string.hpp>

namespace flight {

// This initial implementation is a deterministic, locale-neutral baseline. These dictionaries
// preserve the options Flight currently transports without claiming that the baseline formatters
// interpret them. A host-backed internationalization provider may use the same records.
struct IntlCollatorOptions final {
  std::optional<String> sensitivity;
  std::optional<bool> numeric;
  std::optional<String> case_first;
};

struct IntlDateTimeFormatOptions final {
  std::optional<String> year;
  std::optional<String> month;
  std::optional<String> day;
  std::optional<String> hour;
  std::optional<String> minute;
};

struct IntlListFormatOptions final {
  std::optional<String> type;
  std::optional<String> style;
};

struct IntlNumberFormatOptions final {
  std::optional<String> notation;
  std::optional<String> style;
  std::optional<String> currency;
  std::optional<String> unit;
};

struct IntlPluralRulesOptions final {
  std::optional<String> type;
};

struct IntlRelativeTimeFormatOptions final {
  std::optional<String> numeric;
};

struct IntlSegmenterOptions final {
  std::optional<String> granularity;
};

using IntlLocalesArgument = std::variant<String, Array<String>>;

struct IntlSegmentData final {
  String segment;
  double index{};
  std::optional<bool> is_word_like;
};

using IntlSegments = Array<IntlSegmentData>;

// The portable runtime does not advertise the Intl.Segmenter constructor. This type-only carrier
// lets a host provide real Unicode segmentation without making a dependency-free build pretend to
// implement UAX #29. The compiler profile therefore binds the type, but no Intl.Segmenter value.
class IntlSegmenter final {
 public:
  using Operation = std::function<IntlSegments(const String&)>;

  explicit IntlSegmenter(Operation operation) : operation_(std::move(operation)) {}

  [[nodiscard]] IntlSegments segment(const String& input) const { return operation_(input); }

 private:
  Operation operation_;
};

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
