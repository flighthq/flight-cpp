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
#include <flight/error.hpp>
#include <flight/string.hpp>

namespace flight {

namespace detail {

// ECMAScript patterns without the Unicode flag operate on UTF-16 code units. A wchar_t stores
// one unit even on platforms where it is wider; no UTF-8 round trip or surrogate repair occurs.
inline std::wstring regexp_units(const String& value) {
  return std::wstring(value.begin(), value.end());
}

inline String regexp_string(std::wstring_view value) {
  return String(std::u16string(value.begin(), value.end()));
}

inline std::wstring regexp_pattern(const String& value) {
  const auto units = regexp_units(value);
  // ECMAScript WhiteSpace and LineTerminator, independent of the process locale. Expanding both
  // classes to ranges also works inside character classes, including [\s\S].
  const std::wstring spaces = L"\x0009-\x000d\x0020\x00a0\x1680\x2000-\x200a\x2028\x2029\x202f\x205f\x3000\xfeff";
  const std::wstring nonspaces = L"\\x00-\\x08\x000e-\x001f\x0021-\x009f\x00a1-\x167f\x1681-\x1fff\x200b-\x2027\x202a-\x202e\x2030-\x205e\x2060-\x2fff\x3001-\xfefe\xff00-\xffff";
  std::wstring result;
  bool in_class = false;
  for (std::size_t index = 0; index < units.size(); ++index) {
    const auto unit = units[index];
    if (unit == L'\\' && index + 1 < units.size()) {
      const auto escaped = units[++index];
      if (escaped == L's' || escaped == L'S') {
        if (!in_class) result.push_back(L'[');
        result.append(escaped == L's' ? spaces : nonspaces);
        if (!in_class) result.push_back(L']');
      } else {
        result.push_back(L'\\');
        result.push_back(escaped);
      }
    } else if (unit == L'.' && !in_class) {
      result.append(L"[^\n\r\x2028\x2029]");
    } else {
      if (unit == L'[') in_class = true;
      if (unit == L']') in_class = false;
      result.push_back(unit);
    }
  }
  return result;
}

} // namespace detail

// A capture read itself remains undefined when unmatched. Reaching a String member is the point
// at which JavaScript throws TypeError; an empty, participating capture is a present String.
inline String regexp_capture_value(std::optional<String> capture) {
  if (!capture) throw TypeError(String("Cannot read properties of undefined"));
  return std::move(*capture);
}

class RegExpExecArray : public Array<String> {
 public:
  double index = 0.0;
  String input;

  [[nodiscard]] bool has_capture(std::size_t capture_index) const noexcept {
    return capture_index < capture_presence_.size() && capture_presence_[capture_index];
  }

  [[nodiscard]] std::optional<String> capture(double capture_index) const {
    const auto value = get(capture_index);
    return value && has_capture(static_cast<std::size_t>(capture_index)) ? value : std::nullopt;
  }

 private:
  friend class RegExp;
  friend class String;

  void push_capture(const std::wssub_match& capture) {
    capture_presence_.push_back(capture.matched);
    push(capture.matched ? detail::regexp_string(capture.str()) : String());
  }

  std::vector<bool> capture_presence_;
};

class RegExp {
  struct State {
    State(const std::wstring& pattern, std::regex_constants::syntax_option_type options, bool global)
        : expression(pattern, options), global(global) {}

    std::wregex expression;
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
    state_ = std::make_shared<State>(detail::regexp_pattern(pattern), options, global);
  }

  [[nodiscard]] std::optional<RegExpExecArray> exec(const String& input) const {
    const auto encoded = detail::regexp_units(input);
    const auto start = state_->global ? state_->last_index : 0;
    if (start > input.length()) {
      state_->last_index = 0;
      return std::nullopt;
    }

    std::match_results<std::wstring::const_iterator> match;
    const auto begin = encoded.cbegin() + static_cast<std::ptrdiff_t>(start);
    auto match_flags = std::regex_constants::match_default;
    if (start != 0) match_flags |= std::regex_constants::match_not_bol;
    if (!std::regex_search(begin, encoded.cend(), match, state_->expression, match_flags)) {
      if (state_->global) state_->last_index = 0;
      return std::nullopt;
    }

    const auto string_index = start + static_cast<std::size_t>(match.position());
    if (state_->global) {
      state_->last_index = string_index + static_cast<std::size_t>(match.length());
    }
    RegExpExecArray result;
    result.index = static_cast<double>(string_index);
    result.input = input;
    for (const auto& capture : match) result.push_capture(capture);
    return result;
  }

  [[nodiscard]] bool test(const String& input) const { return exec(input).has_value(); }

