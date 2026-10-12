// Derived from @flighthq/textlayout/packages/textlayout/src/textFormat.ts
// at generated digest 738ca43812bb9a079f7f85bf43e32f50f96fb5c29e264af5c16c15ef70a1f1fb.
//
// OVERRIDE: the emitter refuses textFormat.ts because Object.keys + keyof cannot be lowered, so the
// emitted header is a PARTIAL. This supplies all five functions.
//
// Re-derived 2026-10-12 by checking it still matches the current source: ascent = size ?? 12,
// descent = ascent * 0.185, leading = leading ?? 0, height = ascent + descent + leading, and
// merge copies the base then assigns each present override field -- all unchanged.
#pragma once

#include <flight/runtime.hpp>
#include <flight/types/text_format.hpp>

static_assert(flight::runtime_contract.compiler_contract == "flight-runtime-contract/2", "Flight compiler/runtime contract mismatch");
static_assert(flight::runtime_contract.cpp_abi == 1, "Flight C++ runtime ABI mismatch");

namespace flight::textlayout {

inline const double default_size = 12.0;

inline double get_text_format_ascent(flight::Ref<flight::types::TextFormat> format) {
  return format->size.value_or(default_size);
}

inline double get_text_format_descent(flight::Ref<flight::types::TextFormat> format) {
  return format->size.value_or(default_size) * 0.185;
}

inline double get_text_format_leading(flight::Ref<flight::types::TextFormat> format) {
  return format->leading.value_or(0.0);
}

inline double get_text_format_height(flight::Ref<flight::types::TextFormat> format) {
  return get_text_format_ascent(format) + get_text_format_descent(format) +
         get_text_format_leading(format);
}

// OVERRIDE: Object.keys plus keyof could not be lowered. TextFormat's generated carrier represents
// each optional source property directly, so copy the base and assign each present override value.
inline flight::Ref<flight::types::TextFormat> merge_text_format(
    flight::Ref<flight::types::TextFormat> base,
    flight::Ref<flight::types::TextFormat> override_format) {
  auto result = flight::make_ref<flight::types::TextFormat>(*base);
#define FLIGHT_TEXTLAYOUT_MERGE_FIELD(name)        \
  if (override_format->name.has_value()) {         \
    result->name = override_format->name.value();  \
  }
  FLIGHT_TEXTLAYOUT_MERGE_FIELD(align)
  FLIGHT_TEXTLAYOUT_MERGE_FIELD(block_indent)
  FLIGHT_TEXTLAYOUT_MERGE_FIELD(bold)
  FLIGHT_TEXTLAYOUT_MERGE_FIELD(bullet)
  FLIGHT_TEXTLAYOUT_MERGE_FIELD(color)
  FLIGHT_TEXTLAYOUT_MERGE_FIELD(font)
  FLIGHT_TEXTLAYOUT_MERGE_FIELD(indent)
  FLIGHT_TEXTLAYOUT_MERGE_FIELD(italic)
  FLIGHT_TEXTLAYOUT_MERGE_FIELD(kerning)
  FLIGHT_TEXTLAYOUT_MERGE_FIELD(leading)
  FLIGHT_TEXTLAYOUT_MERGE_FIELD(left_margin)
  FLIGHT_TEXTLAYOUT_MERGE_FIELD(letter_spacing)
  FLIGHT_TEXTLAYOUT_MERGE_FIELD(list_marker)
  FLIGHT_TEXTLAYOUT_MERGE_FIELD(right_margin)
  FLIGHT_TEXTLAYOUT_MERGE_FIELD(size)
  FLIGHT_TEXTLAYOUT_MERGE_FIELD(strikethrough)
  FLIGHT_TEXTLAYOUT_MERGE_FIELD(tab_stops)
  FLIGHT_TEXTLAYOUT_MERGE_FIELD(target)
  FLIGHT_TEXTLAYOUT_MERGE_FIELD(underline)
  FLIGHT_TEXTLAYOUT_MERGE_FIELD(url)
  FLIGHT_TEXTLAYOUT_MERGE_FIELD(variations)
#undef FLIGHT_TEXTLAYOUT_MERGE_FIELD
  return result;
}

} // namespace flight::textlayout
