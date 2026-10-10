// Native lowering of texture.ts at the pinned Flight revision.
#pragma once

// The generated module refused its source/clone/construction declarations. This override retains
// the emitted operations and supplies those declarations below.
#include <cmath>
#include <cstdint>
#include <flight/boolean.hpp>
#include <flight/structural_ref.hpp>
#include <optional>
#include <random>
#include <type_traits>
#include <variant>
#include <flight/runtime.hpp>
#include <flight/entity/entity.hpp>

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
using flight::types::Texture2DArray;
using flight::types::Texture3D;
using flight::types::TextureCube;
using flight::types::TextureColorSpace;
using flight::types::TextureLike;
using flight::types::TextureSource;
using flight::types::TextureSourceCubeFaces;
using flight::types::TextureSourceKind;
using flight::types::TextureSourceValue;
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


// Visits the TextureLike union to implement the source discriminant switch.
inline std::optional<TextureSourceValue> get_first_texture_source(TextureLike texture) {
  return std::visit([](const auto& alt) -> std::optional<TextureSourceValue> {
    using AltType = std::decay_t<decltype(*alt)>;
    if constexpr (std::same_as<AltType, Texture2DArray> ||
                  std::same_as<AltType, TextureCube>) {
      // 2d-array or cube: find first non-null source
      for (const auto& s : alt->sources) {
        if (s.has_value()) return s;
      }
      return std::nullopt;
    } else if constexpr (std::same_as<AltType, Texture2D>) {
      return alt->source;
    } else {
      if (alt->source.has_value()) {
        return TextureSourceValue{
            std::in_place_type<flight::Ref<VoxelGrid>>, alt->source.value()};
      }
      return std::nullopt;
    }
  }, texture);
}

inline void copy_texture(TextureLike out, TextureLike source) {
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
            o->sources = s->sources.slice();
          }
        }, source);
      }, out);
    }
  }
  std::visit([&](auto& a) { a->uv_rotation = uv_rotation; }, out);
  std::visit([&](auto& a) { a->version = version; }, out);
}

