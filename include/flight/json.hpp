#pragma once

#include <algorithm>
#include <charconv>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <flight/array.hpp>
#include <flight/presence.hpp>
#include <flight/string.hpp>

namespace flight {

template <typename Key, typename Value>
class Record;

class JsonArray;
class JsonObject;

class JsonSyntaxError final : public std::runtime_error {
 public:
  JsonSyntaxError() : std::runtime_error("invalid JSON text") {}
};

class BadJsonValueAccess final : public std::runtime_error {
 public:
  BadJsonValueAccess() : std::runtime_error("flight::JsonValue kind mismatch") {}
};

class JsonValue {
 public:
  enum class Kind { null, boolean, number, string, array, object };

  JsonValue() noexcept = default;
  JsonValue(Null) noexcept {}
  JsonValue(std::nullptr_t) noexcept {}
  JsonValue(bool value) : value_(value) {}

  template <typename Number>
    requires(std::is_arithmetic_v<Number> && !std::same_as<std::remove_cvref_t<Number>, bool>)
  JsonValue(Number value) : value_(static_cast<double>(value)) {}

  JsonValue(String value) : value_(std::move(value)) {}
  JsonValue(const char* value) : value_(String(value)) {}
  JsonValue(JsonArray value);
  JsonValue(JsonObject value);

  [[nodiscard]] Kind kind() const noexcept { return static_cast<Kind>(value_.index()); }
  [[nodiscard]] bool is_null() const noexcept { return std::holds_alternative<Null>(value_); }
  [[nodiscard]] bool as_boolean() const { return require<bool>(); }
  [[nodiscard]] double as_number() const { return require<double>(); }
  [[nodiscard]] const String& as_string() const { return require<String>(); }
  [[nodiscard]] JsonArray& as_array() { return *require<std::shared_ptr<JsonArray>>(); }
  [[nodiscard]] const JsonArray& as_array() const { return *require<std::shared_ptr<JsonArray>>(); }
  [[nodiscard]] JsonObject& as_object() { return *require<std::shared_ptr<JsonObject>>(); }
  [[nodiscard]] const JsonObject& as_object() const { return *require<std::shared_ptr<JsonObject>>(); }

  // These handles retain the parsed container itself. A dynamic property/array reader must not
  // rebuild it as Record or Array storage: aliases of the parsed value share this exact owner.
  [[nodiscard]] const std::shared_ptr<JsonArray>& array_owner() const {
    return require<std::shared_ptr<JsonArray>>();
  }
  [[nodiscard]] const std::shared_ptr<JsonObject>& object_owner() const {
    return require<std::shared_ptr<JsonObject>>();
  }

 private:
  template <typename Value>
  [[nodiscard]] Value& require() {
    auto* value = std::get_if<Value>(&value_);
    if (!value) throw BadJsonValueAccess();
    return *value;
  }

  template <typename Value>
  [[nodiscard]] const Value& require() const {
    const auto* value = std::get_if<Value>(&value_);
    if (!value) throw BadJsonValueAccess();
    return *value;
  }

  std::variant<Null, bool, double, String, std::shared_ptr<JsonArray>, std::shared_ptr<JsonObject>> value_;
};

class JsonArray {
 public:
  using const_iterator = std::vector<JsonValue>::const_iterator;
  using iterator = std::vector<JsonValue>::iterator;
  using size_type = std::vector<JsonValue>::size_type;

  JsonArray() = default;
  JsonArray(std::initializer_list<JsonValue> values) : values_(values) {}

  [[nodiscard]] size_type size() const noexcept { return values_.size(); }
  [[nodiscard]] bool empty() const noexcept { return values_.empty(); }
  [[nodiscard]] JsonValue& operator[](size_type index) noexcept { return values_[index]; }
  [[nodiscard]] const JsonValue& operator[](size_type index) const noexcept { return values_[index]; }
  [[nodiscard]] iterator begin() noexcept { return values_.begin(); }
  [[nodiscard]] const_iterator begin() const noexcept { return values_.begin(); }
  [[nodiscard]] iterator end() noexcept { return values_.end(); }
  [[nodiscard]] const_iterator end() const noexcept { return values_.end(); }

