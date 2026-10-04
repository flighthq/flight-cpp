// Derived from @flighthq/glyphatlas/packages/glyphatlas/src/explainGlyphAtlasEntry.ts
// at generated digest 56308cad42908447cd9d7d9bde8b913c5ac4e64c8f9332702cb978a882bb50a7.
#pragma once

#include <flight/runtime.hpp>
#include <flight/types/glyph_atlas_entry_explanation.hpp>
#include <flight/types/glyph_source.hpp>

static_assert(flight::runtime_contract.compiler_contract == "flight-runtime-contract/2", "Flight compiler/runtime contract mismatch");
static_assert(flight::runtime_contract.cpp_abi == 1, "Flight C++ runtime ABI mismatch");

namespace flight::glyphatlas {

// OVERRIDE: Map::has proves the following Map::get is present in TypeScript, but the emitter does not
// carry that proof to the entry member reads. Bind and test the optional result directly in C++.
inline flight::Ref<flight::types::GlyphAtlasEntryExplanation> explain_glyph_atlas_entry(
    flight::Ref<flight::types::GlyphAtlas> atlas,
    double codepoint) {
  const auto& runtime = atlas->runtime;
  const double padding = runtime->padding;
  const double usable_width = runtime->bitmap->width - 2.0 * padding;
  const double usable_height = runtime->bitmap->height - 2.0 * padding;

  const auto entry = runtime->entries.get(codepoint);
  if (entry.has_value()) {
    return flight::make_ref<flight::types::GlyphAtlasEntryExplanation>(
        flight::types::GlyphAtlasEntryExplanation{
            .renderable = true,
            .reason = flight::String("ok"),
            .glyph_width = entry.value()->width,
            .glyph_height = entry.value()->height,
            .usable_width = usable_width,
            .usable_height = usable_height,
        });
  }

  const auto bitmap = runtime->rasterizer_backend->rasterize(codepoint, runtime->rasterize_options);
  if (!bitmap.has_value()) {
    return flight::make_ref<flight::types::GlyphAtlasEntryExplanation>(
        flight::types::GlyphAtlasEntryExplanation{
            .renderable = false,
            .reason = flight::String("rasterizer-returned-null"),
            .glyph_width = 0.0,
            .glyph_height = 0.0,
            .usable_width = usable_width,
            .usable_height = usable_height,
        });
  }

  const bool fits = bitmap.value()->width <= usable_width && bitmap.value()->height <= usable_height;
  return flight::make_ref<flight::types::GlyphAtlasEntryExplanation>(
      flight::types::GlyphAtlasEntryExplanation{
          .renderable = fits,
          .reason = fits ? flight::String("ok") : flight::String("glyph-larger-than-atlas"),
          .glyph_width = bitmap.value()->width,
          .glyph_height = bitmap.value()->height,
          .usable_width = usable_width,
          .usable_height = usable_height,
      });
}

} // namespace flight::glyphatlas
