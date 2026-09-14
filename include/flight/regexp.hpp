#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <regex>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include <flight/array.hpp>
#include <flight/string.hpp>

namespace flight {

class RegExpExecArray : public Array<String> {
 public:
  double index = 0.0;
  String input;

  [[nodiscard]] bool has_capture(std::size_t capture_index) const noexcept {
    return capture_index < capture_presence_.size() && capture_presence_[capture_index];
  }

  [[nodiscard]] std::optional<String> capture(std::size_t capture_index) const {
    return has_capture(capture_index) ? std::optional<String>((*this)[capture_index]) : std::nullopt;
  }

 private:
  friend class RegExp;

  void push_capture(const std::ssub_match& capture) {
    capture_presence_.push_back(capture.matched);
    push(capture.matched ? String::from_utf8(capture.str()) : String());
  }

  std::vector<bool> capture_presence_;
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
    if (start > input.length()) {
      state_->last_index = 0;
      return std::nullopt;
    }

    const auto byte_start = input.slice(0, static_cast<std::ptrdiff_t>(start)).to_utf8().size();

    std::match_results<std::string::const_iterator> match;
    const auto begin = encoded.cbegin() + static_cast<std::ptrdiff_t>(byte_start);
    auto match_flags = std::regex_constants::match_default;
    if (start != 0) match_flags |= std::regex_constants::match_not_bol;
    if (!std::regex_search(begin, encoded.cend(), match, state_->expression, match_flags)) {
      if (state_->global) state_->last_index = 0;
      return std::nullopt;
    }

