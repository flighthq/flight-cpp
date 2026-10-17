// Source-derived textLabel.ts. Native runtime heritage retains one original owner.
#pragma once
#include <flight/scene2d/display_object.hpp>
#include <flight/node/native_node2d_access.hpp>
#include <flight/text/text_label_layout.hpp>
#include <flight/textlayout/text_bounds.hpp>
#include <flight/textlayout/text_format_range.hpp>
namespace flight::text {
using NativeTextLabel = flight::Ref<flight::types::TextLabel>;
using NativeTextLabelData = flight::Ref<flight::types::TextLabelData>;
using NativeTextLabelRuntime = flight::Ref<flight::types::TextLabelRuntime>;
using TextLabelDataOptions = std::optional<flight::StructuralRef<flight::RowReadonly<flight::RowPartial<flight::RowOf<NativeTextLabelData>>>>>;
using TextLabelOptions = std::optional<flight::StructuralRef<flight::RowReadonly<flight::RowPartial<flight::RowOf<NativeTextLabel>>>>>;
inline void initialize_text_label_data(NativeTextLabelData out, TextLabelDataOptions data = std::nullopt) {
  using flight::RowKey;
  out->auto_size = flight::scene2d::node_2d_option_value_or<RowKey<"autoSize">>(data, flight::String("none"));
  out->height = flight::scene2d::node_2d_option_value_or<RowKey<"height">>(data, 100.0);
  out->text = flight::scene2d::node_2d_option_value_or<RowKey<"text">>(data, flight::String(""));
  out->text_format = flight::scene2d::node_2d_option_value_or<RowKey<"textFormat">>(data, flight::make_ref<flight::types::TextFormat>());
  out->vertical_align = flight::scene2d::node_2d_option_value_or<RowKey<"verticalAlign">>(data, flight::String("top"));
  out->width = flight::scene2d::node_2d_option_value_or<RowKey<"width">>(data, 100.0);
}
inline NativeTextLabelData create_text_label_data(TextLabelDataOptions data = std::nullopt) {
  auto out = flight::make_ref<flight::types::TextLabelData>();
  initialize_text_label_data(out, data);
  return out;
}
inline NativeTextLabelRuntime get_text_label_runtime(NativeTextLabel source) { return source->entity_runtime_key.value(); }
inline void compute_text_label_local_bounds_rectangle(flight::Ref<flight::types::Rectangle> out, NativeTextLabel label) {
  auto data = label->data;
  auto layout = data->auto_size == flight::String("none") ? std::nullopt : get_text_layout(label);
  if (!layout) { out->x = out->y = 0; out->width = data->width; out->height = data->height; return; }
  // TextLabelData has no wordWrap member. This read-only value specification
  // supplies its actual absence to the generic bounds helper; no data owner is cast.
  auto spec = flight::make_structural_ref<flight::types::TextBoundsSpec::schema_type>(
      flight::row_field<flight::RowKey<"autoSize">>(data->auto_size),
      flight::row_field<flight::RowKey<"height">>(data->height),
      flight::row_field<flight::RowKey<"width">>(data->width),
      flight::row_field<flight::RowKey<"wordWrap">>(std::optional<bool>{}));
  flight::textlayout::compute_text_bounds_rectangle(out, spec, *layout);
}
inline NativeTextLabelRuntime create_text_label_runtime() {
  auto out = flight::make_ref<flight::types::TextLabelRuntime>();
  // Copy only fresh, unpublished base initialization. No observed owner is replaced.
  static_cast<flight::node::NativeNode2DRuntimeOwner::element_type&>(*out) = *flight::scene2d::create_node_2d_runtime();
  out->build_text_layout_params = [](auto source, flight::types::TextMeasureFunction measure) {
    auto label = source.template shared_exact_object<flight::types::TextLabel>();
    if (!label) throw std::logic_error("NativeTextLabel layout owner mismatch");
    auto data = label->data;
    auto params = flight::make_ref<flight::types::TextLayoutParams>();
    params->format_ranges = flight::Array<flight::Ref<flight::types::TextFormatRange>>{
        flight::textlayout::create_text_format_range(data->text_format, 0, static_cast<double>(data->text.length()))};
    params->text = data->text; params->width = data->width; params->height = data->height;
    params->measure = std::move(measure);
    params->vertical_align = data->auto_size == flight::String("none") ? data->vertical_align : flight::String("top");
    return params;
  };
  out->compute_local_bounds_rectangle = [](auto rectangle, auto source) {
    auto label = source.template shared_exact_object<flight::types::TextLabel>();
    if (!label) throw std::logic_error("NativeTextLabel bounds owner mismatch");
    compute_text_label_local_bounds_rectangle(rectangle, label);
  };
  out->text_layout_using_content_id = -1;
  return out;
}
inline NativeTextLabel create_text_label(TextLabelOptions obj = std::nullopt) {
  using flight::RowKey;
  auto out = flight::make_ref<flight::types::TextLabel>();
  auto data = flight::scene2d::node_2d_optional_value<RowKey<"data">, TextLabelOptions::value_type::schema_type, NativeTextLabelData>(obj);
  out->data = data ? create_text_label_data(TextLabelDataOptions{TextLabelDataOptions::value_type(*data)}) : create_text_label_data();
  out->entity_runtime_key = create_text_label_runtime();
  out->kind = flight::types::text_label_kind;
  out->enabled = flight::scene2d::node_2d_option_value_or<RowKey<"enabled">>(obj, true);
  out->visible = flight::scene2d::node_2d_option_value_or<RowKey<"visible">>(obj, true);
  out->alpha = flight::scene2d::node_2d_option_value_or<RowKey<"alpha">>(obj, 1.0);
  out->name = flight::scene2d::node_2d_optional_value<RowKey<"name">, TextLabelOptions::value_type::schema_type, flight::String>(obj);
#define FLIGHT_TEXT_OPTION(member, key, fallback) out->member = flight::scene2d::node_2d_option_value_or<RowKey<key>>(obj, fallback)
  FLIGHT_TEXT_OPTION(pivot_x, "pivotX", 0.0); FLIGHT_TEXT_OPTION(pivot_y, "pivotY", 0.0);
  FLIGHT_TEXT_OPTION(rotation, "rotation", 0.0); FLIGHT_TEXT_OPTION(scale_x, "scaleX", 1.0); FLIGHT_TEXT_OPTION(scale_y, "scaleY", 1.0);
  FLIGHT_TEXT_OPTION(skew_x, "skewX", 0.0); FLIGHT_TEXT_OPTION(skew_y, "skewY", 0.0); FLIGHT_TEXT_OPTION(x, "x", 0.0); FLIGHT_TEXT_OPTION(y, "y", 0.0);
#undef FLIGHT_TEXT_OPTION
  if (obj) {
    out->blend_mode = flight::row_get<RowKey<"blendMode">>(*obj);
    out->clip = flight::row_get<RowKey<"clip">>(*obj);
    out->material = flight::row_get<RowKey<"material">>(*obj);
    out->material_data = flight::row_get<RowKey<"materialData">>(*obj);
  }
  return out;
}
inline auto make_text_label() { return create_text_label(); }
inline auto get_text_label_format(NativeTextLabel source) { return source->data->text_format; }
inline auto get_text_label_string(NativeTextLabel source) { return source->data->text; }
inline void invalidate_text_label_bounds(NativeTextLabel source) {
  auto runtime = get_text_label_runtime(source);
  runtime->local_bounds_id = flight::unsigned_right_shift(runtime->local_bounds_id + 1, 0);
}
inline void invalidate_text_label_content(NativeTextLabel source) {
  auto runtime = get_text_label_runtime(source);
  runtime->local_content_id = flight::unsigned_right_shift(runtime->local_content_id + 1, 0);
  if (source->data->auto_size != flight::String("none")) invalidate_text_label_bounds(source);
}
inline void set_text_label_string(NativeTextLabel source, flight::String value) {
  if (source->data->text == value) return;
  source->data->text = std::move(value); invalidate_text_label_content(source);
}
inline void append_text_label_string(NativeTextLabel source, flight::String value) {
  if (value.length() == 0) return;
  source->data->text += value; invalidate_text_label_content(source);
}
inline void set_text_label_format(NativeTextLabel source, flight::Ref<flight::types::TextFormat> value) {
  source->data->text_format = value; invalidate_text_label_content(source);
}
inline void set_text_label_auto_size(NativeTextLabel source, flight::types::TextAutoSize value) {
  if (source->data->auto_size == value) return;
  source->data->auto_size = value;
  auto runtime = get_text_label_runtime(source);
  runtime->local_content_id = flight::unsigned_right_shift(runtime->local_content_id + 1, 0);
  invalidate_text_label_bounds(source);
}
inline void set_text_label_vertical_align(NativeTextLabel source, flight::types::TextVerticalAlign value) {
  if (source->data->vertical_align == value) return;
  source->data->vertical_align = value;
  auto runtime = get_text_label_runtime(source);
  runtime->local_content_id = flight::unsigned_right_shift(runtime->local_content_id + 1, 0);
}
inline void set_text_label_height(NativeTextLabel source, double value) {
  if (source->data->height == value) return;
  source->data->height = value;
  auto runtime = get_text_label_runtime(source);
  runtime->local_content_id = flight::unsigned_right_shift(runtime->local_content_id + 1, 0);
  invalidate_text_label_bounds(source);
}
inline void set_text_label_width(NativeTextLabel source, double value) {
  if (source->data->width == value) return;
  source->data->width = value;
  auto runtime = get_text_label_runtime(source);
  runtime->local_content_id = flight::unsigned_right_shift(runtime->local_content_id + 1, 0);
  invalidate_text_label_bounds(source);
}
} // namespace flight::text
