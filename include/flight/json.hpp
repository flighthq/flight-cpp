#pragma once

#include <algorithm>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <iomanip>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <type_traits>
#include <variant>

#include <flight/array.hpp>
#include <flight/presence.hpp>
#include <flight/string.hpp>

namespace flight {

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

template <typename Value>
void append_json_array(std::string& output, const Array<Value>& values, std::size_t indentation, std::size_t depth) {
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

template <typename Value>
void append_json_value(std::string& output, const Value& value, std::size_t indentation, std::size_t depth) {
  using Type = std::remove_cvref_t<Value>;
  if constexpr (std::same_as<Type, String>) {
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
    // Object member metadata is compiler-generated separately. Until a type opts into that
    // metadata, preserve the JavaScript object domain without inventing a scalar encoding.
    output.append("{}");
  }
}

} // namespace detail

class Json {
 public:
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