  JsonArray& push(JsonValue value) {
    values_.push_back(std::move(value));
    return *this;
  }

 private:
  std::vector<JsonValue> values_;
};

class JsonObject {
 public:
  using Entry = std::pair<String, JsonValue>;
  using const_iterator = std::vector<Entry>::const_iterator;
  using iterator = std::vector<Entry>::iterator;

  JsonObject() = default;
  JsonObject(std::initializer_list<Entry> entries) {
    for (const auto& [key, value] : entries) set(key, value);
  }

  JsonObject& set(String key, JsonValue value) {
    const auto found = find(key);
    if (found == entries_.end()) entries_.emplace_back(std::move(key), std::move(value));
    else found->second = std::move(value);
    return *this;
  }

  [[nodiscard]] std::optional<JsonValue> get(const String& key) const {
    const auto found = find(key);
    return found == entries_.end() ? std::nullopt : std::optional<JsonValue>(found->second);
  }

  [[nodiscard]] bool has(const String& key) const { return find(key) != entries_.end(); }
  [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
  [[nodiscard]] const_iterator begin() const noexcept { return entries_.begin(); }
  [[nodiscard]] const_iterator end() const noexcept { return entries_.end(); }

 private:
  [[nodiscard]] iterator find(const String& key) {
    return std::find_if(entries_.begin(), entries_.end(), [&](const Entry& entry) { return entry.first == key; });
  }

  [[nodiscard]] const_iterator find(const String& key) const {
    return std::find_if(entries_.begin(), entries_.end(), [&](const Entry& entry) { return entry.first == key; });
  }

  std::vector<Entry> entries_;
};

inline JsonValue::JsonValue(JsonArray value)
    : value_(std::make_shared<JsonArray>(std::move(value))) {}

inline JsonValue::JsonValue(JsonObject value)
    : value_(std::make_shared<JsonObject>(std::move(value))) {}

// Parsed arrays retain their JSON carrier. Array.isArray must inspect that carrier rather than
// select array.hpp's non-array fallback (or require a conversion to Any first).
[[nodiscard]] inline bool is_array(const JsonValue& value) noexcept {
  return value.kind() == JsonValue::Kind::array;
}

[[nodiscard]] inline constexpr bool is_array(const JsonArray&) noexcept { return true; }

namespace detail {

inline void append_json_string(std::string& output, const String& value) {
  output.push_back('"');
  for (const unsigned char unit : value.to_utf8()) {
    switch (unit) {
      case '"': output.append("\\\""); break;
      case '\\': output.append("\\\\"); break;
      case '\b': output.append("\\b"); break;
      case '\f': output.append("\\f"); break;
      case '\n': output.append("\\n"); break;
      case '\r': output.append("\\r"); break;
      case '\t': output.append("\\t"); break;
      default:
        if (unit < 0x20U) {
          std::ostringstream escaped;
          escaped << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<unsigned>(unit);
          output.append(escaped.str());
        } else {
          output.push_back(static_cast<char>(unit));
        }
    }
  }
  output.push_back('"');
}

inline void append_json_indent(std::string& output, std::size_t depth, std::size_t width) {
  output.append(depth * width, ' ');
}

template <typename Value>
void append_json_value(std::string& output, const Value& value, std::size_t indentation, std::size_t depth);

template <typename Owner>
void append_json_row(std::string& output, const Owner& owner, std::size_t indentation, std::size_t depth);

template <typename Erased>
void append_json_erased(std::string& output, const Erased& value, std::size_t indentation, std::size_t depth);

template <typename Record>
void append_json_record(std::string& output, const Record& record, std::size_t indentation, std::size_t depth);

// flight::Record is recognized by its template, not by shape: a Map has the same entry iteration and
// JSON.stringify writes a Map as `{}`, so only a Record may take the object path. Declared rather than
// included, so this header keeps its dependency-free footprint.
template <typename Type>
inline constexpr bool is_json_record = false;
template <typename Key, typename Value>
inline constexpr bool is_json_record<Record<Key, Value>> = true;

template <typename Type>
concept json_complete_type = requires { sizeof(Type); };

template <typename Range>
inline constexpr bool json_bool_range = false;
template <typename Range>
  requires requires { typename Range::value_type; }
inline constexpr bool json_bool_range<Range> = std::same_as<typename Range::value_type, bool>;

template <typename Range>
void append_json_array(std::string& output, const Range& values, std::size_t indentation, std::size_t depth) {
  output.push_back('[');
  bool first = true;
  for (const auto& value : values) {
    if (!first) output.push_back(',');
    if (indentation > 0) {
      output.push_back('\n');
      append_json_indent(output, depth + 1, indentation);
    }
    // A container of bool yields a proxy reference, not a bool; the element's own value is what is written.
    if constexpr (json_bool_range<Range>) {
      append_json_value(output, static_cast<bool>(value), indentation, depth + 1);
    } else {
      append_json_value(output, value, indentation, depth + 1);
    }
    first = false;
  }
  if (!first && indentation > 0) {
    output.push_back('\n');
    append_json_indent(output, depth, indentation);
  }
  output.push_back(']');
}

inline void append_json_object(
    std::string& output,
    const JsonObject& object,
    std::size_t indentation,
    std::size_t depth) {
  output.push_back('{');
  bool first = true;
  for (const auto& [key, value] : object) {
    if (!first) output.push_back(',');
    if (indentation > 0) {
      output.push_back('\n');
      append_json_indent(output, depth + 1, indentation);
    }
    append_json_string(output, key);
    output.push_back(':');
    if (indentation > 0) output.push_back(' ');
    append_json_value(output, value, indentation, depth + 1);
    first = false;
  }
  if (!first && indentation > 0) {
    output.push_back('\n');
    append_json_indent(output, depth, indentation);
  }
  output.push_back('}');
}

template <typename Value>
void append_json_value(std::string& output, const Value& value, std::size_t indentation, std::size_t depth) {
  using Type = std::remove_cvref_t<Value>;
  if constexpr (std::same_as<Type, JsonValue>) {
    switch (value.kind()) {
      case JsonValue::Kind::null: output.append("null"); break;
      case JsonValue::Kind::boolean: append_json_value(output, value.as_boolean(), indentation, depth); break;
      case JsonValue::Kind::number: append_json_value(output, value.as_number(), indentation, depth); break;
      case JsonValue::Kind::string: append_json_string(output, value.as_string()); break;
      case JsonValue::Kind::array: append_json_array(output, value.as_array(), indentation, depth); break;
      case JsonValue::Kind::object: append_json_object(output, value.as_object(), indentation, depth); break;
    }
  } else if constexpr (std::same_as<Type, JsonArray>) {
    append_json_array(output, value, indentation, depth);
  } else if constexpr (std::same_as<Type, JsonObject>) {
    append_json_object(output, value, indentation, depth);
  } else if constexpr (std::same_as<Type, String>) {
    append_json_string(output, value);
  } else if constexpr (std::same_as<Type, bool>) {
    output.append(value ? "true" : "false");
  } else if constexpr (std::integral<Type> || std::floating_point<Type>) {
    if constexpr (std::floating_point<Type>) {
      if (!std::isfinite(value)) {
        output.append("null");
        return;
      }
    }
    output.append(String::from_number(static_cast<double>(value)).to_utf8());
  } else if constexpr (std::same_as<Type, Null> || std::same_as<Type, Undefined> || std::same_as<Type, std::nullptr_t>) {
    output.append("null");
  } else if constexpr (requires { value.has_value(); *value; } && !requires { value.index(); }) {
    if (value.has_value()) append_json_value(output, *value, indentation, depth);
    else output.append("null");
  } else if constexpr (requires { value.index(); std::visit([](const auto&) {}, value); }) {
    std::visit([&](const auto& alternative) { append_json_value(output, alternative, indentation, depth); }, value);
  } else if constexpr (requires { typename Type::value_type; value.begin(); value.end(); } &&
                       requires { is_array(value); }) {
    append_json_array(output, value, indentation, depth);
  } else if constexpr (requires { typename Type::element_type; value.get(); }) {
    if (value) append_json_value(output, *value, indentation, depth);
    else output.append("null");
  } else if constexpr (is_json_record<Type>) {
    append_json_record(output, value, indentation, depth);
  } else if constexpr (requires { value.shared_owner(); }) {
    append_json_row(output, value.shared_owner(), indentation, depth);
  } else if constexpr (requires {
                         value.kind();
                         value.is_undefined();
                         value.as_number();
                         value.as_string();
                         value.as_boolean();
                       }) {
    append_json_erased(output, value, indentation, depth);
  } else {
    output.append("{}");
  }
}

// A structural row, serialized from its owner's own enumerable string keys in source declaration order,
// which is the order JavaScript would enumerate them in.
//
// Two properties are OMITTED rather than written, matching `JSON.stringify` on an object: one whose
// value is `undefined`, and one whose cell has no erased representation at all. The second is almost
// always a function-valued member, and JavaScript omits those too, so the agreement is real rather than
// convenient. `named_cell(...)->as_any()` returning nothing is how the runtime says "not representable",
// and taking it as "omit" is what keeps this from throwing out of a cache-key computation.
//
// Reached by duck typing on `shared_owner()` so this header stays free of a dependency on
// structural_ref.hpp: the branch only instantiates for a caller that already has the type.
template <typename Owner>
void append_json_row(std::string& output, const Owner& owner, std::size_t indentation, std::size_t depth) {
  if (!owner) {
    output.append("null");
    return;
  }
  output.push_back('{');
  bool first = true;
  for (const auto& key : owner->named_keys()) {
    const auto cell = owner->named_cell(key);
    if (!cell) continue;
    auto held = cell->as_any();
    if (!held || held->is_undefined()) continue;
    if (!first) output.push_back(',');
    if (indentation > 0) {
      output.push_back('\n');
      append_json_indent(output, depth + 1, indentation);
    }
    append_json_string(output, String::from_utf8(key));
    output.push_back(':');
    if (indentation > 0) output.push_back(' ');
    append_json_value(output, *held, indentation, depth + 1);
    first = false;
  }
  if (!first && indentation > 0) {
    output.push_back('\n');
    append_json_indent(output, depth, indentation);
  }
  output.push_back('}');
}

// An erased dynamic value, dispatched on the kind tag it carries rather than on its C++ type.
//
// `symbol`, `function` and `external` become `null`. JavaScript omits a function-valued PROPERTY, which
// append_json_row does, but a function reached any other way stringifies as null, and these three have
// no JSON form of their own. An `object` has one, and this does not yet produce it: the erased row owner
// an Any carries is a `shared_ptr<void>`, so its keys cannot be read back without the concrete type.
// That case keeps the long-standing `{}` rather than inventing something. An array is the exception: an Any
// holding one keeps its concrete element type, so an array of dynamic values, numbers, strings or booleans is
// written as the JSON array JavaScript writes; an array of any other element type keeps `{}` with the rest.
template <typename Erased, typename Element>
bool append_json_erased_array(std::string& output, const Erased& value, std::size_t indentation, std::size_t depth) {
  if constexpr (json_complete_type<Array<Element>>) {
    if (const auto* array = value.template external_if<Array<Element>>()) {
      append_json_value(output, *array, indentation, depth);
      return true;
    }
  }
  return false;
}

template <typename Erased>
void append_json_erased(std::string& output, const Erased& value, std::size_t indentation, std::size_t depth) {
  switch (value.kind()) {
    case std::remove_cvref_t<decltype(value.kind())>::boolean:
      append_json_value(output, value.as_boolean(), indentation, depth);
      return;
    case std::remove_cvref_t<decltype(value.kind())>::number:
      append_json_value(output, value.as_number(), indentation, depth);
      return;
    case std::remove_cvref_t<decltype(value.kind())>::string:
      append_json_string(output, value.as_string());
      return;
    case std::remove_cvref_t<decltype(value.kind())>::object:
      if constexpr (requires { value.template object_if<JsonObject>(); }) {
        if (const auto object = value.template object_if<JsonObject>()) {
          append_json_value(output, *object, indentation, depth);
          return;
        }
        if (const auto* array = value.template external_if<std::shared_ptr<JsonArray>>()) {
          append_json_value(output, **array, indentation, depth);
          return;
        }
      }
      if constexpr (requires { value.is_array(); }) {
        if (value.is_array() && (append_json_erased_array<Erased, Erased>(output, value, indentation, depth) ||
                                 append_json_erased_array<Erased, double>(output, value, indentation, depth) ||
                                 append_json_erased_array<Erased, String>(output, value, indentation, depth) ||
                                 append_json_erased_array<Erased, bool>(output, value, indentation, depth))) {
          return;
        }
      }
      output.append("{}");
      return;
    case std::remove_cvref_t<decltype(value.kind())>::external:
      // A Record value travels in an Any as an external -- the runtime's carrier for one -- and is the
      // object JavaScript would serialize. Any other external has no JSON form.
      if constexpr (json_complete_type<Record<String, Erased>>) {
        if (const auto* record = value.template external_if<Record<String, Erased>>()) {
          append_json_record(output, *record, indentation, depth);
          return;
        }
      }
      output.append("null");
      return;
    default: output.append("null"); return;
  }
}

// Whether JSON.stringify leaves an object property out: a value that is undefined, a function or a symbol.
template <typename Value>
[[nodiscard]] bool json_omits_property(const Value& value) {
  using Type = std::remove_cvref_t<Value>;
  if constexpr (std::same_as<Type, Undefined>) {
    return true;
  } else if constexpr (requires { value.type_of(); value.is_undefined(); }) {
    const auto type = value.type_of();
    return value.is_undefined() || type == String("function") || type == String("symbol");
  } else if constexpr (requires { value.has_value(); *value; } && !requires { value.index(); }) {
    return !value.has_value();
  } else {
    return false;
  }
}

// A flight::Record is a JavaScript object: its own keys in the record's order -- which is already the order
// JavaScript enumerates, integer-like keys first -- each written as a string. A number key is written as its
// string form; a symbol-keyed property is skipped, as JSON.stringify skips symbol keys; a property whose value is
// undefined, a function or a symbol is omitted.
template <typename Record>
void append_json_record(std::string& output, const Record& record, std::size_t indentation, std::size_t depth) {
  output.push_back('{');
  bool first = true;
  for (const auto& [key, value] : record) {
    using Key = std::remove_cvref_t<decltype(key)>;
    String name;
    if constexpr (std::same_as<Key, String>) {
      name = key;
    } else if constexpr (std::floating_point<Key> || std::integral<Key>) {
      name = String::from_number(static_cast<double>(key));
    } else {
      continue;
    }
    if (json_omits_property(value)) continue;
    if (!first) output.push_back(',');
    if (indentation > 0) {
      output.push_back('\n');
      append_json_indent(output, depth + 1, indentation);
    }
    append_json_string(output, name);
    output.push_back(':');
    if (indentation > 0) output.push_back(' ');
    append_json_value(output, value, indentation, depth + 1);
    first = false;
  }
  if (!first && indentation > 0) {
    output.push_back('\n');
    append_json_indent(output, depth, indentation);
  }
  output.push_back('}');
}

class JsonParser {
 public:
  explicit JsonParser(const String& source) : source_(source.native()) {}

