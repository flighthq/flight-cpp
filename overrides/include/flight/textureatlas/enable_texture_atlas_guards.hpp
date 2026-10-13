// Faithful native subset of enableTextureAtlasGuards.ts at Flight a627849.
#pragma once

#include <flight/runtime.hpp>
#include <flight/textureatlas/texture_atlas_region.hpp>

namespace flight::textureatlas {

// The built-in warning callback is intentionally absent. Its Record<string, unknown> payload must
// retain the original Readonly<TextureAtlas> structural owner, for which Any has no carrier yet.
inline bool texture_atlas_guards_enabled = false;

inline bool are_texture_atlas_guards_enabled() {
  return texture_atlas_guards_enabled;
}

inline void disable_texture_atlas_guards() {
  set_texture_atlas_region_texture_guard(std::nullopt);
  texture_atlas_guards_enabled = false;
}

} // namespace flight::textureatlas
