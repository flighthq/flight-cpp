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
#include <flight/out_of_profile.hpp>
#include <flight/json.hpp>
#include <flight/string.hpp>
#include <flight/structural_ref.hpp>

namespace flight {

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
      case AnyKind::symbol:
        // ToNumber of a Symbol throws; it is never NaN.
        throw TypeError("Cannot convert a Symbol value to a number");
      default:
        // ToNumber of a composite is ToNumber(ToPrimitive(x)): [] -> "" -> 0, [30] -> "30" -> 30,
        // [1,2] -> "1,2" -> NaN, {} -> "[object Object]" -> NaN.
        if (value_.kind() == AnyKind::object) return parse_number(composite_primitive_string(value_));
        return std::numeric_limits<double>::quiet_NaN();
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
    if (!present_) return flight::String("undefined");
    return is_composite() ? composite_primitive_string(value_) : flight::to_string(value_);
  }

  // ToPrimitive for a parsed JSON composite, which is what makes the source's numeric and string uses
  // of an array or object field faithful: an array joins its elements with ',' (a null element
  // contributes the empty string, a nested array flattens by the same rule) and a plain object is
  // "[object Object]". So ToNumber([]) is 0, ToNumber([30]) is 30, ToNumber([1,2]) and ToNumber({})
  // are NaN, and String({}) / String([1,2]) are "[object Object]" / "1,2" -- not a blanket NaN.
  [[nodiscard]] String to_primitive_string() const {
    return present_ ? composite_primitive_string(value_) : flight::String("undefined");
  }

  [[nodiscard]] bool is_composite() const noexcept {
    return present_ && value_.kind() == AnyKind::object;
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

  // JavaScript `+` is NOT universal ToNumber: ToPrimitive first, and if EITHER primitive is a string
  // the result is string concatenation. The result stays a raw-valued cell so downstream operations
  // keep their source semantics (e.g. "30" + 2 -> "302", then ToNumber if used numerically).
  [[nodiscard]] static JsonScalarCell add(const JsonScalarCell& left, const JsonScalarCell& right) {
    // A Symbol cannot be converted implicitly by `+`: string concatenation and numeric addition both
    // throw, with the message of the conversion the source would have attempted. Explicit
    // String(symbol) remains available through to_string().
    if (left.primitive_is_symbol() || right.primitive_is_symbol()) {
      if (left.primitive_is_string() || right.primitive_is_string()) {
        throw TypeError("Cannot convert a Symbol value to a string");
      }
      throw TypeError("Cannot convert a Symbol value to a number");
    }
    if (left.primitive_is_string() || right.primitive_is_string()) {
      return JsonScalarCell{Any(left.to_string() + right.to_string())};
    }
    return JsonScalarCell{Any(left.to_number() + right.to_number())};
  }
  [[nodiscard]] friend JsonScalarCell operator+(const JsonScalarCell& left, const JsonScalarCell& right) {
    return add(left, right);
  }
  [[nodiscard]] friend JsonScalarCell operator+(const JsonScalarCell& left, double right) {
    return add(left, JsonScalarCell{Any(right)});
  }
  [[nodiscard]] friend JsonScalarCell operator+(double left, const JsonScalarCell& right) {
    return add(JsonScalarCell{Any(left)}, right);
  }

  // - * / are numeric (ToNumber both); the raw cells only enter through that operation.
  [[nodiscard]] friend double operator-(const JsonScalarCell& left, const JsonScalarCell& right) {
    return left.to_number() - right.to_number();
  }
  [[nodiscard]] friend double operator-(const JsonScalarCell& left, double right) {
    return left.to_number() - right;
  }
  [[nodiscard]] friend double operator-(double left, const JsonScalarCell& right) {
    return left - right.to_number();
  }
  [[nodiscard]] friend double operator*(const JsonScalarCell& left, const JsonScalarCell& right) {
    return left.to_number() * right.to_number();
  }
  [[nodiscard]] friend double operator*(const JsonScalarCell& left, double right) {
    return left.to_number() * right;
  }
  [[nodiscard]] friend double operator*(double left, const JsonScalarCell& right) {
    return left * right.to_number();
  }
  [[nodiscard]] friend double operator/(const JsonScalarCell& left, const JsonScalarCell& right) {
    return left.to_number() / right.to_number();
  }
  [[nodiscard]] friend double operator/(const JsonScalarCell& left, double right) {
    return left.to_number() / right;
  }
  [[nodiscard]] friend double operator/(double left, const JsonScalarCell& right) {
    return left / right.to_number();
  }

  // Relational: ToPrimitive both; string/string is LEXICAL, every other combination is ToNumber.
  // NaN makes each comparison false.
  [[nodiscard]] static bool less(const JsonScalarCell& left, const JsonScalarCell& right) {
    if (left.primitive_is_string() && right.primitive_is_string()) {
      return left.to_string().native() < right.to_string().native();
    }
    return left.to_number() < right.to_number();
  }
  [[nodiscard]] static bool less_equal(const JsonScalarCell& left, const JsonScalarCell& right) {
    if (left.primitive_is_string() && right.primitive_is_string()) {
      return left.to_string().native() <= right.to_string().native();
    }
    const double l = left.to_number();
    const double r = right.to_number();
    return !std::isnan(l) && !std::isnan(r) && l <= r;
  }
  [[nodiscard]] static bool greater(const JsonScalarCell& left, const JsonScalarCell& right) {
    return less(right, left);
  }
  [[nodiscard]] static bool greater_equal(const JsonScalarCell& left, const JsonScalarCell& right) {
    return less_equal(right, left);
  }
  [[nodiscard]] friend bool operator<(const JsonScalarCell& l, const JsonScalarCell& r) { return less(l, r); }
  [[nodiscard]] friend bool operator>(const JsonScalarCell& l, const JsonScalarCell& r) { return greater(l, r); }
  [[nodiscard]] friend bool operator<=(const JsonScalarCell& l, const JsonScalarCell& r) { return less_equal(l, r); }
  [[nodiscard]] friend bool operator>=(const JsonScalarCell& l, const JsonScalarCell& r) { return greater_equal(l, r); }
  [[nodiscard]] friend bool operator<(const JsonScalarCell& l, double r) { return less(l, JsonScalarCell{Any(r)}); }
  [[nodiscard]] friend bool operator<(double l, const JsonScalarCell& r) { return less(JsonScalarCell{Any(l)}, r); }
  [[nodiscard]] friend bool operator>(const JsonScalarCell& l, double r) { return greater(l, JsonScalarCell{Any(r)}); }
  [[nodiscard]] friend bool operator>(double l, const JsonScalarCell& r) { return greater(JsonScalarCell{Any(l)}, r); }
  [[nodiscard]] friend bool operator<=(const JsonScalarCell& l, double r) { return less_equal(l, JsonScalarCell{Any(r)}); }
  [[nodiscard]] friend bool operator<=(double l, const JsonScalarCell& r) { return less_equal(JsonScalarCell{Any(l)}, r); }
  [[nodiscard]] friend bool operator>=(const JsonScalarCell& l, double r) { return greater_equal(l, JsonScalarCell{Any(r)}); }
  [[nodiscard]] friend bool operator>=(double l, const JsonScalarCell& r) { return greater_equal(JsonScalarCell{Any(l)}, r); }

  // ToPrimitive's string question for the operators above. Strings and JSON objects/arrays (whose
  // primitive form is a string) answer true; number/boolean/null/undefined answer false.
  [[nodiscard]] bool primitive_is_symbol() const noexcept {
    return present_ && value_.kind() == AnyKind::symbol;
  }

  [[nodiscard]] bool primitive_is_string() const noexcept {
    if (!present_) return false;
    switch (value_.kind()) {
      case AnyKind::string:
      case AnyKind::object:
      case AnyKind::external:
      case AnyKind::function:
        return true;
      default:
        return false;
    }
  }

  // Escape to a native double: only an actual number converts. Any other present value (and an
  // absent one) is out of profile and raises the dedicated error.
  [[nodiscard]] double as_native_number() const {
    if (present_ && value_.kind() == AnyKind::number) return value_.as_number();
    throw NativeScalarEscapeError(type_of());
  }

 private:
  // The JavaScript ToPrimitive string of a parsed JSON value. An array joins with ',', a null element
  // contributes nothing, nested arrays flatten by the same rule, and an ordinary object is
  // "[object Object]" -- the primitive the source's numeric and string operations actually see.
  [[nodiscard]] static String composite_primitive_string(const Any& value) {
    switch (value.kind()) {
      case AnyKind::null: return String("null");
      case AnyKind::undefined: return String("undefined");
      case AnyKind::boolean: return value.as_boolean() ? String("true") : String("false");
      case AnyKind::number: return flight::to_string(value);
      case AnyKind::string: return value.as_string();
      default: break;
    }
    if (value.is_array()) {
      String joined;
      const auto size = value.array_size();
      for (std::size_t index = 0; index < size; ++index) {
        if (index != 0) joined = joined + String(",");
        const auto element = value.array_get(static_cast<double>(index));
        // A null (or absent) element contributes the empty string, as Array.prototype.join does.
        if (!element || element->is_nullish()) continue;
        joined = joined + composite_primitive_string(*element);
      }
      return joined;
    }
    if (const auto object = value.object_reference_if<JsonObject>()) {
      // JSON cannot hold a callable, so an own `toString` is necessarily non-callable. It shadows
      // Object.prototype.toString, Object.prototype.valueOf returns the object itself (not a
      // primitive), and OrdinaryToPrimitive therefore cannot complete -- JavaScript throws.
      if (object->has(String("toString"))) throw TypeError("Cannot convert object to primitive value");
    }
    return String("[object Object]");
  }

  [[nodiscard]] static double parse_number(const String& text) {
    // ECMAScript trims its own whitespace set (including U+00A0, U+2028/9, U+3000, U+FEFF), so the
    // shared UTF-16 predicate decides -- the same one the typed number conversion uses. Trimming the
    // UTF-8 bytes with isspace would leave a non-breaking space in front of the digits and read NaN.
    const auto& units = text.native();
    std::size_t begin = 0;
    std::size_t finish = units.size();
    while (begin < finish && detail::number_whitespace(units[begin])) ++begin;
    while (finish > begin && detail::number_whitespace(units[finish - 1])) --finish;
    if (begin == finish) return 0.0;  // Number("") and whitespace-only is 0
    const std::string value = String(std::u16string(units.begin() + begin, units.begin() + finish)).to_utf8();

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

// The projection the emitter will call for a typed member read on a parsed-JSON receiver: the named
// member keeps its RAW JSON value and explicit presence. A missing key is absent; a present null,
// string, boolean, array or object is preserved as-is for the approved operations and only errors at
// a native-double escape.
[[nodiscard]] inline JsonScalarCell read_json_scalar_member(const JsonValue& object, const String& key) {
  const auto properties = named_properties(object);
  if (!properties.has(key)) return JsonScalarCell{};
  return JsonScalarCell{properties.get(key)};
}

// The Any-receiver form the private parsed-JSON read path actually has. It mirrors
// read_json_data_property's direct-read rule (a nullish receiver throws TypeError) and keeps presence:
// a missing own key is absent, while a present value is preserved raw.
//
// A boxed String is part of that path: it owns exactly `length` and its UTF-16 indices, and reading
// any other name is absent rather than throwing. Presence follows the data read's own index grammar
// and the value comes from that read, so selecting this projection cannot drop a property the
// existing path answered.
[[nodiscard]] inline JsonScalarCell read_json_scalar_property(const Any& value, const String& key) {
  if (value.is_nullish()) throw TypeError("Cannot read properties of null or undefined");
  if (value.kind() == AnyKind::string) {
    const auto& text = value.as_string().native();
    if (key == String("length")) return JsonScalarCell{read_json_data_property(value, key)};
    const auto& units = key.native();
    if (units.empty() || (units.size() > 1 && units.front() == u'0')) return JsonScalarCell{};
    std::size_t index = 0;
    for (const auto unit : units) {
      if (unit < u'0' || unit > u'9') return JsonScalarCell{};
      const auto digit = static_cast<std::size_t>(unit - u'0');
      if (digit >= text.size() || index > (text.size() - 1 - digit) / 10) return JsonScalarCell{};
      index = index * 10 + digit;
    }
    return JsonScalarCell{read_json_data_property(value, key)};
  }
  const auto properties = named_properties(value);
  if (!properties.has(key)) return JsonScalarCell{};
  return JsonScalarCell{properties.get(key)};
}

// The row-receiver form the 42 proven Lottie row reads use: a structural row over the parsed JSON
// owner, read by declared member name. The value and its presence are the raw JSON cell's; no typed
// row cell is fabricated and nothing is coerced at the read.
template <typename Schema>
[[nodiscard]] inline JsonScalarCell read_json_scalar_row_member(const StructuralRef<Schema>& row,
                                                                const String& key) {
  const auto properties = named_properties(row);
  if (!properties.has(key)) return JsonScalarCell{};
  return JsonScalarCell{properties.get(key)};
}

// (a) Lazy JSON-backed structural row. An object parsed by JSON.parse becomes a structural row over
// the SAME parsed owner (no recursive validation, no copy, no tree materialization). A null or
// primitive root is NOT a row: this answers nullopt so the assertion call site can keep the parsed
// value unchanged for the source's own null/primitive check instead of throwing at the assertion.
template <typename Schema>
[[nodiscard]] inline std::optional<StructuralRef<Schema>> json_structural_row(const JsonValue& value) {
  if (value.kind() != JsonValue::Kind::object) return std::nullopt;
  const auto properties = named_properties(value);
  if (!properties.shared_owner()) return std::nullopt;
  return StructuralRef<Schema>::from_owner(properties.shared_owner());
}

// A bounded assertion carrier for `JSON.parse(x) as Document`. It retains the ORIGINAL JsonValue root
// as well as the object-backed row, so a null root stays null and a primitive root keeps its own
// truthiness/typeof/ToNumber semantics; only an object root dispatches member reads into the row.
// No schema copy, no recursive materialization, no global ABI.
template <typename Schema>
class JsonAssertionRow final {
 public:
  explicit JsonAssertionRow(JsonValue value) : value_(std::move(value)) {}

  [[nodiscard]] const JsonValue& raw() const noexcept { return value_; }
  [[nodiscard]] bool is_object_root() const noexcept { return value_.kind() == JsonValue::Kind::object; }
  [[nodiscard]] std::optional<StructuralRef<Schema>> row() const {
    return json_structural_row<Schema>(value_);
  }

  [[nodiscard]] bool is_nullish() const noexcept { return value_.kind() == JsonValue::Kind::null; }

  [[nodiscard]] bool to_boolean() const {
    return Any(value_).to_boolean();
  }

  [[nodiscard]] std::string_view type_of() const noexcept {
    switch (value_.kind()) {
      case JsonValue::Kind::null: return "object";
      case JsonValue::Kind::boolean: return "boolean";
      case JsonValue::Kind::number: return "number";
      case JsonValue::Kind::string: return "string";
      case JsonValue::Kind::array:
      case JsonValue::Kind::object: return "object";
    }
    return "undefined";
  }

  [[nodiscard]] double to_number() const {
    return JsonScalarCell{Any(value_)}.to_number();
  }

  // Member dispatch: an object root reads the row's raw scalar cell; a primitive root answers
  // undefined (no throw); a null root throws TypeError exactly at the member read.
  [[nodiscard]] JsonScalarCell read_member(const String& key) const {
    if (is_nullish()) throw TypeError("Cannot read properties of null or undefined");
    if (!is_object_root()) return JsonScalarCell{};
    const auto object_row = row();
    if (!object_row) return JsonScalarCell{};
    return read_json_scalar_row_member(*object_row, key);
  }

 private:
  JsonValue value_;
};

// The assertion answer at the ABI the generated code already uses: a plain StructuralRef<Schema>.
// An object or array root is a row over the parsed owner exactly as json_structural_row builds it.
// A primitive or null root is a row over this bounded owner, which keeps the raw value so the
// declared row type crosses parameters, returns and context fields unchanged:
//   - typeof: null -> "object", number/string/boolean -> their own name;
//   - truthiness and ToNumber come from the raw value;
//   - a member read on null throws TypeError, exactly where JavaScript does;
//   - a member read on a number or boolean is absent (JavaScript yields undefined);
//   - a string root owns `length` and its UTF-16 indices and nothing else.
// Nothing is copied, no schema is generated, and the row type itself is unchanged.
class JsonRootRowOwner final : public RowOwner {
 public:
  explicit JsonRootRowOwner(Any root) : root_(std::move(root)) {}

  [[nodiscard]] std::shared_ptr<void> native_object() const noexcept override { return {}; }
  [[nodiscard]] std::type_index native_type() const noexcept override { return typeid(void); }

  [[nodiscard]] std::shared_ptr<RowCell> named_cell(std::string_view name) const override {
    require_member_access();
    const auto key = String::from_utf8(name);
    if (root_.kind() == AnyKind::string) {
      if (!string_member_present(key)) return {};
      const Any root = root_;
      return std::make_shared<detail::ReadonlyNamedCell>([root, key] { return read_json_data_property(root, key); });
    }
    // A native (or boxed) array root owns `length` and its indices, exactly as JavaScript does. A boxed
    // native Array reaches here because it has no erased row owner of its own.
    if (root_.is_array()) {
      const Any root = root_;
      if (key == String("length")) {
        return std::make_shared<detail::ReadonlyNamedCell>(
            [root] { return Any(static_cast<double>(root.array_size())); });
      }
      const auto index = detail::record_array_index(key);
      if (!index || *index >= root.array_size()) return {};
      return std::make_shared<detail::ReadonlyNamedCell>(
          [root, position = *index] { return root.array_get(static_cast<double>(position)).value_or(Any(undefined)); });
    }
    return {};
  }

  [[nodiscard]] bool has_named_cell(std::string_view name) const override {
    require_member_access();
    const auto key = String::from_utf8(name);
    if (root_.kind() == AnyKind::string) return string_member_present(key);
    if (root_.is_array()) {
      if (key == String("length")) return true;
      const auto index = detail::record_array_index(key);
      return index && *index < root_.array_size();
    }
    return false;
  }

  [[nodiscard]] std::optional<std::vector<String>> enumerable_string_keys() const override {
    require_member_access();
    std::vector<String> keys;
    if (root_.kind() == AnyKind::string) {
      const auto& units = root_.as_string().native();
      for (std::size_t index = 0; index < units.size(); ++index) {
        keys.push_back(String::from_number(static_cast<double>(index)));
      }
      keys.push_back(String("length"));
      return keys;
    }
    // Object.keys(array) enumerates the indices; `length` is present for `in` but not enumerable.
    if (root_.is_array()) {
      const auto size = root_.array_size();
      for (std::size_t index = 0; index < size; ++index) {
        keys.push_back(String::from_number(static_cast<double>(index)));
      }
    }
    return keys;
  }

  [[nodiscard]] const Any& root() const noexcept { return root_; }
  [[nodiscard]] bool is_nullish() const noexcept { return root_.is_nullish(); }

  [[nodiscard]] std::optional<Any> raw_value() const override { return root_; }

  [[nodiscard]] bool to_boolean() const noexcept { return root_.to_boolean(); }

  [[nodiscard]] double to_number() const { return JsonScalarCell{root_}.to_number(); }

  [[nodiscard]] std::string_view type_of() const noexcept {
    switch (root_.kind()) {
      case AnyKind::null: return "object";
      case AnyKind::boolean: return "boolean";
      case AnyKind::number: return "number";
      case AnyKind::string: return "string";
      case AnyKind::undefined: return "undefined";
      default: return "object";
    }
  }

 private:
  void require_member_access() const {
    if (root_.is_nullish()) throw TypeError("Cannot read properties of null or undefined");
  }

  [[nodiscard]] bool string_member_present(const String& key) const {
    const auto& units = root_.as_string().native();
    if (key == String("length")) return true;
    const auto& digits = key.native();
    if (digits.empty() || (digits.size() > 1 && digits.front() == u'0')) return false;
    std::size_t index = 0;
    for (const auto digit_char : digits) {
      if (digit_char < u'0' || digit_char > u'9') return false;
      const auto digit = static_cast<std::size_t>(digit_char - u'0');
      if (digit >= units.size() || index > (units.size() - 1 - digit) / 10) return false;
      index = index * 10 + digit;
    }
    return true;
  }

  Any root_;
};

template <typename Schema>
[[nodiscard]] inline StructuralRef<Schema> json_assertion_row(const JsonValue& value) {
  if (value.kind() == JsonValue::Kind::object || value.kind() == JsonValue::Kind::array) {
    const auto properties = named_properties(value);
    if (properties.shared_owner()) return StructuralRef<Schema>::from_owner(properties.shared_owner());
  }
  return StructuralRef<Schema>::from_owner(std::make_shared<JsonRootRowOwner>(Any(value)));
}

template <typename Schema>
[[nodiscard]] inline std::string_view json_row_type_of(const StructuralRef<Schema>& row) {
  if (const auto* root = dynamic_cast<const JsonRootRowOwner*>(row.shared_owner().get())) return root->type_of();
  return "object";
}

template <typename Schema>
[[nodiscard]] inline bool json_row_is_nullish(const StructuralRef<Schema>& row) {
  if (const auto* root = dynamic_cast<const JsonRootRowOwner*>(row.shared_owner().get())) return root->is_nullish();
  return false;
}

template <typename Schema>
[[nodiscard]] inline bool json_row_to_boolean(const StructuralRef<Schema>& row) {
  if (const auto* root = dynamic_cast<const JsonRootRowOwner*>(row.shared_owner().get())) return root->to_boolean();
  return true;
}


// Direct, same-owner access to a parsed JSON document carried as a structural row. Every operation
// works on the row's existing owner: no copy, no second tree, no schema validation and no election of
// an array-vs-object shape. A value that JSON cannot represent (undefined, symbol, callable, opaque
// host object) is refused rather than coerced.
template <typename Schema>
[[nodiscard]] inline std::shared_ptr<JsonObject> json_row_object(const StructuralRef<Schema>& row) {
  const auto& owner = row.shared_owner();
  if (!owner || owner->native_type() != typeid(JsonObject)) return {};
  return std::static_pointer_cast<JsonObject>(owner->native_object());
}

template <typename Schema>
[[nodiscard]] inline std::shared_ptr<JsonArray> json_row_array(const StructuralRef<Schema>& row) {
  const auto& owner = row.shared_owner();
  if (!owner || owner->native_type() != typeid(JsonArray)) return {};
  return std::static_pointer_cast<JsonArray>(owner->native_object());
}

template <typename Schema>
[[nodiscard]] inline bool json_row_is_object(const StructuralRef<Schema>& row) {
  if (json_row_object(row)) return true;
  // A native owner's boxed value is an object the same way; an array or a primitive is not.
  const Any raw = json_row_raw(row);
  return raw.kind() == AnyKind::object && !raw.is_array();
}

template <typename Schema>
[[nodiscard]] inline bool json_row_is_array(const StructuralRef<Schema>& row) {
  if (json_row_array(row)) return true;
  const Any raw = json_row_raw(row);
  return raw.is_array();
}

template <typename Schema>
[[nodiscard]] inline bool json_row_has(const StructuralRef<Schema>& row, const String& key) {
  if (const auto object = json_row_object(row)) return object->has(key);
  if (const auto array = json_row_array(row)) {
    if (key == String("length")) return true;
    const auto& digits = key.native();
    if (digits.empty() || (digits.size() > 1 && digits.front() == u'0')) return false;
    std::size_t index = 0;
    for (const auto digit_char : digits) {
      if (digit_char < u'0' || digit_char > u'9') return false;
      const auto digit = static_cast<std::size_t>(digit_char - u'0');
      if (digit >= array->size() || index > (array->size() - 1 - digit) / 10) return false;
      index = index * 10 + digit;
    }
    return true;
  }
  const auto view = named_properties(row);
  return view.shared_owner() && view.has(key);
}

template <typename Schema>
[[nodiscard]] inline Any json_row_member(const StructuralRef<Schema>& row, const String& key) {
  if (const auto object = json_row_object(row)) {
    const auto value = object->get(key);
    return value ? Any(*value) : Any(undefined);
  }
  if (const auto array = json_row_array(row)) {
    const auto index = detail::record_array_index(key);
    if (index && *index < array->size()) return Any((*array)[*index]);
  }
  const auto view = named_properties(row);
  if (view.shared_owner() && view.has(key)) return view.get(key);
  return Any(undefined);
}

template <typename Schema>
[[nodiscard]] inline NamedProperties json_row_member_view(const StructuralRef<Schema>& row, const String& key) {
  const Any member = json_row_member(row, key);
  if (member.is_nullish()) return {};
  return named_properties(member);
}

template <typename Schema>
[[nodiscard]] inline bool json_row_set(const StructuralRef<Schema>& row, const String& key, const Any& value) {
  const auto object = json_row_object(row);
  if (!object) return false;
  const auto json = any_to_json_value(value);
  if (!json) return false;
  object->set(key, *json);
  return true;
}

template <typename Schema>
[[nodiscard]] inline bool json_row_erase(const StructuralRef<Schema>& row, const String& key) {
  const auto object = json_row_object(row);
  return object && object->erase(key);
}

template <typename Schema>
[[nodiscard]] inline double json_row_length(const StructuralRef<Schema>& row) {
  if (const auto array = json_row_array(row)) return static_cast<double>(array->size());
  const Any raw = json_row_raw(row);
  return raw.is_array() ? static_cast<double>(raw.array_size()) : 0.0;
}

template <typename Schema>
[[nodiscard]] inline Any json_row_index(const StructuralRef<Schema>& row, double index) {
  if (!std::isfinite(index) || index < 0 || std::floor(index) != index) return Any(undefined);
  if (const auto array = json_row_array(row)) {
    if (index >= static_cast<double>(array->size())) return Any(undefined);
    return Any((*array)[static_cast<std::size_t>(index)]);
  }
  const Any raw = json_row_raw(row);
  if (!raw.is_array()) return Any(undefined);
  return raw.array_get(index).value_or(Any(undefined));
}

template <typename Schema>
[[nodiscard]] inline bool json_row_set_index(const StructuralRef<Schema>& row, double index, const Any& value) {
  const auto array = json_row_array(row);
  if (!array || !std::isfinite(index) || index < 0 || std::floor(index) != index ||
      index >= static_cast<double>(array->size())) {
    return false;
  }
  const auto json = any_to_json_value(value);
  if (!json) return false;
  (*array)[static_cast<std::size_t>(index)] = *json;
  return true;
}

template <typename Schema>
[[nodiscard]] inline bool json_row_insert(const StructuralRef<Schema>& row, double index, const Any& value) {
  const auto array = json_row_array(row);
  if (!array || !std::isfinite(index) || index < 0 || std::floor(index) != index ||
      index > static_cast<double>(array->size())) {
    return false;
  }
  const auto json = any_to_json_value(value);
  if (!json) return false;
  array->insert(static_cast<std::size_t>(index), *json);
  return true;
}

template <typename Schema>
[[nodiscard]] inline bool json_row_erase_index(const StructuralRef<Schema>& row, double index) {
  const auto array = json_row_array(row);
  if (!array || !std::isfinite(index) || index < 0 || std::floor(index) != index ||
      index >= static_cast<double>(array->size())) {
    return false;
  }
  return array->erase(static_cast<std::size_t>(index));
}

// Any-receiver assertion row for NESTED parsed values: an object/array Any reuses the parsed owner the
// value already holds; every other Any becomes the bounded root row over that raw value.
template <typename Schema>
[[nodiscard]] inline StructuralRef<Schema> json_assertion_row(const Any& value) {
  if (value.kind() == AnyKind::object) {
    const auto properties = named_properties(value);
    if (properties.shared_owner()) return StructuralRef<Schema>::from_owner(properties.shared_owner());
  }
  return StructuralRef<Schema>::from_owner(std::make_shared<JsonRootRowOwner>(value));
}

// The original root an assertion row was built from, retaining its identity: a parsed object/array
// owner is the same shared container the row reads; a primitive/null root returns its raw Any.
// This performs NO member read, so it cannot trigger a null member-access TypeError.
template <typename Schema>
[[nodiscard]] inline Any json_row_raw(const StructuralRef<Schema>& row) {
  const auto owner = row.shared_owner();
  if (!owner) return Any();
  if (const auto raw = owner->raw_value()) return *raw;
  return Any();
}

} // namespace flight