  [[nodiscard]] JsonValue parse() {
    skip_whitespace();
    auto result = parse_value();
    skip_whitespace();
    if (position_ != source_.size()) throw JsonSyntaxError();
    return result;
  }

 private:
  [[nodiscard]] JsonValue parse_value() {
    if (position_ == source_.size()) throw JsonSyntaxError();
    switch (source_[position_]) {
      case 'n': consume(u"null"); return nullptr;
      case 't': consume(u"true"); return true;
      case 'f': consume(u"false"); return false;
      case '"': return parse_string();
      case '[': return parse_array();
      case '{': return parse_object();
      default: return parse_number();
    }
  }

  [[nodiscard]] String parse_string() {
    expect('"');
    std::u16string result;
    while (position_ < source_.size()) {
      const char16_t unit = source_[position_++];
      if (unit == '"') return String(std::move(result));
      if (unit < 0x20U) throw JsonSyntaxError();
      if (unit != '\\') {
        result.push_back(unit);
        continue;
      }
      if (position_ == source_.size()) throw JsonSyntaxError();
      switch (source_[position_++]) {
        case '"': result.push_back('"'); break;
        case '\\': result.push_back('\\'); break;
        case '/': result.push_back('/'); break;
        case 'b': result.push_back('\b'); break;
        case 'f': result.push_back('\f'); break;
        case 'n': result.push_back('\n'); break;
        case 'r': result.push_back('\r'); break;
        case 't': result.push_back('\t'); break;
        case 'u': {
          // JSON strings, like JavaScript strings, contain UTF-16 code units. An escaped
          // surrogate is valid on its own and must not be rejected or replaced with U+FFFD.
          result.push_back(static_cast<char16_t>(parse_hex_quad()));
          break;
        }
        default: throw JsonSyntaxError();
      }
    }
    throw JsonSyntaxError();
  }

