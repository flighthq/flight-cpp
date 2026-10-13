#pragma once

#include <cmath>
#include <optional>
#include <utility>
#include <variant>

#include <flight/json.hpp>
#include <flight/number.hpp>
#include <flight/textureatlas/texture_atlas_region.hpp>
#include <flight/types/texture_atlas.hpp>
#include <flight/types/texture_atlas_region.hpp>
#include <flight/types/texture_packer_atlas_parse_options.hpp>

namespace flight::textureatlas_formats::detail {

enum class JsonAtlasFlavor { Aseprite, TexturePacker };

inline std::optional<flight::JsonValue> json_member(
    const flight::JsonObject& object,
    const char* name) {
  return object.get(flight::String(name));
}

inline const flight::JsonObject* json_object_if(const flight::JsonValue& value) {
  return value.kind() == flight::JsonValue::Kind::object ? &value.as_object() : nullptr;
}

inline std::optional<flight::String> json_string_member(
    const flight::JsonObject& object,
    const char* name) {
  const auto value = json_member(object, name);
  if (!value.has_value() || value->kind() != flight::JsonValue::Kind::string) return std::nullopt;
  return value->as_string();
}

inline std::optional<double> json_number_member(
    const flight::JsonObject& object,
    const char* name) {
  const auto value = json_member(object, name);
  if (!value.has_value() || value->kind() != flight::JsonValue::Kind::number) return std::nullopt;
  return value->as_number();
}

inline bool json_true_member(const flight::JsonObject& object, const char* name) {
  const auto value = json_member(object, name);
  return value.has_value() && value->kind() == flight::JsonValue::Kind::boolean && value->as_boolean();
}

inline const flight::JsonObject* json_object_member(
    const flight::JsonObject& object,
    const char* name,
    std::optional<flight::JsonValue>& storage) {
  storage = json_member(object, name);
  return storage.has_value() ? json_object_if(*storage) : nullptr;
}

inline std::variant<double, flight::Null, flight::Undefined> nullable_number(
    std::optional<double> value) {
  if (value.has_value()) {
    return std::variant<double, flight::Null, flight::Undefined>{
        std::in_place_type<double>, *value};
  }
  return std::variant<double, flight::Null, flight::Undefined>{
      std::in_place_type<flight::Null>, flight::null};
}

inline flight::String normalize_json_frame_name(flight::String name, bool strip) {
  if (!strip) return name;
  const double slash = flight::maximum(
      name.last_index_of(flight::String("/")), name.last_index_of(flight::String("\\")));
  return slash >= 0.0 ? name.slice(slash + 1.0) : name;
}

inline void reset_json_atlas_page_meta(flight::Ref<flight::types::TextureAtlas> atlas) {
  atlas->image_height = 0.0;
  atlas->image_name = std::nullopt;
  atlas->image_width = 0.0;
  atlas->scale = 1.0;
}

inline void apply_json_atlas_page_meta(
    flight::Ref<flight::types::TextureAtlas> atlas,
    const flight::JsonObject& root) {
  reset_json_atlas_page_meta(atlas);
  std::optional<flight::JsonValue> meta_value;
  const auto* meta = json_object_member(root, "meta", meta_value);
  if (meta == nullptr) return;

  atlas->image_name = json_string_member(*meta, "image");
  std::optional<flight::JsonValue> size_value;
  if (const auto* size = json_object_member(*meta, "size", size_value); size != nullptr) {
    atlas->image_width = json_number_member(*size, "w").value_or(0.0);
    atlas->image_height = json_number_member(*size, "h").value_or(0.0);
  }

  const auto scale = json_member(*meta, "scale");
  double numeric_scale = 1.0;
  if (scale.has_value() && scale->kind() == flight::JsonValue::Kind::number) {
    numeric_scale = scale->as_number();
  } else if (scale.has_value() && scale->kind() == flight::JsonValue::Kind::string) {
    numeric_scale = flight::parse_float(scale->as_string());
  }
  atlas->scale = std::isfinite(numeric_scale) && numeric_scale > 0.0 ? numeric_scale : 1.0;
}

inline void append_json_atlas_region(
    flight::Ref<flight::types::TextureAtlas> atlas,
    flight::String name,
    const flight::JsonObject& entry,
    JsonAtlasFlavor flavor,
    bool strip_path_prefix) {
  std::optional<flight::JsonValue> frame_value;
  const auto* frame = json_object_member(entry, "frame", frame_value);
  if (frame == nullptr) return;

  const double x = json_number_member(*frame, "x").value_or(0.0);
  const double y = json_number_member(*frame, "y").value_or(0.0);
  const double width = json_number_member(*frame, "w").value_or(0.0);
  const double height = json_number_member(*frame, "h").value_or(0.0);
  const bool trimmed = json_true_member(entry, "trimmed");
  const bool rotated = json_true_member(entry, "rotated");

  std::optional<flight::JsonValue> source_size_value;
  const auto* source_size = json_object_member(entry, "sourceSize", source_size_value);
  const std::optional<double> original_width =
      trimmed && source_size != nullptr ? json_number_member(*source_size, "w") : std::nullopt;
  const std::optional<double> original_height =
      trimmed && source_size != nullptr ? json_number_member(*source_size, "h") : std::nullopt;

  std::optional<flight::JsonValue> sprite_source_size_value;
  const auto* sprite_source_size =
      json_object_member(entry, "spriteSourceSize", sprite_source_size_value);
  const double source_x = sprite_source_size == nullptr
      ? 0.0
      : json_number_member(*sprite_source_size, "x").value_or(0.0);
  const double source_y = sprite_source_size == nullptr
      ? 0.0
      : json_number_member(*sprite_source_size, "y").value_or(0.0);

  std::optional<double> pivot_x;
  std::optional<double> pivot_y;
  if (flavor == JsonAtlasFlavor::TexturePacker) {
    std::optional<flight::JsonValue> pivot_value;
    if (const auto* pivot = json_object_member(entry, "pivot", pivot_value); pivot != nullptr) {
      pivot_x = json_number_member(*pivot, "x");
      pivot_y = json_number_member(*pivot, "y");
    }
  }

  using RegionOptions =
      flight::textureatlas::height_id_name_original_height_original_width_page_name_pivot_x_pivot_y_rotation_source_x_source_y_trimmed_x_y_width_cc6b37d497b99a10;
  const auto options = flight::make_ref<RegionOptions>(RegionOptions{
      .height = height,
      .id = static_cast<double>(atlas->regions.size()),
      .name = std::variant<flight::String, flight::Null, flight::Undefined>{
          std::in_place_type<flight::String>, normalize_json_frame_name(name, strip_path_prefix)},
      .original_height = nullable_number(original_height),
      .original_width = nullable_number(original_width),
      .page_name = std::variant<flight::String, flight::Null, flight::Undefined>{
          std::in_place_type<flight::Null>, flight::null},
      .pivot_x = nullable_number(pivot_x),
      .pivot_y = nullable_number(pivot_y),
      .rotation = rotated ? flight::types::TextureAtlasRotation::Clockwise90
                          : flight::types::TextureAtlasRotation::None,
      .source_x = source_x,
      .source_y = source_y,
      .trimmed = trimmed,
      .x = x,
      .y = y,
      .width = width,
  });
  atlas->regions.push(flight::textureatlas::create_texture_atlas_region(options));
}

inline flight::Ref<flight::types::TextureAtlas> parse_json_atlas_checked(
    flight::String json,
    flight::Ref<flight::types::TextureAtlas> atlas,
    JsonAtlasFlavor flavor,
    bool strip_path_prefix) {
  try {
    const flight::JsonValue document = flight::Json::parse(json);
    const auto* root = json_object_if(document);
    if (root == nullptr) return atlas;
    const auto frames_value = json_member(*root, "frames");
    if (!frames_value.has_value() ||
        (frames_value->kind() != flight::JsonValue::Kind::array &&
         frames_value->kind() != flight::JsonValue::Kind::object)) {
      return atlas;
    }

    atlas->regions.resize(0.0);
    apply_json_atlas_page_meta(atlas, *root);
    if (frames_value->kind() == flight::JsonValue::Kind::array) {
      for (const auto& value : frames_value->as_array()) {
        const auto* entry = json_object_if(value);
        if (entry == nullptr) continue;
        const auto name = json_string_member(*entry, "filename");
        if (!name.has_value()) continue;
        append_json_atlas_region(atlas, *name, *entry, flavor, strip_path_prefix);
      }
    } else {
      for (const auto& [name, value] : frames_value->as_object()) {
        const auto* entry = json_object_if(value);
        if (entry == nullptr) continue;
        append_json_atlas_region(atlas, name, *entry, flavor, strip_path_prefix);
      }
    }
  } catch (...) {
    return atlas;
  }
  return atlas;
}

inline flight::Ref<flight::types::TextureAtlas> parse_texture_atlas_aseprite_json_checked(
    flight::String json,
    flight::Ref<flight::types::TextureAtlas> atlas) {
  return parse_json_atlas_checked(
      std::move(json), std::move(atlas), JsonAtlasFlavor::Aseprite, false);
}

inline flight::Ref<flight::types::TextureAtlas> parse_texture_packer_atlas_json_checked(
    flight::String json,
    flight::Ref<flight::types::TextureAtlas> atlas,
    const std::optional<flight::Ref<flight::types::TexturePackerAtlasParseOptions>>& options) {
  const bool strip_path_prefix = options.has_value() && options.value() != nullptr &&
      options.value()->strip_path_prefix.value_or(false);
  return parse_json_atlas_checked(
      std::move(json), std::move(atlas), JsonAtlasFlavor::TexturePacker, strip_path_prefix);
}

}  // namespace flight::textureatlas_formats::detail
