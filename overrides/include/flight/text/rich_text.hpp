// Derived from @flighthq/text/packages/text/src/richText.ts
// at generated digest 4f2b3f6739fd0474b0c7130721027f3f0e7dbdd0d5a397fd64edf776ac82f856.
// The default-only construction core is kept; inherited data cells are initialized on the same allocation.
// Default colors and separate format owners follow the original factory; supplied options are explicit partial uses.
//
// OVERRIDE: the emitted rich_text.hpp is a PARTIAL (14 declarations omitted). This completes the
// construction, content and runtime-access core the examples use.
//
// createRichTextRuntime is supplied here with the declared Node2D runtime heritage: the one object the
// factory allocates is stored on the node's EntityRuntimeKey and getRichTextRuntime reads that same
// owner back. The former weak side registry retained a second runtime and let it die at factory return;
// it is gone.
//
// KNOWN GAP, measured 2026-10-12 and NOT closed by re-deriving: ten omitted declarations remain absent
// from the compiled surface -- buildRichTextLayoutParams, computeRichTextLocalBoundsRectangle,
// dispatchRichTextLinkAtPoint, emitTextFieldChange, emitTextFieldScroll,
// enableTextFieldSignals, equalsRichTextFormat, getRichTextFormatRangeAt, getRichTextPasswordCharacter,
// getTextFieldSignals. buildTextLayoutParams therefore stays unset on the retained runtime, so layout
// paths that require it remain a recorded gap. Treat the module's surface as incomplete until they exist.
// Runtime completion of the generated partial richText module.
#pragma once

#include <optional>
#include <type_traits>
#include <flight/node/native_node2d_access.hpp>
#include <flight/out_of_profile.hpp>
#include <flight/runtime.hpp>
#include <flight/scene2d/display_object.hpp>
#include <flight/text/text_label.hpp>
#include <flight/types/rich_text.hpp>
#include <flight/types/rich_text_content.hpp>

namespace flight::text {

namespace detail {

template <typename Options>
inline bool rich_text_default_options(const Options& options) {
  using Stored = std::remove_cvref_t<Options>;
  if constexpr (std::is_same_v<Stored, std::nullopt_t> || std::is_same_v<Stored, flight::Undefined> ||
                std::is_same_v<Stored, flight::Null> || std::is_same_v<Stored, std::nullptr_t>) return true;
  else if constexpr (flight::detail::is_std_optional_v<Stored>)
    return !options.has_value() || rich_text_default_options(*options);
  else if constexpr (flight::detail::is_shared_ptr_v<Stored>) return !options;
  else if constexpr (std::is_same_v<Stored, flight::Any>) return options.is_nullish();
  else return false;
}

} // namespace detail

// The runtime is the node's own EntityRuntimeKey value. There is no side table: the node owns the
// RichTextRuntime it was constructed with, exactly as the source stores createRichTextRuntime on the
// node and getRichTextRuntime reads it back.

inline flight::Ref<flight::types::RichTextData> create_rich_text_data() {
  auto format = flight::make_ref<flight::types::TextFormat>(flight::types::TextFormat{});
  auto out = flight::make_ref<flight::types::RichTextData>(flight::types::RichTextData{
      .background = false,
      .background_color = 0xffffffff,
      .border = false,
      .border_color = 0x000000ff,
      .condense_white = false,
      .default_text_format = flight::make_ref<flight::types::TextFormat>(),
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
  // C++ aggregate designators cannot name TextLabelData base cells. Keep one owner.
  out->entity_runtime_key = std::nullopt;
  out->auto_size = flight::String("none");
  out->height = 100.0;
  out->text = flight::String("");
  out->text_format = format;
  out->vertical_align = flight::String("top");
  out->width = 100.0;
  return out;
}

template <typename Options>
inline flight::Ref<flight::types::RichTextData> create_rich_text_data(Options&& options) {
  if (detail::rich_text_default_options(options)) return create_rich_text_data();
  throw flight::OutOfProfileError("createRichTextData supplied options are unsupported by this partial override");
}

inline flight::Ref<flight::types::RichTextRuntime> create_rich_text_runtime() {
  auto out = flight::make_ref<flight::types::RichTextRuntime>();
  // Copy only fresh, unpublished base initialization. No observed owner is replaced.
  static_cast<flight::node::NativeNode2DRuntimeOwner::element_type&>(*out) =
      *flight::scene2d::create_node_2d_runtime();
  out->input = std::nullopt;
  out->rich_text_content = std::nullopt;
  out->selection_begin_index = 0.0;
  out->selection_end_index = 0.0;
  out->text_field_signals = std::nullopt;
  out->text_layout = std::nullopt;
  out->text_layout_using_content_id = -1.0;
  out->text_layout_using_text = std::nullopt;
  return out;
}

inline flight::Ref<flight::types::RichText> make_rich_text() {
  return flight::make_ref<flight::types::RichText>(flight::types::RichText{
      .data = create_rich_text_data(),
      .enabled = true,
      .kind = flight::types::rich_text_kind,
      .name = std::nullopt,
      .entity_runtime_key = create_rich_text_runtime(),
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
inline flight::Ref<flight::types::RichText> create_rich_text(Options&& options) {
  if (detail::rich_text_default_options(options)) return make_rich_text();
  throw flight::OutOfProfileError("createRichText supplied options are unsupported by this partial override");
}

inline flight::Ref<flight::types::RichText> create_rich_text() {
  return make_rich_text();
}

using RichTextRuntimeView = flight::StructuralRef<
    flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::RichTextRuntime>>>>;

inline RichTextRuntimeView get_rich_text_runtime(
    const flight::Ref<flight::types::RichText>& source) {
  if (!source->entity_runtime_key.has_value()) {
    throw flight::Error(flight::String("RichText runtime is unavailable"));
  }
  return RichTextRuntimeView(source->entity_runtime_key.value());
}

inline RichTextRuntimeView get_rich_text_runtime(
    flight::StructuralRef<
        flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::RichText>>>> source) {
  return get_rich_text_runtime(
      flight::structural_ref_cast<flight::Ref<flight::types::RichText>>(source));
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
  if (source->entity_runtime_key.has_value()) {
    source->entity_runtime_key.value()->rich_text_content = content;
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
