// Derived from @flighthq/bitmapfont/packages/bitmapfont/src/bitmapFontFromGlyphAtlas.ts
// at generated digest e51c8647e39e4b1d9a17cb535ef6a1e3289e3cc294a6226ca65070a79c715bc2.
#pragma once
#include <flight/structural_ref.hpp>
#include <optional>
#include <tuple>
#include <variant>
#include <flight/runtime.hpp>

static_assert(flight::runtime_contract.compiler_contract == "flight-runtime-contract/2", "Flight compiler/runtime contract mismatch");
static_assert(flight::runtime_contract.cpp_abi == 1, "Flight C++ runtime ABI mismatch");

#include <flight/textureatlas/texture_atlas_grid.hpp>

namespace flight::types { struct Bitmap; }
namespace flight::types { struct BitmapFont; }
namespace flight::types { struct BitmapFontData; }
namespace flight::types { struct BitmapFontGlyphData; }
namespace flight::types { struct BitmapFontKerningData; }
namespace flight::types { struct GlyphAtlas; }
namespace flight::types { struct GlyphAtlasRuntime; }
namespace flight::types { struct GlyphAtlasShelf; }
namespace flight::types { struct GlyphEntry; }
namespace flight::types { struct GlyphMetrics; }
namespace flight::types { struct GlyphRasterizeOptions; }
namespace flight::types { struct GlyphRasterizedBitmap; }
namespace flight::types { struct HostGlyphRasterizerCapability; }
namespace flight::types { struct Sampler; }
namespace flight::types { struct Texture2D; }
namespace flight::types { struct TextureAtlas; }
namespace flight::types { struct TextureAtlasRegion; }
namespace flight::types { struct TextureResourceOptions; }
namespace flight::types { struct Vector2; }

#include <flight/texture/texture.hpp>
#include <flight/textureatlas/texture_atlas.hpp>
#include <flight/types/bitmap.hpp>
#include <flight/types/bitmap_font.hpp>
#include <flight/types/create_texture_options.hpp>
#include <flight/types/glyph_source.hpp>
#include <flight/types/sampler.hpp>
#include <flight/types/texture.hpp>
#include <flight/types/texture_atlas.hpp>
#include <flight/types/texture_atlas_region.hpp>
#include <flight/types/texture_source_value.hpp>
#include <flight/types/vector2.hpp>
#include <flight/bitmapfont/bitmap_font.hpp>

namespace flight::bitmapfont {

using flight::types::BitmapFontData;
using flight::types::BitmapFontGlyphData;
using flight::types::GlyphAtlas;
using flight::types::GlyphAtlasRuntime;
using flight::types::GlyphMetrics;
using flight::types::TextureAtlas;

using flight::types::BitmapFont;

#ifndef FLIGHT_COMPILER_ANONYMOUS__FLIGHTHQ_BITMAPFONT_ENTITY_RUNTIME_KEY_IMAGE_HEIGHT_IMAGE_NAME_IMAGE_WIDTH_REGIONS_SCALE_TEXTURE_043BB97832A671F4
#define FLIGHT_COMPILER_ANONYMOUS__FLIGHTHQ_BITMAPFONT_ENTITY_RUNTIME_KEY_IMAGE_HEIGHT_IMAGE_NAME_IMAGE_WIDTH_REGIONS_SCALE_TEXTURE_043BB97832A671F4
using entity_runtime_key_image_height_image_name_image_width_regions_scale_texture_043bb97832a671f4 = flight::textureatlas::entity_runtime_key_image_height_image_name_image_width_regions_scale_texture_043bb97832a671f4;
#endif // FLIGHT_COMPILER_ANONYMOUS__FLIGHTHQ_BITMAPFONT_ENTITY_RUNTIME_KEY_IMAGE_HEIGHT_IMAGE_NAME_IMAGE_WIDTH_REGIONS_SCALE_TEXTURE_043BB97832A671F4

// OVERRIDE: the emitter wrote create_texture({.source = runtime->bitmap}). Every create_texture
// overload takes an owned options reference or a variant of them, so a bare designated initializer
// materializes no owner and the call matches nothing. This is the lowering
// docs/compiler-requests/contextual-object-literal-owner-materialization.md asks for: the 2D options
// owner is materialized explicitly around the emitted argument, and create_texture's 2D overload
// returns the Ref<Texture2D> the emitted atlas arm needs.
inline flight::Ref<flight::types::BitmapFont> create_bitmap_font_from_glyph_atlas(flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::GlyphAtlas>>>> atlas) {
  auto runtime = flight::row_get<flight::RowKey<"runtime">>(atlas);
  flight::Array<flight::Ref<flight::types::BitmapFontGlyphData>> glyphs = flight::Array<flight::Ref<flight::types::BitmapFontGlyphData>>{};
  for (auto array_pattern_value : runtime->entries) {
    const double codepoint = std::get<0>(array_pattern_value);
    auto entry = std::get<1>(array_pattern_value);
    glyphs.push(flight::make_ref<flight::types::BitmapFontGlyphData>(flight::types::BitmapFontGlyphData{.advance = entry->advance, .bearing_x = entry->bearing_x, .bearing_y = entry->bearing_y, .codepoint = codepoint, .height = entry->height, .page = std::optional<double>{0.0}, .width = entry->width, .x = entry->x, .y = entry->y}));
  }
  auto page = flight::textureatlas::create_texture_atlas(flight::make_ref<entity_runtime_key_image_height_image_name_image_width_regions_scale_texture_043bb97832a671f4>(entity_runtime_key_image_height_image_name_image_width_regions_scale_texture_043bb97832a671f4{.texture = std::variant<flight::Ref<flight::types::Texture2D>, flight::Null, flight::Undefined>{std::in_place_type<flight::Ref<flight::types::Texture2D>>, flight::texture::create_texture(flight::make_ref<flight::texture::CreateTexture2DVariantOptions>(flight::texture::CreateTexture2DVariantOptions{.source = runtime->bitmap}))}}));
  return flight::bitmapfont::create_bitmap_font(flight::structural_ref_cast<flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::BitmapFontData>>>>>(flight::make_structural_ref<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::BitmapFontData>>>>(flight::row_field<flight::RowKey<"glyphs">>(glyphs), flight::row_field<flight::RowKey<"metrics">>(flight::make_ref<flight::types::GlyphMetrics>(*runtime->metrics)), flight::row_field<flight::RowKey<"pages">>(flight::Array<flight::Ref<flight::types::TextureAtlas>>{page}))));
}

} // namespace flight::bitmapfont
