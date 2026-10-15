// Focused native parity for @flighthq/bitmapfont#createBitmapFontFromGlyphAtlas at Flight a627849.
// Pins the conversion's observable shape: the glyph table it copies out of the atlas entries, the
// single region-less page it builds over the atlas bitmap, the atlas defaults a texture-only call
// produces, the by-value `{...metrics}` spread, and the reference identity the "snapshot" claim rests
// on -- the font holds the atlas bitmap itself, not a copy of its pixels.
#include <flight/bitmap/bitmap.hpp>
#include <flight/bitmapfont/bitmap_font_from_glyph_atlas.hpp>

#include <cstdint>
#include <iostream>
#include <memory>
#include <string>

namespace {

namespace bitmap = flight::bitmap;
namespace bitmapfont = flight::bitmapfont;

int failures = 0;

void check(bool condition, const std::string& message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

template <typename Type>
auto readonly(const std::shared_ptr<Type>& value) {
  using View = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<Type>>>>;
  return View(value);
}

flight::Ref<flight::types::GlyphEntry> glyph_entry(
    double advance, double bearing_x, double bearing_y, double height, double page, double width,
    double x, double y) {
  auto value = flight::make_ref<flight::types::GlyphEntry>();
  value->advance = advance;
  value->bearing_x = bearing_x;
  value->bearing_y = bearing_y;
  value->height = height;
  value->page = page;
  value->width = width;
  value->x = x;
  value->y = y;
  return value;
}

// The atlas runtime is generated with no default member initializers, so it is value-initialized
// first and only the fields the conversion reads are assigned.
flight::Ref<flight::types::GlyphAtlasRuntime> glyph_atlas_runtime(
    const flight::Ref<flight::types::Bitmap>& source_bitmap) {
  auto value = flight::make_ref<flight::types::GlyphAtlasRuntime>();
  value->bitmap = source_bitmap;
  value->entries.set(65.0, glyph_entry(1.5, 2.0, 3.0, 4.0, 0.0, 5.0, 6.0, 7.0));
  value->entries.set(66.0, glyph_entry(11.5, 12.0, 13.0, 14.0, 0.0, 15.0, 16.0, 17.0));
  value->metrics = flight::make_ref<flight::types::GlyphMetrics>();
  value->metrics->ascent = 0.75;
  value->metrics->descent = 0.25;
  value->metrics->line_gap = 0.125;
  return value;
}

flight::Ref<flight::types::GlyphAtlas> glyph_atlas(
    const flight::Ref<flight::types::GlyphAtlasRuntime>& runtime) {
  auto value = flight::make_ref<flight::types::GlyphAtlas>();
  value->entity_runtime_key = std::nullopt;
  value->runtime = runtime;
  return value;
}

// The page's texture source as a raw pointer, read through the variant rather than dereferenced, so
// the assertion is about which object the page names and not about what either object contains.
const flight::types::Bitmap* page_source_bitmap(
    const flight::Ref<flight::types::TextureAtlas>& page) {
  if (!page->texture.has_value()) return nullptr;
  const auto texture = page->texture.value();
  if (!texture->source.has_value()) return nullptr;
  const auto* source = std::get_if<flight::Ref<flight::types::Bitmap>>(&texture->source.value());
  if (source == nullptr) return nullptr;
  return source->get();
}

void glyphs_are_copied_from_the_atlas_entries() {
  const auto source_bitmap = bitmap::create_bitmap(4.0, 4.0, 0xff0000ffu);
  const auto runtime = glyph_atlas_runtime(source_bitmap);
  const auto font =
      bitmapfont::create_bitmap_font_from_glyph_atlas(readonly(glyph_atlas(runtime)));

  check(font->glyphs.size() == 2, "the font glyph table carries one glyph per atlas entry");

  const auto first = font->glyphs.get(65.0);
  check(first.has_value(), "codepoint 65 is a key in the font glyph table");
  if (first.has_value()) {
    const auto& glyph = first.value();
    check(glyph->advance == 1.5 && glyph->bearing_x == 2.0 && glyph->bearing_y == 3.0 &&
              glyph->height == 4.0 && glyph->width == 5.0 && glyph->x == 6.0 && glyph->y == 7.0,
          "codepoint 65 copies advance, bearings, height, width, x and y verbatim");
    check(glyph->page == 0.0, "a glyph on the single growing bitmap resolves to page 0");
  }

  const auto second = font->glyphs.get(66.0);
  check(second.has_value(), "codepoint 66 is a key in the font glyph table");
  if (second.has_value()) {
    const auto& glyph = second.value();
    check(glyph->advance == 11.5 && glyph->bearing_x == 12.0 && glyph->bearing_y == 13.0 &&
              glyph->height == 14.0 && glyph->width == 15.0 && glyph->x == 16.0 &&
              glyph->y == 17.0 && glyph->page == 0.0,
          "the second glyph keeps its own field values rather than the first's");
  }

  check(!font->glyphs.get(67.0).has_value(), "a codepoint absent from the atlas is absent here");
}

void the_page_is_single_and_regionless() {
  const auto source_bitmap = bitmap::create_bitmap(4.0, 4.0, 0xff0000ffu);
  const auto runtime = glyph_atlas_runtime(source_bitmap);
  const auto font =
      bitmapfont::create_bitmap_font_from_glyph_atlas(readonly(glyph_atlas(runtime)));

  check(font->pages.size() == 1, "the conversion emits exactly one page");

  const auto page = font->pages.get(0.0);
  check(page.has_value(), "page 0 is present");
  if (page.has_value()) {
    // Regions stay empty by design: a bitmap font addresses glyphs through its glyph table, so the
    // page carries the pixels and names no sub-rectangles.
    check(page.value()->regions.size() == 0,
          "the single page has no regions, because the glyph table carries the layout");
  }
}

void the_page_carries_the_atlas_defaults_for_a_texture_only_call() {
  const auto source_bitmap = bitmap::create_bitmap(4.0, 4.0, 0xff0000ffu);
  const auto runtime = glyph_atlas_runtime(source_bitmap);
  const auto font =
      bitmapfont::create_bitmap_font_from_glyph_atlas(readonly(glyph_atlas(runtime)));

  const auto page = font->pages.get(0.0);
  check(page.has_value(), "page 0 is present");
  if (!page.has_value()) return;

  check(page.value()->image_height == 0.0 && page.value()->image_width == 0.0,
        "the atlas carries the zero image extent the options omitted");
  check(page.value()->scale == 1.0, "the atlas scale defaults to 1");
  check(page.value()->texture.has_value(), "the atlas texture is engaged for a texture-only call");
  if (!page.value()->texture.has_value()) return;
  check(page.value()->texture.value()->dimension == flight::String("2d"),
        "the page texture is the 2d texture createTexture builds");
}

void metrics_are_copied_by_value() {
  const auto source_bitmap = bitmap::create_bitmap(4.0, 4.0, 0xff0000ffu);
  const auto runtime = glyph_atlas_runtime(source_bitmap);
  const auto font =
      bitmapfont::create_bitmap_font_from_glyph_atlas(readonly(glyph_atlas(runtime)));

  check(static_cast<const void*>(font->metrics.get()) !=
            static_cast<const void*>(runtime->metrics.get()),
        "the font owns a metrics object distinct from the atlas runtime's, the {...metrics} spread");
  check(font->metrics->ascent == runtime->metrics->ascent &&
            font->metrics->descent == runtime->metrics->descent &&
            font->metrics->line_gap == runtime->metrics->line_gap,
        "the copied metrics carry the atlas runtime's ascent, descent and line gap");

  // The copy is a spread, not a live link: mutating the atlas metrics afterwards leaves the font's.
  runtime->metrics->ascent = 99.0;
  check(font->metrics->ascent == 0.75,
        "writing the atlas runtime metrics does not reach the font's copied metrics");
}

void the_page_texture_source_is_the_very_atlas_bitmap() {
  const auto source_bitmap = bitmap::create_bitmap(4.0, 4.0, 0xff0000ffu);
  const auto runtime = glyph_atlas_runtime(source_bitmap);
  const auto font =
      bitmapfont::create_bitmap_font_from_glyph_atlas(readonly(glyph_atlas(runtime)));

  const auto page = font->pages.get(0.0);
  check(page.has_value(), "page 0 is present");
  if (!page.has_value()) return;

  const flight::types::Bitmap* const before = page_source_bitmap(page.value());
  check(before != nullptr, "the page texture source is a bitmap, not another source kind");
  check(before == source_bitmap.get(),
        "the page's texture source is the same Bitmap object the atlas runtime holds");

  // Snapshot, read honestly: the font holds its OWN REFERENCE to the atlas bitmap. Rasterizing more
  // glyphs into the source atlas repacks the atlas, but it does not re-point this page -- the page
  // keeps naming the same Bitmap object. Because the reference is shared, pixels written into that
  // bitmap are visible through the page; what the snapshot freezes is the page and glyph table, not
  // a private copy of the bytes.
  const void* const storage_before = source_bitmap->data.identity();
  source_bitmap->data.set_index(0.0, 0x7fu);
  source_bitmap->width = 8.0;

  const flight::types::Bitmap* const after = page_source_bitmap(page.value());
  check(after == before, "mutating the atlas bitmap does not re-point the page's texture source");
  check(after == source_bitmap.get(), "the page still names the one shared atlas bitmap");
  check(after->data.identity() == storage_before && after->data.get_index(0.0) == 0x7fu,
        "the shared reference means the atlas bitmap stays the object carrying the pixels");
  check(page.value()->image_width == 0.0,
        "the page's own image extent is independent of the source bitmap's dimensions");
}

void encoding_defaults_to_raster() {
  const auto source_bitmap = bitmap::create_bitmap(4.0, 4.0, 0xff0000ffu);
  const auto runtime = glyph_atlas_runtime(source_bitmap);
  const auto font =
      bitmapfont::create_bitmap_font_from_glyph_atlas(readonly(glyph_atlas(runtime)));

  check(font->encoding == flight::String("raster"),
        "the conversion omits encoding, so the font defaults to the raster encoding");
}

}  // namespace

int main() {
  glyphs_are_copied_from_the_atlas_entries();
  the_page_is_single_and_regionless();
  the_page_carries_the_atlas_defaults_for_a_texture_only_call();
  metrics_are_copied_by_value();
  the_page_texture_source_is_the_very_atlas_bitmap();
  encoding_defaults_to_raster();
  return failures == 0 ? 0 : 1;
}
