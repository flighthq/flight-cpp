// One source runtime owner for RichText. The source factory passes createRichTextRuntime to
// createNode2D, which stores that one runtime on the node's EntityRuntimeKey; getRichTextRuntime reads
// it back. The generated type flattens that runtime heritage, so the override gives RichTextRuntime its
// declared NodeRuntime<Node2DTraits> base and the node its exact runtime slot. This fixture pins the
// ownership, identity, state, alias/drop lifetime and error boundary on the native surface.
#include <flight/text/rich_text.hpp>
#include <flight/types/rich_text_content.hpp>

#include <cstdio>
#include <memory>
#include <string>

namespace {

int failures = 0;

void check(bool condition, const char* message) {
  if (!condition) {
    std::printf("FAIL: %s\n", message);
    ++failures;
  }
}

using Runtime = flight::types::RichTextRuntime;
using RuntimeView = flight::text::RichTextRuntimeView;

flight::Ref<Runtime> runtime_of(const RuntimeView& view) {
  return flight::structural_ref_cast<flight::Ref<Runtime>>(view);
}

}  // namespace

int main() {
  auto node = flight::text::create_rich_text();
  check(node != nullptr, "the factory returns a node");
  check(node->kind == flight::types::rich_text_kind, "the node kind is RichText");
  check(node->data != nullptr, "the node owns its data");

  // The first public read recovers the runtime the factory allocated; the second recovers the same one.
  auto first_view = flight::text::get_rich_text_runtime(node);
  auto first = runtime_of(first_view);
  auto second = runtime_of(flight::text::get_rich_text_runtime(node));
  check(first != nullptr, "a fresh node retains a runtime");
  check(first == second, "repeated reads return the same runtime object");

  // The retained runtime carries the source's initial state and the Node2D runtime base.
  check(first->selection_begin_index == 0.0 && first->selection_end_index == 0.0, "selection starts at zero");
  check(!first->input.has_value(), "the input slot starts absent");
  check(!first->rich_text_content.has_value(), "the content slot starts absent");
  check(!first->text_field_signals.has_value(), "the signals slot starts absent");
  check(!first->text_layout.has_value() && first->text_layout_using_content_id == -1.0,
        "the layout slot starts absent");
  check(first->traits.has_value() && first->traits.value() == flight::types::node_2d_traits_key,
        "the retained runtime carries the Node2D runtime base");
  check(!first->parent.has_value() && !first->children.has_value(), "a fresh runtime has no graph cells");

  // A second reference to the node is an alias, not a rebuilt node: it recovers the same runtime.
  flight::Ref<flight::types::RichText> alias = node;
  auto via_alias = runtime_of(flight::text::get_rich_text_runtime(alias));
  check(via_alias == first, "an alias recovers the same runtime");
  {
    auto structural = flight::structural_ref_cast<
        flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::RichText>>>>>(node);
    auto via_structural = runtime_of(flight::text::get_rich_text_runtime(structural));
    check(via_structural == first, "the structural overload recovers the same runtime");
  }

  // A source content update writes the one retained runtime, and keeps the caller's content owner.
  auto content = flight::make_ref<flight::types::RichTextContent>(flight::types::RichTextContent{
      .format_ranges = {},
      .text = flight::String("rich"),
  });
  flight::text::set_rich_text_content(node, content);
  check(first->rich_text_content.has_value() && first->rich_text_content.value() == content,
        "content is retained on the one runtime");
  check(flight::text::get_rich_text_string(node) == flight::String("rich"), "the data text follows the content");

  // The node owns the runtime: it stays alive while any node reference exists and is released with the
  // last one. No global table or cycle keeps it alive.
  std::weak_ptr<Runtime> weak_runtime = first;
  auto owner = node;
  first_view = RuntimeView();
  first = nullptr;
  second = nullptr;
  via_alias = nullptr;
  check(!weak_runtime.expired(), "the runtime stays alive while the node is alive");
  node = nullptr;
  alias = nullptr;
  check(!weak_runtime.expired(), "another node reference keeps the same runtime alive");
  owner = nullptr;
  check(weak_runtime.expired(), "the runtime is released with the last node reference");

  // A node with no runtime in its slot reports the source error instead of manufacturing one.
  auto bare = flight::make_ref<flight::types::RichText>(flight::types::RichText{
      .data = flight::text::create_rich_text_data(),
      .enabled = true,
      .kind = flight::types::rich_text_kind,
      .name = std::nullopt,
      .entity_runtime_key = std::nullopt,
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
  bool threw = false;
  try {
    (void)flight::text::get_rich_text_runtime(bare);
  } catch (const flight::Error& error) {
    threw = std::string(error.what()).find("RichText runtime is unavailable") != std::string::npos;
  }
  check(threw, "an absent runtime reports the source error");
  check(!bare->entity_runtime_key.has_value(), "the error path does not manufacture a runtime");

  if (failures != 0) {
    std::printf("%d failure(s)\n", failures);
    return 1;
  }
  std::printf("rich text runtime ownership passed\n");
  return 0;
}
