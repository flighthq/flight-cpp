// Host font discovery and TrueType loading only. Glyph coverage is supplied by
// the SDK GlyphOutlineSource capability; text layout and drawing stay in Scene2D GL.
#include "font_host.hpp"
#include <flight/font/glyph_outline_source.hpp>
#include <flight/font_formats/open_type_cmap.hpp>
#include <flight/font_formats/open_type_glyf.hpp>
#include <flight/font_formats/open_type_metrics.hpp>
#include <flight/font_formats/sfnt_table_directory.hpp>
#include <flight/scene2d_gl/gl_text_label.hpp>
#include <flight/render/renderer.hpp>
#include <flight/render/render_proxy.hpp>
#include <fstream>
#include <cstdlib>
#include <vector>
namespace flight::examples {
namespace {
template <typename Value>
[[nodiscard]] auto readonly_row(const std::shared_ptr<Value>& value) {
  return flight::structural_ref_cast<
      flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<Value>>>>>(
      flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<Value>>>>(value));
}

struct LoadedFont final {
  flight::Ref<flight::types::GlyphOutlineSource> source;
  flight::Ref<flight::types::GlyphOutlineMetrics> metrics;
};

[[nodiscard]] std::optional<flight::Uint32Array> read_glyph_ranges(
    const flight::Uint8Array& bytes,
    const flight::Ref<flight::types::SfntTableDirectory>& directory,
    double glyph_count,
    double loca_format) {
  const auto loca = directory->tables.get(flight::String("loca"));
  if (!loca.has_value()) return std::nullopt;
  const double entry_count = glyph_count + 1.0;
  const double entry_bytes = loca_format == 0.0 ? 2.0 : 4.0;
  const double length = flight::row_get<flight::RowKey<"length">>(loca.value());
  const double offset = flight::row_get<flight::RowKey<"offset">>(loca.value());
  if (entry_count * entry_bytes > length) return std::nullopt;
  flight::DataView view(
      bytes.buffer,
      static_cast<double>(bytes.byte_offset),
      static_cast<double>(bytes.byte_length));
  flight::Uint32Array ranges(entry_count);
  for (double index = 0.0; index < entry_count; index += 1.0) {
    const double value = loca_format == 0.0
                             ? view.get_uint16(offset + index * 2.0) * 2.0
                             : view.get_uint32(offset + index * 4.0);
    ranges.set_index(index, value);
  }
  return ranges;
}

[[nodiscard]] std::optional<LoadedFont> load_true_type_font(std::string_view path) {
  std::ifstream stream(std::string(path), std::ios::binary | std::ios::ate);
  if (!stream) return std::nullopt;
  const auto end = stream.tellg();
  if (end <= 0) return std::nullopt;
  std::vector<std::uint8_t> storage(static_cast<std::size_t>(end));
  stream.seekg(0);
  if (!stream.read(reinterpret_cast<char*>(storage.data()), end)) return std::nullopt;

  const flight::Uint8Array bytes(storage);
  const auto directory = flight::font_formats::read_sfnt_table_directory(bytes);
  if (!directory.has_value()) return std::nullopt;
  const auto directory_row = readonly_row(directory.value());
  const double glyph_count = flight::font_formats::read_open_type_glyph_count(
      bytes, directory_row);
  const double loca_format = flight::font_formats::read_open_type_loca_format(
      bytes, directory_row);
  const auto metrics = flight::font_formats::read_open_type_metrics(bytes, directory_row);
  const auto advances = flight::font_formats::read_open_type_advances(
      bytes, directory_row, glyph_count);
  const auto codepoints = flight::font_formats::read_open_type_codepoint_map(
      bytes, directory_row);
  const auto ranges = read_glyph_ranges(
      bytes, directory.value(), glyph_count, loca_format);
  if (glyph_count <= 0.0 || loca_format < 0.0 || !metrics.has_value() ||
      !advances.has_value() || !codepoints.has_value() || !ranges.has_value()) {
    return std::nullopt;
  }

  auto source = flight::make_ref<flight::types::GlyphOutlineSource>(
      flight::types::GlyphOutlineSource{});
  source->get_glyph_outline =
      [bytes, directory = directory.value(), ranges = ranges.value(), glyph_count](
          flight::Ref<flight::types::Path> path_out,
          double glyph_index) {
        if (glyph_index < 0.0 || glyph_index >= glyph_count) return false;
        return flight::font_formats::read_open_type_glyph_outline(
            path_out, bytes, readonly_row(directory), ranges, glyph_index);
      };
  source->get_glyph_outline_advance =
      [advances = advances.value(), glyph_count](double glyph_index) {
        return glyph_index < 0.0 || glyph_index >= glyph_count
                   ? 0.0
                   : advances.get_index(glyph_index);
      };
  source->get_glyph_outline_index_for_code_point =
      [codepoints = codepoints.value()](double codepoint) {
        return codepoints.get(codepoint).value_or(-1.0);
      };
  source->get_glyph_outline_metrics = [metrics = metrics.value()] {
    return readonly_row(metrics);
  };
  return LoadedFont{.source = std::move(source), .metrics = metrics.value()};
}

[[nodiscard]] std::optional<LoadedFont> load_example_font() {
  std::vector<std::string> candidates;
  if (const char* configured = std::getenv("FLIGHT_CPP_EXAMPLE_FONT")) {
    candidates.emplace_back(configured);
  }
  candidates.emplace_back("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
  candidates.emplace_back("/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf");
  candidates.emplace_back("/usr/share/fonts/truetype/freefont/FreeSans.ttf");
  candidates.emplace_back("/System/Library/Fonts/Supplemental/Arial.ttf");
  if (const char* windows = std::getenv("WINDIR")) {
    candidates.emplace_back(std::string(windows) + "/Fonts/arial.ttf");
  }
  for (const auto& candidate : candidates) {
    try {
      if (auto font = load_true_type_font(candidate)) return font;
    } catch (const std::exception&) {
      // A malformed or unsupported candidate must not prevent later system fallbacks.
    }
  }
  return std::nullopt;
}

} // namespace
void install_text_renderer(flight::Ref<flight::types::GlRenderState> state) {
  auto font = load_example_font();
  if (!font) throw std::runtime_error("TextLabel requires a TrueType font; set FLIGHT_CPP_EXAMPLE_FONT");
  auto capability = flight::font::allocate_glyph_rasterizer_backend_from_glyph_outline_source(font->source);
  auto provider = std::make_shared<flight::scene2d_gl::NativeGlyphTextProvider>(capability);
  flight::textlayout::set_text_layout_measure_provider(flight::types::TextMeasureFunction(
      [provider](auto text, auto format) { return provider->measure(text, format); }));
  flight::render::register_node_renderer(state, flight::types::text_label_kind,
      flight::scene2d_gl::create_native_gl_text_label_renderer(provider));
}
void verify_text_labels(const Scene2DRenderFrame& frame,
    std::span<const flight::Ref<flight::types::TextLabel>> labels) {
  for (auto label : labels) {
    const auto node = flight::node::native_node_2d(label);
    auto proxy = flight::render::get_render_proxy_2d(frame.state, node);
    if (!proxy || (*proxy)->source.shared_exact_object<flight::types::TextLabel>() != label || !(*proxy)->renderer_data)
      throw std::runtime_error("TextLabel proxy lost its original owner/data");
    auto data = flight::scene2d_gl::get_native_gl_text_label_data(*(*proxy)->renderer_data);
    auto runtime = flight::text::get_text_label_runtime(label);
    if (flight::node::native_node_2d_runtime(node) != runtime)
      throw std::runtime_error("TextLabel runtime identity differs from graph runtime");
    if (!(*proxy)->visible || label->data->text.length() == 0) continue;
    // Query through the public layout contract, which owns its distinct cache revision.
    flight::text::ensure_text_layout(label);
    if (!data->bitmap || data->last_content_id != runtime->local_content_id ||
        !runtime->text_layout || runtime->text_layout_using_content_id != runtime->local_content_id ||
        runtime->text_layout_using_text != label->data->text || data->log_w <= 0 || data->log_h <= 0)
      throw std::runtime_error("TextLabel did not rasterize the current actual layout/revision");
    auto bitmap = *data->bitmap;
    const auto t = (*proxy)->transform_2d;
    const double ratio = data->last_pixel_ratio;
    std::size_t rendered = 0;
    for (int y = 0; y < static_cast<int>(bitmap->height); ++y) for (int x = 0; x < static_cast<int>(bitmap->width); ++x) {
      auto offset = static_cast<std::size_t>((y * static_cast<int>(bitmap->width) + x) * 4);
      if (bitmap->data[offset+3] < 250) continue;
      const double lx = (x+.5)/ratio, ly = (y+.5)/ratio;
      const int px = static_cast<int>(std::floor(t->a*lx+t->c*ly+t->tx));
      const int py = static_cast<int>(std::floor(t->b*lx+t->d*ly+t->ty));
      if (px<0 || py<0 || px>=frame.width || py>=frame.height) continue;
      const auto actual = static_cast<std::size_t>(((frame.height-1-py)*frame.width+px)*4);
      bool matches = true;
      for (std::size_t c=0; c<3; ++c) if (std::abs(static_cast<int>(frame.pixels[actual+c])-static_cast<int>(bitmap->data[offset+c]))>20) matches=false;
      if (matches) ++rendered;
    }
    if (rendered == 0) throw std::runtime_error("TextLabel glyph coverage did not reach the real GL framebuffer");
  }
}
} // namespace flight::examples
