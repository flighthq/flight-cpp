// Native lowering of cubeTexture.ts at the pinned Flight revision. See overrides/manifest.json.
#pragma once

// PARTIAL: the C++ emitter refused declarations in this module. Everything else compiled, and each
// omission is marked NOT GENERATED below with the reason. This file is NOT complete.
//   missing: function setCubeTextureFace -- source line 57
#include <flight/boolean.hpp>
#include <flight/structural_ref.hpp>
#include <optional>
#include <tuple>
#include <variant>
#include <flight/runtime.hpp>

static_assert(flight::runtime_contract.compiler_contract == "flight-runtime-contract/2", "Flight compiler/runtime contract mismatch");
static_assert(flight::runtime_contract.cpp_abi == 1, "Flight C++ runtime ABI mismatch");

namespace flight::types { struct Bitmap; }
namespace flight::types { struct CompressedImageResource; }
namespace flight::types { struct CreateCubeTextureOptions; }
namespace flight::types { struct ExternalTexture; }
namespace flight::types { struct ImageResource; }
namespace flight::types { struct RenderTarget; }
namespace flight::types { struct Sampler; }
namespace flight::types { struct Texture2D; }
namespace flight::types { struct Texture2DArray; }
namespace flight::types { struct Texture3D; }
namespace flight::types { struct TextureCube; }
namespace flight::types { struct TextureResourceOptions; }
namespace flight::types { struct TextureSource; }
namespace flight::types { struct Vector2; }
namespace flight::types { struct VoxelGrid; }

#include <flight/types/bitmap.hpp>
#include <flight/types/compressed_image_resource.hpp>
#include <flight/types/create_cube_texture_options.hpp>
#include <flight/types/create_texture_options.hpp>
#include <flight/types/cube_texture.hpp>
#include <flight/types/external_texture.hpp>
#include <flight/types/image_resource.hpp>
#include <flight/types/render_target.hpp>
#include <flight/types/sampler.hpp>
#include <flight/types/texture.hpp>
#include <flight/types/texture_source.hpp>
#include <flight/types/texture_source_value.hpp>
#include <flight/types/vector2.hpp>
#include <flight/types/voxel_grid.hpp>
#include <flight/texture/sampler.hpp>
#include <flight/texture/texture.hpp>

