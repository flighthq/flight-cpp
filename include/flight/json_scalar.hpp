#pragma once

// A lazy, presence-aware JSON scalar cell.
//
// A typed member projected from parsed JSON keeps the RAW JSON value: `typeof`, equality,
// ToNumber arithmetic/comparison, unary +/-, ToString/templates, truthiness and `??`/`?.` are
// answered from that value at the use. Nothing is coerced or defaulted at the read, and the cell
// does not narrow the value to the declared TypeScript scalar. Only an actual escape to a native
// double (a parameter, field, return value or array element) requires a number, and a non-number
// there raises the dedicated out-of-profile error below -- never a JS-shaped catchable error and
// never a silent coercion.

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include <flight/any.hpp>
#include <flight/boolean.hpp>
#include <flight/string.hpp>

namespace flight {

// Out-of-profile only: a JSON value that is not a number reached a native double boundary. This is
// deliberately NOT flight::Error/DOMException, so it cannot be observed as a JS-shaped error.
class NativeScalarEscapeError final : public std::runtime_error {
 public:
  explicit NativeScalarEscapeError(std::string_view held)
      : std::runtime_error("a JSON value of type '" + std::string(held) +
                           "' cannot be used where a native number is required") {}
};

class JsonScalarCell final {
 public:
  JsonScalarCell() = default;  // the member is absent (undefined)
  explicit JsonScalarCell(Any value) : value_(std::move(value)), present_(true) {}

  [[nodiscard]] bool is_absent() const noexcept { return !present_; }
  [[nodiscard]] bool is_present() const noexcept { return present_; }
  [[nodiscard]] bool is_nullish() const noexcept {
    return !present_ || value_.is_nullish();
  }
  [[nodiscard]] const Any& raw() const noexcept { return value_; }

  [[nodiscard]] std::string_view type_of() const noexcept {
    if (!present_) return "undefined";
    switch (value_.kind()) {
      case AnyKind::undefined: return "undefined";
      case AnyKind::null: return "object";
      case AnyKind::boolean: return "boolean";
      case AnyKind::number: return "number";
      case AnyKind::string: return "string";
      case AnyKind::function: return "function";
      case AnyKind::symbol: return "symbol";
      case AnyKind::object:
      case AnyKind::external: return "object";
    }
    return "undefined";
  }

  [[nodiscard]] bool to_boolean() const noexcept {
    return present_ && value_.to_boolean();
  }

  // ToNumber. Absent/null -> 0 for null and NaN for absent, as JavaScript. Strings parse as
  // JavaScript Number() accepts (trimmed decimal/Infinity); other objects are NaN.
  [[nodiscard]] double to_number() const {
    if (!present_) return std::numeric_limits<double>::quiet_NaN();
    switch (value_.kind()) {
      case AnyKind::number: return value_.as_number();
      case AnyKind::null: return 0.0;
      case AnyKind::undefined: return std::numeric_limits<double>::quiet_NaN();
      case AnyKind::boolean: return value_.as_boolean() ? 1.0 : 0.0;
      case AnyKind::string: return parse_number(value_.as_string());
      default: return std::numeric_limits<double>::quiet_NaN();
    }
  }

  [[nodiscard]] double unary_plus() const { return to_number(); }
  [[nodiscard]] double unary_minus() const { return -to_number(); }

  // `Number.isFinite` and `Number.isNaN` ask about the VALUE; they do not coerce. A JSON null, a
  // numeric string, or a boolean is not a number to them even though `> 0` and arithmetic on the same
  // value go through ToNumber. Answering these from a numeric conversion would make null finite.
  [[nodiscard]] bool is_finite() const noexcept {
    return present_ && value_.kind() == AnyKind::number && std::isfinite(value_.as_number());
  }

  [[nodiscard]] bool is_nan() const noexcept {
    return present_ && value_.kind() == AnyKind::number && std::isnan(value_.as_number());
  }

  [[nodiscard]] String to_string() const {
    return present_ ? flight::to_string(value_) : flight::String("undefined");
  }

  // A member that is not present reads as `undefined`, exactly like a member whose value is
  // `undefined`. Presence is what separates them for `in`; equality is a question about the value,
  // so the two must compare equal here and nowhere else.
  [[nodiscard]] bool evaluates_to_undefined() const noexcept {
    return !present_ || value_.is_undefined();
  }

  [[nodiscard]] bool strict_equals(const JsonScalarCell& other) const {
    if (evaluates_to_undefined() || other.evaluates_to_undefined()) {
      return evaluates_to_undefined() && other.evaluates_to_undefined();
    }
    return value_.strict_equals(other.value_);
  }

