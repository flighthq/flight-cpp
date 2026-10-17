// Source-derived textLabelLayout.ts: cache lives on the original TextLabelRuntime.
#pragma once
#include <flight/types/text_label.hpp>
#include <flight/textlayout/text_layout.hpp>
#include <flight/textlayout/text_layout_measure.hpp>
#include <flight/textlayout/text_layout_runtime.hpp>
#include <flight/textlayout/text_metrics.hpp>
namespace flight::text {
inline std::optional<std::function<void(flight::String, flight::String)>> text_label_guard;
inline void ensure_text_layout(flight::Ref<flight::types::TextLabel> source) {
  auto runtime = source->entity_runtime_key.value();
  const auto content = runtime->local_content_id;
  if (runtime->text_layout && runtime->text_layout_using_content_id == content) {
    if (text_label_guard && runtime->text_layout_using_text && source->data->text != *runtime->text_layout_using_text)
      (*text_label_guard)(source->data->text, *runtime->text_layout_using_text);
    return;
  }
  auto measure = flight::textlayout::get_text_layout_measure_provider();
  if (!measure) return;
  using View = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::TextLabel>>>>;
  auto params = runtime->build_text_layout_params(View(source), *measure);
  flight::textlayout::compute_text_layout(flight::textlayout::get_text_layout_result(runtime), params);
  runtime->text_layout_using_content_id = content;
  runtime->text_layout_using_text = source->data->text;
}
inline std::optional<flight::Ref<flight::types::TextLayoutResult>> get_text_layout(flight::Ref<flight::types::TextLabel> source) {
  ensure_text_layout(source);
  return source->entity_runtime_key.value()->text_layout;
}
inline void get_text_layout_metrics(flight::Ref<flight::types::TextMetrics> out, flight::Ref<flight::types::TextLabel> source) {
  auto layout = get_text_layout(source);
  if (layout) flight::textlayout::get_text_metrics(out, *layout);
  else { out->height = 0; out->num_lines = 0; out->width = 0; }
}
inline void set_text_label_guard(std::optional<std::function<void(flight::String, flight::String)>> guard) {
  text_label_guard = std::move(guard);
}
} // namespace flight::text
