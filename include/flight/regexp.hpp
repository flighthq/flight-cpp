#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <regex>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

#include <flight/array.hpp>
#include <flight/string.hpp>

namespace flight {

class RegExpExecArray : public Array<String> {
 public:
  double index = 0.0;
  String input;
};

class RegExp {
  struct State {
    State(const std::string& pattern, std::regex_constants::syntax_option_type options, bool global)
        : expression(pattern, options), global(global) {}

    std::regex expression;
    std::size_t last_index = 0;
    bool global = false;
  };

 public:
  explicit RegExp(const String& pattern) : RegExp(pattern, String()) {}

  RegExp(const String& pattern, const String& flags) {
    auto options = std::regex_constants::ECMAScript;
    bool global = false;
    bool multiline = false;
    bool insensitive = false;
    for (const char flag : flags.to_utf8()) {
      if (flag == 'g' && !global) {
        global = true;
      } else if (flag == 'i' && !insensitive) {
        insensitive = true;
        options |= std::regex_constants::icase;
      } else if (flag == 'm' && !multiline) {
        multiline = true;
        options |= std::regex_constants::multiline;
      } else {
        throw std::invalid_argument("flight::RegExp flag is unsupported or duplicated");
      }
    }
    state_ = std::make_shared<State>(pattern.to_utf8(), options, global);
  }

  [[nodiscard]] std::optional<RegExpExecArray> exec(const String& input) const {
    const std::string encoded = input.to_utf8();
    const auto start = state_->global ? state_->last_index : 0;
    if (start > encoded.size()) {
      state_->last_index = 0;
      return std::nullopt;
    }

    std::match_results<std::string::const_iterator> match;
    const auto begin = encoded.cbegin() + static_cast<std::ptrdiff_t>(start);
    auto match_flags = std::regex_constants::match_default;
    if (start != 0) match_flags |= std::regex_constants::match_not_bol;
    if (!std::regex_search(begin, encoded.cend(), match, state_->expression, match_flags)) {
      if (state_->global) state_->last_index = 0;
      return std::nullopt;
    }

    const auto byte_index = start + static_cast<std::size_t>(match.position());
    if (state_->global) state_->last_index = byte_index + static_cast<std::size_t>(match.length());
    RegExpExecArray result;
    result.index = static_cast<double>(String::from_utf8(encoded.substr(0, byte_index)).length());
    result.input = input;
    for (const auto& capture : match) {
      result.push(capture.matched ? String::from_utf8(capture.str()) : String());
    }
    return result;
  }

  [[nodiscard]] bool test(const String& input) const { return exec(input).has_value(); }

  [[nodiscard]] const std::regex& native_expression() const noexcept { return state_->expression; }
  [[nodiscard]] bool global() const noexcept { return state_->global; }
  void reset() const noexcept { state_->last_index = 0; }

 private:
  friend class String;

  std::shared_ptr<State> state_;
};

namespace detail {

inline String regexp_capture(const std::smatch& match, std::size_t index) {
  return index < match.size() && match[index].matched ? String::from_utf8(match[index].str()) : String();
}

template <typename Function>
String regexp_replacement(const std::smatch& match, Function&& replacement) {
  if constexpr (std::is_invocable_v<Function, String, String, String, String, String, String>) {
    return std::invoke(std::forward<Function>(replacement), regexp_capture(match, 0), regexp_capture(match, 1),
                       regexp_capture(match, 2), regexp_capture(match, 3), regexp_capture(match, 4),
                       regexp_capture(match, 5));
  } else if constexpr (std::is_invocable_v<Function, String, String, String, String, String>) {
    return std::invoke(std::forward<Function>(replacement), regexp_capture(match, 0), regexp_capture(match, 1),
                       regexp_capture(match, 2), regexp_capture(match, 3), regexp_capture(match, 4));
  } else if constexpr (std::is_invocable_v<Function, String, String, String, String>) {
    return std::invoke(std::forward<Function>(replacement), regexp_capture(match, 0), regexp_capture(match, 1),
                       regexp_capture(match, 2), regexp_capture(match, 3));
  } else if constexpr (std::is_invocable_v<Function, String, String, String>) {
    return std::invoke(std::forward<Function>(replacement), regexp_capture(match, 0), regexp_capture(match, 1),
                       regexp_capture(match, 2));
  } else if constexpr (std::is_invocable_v<Function, String, String>) {
    return std::invoke(std::forward<Function>(replacement), regexp_capture(match, 0), regexp_capture(match, 1));
  } else {
    static_assert(std::is_invocable_v<Function, String>,
                  "flight::String RegExp replacement callbacks accept one to six string arguments");
    return std::invoke(std::forward<Function>(replacement), regexp_capture(match, 0));
  }
}

template <typename Replacement>
String regexp_replace(const String& input, const RegExp& expression, Replacement replacement) {
  const auto encoded = input.to_utf8();
  std::string result;
  std::size_t cursor = 0;
  for (auto iterator = std::sregex_iterator(encoded.cbegin(), encoded.cend(), expression.native_expression());
       iterator != std::sregex_iterator(); ++iterator) {
    const auto& match = *iterator;
    const auto position = static_cast<std::size_t>(match.position());
    result.append(encoded, cursor, position - cursor);
    result.append(replacement(match).to_utf8());
    cursor = position + static_cast<std::size_t>(match.length());
    if (!expression.global()) break;
  }
  result.append(encoded, cursor, encoded.size() - cursor);
  if (expression.global()) expression.reset();
  return String::from_utf8(result);
}

} // namespace detail

inline std::optional<RegExpExecArray> String::match(const RegExp& expression) const {
  return expression.exec(*this);
}

inline String String::replace(const RegExp& expression, const String& replacement) const {
  return detail::regexp_replace(*this, expression, [&](const std::smatch& match) {
    return String::from_utf8(match.format(replacement.to_utf8()));
  });
}

template <typename Function>
  requires(!std::convertible_to<Function, String>)
String String::replace(const RegExp& expression, Function&& replacement) const {
  return detail::regexp_replace(*this, expression, [&](const std::smatch& match) {
    return detail::regexp_replacement(match, replacement);
  });
}

} // namespace flight
