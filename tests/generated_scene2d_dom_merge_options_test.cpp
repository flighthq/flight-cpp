// Focused native behavior for @flighthq/scene2d-dom mergeDomRenderOptions at Flight a627849, compiled
// through the scene2d-dom-merge-options-empty-map-fallback source patch, which respells each
// `merged.X ?? []` spread fallback as an empty Map of the field's declared types.
// These fixtures pin the source contract: kind-keyed maps merge with later fragments winning per key
// and first-insertion order kept, absent maps stay absent, a present empty map is kept, scalars are
// last-wins and an absent field never overwrites, values keep their identity, inputs are not mutated,
// and every merged map is a fresh map rather than an alias of an input.
#include <flight/scene2d_dom/merge_dom_render_options.hpp>

#include <iostream>
#include <string>
#include <vector>

namespace {

using flight::RowKey;
namespace types = flight::types;

using RendererMap = flight::Map<flight::String, flight::Ref<types::NodeRenderer>>;
using Fragment = flight::StructuralRef<flight::RowReadonly<flight::RowPartial<flight::RowOf<flight::Ref<types::DomRenderOptions>>>>>;

int failures = 0;

void check(bool condition, const std::string& message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

std::vector<std::string> keys_of(const RendererMap& map) {
  std::vector<std::string> out;
  for (const auto& key : map.keys()) out.push_back(key.to_utf8());
  return out;
}

flight::Ref<types::NodeRenderer> renderer() { return flight::make_ref<types::NodeRenderer>(); }

}  // namespace

int main() {
  const auto a = renderer();
  const auto b = renderer();
  const auto c = renderer();

  RendererMap first;
  first.set(flight::String("n1"), a);
  first.set(flight::String("n2"), b);
  RendererMap second;
  second.set(flight::String("n2"), c);
  second.set(flight::String("n3"), a);
  const flight::Map<flight::String, types::DomTextureResolver> empty_resolvers;

  const Fragment f1 = flight::make_structural_ref<flight::RowReadonly<flight::RowPartial<flight::RowOf<flight::Ref<types::DomRenderOptions>>>>>(
      flight::row_field<RowKey<"nodeRenderers">>(first), flight::row_field<RowKey<"pixelRatio">>(2.0));
  const Fragment f2 = flight::make_structural_ref<flight::RowReadonly<flight::RowPartial<flight::RowOf<flight::Ref<types::DomRenderOptions>>>>>(
      flight::row_field<RowKey<"nodeRenderers">>(second), flight::row_field<RowKey<"textureResolvers">>(empty_resolvers));

  // No fragments: nothing is present.
  {
    const auto merged = flight::scene2d_dom::merge_dom_render_options(flight::Array<Fragment>{});
    check(!merged->node_renderers.has_value() && !merged->texture_resolvers.has_value() &&
              !merged->canvas_shape_commands.has_value() && !merged->pixel_ratio.has_value(),
          "an empty merge must leave every field absent");
  }

  // One fragment: its map is copied into a fresh map with the same entries and values.
  {
    const auto merged = flight::scene2d_dom::merge_dom_render_options(flight::Array<Fragment>{f1});
    check(merged->node_renderers.has_value(), "a present map must be merged");
    if (!merged->node_renderers.has_value()) return 1;
    const auto& map = merged->node_renderers.value();
    check(keys_of(map) == std::vector<std::string>{"n1", "n2"}, "single-fragment keys or order changed");
    check(map.get(flight::String("n1")).value() == a && map.get(flight::String("n2")).value() == b,
          "renderer value identity was not preserved");
    check(merged->pixel_ratio == 2.0, "a scalar must be carried");
    check(!merged->texture_resolvers.has_value() && !merged->effect_padding_resolvers.has_value(),
          "an absent map must stay absent");
    map.set(flight::String("probe"), c);
    check(!first.has(flight::String("probe")) && first.size() == 2,
          "the merged map must be fresh, not an alias of the input map");
  }

  // Two fragments: later wins per key, first-insertion order is kept, a present empty map is kept,
  // and an absent scalar in a later fragment does not overwrite.
  {
    const auto merged = flight::scene2d_dom::merge_dom_render_options(flight::Array<Fragment>{f1, f2});
    check(merged->node_renderers.has_value(), "a two-fragment merge must keep the map");
    if (!merged->node_renderers.has_value()) return 1;
    const auto& map = merged->node_renderers.value();
    check(keys_of(map) == std::vector<std::string>{"n1", "n2", "n3"}, "merged keys or insertion order changed");
    check(map.get(flight::String("n1")).value() == a && map.get(flight::String("n2")).value() == c &&
              map.get(flight::String("n3")).value() == a,
          "the later fragment must win per key while keeping value identity");
    check(merged->texture_resolvers.has_value() && merged->texture_resolvers.value().size() == 0,
          "a present empty map must be merged as present and empty");
    check(merged->pixel_ratio == 2.0, "an absent scalar in a later fragment must not overwrite");
    check(keys_of(first) == std::vector<std::string>{"n1", "n2"} && first.get(flight::String("n2")).value() == b &&
              keys_of(second) == std::vector<std::string>{"n2", "n3"} && second.get(flight::String("n2")).value() == c,
          "input maps must not be mutated");
  }

  // Reversed order: precedence follows fragment order.
  {
    const auto merged = flight::scene2d_dom::merge_dom_render_options(flight::Array<Fragment>{f2, f1});
    check(merged->node_renderers.has_value(), "a reversed merge must keep the map");
    if (!merged->node_renderers.has_value()) return 1;
    const auto& map = merged->node_renderers.value();
    check(keys_of(map) == std::vector<std::string>{"n2", "n3", "n1"}, "reversed merge keys or order changed");
    check(map.get(flight::String("n2")).value() == b && map.get(flight::String("n3")).value() == a,
          "reversed precedence must let the later fragment win");
  }

  if (failures == 0) std::cout << "scene2d-dom mergeDomRenderOptions: all checks passed\n";
  return failures == 0 ? 0 : 1;
}