  [[nodiscard]] JsonValue parse_array() {
    expect('[');
    JsonArray result;
    skip_whitespace();
    if (take(']')) return result;
    while (true) {
      skip_whitespace();
      result.push(parse_value());
      skip_whitespace();
      if (take(']')) return result;
      expect(',');
    }
  }

  [[nodiscard]] JsonValue parse_object() {
    expect('{');
    JsonObject result;
    skip_whitespace();
    if (take('}')) return result;
    while (true) {
      skip_whitespace();
      if (position_ == source_.size() || source_[position_] != '"') throw JsonSyntaxError();
      auto key = parse_string();
      skip_whitespace();
      expect(':');
      skip_whitespace();
      result.set(std::move(key), parse_value());
      skip_whitespace();
      if (take('}')) return result;
      expect(',');
    }
  }

  [[nodiscard]] JsonValue parse_number() {
    const auto begin = position_;
    static_cast<void>(take('-'));
    if (take('0')) {
      if (position_ < source_.size() && source_[position_] >= '0' && source_[position_] <= '9') {
        throw JsonSyntaxError();
      }
    } else {
      take_digits(true);
    }
    if (take('.')) take_digits(true);
    if (take('e') || take('E')) {
      static_cast<void>(take('+') || take('-'));
      take_digits(true);
    }
    if (position_ == begin) throw JsonSyntaxError();
    double value = 0.0;
    // The grammar above admits only ASCII here. from_chars supplies correctly rounded finite
    // values, but reports range loss where JSON.parse produces infinity or signed zero.
    const std::string text(source_.data() + begin, source_.data() + position_);
    const auto converted = std::from_chars(text.data(), text.data() + text.size(), value);
    if (converted.ptr != text.data() + text.size()) throw JsonSyntaxError();
    if (converted.ec == std::errc::result_out_of_range) {
      const auto exponent = text.find_first_of("eE");
      const auto end = exponent == std::string::npos ? text.size() : exponent;
      std::size_t digits_before_decimal = 0;
      std::optional<std::size_t> first_nonzero;
      std::size_t digit = 0;
      bool before_decimal = true;
      for (std::size_t index = text.front() == '-' ? 1 : 0; index < end; ++index) {
        if (text[index] == '.') { before_decimal = false; continue; }
        if (before_decimal) ++digits_before_decimal;
        if (!first_nonzero && text[index] != '0') first_nonzero = digit;
        ++digit;
      }
      // Saturating beyond the entire significand length suffices to decide which end of
      // the double range was exceeded, even for a very long authored decimal/exponent.
      const long double limit = static_cast<long double>(text.size()) + 400.0L;
      long double explicit_exponent = 0;
      if (exponent != std::string::npos) {
        auto index = exponent + 1;
        const bool negative = text[index] == '-';
        if (text[index] == '-' || text[index] == '+') ++index;
        for (; index < text.size(); ++index) {
          explicit_exponent = std::min(limit, explicit_exponent * 10 + (text[index] - '0'));
        }
        if (negative) explicit_exponent = -explicit_exponent;
      }
      const auto magnitude = static_cast<long double>(digits_before_decimal) -
                             static_cast<long double>(first_nonzero.value_or(digit)) - 1 + explicit_exponent;
      value = first_nonzero && magnitude >= 0 ? std::numeric_limits<double>::infinity() : 0.0;
      if (text.front() == '-') value = -value;
    } else if (converted.ec != std::errc{}) {
      throw JsonSyntaxError();
    }
    return value;
  }

