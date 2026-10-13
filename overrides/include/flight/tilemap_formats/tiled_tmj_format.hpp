// Native completion of the a627849 TMJ/TSJ formatter over flight::JsonObject.
#pragma once

#include <algorithm>
#include <optional>
#include <stdexcept>
#include <variant>

#include <flight/json.hpp>
#include <flight/structural_ref.hpp>
#include <flight/tilemap_formats/tiled_color.hpp>
#include <flight/types/tiled_layer.hpp>
#include <flight/types/tiled_map.hpp>
#include <flight/types/tiled_object.hpp>
#include <flight/types/tiled_property.hpp>
#include <flight/types/tiled_tileset.hpp>

namespace flight::tilemap_formats {

namespace tiled_tmj_detail {

inline flight::JsonValue property_value(
    const std::variant<bool, double, flight::String>& value) {
  return std::visit([](const auto& entry) { return flight::JsonValue(entry); }, value);
}

inline flight::JsonObject property(const flight::Ref<flight::types::TiledProperty>& source) {
  flight::JsonObject result;
  result.set(flight::String("name"), source->name);
  result.set(flight::String("type"), source->type);
  result.set(flight::String("value"), property_value(source->value));
  return result;
}

inline flight::JsonArray properties(
    const flight::Array<flight::Ref<flight::types::TiledProperty>>& source) {
  flight::JsonArray result;
  for (const auto& entry : source) result.push(property(entry));
  return result;
}

inline flight::JsonArray points(const flight::Array<flight::types::Vector2Like>& source) {
  flight::JsonArray result;
  for (const auto& point : source) {
    flight::JsonObject value;
    value.set(flight::String("x"), point->x);
    value.set(flight::String("y"), point->y);
    result.push(value);
  }
  return result;
}

inline flight::JsonObject object(const flight::Ref<flight::types::TiledObject>& source) {
  flight::JsonObject result;
  result.set(flight::String("height"), source->height);
  result.set(flight::String("id"), source->id);
  result.set(flight::String("name"), source->name);
  result.set(flight::String("rotation"), source->rotation);
  result.set(flight::String("type"), source->type);
  result.set(flight::String("visible"), source->visible);
  result.set(flight::String("width"), source->width);
  result.set(flight::String("x"), source->x);
  result.set(flight::String("y"), source->y);
  if (source->gid) result.set(flight::String("gid"), *source->gid);
  if (source->point) result.set(flight::String("point"), true);
  if (source->ellipse) result.set(flight::String("ellipse"), true);
  if (source->polygon) result.set(flight::String("polygon"), points(*source->polygon));
  if (source->polyline) result.set(flight::String("polyline"), points(*source->polyline));
  if (source->properties.length > 0) {
    result.set(flight::String("properties"), properties(source->properties));
  }
  return result;
}

inline flight::JsonArray objects(
    const flight::Array<flight::Ref<flight::types::TiledObject>>& source) {
  flight::JsonArray result;
  for (const auto& entry : source) result.push(object(entry));
  return result;
}

inline flight::JsonObject layer(const flight::types::TiledLayer& source);

inline flight::JsonArray layers(const flight::Array<flight::types::TiledLayer>& source) {
  flight::JsonArray result;
  for (const auto& entry : source) result.push(layer(entry));
  return result;
}

inline flight::JsonObject layer(const flight::types::TiledLayer& source) {
  flight::JsonObject result;
  std::visit(
      [&](const auto& concrete) {
        result.set(flight::String("id"), concrete->id);
        result.set(flight::String("name"), concrete->name);
        result.set(flight::String("opacity"), concrete->opacity);
        result.set(flight::String("type"), concrete->type);
        result.set(flight::String("visible"), concrete->visible);
        result.set(flight::String("x"), 0.0);
        result.set(flight::String("y"), 0.0);
        if (concrete->offset_x != 0.0) result.set(flight::String("offsetx"), concrete->offset_x);
        if (concrete->offset_y != 0.0) result.set(flight::String("offsety"), concrete->offset_y);
        if (concrete->tint_color) {
          result.set(flight::String("tintcolor"), format_tiled_color(*concrete->tint_color));
        }
        if (concrete->parallax_x != 1.0) {
          result.set(flight::String("parallaxx"), concrete->parallax_x);
        }
        if (concrete->parallax_y != 1.0) {
          result.set(flight::String("parallaxy"), concrete->parallax_y);
        }
        if (concrete->class_ != flight::String("")) {
          result.set(flight::String("class"), concrete->class_);
        }
        if (concrete->properties.length > 0) {
          result.set(flight::String("properties"), properties(concrete->properties));
        }
      },
      source);

  if (const auto* concrete = std::get_if<flight::Ref<flight::types::TiledTileLayer>>(&source)) {
    flight::JsonArray data;
    for (double index = 0.0; index < static_cast<double>((*concrete)->data.length); index += 1.0) {
      data.push((*concrete)->data.get_index(index));
    }
    result.set(flight::String("data"), data);
    result.set(flight::String("height"), (*concrete)->height);
    result.set(flight::String("width"), (*concrete)->width);
  } else if (const auto* concrete =
                 std::get_if<flight::Ref<flight::types::TiledObjectGroup>>(&source)) {
    result.set(flight::String("objects"), objects((*concrete)->objects));
  } else if (const auto* concrete =
                 std::get_if<flight::Ref<flight::types::TiledImageLayer>>(&source)) {
    result.set(flight::String("image"), (*concrete)->image);
    if ((*concrete)->repeat_x) result.set(flight::String("repeatx"), true);
    if ((*concrete)->repeat_y) result.set(flight::String("repeaty"), true);
  } else {
    result.set(
        flight::String("layers"),
        layers(std::get<flight::Ref<flight::types::TiledGroupLayer>>(source)->layers));
  }
  return result;
}

inline flight::JsonObject tileset_tile(
    const flight::Ref<flight::types::TiledTilesetTile>& source) {
  flight::JsonObject result;
  result.set(flight::String("id"), source->id);
  if (source->type != flight::String("")) result.set(flight::String("type"), source->type);
  if (source->image) result.set(flight::String("image"), *source->image);
  if (source->animation) {
    flight::JsonArray animation;
    for (const auto& frame : *source->animation) {
      flight::JsonObject value;
      value.set(flight::String("duration"), frame->duration);
      value.set(flight::String("tileid"), frame->tile_id);
      animation.push(value);
    }
    result.set(flight::String("animation"), animation);
  }
  if (source->objects) {
    flight::JsonObject group;
    group.set(flight::String("draworder"), flight::String("index"));
    group.set(flight::String("objects"), objects(*source->objects));
    group.set(flight::String("type"), flight::String("objectgroup"));
    result.set(flight::String("objectgroup"), group);
  }
  if (source->properties.length > 0) {
    result.set(flight::String("properties"), properties(source->properties));
  }
  return result;
}

inline flight::JsonObject tileset(const flight::Ref<flight::types::TiledTileset>& source) {
  flight::JsonObject result;
  result.set(flight::String("columns"), source->columns);
  result.set(flight::String("margin"), source->margin);
  result.set(flight::String("name"), source->name);
  result.set(flight::String("spacing"), source->spacing);
  result.set(flight::String("tilecount"), source->tile_count);
  result.set(flight::String("tileheight"), source->tile_height);
  result.set(flight::String("tilewidth"), source->tile_width);
  if (source->image) {
    result.set(flight::String("image"), *source->image);
    result.set(flight::String("imageheight"), source->image_height);
    result.set(flight::String("imagewidth"), source->image_width);
  }
  if (source->tile_offset_x != 0.0 || source->tile_offset_y != 0.0) {
    flight::JsonObject offset;
    offset.set(flight::String("x"), source->tile_offset_x);
    offset.set(flight::String("y"), source->tile_offset_y);
    result.set(flight::String("tileoffset"), offset);
  }
  if (source->object_alignment != flight::String("unspecified")) {
    result.set(flight::String("objectalignment"), source->object_alignment);
  }
  if (source->tiles.length > 0) {
    flight::JsonArray entries;
    for (const auto& tile : source->tiles) entries.push(tileset_tile(tile));
    result.set(flight::String("tiles"), entries);
  }
  if (source->properties.length > 0) {
    result.set(flight::String("properties"), properties(source->properties));
  }
  return result;
}

inline flight::JsonObject tileset_ref(
    const flight::Ref<flight::types::TiledTilesetRef>& source) {
  if (source->source) {
    flight::JsonObject result;
    result.set(flight::String("firstgid"), source->first_gid);
    result.set(flight::String("source"), *source->source);
    return result;
  }
  if (!source->tileset) {
    flight::JsonObject result;
    result.set(flight::String("firstgid"), source->first_gid);
    return result;
  }
  auto result = tileset(*source->tileset);
  flight::JsonObject with_first_gid;
  with_first_gid.set(flight::String("firstgid"), source->first_gid);
  for (const auto& [key, value] : result) with_first_gid.set(key, value);
  return with_first_gid;
}

inline double next_layer_id(const flight::Array<flight::types::TiledLayer>& source) {
  double maximum = 0.0;
  for (const auto& entry : source) {
    std::visit([&](const auto& concrete) { maximum = std::max(maximum, concrete->id); }, entry);
    if (const auto* group = std::get_if<flight::Ref<flight::types::TiledGroupLayer>>(&entry)) {
      maximum = std::max(maximum, next_layer_id((*group)->layers) - 1.0);
    }
  }
  return maximum + 1.0;
}

inline double next_object_id(const flight::Array<flight::types::TiledLayer>& source) {
  double maximum = 0.0;
  for (const auto& entry : source) {
    if (const auto* group = std::get_if<flight::Ref<flight::types::TiledObjectGroup>>(&entry)) {
      for (const auto& object : (*group)->objects) maximum = std::max(maximum, object->id);
    }
    if (const auto* group = std::get_if<flight::Ref<flight::types::TiledGroupLayer>>(&entry)) {
      maximum = std::max(maximum, next_object_id((*group)->layers) - 1.0);
    }
  }
  return maximum + 1.0;
}

template <typename Type, typename Schema>
inline flight::Ref<Type> exact(const flight::StructuralRef<Schema>& source) {
  auto result = source.template shared_exact_object<Type>();
  if (!result) throw std::bad_cast();
  return result;
}

} // namespace tiled_tmj_detail

inline flight::String format_tiled_tileset_json(
    flight::StructuralRef<flight::RowReadonly<
        flight::RowOf<flight::Ref<flight::types::TiledTileset>>>> source) {
  auto value = tiled_tmj_detail::tileset(
      tiled_tmj_detail::exact<flight::types::TiledTileset>(source));
  value.set(flight::String("type"), flight::String("tileset"));
  return flight::Json::stringify(value, nullptr, 2.0);
}

inline flight::String format_tiled_tmj(
    flight::StructuralRef<flight::RowReadonly<
        flight::RowOf<flight::Ref<flight::types::TiledMap>>>> source) {
  const auto map = tiled_tmj_detail::exact<flight::types::TiledMap>(source);
  flight::JsonObject result;
  result.set(flight::String("compressionlevel"), -1.0);
  result.set(flight::String("height"), map->height);
  result.set(flight::String("infinite"), map->infinite);
  result.set(flight::String("layers"), tiled_tmj_detail::layers(map->layers));
  result.set(flight::String("nextlayerid"), tiled_tmj_detail::next_layer_id(map->layers));
  result.set(flight::String("nextobjectid"), tiled_tmj_detail::next_object_id(map->layers));
  result.set(flight::String("orientation"), map->orientation);
  result.set(flight::String("renderorder"), map->render_order);
  if (map->tiled_version) result.set(flight::String("tiledversion"), *map->tiled_version);
  result.set(flight::String("tileheight"), map->tile_height);
  flight::JsonArray tilesets;
  for (const auto& entry : map->tilesets) tilesets.push(tiled_tmj_detail::tileset_ref(entry));
  result.set(flight::String("tilesets"), tilesets);
  result.set(flight::String("tilewidth"), map->tile_width);
  result.set(flight::String("type"), flight::String("map"));
  result.set(flight::String("version"), map->version);
  result.set(flight::String("width"), map->width);
  if (map->background_color) {
    result.set(flight::String("backgroundcolor"), format_tiled_color(*map->background_color));
  }
  if (map->stagger_axis) result.set(flight::String("staggeraxis"), *map->stagger_axis);
  if (map->stagger_index) result.set(flight::String("staggerindex"), *map->stagger_index);
  if (map->hex_side_length) result.set(flight::String("hexsidelength"), *map->hex_side_length);
  if (map->properties.length > 0) {
    result.set(flight::String("properties"), tiled_tmj_detail::properties(map->properties));
  }
  return flight::Json::stringify(result, nullptr, 2.0);
}

} // namespace flight::tilemap_formats
