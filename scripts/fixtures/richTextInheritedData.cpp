// B-coherence and original defaults, plus explicit partial options/runtime boundaries.
#include <flight/text/rich_text.hpp>
#include <iostream>
#include <string_view>
#include <type_traits>
int main() {
  int checks = 0, failures = 0;
  auto check = [&](bool value) { ++checks; if (!value) ++failures; };
  auto data = flight::text::create_rich_text_data();
  check(data->width == 100 && data->height == 100);
  check(data->auto_size == flight::String("none") && data->text == flight::String(""));
  check(data->vertical_align == flight::String("top"));
  check(!data->entity_runtime_key.has_value());
  check(!data->background && data->background_color == 0xffffffff);
  check(!data->border && data->border_color == 0xff);
  check(!data->condense_white && data->text_format != data->default_text_format);
  check(data->max_chars == -1 && data->mouse_wheel_enabled && data->multiline);
  check(data->scroll_h == 0 && data->scroll_v == 1 && data->selectable);
  check(data->text_color == 0xff && data->text_format_ranges.empty() && !data->word_wrap);
  static_assert(std::is_same_v<decltype(data), flight::Ref<flight::types::RichTextData>>);
  if constexpr (std::is_base_of_v<flight::types::TextLabelData, flight::types::RichTextData>) {
    auto base = std::static_pointer_cast<flight::types::TextLabelData>(data);
    check(base.get() == static_cast<flight::types::TextLabelData*>(data.get()));
    check(!base.owner_before(data) && !data.owner_before(base));
    base->width = 173;
    check(data->width == 173);
    data->width = 100;
  }
  auto second = flight::text::create_rich_text_data();
  check(second != data && second->text_format != data->text_format &&
        second->default_text_format != data->default_text_format);
  data->text_format->size = 23.0;
  check(!data->default_text_format->size.has_value());
  using DataOptions = flight::RowReadonly<flight::RowPartial<flight::RowOf<flight::Ref<flight::types::RichTextData>>>>;
  auto data_options = flight::make_structural_ref<DataOptions>(flight::row_field<flight::RowKey<"width">>(321.0));
  bool data_options_refused = false, node_options_refused = false;
  try { (void)flight::text::create_rich_text_data(data_options); }
  catch (const flight::OutOfProfileError&) { data_options_refused = true; }
  using NodeOptions = flight::RowReadonly<flight::RowPartial<flight::RowOf<flight::Ref<flight::types::RichText>>>>;
  auto node_options = flight::make_structural_ref<NodeOptions>(flight::row_field<flight::RowKey<"alpha">>(0.3));
  try { (void)flight::text::create_rich_text(node_options); }
  catch (const flight::OutOfProfileError&) { node_options_refused = true; }
  check(data_options_refused && flight::row_get<flight::RowKey<"width">>(data_options) == 321.0);
  check(node_options_refused && flight::row_get<flight::RowKey<"alpha">>(node_options) == 0.3);
  check(flight::text::create_rich_text_data(std::nullopt)->width == 100);
  check(flight::text::create_rich_text_data(std::optional<flight::StructuralRef<DataOptions>>{})->width == 100);
  check(flight::text::create_rich_text_data(flight::null)->height == 100);
  check(flight::text::create_rich_text(flight::undefined)->kind == flight::types::rich_text_kind);
  auto node = flight::text::create_rich_text();
  check(node->data && node->data != data && node->kind == flight::types::rich_text_kind);
  check(node->enabled && node->visible && node->alpha == 1);
  // The preexisting weak registry has no strong runtime owner after construction.
  // Keep this as an explicit partial boundary, rather than fabricating a replacement owner.
  bool runtime_unavailable = false;
  try { (void)flight::text::get_rich_text_runtime(node); }
  catch (const flight::Error& error) {
    runtime_unavailable = std::string_view(error.what()) == "RichText runtime is unavailable";
  }
  check(runtime_unavailable);
  flight::text::set_rich_text_string(node, "one");
  check(flight::text::get_rich_text_string(node) == flight::String("one"));
  flight::text::append_rich_text_string(node, " two");
  check(flight::text::get_rich_text_string(node) == flight::String("one two"));
  std::cout << "{\"checks\":" << checks << ",\"failures\":" << failures
            << ",\"legacyRuntimeUnavailable\":" << (runtime_unavailable ? "true" : "false")
            << ",\"dataOptionsRefused\":" << (data_options_refused ? "true" : "false")
            << ",\"nodeOptionsRefused\":" << (node_options_refused ? "true" : "false")
            << ",\"scope\":\"source defaults, inherited owner and mutation components plus explicit options/runtime boundaries\"}\n";
  return failures ? 1 : 0;
}