  void take_digits(bool required) {
    const auto begin = position_;
    while (position_ < source_.size() && source_[position_] >= '0' && source_[position_] <= '9') ++position_;
    if (required && begin == position_) throw JsonSyntaxError();
  }

  [[nodiscard]] std::uint32_t parse_hex_quad() {
    if (position_ + 4 > source_.size()) throw JsonSyntaxError();
    std::uint32_t result = 0;
    for (int index = 0; index < 4; ++index) {
      const char16_t unit = source_[position_++];
      result <<= 4U;
      if (unit >= '0' && unit <= '9') result |= static_cast<std::uint32_t>(unit - '0');
      else if (unit >= 'a' && unit <= 'f') result |= static_cast<std::uint32_t>(unit - 'a' + 10);
      else if (unit >= 'A' && unit <= 'F') result |= static_cast<std::uint32_t>(unit - 'A' + 10);
      else throw JsonSyntaxError();
    }
    return result;
  }

  void skip_whitespace() {
    while (position_ < source_.size() &&
           (source_[position_] == ' ' || source_[position_] == '\n' ||
            source_[position_] == '\r' || source_[position_] == '\t')) {
      ++position_;
    }
  }

  void consume(std::u16string_view expected) {
    if (source_.substr(position_, expected.size()) != expected) throw JsonSyntaxError();
    position_ += expected.size();
  }

