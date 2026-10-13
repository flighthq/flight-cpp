// Native completion of the a627849 TMJ/TSJ parser over flight::JsonValue.
#pragma once

#include <cmath>
#include <optional>
#include <variant>
#include <vector>

#include <flight/importdiagnostics/import_diagnostic_collector.hpp>
#include <flight/json.hpp>
#include <flight/structural_ref.hpp>
#include <flight/tilemap_formats/tiled_color.hpp>
#include <flight/tilemap_formats/tiled_layer_data.hpp>
#include <flight/types/import_diagnostic.hpp>
#include <flight/types/tiled_layer.hpp>
#include <flight/types/tiled_map.hpp>
#include <flight/types/tiled_object.hpp>
#include <flight/types/tiled_parse_options.hpp>
#include <flight/types/tiled_property.hpp>
#include <flight/types/tiled_tileset.hpp>
#include <flight/types/vector2.hpp>

namespace flight::tilemap_formats {

using JsonOptionsView = flight::StructuralRef<
    flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::TiledParseOptions>>>>;
using JsonDiagnostics = flight::Array<flight::Ref<flight::types::ImportDiagnostic>>;
using JsonDiagnosticValue = std::variant<bool, double, flight::String>;
using JsonDiagnosticDetail = flight::Record<flight::String, JsonDiagnosticValue>;

namespace tiled_json_detail {

inline std::optional<flight::JsonObject> parse_object(flight::String text) {
  try {
    const auto value = flight::Json::parse(text);
    if (value.kind() != flight::JsonValue::Kind::object) return std::nullopt;
    return value.as_object();
  } catch (const flight::JsonSyntaxError&) {
    return std::nullopt;
  }
}

inline std::optional<flight::JsonValue> value(
    const flight::JsonObject& object,
    const char* key) {
  return object.get(flight::String(key));
}

inline std::optional<flight::String> string_value(
    const flight::JsonObject& object,
    const char* key) {
  const auto candidate = value(object, key);
  if (!candidate || candidate->kind() != flight::JsonValue::Kind::string) return std::nullopt;
  return candidate->as_string();
}

inline double number_value(
    const flight::JsonObject& object,
    const char* key,
    double fallback = 0.0) {
  const auto candidate = value(object, key);
  return candidate && candidate->kind() == flight::JsonValue::Kind::number &&
                 std::isfinite(candidate->as_number())
             ? candidate->as_number()
             : fallback;
}

inline bool boolean_value(
    const flight::JsonObject& object,
    const char* key,
    bool fallback = false) {
  const auto candidate = value(object, key);
  return candidate && candidate->kind() == flight::JsonValue::Kind::boolean
             ? candidate->as_boolean()
             : fallback;
}

inline std::vector<flight::JsonObject> object_array(
    const flight::JsonObject& object,
    const char* key) {
  std::vector<flight::JsonObject> result;
  const auto candidate = value(object, key);
  if (!candidate || candidate->kind() != flight::JsonValue::Kind::array) return result;
  for (const auto& entry : candidate->as_array()) {
    if (entry.kind() == flight::JsonValue::Kind::object) result.push_back(entry.as_object());
  }
  return result;
}

inline JsonDiagnosticDetail diagnostic_detail(
    std::initializer_list<std::pair<flight::String, JsonDiagnosticValue>> entries) {
  JsonDiagnosticDetail result;
  for (const auto& [key, entry] : entries) result.set(key, entry);
  return result;
}

inline void report(
    const std::optional<JsonDiagnostics>& diagnostics,
    flight::String severity,
    flight::String kind,
    flight::String origin,
    JsonDiagnosticDetail detail) {
  flight::importdiagnostics::report_import_diagnostic(
      diagnostics, severity, kind, origin, std::optional<JsonDiagnosticDetail>{std::move(detail)});
}

inline void report_missing(
    const std::optional<JsonDiagnostics>& diagnostics,
    flight::String origin,
    flight::String path,
    std::optional<flight::String> field = std::nullopt) {
  const auto full_path = field ? path + flight::String(".") + *field : path;
  report(
      diagnostics,
      flight::types::import_diagnostic_severity->recover,
      flight::String("tiled.required-field-missing"),
      origin,
      diagnostic_detail({{flight::String("path"), full_path}}));
}

inline void report_missing_field(
    const flight::JsonObject& object,
    const char* key,
    const std::optional<JsonDiagnostics>& diagnostics,
    flight::String origin,
    flight::String path) {
  if (diagnostics && !object.has(flight::String(key))) {
    report_missing(diagnostics, origin, path, flight::String(key));
  }
}

inline flight::String property_type(std::optional<flight::String> value) {
  if (value == flight::String("int") || value == flight::String("float") ||
      value == flight::String("bool") || value == flight::String("color") ||
      value == flight::String("file") || value == flight::String("object")) {
    return *value;
  }
  return flight::String("string");
}

inline flight::String orientation(std::optional<flight::String> value) {
  if (value == flight::String("isometric") || value == flight::String("staggered") ||
      value == flight::String("hexagonal")) {
    return *value;
  }
  return flight::String("orthogonal");
}

inline flight::String render_order(std::optional<flight::String> value) {
  if (value == flight::String("right-up") || value == flight::String("left-down") ||
      value == flight::String("left-up")) {
    return *value;
  }
  return flight::String("right-down");
}

inline flight::String object_alignment(std::optional<flight::String> value) {
  static const char* values[] = {
      "topleft", "top", "topright", "left", "center", "right",
      "bottomleft", "bottom", "bottomright"};
  for (const auto* candidate : values) {
    if (value == flight::String(candidate)) return *value;
  }
  return flight::String("unspecified");
}

inline std::optional<flight::String> stagger_axis(std::optional<flight::String> value) {
  return value == flight::String("x") || value == flight::String("y") ? value : std::nullopt;
}

inline std::optional<flight::String> stagger_index(std::optional<flight::String> value) {
  return value == flight::String("even") || value == flight::String("odd") ? value : std::nullopt;
}

inline std::optional<flight::String> compression(std::optional<flight::String> value) {
  return value == flight::String("gzip") || value == flight::String("zlib") ||
                 value == flight::String("zstd")
             ? value
             : std::nullopt;
}

inline flight::String value_to_string(const flight::JsonValue& value);

inline flight::String array_to_string(const flight::JsonArray& array) {
  flight::Array<flight::String> parts;
  for (const auto& entry : array) {
    parts.push(entry.is_null() ? flight::String("") : value_to_string(entry));
  }
  return parts.join(flight::String(","));
}

inline flight::String value_to_string(const flight::JsonValue& value) {
  switch (value.kind()) {
    case flight::JsonValue::Kind::null: return flight::String("null");
    case flight::JsonValue::Kind::boolean:
      return value.as_boolean() ? flight::String("true") : flight::String("false");
    case flight::JsonValue::Kind::number: return flight::String::from_number(value.as_number());
    case flight::JsonValue::Kind::string: return value.as_string();
    case flight::JsonValue::Kind::array: return array_to_string(value.as_array());
    case flight::JsonValue::Kind::object: return flight::String("[object Object]");
  }
  return flight::String("");
}

inline std::variant<bool, double, flight::String> property_value(
    const flight::String& type,
    const std::optional<flight::JsonValue>& raw) {
  if (type == flight::String("bool")) {
    return raw && raw->kind() == flight::JsonValue::Kind::boolean && raw->as_boolean();
  }
  if (type == flight::String("int") || type == flight::String("float")) {
    return raw && raw->kind() == flight::JsonValue::Kind::number ? raw->as_number() : 0.0;
  }
  return raw && !raw->is_null() ? value_to_string(*raw) : flight::String("");
}

inline flight::Array<flight::Ref<flight::types::TiledProperty>> properties(
    const flight::JsonObject& object) {
  flight::Array<flight::Ref<flight::types::TiledProperty>> result;
  for (const auto& entry : object_array(object, "properties")) {
    const auto type = property_type(string_value(entry, "type"));
    result.push(flight::make_ref<flight::types::TiledProperty>(flight::types::TiledProperty{
        .name = string_value(entry, "name").value_or(flight::String("")),
        .type = type,
        .value = property_value(type, value(entry, "value")),
    }));
  }
  return result;
}

struct LayerBase {
  double id;
  flight::String name;
  double opacity;
  bool visible;
  double offset_x;
  double offset_y;
  std::optional<double> tint_color;
  double parallax_x;
  double parallax_y;
  flight::String class_;
  flight::Array<flight::Ref<flight::types::TiledProperty>> properties;
};

inline LayerBase layer_base(const flight::JsonObject& object) {
  const auto tint = string_value(object, "tintcolor");
  return LayerBase{
      .id = number_value(object, "id"),
      .name = string_value(object, "name").value_or(flight::String("")),
      .opacity = number_value(object, "opacity", 1.0),
      .visible = boolean_value(object, "visible", true),
      .offset_x = number_value(object, "offsetx"),
      .offset_y = number_value(object, "offsety"),
      .tint_color = tint ? parse_tiled_color(*tint) : std::nullopt,
      .parallax_x = number_value(object, "parallaxx", 1.0),
      .parallax_y = number_value(object, "parallaxy", 1.0),
      .class_ = string_value(object, "class").value_or(flight::String("")),
      .properties = properties(object),
  };
}

template <typename Layer>
inline void apply_base(const std::shared_ptr<Layer>& layer, const LayerBase& base) {
  layer->id = base.id;
  layer->name = base.name;
  layer->opacity = base.opacity;
  layer->visible = base.visible;
  layer->offset_x = base.offset_x;
  layer->offset_y = base.offset_y;
  layer->tint_color = base.tint_color;
  layer->parallax_x = base.parallax_x;
  layer->parallax_y = base.parallax_y;
  layer->class_ = base.class_;
  layer->properties = base.properties;
}

inline flight::Array<flight::types::Vector2Like> points(
    const flight::JsonObject& object,
    const char* key) {
  flight::Array<flight::types::Vector2Like> result;
  for (const auto& point : object_array(object, key)) {
    result.push(flight::make_ref<flight::types::Vector2>(flight::types::Vector2{
        .entity_runtime_key = std::nullopt,
        .x = number_value(point, "x"),
        .y = number_value(point, "y"),
    }));
  }
  return result;
}

inline flight::Ref<flight::types::TiledObject> object(const flight::JsonObject& source) {
  const auto gid = value(source, "gid");
  const auto polygon_value = value(source, "polygon");
  const auto polyline_value = value(source, "polyline");
  return flight::make_ref<flight::types::TiledObject>(flight::types::TiledObject{
      .id = number_value(source, "id"),
      .name = string_value(source, "name").value_or(flight::String("")),
      .type = string_value(source, "type")
                  .value_or(string_value(source, "class").value_or(flight::String(""))),
      .x = number_value(source, "x"),
      .y = number_value(source, "y"),
      .width = number_value(source, "width"),
      .height = number_value(source, "height"),
      .gid = gid && gid->kind() == flight::JsonValue::Kind::number
                 ? std::optional<double>{flight::unsigned_right_shift(gid->as_number(), 0.0)}
                 : std::nullopt,
      .point = boolean_value(source, "point"),
      .ellipse = boolean_value(source, "ellipse"),
      .polygon = polygon_value && polygon_value->kind() == flight::JsonValue::Kind::array
                     ? std::optional{points(source, "polygon")}
                     : std::nullopt,
      .polyline = polyline_value && polyline_value->kind() == flight::JsonValue::Kind::array
                      ? std::optional{points(source, "polyline")}
                      : std::nullopt,
      .rotation = number_value(source, "rotation"),
      .visible = boolean_value(source, "visible", true),
      .properties = properties(source),
  });
}

inline std::optional<flight::types::TiledInflate> inflate(
    const std::optional<JsonOptionsView>& options) {
  return options ? flight::row_get<flight::RowKey<"inflate">>(*options) : std::nullopt;
}

inline flight::Uint32Array layer_data(
    const flight::JsonObject& object,
    double width,
    double height,
    const std::optional<JsonOptionsView>& options,
    const std::optional<JsonDiagnostics>& diagnostics,
    const flight::String& path) {
  flight::Uint32Array grid(width * height);
  std::optional<flight::Uint32Array> decoded;
  const auto data = value(object, "data");
  if (data && data->kind() == flight::JsonValue::Kind::array) {
    decoded = flight::Uint32Array(static_cast<double>(data->as_array().size()));
    double index = 0.0;
    for (const auto& entry : data->as_array()) {
      decoded->set_index(
          index++, entry.kind() == flight::JsonValue::Kind::number
                       ? flight::unsigned_right_shift(entry.as_number(), 0.0)
                       : 0.0);
    }
  } else if (data && data->kind() == flight::JsonValue::Kind::string) {
    const auto encoding = string_value(object, "encoding");
    if (encoding == flight::String("csv")) {
      decoded = decode_tiled_csv_layer(data->as_string());
    } else {
      if (!encoding) {
        report_missing(diagnostics, flight::String("buildTiledLayerDataFromJson"), path,
                       flight::String("encoding"));
      } else if (encoding != flight::String("base64")) {
        report(
            diagnostics,
            flight::types::import_diagnostic_severity->recover,
            flight::String("tiled.layer-encoding-invalid"),
            flight::String("buildTiledLayerDataFromJson"),
            diagnostic_detail({
                {flight::String("encoding"), *encoding},
                {flight::String("path"), path + flight::String(".encoding")},
            }));
      }
      const auto compressed = compression(string_value(object, "compression"));
      const auto inflater = inflate(options);
      if (compressed && !inflater) {
        report(
            diagnostics,
            flight::types::import_diagnostic_severity->recover,
            flight::String("tiled.layer-inflate-unavailable"),
            flight::String("buildTiledLayerDataFromJson"),
            diagnostic_detail({
                {flight::String("compression"), *compressed},
                {flight::String("path"), path},
            }));
      }
      decoded = decode_tiled_base64_layer(data->as_string(), compressed, inflater);
    }
  } else if (!data) {
    report_missing(diagnostics, flight::String("buildTiledLayerDataFromJson"), path);
  }
  if (decoded) grid.set(decoded->subarray(0.0, static_cast<double>(grid.length)));
  return grid;
}

inline std::optional<flight::types::TiledLayer> layer(
    const flight::JsonObject& object,
    const std::optional<JsonOptionsView>& options,
    const std::optional<JsonDiagnostics>& diagnostics,
    flight::String path) {
  const auto base = layer_base(object);
  const auto type = string_value(object, "type");
  if (type == flight::String("tilelayer")) {
    report_missing_field(object, "height", diagnostics, flight::String("buildTiledLayerFromJson"), path);
    report_missing_field(object, "width", diagnostics, flight::String("buildTiledLayerFromJson"), path);
    const auto width = number_value(object, "width");
    const auto height = number_value(object, "height");
    auto result = flight::make_ref<flight::types::TiledTileLayer>();
    apply_base(result, base);
    result->type = flight::String("tilelayer");
    result->width = width;
    result->height = height;
    result->data = layer_data(
        object, width, height, options, diagnostics,
        diagnostics ? path + flight::String(".data") : flight::String(""));
    return flight::types::TiledLayer(
        std::in_place_type<flight::Ref<flight::types::TiledTileLayer>>, result);
  }
  if (type == flight::String("objectgroup")) {
    flight::Array<flight::Ref<flight::types::TiledObject>> objects;
    for (const auto& entry : object_array(object, "objects")) objects.push(tiled_json_detail::object(entry));
    auto result = flight::make_ref<flight::types::TiledObjectGroup>();
    apply_base(result, base);
    result->type = flight::String("objectgroup");
    result->objects = objects;
    return flight::types::TiledLayer(
        std::in_place_type<flight::Ref<flight::types::TiledObjectGroup>>, result);
  }
  if (type == flight::String("imagelayer")) {
    auto result = flight::make_ref<flight::types::TiledImageLayer>();
    apply_base(result, base);
    result->type = flight::String("imagelayer");
    result->image = string_value(object, "image").value_or(flight::String(""));
    result->repeat_x = boolean_value(object, "repeatx");
    result->repeat_y = boolean_value(object, "repeaty");
    return flight::types::TiledLayer(
        std::in_place_type<flight::Ref<flight::types::TiledImageLayer>>, result);
  }
  if (type == flight::String("group")) {
    flight::Array<flight::types::TiledLayer> layers;
    double index = 0.0;
    for (const auto& entry : object_array(object, "layers")) {
      auto child = layer(
          entry, options, diagnostics,
          diagnostics ? path + flight::String(".layers[") + flight::to_string(index) +
                            flight::String("]")
                      : flight::String(""));
      if (child) layers.push(*child);
      index += 1.0;
    }
    auto result = flight::make_ref<flight::types::TiledGroupLayer>();
    apply_base(result, base);
    result->type = flight::String("group");
    result->layers = layers;
    return flight::types::TiledLayer(
        std::in_place_type<flight::Ref<flight::types::TiledGroupLayer>>, result);
  }
  if (!type) {
    report(
        diagnostics,
        flight::types::import_diagnostic_severity->drop,
        flight::String("tiled.required-field-missing"),
        flight::String("buildTiledLayerFromJson"),
        diagnostic_detail({{flight::String("path"), path + flight::String(".type")}}));
  }
  return std::nullopt;
}

inline flight::Ref<flight::types::TiledTilesetTile> tileset_tile(
    const flight::JsonObject& object) {
  std::optional<flight::Array<flight::Ref<flight::types::TiledTilesetTileFrame>>> animation;
  const auto raw_animation = value(object, "animation");
  if (raw_animation && raw_animation->kind() == flight::JsonValue::Kind::array) {
    animation = flight::Array<flight::Ref<flight::types::TiledTilesetTileFrame>>{};
    for (const auto& frame : raw_animation->as_array()) {
      if (frame.kind() != flight::JsonValue::Kind::object) continue;
      animation->push(flight::make_ref<flight::types::TiledTilesetTileFrame>(
          flight::types::TiledTilesetTileFrame{
              .tile_id = number_value(frame.as_object(), "tileid"),
              .duration = number_value(frame.as_object(), "duration"),
          }));
    }
  }
  std::optional<flight::Array<flight::Ref<flight::types::TiledObject>>> objects;
  const auto object_group = value(object, "objectgroup");
  if (object_group && object_group->kind() == flight::JsonValue::Kind::object) {
    objects = flight::Array<flight::Ref<flight::types::TiledObject>>{};
    for (const auto& entry : object_array(object_group->as_object(), "objects")) {
      objects->push(tiled_json_detail::object(entry));
    }
  }
  return flight::make_ref<flight::types::TiledTilesetTile>(flight::types::TiledTilesetTile{
      .id = number_value(object, "id"),
      .type = string_value(object, "type")
                  .value_or(string_value(object, "class").value_or(flight::String(""))),
      .properties = properties(object),
      .animation = animation,
      .objects = objects,
      .image = string_value(object, "image"),
  });
}

inline flight::Ref<flight::types::TiledTileset> tileset(
    const flight::JsonObject& object,
    const std::optional<JsonDiagnostics>& diagnostics,
    flight::String path) {
  report_missing_field(object, "name", diagnostics, flight::String("buildTiledTilesetFromJson"), path);
  report_missing_field(object, "tileheight", diagnostics, flight::String("buildTiledTilesetFromJson"), path);
  report_missing_field(object, "tilewidth", diagnostics, flight::String("buildTiledTilesetFromJson"), path);
  std::optional<flight::JsonObject> tile_offset;
  const auto raw_offset = value(object, "tileoffset");
  if (raw_offset && raw_offset->kind() == flight::JsonValue::Kind::object) {
    tile_offset = raw_offset->as_object();
  }
  flight::Array<flight::Ref<flight::types::TiledTilesetTile>> tiles;
  for (const auto& entry : object_array(object, "tiles")) tiles.push(tileset_tile(entry));
  return flight::make_ref<flight::types::TiledTileset>(flight::types::TiledTileset{
      .name = string_value(object, "name").value_or(flight::String("")),
      .tile_width = number_value(object, "tilewidth"),
      .tile_height = number_value(object, "tileheight"),
      .tile_count = number_value(object, "tilecount"),
      .columns = number_value(object, "columns"),
      .image = string_value(object, "image"),
      .image_width = number_value(object, "imagewidth"),
      .image_height = number_value(object, "imageheight"),
      .margin = number_value(object, "margin"),
      .spacing = number_value(object, "spacing"),
      .tile_offset_x = tile_offset ? number_value(*tile_offset, "x") : 0.0,
      .tile_offset_y = tile_offset ? number_value(*tile_offset, "y") : 0.0,
      .object_alignment = object_alignment(string_value(object, "objectalignment")),
      .tiles = tiles,
      .properties = properties(object),
  });
}

inline flight::Ref<flight::types::TiledTilesetRef> tileset_ref(
    const flight::JsonObject& object,
    const std::optional<JsonDiagnostics>& diagnostics,
    flight::String path) {
  const auto source = string_value(object, "source");
  return flight::make_ref<flight::types::TiledTilesetRef>(flight::types::TiledTilesetRef{
      .first_gid = number_value(object, "firstgid", 1.0),
      .source = source,
      .tileset = source ? std::nullopt : std::optional{tileset(object, diagnostics, path)},
  });
}

} // namespace tiled_json_detail

inline std::optional<flight::Ref<flight::types::TiledTileset>> parse_tiled_tileset_json(
    flight::String text,
    std::optional<JsonOptionsView> = std::nullopt,
    std::optional<JsonDiagnostics> diagnostics = std::nullopt) {
  const auto root = tiled_json_detail::parse_object(text);
  if (!root) {
    tiled_json_detail::report(
        diagnostics,
        flight::types::import_diagnostic_severity->reject,
        flight::String("tiled.json-malformed"),
        flight::String("parseTiledTilesetJson"),
        tiled_json_detail::diagnostic_detail(
            {{flight::String("path"), flight::String("tileset")}}));
    return std::nullopt;
  }
  return tiled_json_detail::tileset(*root, diagnostics, flight::String("tileset"));
}

inline std::optional<flight::Ref<flight::types::TiledMap>> parse_tiled_tmj(
    flight::String text,
    std::optional<JsonOptionsView> options = std::nullopt,
    std::optional<JsonDiagnostics> diagnostics = std::nullopt) {
  const auto root = tiled_json_detail::parse_object(text);
  if (!root) {
    tiled_json_detail::report(
        diagnostics,
        flight::types::import_diagnostic_severity->reject,
        flight::String("tiled.json-malformed"),
        flight::String("parseTiledTmj"),
        tiled_json_detail::diagnostic_detail({{flight::String("path"), flight::String("map")}}));
    return std::nullopt;
  }
  tiled_json_detail::report_missing_field(
      *root, "height", diagnostics, flight::String("parseTiledTmj"), flight::String("map"));
  tiled_json_detail::report_missing_field(
      *root, "tileheight", diagnostics, flight::String("parseTiledTmj"), flight::String("map"));
  tiled_json_detail::report_missing_field(
      *root, "tilewidth", diagnostics, flight::String("parseTiledTmj"), flight::String("map"));
  tiled_json_detail::report_missing_field(
      *root, "width", diagnostics, flight::String("parseTiledTmj"), flight::String("map"));

  flight::Array<flight::types::TiledLayer> layers;
  double layer_index = 0.0;
  for (const auto& entry : tiled_json_detail::object_array(*root, "layers")) {
    auto layer = tiled_json_detail::layer(
        entry, options, diagnostics,
        diagnostics ? flight::String("map.layers[") + flight::to_string(layer_index) +
                          flight::String("]")
                    : flight::String(""));
    if (layer) layers.push(*layer);
    layer_index += 1.0;
  }
  flight::Array<flight::Ref<flight::types::TiledTilesetRef>> tilesets;
  double tileset_index = 0.0;
  for (const auto& entry : tiled_json_detail::object_array(*root, "tilesets")) {
    tilesets.push(tiled_json_detail::tileset_ref(
        entry, diagnostics,
        diagnostics ? flight::String("map.tilesets[") + flight::to_string(tileset_index) +
                          flight::String("]")
                    : flight::String("")));
    tileset_index += 1.0;
  }
  const auto background = tiled_json_detail::string_value(*root, "backgroundcolor");
  const auto hex_side = tiled_json_detail::value(*root, "hexsidelength");
  const auto version = tiled_json_detail::value(*root, "version");
  flight::String version_string("1.0");
  if (version && version->kind() == flight::JsonValue::Kind::string) {
    version_string = version->as_string();
  } else if (version && version->kind() == flight::JsonValue::Kind::number) {
    version_string = flight::String::from_number(version->as_number());
  }
  return flight::make_ref<flight::types::TiledMap>(flight::types::TiledMap{
      .version = version_string,
      .tiled_version = tiled_json_detail::string_value(*root, "tiledversion"),
      .orientation = tiled_json_detail::orientation(
          tiled_json_detail::string_value(*root, "orientation")),
      .render_order = tiled_json_detail::render_order(
          tiled_json_detail::string_value(*root, "renderorder")),
      .width = tiled_json_detail::number_value(*root, "width"),
      .height = tiled_json_detail::number_value(*root, "height"),
      .tile_width = tiled_json_detail::number_value(*root, "tilewidth"),
      .tile_height = tiled_json_detail::number_value(*root, "tileheight"),
      .infinite = tiled_json_detail::boolean_value(*root, "infinite"),
      .background_color = background ? parse_tiled_color(*background) : std::nullopt,
      .stagger_axis = tiled_json_detail::stagger_axis(
          tiled_json_detail::string_value(*root, "staggeraxis")),
      .stagger_index = tiled_json_detail::stagger_index(
          tiled_json_detail::string_value(*root, "staggerindex")),
      .hex_side_length = hex_side && hex_side->kind() == flight::JsonValue::Kind::number
                             ? std::optional<double>{hex_side->as_number()}
                             : std::nullopt,
      .layers = layers,
      .tilesets = tilesets,
      .properties = tiled_json_detail::properties(*root),
  });
}

} // namespace flight::tilemap_formats
