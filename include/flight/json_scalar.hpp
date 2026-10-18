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

  // C++ arithmetic/comparison on this cell is a JavaScript numeric operation, so it uses ToNumber.
  [[nodiscard]] operator double() const { return to_number(); }

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
    const std::string value = text.to_utf8();
    std::size_t begin = 0;
    std::size_t end = value.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(value[begin]))) ++begin;
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1]))) --end;
    if (begin == end) return 0.0;
    const std::string trimmed = value.substr(begin, end - begin);
    char* stop = nullptr;
    const double parsed = std::strtod(trimmed.c_str(), &stop);
    if (stop == trimmed.c_str()) return std::numeric_limits<double>::quiet_NaN();
    while (*stop != '\0' && std::isspace(static_cast<unsigned char>(*stop))) ++stop;
    if (*stop != '\0') return std::numeric_limits<double>::quiet_NaN();
    return parsed;
  }

  Any value_{};
  bool present_{false};
};

} // namespace flight