  void expect(char16_t expected) {
    if (!take(expected)) throw JsonSyntaxError();
  }

  [[nodiscard]] bool take(char16_t expected) {
    if (position_ == source_.size() || source_[position_] != expected) return false;
    ++position_;
    return true;
  }

  std::u16string source_;
  std::size_t position_ = 0;
};

} // namespace detail

class Json {
 public:
  [[nodiscard]] static JsonValue parse(const String& source) {
    return detail::JsonParser(source).parse();
  }

  template <typename Value>
  [[nodiscard]] static String stringify(const Value& value) {
    return stringify(value, nullptr, 0.0);
  }

  template <typename Value, typename Replacer>
  [[nodiscard]] static String stringify(const Value& value, Replacer replacer) {
    return stringify(value, replacer, 0.0);
  }

  // `space` is absent, a number, or an optional number. `JSON.stringify(value, replacer, undefined)`
  // indents by nothing, and emitted code writes that third argument as `std::nullopt`, so a signature
  // demanding `double` rejects a call the source was entitled to make.
  template <typename Value, typename Replacer, typename Space>
  [[nodiscard]] static String stringify(const Value& value, Replacer, const Space& space) {
    std::string output;
    detail::append_json_value(output, value, json_indentation(space), 0);
    return String::from_utf8(output);
  }

 private:
  template <typename Space>
  [[nodiscard]] static std::size_t json_indentation(const Space& space) {
    using Type = std::remove_cvref_t<Space>;
    if constexpr (std::same_as<Type, std::nullopt_t> || std::same_as<Type, std::nullptr_t> ||
                  std::same_as<Type, Undefined> || std::same_as<Type, Null>) {
      return 0;
    } else if constexpr (requires { space.has_value(); *space; }) {
      return space.has_value() ? json_indentation(*space) : std::size_t{0};
    } else {
      const auto width = static_cast<double>(space);
      return !std::isfinite(width) || width <= 0.0
                 ? std::size_t{0}
                 : std::min<std::size_t>(10, static_cast<std::size_t>(width));
    }
  }
};

} // namespace flight