    const auto byte_index = byte_start + static_cast<std::size_t>(match.position());
    const auto string_index = String::from_utf8(encoded.substr(0, byte_index)).length();
    if (state_->global) {
      state_->last_index = string_index + String::from_utf8(match.str()).length();
    }
    RegExpExecArray result;
    result.index = static_cast<double>(string_index);
    result.input = input;
    for (const auto& capture : match) result.push_capture(capture);
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

template <std::size_t Count, typename Function, typename Tuple, std::size_t... Indices>
[[nodiscard]] constexpr bool regexp_prefix_invocable(std::index_sequence<Indices...>) {
  return std::is_invocable_r_v<String, Function&, std::tuple_element_t<Indices, Tuple>...>;
}

template <std::size_t Count, typename Function, typename Tuple, std::size_t... Indices>
String invoke_regexp_prefix(Function& function, Tuple& arguments, std::index_sequence<Indices...>) {
  return std::invoke(function, std::get<Indices>(arguments)...);
}

template <std::size_t Count, typename Function, typename Tuple>
consteval std::size_t regexp_invocable_prefix_score() {
  if constexpr (regexp_prefix_invocable<Count, Function, Tuple>(std::make_index_sequence<Count>{})) {
    return Count + 1;
  } else if constexpr (Count > 0) {
    return regexp_invocable_prefix_score<Count - 1, Function, Tuple>();
  } else {
    return 0;
  }
}

template <std::size_t... CaptureIndices, typename Function>
String regexp_replacement_with_captures(
    const std::smatch& match,
    const String& input,
    Function& replacement,
    std::index_sequence<CaptureIndices...>) {
  [[maybe_unused]] const auto capture = [&](std::size_t capture_index) {
    return match[capture_index].matched
               ? std::optional<String>(String::from_utf8(match[capture_index].str()))
               : std::nullopt;
  };
  const auto offset = static_cast<double>(String::from_utf8(input.to_utf8().substr(
      0, static_cast<std::size_t>(match.position()))).length());
  auto optional_arguments = std::tuple{
      String::from_utf8(match.str()),
      capture(CaptureIndices + 1)...,
      offset,
      input,
  };
  auto string_arguments = std::tuple{
      String::from_utf8(match.str()),
      capture(CaptureIndices + 1).value_or(String())...,
      offset,
      input,
  };
  constexpr auto optional_score = regexp_invocable_prefix_score<
      std::tuple_size_v<decltype(optional_arguments)>, Function, decltype(optional_arguments)>();
  constexpr auto string_score = regexp_invocable_prefix_score<
      std::tuple_size_v<decltype(string_arguments)>, Function, decltype(string_arguments)>();
  if constexpr (optional_score >= string_score && optional_score > 0) {
    return invoke_regexp_prefix<optional_score - 1>(
        replacement, optional_arguments, std::make_index_sequence<optional_score - 1>{});
  } else if constexpr (string_score > 0) {
    return invoke_regexp_prefix<string_score - 1>(
        replacement, string_arguments, std::make_index_sequence<string_score - 1>{});
  } else {
    throw std::invalid_argument(
        "flight::String RegExp replacement callback parameters do not match the expression's captures");
  }
}

template <std::size_t CaptureCount = 0, typename Function>
String regexp_replacement(
    const std::smatch& match,
    const String& input,
    Function& replacement) {
  if (match.size() == CaptureCount + 1) {
    return regexp_replacement_with_captures(match, input, replacement,
                                            std::make_index_sequence<CaptureCount>{});
  }
  if constexpr (CaptureCount < 16) {
    return regexp_replacement<CaptureCount + 1>(match, input, replacement);
  } else {
    throw std::range_error("flight::String replacement callbacks support at most 16 capture groups");
  }
}

inline String regexp_string_replacement(
    const std::smatch& match,
    std::string_view input,
    const String& replacement) {
  const auto replacement_bytes = replacement.to_utf8();
  std::string output;
  output.reserve(replacement_bytes.size());
  for (std::size_t index = 0; index < replacement_bytes.size(); ++index) {
    if (replacement_bytes[index] != '$' || index + 1 >= replacement_bytes.size()) {
      output.push_back(replacement_bytes[index]);
      continue;
    }

    const char token = replacement_bytes[index + 1];
    if (token == '$') {
      output.push_back('$');
      ++index;
    } else if (token == '&') {
      output.append(match.str());
      ++index;
    } else if (token == '`') {
      output.append(input.substr(0, static_cast<std::size_t>(match.position())));
      ++index;
    } else if (token == '\'') {
      output.append(input.substr(static_cast<std::size_t>(match.position() + match.length())));
      ++index;
    } else if (token >= '0' && token <= '9') {
      std::size_t digits = 1;
      std::size_t capture_index = static_cast<std::size_t>(token - '0');
      if (index + 2 < replacement_bytes.size() && replacement_bytes[index + 2] >= '0' &&
          replacement_bytes[index + 2] <= '9') {
        const auto two_digit_index = capture_index * 10 +
                                     static_cast<std::size_t>(replacement_bytes[index + 2] - '0');
        if (two_digit_index > 0 && two_digit_index < match.size()) {
          capture_index = two_digit_index;
          digits = 2;
        }
      }
      if (capture_index > 0 && capture_index < match.size()) {
        if (match[capture_index].matched) output.append(match[capture_index].str());
        index += digits;
      } else {
        output.push_back('$');
      }
    } else {
      output.push_back('$');
    }
  }
  return String::from_utf8(output);
}

template <typename Replacement>
String regexp_replace(const String& input, const RegExp& expression, Replacement replacement) {
  const auto encoded = input.to_utf8();
  std::string result;
  std::size_t cursor = 0;
  if (expression.global()) expression.reset();
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
  return String::from_utf8(result);
}

} // namespace detail

inline std::optional<RegExpExecArray> String::match(const RegExp& expression) const {
  if (!expression.global()) return expression.exec(*this);

  expression.reset();
  RegExpExecArray result;
  const auto encoded = to_utf8();
  for (auto iterator = std::sregex_iterator(encoded.cbegin(), encoded.cend(), expression.native_expression());
       iterator != std::sregex_iterator(); ++iterator) {
    result.push(String::from_utf8(iterator->str()));
  }
  return result.empty() ? std::nullopt : std::optional<RegExpExecArray>(std::move(result));
}

inline String String::replace(const RegExp& expression, const String& replacement) const {
  return detail::regexp_replace(*this, expression, [&](const std::smatch& match) {
    return detail::regexp_string_replacement(match, to_utf8(), replacement);
  });
}

template <typename Function>
  requires(!std::convertible_to<Function, String>)
String String::replace(const RegExp& expression, Function&& replacement) const {
  return detail::regexp_replace(*this, expression, [&](const std::smatch& match) {
    return detail::regexp_replacement(match, *this, replacement);
  });
}

} // namespace flight