  // JavaScript `==`: nullish equals only nullish; otherwise compare ToNumber for scalar types.
  [[nodiscard]] bool loose_equals(const JsonScalarCell& other) const {
    if (is_nullish() || other.is_nullish()) {
      return is_nullish() && other.is_nullish();
    }
    if (present_ && other.present_) {
      if (value_.kind() == other.value_.kind()) return value_.strict_equals(other.value_);
    }
    const double left = to_number();
    const double right = other.to_number();
    return !std::isnan(left) && !std::isnan(right) && left == right;
  }

  // Escape to a native double: only an actual number converts. Any other present value (and an
  // absent one) is out of profile and raises the dedicated error.
  [[nodiscard]] double as_native_number() const {
    if (present_ && value_.kind() == AnyKind::number) return value_.as_number();
    throw NativeScalarEscapeError(type_of());
  }

 private:
  [[nodiscard]] static double parse_number(const String& text) {
    const std::string raw = text.to_utf8();
    std::size_t begin = 0;
    std::size_t finish = raw.size();
    while (begin < finish && std::isspace(static_cast<unsigned char>(raw[begin]))) ++begin;
    while (finish > begin && std::isspace(static_cast<unsigned char>(raw[finish - 1]))) --finish;
    if (begin == finish) return 0.0;  // Number("") and whitespace-only is 0
    const std::string value = raw.substr(begin, finish - begin);

    // Radix prefixes: accepted only without a sign, exactly as JavaScript Number().
    if (value.size() > 2 && value[0] == '0') {
      const char prefix = value[1];
      if (prefix == 'x' || prefix == 'X' || prefix == 'b' || prefix == 'B' || prefix == 'o' || prefix == 'O') {
        const unsigned base = (prefix == 'x' || prefix == 'X') ? 16u : (prefix == 'b' || prefix == 'B') ? 2u : 8u;
        long double accumulated = 0.0L;
        bool any = false;
        for (std::size_t index = 2; index < value.size(); ++index) {
          const unsigned char character = static_cast<unsigned char>(value[index]);
          unsigned digit = 0;
          if (character >= '0' && character <= '9') digit = character - '0';
          else if (character >= 'a' && character <= 'f') digit = 10 + (character - 'a');
          else if (character >= 'A' && character <= 'F') digit = 10 + (character - 'A');
          else return std::numeric_limits<double>::quiet_NaN();
          if (digit >= base) return std::numeric_limits<double>::quiet_NaN();
          accumulated = accumulated * base + digit;
          any = true;
        }
        if (!any) return std::numeric_limits<double>::quiet_NaN();
        if (accumulated > static_cast<long double>(std::numeric_limits<double>::max())) {
          return std::numeric_limits<double>::infinity();
        }
        return static_cast<double>(accumulated);
      }
    }

    // Infinity is JavaScript's exact spelling after an optional sign; strtod's "inf"/"nan" are not.
    std::size_t sign_offset = (value[0] == '+' || value[0] == '-') ? 1 : 0;
    const bool negative = value[0] == '-';
    const std::string_view body(value.data() + sign_offset, value.size() - sign_offset);
    if (body == "Infinity") {
      return negative ? -std::numeric_limits<double>::infinity() : std::numeric_limits<double>::infinity();
    }

    // Decimal syntax: optional sign, digits with at most one '.', optional exponent. Every character
    // must be consumed, so "30px", "0x" without digits, "inf"/"nan" and trailing junk are NaN.
    std::size_t index = sign_offset;
    bool digits = false;
    bool point = false;
    while (index < value.size() && std::isdigit(static_cast<unsigned char>(value[index]))) {
      digits = true;
      ++index;
    }
    if (index < value.size() && value[index] == '.') {
      point = true;
      ++index;
      while (index < value.size() && std::isdigit(static_cast<unsigned char>(value[index]))) {
        digits = true;
        ++index;
      }
    }
    if (!digits) return std::numeric_limits<double>::quiet_NaN();
    static_cast<void>(point);
    if (index < value.size() && (value[index] == 'e' || value[index] == 'E')) {
      ++index;
      if (index < value.size() && (value[index] == '+' || value[index] == '-')) ++index;
      bool exponent_digits = false;
      while (index < value.size() && std::isdigit(static_cast<unsigned char>(value[index]))) {
        exponent_digits = true;
        ++index;
      }
      if (!exponent_digits) return std::numeric_limits<double>::quiet_NaN();
    }
    if (index != value.size()) return std::numeric_limits<double>::quiet_NaN();
    char* stop = nullptr;
    const double parsed = std::strtod(value.c_str(), &stop);
    return stop == value.c_str() ? std::numeric_limits<double>::quiet_NaN() : parsed;
  }

  Any value_{};
  bool present_{false};
};

} // namespace flight
