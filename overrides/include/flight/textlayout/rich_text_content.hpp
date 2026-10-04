// Derived from @flighthq/textlayout/packages/textlayout/src/richTextContent.ts
// at generated digest 074b52dc486f75c7b5471fed3253315120f7e659727fc91e7bbdb881722c709f.
#pragma once

#include <flight/entity/entity.hpp>
#include <flight/number.hpp>
#include <flight/record.hpp>
#include <flight/regexp.hpp>
#include <flight/runtime.hpp>
#include <flight/textlayout/text_format.hpp>
#include <flight/textlayout/text_format_range.hpp>
#include <flight/types/rich_text.hpp>
#include <flight/types/rich_text_content.hpp>
#include <flight/types/text_format.hpp>
#include <flight/types/text_format_range.hpp>

static_assert(flight::runtime_contract.compiler_contract == "flight-runtime-contract/2", "Flight compiler/runtime contract mismatch");
static_assert(flight::runtime_contract.cpp_abi == 1, "Flight C++ runtime ABI mismatch");

namespace flight::textlayout {

inline void clear_rich_text_content(flight::Ref<flight::types::RichTextRuntime> runtime) {
  runtime->rich_text_content = std::nullopt;
}

inline void initialize_rich_text_content(
    flight::types::EntityConstruction<flight::Ref<flight::types::RichTextContent>> out) {
  const auto object = out.shared_object();
  if (!object) throw std::bad_weak_ptr();
  object->format_ranges = flight::Array<flight::Ref<flight::types::TextFormatRange>>{};
  object->text = flight::String("");
}

inline flight::Ref<flight::types::RichTextContent> create_rich_text_content() {
  auto out = flight::entity::allocate_entity<flight::Ref<flight::types::RichTextContent>>();
  initialize_rich_text_content(out);
  return flight::entity::finish_entity<flight::Ref<flight::types::RichTextContent>>(out);
}

// OVERRIDE: the generated RichTextRuntime field is std::optional<Ref<RichTextContent>>, which is the
// source null channel the emitter failed to recognize. Initialize it once and retain that identity.
inline flight::Ref<flight::types::RichTextContent> get_rich_text_content(
    flight::Ref<flight::types::RichTextRuntime> runtime) {
  if (!runtime->rich_text_content.has_value() || !runtime->rich_text_content.value()) {
    runtime->rich_text_content = create_rich_text_content();
  }
  return runtime->rich_text_content.value();
}

inline void clamp_ranges(
    flight::Array<flight::Ref<flight::types::TextFormatRange>> ranges,
    double length) {
  for (double index = static_cast<double>(ranges.size()) - 1.0; index >= 0.0; index -= 1.0) {
    const auto range = ranges.element(index);
    if (range->start >= length) {
      (void)ranges.splice(index, 1.0);
    } else if (range->end > length) {
      range->end = length;
    }
  }
}

// OVERRIDE: color is optional storage in the generated TextFormat carrier. Its disengaged state is
// precisely source undefined, so the source fallback is a direct presence test here.
inline flight::Ref<flight::types::TextFormat> create_base_format(
    flight::Ref<flight::types::RichTextData> data) {
  auto format = merge_text_format(data->default_text_format, data->text_format);
  if (!format->color.has_value()) format->color = data->text_color;
  return format;
}

inline flight::String get_renderable_source(
    flight::Ref<flight::types::RichTextData> data,
    std::optional<flight::String> password_character) {
  if (!password_character.has_value()) return data->text;
  const flight::String mask = password_character->length() > 0
                                  ? password_character->char_at(0.0)
                                  : flight::String("•");
  return mask.repeat(static_cast<double>(data->text.length()));
}

template <typename Value>
inline bool text_format_optional_equals(
    const std::optional<Value>& left,
    const std::optional<Value>& right) {
  if (left.has_value() != right.has_value()) return false;
  return !left.has_value() || left.value() == right.value();
}

template <typename Value>
inline bool text_format_optional_array_equals(
    const std::optional<flight::Array<Value>>& left,
    const std::optional<flight::Array<Value>>& right) {
  if (left.has_value() != right.has_value()) return false;
  if (!left.has_value()) return true;
  if (left->size() != right->size()) return false;
  for (double index = 0.0; index < static_cast<double>(left->size()); index += 1.0) {
    if (left->element(index) != right->element(index)) return false;
  }
  return true;
}

// OVERRIDE: keyof iteration is finite for TextFormat. Compare the same optional fields explicitly;
// its two array-valued fields retain the source's shallow, element-by-element equality rule.
inline bool text_format_equals(
    flight::Ref<flight::types::TextFormat> left,
    flight::Ref<flight::types::TextFormat> right) {
#define FLIGHT_TEXTLAYOUT_EQUAL_FIELD(name)                     \
  if (!text_format_optional_equals(left->name, right->name)) {  \
    return false;                                               \
  }
  FLIGHT_TEXTLAYOUT_EQUAL_FIELD(align)
  FLIGHT_TEXTLAYOUT_EQUAL_FIELD(block_indent)
  FLIGHT_TEXTLAYOUT_EQUAL_FIELD(bold)
  FLIGHT_TEXTLAYOUT_EQUAL_FIELD(bullet)
  FLIGHT_TEXTLAYOUT_EQUAL_FIELD(color)
  FLIGHT_TEXTLAYOUT_EQUAL_FIELD(font)
  FLIGHT_TEXTLAYOUT_EQUAL_FIELD(indent)
  FLIGHT_TEXTLAYOUT_EQUAL_FIELD(italic)
  FLIGHT_TEXTLAYOUT_EQUAL_FIELD(kerning)
  FLIGHT_TEXTLAYOUT_EQUAL_FIELD(leading)
  FLIGHT_TEXTLAYOUT_EQUAL_FIELD(left_margin)
  FLIGHT_TEXTLAYOUT_EQUAL_FIELD(letter_spacing)
  FLIGHT_TEXTLAYOUT_EQUAL_FIELD(list_marker)
  FLIGHT_TEXTLAYOUT_EQUAL_FIELD(right_margin)
  FLIGHT_TEXTLAYOUT_EQUAL_FIELD(size)
  FLIGHT_TEXTLAYOUT_EQUAL_FIELD(strikethrough)
  FLIGHT_TEXTLAYOUT_EQUAL_FIELD(target)
  FLIGHT_TEXTLAYOUT_EQUAL_FIELD(underline)
  FLIGHT_TEXTLAYOUT_EQUAL_FIELD(url)
#undef FLIGHT_TEXTLAYOUT_EQUAL_FIELD
  return text_format_optional_array_equals(left->tab_stops, right->tab_stops) &&
         text_format_optional_array_equals(left->variations, right->variations);
}

inline void write_format_range(
    flight::Array<flight::Ref<flight::types::TextFormatRange>> ranges,
    flight::Ref<flight::types::TextFormat> format,
    double start,
    double end) {
  if (start == end) return;
  const auto previous = ranges.get(static_cast<double>(ranges.size()) - 1.0);
  if (previous.has_value() && previous.value()->end == start &&
      text_format_equals(previous.value()->format, format)) {
    previous.value()->end = end;
  } else {
    ranges.push(create_text_format_range(
        flight::make_ref<flight::types::TextFormat>(*format), start, end));
  }
}

inline void apply_text_format_ranges(
    flight::Ref<flight::types::RichTextContent> out,
    flight::Array<flight::Ref<flight::types::TextFormatRange>> overrides) {
  if (overrides.empty() || out->text.length() == 0) return;

  auto ranges = out->format_ranges;
  for (const auto& override_range : overrides) {
    const double start = flight::maximum(
        0.0, flight::minimum(static_cast<double>(out->text.length()), override_range->start));
    const double end = flight::maximum(
        start, flight::minimum(static_cast<double>(out->text.length()), override_range->end));
    if (start == end) continue;

    flight::Array<flight::Ref<flight::types::TextFormatRange>> next;
    for (const auto& range : ranges) {
      if (range->end <= start || range->start >= end) {
        write_format_range(next, range->format, range->start, range->end);
        continue;
      }
      if (range->start < start) write_format_range(next, range->format, range->start, start);
      write_format_range(
          next,
          merge_text_format(range->format, override_range->format),
          flight::maximum(range->start, start),
          flight::minimum(range->end, end));
      if (range->end > end) write_format_range(next, range->format, end, range->end);
    }
    ranges = next;
  }

  out->format_ranges.resize(0.0);
  for (const auto& range : ranges) {
    write_format_range(out->format_ranges, range->format, range->start, range->end);
  }
}

inline flight::Record<flight::String, flight::String> named_entities = {
    {flight::String("amp"), flight::String("&")},
    {flight::String("apos"), flight::String("'")},
    {flight::String("gt"), flight::String(">")},
    {flight::String("lt"), flight::String("<")},
    {flight::String("nbsp"), flight::String(" ")},
    {flight::String("quot"), flight::String("\"")},
};

inline bool rich_text_whitespace(char16_t unit) {
  return (unit >= 0x0009 && unit <= 0x000D) || unit == 0x0020 || unit == 0x00A0 ||
         unit == 0x1680 || (unit >= 0x2000 && unit <= 0x200A) || unit == 0x2028 ||
         unit == 0x2029 || unit == 0x202F || unit == 0x205F || unit == 0x3000 ||
         unit == 0xFEFF;
}

inline flight::String rich_text_trim_start(const flight::String& value) {
  const auto& units = value.native();
  std::size_t first = 0;
  while (first < units.size() && rich_text_whitespace(units[first])) ++first;
  return flight::String(units.substr(first));
}

inline flight::String decode_html_entities(flight::String value) {
  return value.replace(
      flight::RegExp(flight::String("&(#x[0-9a-f]+|#[0-9]+|[a-z]+);"), flight::String("gi")),
      [=](flight::String, flight::String entity) {
        const flight::String lower = entity.to_lower();
        if (lower.starts_with(flight::String("#x"))) {
          return flight::String::from_code_point(flight::parse_int(lower.slice(2.0), 16.0));
        }
        if (lower.starts_with(flight::String("#"))) {
          return flight::String::from_code_point(flight::parse_int(lower.slice(1.0), 10.0));
        }
        const auto named = named_entities.get(lower);
        return named.has_value()
                   ? named.value()
                   : flight::String("&") + flight::to_string(entity) + flight::String(";");
      });
}

inline void append_text(
    flight::Ref<flight::types::RichTextContent> out,
    flight::String text,
    flight::Ref<flight::types::TextFormat> format,
    bool condense_white,
    double max_chars) {
  flight::String value = decode_html_entities(text);
  if (condense_white) {
    value = value.replace(
        flight::RegExp(flight::String("[ \\f\\n\\r\\t\\v]+"), flight::String("g")),
        flight::String(" "));
    if (out->text.length() == 0) value = rich_text_trim_start(value);
    if (out->text.ends_with(flight::String(" "))) value = rich_text_trim_start(value);
  }
  if (value.length() == 0) return;

  const double remaining = max_chars < 0.0
                               ? static_cast<double>(value.length())
                               : flight::maximum(
                                     0.0, max_chars - static_cast<double>(out->text.length()));
  if (remaining == 0.0) return;
  if (static_cast<double>(value.length()) > remaining) value = value.slice(0.0, remaining);

  const double start = static_cast<double>(out->text.length());
  out->text += value;
  write_format_range(
      out->format_ranges, format, start, static_cast<double>(out->text.length()));
}

inline void compute_rich_text_content(
    flight::Ref<flight::types::RichTextContent> out,
    flight::Ref<flight::types::RichTextData> data,
    std::optional<std::optional<flight::String>> password_character = std::nullopt) {
  password_character = password_character.value_or(std::nullopt);
  out->text = flight::String("");
  out->format_ranges.resize(0.0);
  const auto base_format = create_base_format(data);
  const auto source = get_renderable_source(data, password_character.value());
  if (source.length() == 0) return;
  append_text(out, source, base_format, data->condense_white, data->max_chars);
  clamp_ranges(out->format_ranges, static_cast<double>(out->text.length()));
  apply_text_format_ranges(out, data->text_format_ranges);
}

} // namespace flight::textlayout
