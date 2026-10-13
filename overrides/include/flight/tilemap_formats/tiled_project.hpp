// Native completion of the a627849 Tiled-to-Tilemap projection.
#pragma once

#include <cmath>
#include <map>
#include <optional>
#include <set>
#include <variant>
#include <vector>

#include <flight/importdiagnostics/import_diagnostic_collector.hpp>
#include <flight/structural_ref.hpp>
#include <flight/tilemap/tilemap.hpp>
#include <flight/tilemap_formats/tiled_gid.hpp>
#include <flight/types/import_diagnostic.hpp>
#include <flight/types/tiled_map.hpp>
#include <flight/types/tiled_parse_options.hpp>
#include <flight/types/tilemap.hpp>

namespace flight::tilemap_formats {

using ProjectMapView = flight::StructuralRef<
    flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::TiledMap>>>>;
using ProjectTilesetView = flight::StructuralRef<
    flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::TiledTilesetRef>>>>;
using ProjectDiagnostics = flight::Array<flight::Ref<flight::types::ImportDiagnostic>>;

namespace tiled_project_detail {

struct Group {
  double first_gid;
  flight::Ref<flight::types::TilemapData> layout;
  flight::Int16Array tiles;
};

inline void report(
    const std::optional<ProjectDiagnostics>& diagnostics,
    flight::String kind,
    double cells,
    double layer_index,
    std::optional<double> tilesets = std::nullopt) {
  flight::Record<flight::String, std::variant<bool, double, flight::String>> detail;
  detail.set(flight::String("cells"), cells);
  detail.set(flight::String("layerIndex"), layer_index);
  if (tilesets) detail.set(flight::String("tilesets"), *tilesets);
  flight::importdiagnostics::report_import_diagnostic(
      diagnostics,
      flight::types::import_diagnostic_severity->drop,
      kind,
      flight::String("buildTilemapLayersFromTiled"),
      detail);
}

} // namespace tiled_project_detail

inline std::optional<flight::Array<flight::Ref<flight::types::TilemapData>>>
build_tilemap_layers_from_tiled(
    ProjectMapView map,
    double layer_index,
    flight::types::TiledTilesetResolver resolve_tileset,
    std::optional<ProjectDiagnostics> diagnostics = std::nullopt) {
  if (!std::isfinite(layer_index) || std::floor(layer_index) != layer_index || layer_index < 0.0 ||
      layer_index >= static_cast<double>(flight::row_get<flight::RowKey<"layers">>(map).length)) {
    return std::nullopt;
  }
  const auto& layer = flight::row_get<flight::RowKey<"layers">>(map).element(layer_index);
  const auto* tile_layer = std::get_if<flight::Ref<flight::types::TiledTileLayer>>(&layer);
  if (!tile_layer) return std::nullopt;

  const double width = (*tile_layer)->width;
  const double height = (*tile_layer)->height;
  const double cell_count = width * height;
  std::vector<tiled_project_detail::Group> groups;
  std::map<double, std::optional<std::size_t>> by_first_gid;
  bool any_resolved = false;
  double cells_without_tileset = 0.0;
  double cells_left_empty = 0.0;
  std::set<double> unresolved_tilesets;

  for (double index = 0.0; index < cell_count; index += 1.0) {
    const auto gid = decode_tiled_gid((*tile_layer)->data.get_index(index));
    if (gid->tile_id <= 0.0) continue;
    const auto ref = get_tiled_tileset_ref_for_gid(map, gid->tile_id);
    if (!ref) {
      cells_without_tileset += 1.0;
      continue;
    }

    auto found = by_first_gid.find((*ref)->first_gid);
    if (found == by_first_gid.end()) {
      const auto layout = resolve_tileset(ProjectTilesetView(*ref));
      if (!layout) {
        found = by_first_gid.emplace((*ref)->first_gid, std::nullopt).first;
      } else {
        any_resolved = true;
        const auto group_index = groups.size();
        groups.push_back(tiled_project_detail::Group{
            .first_gid = (*ref)->first_gid,
            .layout = *layout,
            .tiles = flight::Int16Array(cell_count).fill(-1.0),
        });
        found = by_first_gid.emplace((*ref)->first_gid, group_index).first;
      }
    }
    if (!found->second) {
      cells_left_empty += 1.0;
      unresolved_tilesets.insert((*ref)->first_gid);
      continue;
    }
    auto& group = groups[*found->second];
    group.tiles.set_index(index, gid->tile_id - group.first_gid);
  }

  if (cells_without_tileset > 0.0) {
    tiled_project_detail::report(
        diagnostics,
        flight::String("tiled.tile-outside-every-tileset"),
        cells_without_tileset,
        layer_index);
  }
  if (cells_left_empty > 0.0) {
    tiled_project_detail::report(
        diagnostics,
        flight::String("tiled.tileset-unresolved"),
        cells_left_empty,
        layer_index,
        static_cast<double>(unresolved_tilesets.size()));
  }
  if (!any_resolved) return std::nullopt;

  flight::Array<flight::Ref<flight::types::TilemapData>> result;
  for (const auto& group : groups) {
    const auto partial = flight::make_ref<flight::types::TilemapData>(flight::types::TilemapData{
        .entity_runtime_key = std::nullopt,
        .atlas = group.layout->atlas,
        .columns = width,
        .rows = height,
        .tile_height = group.layout->tile_height,
        .tile_width = group.layout->tile_width,
        .tiles = group.tiles,
        .material_data = std::nullopt,
    });
    result.push(flight::tilemap::create_tilemap_data(flight::tilemap::PartialTilemapData(partial)));
  }
  return result;
}

} // namespace flight::tilemap_formats
