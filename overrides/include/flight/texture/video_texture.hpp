// Native lowering of videoTexture.ts at the pinned Flight revision.
#pragma once

// PARTIAL: the public constructor is supplied for readonly host and the four declared
// partial TextureLike option rows and eight readonly helpers. The readonly-host
// advance carrier remains unsupplied; native convenience operations are retained.
// See overrides/manifest.json for the asserted-source/undefined-argument boundary.
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>
#include <flight/runtime.hpp>

static_assert(flight::runtime_contract.compiler_contract == "flight-runtime-contract/2", "Flight compiler/runtime contract mismatch");
static_assert(flight::runtime_contract.cpp_abi == 1, "Flight C++ runtime ABI mismatch");

#include <flight/types/create_texture_options.hpp>
#include <flight/types/entity.hpp>
#include <flight/types/host_image_source.hpp>
#include <flight/types/host_video.hpp>
#include <flight/types/image_resource.hpp>
#include <flight/types/matrix3.hpp>
#include <flight/types/texture.hpp>
#include <flight/types/texture_source_kind.hpp>
#include <flight/types/video_resource.hpp>

#include <flight/entity/entity.hpp>
#include <flight/geometry/matrix3.hpp>
#include <flight/texture/texture.hpp>

namespace flight::texture {

using flight::types::CreateTexture2DOptions;
using flight::types::EntityConstruction;
using flight::types::HostImageSource;
using flight::types::HostVideoCapability;
using flight::types::ImageResource;
using flight::types::Matrix3Like;
using flight::types::Texture;
using flight::types::Texture2D;
using flight::types::TextureLike;
using flight::types::TextureSourceValue;
using flight::types::VideoResource;

inline constexpr double initial_video_version = 4294967295.0;

inline double video_texture_version(const TextureLike& texture) {
  return std::visit([](const auto& value) { return value->version; }, texture);
}

inline void set_video_texture_version(const TextureLike& texture, double version) {
  std::visit([&](const auto& value) { value->version = version; }, texture);
}

inline std::optional<flight::Ref<ImageResource>> get_video_image(const TextureLike& texture) {
  const auto* two_d = std::get_if<flight::Ref<Texture2D>>(&texture);
  if (two_d == nullptr || (*two_d)->dimension != flight::String("2d") ||
      !(*two_d)->source.has_value()) {
    return std::nullopt;
  }
  return std::visit(
      [](const auto& source) -> std::optional<flight::Ref<ImageResource>> {
        using Source = std::decay_t<decltype(source)>;
        if constexpr (std::same_as<Source, flight::Ref<ImageResource>>) return source;
        return std::nullopt;
      },
      (*two_d)->source.value());
}

inline std::optional<HostImageSource> get_video_source(const TextureLike& texture) {
  const auto image = get_video_image(texture);
  return image ? std::optional<HostImageSource>{(*image)->source} : std::nullopt;
}

inline void initialize_video_image_resource(
    EntityConstruction<flight::Ref<ImageResource>> out,
    HostImageSource source,
    double version) {
  out->alpha_type = flight::String("straight");
  out->gamut = flight::String("srgb");
  out->height = 0.0;
  out->kind = flight::types::image_texture_source_kind;
  out->source = std::move(source);
  out->version = version;
  out->width = 0.0;
}

inline void update_video_image_size(
    flight::Ref<HostVideoCapability> host_video,
    flight::Ref<ImageResource> image) {
  image->width = host_video->get_width ? (*host_video->get_width)(image->source) : 0.0;
  image->height = host_video->get_height ? (*host_video->get_height)(image->source) : 0.0;
}

inline std::optional<flight::Ref<ImageResource>> create_video_image_resource(
    flight::Ref<HostVideoCapability> host_video,
    flight::Ref<VideoResource> source) {
  if (!source->element.has_value()) return std::nullopt;
  auto image_construction = flight::entity::allocate_entity<flight::Ref<ImageResource>>();
  initialize_video_image_resource(image_construction, source->element.value(), initial_video_version);
  auto image = flight::entity::finish_entity<flight::Ref<ImageResource>>(image_construction);
  update_video_image_size(host_video, image);
  return image;
}

// The public source accepts a readonly partial of each concrete TextureLike arm.
using ReadonlyVideoHost = flight::StructuralRef<flight::RowReadonly<
    flight::RowOf<flight::Ref<HostVideoCapability>>>>;
using ReadonlyVideoResource = flight::StructuralRef<flight::RowReadonly<
    flight::RowOf<flight::Ref<VideoResource>>>>;
template <typename TextureOwner>
using ReadonlyPartialVideoOptions = flight::StructuralRef<flight::RowReadonly<
    flight::RowPartial<flight::RowOf<flight::Ref<TextureOwner>>>>>;
using VideoTextureOptions = std::variant<
    ReadonlyPartialVideoOptions<flight::types::Texture2DArray>,
    ReadonlyPartialVideoOptions<flight::types::TextureCube>,
    ReadonlyPartialVideoOptions<flight::types::Texture2D>,
    ReadonlyPartialVideoOptions<flight::types::Texture3D>>;

namespace video_detail {
template <typename Row>
inline constexpr bool options_row_v =
    std::same_as<Row, ReadonlyPartialVideoOptions<flight::types::Texture2DArray>> ||
    std::same_as<Row, ReadonlyPartialVideoOptions<flight::types::TextureCube>> ||
    std::same_as<Row, ReadonlyPartialVideoOptions<flight::types::Texture2D>> ||
    std::same_as<Row, ReadonlyPartialVideoOptions<flight::types::Texture3D>>;

// The source spread is a fresh option object, not a retyped source owner. Keep the
// arm's content and computed member alive in that snapshot even though the forced
// 2D constructor does not consume them. Its source/dimension/version writes follow.
struct SpreadOptions {
  flight::Ref<CreateTexture2DOptions> common = flight::make_ref<CreateTexture2DOptions>();
  std::optional<TextureSourceValue> source;
  std::optional<flight::Array<std::optional<TextureSourceValue>>> sources;
  std::optional<flight::Ref<flight::types::EntityRuntime>> entity_runtime;
};

template <typename Row>
  requires options_row_v<Row>
inline SpreadOptions snapshot_options(Row source) {
  SpreadOptions result;
  // This enumerates only names, without erasing a value through Any. Native
  // members retain declaration order; partial object literals retain field order.
  for (const auto& key : flight::NamedProperties(source.shared_owner()).keys()) {
    const auto name = key.to_utf8();
    if (name == "colorSpace") result.common->color_space = flight::row_get<flight::RowKey<"colorSpace">>(source);
    else if (name == "flipX") result.common->flip_x = flight::row_get<flight::RowKey<"flipX">>(source);
    else if (name == "flipY") result.common->flip_y = flight::row_get<flight::RowKey<"flipY">>(source);
    else if (name == "sampler") result.common->sampler = flight::row_get<flight::RowKey<"sampler">>(source);
    else if (name == "uvOffset") result.common->uv_offset = flight::row_get<flight::RowKey<"uvOffset">>(source);
    else if (name == "uvRotation") result.common->uv_rotation = flight::row_get<flight::RowKey<"uvRotation">>(source);
    else if (name == "uvScale") result.common->uv_scale = flight::row_get<flight::RowKey<"uvScale">>(source);
    else if (name == "dimension") result.common->dimension = flight::row_get<flight::RowKey<"dimension">>(source);
    else if (name == "version") result.common->version = flight::row_get<flight::RowKey<"version">>(source);
    else if constexpr (std::same_as<Row, ReadonlyPartialVideoOptions<flight::types::Texture2D>> ||
                       std::same_as<Row, ReadonlyPartialVideoOptions<flight::types::Texture3D>>) {
      if (name == "source") {
        const auto value = flight::row_get<flight::RowKey<"source">>(source);
        if (value) {
          if constexpr (std::same_as<Row, ReadonlyPartialVideoOptions<flight::types::Texture2D>>) result.source = *value;
          else result.source = TextureSourceValue{std::in_place_type<flight::Ref<flight::types::VoxelGrid>>, *value};
        }
      }
    } else {
      if (name == "sources") result.sources = flight::row_get<flight::RowKey<"sources">>(source);
    }
  }
  result.entity_runtime = flight::row_get<
      std::optional<flight::Ref<flight::types::EntityRuntime>>>(source, flight::types::entity_runtime_key);
  return result;
}
} // namespace video_detail

inline std::optional<flight::Ref<ImageResource>> create_video_image_resource(
    ReadonlyVideoHost host_video,
    ReadonlyVideoResource source) {
  if (!flight::row_get<flight::RowKey<"element">>(source).has_value()) return std::nullopt;
  auto image = flight::entity::allocate_entity<flight::Ref<ImageResource>>();
  initialize_video_image_resource(image,
      flight::row_get<flight::RowKey<"element">>(source).value(), initial_video_version);
  const auto get_width = flight::row_get<flight::RowKey<"getWidth">>(host_video);
  flight::row_set<flight::RowKey<"width">>(image,
      get_width ? (*get_width)(flight::row_get<flight::RowKey<"source">>(image)) : 0.0);
  const auto get_height = flight::row_get<flight::RowKey<"getHeight">>(host_video);
  flight::row_set<flight::RowKey<"height">>(image,
      get_height ? (*get_height)(flight::row_get<flight::RowKey<"source">>(image)) : 0.0);
  return flight::entity::finish_entity<flight::Ref<ImageResource>>(image);
}

inline flight::Ref<Texture2D> create_video_texture(
    ReadonlyVideoHost host_video,
    flight::Ref<VideoResource> source,
    std::optional<VideoTextureOptions> opts = std::nullopt) {
  // Host callbacks run before any option property is copied by the spread.
  const auto image = create_video_image_resource(host_video, ReadonlyVideoResource(source));
  auto snapshot = opts ? std::visit([](const auto& row) {
    return video_detail::snapshot_options(row);
  }, *opts) : video_detail::SpreadOptions{};
  snapshot.common->dimension = flight::String("2d");
  snapshot.common->source = image
      ? decltype(snapshot.common->source){std::in_place_type<flight::Ref<ImageResource>>, *image}
      : decltype(snapshot.common->source){std::in_place_type<flight::Null>, flight::null};
  snapshot.common->version = image ? (*image)->version : initial_video_version;
  return create_texture2_d(std::optional<flight::Ref<CreateTexture2DOptions>>{snapshot.common});
}

template <typename Row>
  requires video_detail::options_row_v<Row>
inline flight::Ref<Texture2D> create_video_texture(
    ReadonlyVideoHost host_video, flight::Ref<VideoResource> source, Row opts) {
  return create_video_texture(host_video, source,
      std::optional<VideoTextureOptions>{VideoTextureOptions{std::in_place_type<Row>, std::move(opts)}});
}

inline flight::Ref<Texture2D> create_video_texture(
    flight::Ref<HostVideoCapability> host_video,
    flight::Ref<VideoResource> source,
    std::optional<flight::Ref<CreateTexture2DOptions>> opts = std::nullopt) {
  const auto image = create_video_image_resource(host_video, source);
  auto create_options = flight::make_ref<CreateTexture2DOptions>();
  if (opts) {
    create_options->color_space = (*opts)->color_space;
    create_options->sampler = (*opts)->sampler;
    create_options->flip_x = (*opts)->flip_x;
    create_options->flip_y = (*opts)->flip_y;
    create_options->uv_offset = (*opts)->uv_offset;
    create_options->uv_rotation = (*opts)->uv_rotation;
    create_options->uv_scale = (*opts)->uv_scale;
    create_options->resource = (*opts)->resource;
  }
  create_options->dimension = flight::String("2d");
  create_options->version = image ? (*image)->version : initial_video_version;
  create_options->source = image
      ? decltype(create_options->source){
            std::in_place_type<flight::Ref<ImageResource>>, *image}
      : decltype(create_options->source){
            std::in_place_type<flight::Null>, flight::null};
  return create_texture2_d(
      std::optional<flight::Ref<CreateTexture2DOptions>>{std::move(create_options)});
}

inline Texture clone_video_texture(TextureLike source) {
  return clone_texture(std::move(source));
}

inline void copy_video_texture(TextureLike out, TextureLike source) {
  copy_texture(std::move(out), std::move(source));
}

inline void get_video_texture_uv_matrix(Matrix3Like out, TextureLike texture) {
  get_texture_uv_matrix(std::move(out), std::move(texture));
}

inline void get_video_texture_inverse_uv_matrix(Matrix3Like out, TextureLike texture) {
  get_video_texture_uv_matrix(out, std::move(texture));
  flight::geometry::inverse_matrix3(out, out);
}

inline double get_video_texture_height(
    flight::Ref<HostVideoCapability> host_video,
    TextureLike texture) {
  const auto source = get_video_source(texture);
  const double height = source && host_video->get_height
      ? (*host_video->get_height)(*source)
      : 0.0;
  return height > 0.0 ? height : -1.0;
}

inline double get_video_texture_width(
    flight::Ref<HostVideoCapability> host_video,
    TextureLike texture) {
  const auto source = get_video_source(texture);
  const double width = source && host_video->get_width
      ? (*host_video->get_width)(*source)
      : 0.0;
  return width > 0.0 ? width : -1.0;
}

inline bool is_video_texture_frame_ready(
    flight::Ref<HostVideoCapability> host_video,
    TextureLike texture) {
  const auto source = get_video_source(texture);
  return source && host_video->is_ready && (*host_video->is_ready)(*source) &&
         host_video->get_width && (*host_video->get_width)(*source) > 0.0 &&
         host_video->get_height && (*host_video->get_height)(*source) > 0.0;
}

inline double advance_video_texture(
    flight::Ref<HostVideoCapability> host_video,
    TextureLike texture) {
  const auto image = get_video_image(texture);
  if (!image) return video_texture_version(texture);
  update_video_image_size(host_video, *image);
  (*image)->version = flight::unsigned_right_shift((*image)->version + 1.0, 0.0);
  set_video_texture_version(texture, (*image)->version);
  return video_texture_version(texture);
}

inline void destroy_video_texture(flight::Ref<Texture2D> texture) {
  texture->source = std::nullopt;
  texture->version = initial_video_version;
}

inline void reset_video_texture_frame(TextureLike texture) {
  if (const auto image = get_video_image(texture)) (*image)->version = initial_video_version;
  set_video_texture_version(texture, initial_video_version);
}

inline void set_video_texture_source(
    flight::Ref<HostVideoCapability> host_video,
    TextureLike texture,
    flight::Ref<VideoResource> source) {
  auto* two_d = std::get_if<flight::Ref<Texture2D>>(&texture);
  if (two_d == nullptr || (*two_d)->dimension != flight::String("2d")) {
    throw flight::Error(flight::String("setVideoTextureSource requires a Texture2D"));
  }
  const auto image = create_video_image_resource(host_video, source);
  (*two_d)->source = image
      ? std::optional<TextureSourceValue>{TextureSourceValue{
            std::in_place_type<flight::Ref<ImageResource>>, *image}}
      : std::nullopt;
  (*two_d)->version = initial_video_version;
}

// A readonly union retains the active concrete row; it does not recover a native
// owner. The pinned emitter currently erases Readonly<TextureLike> to TextureLike.
template <typename Owner>
using ReadonlyVideoTextureArm = flight::StructuralRef<flight::RowReadonly<
    flight::RowOf<flight::Ref<Owner>>>>;
using ReadonlyVideoTexture = std::variant<
    ReadonlyVideoTextureArm<flight::types::Texture2DArray>,
    ReadonlyVideoTextureArm<flight::types::TextureCube>,
    ReadonlyVideoTextureArm<flight::types::Texture2D>,
    ReadonlyVideoTextureArm<flight::types::Texture3D>>;

namespace video_detail {
template <typename Row>
inline Texture clone_readonly_texture(Row source) {
  // Object-literal evaluation order in cloneTexture, before its discriminant read.
  const auto color_space = flight::row_get<flight::RowKey<"colorSpace">>(source);
  const auto flip_x = flight::row_get<flight::RowKey<"flipX">>(source);
  const auto flip_y = flight::row_get<flight::RowKey<"flipY">>(source);
  const auto sampler = clone_sampler(flight::row_get<flight::RowKey<"sampler">>(source));
  const auto uv_offset = clone_vector2(flight::row_get<flight::RowKey<"uvOffset">>(source));
  const auto uv_rotation = flight::row_get<flight::RowKey<"uvRotation">>(source);
  const auto uv_scale = clone_vector2(flight::row_get<flight::RowKey<"uvScale">>(source));
  const auto version = flight::unsigned_right_shift(flight::row_get<flight::RowKey<"version">>(source), 0.0);
  const auto dimension = flight::row_get<flight::RowKey<"dimension">>(source);
  using Owner = typename Row::object_type;
  using Ref = flight::Ref<Owner>;
  auto out = flight::entity::allocate_entity<Ref>();
  flight::row_set<flight::RowKey<"colorSpace">>(out, color_space);
  flight::row_set<flight::RowKey<"flipX">>(out, flip_x);
  flight::row_set<flight::RowKey<"flipY">>(out, flip_y);
  flight::row_set<flight::RowKey<"sampler">>(out, sampler);
  flight::row_set<flight::RowKey<"uvOffset">>(out, uv_offset);
  flight::row_set<flight::RowKey<"uvRotation">>(out, uv_rotation);
  flight::row_set<flight::RowKey<"uvScale">>(out, uv_scale);
  flight::row_set<flight::RowKey<"version">>(out, version);
  // The closed row's dimension is its discriminant. Keep the single source read.
  flight::row_set<flight::RowKey<"dimension">>(out, dimension);
  if constexpr (std::same_as<Owner, flight::types::TextureCube> ||
                std::same_as<Owner, flight::types::Texture2DArray>) {
    flight::row_set<flight::RowKey<"sources">>(out,
        flight::row_get<flight::RowKey<"sources">>(source).slice());
  } else {
    flight::row_set<flight::RowKey<"source">>(out,
        flight::row_get<flight::RowKey<"source">>(source));
  }
  return Texture{std::in_place_type<Ref>, flight::entity::finish_entity<Ref>(out)};
}

template <typename Row>
inline void copy_readonly_texture(TextureLike out, Row source) {
  const auto out_dimension = std::visit([](const auto& value) {return value->dimension;}, out);
  if (out_dimension != flight::row_get<flight::RowKey<"dimension">>(source)) {
    throw flight::Error(flight::String("copyTexture requires matching dimensions"));
  }
  const auto color_space = flight::row_get<flight::RowKey<"colorSpace">>(source);
  const auto flip_x = flight::row_get<flight::RowKey<"flipX">>(source);
  const auto flip_y = flight::row_get<flight::RowKey<"flipY">>(source);
  const auto uv_rotation = flight::row_get<flight::RowKey<"uvRotation">>(source);
  const auto version = flight::unsigned_right_shift(flight::row_get<flight::RowKey<"version">>(source), 0.0);
  // Evaluate each destination reference before reading the source getter.
  const auto out_sampler = std::visit([](const auto& value) {return value->sampler;}, out);
  const auto source_sampler = flight::row_get<flight::RowKey<"sampler">>(source);
  copy_sampler(out_sampler, source_sampler);
  const auto out_offset = std::visit([](const auto& value) {return value->uv_offset;}, out);
  const auto source_offset = flight::row_get<flight::RowKey<"uvOffset">>(source);
  copy_vector2(out_offset, source_offset);
  const auto out_scale = std::visit([](const auto& value) {return value->uv_scale;}, out);
  const auto source_scale = flight::row_get<flight::RowKey<"uvScale">>(source);
  copy_vector2(out_scale, source_scale);
  std::visit([&](const auto& value) {
    value->color_space = color_space;
    value->flip_x = flip_x;
    value->flip_y = flip_y;
  }, out);
  const auto dimension = std::visit([](const auto& value) {return value->dimension;}, out);
  if (dimension == flight::String("2d") || dimension == flight::String("2d-array") ||
      dimension == flight::String("3d") || dimension == flight::String("cube")) {
    if (flight::row_get<flight::RowKey<"dimension">>(source) != dimension) {
      throw flight::Error(flight::String("copyTexture requires matching dimensions"));
    }
    using SourceOwner = typename Row::object_type;
    if constexpr (std::same_as<SourceOwner, flight::types::TextureCube> ||
                  std::same_as<SourceOwner, flight::types::Texture2DArray>) {
      const auto content = flight::row_get<flight::RowKey<"sources">>(source).slice();
      std::visit([&](const auto& value) {
        if constexpr (requires {value->sources = content;}) value->sources = content;
      }, out);
    } else {
      const auto content = flight::row_get<flight::RowKey<"source">>(source);
      std::visit([&](const auto& value) {
        if constexpr (requires {value->source = content;}) value->source = content;
      }, out);
    }
  }
  std::visit([&](const auto& value) {value->uv_rotation = uv_rotation; value->version = version;}, out);
}

template <typename Row>
inline void readonly_uv_matrix(Matrix3Like out, Row texture) {
  const double rotation = flight::row_get<flight::RowKey<"uvRotation">>(texture);
  const double flip_x = flight::row_get<flight::RowKey<"flipX">>(texture) ? -1.0 : 1.0;
  const double flip_y = flight::row_get<flight::RowKey<"flipY">>(texture) ? -1.0 : 1.0;
  const double sx = flight::row_get<flight::RowKey<"uvScale">>(texture)->x * flip_x;
  const double sy = flight::row_get<flight::RowKey<"uvScale">>(texture)->y * flip_y;
  const double pre_x = flight::row_get<flight::RowKey<"flipX">>(texture)
      ? flight::row_get<flight::RowKey<"uvScale">>(texture)->x : 0.0;
  const double pre_y = flight::row_get<flight::RowKey<"flipY">>(texture)
      ? flight::row_get<flight::RowKey<"uvScale">>(texture)->y : 0.0;
  double cosine, sine;
  if (rotation == 0.0) {cosine = 1.0; sine = 0.0;}
  else if (rotation == half_pi) {cosine = 0.0; sine = 1.0;}
  else if (rotation == -half_pi) {cosine = 0.0; sine = -1.0;}
  else if (rotation == flight::pi || rotation == -flight::pi) {cosine = -1.0; sine = 0.0;}
  else {cosine = std::cos(rotation); sine = std::sin(rotation);}
  const double tx = flight::row_get<flight::RowKey<"uvOffset">>(texture)->x + cosine * pre_x - sine * pre_y;
  const double ty = flight::row_get<flight::RowKey<"uvOffset">>(texture)->y + sine * pre_x + cosine * pre_y;
  auto m = out->m;
  m.set_index(0.0, sx * cosine); m.set_index(1.0, sx * sine); m.set_index(2.0, 0.0);
  m.set_index(3.0, -sy * sine); m.set_index(4.0, sy * cosine); m.set_index(5.0, 0.0);
  m.set_index(6.0, tx); m.set_index(7.0, ty); m.set_index(8.0, 1.0);
}

template <typename Row>
inline std::optional<HostImageSource> readonly_video_source(Row texture) {
  if (flight::row_get<flight::RowKey<"dimension">>(texture) != flight::String("2d")) return std::nullopt;
  if constexpr (std::same_as<typename Row::object_type, Texture2D>) {
    const auto image = flight::row_get<flight::RowKey<"source">>(texture);
    if (!image) return std::nullopt;
    return std::visit([](const auto& owner) -> std::optional<HostImageSource> {
      using Owner = typename std::decay_t<decltype(owner)>::element_type;
      if constexpr (std::same_as<Owner, ImageResource>) return owner->source;
      // These closed native source declarations have no host `source` property.
      // The pinned optional read produces undefined, then its ?? null fallback.
      else return std::nullopt;
    }, *image);
  }
  return std::nullopt;
}
} // namespace video_detail

inline Texture clone_video_texture(ReadonlyVideoTexture source) {
  return std::visit([](const auto& row) {return video_detail::clone_readonly_texture(row);}, source);
}
inline void copy_video_texture(TextureLike out, ReadonlyVideoTexture source) {
  std::visit([&](const auto& row) {video_detail::copy_readonly_texture(out, row);}, source);
}
inline void get_video_texture_uv_matrix(Matrix3Like out, ReadonlyVideoTexture texture) {
  std::visit([&](const auto& row) {video_detail::readonly_uv_matrix(out, row);}, texture);
}
inline void get_video_texture_inverse_uv_matrix(Matrix3Like out, ReadonlyVideoTexture texture) {
  get_video_texture_uv_matrix(out, texture);
  flight::geometry::inverse_matrix3(out, out);
}
inline double get_video_texture_width(ReadonlyVideoHost host, ReadonlyVideoTexture texture) {
  const auto source = std::visit([](const auto& row) {return video_detail::readonly_video_source(row);}, texture);
  double width = 0.0;
  if (source) {
    const auto callback = flight::row_get<flight::RowKey<"getWidth">>(host);
    if (callback) width = (*callback)(*source);
  }
  return width > 0.0 ? width : -1.0;
}
inline double get_video_texture_height(ReadonlyVideoHost host, ReadonlyVideoTexture texture) {
  const auto source = std::visit([](const auto& row) {return video_detail::readonly_video_source(row);}, texture);
  double height = 0.0;
  if (source) {
    const auto callback = flight::row_get<flight::RowKey<"getHeight">>(host);
    if (callback) height = (*callback)(*source);
  }
  return height > 0.0 ? height : -1.0;
}
inline bool is_video_texture_frame_ready(ReadonlyVideoHost host, ReadonlyVideoTexture texture) {
  const auto source = std::visit([](const auto& row) {return video_detail::readonly_video_source(row);}, texture);
  if (!source) return false;
  const auto ready = flight::row_get<flight::RowKey<"isReady">>(host);
  if (!ready || !(*ready)(*source)) return false;
  const auto width = flight::row_get<flight::RowKey<"getWidth">>(host);
  if (!width || !((*width)(*source) > 0.0)) return false;
  const auto height = flight::row_get<flight::RowKey<"getHeight">>(host);
  return height && (*height)(*source) > 0.0;
}
inline void set_video_texture_source(ReadonlyVideoHost host, TextureLike texture,
    flight::Ref<VideoResource> source) {
  if (std::visit([](const auto& owner) {return owner->dimension;}, texture) != flight::String("2d")) {
    throw flight::Error(flight::String("setVideoTextureSource requires a Texture2D"));
  }
  const auto image = create_video_image_resource(host, ReadonlyVideoResource(source));
  const auto owner = std::get<flight::Ref<Texture2D>>(texture);
  owner->source = image ? std::optional<TextureSourceValue>{TextureSourceValue{
      std::in_place_type<flight::Ref<ImageResource>>, *image}} : std::nullopt;
  owner->version = initial_video_version;
}

namespace video_detail {
inline ReadonlyVideoTexture readonly_texture(TextureLike texture) {
  return std::visit([](const auto& owner) -> ReadonlyVideoTexture {
    using Owner = typename std::decay_t<decltype(owner)>::element_type;
    using Row = ReadonlyVideoTextureArm<Owner>;
    return ReadonlyVideoTexture{std::in_place_type<Row>, Row(owner)};
  }, texture);
}
} // namespace video_detail
// The pinned emitter erases readonly TextureLike to the native variant. Bridge
// those exact host-row call sites into the same live readonly rows.
template <typename NativeTexture>
  requires std::same_as<NativeTexture, TextureLike>
inline double get_video_texture_width(ReadonlyVideoHost host, NativeTexture texture) {
  return get_video_texture_width(host, video_detail::readonly_texture(texture));
}
template <typename NativeTexture>
  requires std::same_as<NativeTexture, TextureLike>
inline double get_video_texture_height(ReadonlyVideoHost host, NativeTexture texture) {
  return get_video_texture_height(host, video_detail::readonly_texture(texture));
}
template <typename NativeTexture>
  requires std::same_as<NativeTexture, TextureLike>
inline bool is_video_texture_frame_ready(ReadonlyVideoHost host, NativeTexture texture) {
  return is_video_texture_frame_ready(host, video_detail::readonly_texture(texture));
}

template <typename Host, typename Owner>
  requires (std::same_as<Host, ReadonlyVideoHost> &&
      (std::same_as<Owner, flight::types::Texture2DArray> ||
       std::same_as<Owner, flight::types::TextureCube> ||
       std::same_as<Owner, flight::types::Texture2D> ||
       std::same_as<Owner, flight::types::Texture3D>))
inline double get_video_texture_width(Host host, std::shared_ptr<Owner> texture) {
  using Row = ReadonlyVideoTextureArm<Owner>;
  return get_video_texture_width(host, ReadonlyVideoTexture{std::in_place_type<Row>, Row(texture)});
}

template <typename Host, typename Owner>
  requires (std::same_as<Host, ReadonlyVideoHost> &&
      (std::same_as<Owner, flight::types::Texture2DArray> ||
       std::same_as<Owner, flight::types::TextureCube> ||
       std::same_as<Owner, flight::types::Texture2D> ||
       std::same_as<Owner, flight::types::Texture3D>))
inline double get_video_texture_height(Host host, std::shared_ptr<Owner> texture) {
  using Row = ReadonlyVideoTextureArm<Owner>;
  return get_video_texture_height(host, ReadonlyVideoTexture{std::in_place_type<Row>, Row(texture)});
}

template <typename Host, typename Owner>
  requires (std::same_as<Host, ReadonlyVideoHost> &&
      (std::same_as<Owner, flight::types::Texture2DArray> ||
       std::same_as<Owner, flight::types::TextureCube> ||
       std::same_as<Owner, flight::types::Texture2D> ||
       std::same_as<Owner, flight::types::Texture3D>))
inline bool is_video_texture_frame_ready(Host host, std::shared_ptr<Owner> texture) {
  using Row = ReadonlyVideoTextureArm<Owner>;
  return is_video_texture_frame_ready(host, ReadonlyVideoTexture{std::in_place_type<Row>, Row(texture)});
}

} // namespace flight::texture
