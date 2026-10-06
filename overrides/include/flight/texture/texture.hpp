// Override of generated/include/flight/texture/texture.hpp
// Fixes:
//   1. Ref<TextureLike> is variant<Ref<A>,...> -- generated code uses -> on the variant; fixed with std::visit
//   2. Ref<Vector2> members accessed with .x/.y instead of ->x/->y
//   3. Stub for get_first_texture_source (refused: cpp-contextual-union-missing-expression-type)
//   4. Stub for equals_texture_content (refused: cpp-union-member-access-unguarded)
//   5. Ref<CreateTextureOptions> is also a variant needing the same visit treatment
#pragma once

// PARTIAL: the C++ emitter refused declarations in this module. Everything else compiled, and each
// omission is marked NOT GENERATED below with the reason. This file is NOT complete.
//   missing: function getFirstTextureSource -- source line 23
//   missing: function equalsTextureContent -- source line 378
//   missing: function cloneTexture -- source line 36
//   missing: function initializeTexture2D -- source line 337
//   missing: function createTexture2D -- source line 174
//   missing: function createTexture -- source line 126
#include <cmath>
#include <cstdint>
#include <flight/boolean.hpp>
#include <flight/structural_ref.hpp>
#include <optional>
#include <random>
#include <variant>
#include <flight/runtime.hpp>

static_assert(flight::runtime_contract.compiler_contract == "flight-runtime-contract/2", "Flight compiler/runtime contract mismatch");
static_assert(flight::runtime_contract.cpp_abi == 1, "Flight C++ runtime ABI mismatch");

#include <flight/types/voxel_grid.hpp>

#include <flight/types/vector2.hpp>

#include <flight/types/texture_uv_transform.hpp>

#include <flight/types/texture_source_kind.hpp>

#include <flight/types/texture_source.hpp>

#include <flight/types/texture.hpp>

#include <flight/types/sampler.hpp>

#include <flight/types/matrix3.hpp>

#include <flight/types/image_resource_reference.hpp>

#include <flight/types/entity.hpp>

#include <flight/types/create_texture_options.hpp>

#include <flight/geometry/vector2.hpp>

#include <flight/geometry/matrix3.hpp>

namespace flight::texture {

using flight::types::CreateTextureOptions;
using flight::types::EmbeddedImageResourceReference;
using flight::types::EntityConstruction;
using flight::types::ExternalImageResourceReference;
using flight::types::ImageResourceReference;
using flight::types::Matrix3Like;
using flight::types::Sampler;
using flight::types::Texture;
using flight::types::Texture2D;
using flight::types::TextureColorSpace;
using flight::types::TextureLike;
using flight::types::TextureSource;
using flight::types::TextureSourceCubeFaces;
using flight::types::TextureSourceKind;
using flight::types::TextureUvTransform;
using flight::types::Vector2;
using flight::types::Vector2Like;
using flight::types::VoxelGrid;

using flight::geometry::clone_vector2;
using flight::geometry::copy_vector2;
using flight::geometry::create_vector2;
using flight::geometry::inverse_matrix3;
struct CreateTexture2DArrayOptions;
struct CreateTexture3DOptions;
struct CreateTextureCubeOptions;
} // namespace flight::texture

#include <flight/texture/sampler.hpp>

