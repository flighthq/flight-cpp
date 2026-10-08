// Runtime completion of the generated partial richText module.
#pragma once

#include <mutex>
#include <optional>
#include <unordered_map>
#include <flight/runtime.hpp>
#include <flight/scene2d/display_object.hpp>
#include <flight/text/text_label.hpp>
#include <flight/types/rich_text.hpp>
#include <flight/types/rich_text_content.hpp>

namespace flight::text {

namespace detail {

inline std::mutex rich_text_runtime_mutex;
inline std::unordered_map<const void*, std::weak_ptr<flight::types::RichTextRuntime>>
    rich_text_runtimes;

inline void register_rich_text_runtime(
    const flight::Ref<flight::types::RichText>& source,
    const flight::Ref<flight::types::RichTextRuntime>& runtime) {
  const std::lock_guard lock(rich_text_runtime_mutex);
  rich_text_runtimes.insert_or_assign(source.get(), runtime);
}

inline flight::Ref<flight::types::RichTextRuntime> find_rich_text_runtime(
    const void* identity) {
  const std::lock_guard lock(rich_text_runtime_mutex);
  const auto found = rich_text_runtimes.find(identity);
  if (found == rich_text_runtimes.end()) return {};
  if (auto runtime = found->second.lock()) return runtime;
  rich_text_runtimes.erase(found);
  return {};
}

} // namespace detail

inline flight::Ref<flight::types::RichTextData> create_rich_text_data() {
  auto format = flight::make_ref<flight::types::TextFormat>(flight::types::TextFormat{});
  return flight::make_ref<flight::types::RichTextData>(flight::types::RichTextData{
      .entity_runtime_key = std::nullopt,
      .auto_size = flight::String("none"),
      .height = 100.0,
      .text = flight::String(""),
      .text_format = format,
      .vertical_align = flight::String("top"),
      .width = 100.0,
      .background = false,
      .background_color = 0xffffff,
      .border = false,
      .border_color = 0.0,
      .condense_white = false,
      .default_text_format = format,
      .max_chars = -1.0,
      .mouse_wheel_enabled = true,
      .multiline = true,
      .scroll_h = 0.0,
      .scroll_v = 1.0,
      .selectable = true,
      .text_color = 0x000000ff,
      .text_format_ranges = {},
      .word_wrap = false,
  });
}

template <typename Options>
inline flight::Ref<flight::types::RichTextData> create_rich_text_data(Options&&) {
  return create_rich_text_data();
}

inline flight::Ref<flight::types::RichText> make_rich_text() {
  auto node = flight::make_ref<flight::types::RichText>(flight::types::RichText{
      .data = create_rich_text_data(),
      .enabled = true,
      .kind = flight::types::rich_text_kind,
      .name = std::nullopt,
      .entity_runtime_key = flight::scene2d::create_node2_d_runtime(),
      .alpha = 1.0,
      .visible = true,
      .blend_mode = std::nullopt,
      .clip = std::nullopt,
      .material = std::nullopt,
      .material_data = std::nullopt,
      .pivot_x = 0.0,
      .pivot_y = 0.0,
      .rotation = 0.0,
      .scale_x = 1.0,
      .scale_y = 1.0,
      .skew_x = 0.0,
      .skew_y = 0.0,
      .x = 0.0,
      .y = 0.0,
  });
  auto runtime = flight::make_ref<flight::types::RichTextRuntime>();
  runtime->input = std::nullopt;
  runtime->rich_text_content = std::nullopt;
  runtime->selection_begin_index = 0.0;
  runtime->selection_end_index = 0.0;
  runtime->text_field_signals = std::nullopt;
  runtime->text_layout = std::nullopt;
  runtime->text_layout_using_content_id = -1.0;
  runtime->text_layout_using_text = std::nullopt;
  detail::register_rich_text_runtime(node, runtime);
  return node;
}

template <typename Options>
inline flight::Ref<flight::types::RichText> create_rich_text(Options&&) {
  return make_rich_text();
}

inline flight::Ref<flight::types::RichText> create_rich_text() {
  return make_rich_text();
}

using RichTextRuntimeView = flight::StructuralRef<
    flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::RichTextRuntime>>>>;

inline RichTextRuntimeView get_rich_text_runtime(
    flight::StructuralRef<
        flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::RichText>>>> source) {
  auto runtime = detail::find_rich_text_runtime(source.shared_native_object().get());
  if (!runtime) throw flight::Error(flight::String("RichText runtime is unavailable"));
  return RichTextRuntimeView(runtime);
}

inline RichTextRuntimeView get_rich_text_runtime(
    const flight::Ref<flight::types::RichText>& source) {
  auto runtime = detail::find_rich_text_runtime(source.get());
  if (!runtime) throw flight::Error(flight::String("RichText runtime is unavailable"));
  return RichTextRuntimeView(runtime);
}

inline void invalidate_rich_text_content(flight::Ref<flight::types::RichText> source) {
  flight::node::invalidate_node_local_content(source);
  if (source->data->auto_size != flight::String("none")) {
    flight::node::invalidate_node_local_bounds(source);
  }
}

inline flight::String get_rich_text_string(flight::Ref<flight::types::RichText> source) {
  return source->data->text;
}

inline void set_rich_text_string(flight::Ref<flight::types::RichText> source,
                                 flight::String value) {
  if (source->data->text == value) return;
  source->data->text = std::move(value);
  source->data->text_format_ranges.clear();
  invalidate_rich_text_content(source);
}

inline void append_rich_text_string(flight::Ref<flight::types::RichText> source,
                                    flight::String value) {
  if (value.length() == 0) return;
  source->data->text += value;
  invalidate_rich_text_content(source);
}

inline void set_rich_text_content(
    flight::Ref<flight::types::RichText> source,
    flight::Ref<flight::types::RichTextContent> content) {
  source->data->text = content->text;
  source->data->text_format_ranges = content->format_ranges;
  if (auto runtime = detail::find_rich_text_runtime(source.get())) {
    runtime->rich_text_content = content;
  }
  invalidate_rich_text_content(source);
}

inline void set_rich_text_scroll_h(
    flight::Ref<flight::types::RichText> source, double value,
    std::optional<flight::Ref<flight::types::TextLayoutResult>> = std::nullopt) {
  source->data->scroll_h = flight::maximum(0.0, value);
}

inline void set_rich_text_scroll_v(
    flight::Ref<flight::types::RichText> source, double value,
    std::optional<flight::Ref<flight::types::TextLayoutResult>> = std::nullopt) {
  source->data->scroll_v = flight::maximum(1.0, value);
}

} // namespace flight::text
