#pragma once

#include <algorithm>
#include <charconv>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <iomanip>
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
    append_json_value(output, value, indentation, depth + 1);
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
  } else {
    output.append("{}");
  }
}

class JsonParser {
 public:
  explicit JsonParser(const String& source) : source_(source.to_utf8()) {}

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
      case 'n': consume("null"); return nullptr;
      case 't': consume("true"); return true;
      case 'f': consume("false"); return false;
      case '"': return parse_string();
      case '[': return parse_array();
      case '{': return parse_object();
      default: return parse_number();
    }
  }

  [[nodiscard]] String parse_string() {
    expect('"');
    std::string result;
    while (position_ < source_.size()) {
      const unsigned char unit = static_cast<unsigned char>(source_[position_++]);
      if (unit == '"') return String::from_utf8(result);
      if (unit < 0x20U) throw JsonSyntaxError();
      if (unit != '\\') {
        result.push_back(static_cast<char>(unit));
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
          auto code_point = parse_hex_quad();
          if (code_point >= 0xD800U && code_point <= 0xDBFFU) {
            if (position_ + 2 > source_.size() || source_[position_] != '\\' || source_[position_ + 1] != 'u') {
              throw JsonSyntaxError();
            }
            position_ += 2;
            const auto low = parse_hex_quad();
            if (low < 0xDC00U || low > 0xDFFFU) throw JsonSyntaxError();
            code_point = 0x10000U + ((code_point - 0xD800U) << 10U) + (low - 0xDC00U);
          } else if (code_point >= 0xDC00U && code_point <= 0xDFFFU) {
            throw JsonSyntaxError();
          }
          append_utf8(result, code_point);
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
    const auto converted = std::from_chars(source_.data() + begin, source_.data() + position_, value);
    if (converted.ec != std::errc{} || converted.ptr != source_.data() + position_ || !std::isfinite(value)) {
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
      const char unit = source_[position_++];
      result <<= 4U;
      if (unit >= '0' && unit <= '9') result |= static_cast<std::uint32_t>(unit - '0');
      else if (unit >= 'a' && unit <= 'f') result |= static_cast<std::uint32_t>(unit - 'a' + 10);
      else if (unit >= 'A' && unit <= 'F') result |= static_cast<std::uint32_t>(unit - 'A' + 10);
      else throw JsonSyntaxError();
    }
    return result;
  }

  static void append_utf8(std::string& output, std::uint32_t code_point) {
    if (code_point <= 0x7FU) output.push_back(static_cast<char>(code_point));
    else if (code_point <= 0x7FFU) {
      output.push_back(static_cast<char>(0xC0U | (code_point >> 6U)));
      output.push_back(static_cast<char>(0x80U | (code_point & 0x3FU)));
    } else if (code_point <= 0xFFFFU) {
      output.push_back(static_cast<char>(0xE0U | (code_point >> 12U)));
      output.push_back(static_cast<char>(0x80U | ((code_point >> 6U) & 0x3FU)));
      output.push_back(static_cast<char>(0x80U | (code_point & 0x3FU)));
    } else {
      output.push_back(static_cast<char>(0xF0U | (code_point >> 18U)));
      output.push_back(static_cast<char>(0x80U | ((code_point >> 12U) & 0x3FU)));
      output.push_back(static_cast<char>(0x80U | ((code_point >> 6U) & 0x3FU)));
      output.push_back(static_cast<char>(0x80U | (code_point & 0x3FU)));
    }
  }

  void skip_whitespace() {
    while (position_ < source_.size() &&
           (source_[position_] == ' ' || source_[position_] == '\n' ||
            source_[position_] == '\r' || source_[position_] == '\t')) {
      ++position_;
    }
  }

  void consume(std::string_view expected) {
    if (source_.substr(position_, expected.size()) != expected) throw JsonSyntaxError();
    position_ += expected.size();
  }

  void expect(char expected) {
    if (!take(expected)) throw JsonSyntaxError();
  }

  [[nodiscard]] bool take(char expected) {
    if (position_ == source_.size() || source_[position_] != expected) return false;
    ++position_;
    return true;
  }

  std::string source_;
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
  [[nodiscard]] static String stringify(const Value& value, Replacer, double space = 0.0) {
    const auto indentation = !std::isfinite(space) || space <= 0.0
                                 ? std::size_t{0}
                                 : std::min<std::size_t>(10, static_cast<std::size_t>(space));
    std::string output;
    detail::append_json_value(output, value, indentation, 0);
    return String::from_utf8(output);
  }
};

} // namespace flight