namespace flight::texture {

inline const double half_pi = (flight::pi / 2.0);


// Stub for getFirstTextureSource (refused: cpp-contextual-union-missing-expression-type).
// Visits the TextureLike variant to extract the first non-null TextureSource.
// For 3d textures (VoxelGrid source), returns nullopt since VoxelGrid is not TextureSource.
inline std::optional<flight::Ref<TextureSource>> get_first_texture_source(flight::Ref<TextureLike> texture) {
  return std::visit([](const auto& alt) -> std::optional<flight::Ref<TextureSource>> {
    using AltType = std::decay_t<decltype(*alt)>;
    if constexpr (requires(const AltType& a) { a.sources; }) {
      // 2d-array or cube: find first non-null source
      for (const auto& s : alt->sources) {
        if (s.has_value()) return s;
      }
      return std::nullopt;
    } else if constexpr (requires { { alt->source } -> std::convertible_to<std::optional<flight::Ref<TextureSource>>&>; }) {
      // 2d: TextureSource
      return alt->source;
    } else {
      // 3d: VoxelGrid source, not a TextureSource
      return std::nullopt;
    }
  }, texture);
}

inline void copy_texture(flight::Ref<TextureLike> out, flight::Ref<TextureLike> source) {
  // Use std::visit for all member accesses on the TextureLike variant
  auto get_dim = [](const auto& var) -> flight::String {
    return std::visit([](const auto& a) -> flight::String { return a->dimension; }, var);
  };

  if ((get_dim(out) != get_dim(source))) {
    throw flight::Error(flight::String("copyTexture requires matching dimensions"));
  }
  const flight::String color_space = std::visit([](const auto& a) { return a->color_space; }, source);
  const bool flip_x = std::visit([](const auto& a) { return a->flip_x; }, source);
  const bool flip_y = std::visit([](const auto& a) { return a->flip_y; }, source);
  const double uv_rotation = std::visit([](const auto& a) { return a->uv_rotation; }, source);
  const double version = flight::unsigned_right_shift(std::visit([](const auto& a) { return a->version; }, source), 0.0);
  copy_sampler(std::visit([](auto& a) -> decltype(auto) { return a->sampler; }, out), std::visit([](const auto& a) -> decltype(auto) { return a->sampler; }, source));
  copy_vector2(std::visit([](auto& a) -> decltype(auto) { return a->uv_offset; }, out), std::visit([](const auto& a) -> decltype(auto) { return a->uv_offset; }, source));
  copy_vector2(std::visit([](auto& a) -> decltype(auto) { return a->uv_scale; }, out), std::visit([](const auto& a) -> decltype(auto) { return a->uv_scale; }, source));
  std::visit([&](auto& a) { a->color_space = color_space; }, out);
  std::visit([&](auto& a) { a->flip_x = flip_x; }, out);
  std::visit([&](auto& a) { a->flip_y = flip_y; }, out);
  {
    auto switch_value_3 = get_dim(out);
    if (switch_value_3 == flight::String("2d")) {
      if ((get_dim(source) != flight::String("2d"))) {
        throw flight::Error(flight::String("copyTexture requires matching dimensions"));
      }
      // Both are 2d: copy source member
      std::visit([&](auto& o) {
        std::visit([&](const auto& s) {
          if constexpr (requires { o->source = s->source; }) {
            o->source = s->source;
          }
        }, source);
      }, out);
    }
    else if (switch_value_3 == flight::String("2d-array")) {
      if ((get_dim(source) != flight::String("2d-array"))) {
        throw flight::Error(flight::String("copyTexture requires matching dimensions"));
      }
      std::visit([&](auto& o) {
        std::visit([&](const auto& s) {
          if constexpr (requires { o->sources; s->sources; }) {
            o->sources = s->sources.slice();
          }
        }, source);
      }, out);
    }
    else if (switch_value_3 == flight::String("3d")) {
      if ((get_dim(source) != flight::String("3d"))) {
        throw flight::Error(flight::String("copyTexture requires matching dimensions"));
      }
      std::visit([&](auto& o) {
        std::visit([&](const auto& s) {
          if constexpr (requires { o->source = s->source; }) {
            o->source = s->source;
          }
        }, source);
      }, out);
    }
    else if (switch_value_3 == flight::String("cube")) {
      if ((get_dim(source) != flight::String("cube"))) {
        throw flight::Error(flight::String("copyTexture requires matching dimensions"));
      }
      std::visit([&](auto& o) {
        std::visit([&](const auto& s) {
          if constexpr (requires { o->sources; s->sources; }) {
            o->sources = static_cast<flight::Ref<TextureSourceCubeFaces>>(s->sources.slice());
          }
        }, source);
      }, out);
    }
  }
  std::visit([&](auto& a) { a->uv_rotation = uv_rotation; }, out);
  std::visit([&](auto& a) { a->version = version; }, out);
}

struct CreateTexture2DArrayOptions : public flight::ReferenceEnabled {
  std::optional<flight::Array<std::optional<flight::Ref<TextureSource>>>> sources;
  std::optional<double> version;
  std::optional<flight::String> color_space;
  std::optional<flight::Ref<Sampler>> sampler;
  std::optional<bool> flip_x;
  std::optional<bool> flip_y;
  std::optional<flight::Ref<Vector2>> uv_offset;
  std::optional<double> uv_rotation;
  std::optional<flight::Ref<Vector2>> uv_scale;
  flight::String dimension;
  std::variant<flight::Ref<EmbeddedImageResourceReference>, flight::Ref<ExternalImageResourceReference>, flight::Null, flight::Undefined> resource = std::variant<flight::Ref<EmbeddedImageResourceReference>, flight::Ref<ExternalImageResourceReference>, flight::Null, flight::Undefined>{std::in_place_type<flight::Undefined>, flight::undefined};
};

struct CreateTexture3DOptions : public flight::ReferenceEnabled {
  std::variant<flight::Ref<VoxelGrid>, flight::Null, flight::Undefined> source = std::variant<flight::Ref<VoxelGrid>, flight::Null, flight::Undefined>{std::in_place_type<flight::Undefined>, flight::undefined};
  std::optional<double> version;
  std::optional<flight::String> color_space;
  std::optional<flight::Ref<Sampler>> sampler;
  std::optional<bool> flip_x;
  std::optional<bool> flip_y;
  std::optional<flight::Ref<Vector2>> uv_offset;
  std::optional<double> uv_rotation;
  std::optional<flight::Ref<Vector2>> uv_scale;
  flight::String dimension;
  std::variant<flight::Ref<EmbeddedImageResourceReference>, flight::Ref<ExternalImageResourceReference>, flight::Null, flight::Undefined> resource = std::variant<flight::Ref<EmbeddedImageResourceReference>, flight::Ref<ExternalImageResourceReference>, flight::Null, flight::Undefined>{std::in_place_type<flight::Undefined>, flight::undefined};
};

struct CreateTextureCubeOptions : public flight::ReferenceEnabled {
  std::optional<flight::Array<std::optional<flight::Ref<TextureSource>>>> sources;
  std::optional<double> version;
  std::optional<flight::String> color_space;
  std::optional<flight::Ref<Sampler>> sampler;
  std::optional<bool> flip_x;
  std::optional<bool> flip_y;
  std::optional<flight::Ref<Vector2>> uv_offset;
  std::optional<double> uv_rotation;
  std::optional<flight::Ref<Vector2>> uv_scale;
  flight::String dimension;
  std::variant<flight::Ref<EmbeddedImageResourceReference>, flight::Ref<ExternalImageResourceReference>, flight::Null, flight::Undefined> resource = std::variant<flight::Ref<EmbeddedImageResourceReference>, flight::Ref<ExternalImageResourceReference>, flight::Null, flight::Undefined>{std::in_place_type<flight::Undefined>, flight::undefined};
};

inline double get_texture_height(flight::Ref<TextureLike> texture) {
  return ([&]() -> double { auto nullish_coalesce_left = ([&]() -> std::optional<double> { auto optional_chain_receiver = get_first_texture_source(texture); if (!optional_chain_receiver.has_value()) return std::nullopt; return optional_chain_receiver.value()->height; }()); if (nullish_coalesce_left.has_value()) return nullish_coalesce_left.value(); return -1.0; }());
}

inline std::optional<flight::Ref<TextureSource>> get_texture_source(flight::Ref<TextureLike> texture) {
  return get_first_texture_source(texture);
}

inline std::optional<flight::Ref<TextureSourceKind>> get_texture_source_kind(flight::Ref<TextureLike> texture) {
  return ([&]() -> std::optional<flight::Ref<TextureSourceKind>> { auto optional_chain_receiver = get_first_texture_source(texture); if (!optional_chain_receiver.has_value()) return std::nullopt; return optional_chain_receiver.value()->kind; }());
}

inline void get_texture_uv_matrix(flight::Ref<Matrix3Like> out, flight::Ref<TextureUvTransform> texture) {
  const double r = texture->uv_rotation;
  const double flip_scale_x = (flight::to_boolean(texture->flip_x) ? -1.0 : 1.0);
  const double flip_scale_y = (flight::to_boolean(texture->flip_y) ? -1.0 : 1.0);
  const double sx = (texture->uv_scale->x * flip_scale_x);
  const double sy = (texture->uv_scale->y * flip_scale_y);
  const double pre_offset_x = (flight::to_boolean(texture->flip_x) ? texture->uv_scale->x : 0.0);
  const double pre_offset_y = (flight::to_boolean(texture->flip_y) ? texture->uv_scale->y : 0.0);
  double cos_r;
  double sin_r;
  if ((r == 0.0)) {
    (cos_r = 1.0);
    (sin_r = 0.0);
  }
  else {
    if ((r == half_pi)) {
      (cos_r = 0.0);
      (sin_r = 1.0);
    }
    else {
      if ((r == -half_pi)) {
        (cos_r = 0.0);
        (sin_r = -1.0);
      }
      else {
        if (((r == flight::pi) || (r == -flight::pi))) {
          (cos_r = -1.0);
          (sin_r = 0.0);
        }
        else {
          (cos_r = std::cos(r));
          (sin_r = std::sin(r));
        }
      }
    }
  }
  const double tx = ((texture->uv_offset->x + (cos_r * pre_offset_x)) - (sin_r * pre_offset_y));
  const double ty = ((texture->uv_offset->y + (sin_r * pre_offset_x)) + (cos_r * pre_offset_y));
  flight::Float32Array m = out->m;
  ([&]() { auto&& typed_array = m; const auto typed_index = 0.0; const auto typed_value = (sx * cos_r); return typed_array.set_index(typed_index, typed_value); }());
  ([&]() { auto&& typed_array_2 = m; const auto typed_index_2 = 1.0; const auto typed_value_2 = (sx * sin_r); return typed_array_2.set_index(typed_index_2, typed_value_2); }());
  ([&]() { auto&& typed_array_3 = m; const auto typed_index_3 = 2.0; const auto typed_value_3 = 0.0; return typed_array_3.set_index(typed_index_3, typed_value_3); }());
  ([&]() { auto&& typed_array_4 = m; const auto typed_index_4 = 3.0; const auto typed_value_4 = (-sy * sin_r); return typed_array_4.set_index(typed_index_4, typed_value_4); }());
  ([&]() { auto&& typed_array_5 = m; const auto typed_index_5 = 4.0; const auto typed_value_5 = (sy * cos_r); return typed_array_5.set_index(typed_index_5, typed_value_5); }());
  ([&]() { auto&& typed_array_6 = m; const auto typed_index_6 = 5.0; const auto typed_value_6 = 0.0; return typed_array_6.set_index(typed_index_6, typed_value_6); }());
  ([&]() { auto&& typed_array_7 = m; const auto typed_index_7 = 6.0; const auto typed_value_7 = tx; return typed_array_7.set_index(typed_index_7, typed_value_7); }());
  ([&]() { auto&& typed_array_8 = m; const auto typed_index_8 = 7.0; const auto typed_value_8 = ty; return typed_array_8.set_index(typed_index_8, typed_value_8); }());
  ([&]() { auto&& typed_array_9 = m; const auto typed_index_9 = 8.0; const auto typed_value_9 = 1.0; return typed_array_9.set_index(typed_index_9, typed_value_9); }());
}

inline void get_texture_inverse_uv_matrix(flight::Ref<Matrix3Like> out, flight::Ref<TextureLike> texture) {
  // TextureLike is a variant, not TextureUvTransform; extract the UV fields into a temporary.
  auto uv_transform = flight::make_ref<TextureUvTransform>(TextureUvTransform{
    .flip_x = std::visit([](const auto& a) { return a->flip_x; }, texture),
    .flip_y = std::visit([](const auto& a) { return a->flip_y; }, texture),
    .uv_offset = std::visit([](const auto& a) -> flight::Ref<Vector2> { return a->uv_offset; }, texture),
    .uv_rotation = std::visit([](const auto& a) { return a->uv_rotation; }, texture),
    .uv_scale = std::visit([](const auto& a) -> flight::Ref<Vector2> { return a->uv_scale; }, texture),
  });
  get_texture_uv_matrix(out, uv_transform);
  inverse_matrix3(out, out);
}

inline void get_texture_view_size(flight::Ref<Vector2Like> out, flight::Ref<TextureLike> texture) {
  std::optional<flight::Ref<TextureSource>> source = get_first_texture_source(texture);
  if (((!source.has_value() || (source.value()->width <= 0.0)) || (source.value()->height <= 0.0))) {
    (out->x = 0.0);
    (out->y = 0.0);
    return;
  }
  const double rotation = std::visit([](const auto& a) { return a->uv_rotation; }, texture);
  if ((((rotation == 0.0) || (rotation == flight::pi)) || (rotation == -flight::pi))) {
    (out->x = std::abs((source.value()->width * std::visit([](const auto& a) { return a->uv_scale->x; }, texture))));
    (out->y = std::abs((source.value()->height * std::visit([](const auto& a) { return a->uv_scale->y; }, texture))));
    return;
  }
  if (((rotation == half_pi) || (rotation == -half_pi))) {
    (out->x = std::abs((source.value()->height * std::visit([](const auto& a) { return a->uv_scale->x; }, texture))));
    (out->y = std::abs((source.value()->width * std::visit([](const auto& a) { return a->uv_scale->y; }, texture))));
    return;
  }
  const double cos_r = std::cos(rotation);
  const double sin_r = std::sin(rotation);
  const double uv_scale_x = std::visit([](const auto& a) { return a->uv_scale->x; }, texture);
  const double uv_scale_y = std::visit([](const auto& a) { return a->uv_scale->y; }, texture);
  (out->x = std::hypot(((source.value()->width * uv_scale_x) * cos_r), ((source.value()->height * uv_scale_x) * sin_r)));
  (out->y = std::hypot(((source.value()->width * uv_scale_y) * sin_r), ((source.value()->height * uv_scale_y) * cos_r)));
}

inline double get_texture_width(flight::Ref<TextureLike> texture) {
  return ([&]() -> double { auto nullish_coalesce_left = ([&]() -> std::optional<double> { auto optional_chain_receiver = get_first_texture_source(texture); if (!optional_chain_receiver.has_value()) return std::nullopt; return optional_chain_receiver.value()->width; }()); if (nullish_coalesce_left.has_value()) return nullish_coalesce_left.value(); return -1.0; }());
}

inline bool has_texture_source(flight::Ref<TextureLike> texture) {
  return get_texture_source_kind(texture).has_value();
}

inline bool has_texture_uv_transform(flight::Ref<TextureUvTransform> texture) {
  return ((((((texture->flip_x || texture->flip_y) || (texture->uv_scale->x != 1.0)) || (texture->uv_scale->y != 1.0)) || (texture->uv_offset->x != 0.0)) || (texture->uv_offset->y != 0.0)) || (texture->uv_rotation != 0.0));
}

inline bool is_texture_ready(flight::Ref<TextureLike> texture) {
  return has_texture_source(texture);
}

inline void reset_texture_uv_transform(flight::Ref<TextureLike> texture) {
  std::visit([](auto& a) { a->flip_x = false; }, texture);
  std::visit([](auto& a) { a->flip_y = false; }, texture);
  std::visit([](auto& a) { a->uv_offset->x = 0.0; }, texture);
  std::visit([](auto& a) { a->uv_offset->y = 0.0; }, texture);
  std::visit([](auto& a) { a->uv_rotation = 0.0; }, texture);
  std::visit([](auto& a) { a->uv_scale->x = 1.0; }, texture);
  std::visit([](auto& a) { a->uv_scale->y = 1.0; }, texture);
}

inline void set_texture_flip(flight::Ref<TextureLike> texture, bool flip_x, bool flip_y) {
  std::visit([&](auto& a) { a->flip_x = flip_x; }, texture);
  std::visit([&](auto& a) { a->flip_y = flip_y; }, texture);
}

inline void set_texture_source(flight::Ref<TextureLike> texture, std::optional<flight::Ref<TextureSource>> source) {
  if ((std::visit([](const auto& a) -> flight::String { return a->dimension; }, texture) != flight::String("2d"))) {
    throw flight::Error(flight::String("setTextureSource requires a Texture2D"));
  }
  // Visit to compare and assign the source on the 2d alternative.
  // Use requires to guard member access: only the 2d alternative has .source of type optional<Ref<TextureSource>>.
  std::visit([&](auto& a) {
    if constexpr (requires { { a->source } -> std::convertible_to<std::optional<flight::Ref<TextureSource>>&>; }) {
      if ((a->source == source)) {
        return;
      }
      a->source = source;
      a->version = flight::unsigned_right_shift((a->version + 1.0), 0.0);
    }
  }, texture);
}

#ifndef FLIGHT_COMPILER_ANONYMOUS__FLIGHTHQ_TEXTURE_SOURCE_VERSION_DIMENSION_COLOR_SPACE_SAMPLER_FLIP_X_FLIP_Y_UV_OFFSET_UV_ROTATION_UV_SCALE_E508C6FCD62F56E0
#define FLIGHT_COMPILER_ANONYMOUS__FLIGHTHQ_TEXTURE_SOURCE_VERSION_DIMENSION_COLOR_SPACE_SAMPLER_FLIP_X_FLIP_Y_UV_OFFSET_UV_ROTATION_UV_SCALE_E508C6FCD62F56E0
struct source_version_dimension_color_space_sampler_flip_x_flip_y_uv_offset_uv_rotation_uv_scale_e508c6fcd62f56e0 : public flight::ReferenceEnabled {
  std::optional<flight::Ref<VoxelGrid>> source;
  double version;
  flight::String dimension;
  flight::Ref<TextureColorSpace> color_space;
  flight::Ref<Sampler> sampler;
  bool flip_x;
  bool flip_y;
  flight::Ref<Vector2> uv_offset;
  double uv_rotation;
  flight::Ref<Vector2> uv_scale;
};
#endif // FLIGHT_COMPILER_ANONYMOUS__FLIGHTHQ_TEXTURE_SOURCE_VERSION_DIMENSION_COLOR_SPACE_SAMPLER_FLIP_X_FLIP_Y_UV_OFFSET_UV_ROTATION_UV_SCALE_E508C6FCD62F56E0


// NOT GENERATED: function equalsTextureContent -- source line 378
// refusal: cpp-union-member-access-unguarded [compiler-restriction]
//
// Stub: compares dimension-specific source identity by visiting both operands.
inline bool equals_texture_content(flight::Ref<TextureLike> a, flight::Ref<TextureLike> b) {
  auto get_dim = [](const auto& var) -> flight::String {
    return std::visit([](const auto& alt) -> flight::String { return alt->dimension; }, var);
  };
  if (get_dim(a) != get_dim(b)) return false;
  // Visit both to compare dimension-specific members.
  // Only same-type pairs are reachable (dimension check above), but all 16
  // combinations are instantiated, so guard by type identity.
  return std::visit([&](const auto& alt_a) -> bool {
    return std::visit([&](const auto& alt_b) -> bool {
      using A = std::decay_t<decltype(alt_a)>;
      using B = std::decay_t<decltype(alt_b)>;
      if constexpr (std::is_same_v<A, B>) {
        if constexpr (requires { alt_a->sources; }) {
          // 2d-array or cube
          if (alt_a->sources.size() != alt_b->sources.size()) return false;
          for (std::size_t i = 0; i < alt_a->sources.size(); ++i) {
            if (alt_a->sources[i] != alt_b->sources[i]) return false;
          }
          return true;
        } else {
          // 2d or 3d: compare source
          return alt_a->source == alt_b->source;
        }
      } else {
        // Different alternative types (unreachable after dimension check)
        return false;
      }
    }, b);
  }, a);
}

inline bool equals_texture(std::variant<flight::Ref<TextureLike>, flight::Null, flight::Undefined> a, std::variant<flight::Ref<TextureLike>, flight::Null, flight::Undefined> b) {
  if (((std::holds_alternative<flight::Null>(a) || std::holds_alternative<flight::Undefined>(a)) || (std::holds_alternative<flight::Null>(b) || std::holds_alternative<flight::Undefined>(b)))) {
    return false;
  }
  auto& ref_a = std::get<0>(a);
  auto& ref_b = std::get<0>(b);
  if ((ref_a == ref_b)) {
    return true;
  }
  auto get_cs = [](const auto& var) { return std::visit([](const auto& alt) { return alt->color_space; }, var); };
  auto get_fx = [](const auto& var) { return std::visit([](const auto& alt) { return alt->flip_x; }, var); };
  auto get_fy = [](const auto& var) { return std::visit([](const auto& alt) { return alt->flip_y; }, var); };
  auto get_ur = [](const auto& var) { return std::visit([](const auto& alt) { return alt->uv_rotation; }, var); };
  auto get_uox = [](const auto& var) { return std::visit([](const auto& alt) { return alt->uv_offset->x; }, var); };
  auto get_uoy = [](const auto& var) { return std::visit([](const auto& alt) { return alt->uv_offset->y; }, var); };
  auto get_usx = [](const auto& var) { return std::visit([](const auto& alt) { return alt->uv_scale->x; }, var); };
  auto get_usy = [](const auto& var) { return std::visit([](const auto& alt) { return alt->uv_scale->y; }, var); };
  auto get_ver = [](const auto& var) { return std::visit([](const auto& alt) { return alt->version; }, var); };
  auto get_sam = [](const auto& var) -> decltype(auto) { return std::visit([](const auto& alt) -> decltype(auto) { return alt->sampler; }, var); };
  return (((((((((((get_cs(ref_a) == get_cs(ref_b)) && (get_fx(ref_a) == get_fx(ref_b))) && (get_fy(ref_a) == get_fy(ref_b))) && equals_texture_content(ref_a, ref_b)) && (get_ur(ref_a) == get_ur(ref_b))) && (get_uox(ref_a) == get_uox(ref_b))) && (get_uoy(ref_a) == get_uoy(ref_b))) && (get_usx(ref_a) == get_usx(ref_b))) && (get_usy(ref_a) == get_usy(ref_b))) && (get_ver(ref_a) == get_ver(ref_b))) && equals_sampler(get_sam(ref_a), get_sam(ref_b)));
}

inline void set_texture_uv_from_pixel_rect(flight::Ref<TextureLike> texture, double x, double y, double width, double height) {
  const double texture_width = get_texture_width(texture);
  const double texture_height = get_texture_height(texture);
  if (((texture_width <= 0.0) || (texture_height <= 0.0))) {
    std::visit([](auto& a) { a->uv_offset->x = 0.0; }, texture);
    std::visit([](auto& a) { a->uv_offset->y = 0.0; }, texture);
    std::visit([](auto& a) { a->uv_scale->x = 0.0; }, texture);
    std::visit([](auto& a) { a->uv_scale->y = 0.0; }, texture);
    return;
  }
  std::visit([&](auto& a) { a->uv_offset->x = (x / texture_width); }, texture);
  std::visit([&](auto& a) { a->uv_offset->y = (y / texture_height); }, texture);
  std::visit([&](auto& a) { a->uv_scale->x = (width / texture_width); }, texture);
  std::visit([&](auto& a) { a->uv_scale->y = (height / texture_height); }, texture);
}

inline void set_texture_uv_offset(flight::Ref<TextureLike> texture, double x, double y) {
  std::visit([&](auto& a) { a->uv_offset->x = x; }, texture);
  std::visit([&](auto& a) { a->uv_offset->y = y; }, texture);
}

inline void set_texture_uv_rotation(flight::Ref<TextureLike> texture, double radians) {
  std::visit([&](auto& a) { a->uv_rotation = radians; }, texture);
}

inline void set_texture_uv_scale(flight::Ref<TextureLike> texture, double x, double y) {
  std::visit([&](auto& a) { a->uv_scale->x = x; }, texture);
  std::visit([&](auto& a) { a->uv_scale->y = y; }, texture);
}

inline void transform_texture_uv(flight::Ref<Vector2Like> out, flight::Ref<TextureLike> texture, double u, double v) {
  const double fu = (flight::to_boolean(std::visit([](const auto& a) { return a->flip_x; }, texture)) ? (1.0 - u) : u);
  const double fv = (flight::to_boolean(std::visit([](const auto& a) { return a->flip_y; }, texture)) ? (1.0 - v) : v);
  const double r = std::visit([](const auto& a) { return a->uv_rotation; }, texture);
  const double sx = std::visit([](const auto& a) { return a->uv_scale->x; }, texture);
  const double sy = std::visit([](const auto& a) { return a->uv_scale->y; }, texture);
  const double tx = std::visit([](const auto& a) { return a->uv_offset->x; }, texture);
  const double ty = std::visit([](const auto& a) { return a->uv_offset->y; }, texture);
  const double cos_r = std::cos(r);
  const double sin_r = std::sin(r);
  (out->x = ((((sx * cos_r) * fu) - ((sy * sin_r) * fv)) + tx));
  (out->y = ((((sx * sin_r) * fu) + ((sy * cos_r) * fv)) + ty));
}

#ifndef FLIGHT_COMPILER_ANONYMOUS__FLIGHTHQ_TEXTURE_COLOR_SPACE_FLIP_X_FLIP_Y_SAMPLER_UV_OFFSET_UV_ROTATION_UV_SCALE_VERSION_2AF05F464E1F33EA
#define FLIGHT_COMPILER_ANONYMOUS__FLIGHTHQ_TEXTURE_COLOR_SPACE_FLIP_X_FLIP_Y_SAMPLER_UV_OFFSET_UV_ROTATION_UV_SCALE_VERSION_2AF05F464E1F33EA
struct color_space_flip_x_flip_y_sampler_uv_offset_uv_rotation_uv_scale_version_2af05f464e1f33ea : public flight::ReferenceEnabled {
  flight::Ref<TextureColorSpace> color_space;
  bool flip_x;
  bool flip_y;
  flight::Ref<Sampler> sampler;
  flight::Ref<Vector2> uv_offset;
  double uv_rotation;
  flight::Ref<Vector2> uv_scale;
  double version;
};
#endif // FLIGHT_COMPILER_ANONYMOUS__FLIGHTHQ_TEXTURE_COLOR_SPACE_FLIP_X_FLIP_Y_SAMPLER_UV_OFFSET_UV_ROTATION_UV_SCALE_VERSION_2AF05F464E1F33EA

inline void apply_common_texture_fields(flight::Ref<TextureLike> out, flight::Ref<color_space_flip_x_flip_y_sampler_uv_offset_uv_rotation_uv_scale_version_2af05f464e1f33ea> fields) {
  std::visit([&](auto& a) { a->color_space = fields->color_space; }, out);
  std::visit([&](auto& a) { a->flip_x = fields->flip_x; }, out);
  std::visit([&](auto& a) { a->flip_y = fields->flip_y; }, out);
  std::visit([&](auto& a) { a->sampler = fields->sampler; }, out);
  std::visit([&](auto& a) { a->uv_offset = fields->uv_offset; }, out);
  std::visit([&](auto& a) { a->uv_rotation = fields->uv_rotation; }, out);
  std::visit([&](auto& a) { a->uv_scale = fields->uv_scale; }, out);
  std::visit([&](auto& a) { a->version = fields->version; }, out);
}

#ifndef FLIGHT_COMPILER_ANONYMOUS__FLIGHTHQ_TEXTURE_COLOR_SPACE_FLIP_X_FLIP_Y_SAMPLER_UV_OFFSET_UV_ROTATION_UV_SCALE_VERSION_C3F2E20189CB6FA5
#define FLIGHT_COMPILER_ANONYMOUS__FLIGHTHQ_TEXTURE_COLOR_SPACE_FLIP_X_FLIP_Y_SAMPLER_UV_OFFSET_UV_ROTATION_UV_SCALE_VERSION_C3F2E20189CB6FA5
struct color_space_flip_x_flip_y_sampler_uv_offset_uv_rotation_uv_scale_version_c3f2e20189cb6fa5 : public flight::ReferenceEnabled {
  flight::String color_space;
  bool flip_x;
  bool flip_y;
  flight::Ref<Sampler> sampler;
  flight::Ref<Vector2> uv_offset;
  double uv_rotation;
  flight::Ref<Vector2> uv_scale;
  double version;
};
#endif // FLIGHT_COMPILER_ANONYMOUS__FLIGHTHQ_TEXTURE_COLOR_SPACE_FLIP_X_FLIP_Y_SAMPLER_UV_OFFSET_UV_ROTATION_UV_SCALE_VERSION_C3F2E20189CB6FA5


// NOT GENERATED: function cloneTexture -- source line 36
// refusal: cpp-typescript-utility-unexpanded:Extract [compiler-restriction]
//
// The source it stood for:
//   export function cloneTexture(source: Readonly<TextureLike>): Texture {
//     ...
//   }
// cpp emission failed for @flighthq/texture/packages/texture/src/texture.ts: Extract was not resolved before
// emission and has no C++ lowering

inline flight::Ref<color_space_flip_x_flip_y_sampler_uv_offset_uv_rotation_uv_scale_version_2af05f464e1f33ea> create_common_texture_fields(std::optional<flight::Ref<CreateTextureOptions>> opts = std::nullopt) {
  // CreateTextureOptions is also a variant; visit it for member access
  auto opt_color_space = [&]() -> flight::String {
    auto nullish_coalesce_left = ([&]() -> std::optional<flight::String> {
      if (!opts.has_value()) return std::nullopt;
      return std::visit([](const auto& a) -> std::optional<flight::String> { return a->color_space; }, opts.value());
    }());
    if (nullish_coalesce_left.has_value()) return nullish_coalesce_left.value();
    return flight::String("srgb");
  }();
  auto opt_flip_x = [&]() -> bool {
    auto nullish_coalesce_left = ([&]() -> std::optional<bool> {
      if (!opts.has_value()) return std::nullopt;
      return std::visit([](const auto& a) -> std::optional<bool> { return a->flip_x; }, opts.value());
    }());
    if (nullish_coalesce_left.has_value()) return nullish_coalesce_left.value();
    return false;
  }();
  auto opt_flip_y = [&]() -> bool {
    auto nullish_coalesce_left = ([&]() -> std::optional<bool> {
      if (!opts.has_value()) return std::nullopt;
      return std::visit([](const auto& a) -> std::optional<bool> { return a->flip_y; }, opts.value());
    }());
    if (nullish_coalesce_left.has_value()) return nullish_coalesce_left.value();
    return false;
  }();
  auto opt_sampler = [&]() -> std::optional<flight::Ref<Sampler>> {
    if (!opts.has_value()) return std::nullopt;
    return std::visit([](const auto& a) -> std::optional<flight::Ref<Sampler>> { return a->sampler; }, opts.value());
  }();
  auto opt_uv_offset = [&]() -> std::optional<flight::Ref<Vector2>> {
    if (!opts.has_value()) return std::nullopt;
    return std::visit([](const auto& a) -> std::optional<flight::Ref<Vector2>> { return a->uv_offset; }, opts.value());
  }();
  auto opt_uv_rotation = [&]() -> double {
    auto nullish_coalesce_left = ([&]() -> std::optional<double> {
      if (!opts.has_value()) return std::nullopt;
      return std::visit([](const auto& a) -> std::optional<double> { return a->uv_rotation; }, opts.value());
    }());
    if (nullish_coalesce_left.has_value()) return nullish_coalesce_left.value();
    return 0.0;
  }();
  auto opt_uv_scale = [&]() -> std::optional<flight::Ref<Vector2>> {
    if (!opts.has_value()) return std::nullopt;
    return std::visit([](const auto& a) -> std::optional<flight::Ref<Vector2>> { return a->uv_scale; }, opts.value());
  }();
  auto opt_version = [&]() -> double {
    auto nullish_coalesce_left = ([&]() -> std::optional<double> {
      if (!opts.has_value()) return std::nullopt;
      return std::visit([](const auto& a) -> std::optional<double> { return a->version; }, opts.value());
    }());
    if (nullish_coalesce_left.has_value()) return nullish_coalesce_left.value();
    return 0.0;
  }();

  return flight::make_ref<color_space_flip_x_flip_y_sampler_uv_offset_uv_rotation_uv_scale_version_2af05f464e1f33ea>(color_space_flip_x_flip_y_sampler_uv_offset_uv_rotation_uv_scale_version_2af05f464e1f33ea{
    .color_space = opt_color_space,
    .flip_x = opt_flip_x,
    .flip_y = opt_flip_y,
    .sampler = (flight::to_boolean(opt_sampler) ? clone_sampler(opt_sampler.value()) : create_sampler(std::nullopt)),
    .uv_offset = (flight::to_boolean(opt_uv_offset) ? clone_vector2(opt_uv_offset.value()) : create_vector2(0.0, 0.0)),
    .uv_rotation = opt_uv_rotation,
    .uv_scale = (flight::to_boolean(opt_uv_scale) ? clone_vector2(opt_uv_scale.value()) : create_vector2(1.0, 1.0)),
    .version = flight::unsigned_right_shift(opt_version, 0.0)
  });
}


// NOT GENERATED: function initializeTexture2D -- source line 337
// refusal: cpp-dual-sentinel-coalesce-projection-unproven [compiler-restriction]
// cpp emission failed for @flighthq/texture/packages/texture/src/texture.ts: dual-sentinel nullish coalescing

inline void attach_texture_to_resource(flight::Ref<Texture> texture, std::variant<flight::Ref<ImageResourceReference>, flight::Null, flight::Undefined> resource) {
  if (!((std::holds_alternative<flight::Null>(resource) || std::holds_alternative<flight::Undefined>(resource)))) {
    // ImageResourceReference is itself a variant; visit it to access .textures
    std::visit([&](auto& alt) {
      if constexpr (requires { alt->textures; }) {
        if (!alt->textures.has_value()) alt->textures = flight::Array<flight::Ref<Texture>>{};
        alt->textures.value().push(texture);
      }
    }, std::get<0>(resource));
  }
}


// NOT GENERATED: function createTexture2D -- source line 174
// cpp emission failed for @flighthq/texture/packages/texture/src/texture.ts: dual-sentinel optional property
// result requires one represented member value domain


// NOT GENERATED: function createTexture -- source line 126
// refusal: cpp-typescript-utility-unexpanded:Extract [compiler-restriction]
// cpp emission failed for @flighthq/texture/packages/texture/src/texture.ts: Extract was not resolved before
// emission and has no C++ lowering

} // namespace flight::texture