struct CreateTexture2DArrayOptions : public flight::ReferenceEnabled {
  std::optional<flight::Array<std::optional<TextureSourceValue>>> sources;
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
  std::optional<flight::Array<std::optional<TextureSourceValue>>> sources;
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

inline double texture_source_height(const TextureSourceValue& source) {
  return std::visit([](const auto& selected) { return selected->height; }, source);
}

inline double texture_source_width(const TextureSourceValue& source) {
  return std::visit([](const auto& selected) { return selected->width; }, source);
}

inline flight::String texture_source_kind(const TextureSourceValue& source) {
  return std::visit([](const auto& selected) { return selected->kind; }, source);
}

inline double get_texture_height(TextureLike texture) {
  const auto source = get_first_texture_source(texture);
  return source.has_value() ? texture_source_height(source.value()) : -1.0;
}

inline std::optional<TextureSourceValue> get_texture_source(TextureLike texture) {
  return get_first_texture_source(texture);
}

inline std::optional<flight::String> get_texture_source_kind(TextureLike texture) {
  const auto source = get_first_texture_source(texture);
  if (!source.has_value()) return std::nullopt;
  return texture_source_kind(source.value());
}

inline void get_texture_uv_matrix(Matrix3Like out, flight::Ref<TextureUvTransform> texture) {
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

inline void get_texture_inverse_uv_matrix(Matrix3Like out, TextureLike texture) {
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

inline void get_texture_view_size(Vector2Like out, TextureLike texture) {
  const auto source = get_first_texture_source(texture);
  const double source_width = source.has_value() ? texture_source_width(source.value()) : -1.0;
  const double source_height = source.has_value() ? texture_source_height(source.value()) : -1.0;
  if (((!source.has_value() || (source_width <= 0.0)) || (source_height <= 0.0))) {
    (out->x = 0.0);
    (out->y = 0.0);
    return;
  }
  const double rotation = std::visit([](const auto& a) { return a->uv_rotation; }, texture);
  if ((((rotation == 0.0) || (rotation == flight::pi)) || (rotation == -flight::pi))) {
    (out->x = std::abs((source_width * std::visit([](const auto& a) { return a->uv_scale->x; }, texture))));
    (out->y = std::abs((source_height * std::visit([](const auto& a) { return a->uv_scale->y; }, texture))));
    return;
  }
  if (((rotation == half_pi) || (rotation == -half_pi))) {
    (out->x = std::abs((source_height * std::visit([](const auto& a) { return a->uv_scale->x; }, texture))));
    (out->y = std::abs((source_width * std::visit([](const auto& a) { return a->uv_scale->y; }, texture))));
    return;
  }
  const double cos_r = std::cos(rotation);
  const double sin_r = std::sin(rotation);
  const double uv_scale_x = std::visit([](const auto& a) { return a->uv_scale->x; }, texture);
  const double uv_scale_y = std::visit([](const auto& a) { return a->uv_scale->y; }, texture);
  (out->x = std::hypot(((source_width * uv_scale_x) * cos_r), ((source_height * uv_scale_x) * sin_r)));
  (out->y = std::hypot(((source_width * uv_scale_y) * sin_r), ((source_height * uv_scale_y) * cos_r)));
}

inline double get_texture_width(TextureLike texture) {
  const auto source = get_first_texture_source(texture);
  return source.has_value() ? texture_source_width(source.value()) : -1.0;
}

inline bool has_texture_source(TextureLike texture) {
  return get_texture_source_kind(texture).has_value();
}

inline bool has_texture_uv_transform(flight::Ref<TextureUvTransform> texture) {
  return ((((((texture->flip_x || texture->flip_y) || (texture->uv_scale->x != 1.0)) || (texture->uv_scale->y != 1.0)) || (texture->uv_offset->x != 0.0)) || (texture->uv_offset->y != 0.0)) || (texture->uv_rotation != 0.0));
}

inline bool is_texture_ready(TextureLike texture) {
  return has_texture_source(texture);
}

inline void reset_texture_uv_transform(TextureLike texture) {
  std::visit([](auto& a) { a->flip_x = false; }, texture);
  std::visit([](auto& a) { a->flip_y = false; }, texture);
  std::visit([](auto& a) { a->uv_offset->x = 0.0; }, texture);
  std::visit([](auto& a) { a->uv_offset->y = 0.0; }, texture);
  std::visit([](auto& a) { a->uv_rotation = 0.0; }, texture);
  std::visit([](auto& a) { a->uv_scale->x = 1.0; }, texture);
  std::visit([](auto& a) { a->uv_scale->y = 1.0; }, texture);
}

inline void set_texture_flip(TextureLike texture, bool flip_x, bool flip_y) {
  std::visit([&](auto& a) { a->flip_x = flip_x; }, texture);
  std::visit([&](auto& a) { a->flip_y = flip_y; }, texture);
}

inline void set_texture_source(TextureLike texture, std::optional<TextureSourceValue> source) {
  if ((std::visit([](const auto& a) -> flight::String { return a->dimension; }, texture) != flight::String("2d"))) {
    throw flight::Error(flight::String("setTextureSource requires a Texture2D"));
  }
  // Visit to compare and assign the source on the 2d alternative.
  // Only the 2D alternative has a TextureSourceValue source.
  std::visit([&](auto& a) {
    using AltType = std::decay_t<decltype(*a)>;
    if constexpr (std::same_as<AltType, Texture2D>) {
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


// Source function refused as cpp-union-member-access-unguarded; the visit preserves its switch.
// Compares dimension-specific source identity by visiting both operands.
inline bool equals_texture_content(TextureLike a, TextureLike b) {
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

using OptionalTexture = std::variant<flight::Ref<Texture2DArray>, flight::Ref<TextureCube>,
                                     flight::Ref<Texture2D>, flight::Ref<Texture3D>,
                                     flight::Null, flight::Undefined>;

inline bool equals_texture(OptionalTexture a, OptionalTexture b) {
  if (((std::holds_alternative<flight::Null>(a) || std::holds_alternative<flight::Undefined>(a)) || (std::holds_alternative<flight::Null>(b) || std::holds_alternative<flight::Undefined>(b)))) {
    return false;
  }
  const auto ref_a = std::visit([](const auto& value) -> TextureLike {
    using Value = std::decay_t<decltype(value)>;
    if constexpr (std::same_as<Value, flight::Ref<Texture2DArray>> ||
                  std::same_as<Value, flight::Ref<TextureCube>> ||
                  std::same_as<Value, flight::Ref<Texture2D>> ||
                  std::same_as<Value, flight::Ref<Texture3D>>) {
      return TextureLike{std::in_place_type<Value>, value};
    } else {
      throw flight::Error(flight::String("absent texture"));
    }
  }, a);
  const auto ref_b = std::visit([](const auto& value) -> TextureLike {
    using Value = std::decay_t<decltype(value)>;
    if constexpr (std::same_as<Value, flight::Ref<Texture2DArray>> ||
                  std::same_as<Value, flight::Ref<TextureCube>> ||
                  std::same_as<Value, flight::Ref<Texture2D>> ||
                  std::same_as<Value, flight::Ref<Texture3D>>) {
      return TextureLike{std::in_place_type<Value>, value};
    } else {
      throw flight::Error(flight::String("absent texture"));
    }
  }, b);
  if (ref_a == ref_b) {
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

inline void set_texture_uv_from_pixel_rect(TextureLike texture, double x, double y, double width, double height) {
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

inline void set_texture_uv_offset(TextureLike texture, double x, double y) {
  std::visit([&](auto& a) { a->uv_offset->x = x; }, texture);
  std::visit([&](auto& a) { a->uv_offset->y = y; }, texture);
}

inline void set_texture_uv_rotation(TextureLike texture, double radians) {
  std::visit([&](auto& a) { a->uv_rotation = radians; }, texture);
}

inline void set_texture_uv_scale(TextureLike texture, double x, double y) {
  std::visit([&](auto& a) { a->uv_scale->x = x; }, texture);
  std::visit([&](auto& a) { a->uv_scale->y = y; }, texture);
}

inline void transform_texture_uv(Vector2Like out, TextureLike texture, double u, double v) {
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

inline void apply_common_texture_fields(TextureLike out, flight::Ref<color_space_flip_x_flip_y_sampler_uv_offset_uv_rotation_uv_scale_version_2af05f464e1f33ea> fields) {
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


// cloneTexture was refused because Extract remained unexpanded; its owner-preserving lowering is
// supplied after the concrete texture aliases below.

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


// initializeTexture2D and both constructors were refused at dual-sentinel and Extract boundaries;
// their concrete-owner lowerings follow this retained resource attachment helper.

using TextureResource = std::variant<flight::Ref<EmbeddedImageResourceReference>,
                                     flight::Ref<ExternalImageResourceReference>,
                                     flight::Null, flight::Undefined>;

inline void attach_texture_to_resource(Texture texture, TextureResource resource) {
  std::visit([&](auto& selected) {
    using Selected = std::decay_t<decltype(selected)>;
    if constexpr (std::same_as<Selected, flight::Ref<EmbeddedImageResourceReference>> ||
                  std::same_as<Selected, flight::Ref<ExternalImageResourceReference>>) {
      if (!selected->textures.has_value()) selected->textures = flight::Array<Texture>{};
      selected->textures.value().push(texture);
    }
  }, resource);
}

using CreateTexture2DVariantOptions =
    flight::types::source_version_color_space_sampler_flip_x_flip_y_uv_offset_uv_rotation_uv_scale_dimension_resource_944b02899c0c1ddd;
using CreateTexture3DVariantOptions =
    flight::types::source_version_color_space_sampler_flip_x_flip_y_uv_offset_uv_rotation_uv_scale_dimension_resource_5834e12080da0d2c;
using CreateTexture2DArrayVariantOptions =
    flight::types::sources_version_color_space_sampler_flip_x_flip_y_uv_offset_uv_rotation_uv_scale_dimension_resource_255ec905d3c71aa1;
using CreateTextureCubeVariantOptions =
    flight::types::sources_version_color_space_sampler_flip_x_flip_y_uv_offset_uv_rotation_uv_scale_dimension_resource_3ee2dba981682dfd;

template <typename Options>
inline flight::Ref<color_space_flip_x_flip_y_sampler_uv_offset_uv_rotation_uv_scale_version_2af05f464e1f33ea>
create_common_texture_fields_from(const std::optional<std::shared_ptr<Options>>& opts) {
  const auto color_space = opts && (*opts)->color_space ? *(*opts)->color_space : flight::String("srgb");
  const auto flip_x = opts && (*opts)->flip_x ? *(*opts)->flip_x : false;
  const auto flip_y = opts && (*opts)->flip_y ? *(*opts)->flip_y : false;
  const auto sampler = opts && (*opts)->sampler ? clone_sampler(*(*opts)->sampler) : create_sampler(std::nullopt);
  const auto uv_offset = opts && (*opts)->uv_offset ? clone_vector2(*(*opts)->uv_offset) : create_vector2(0.0, 0.0);
  const auto uv_rotation = opts && (*opts)->uv_rotation ? *(*opts)->uv_rotation : 0.0;
  const auto uv_scale = opts && (*opts)->uv_scale ? clone_vector2(*(*opts)->uv_scale) : create_vector2(1.0, 1.0);
  const auto version = flight::unsigned_right_shift(
      opts && (*opts)->version ? *(*opts)->version : 0.0, 0.0);
  return flight::make_ref<color_space_flip_x_flip_y_sampler_uv_offset_uv_rotation_uv_scale_version_2af05f464e1f33ea>(
      color_space_flip_x_flip_y_sampler_uv_offset_uv_rotation_uv_scale_version_2af05f464e1f33ea{
          .color_space = color_space,
          .flip_x = flip_x,
          .flip_y = flip_y,
          .sampler = sampler,
          .uv_offset = uv_offset,
          .uv_rotation = uv_rotation,
          .uv_scale = uv_scale,
          .version = version});
}

template <typename Owner>
inline void apply_common_texture_fields_to_owner(
    std::shared_ptr<Owner> out,
    flight::Ref<color_space_flip_x_flip_y_sampler_uv_offset_uv_rotation_uv_scale_version_2af05f464e1f33ea> fields) {
  out->color_space = fields->color_space;
  out->flip_x = fields->flip_x;
  out->flip_y = fields->flip_y;
  out->sampler = fields->sampler;
  out->uv_offset = fields->uv_offset;
  out->uv_rotation = fields->uv_rotation;
  out->uv_scale = fields->uv_scale;
  out->version = fields->version;
}

template <typename Options>
inline std::optional<TextureSourceValue> texture_source_option(
    const std::shared_ptr<Options>& opts) {
  return std::visit([](const auto& source) -> std::optional<TextureSourceValue> {
    using Source = std::decay_t<decltype(source)>;
    if constexpr (std::same_as<Source, flight::Null> ||
                  std::same_as<Source, flight::Undefined>) {
      return std::nullopt;
    } else {
      return TextureSourceValue{std::in_place_type<Source>, source};
    }
  }, opts->source);
}

template <typename Options>
inline std::optional<flight::Ref<VoxelGrid>> voxel_source_option(
    const std::shared_ptr<Options>& opts) {
  if (const auto* source = std::get_if<flight::Ref<VoxelGrid>>(&opts->source)) return *source;
  return std::nullopt;
}

template <typename Resource>
inline void attach_texture_to_flat_resource(const Texture& texture, const Resource& resource) {
  attach_texture_to_resource(texture, TextureResource{resource});
}

template <typename Options>
inline flight::Ref<Texture2D> create_texture2_d_from_options(
    std::optional<std::shared_ptr<Options>> opts) {
  const auto common = create_common_texture_fields_from(opts);
  auto out = flight::make_ref<Texture2D>();
  out->entity_runtime_key = std::nullopt;
  apply_common_texture_fields_to_owner(out, common);
  out->dimension = flight::String("2d");
  out->source = opts ? texture_source_option(*opts) : std::nullopt;
  Texture texture{std::in_place_type<flight::Ref<Texture2D>>, out};
  if (opts) attach_texture_to_flat_resource(texture, (*opts)->resource);
  return out;
}

inline void initialize_texture2_d(
    flight::types::EntityConstruction<flight::Ref<Texture2D>> out,
    std::optional<flight::Ref<flight::types::CreateTexture2DOptions>> opts = std::nullopt) {
  const auto common = create_common_texture_fields_from(opts);
  const auto owner = out.shared_owner();
  owner->set_named_value("colorSpace", common->color_space);
  owner->set_named_value("flipX", common->flip_x);
  owner->set_named_value("flipY", common->flip_y);
  owner->set_named_value("sampler", common->sampler);
  owner->set_named_value("uvOffset", common->uv_offset);
  owner->set_named_value("uvRotation", common->uv_rotation);
  owner->set_named_value("uvScale", common->uv_scale);
  owner->set_named_value("version", common->version);
  owner->set_named_value("dimension", flight::String("2d"));
  owner->set_named_value(
      "source", opts ? texture_source_option(*opts) : std::optional<TextureSourceValue>{});
}

inline flight::Ref<Texture2D> create_texture2_d(
    std::optional<flight::Ref<flight::types::CreateTexture2DOptions>> opts = std::nullopt) {
  auto construction = flight::entity::allocate_entity<flight::Ref<Texture2D>>();
  initialize_texture2_d(construction, opts);
  auto texture = flight::entity::finish_entity<flight::Ref<Texture2D>>(construction);
  if (opts) {
    Texture wrapped{std::in_place_type<flight::Ref<Texture2D>>, texture};
    attach_texture_to_flat_resource(wrapped, (*opts)->resource);
  }
  return texture;
}

inline flight::Ref<Texture2D> create_texture(
    std::optional<flight::Ref<flight::types::CreateTexture2DOptions>> opts = std::nullopt) {
  return create_texture2_d(std::move(opts));
}

inline flight::Ref<Texture2D> create_texture(flight::Ref<CreateTexture2DVariantOptions> opts) {
  return create_texture2_d_from_options(
      std::optional<flight::Ref<CreateTexture2DVariantOptions>>{std::move(opts)});
}

inline flight::Ref<Texture2DArray> create_texture(
    flight::Ref<CreateTexture2DArrayVariantOptions> opts) {
  const auto common = create_common_texture_fields_from(
      std::optional<flight::Ref<CreateTexture2DArrayVariantOptions>>{opts});
  auto out = flight::make_ref<Texture2DArray>();
  out->entity_runtime_key = std::nullopt;
  apply_common_texture_fields_to_owner(out, common);
  out->dimension = flight::String("2d-array");
  out->sources = opts->sources ? opts->sources->slice()
                              : flight::Array<std::optional<TextureSourceValue>>{};
  Texture texture{std::in_place_type<flight::Ref<Texture2DArray>>, out};
  attach_texture_to_flat_resource(texture, opts->resource);
  return out;
}

inline flight::Ref<Texture3D> create_texture(flight::Ref<CreateTexture3DVariantOptions> opts) {
  const auto common = create_common_texture_fields_from(
      std::optional<flight::Ref<CreateTexture3DVariantOptions>>{opts});
  auto out = flight::make_ref<Texture3D>();
  out->entity_runtime_key = std::nullopt;
  apply_common_texture_fields_to_owner(out, common);
  out->dimension = flight::String("3d");
  out->source = voxel_source_option(opts);
  Texture texture{std::in_place_type<flight::Ref<Texture3D>>, out};
  attach_texture_to_flat_resource(texture, opts->resource);
  return out;
}

inline flight::Ref<TextureCube> create_texture(flight::Ref<CreateTextureCubeVariantOptions> opts) {
  const auto common = create_common_texture_fields_from(
      std::optional<flight::Ref<CreateTextureCubeVariantOptions>>{opts});
  auto out = flight::make_ref<TextureCube>();
  out->entity_runtime_key = std::nullopt;
  apply_common_texture_fields_to_owner(out, common);
  out->dimension = flight::String("cube");
  if (opts->sources) {
    out->sources = opts->sources->slice();
  } else {
    out->sources = flight::Array<std::optional<TextureSourceValue>>{
        std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  }
  Texture texture{std::in_place_type<flight::Ref<TextureCube>>, out};
  attach_texture_to_flat_resource(texture, opts->resource);
  return out;
}

// Compatibility boundary for generated overload calls whose distinct TypeScript option types were
// already erased to one C++ variant. Only the 2D arm has the 2D return type selected by those calls;
// callers of the other arms use the exact-owner overloads above.
inline flight::Ref<Texture2D> create_texture(CreateTextureOptions opts) {
  if (auto* options = std::get_if<flight::Ref<CreateTexture2DVariantOptions>>(&opts)) {
    return create_texture(*options);
  }
  throw flight::Error(
      flight::String("createTexture overload result was erased; pass the exact non-2D options owner"));
}

inline Texture clone_texture(TextureLike source) {
  return std::visit(
      [](const auto& input) -> Texture {
        const auto sampler = clone_sampler(input->sampler);
        const auto uv_offset = clone_vector2(input->uv_offset);
        const auto uv_scale = clone_vector2(input->uv_scale);
        const auto copy_common = [&](const auto& out) {
          out->entity_runtime_key = std::nullopt;
          out->color_space = input->color_space;
          out->flip_x = input->flip_x;
          out->flip_y = input->flip_y;
          out->sampler = sampler;
          out->uv_offset = uv_offset;
          out->uv_rotation = input->uv_rotation;
          out->uv_scale = uv_scale;
          out->version = flight::unsigned_right_shift(input->version, 0.0);
          out->dimension = input->dimension;
        };
        if constexpr (requires { input->sources; }) {
          if (input->dimension == flight::String("cube")) {
            auto out = flight::make_ref<TextureCube>();
            copy_common(out);
            out->sources = input->sources.slice();
            return Texture{std::in_place_type<flight::Ref<TextureCube>>, out};
          }
          auto out = flight::make_ref<Texture2DArray>();
          copy_common(out);
          out->sources = input->sources.slice();
          return Texture{std::in_place_type<flight::Ref<Texture2DArray>>, out};
        } else if constexpr (std::same_as<std::remove_cvref_t<decltype(*input)>, Texture2D>) {
          auto out = flight::make_ref<Texture2D>();
          copy_common(out);
          out->source = input->source;
          return Texture{std::in_place_type<flight::Ref<Texture2D>>, out};
        } else {
          auto out = flight::make_ref<Texture3D>();
          copy_common(out);
          out->source = input->source;
          return Texture{std::in_place_type<flight::Ref<Texture3D>>, out};
        }
      },
      source);
}

} // namespace flight::texture
