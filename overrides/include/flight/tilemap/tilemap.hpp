// Runtime completion of the a627849 generated partial tilemap module.
#pragma once

#include <cmath>
#include <optional>
#include <stdexcept>
#include <utility>

#include <flight/entity/entity.hpp>
#include <flight/scene2d/display_object.hpp>
#include <flight/sequence_view.hpp>
#include <flight/signals/signal.hpp>
#include <flight/structural_ref.hpp>
#include <flight/types/material.hpp>
#include <flight/types/rectangle.hpp>
#include <flight/types/tilemap.hpp>
#include <flight/types/tilemap_signals.hpp>
#include <flight/types/tint_material_data.hpp>
#include <flight/types/vector2.hpp>
#include <flight/weak_map.hpp>

namespace flight::tilemap {

using flight::types::MaterialData;
using flight::types::Rectangle;
using flight::types::Tilemap;
using flight::types::TilemapData;
using flight::types::TilemapRuntime;
using flight::types::TilemapSignals;
using flight::types::TintMaterialData;
using flight::types::Vector2Like;

using ReadonlyTilemap =
    flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<Tilemap>>>>;
using ReadonlyVector2 =
    flight::StructuralRef<flight::RowReadonly<flight::RowOf<Vector2Like>>>;
using PartialTilemapData = flight::StructuralRef<
    flight::RowReadonly<flight::RowPartial<flight::RowOf<flight::Ref<TilemapData>>>>>;
using PartialTilemap = flight::StructuralRef<
    flight::RowReadonly<flight::RowPartial<flight::RowOf<flight::Ref<Tilemap>>>>>;

inline flight::WeakMap<flight::Ref<Tilemap>, flight::Ref<TilemapRuntime>> tilemap_runtimes;
inline flight::WeakMap<flight::Ref<Tilemap>, flight::Ref<TilemapSignals>> tilemap_signals;

namespace detail {

inline flight::Ref<Tilemap> exact_tilemap(const flight::Ref<Tilemap>& source) {
  return source;
}

template <typename Schema>
inline flight::Ref<Tilemap> exact_tilemap(const flight::StructuralRef<Schema>& source) {
  auto exact = source.template shared_exact_object<Tilemap>();
  if (!exact) throw std::bad_cast();
  return exact;
}

inline flight::Ref<MaterialData> create_tint_material_data(double rgba) {
  auto value = flight::make_ref<MaterialData>();
  value->entity_runtime_key = std::nullopt;

  // TintMaterialData extends MaterialData in TypeScript, but those interfaces are emitted as
  // independent owners. Keep one MaterialData object in the declared array and attach the refined
  // field to its structural owner so a TintMaterialData row view observes the same identity.
  auto base = flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<MaterialData>>>>(
      value);
  using TintView =
      flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<TintMaterialData>>>>;
  auto tint = TintView::from_owner(base.shared_owner());
  flight::row_set<flight::RowKey<"tint">>(tint, flight::unsigned_right_shift(rgba, 0.0));
  return value;
}

} // namespace detail

template <typename Source>
inline void compute_tilemap_local_bounds_rectangle(
    flight::Ref<Rectangle> out,
    const Source& source) {
  const auto tilemap = detail::exact_tilemap(source);
  const auto& data = tilemap->data;
  out->x = 0.0;
  out->y = 0.0;
  out->width = data->atlas.has_value() ? data->columns * data->tile_width : 0.0;
  out->height = data->atlas.has_value() ? data->rows * data->tile_height : 0.0;
}

inline void fill_tilemap_tiles(flight::Ref<Tilemap> tilemap, double id) {
  tilemap->data->tiles.fill(id);
}

inline double get_tilemap_column_at_x(ReadonlyTilemap source, double x) {
  const auto data = flight::row_get<flight::RowKey<"data">>(source);
  if (!data->atlas.has_value() || data->tile_width <= 0.0) return -1.0;
  const double column = std::floor(x / data->tile_width);
  return column < 0.0 || column >= data->columns ? -1.0 : column;
}

inline double get_tilemap_row_at_y(ReadonlyTilemap source, double y) {
  const auto data = flight::row_get<flight::RowKey<"data">>(source);
  if (!data->atlas.has_value() || data->tile_height <= 0.0) return -1.0;
  const double row = std::floor(y / data->tile_height);
  return row < 0.0 || row >= data->rows ? -1.0 : row;
}

inline bool get_tilemap_column_row_at_point(
    Vector2Like out,
    ReadonlyTilemap source,
    double x,
    double y) {
  const double column = get_tilemap_column_at_x(source, x);
  const double row = get_tilemap_row_at_y(source, y);
  if (column < 0.0 || row < 0.0) return false;
  out->x = column;
  out->y = row;
  return true;
}

inline double get_tilemap_tile(ReadonlyTilemap tilemap, double column, double row) {
  const auto data = flight::row_get<flight::RowKey<"data">>(tilemap);
  if (column < 0.0 || column >= data->columns || row < 0.0 || row >= data->rows) {
    return -1.0;
  }
  return data->tiles.get_index(row * data->columns + column);
}

inline double get_tilemap_tile_at_point_xy(
    ReadonlyTilemap source,
    double x,
    double y) {
  const double column = get_tilemap_column_at_x(source, x);
  const double row = get_tilemap_row_at_y(source, y);
  if (column < 0.0 || row < 0.0) return -1.0;
  return get_tilemap_tile(source, column, row);
}

inline double get_tilemap_tile_at_point(
    ReadonlyTilemap source,
    ReadonlyVector2 point) {
  return get_tilemap_tile_at_point_xy(
      source,
      flight::row_get<flight::RowKey<"x">>(point),
      flight::row_get<flight::RowKey<"y">>(point));
}

inline bool get_tilemap_tile_rect(
    flight::Ref<Rectangle> out,
    ReadonlyTilemap source,
    double column,
    double row) {
  const auto data = flight::row_get<flight::RowKey<"data">>(source);
  if (!data->atlas.has_value() || column < 0.0 || column >= data->columns ||
      row < 0.0 || row >= data->rows) {
    return false;
  }
  out->x = column * data->tile_width;
  out->y = row * data->tile_height;
  out->width = data->tile_width;
  out->height = data->tile_height;
  return true;
}

inline void initialize_tilemap_data(
    flight::types::EntityConstruction<flight::Ref<TilemapData>> out,
    std::optional<PartialTilemapData> data = std::nullopt) {
  const auto columns = data.has_value()
                           ? flight::row_get<flight::RowKey<"columns">>(data.value())
                           : std::optional<double>{};
  const auto rows = data.has_value()
                        ? flight::row_get<flight::RowKey<"rows">>(data.value())
                        : std::optional<double>{};
  const double column_count = columns.value_or(0.0);
  const double row_count = rows.value_or(0.0);

  flight::row_set<flight::RowKey<"atlas">>(
      out,
      data.has_value() ? flight::row_get<flight::RowKey<"atlas">>(data.value())
                       : std::optional<flight::Ref<flight::types::TextureAtlas>>{});
  flight::row_set<flight::RowKey<"columns">>(out, column_count);
  flight::row_set<flight::RowKey<"materialData">>(
      out,
      data.has_value()
          ? flight::row_get<flight::RowKey<"materialData">>(data.value())
          : std::optional<flight::Array<std::optional<flight::Ref<MaterialData>>>>{});
  flight::row_set<flight::RowKey<"rows">>(out, row_count);
  flight::row_set<flight::RowKey<"tileHeight">>(
      out,
      data.has_value()
          ? flight::row_get<flight::RowKey<"tileHeight">>(data.value()).value_or(0.0)
          : 0.0);
  flight::row_set<flight::RowKey<"tileWidth">>(
      out,
      data.has_value()
          ? flight::row_get<flight::RowKey<"tileWidth">>(data.value()).value_or(0.0)
          : 0.0);
  flight::row_set<flight::RowKey<"tiles">>(
      out,
      data.has_value()
          ? flight::row_get<flight::RowKey<"tiles">>(data.value())
                .value_or(flight::Int16Array(column_count * row_count).fill(-1.0))
          : flight::Int16Array(column_count * row_count).fill(-1.0));
}

inline flight::Ref<TilemapData> create_tilemap_data(
    std::optional<PartialTilemapData> data = std::nullopt) {
  auto out = flight::entity::allocate_entity<flight::Ref<TilemapData>>();
  initialize_tilemap_data(out, std::move(data));
  return flight::entity::finish_entity<flight::Ref<TilemapData>>(out);
}

inline void initialize_tilemap_signals(
    flight::types::EntityConstruction<flight::Ref<TilemapSignals>> out) {
  flight::row_set<flight::RowKey<"onCleared">>(
      out, flight::signals::create_signal<std::function<void()>>());
  flight::row_set<flight::RowKey<"onTileChanged">>(
      out, flight::signals::create_signal<std::function<void(double, double, double)>>());
  flight::row_set<flight::RowKey<"onTilesChanged">>(
      out, flight::signals::create_signal<std::function<void(double, double, double, double)>>());
}

inline flight::Ref<TilemapSignals> create_tilemap_signals() {
  auto out = flight::entity::allocate_entity<flight::Ref<TilemapSignals>>();
  initialize_tilemap_signals(out);
  return flight::entity::finish_entity<flight::Ref<TilemapSignals>>(out);
}

inline flight::Ref<TilemapRuntime> create_tilemap_runtime() {
  const auto base = flight::scene2d::create_node_2d_runtime();
  auto runtime = flight::make_ref<TilemapRuntime>();
  runtime->binding = base->binding;
  runtime->uid = base->uid;
  runtime->color_adjustments = base->color_adjustments;
  runtime->color_adjustments_unsupported = base->color_adjustments_unsupported;
  runtime->resolved_color_matrix = base->resolved_color_matrix;
  runtime->resolved_color_scale_bias = base->resolved_color_scale_bias;
  runtime->appearance_id = base->appearance_id;
  runtime->bounds_using_local_bounds_id = base->bounds_using_local_bounds_id;
  runtime->bounds_using_local_transform_id = base->bounds_using_local_transform_id;
  runtime->can_add_child = base->can_add_child;
  runtime->children = base->children;
  runtime->children_id = base->children_id;
  runtime->traits = base->traits;
  runtime->interaction_signals = base->interaction_signals;
  runtime->local_bounds_id = base->local_bounds_id;
  runtime->local_bounds_using_local_bounds_id = base->local_bounds_using_local_bounds_id;
  runtime->local_content_id = base->local_content_id;
  runtime->local_transform_id = base->local_transform_id;
  runtime->local_transform_using_local_transform_id = base->local_transform_using_local_transform_id;
  runtime->node_signals = base->node_signals;
  runtime->interaction_state = base->interaction_state;
  runtime->parent = base->parent;
  runtime->parent_reference_id = base->parent_reference_id;
  runtime->world_bounds_using_local_bounds_id = base->world_bounds_using_local_bounds_id;
  runtime->world_bounds_using_world_transform_id = base->world_bounds_using_world_transform_id;
  runtime->world_transform_id = base->world_transform_id;
  runtime->world_transform_using_local_transform_id = base->world_transform_using_local_transform_id;
  runtime->world_transform_using_parent_transform_id = base->world_transform_using_parent_transform_id;
  runtime->local_matrix = std::nullopt;
  runtime->rotation_angle = 0.0;
  runtime->rotation_cosine = 1.0;
  runtime->rotation_sine = 0.0;
  runtime->world_matrix = std::nullopt;
  runtime->bounds_rectangle = std::nullopt;
  runtime->compute_local_bounds_rectangle = [](auto out, auto source) {
    compute_tilemap_local_bounds_rectangle(out, source);
  };
  runtime->is_local_bounds_rectangle_valid = std::nullopt;
  runtime->local_bounds_rectangle = std::nullopt;
  runtime->world_bounds_rectangle = std::nullopt;
  runtime->scene2d = std::nullopt;
  return runtime;
}

inline flight::Ref<Tilemap> create_tilemap(
    std::optional<PartialTilemap> object = std::nullopt) {
  auto tilemap = flight::make_ref<Tilemap>();
  tilemap->data = object.has_value()
                      ? flight::row_get<flight::RowKey<"data">>(object.value())
                            .value_or(create_tilemap_data())
                      : create_tilemap_data();
  tilemap->enabled = object.has_value()
                         ? flight::row_get<flight::RowKey<"enabled">>(object.value()).value_or(true)
                         : true;
  tilemap->kind = flight::types::tilemap_kind;
  tilemap->name = object.has_value()
                      ? flight::row_get<flight::RowKey<"name">>(object.value())
                      : std::optional<flight::String>{};
  tilemap->entity_runtime_key = flight::scene2d::create_node_2d_runtime();
  tilemap->alpha = object.has_value()
                       ? flight::row_get<flight::RowKey<"alpha">>(object.value()).value_or(1.0)
                       : 1.0;
  tilemap->visible = object.has_value()
                         ? flight::row_get<flight::RowKey<"visible">>(object.value()).value_or(true)
                         : true;
  tilemap->blend_mode = object.has_value()
                            ? flight::row_get<flight::RowKey<"blendMode">>(object.value())
                            : std::optional<flight::String>{};
  tilemap->clip = object.has_value()
                      ? flight::row_get<flight::RowKey<"clip">>(object.value())
                      : std::optional<flight::Ref<flight::types::ClipRegion>>{};
  tilemap->material = object.has_value()
                          ? flight::row_get<flight::RowKey<"material">>(object.value())
                          : std::optional<flight::Ref<flight::types::Material2D>>{};
  tilemap->material_data = object.has_value()
                               ? flight::row_get<flight::RowKey<"materialData">>(object.value())
                               : std::optional<flight::Ref<MaterialData>>{};
  tilemap->pivot_x = object.has_value()
                         ? flight::row_get<flight::RowKey<"pivotX">>(object.value()).value_or(0.0)
                         : 0.0;
  tilemap->pivot_y = object.has_value()
                         ? flight::row_get<flight::RowKey<"pivotY">>(object.value()).value_or(0.0)
                         : 0.0;
  tilemap->rotation = object.has_value()
                          ? flight::row_get<flight::RowKey<"rotation">>(object.value()).value_or(0.0)
                          : 0.0;
  tilemap->scale_x = object.has_value()
                         ? flight::row_get<flight::RowKey<"scaleX">>(object.value()).value_or(1.0)
                         : 1.0;
  tilemap->scale_y = object.has_value()
                         ? flight::row_get<flight::RowKey<"scaleY">>(object.value()).value_or(1.0)
                         : 1.0;
  tilemap->skew_x = object.has_value()
                        ? flight::row_get<flight::RowKey<"skewX">>(object.value()).value_or(0.0)
                        : 0.0;
  tilemap->skew_y = object.has_value()
                        ? flight::row_get<flight::RowKey<"skewY">>(object.value()).value_or(0.0)
                        : 0.0;
  tilemap->x = object.has_value()
                   ? flight::row_get<flight::RowKey<"x">>(object.value()).value_or(0.0)
                   : 0.0;
  tilemap->y = object.has_value()
                   ? flight::row_get<flight::RowKey<"y">>(object.value()).value_or(0.0)
                   : 0.0;
  tilemap_runtimes.set(tilemap, create_tilemap_runtime());
  return tilemap;
}

inline flight::Ref<Tilemap> clone_tilemap(flight::Ref<Tilemap> source) {
  const auto& source_data = source->data;
  auto copy = create_tilemap();
  copy->data->atlas = source_data->atlas;
  copy->data->columns = source_data->columns;
  copy->data->material_data = source_data->material_data.has_value()
                                  ? std::optional{source_data->material_data.value().slice()}
                                  : std::nullopt;
  copy->data->rows = source_data->rows;
  copy->data->tile_height = source_data->tile_height;
  copy->data->tile_width = source_data->tile_width;
  copy->data->tiles = source_data->tiles.slice();
  return copy;
}

template <typename Source>
inline flight::Ref<TilemapRuntime> get_tilemap_runtime(const Source& source) {
  const auto tilemap = detail::exact_tilemap(source);
  auto runtime = tilemap_runtimes.get(tilemap);
  if (runtime.has_value()) return runtime.value();
  auto created = create_tilemap_runtime();
  tilemap_runtimes.set(tilemap, created);
  return created;
}

inline flight::Ref<TilemapSignals> enable_tilemap_signals(flight::Ref<Tilemap> target) {
  auto signals = tilemap_signals.get(target);
  if (signals.has_value()) return signals.value();
  auto created = create_tilemap_signals();
  tilemap_signals.set(target, created);
  return created;
}

template <typename Source>
inline std::optional<flight::Ref<TilemapSignals>> get_tilemap_signals(const Source& source) {
  return tilemap_signals.get(detail::exact_tilemap(source));
}

inline void clear_tilemap(flight::Ref<Tilemap> tilemap) {
  tilemap->data->tiles.fill(-1.0);
  if (auto signals = get_tilemap_signals(tilemap); signals.has_value()) {
    signals.value()->on_cleared->emit();
  }
}

inline void resize_tilemap(flight::Ref<Tilemap> tilemap, double columns, double rows) {
  const auto data = tilemap->data;
  flight::Int16Array new_tiles(columns * rows);
  new_tiles.fill(-1.0);
  const double copy_columns = flight::minimum(columns, data->columns);
  const double copy_rows = flight::minimum(rows, data->rows);
  for (double row = 0.0; row < copy_rows; row += 1.0) {
    for (double column = 0.0; column < copy_columns; column += 1.0) {
      new_tiles.set_index(
          row * columns + column,
          data->tiles.get_index(row * data->columns + column));
    }
  }
  data->columns = columns;
  data->rows = rows;
  data->tiles = new_tiles;
}

inline void set_tilemap_tile(
    flight::Ref<Tilemap> tilemap,
    double column,
    double row,
    double id) {
  const auto data = tilemap->data;
  if (column < 0.0 || column >= data->columns || row < 0.0 || row >= data->rows) return;
  data->tiles.set_index(row * data->columns + column, id);
  if (auto signals = get_tilemap_signals(tilemap); signals.has_value()) {
    signals.value()->on_tile_changed->emit(column, row, id);
  }
}

inline void set_tilemap_tiles(
    flight::Ref<Tilemap> tilemap,
    flight::SequenceView<double> ids,
    double offset_column,
    double offset_row,
    double width,
    double height) {
  const auto data = tilemap->data;
  for (double row = 0.0; row < height; row += 1.0) {
    const double target_row = offset_row + row;
    if (target_row < 0.0 || target_row >= data->rows) continue;
    for (double column = 0.0; column < width; column += 1.0) {
      const double target_column = offset_column + column;
      if (target_column < 0.0 || target_column >= data->columns) continue;
      data->tiles.set_index(
          target_row * data->columns + target_column,
          ids[row * width + column]);
    }
  }
  if (auto signals = get_tilemap_signals(tilemap); signals.has_value()) {
    signals.value()->on_tiles_changed->emit(offset_column, offset_row, width, height);
  }
}

inline void set_tilemap_tile_tint(
    flight::Ref<Tilemap> tilemap,
    double column,
    double row,
    double rgba) {
  const auto data = tilemap->data;
  if (column < 0.0 || column >= data->columns || row < 0.0 || row >= data->rows) return;
  if (!data->material_data.has_value()) {
    data->material_data =
        flight::Array<std::optional<flight::Ref<MaterialData>>>(data->columns * data->rows);
  }
  data->material_data.value().element(row * data->columns + column) =
      detail::create_tint_material_data(rgba);
}

} // namespace flight::tilemap
