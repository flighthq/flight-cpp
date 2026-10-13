#include <flight/signals/contract.hpp>
#include <flight/tilemap/contract.hpp>

#include <cstdint>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// Native port of packages/tilemap/src/tilemap.test.ts at a627849. Exact arrays and coordinates
// deliberately pin row-major ordering, resize reindexing, clipped layer writes, and failed-output
// immutability rather than accepting only aggregate counts.

namespace {

namespace signals = flight::signals;
namespace tilemap = flight::tilemap;

using flight::types::MaterialData;
using flight::types::Rectangle;
using flight::types::TextureAtlas;
using flight::types::Tilemap;
using flight::types::TintMaterialData;
using flight::types::Vector2;

int failures = 0;

void check(bool condition, const std::string& message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

template <typename Type>
auto readonly(const std::shared_ptr<Type>& value) {
  using View = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<Type>>>>;
  return View(value);
}

template <typename Function>
struct second_argument;

template <typename Result, typename First, typename Second>
struct second_argument<std::function<Result(First, Second)>> {
  using type = Second;
};

flight::Ref<Rectangle> rectangle(double x = 0.0, double y = 0.0, double width = 0.0, double height = 0.0) {
  auto value = flight::make_ref<Rectangle>();
  value->entity_runtime_key = std::nullopt;
  value->height = height;
  value->width = width;
  value->x = x;
  value->y = y;
  return value;
}

flight::Ref<TextureAtlas> atlas() {
  auto value = flight::make_ref<TextureAtlas>();
  value->entity_runtime_key = std::nullopt;
  value->image_height = 1.0;
  value->image_name = std::nullopt;
  value->image_width = 1.0;
  value->regions = {};
  value->scale = 1.0;
  value->texture = std::nullopt;
  return value;
}

flight::Ref<Tilemap> grid(
    double columns,
    double rows,
    double tile_width = 16.0,
    double tile_height = 16.0,
    bool with_atlas = true) {
  auto value = tilemap::create_tilemap();
  value->data->columns = columns;
  value->data->rows = rows;
  value->data->tile_width = tile_width;
  value->data->tile_height = tile_height;
  value->data->atlas = with_atlas ? std::optional{atlas()} : std::nullopt;
  value->data->tiles = flight::Int16Array(columns * rows).fill(-1.0);
  value->data->material_data = std::nullopt;
  return value;
}

bool tiles_are(const flight::Ref<Tilemap>& value, std::initializer_list<int> expected) {
  if (value->data->tiles.length != expected.size()) return false;
  double index = 0.0;
  for (const int cell : expected) {
    if (value->data->tiles.get_index(index) != cell) return false;
    index += 1.0;
  }
  return true;
}

void construction_and_clone_evidence() {
  const auto empty = tilemap::create_tilemap();
  check(empty && empty->kind == flight::types::tilemap_kind && empty->enabled,
        "createTilemap initializes identity fields");
  check(empty->data && empty->data->columns == 0.0 && empty->data->rows == 0.0 &&
            empty->data->tile_width == 0.0 && empty->data->tile_height == 0.0 &&
            !empty->data->atlas.has_value() && !empty->data->material_data.has_value() &&
            empty->data->tiles.length == 0,
        "createTilemap initializes empty data");

  auto supplied = flight::make_ref<flight::types::TilemapData>();
  supplied->entity_runtime_key = std::nullopt;
  supplied->atlas = atlas();
  supplied->columns = 3.0;
  supplied->rows = 2.0;
  supplied->tile_width = 8.0;
  supplied->tile_height = 12.0;
  supplied->tiles = flight::Int16Array{1, 2, 3, 4, 5, 6};
  supplied->material_data = std::nullopt;
  tilemap::PartialTilemapData partial_data(supplied);
  const auto initialized = tilemap::create_tilemap_data(std::optional{partial_data});
  check(initialized != supplied && initialized->atlas == supplied->atlas &&
            initialized->columns == 3.0 && initialized->rows == 2.0 &&
            initialized->tile_width == 8.0 && initialized->tile_height == 12.0 &&
            initialized->tiles.get_index(5.0) == 6,
        "createTilemapData copies predefined values into a new object");

  const auto source = grid(3.0, 2.0, 8.0, 12.0);
  source->data->tiles = flight::Int16Array{1, 2, 3, 4, 5, 6};
  source->data->material_data = flight::Array<std::optional<flight::Ref<MaterialData>>>(6.0);
  const auto cloned = tilemap::clone_tilemap(source);
  check(cloned != source && cloned->data != source->data && cloned->data->atlas == source->data->atlas &&
            cloned->data->columns == 3.0 && cloned->data->rows == 2.0 &&
            cloned->data->tile_width == 8.0 && cloned->data->tile_height == 12.0 &&
            cloned->kind == source->kind && tiles_are(cloned, {1, 2, 3, 4, 5, 6}),
        "cloneTilemap preserves the grid, atlas, kind, and cell values");
  check(cloned->data->material_data->identity() != source->data->material_data->identity(),
        "cloneTilemap slices the material-data array");
  check(tilemap::get_tilemap_runtime(cloned) != tilemap::get_tilemap_runtime(source),
        "cloneTilemap owns a fresh runtime");
  tilemap::set_tilemap_tile(cloned, 0.0, 0.0, 99.0);
  check(tilemap::get_tilemap_tile(readonly(source), 0.0, 0.0) == 1.0,
        "cloneTilemap owns an independent tile buffer");
}

void bounds_runtime_and_signal_evidence() {
  const auto value = grid(5.0, 4.0, 32.0, 16.0);
  auto out = rectangle(9.0, 9.0, 9.0, 9.0);
  tilemap::compute_tilemap_local_bounds_rectangle(out, value);
  check(out->x == 0.0 && out->y == 0.0 && out->width == 160.0 && out->height == 64.0,
        "bounds use exact grid and atlas dimensions");
  value->data->atlas = std::nullopt;
  tilemap::compute_tilemap_local_bounds_rectangle(out, value);
  check(out->width == 0.0 && out->height == 0.0, "atlas absence produces zero bounds");

  value->data->atlas = atlas();
  const auto runtime = tilemap::get_tilemap_runtime(value);
  check(runtime && static_cast<bool>(runtime->compute_local_bounds_rectangle),
        "createTilemapRuntime installs the bounds method");
  auto runtime_out = rectangle();
  using RuntimeBoundsSource =
      typename second_argument<decltype(runtime->compute_local_bounds_rectangle)>::type;
  const auto runtime_source = RuntimeBoundsSource::from_owner(readonly(value).shared_owner());
  runtime->compute_local_bounds_rectangle(runtime_out, runtime_source);
  check(runtime_out->width == 160.0 && runtime_out->height == 64.0,
        "the runtime bounds method delegates to tilemap bounds");
  check(tilemap::get_tilemap_runtime(value) == runtime, "getTilemapRuntime is stable");

  check(!tilemap::get_tilemap_signals(value).has_value(),
        "getTilemapSignals is empty before enable");
  const auto group = tilemap::enable_tilemap_signals(value);
  check(group && group->on_cleared && group->on_tile_changed && group->on_tiles_changed &&
            tilemap::enable_tilemap_signals(value) == group &&
            tilemap::get_tilemap_signals(value).value() == group,
        "enableTilemapSignals creates one stable three-signal group");

  int cleared = 0;
  std::vector<double> changed;
  std::vector<double> block;
  signals::connect_signal(group->on_cleared, std::function<void()>([&] { ++cleared; }));
  signals::connect_signal(
      group->on_tile_changed,
      std::function<void(double, double, double)>(
          [&](double column, double row, double id) { changed = {column, row, id}; }));
  signals::connect_signal(
      group->on_tiles_changed,
      std::function<void(double, double, double, double)>(
          [&](double column, double row, double width, double height) {
            block = {column, row, width, height};
          }));
  tilemap::set_tilemap_tile(value, 1.0, 2.0, 7.0);
  tilemap::set_tilemap_tiles(
      value,
      flight::SequenceView<double>(flight::Array<double>{8.0}),
      0.0,
      0.0,
      1.0,
      1.0);
  tilemap::clear_tilemap(value);
  check(changed == std::vector<double>({1.0, 2.0, 7.0}),
        "setTilemapTile fires exact coordinates and id");
  check(block == std::vector<double>({0.0, 0.0, 1.0, 1.0}),
        "setTilemapTiles fires the exact dirty rectangle");
  check(cleared == 1, "clearTilemap fires onCleared once");
}

void coordinate_and_lookup_evidence() {
  const auto value = grid(4.0, 4.0, 16.0, 16.0);
  const auto view = readonly(value);
  check(tilemap::get_tilemap_column_at_x(view, 0.0) == 0.0 &&
            tilemap::get_tilemap_column_at_x(view, 31.0) == 1.0 &&
            tilemap::get_tilemap_column_at_x(view, 48.0) == 3.0,
        "column queries floor exact in-bounds fixtures");
  check(tilemap::get_tilemap_column_at_x(view, -1.0) == -1.0 &&
            tilemap::get_tilemap_column_at_x(view, 64.0) == -1.0,
        "column queries reject both sides of the grid");
  check(tilemap::get_tilemap_row_at_y(view, 0.0) == 0.0 &&
            tilemap::get_tilemap_row_at_y(view, 31.0) == 1.0 &&
            tilemap::get_tilemap_row_at_y(view, -1.0) == -1.0 &&
            tilemap::get_tilemap_row_at_y(view, 64.0) == -1.0,
        "row queries floor and reject exact fixtures");

  auto point = flight::make_ref<Vector2>(Vector2{.x = 7.0, .y = 8.0});
  check(tilemap::get_tilemap_column_row_at_point(point, view, 33.0, 17.0) &&
            point->x == 2.0 && point->y == 1.0,
        "point conversion writes exact column and row");
  point->x = 7.0;
  point->y = 8.0;
  check(!tilemap::get_tilemap_column_row_at_point(point, view, 64.0, 0.0) &&
            point->x == 7.0 && point->y == 8.0,
        "failed point conversion does not modify output");

  tilemap::set_tilemap_tile(value, 2.0, 1.0, 8.0);
  check(tilemap::get_tilemap_tile(view, 2.0, 1.0) == 8.0 &&
            tilemap::get_tilemap_tile(view, -1.0, 0.0) == -1.0 &&
            tilemap::get_tilemap_tile(view, 4.0, 0.0) == -1.0,
        "getTilemapTile uses row-major cells and out-of-range sentinels");
  point->x = 33.0;
  point->y = 17.0;
  check(tilemap::get_tilemap_tile_at_point(view, readonly(point)) == 8.0 &&
            tilemap::get_tilemap_tile_at_point_xy(view, 33.0, 17.0) == 8.0,
        "point lookup resolves the exact row-major cell");

  auto rect = rectangle(1.0, 2.0, 3.0, 4.0);
  value->data->tile_height = 24.0;
  check(tilemap::get_tilemap_tile_rect(rect, view, 2.0, 1.0) && rect->x == 32.0 &&
            rect->y == 24.0 && rect->width == 16.0 && rect->height == 24.0,
        "tile rectangle uses exact column, row, and tile dimensions");
  rect->x = 1.0;
  rect->y = 2.0;
  rect->width = 3.0;
  rect->height = 4.0;
  check(!tilemap::get_tilemap_tile_rect(rect, view, 4.0, 0.0) && rect->x == 1.0 &&
            rect->y == 2.0 && rect->width == 3.0 && rect->height == 4.0,
        "failed tile rectangle does not modify output");
  value->data->atlas = std::nullopt;
  check(tilemap::get_tilemap_column_at_x(view, 0.0) == -1.0 &&
            tilemap::get_tilemap_tile_at_point_xy(view, 0.0, 0.0) == -1.0 &&
            !tilemap::get_tilemap_tile_rect(rect, view, 0.0, 0.0),
        "atlas absence propagates through coordinate and rectangle queries");
}

void mutation_and_ordering_evidence() {
  auto value = grid(3.0, 2.0);
  tilemap::fill_tilemap_tiles(value, 4.0);
  check(tiles_are(value, {4, 4, 4, 4, 4, 4}), "fillTilemapTiles writes every cell");
  tilemap::clear_tilemap(value);
  check(tiles_are(value, {-1, -1, -1, -1, -1, -1}), "clearTilemap restores every sentinel");

  tilemap::set_tilemap_tile(value, 1.0, 0.0, 5.0);
  check(tiles_are(value, {-1, 5, -1, -1, -1, -1}),
        "setTilemapTile stores the exact row-major index");
  tilemap::set_tilemap_tile(value, -1.0, 0.0, 9.0);
  tilemap::set_tilemap_tile(value, 3.0, 0.0, 9.0);
  check(tiles_are(value, {-1, 5, -1, -1, -1, -1}),
        "out-of-bounds tile writes leave all cells unchanged");
  tilemap::set_tilemap_tile(value, 1.0, 0.0, -1.0);
  check(tiles_are(value, {-1, -1, -1, -1, -1, -1}), "the empty sentinel is writable");

  tilemap::set_tilemap_tiles(
      value,
      flight::SequenceView<double>(flight::Array<double>{1, 2, 3, 4, 5, 6}),
      0.0,
      0.0,
      3.0,
      2.0);
  check(tiles_are(value, {1, 2, 3, 4, 5, 6}), "block writes preserve source row-major order");

  value = grid(4.0, 4.0);
  tilemap::set_tilemap_tiles(
      value,
      flight::SequenceView<double>(flight::Array<double>{10, 11, 12, 13}),
      1.0,
      2.0,
      2.0,
      2.0);
  check(tiles_are(value, {-1, -1, -1, -1, -1, -1, -1, -1,
                          -1, 10, 11, -1, -1, 12, 13, -1}),
        "block writes preserve ordering at a non-zero layer offset");

  value = grid(2.0, 2.0);
  tilemap::set_tilemap_tiles(
      value,
      flight::SequenceView<double>(flight::Array<double>{1, 2, 3, 4, 5, 6, 7, 8, 9}),
      1.0,
      1.0,
      3.0,
      3.0);
  check(tiles_are(value, {-1, -1, -1, 1}), "positive overflow clips to the one covered cell");
  value->data->tiles.fill(-1.0);
  tilemap::set_tilemap_tiles(
      value,
      flight::SequenceView<double>(flight::Array<double>{10, 20, 30, 40, 50, 60, 70, 80, 90}),
      -1.0,
      -1.0,
      3.0,
      3.0);
  check(tiles_are(value, {50, 60, 80, 90}),
        "negative offsets clip targets while retaining source row-major indices");

  value = grid(3.0, 2.0);
  value->data->tiles = flight::Int16Array{1, -1, -1, -1, 2, 9};
  tilemap::resize_tilemap(value, 2.0, 2.0);
  check(value->data->columns == 2.0 && value->data->rows == 2.0 &&
            tiles_are(value, {1, -1, -1, 2}),
        "shrinking preserves the surviving row-major rectangle");
  tilemap::resize_tilemap(value, 4.0, 3.0);
  check(tiles_are(value, {1, -1, -1, -1, -1, 2, -1, -1, -1, -1, -1, -1}),
        "growing reindexes preserved rows and fills all new cells");
  value = grid(3.0, 2.0);
  value->data->tiles.set_index(5.0, 9.0);
  tilemap::resize_tilemap(value, 4.0, 2.0);
  check(value->data->tiles.get_index(6.0) == 9.0,
        "resize moves old index 5 to new row-major index 6");
}

void tint_evidence() {
  const auto value = grid(3.0, 2.0);
  tilemap::set_tilemap_tile_tint(value, 1.0, 1.0, 0x12345678u);
  check(value->data->material_data.has_value() && value->data->material_data->size() == 6 &&
            value->data->material_data->element(4.0).has_value(),
        "tint lazily allocates one row-major material slot per tile");

  const auto material = value->data->material_data->element(4.0).value();
  auto base = flight::StructuralRef<
      flight::RowReadonly<flight::RowOf<flight::Ref<MaterialData>>>>(material);
  using TintView = flight::StructuralRef<
      flight::RowReadonly<flight::RowOf<flight::Ref<TintMaterialData>>>>;
  const auto tint = TintView::from_owner(base.shared_owner());
  check(flight::row_get<flight::RowKey<"tint">>(tint) == 0x12345678u,
        "tint stores the exact unsigned packed RGBA on the same material identity");

  const auto invalid = grid(3.0, 2.0);
  tilemap::set_tilemap_tile_tint(invalid, 3.0, 0.0, 0xffffffffu);
  check(!invalid->data->material_data.has_value(),
        "out-of-range tint writes do not allocate material data");
}

} // namespace

int main() {
  construction_and_clone_evidence();
  bounds_runtime_and_signal_evidence();
  coordinate_and_lookup_evidence();
  mutation_and_ordering_evidence();
  tint_evidence();

  if (failures != 0) {
    std::cerr << failures << " tilemap parity assertion(s) failed\n";
    return 1;
  }
  std::cout << "generated tilemap parity tests passed\n";
  return 0;
}