  [[nodiscard]] const std::wregex& native_expression() const noexcept { return state_->expression; }
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
    const std::wsmatch& match,
    const String& input,
    Function& replacement,
    std::index_sequence<CaptureIndices...>) {
  [[maybe_unused]] const auto capture = [&](std::size_t capture_index) {
    return match[capture_index].matched
               ? std::optional<String>(regexp_string(match[capture_index].str()))
               : std::nullopt;
  };
  const auto offset = static_cast<double>(match.position());
  auto optional_arguments = std::tuple{
      regexp_string(match.str()),
      capture(CaptureIndices + 1)...,
      offset,
      input,
  };
  auto string_arguments = std::tuple{
      regexp_string(match.str()),
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
    const std::wsmatch& match,
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
    const std::wsmatch& match,
    std::wstring_view input,
    const String& replacement) {
  const auto replacement_bytes = regexp_units(replacement);
  std::wstring output;
  output.reserve(replacement_bytes.size());
  for (std::size_t index = 0; index < replacement_bytes.size(); ++index) {
    if (replacement_bytes[index] != '$' || index + 1 >= replacement_bytes.size()) {
      output.push_back(replacement_bytes[index]);
      continue;
    }

    const wchar_t token = replacement_bytes[index + 1];
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
  return regexp_string(output);
}

template <typename Replacement>
String regexp_replace(const String& input, const RegExp& expression, Replacement replacement) {
  const auto encoded = regexp_units(input);
  std::wstring result;
  std::size_t cursor = 0;
  if (expression.global()) expression.reset();
  for (auto iterator = std::wsregex_iterator(encoded.cbegin(), encoded.cend(), expression.native_expression());
       iterator != std::wsregex_iterator(); ++iterator) {
    const auto& match = *iterator;
    const auto position = static_cast<std::size_t>(match.position());
    result.append(encoded, cursor, position - cursor);
    result.append(regexp_units(replacement(match)));
    cursor = position + static_cast<std::size_t>(match.length());
    if (!expression.global()) break;
  }
  result.append(encoded, cursor, encoded.size() - cursor);
  return regexp_string(result);
}

} // namespace detail

// `String.prototype.search`: the index of the first match in UTF-16 code units, or -1.
//
// Deliberately NOT implemented through `RegExp::exec`, for two reasons that both change the answer.
// `search` ignores `lastIndex` and always searches from the start, where `exec` on a global pattern starts
// at `lastIndex`; and `search` leaves `lastIndex` as it found it, where `exec` on a global pattern writes
// to it. Going through `std::regex_search` directly satisfies both without having to save and restore
// state that has no public accessor.
//
// Matching uses one wchar_t per UTF-16 code unit, so position() already has the source index.
inline double String::search(const RegExp& expression) const {
  const auto encoded = detail::regexp_units(*this);
  std::match_results<std::wstring::const_iterator> match;
  if (!std::regex_search(encoded.cbegin(), encoded.cend(), match, expression.native_expression())) {
    return -1.0;
  }
  return static_cast<double>(match.position());
}

inline std::optional<RegExpExecArray> String::match(const RegExp& expression) const {
  if (!expression.global()) return expression.exec(*this);

  expression.reset();
  RegExpExecArray result;
  const auto encoded = detail::regexp_units(*this);
  for (auto iterator = std::wsregex_iterator(encoded.cbegin(), encoded.cend(), expression.native_expression());
       iterator != std::wsregex_iterator(); ++iterator) {
    result.push_capture((*iterator)[0]);
  }
  return result.empty() ? std::nullopt : std::optional<RegExpExecArray>(std::move(result));
}

inline Array<String> String::split(const RegExp& separator, size_type limit) const {
  Array<String> result;
  if (limit == 0) return result;

  const auto encoded = detail::regexp_units(*this);
  std::size_t cursor = 0;
  for (auto iterator = std::wsregex_iterator(encoded.cbegin(), encoded.cend(), separator.native_expression());
       iterator != std::wsregex_iterator() && result.size() < limit; ++iterator) {
    const auto& match = *iterator;
    const auto position = static_cast<std::size_t>(match.position());
    result.push(detail::regexp_string(encoded.substr(cursor, position - cursor)));
    if (result.size() == limit) return result;
    for (std::size_t capture = 1; capture < match.size() && result.size() < limit; ++capture) {
      result.push(match[capture].matched ? detail::regexp_string(match[capture].str()) : String());
    }
    cursor = position + static_cast<std::size_t>(match.length());
  }
  if (result.size() < limit) result.push(detail::regexp_string(encoded.substr(cursor)));
  return result;
}

inline String String::replace(const RegExp& expression, const String& replacement) const {
  return detail::regexp_replace(*this, expression, [&](const std::wsmatch& match) {
    return detail::regexp_string_replacement(match, detail::regexp_units(*this), replacement);
  });
}

template <typename Function>
  requires(!std::convertible_to<Function, String>)
String String::replace(const RegExp& expression, Function&& replacement) const {
  return detail::regexp_replace(*this, expression, [&](const std::wsmatch& match) {
    return detail::regexp_replacement(match, *this, replacement);
  });
}

} // namespace flight
