// Runtime completion of the generated partial textLabel module.
#pragma once

#include <optional>
#include <flight/runtime.hpp>
#include <flight/scene2d/display_object.hpp>
#include <flight/types/text_format.hpp>
#include <flight/types/text_label.hpp>

namespace flight::text {

inline flight::Ref<flight::types::TextLabelData> create_text_label_data() {
  return flight::make_ref<flight::types::TextLabelData>(flight::types::TextLabelData{
      .entity_runtime_key = std::nullopt,
      .auto_size = flight::String("none"),
      .height = 100.0,
      .text = flight::String(""),
      .text_format = flight::make_ref<flight::types::TextFormat>(flight::types::TextFormat{}),
      .vertical_align = flight::String("top"),
      .width = 100.0,
  });
}

template <typename Options>
inline flight::Ref<flight::types::TextLabelData> create_text_label_data(Options&&) {
  return create_text_label_data();
}

inline flight::Ref<flight::types::TextLabel> make_text_label() {
  return flight::make_ref<flight::types::TextLabel>(flight::types::TextLabel{
      .data = create_text_label_data(),
      .enabled = true,
      .kind = flight::types::text_label_kind,
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
}

template <typename Options>
inline flight::Ref<flight::types::TextLabel> create_text_label(Options&&) {
  return make_text_label();
}

inline flight::Ref<flight::types::TextLabel> create_text_label() {
  return make_text_label();
}

inline flight::Ref<flight::types::TextFormat> get_text_label_format(
    flight::Ref<flight::types::TextLabel> source) {
  return source->data->text_format;
}

inline flight::String get_text_label_string(flight::Ref<flight::types::TextLabel> source) {
  return source->data->text;
}

inline void invalidate_text_label_content(flight::Ref<flight::types::TextLabel> source) {
  flight::node::invalidate_node_local_content(source);
  flight::node::invalidate_node_local_bounds(source);
}

inline void set_text_label_auto_size(flight::Ref<flight::types::TextLabel> source,
                                     flight::types::TextAutoSize value) {
  if (source->data->auto_size == value) return;
  source->data->auto_size = std::move(value);
  invalidate_text_label_content(source);
}

inline void set_text_label_height(flight::Ref<flight::types::TextLabel> source, double value) {
  if (source->data->height == value) return;
  source->data->height = value;
  invalidate_text_label_content(source);
}

inline void set_text_label_string(flight::Ref<flight::types::TextLabel> source,
                                  flight::String value) {
  if (source->data->text == value) return;
  source->data->text = std::move(value);
  invalidate_text_label_content(source);
}

inline void set_text_label_format(flight::Ref<flight::types::TextLabel> source,
                                  flight::Ref<flight::types::TextFormat> value) {
  if (source->data->text_format == value) return;
  source->data->text_format = std::move(value);
  invalidate_text_label_content(source);
}

inline void set_text_label_vertical_align(
    flight::Ref<flight::types::TextLabel> source,
    flight::types::TextVerticalAlign value) {
  if (source->data->vertical_align == value) return;
  source->data->vertical_align = std::move(value);
  invalidate_text_label_content(source);
}

inline void set_text_label_width(flight::Ref<flight::types::TextLabel> source, double value) {
  if (source->data->width == value) return;
  source->data->width = value;
  invalidate_text_label_content(source);
}

} // namespace flight::text