namespace flight::texture {

using flight::types::Bitmap;
using flight::types::CompressedImageResource;
using flight::types::CubeTexture;
using flight::types::ExternalTexture;
using flight::types::ImageResource;
using flight::types::RenderTarget;
using flight::types::Sampler;
using flight::types::Texture;
using flight::types::Texture2D;
using flight::types::TextureSource;
using flight::types::Vector2;
using flight::types::VoxelGrid;


using ReadonlyCubeTexture = flight::StructuralRef<
    flight::RowReadonly<flight::RowOf<flight::types::CubeTexture>>>;

inline flight::types::CubeTexture clone_cube_texture(ReadonlyCubeTexture source) {
  const auto color_space = flight::row_get<flight::RowKey<"colorSpace">>(source);
  const auto flip_x = flight::row_get<flight::RowKey<"flipX">>(source);
  const auto flip_y = flight::row_get<flight::RowKey<"flipY">>(source);
  const auto sampler = clone_sampler(flight::row_get<flight::RowKey<"sampler">>(source));
  const auto uv_offset = flight::geometry::clone_vector2(
      flight::row_get<flight::RowKey<"uvOffset">>(source));
  const auto uv_rotation = flight::row_get<flight::RowKey<"uvRotation">>(source);
  const auto uv_scale = flight::geometry::clone_vector2(
      flight::row_get<flight::RowKey<"uvScale">>(source));
  const auto version = flight::unsigned_right_shift(
      flight::row_get<flight::RowKey<"version">>(source), 0.0);
  auto out = flight::entity::allocate_entity<flight::types::CubeTexture>();
  flight::row_set<flight::RowKey<"colorSpace">>(out, color_space);
  flight::row_set<flight::RowKey<"flipX">>(out, flip_x);
  flight::row_set<flight::RowKey<"flipY">>(out, flip_y);
  flight::row_set<flight::RowKey<"sampler">>(out, sampler);
  flight::row_set<flight::RowKey<"uvOffset">>(out, uv_offset);
  flight::row_set<flight::RowKey<"uvRotation">>(out, uv_rotation);
  flight::row_set<flight::RowKey<"uvScale">>(out, uv_scale);
  flight::row_set<flight::RowKey<"version">>(out, version);
  flight::row_set<flight::RowKey<"dimension">>(out, flight::String("cube"));
  flight::row_set<flight::RowKey<"sources">>(out,
      flight::row_get<flight::RowKey<"sources">>(source).slice());
  return flight::entity::finish_entity<flight::types::CubeTexture>(out);
}

inline void copy_cube_texture(flight::types::CubeTexture out, ReadonlyCubeTexture source) {
  if (out->dimension != flight::row_get<flight::RowKey<"dimension">>(source)) {
    throw flight::Error(flight::String("copyTexture requires matching dimensions"));
  }
  const auto color_space = flight::row_get<flight::RowKey<"colorSpace">>(source);
  const auto flip_x = flight::row_get<flight::RowKey<"flipX">>(source);
  const auto flip_y = flight::row_get<flight::RowKey<"flipY">>(source);
  const auto uv_rotation = flight::row_get<flight::RowKey<"uvRotation">>(source);
  const auto version = flight::unsigned_right_shift(
      flight::row_get<flight::RowKey<"version">>(source), 0.0);
  copy_sampler(out->sampler, flight::row_get<flight::RowKey<"sampler">>(source));
  flight::geometry::copy_vector2(out->uv_offset,
      flight::row_get<flight::RowKey<"uvOffset">>(source));
  flight::geometry::copy_vector2(out->uv_scale,
      flight::row_get<flight::RowKey<"uvScale">>(source));
  out->color_space = color_space;
  out->flip_x = flip_x;
  out->flip_y = flip_y;
  if (flight::row_get<flight::RowKey<"dimension">>(source) != flight::String("cube")) {
    throw flight::Error(flight::String("copyTexture requires matching dimensions"));
  }
  out->sources = flight::row_get<flight::RowKey<"sources">>(source).slice();
  out->uv_rotation = uv_rotation;
  out->version = version;
}

inline flight::types::CubeTexture create_cube_texture(std::optional<flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::CreateCubeTextureOptions>>>>> opts = std::nullopt) {
  return flight::texture::create_texture(flight::make_structural_ref<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::CreateTextureCubeOptions>>>>(flight::row_field<flight::RowKey<"colorSpace">>(([&]() -> std::optional<flight::String> { auto nullish_coalesce_left = ([&]() -> std::optional<flight::types::TextureColorSpace> { auto optional_chain_receiver = opts; if (!optional_chain_receiver.has_value()) return std::nullopt; return flight::row_get<flight::RowKey<"colorSpace">>(optional_chain_receiver.value()); }()); if (nullish_coalesce_left.has_value()) return nullish_coalesce_left; return std::optional<flight::String>{flight::String("srgb")}; }())), flight::row_field<flight::RowKey<"sampler">>((flight::to_boolean(([&]() -> std::optional<flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::types::SamplerLike>>>> { auto optional_chain_receiver = opts; if (!optional_chain_receiver.has_value()) return std::nullopt; return flight::row_get<flight::RowKey<"sampler">>(optional_chain_receiver.value()); }())) ? std::optional<flight::Ref<flight::types::Sampler>>{flight::texture::clone_sampler(flight::row_get<flight::RowKey<"sampler">>(opts.value()).value())} : std::nullopt)), flight::row_field<flight::RowKey<"dimension">>(flight::String("cube")), flight::row_field<flight::RowKey<"sources">>(([&]() -> std::optional<flight::Array<std::optional<flight::types::TextureSourceValue>>> { auto nullish_coalesce_left = ([&]() -> std::optional<flight::types::TextureSourceCubeFaces> { auto optional_chain_receiver = opts; if (!optional_chain_receiver.has_value()) return std::nullopt; return flight::row_get<flight::RowKey<"sources">>(optional_chain_receiver.value()); }()); if (nullish_coalesce_left.has_value()) return nullish_coalesce_left; return std::optional<flight::Array<std::optional<flight::types::TextureSourceValue>>>{flight::Array<std::optional<flight::types::TextureSourceValue>>{std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt}}; }()))));
}


namespace cube_detail {

template <typename Value>
inline constexpr bool operand_v = std::same_as<Value, flight::types::CubeTexture> ||
    std::same_as<Value, ReadonlyCubeTexture> || std::same_as<Value, flight::Null> ||
    std::same_as<Value, flight::Undefined>;
template <typename... Values>
inline constexpr bool operand_v<std::variant<Values...>> = (operand_v<Values> && ...);

inline std::optional<ReadonlyCubeTexture> readonly_operand(flight::types::CubeTexture value) {
  return value ? std::optional<ReadonlyCubeTexture>{ReadonlyCubeTexture(value)} : std::nullopt;
}
inline std::optional<ReadonlyCubeTexture> readonly_operand(ReadonlyCubeTexture value) {
  return value ? std::optional<ReadonlyCubeTexture>{value} : std::nullopt;
}
inline std::optional<ReadonlyCubeTexture> readonly_operand(flight::Null) { return std::nullopt; }
inline std::optional<ReadonlyCubeTexture> readonly_operand(flight::Undefined) { return std::nullopt; }
template <typename... Values>
  requires (operand_v<Values> && ...)
inline std::optional<ReadonlyCubeTexture> readonly_operand(const std::variant<Values...>& value) {
  return std::visit([](const auto& held) { return readonly_operand(held); }, value);
}

inline bool equals_content(ReadonlyCubeTexture a, ReadonlyCubeTexture b) {
  if (flight::row_get<flight::RowKey<"dimension">>(a) !=
      flight::row_get<flight::RowKey<"dimension">>(b)) return false;
  const auto& a_sources = flight::row_get<flight::RowKey<"sources">>(a);
  const auto& b_sources = flight::row_get<flight::RowKey<"sources">>(b);
  if (a_sources.size() != b_sources.size()) return false;
  for (std::size_t index = 0; index < a_sources.size(); ++index) {
    if (a_sources[index] != b_sources[index]) return false;
  }
  return true;
}
} // namespace cube_detail

template <typename A, typename B>
  requires (cube_detail::operand_v<std::remove_cvref_t<A>> &&
            cube_detail::operand_v<std::remove_cvref_t<B>>)
inline bool equals_cube_texture(const A& a, const B& b) {
  const auto a_view = cube_detail::readonly_operand(a);
  if (!a_view) return false;
  const auto b_view = cube_detail::readonly_operand(b);
  if (!b_view) return false;
  if (*a_view == *b_view) return true;
  const auto& left = *a_view;
  const auto& right = *b_view;
  return flight::row_get<flight::RowKey<"colorSpace">>(left) == flight::row_get<flight::RowKey<"colorSpace">>(right) &&
      flight::row_get<flight::RowKey<"flipX">>(left) == flight::row_get<flight::RowKey<"flipX">>(right) &&
      flight::row_get<flight::RowKey<"flipY">>(left) == flight::row_get<flight::RowKey<"flipY">>(right) &&
      cube_detail::equals_content(left, right) &&
      flight::row_get<flight::RowKey<"uvRotation">>(left) == flight::row_get<flight::RowKey<"uvRotation">>(right) &&
      flight::row_get<flight::RowKey<"uvOffset">>(left)->x == flight::row_get<flight::RowKey<"uvOffset">>(right)->x &&
      flight::row_get<flight::RowKey<"uvOffset">>(left)->y == flight::row_get<flight::RowKey<"uvOffset">>(right)->y &&
      flight::row_get<flight::RowKey<"uvScale">>(left)->x == flight::row_get<flight::RowKey<"uvScale">>(right)->x &&
      flight::row_get<flight::RowKey<"uvScale">>(left)->y == flight::row_get<flight::RowKey<"uvScale">>(right)->y &&
      flight::row_get<flight::RowKey<"version">>(left) == flight::row_get<flight::RowKey<"version">>(right) &&
      equals_sampler(flight::row_get<flight::RowKey<"sampler">>(left),
                     flight::row_get<flight::RowKey<"sampler">>(right));
}

using OptionalReadonlyCubeTexture =
    std::variant<ReadonlyCubeTexture, flight::Null, flight::Undefined>;

inline bool equals_cube_texture(OptionalReadonlyCubeTexture a, OptionalReadonlyCubeTexture b) {
  return equals_cube_texture<OptionalReadonlyCubeTexture, OptionalReadonlyCubeTexture>(a, b);
}

inline double get_cube_texture_face_size(flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::types::CubeTexture>>> cube) {
  flight::types::TextureSourceCubeFaces sources = flight::row_get<flight::RowKey<"sources">>(cube);
  {
    double i = 0.0;
    while ((i < 6.0)) {
      {
        std::optional<std::variant<flight::Ref<flight::types::VoxelGrid>, flight::Ref<flight::types::TextureSource>, flight::Ref<flight::types::ExternalTexture>, flight::Ref<flight::types::Bitmap>, flight::Ref<flight::types::CompressedImageResource>, flight::Ref<flight::types::RenderTarget>, flight::Ref<flight::types::ImageResource>>> face = sources.element(i);
        if (face.has_value()) {
          return std::visit([](const auto& source) { return source->width; }, face.value());
        }
      }
      (i += 1.0);
    }
  }
  return -1.0;
}

inline bool is_cube_texture_complete(flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::types::CubeTexture>>> cube) {
  flight::types::TextureSourceCubeFaces sources = flight::row_get<flight::RowKey<"sources">>(cube);
  return sources.every([](const auto& face) { return face.has_value(); });
}


// NOT GENERATED: function setCubeTextureFace -- source line 57
// refusal: cpp-erased-value-assertion-unrepresented [source-portability]
//
// The source it stood for:
//   export function setCubeTextureFace(cube: CubeTexture, faceIndex: number, source: TextureSource | null): void {
//     const sources = cube.sources;
//     if (sources[faceIndex] === source) return;
//     (sources as unknown as (TextureSource | null)[])[faceIndex] = source;
//     cube.version = (cube.version + 1) >>> 0;
//   }
//
// cpp emission failed for @flighthq/texture/packages/texture/src/cubeTexture.ts: an erased assertion hides the
// represented source carrier flight::types::TextureSourceCubeFaces before naming
// flight::Array<std::optional<flight::Ref<flight::types::TextureSource>>>, but those carriers are not
// representation-equivalent. Keep the exact array type through the assertion, or explicitly convert its elements
// before asserting the result; the compiler will not reinterpret the source carrier, cast it, or materialize a
// replacement array

} // namespace flight::texture
