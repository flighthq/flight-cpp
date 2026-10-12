// Derived from @flighthq/textinput/packages/textinput/src/textInput.ts
// at generated digest 816ef0938e6e85dda8fe79efaff375ab82528bb2c3da0acfecb7c2449d728880.
//
// Re-derived 2026-10-12. text_input.ts now emits in full, so this is not completing a partial: it
// supplies the state over the exact RichText runtime the text override retains, and every exported
// textInput function it covers (enable/disable/has/get state, caret and selection accessors, insert and
// set selection) is present. The companion manager, history, keyboard and clipboard helpers are separate
// modules and are not this file's surface.
// Core text-input state over the exact RichText runtime retained by the text override.
#pragma once

#include <algorithm>
#include <optional>
#include <flight/runtime.hpp>
#include <flight/text/rich_text.hpp>
#include <flight/types/text_input_state.hpp>

namespace flight::textinput {

inline flight::Ref<flight::types::TextInputState> create_text_input_state() {
  return flight::make_ref<flight::types::TextInputState>(flight::types::TextInputState{
      .always_show_selection = false,
      .caret_color = 0.0,
      .caret_index = 0.0,
      .caret_width = 1.0,
      .desired_caret_x = -1.0,
      .display_as_password = false,
      .focused = false,
      .history = {},
      .history_index = -1.0,
      .history_limit = 100.0,
      .password_character = flight::String("•"),
      .restrict = flight::String(""),
      .selection_alpha = 0.35,
      .selection_color = 30935.0,
      .selection_index = 0.0,
  });
}

inline std::optional<flight::Ref<flight::types::TextInputState>> get_text_input_state(
    const flight::Ref<flight::types::RichText>& node) {
  if (auto runtime = flight::text::detail::find_rich_text_runtime(node.get())) {
    return runtime->input;
  }
  return std::nullopt;
}

inline bool has_text_input(const flight::Ref<flight::types::RichText>& node) {
  return get_text_input_state(node).has_value();
}

inline flight::Ref<flight::types::TextInputState> enable_text_input(
    const flight::Ref<flight::types::RichText>& node) {
  auto runtime = flight::text::detail::find_rich_text_runtime(node.get());
  if (!runtime) throw flight::Error(flight::String("RichText runtime is unavailable"));
  if (!runtime->input.has_value()) runtime->input = create_text_input_state();
  return runtime->input.value();
}

inline void disable_text_input(const flight::Ref<flight::types::RichText>& node) {
  if (auto runtime = flight::text::detail::find_rich_text_runtime(node.get())) {
    runtime->input = std::nullopt;
  }
}

inline double get_text_input_caret_index(const flight::Ref<flight::types::RichText>& source) {
  const auto state = get_text_input_state(source);
  return state.has_value() ? state.value()->caret_index : 0.0;
}

inline double get_text_input_selection_begin_index(
    const flight::Ref<flight::types::RichText>& source) {
  const auto state = get_text_input_state(source);
  return state.has_value()
             ? std::min(state.value()->caret_index, state.value()->selection_index)
             : 0.0;
}

inline double get_text_input_selection_end_index(
    const flight::Ref<flight::types::RichText>& source) {
  const auto state = get_text_input_state(source);
  return state.has_value()
             ? std::max(state.value()->caret_index, state.value()->selection_index)
             : 0.0;
}

inline flight::String get_text_input_display_text(
    const flight::Ref<flight::types::RichText>& source) {
  return source->data->text;
}

inline void set_text_input_selection(const flight::Ref<flight::types::RichText>& source,
                                     double begin_index, double end_index) {
  auto state = enable_text_input(source);
  const double length = static_cast<double>(source->data->text.length());
  state->selection_index = std::clamp(begin_index, 0.0, length);
  state->caret_index = std::clamp(end_index, 0.0, length);
}

inline void insert_text_input(const flight::Ref<flight::types::RichText>& source,
                              flight::String value) {
  auto state = enable_text_input(source);
  const double begin = get_text_input_selection_begin_index(source);
  const double end = get_text_input_selection_end_index(source);
  const auto current = source->data->text;
  source->data->text =
      current.slice(0, static_cast<std::ptrdiff_t>(begin)) + value +
      current.slice(static_cast<std::ptrdiff_t>(end));
  state->caret_index = begin + static_cast<double>(value.length());
  state->selection_index = state->caret_index;
  flight::text::invalidate_rich_text_content(source);
}

} // namespace flight::textinput
