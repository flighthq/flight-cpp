// Native evidence for @flighthq/quadbatch at a627849. Assertions are ported from
// packages/quadbatch/src/quadBatch.test.ts and retain its exact expected values. The byte-layout
// assertions additionally pin the renderer-facing Uint16 id and Float32 transform buffers.
//
// The two generated heritage gaps are asserted at their honest native boundary: local bounds and
// tint data remain available through quadbatch's exact-owner accessors, while generic node bounds
// and a renderer-side MaterialData -> TintMaterialData recovery remain unavailable until the
// frozen generated owners preserve their source interface heritage.
#include <flight/quadbatch/quad_batch.hpp>
#include <flight/signals/slot.hpp>

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <tuple>
#include <vector>

namespace {

namespace quadbatch = flight::quadbatch;
using Batch = flight::Ref<flight::types::QuadBatch>;
using Data = flight::Ref<flight::types::QuadBatchData>;
using Rectangle = flight::Ref<flight::types::Rectangle>;
using Region = flight::Ref<flight::types::TextureAtlasRegion>;

int failures = 0;
bool inject_transform_layout_mutation = false;
bool inject_last_removal_mutation = false;

void check(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}

template <typename T>
auto readonly(const std::shared_ptr<T>& value)
    -> flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<T>>>> {
  return flight::structural_ref_cast<
      flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<T>>>>>(
      flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<T>>>>(
          value));
}

Region region(double id = 0.0, double width = 32.0, double height = 32.0) {
  auto out = flight::make_ref<flight::types::TextureAtlasRegion>();
  out->entity_runtime_key = std::nullopt;
  out->height = height;
  out->id = id;
  out->name = std::nullopt;
  out->original_height = std::nullopt;
  out->original_width = std::nullopt;
  out->page_name = std::nullopt;
  out->pivot_x = std::nullopt;
  out->pivot_y = std::nullopt;
  out->rotation = flight::types::TextureAtlasRotation::None;
  out->source_x = 0.0;
  out->source_y = 0.0;
  out->trimmed = false;
  out->x = 0.0;
  out->y = 0.0;
  out->width = width;
  return out;
}

flight::Ref<flight::types::TextureAtlas> atlas(
    std::initializer_list<Region> regions) {
  auto out = flight::make_ref<flight::types::TextureAtlas>();
  out->entity_runtime_key = std::nullopt;
  out->image_height = 0.0;
  out->image_name = std::nullopt;
  out->image_width = 0.0;
  out->regions = flight::Array<Region>(regions);
  out->scale = 1.0;
  out->texture = std::nullopt;
  return out;
}

Data data_with(std::optional<flight::Ref<flight::types::TextureAtlas>> texture_atlas,
               double count = 0.0,
               flight::types::QuadTransformType type = flight::String("vector2")) {
  auto out = quadbatch::create_quad_batch_data();
  out->atlas = std::move(texture_atlas);
  out->instance_count = count;
  out->transform_type = std::move(type);
  return out;
}

Batch batch_with(std::optional<flight::Ref<flight::types::TextureAtlas>> texture_atlas,
                 double count = 0.0,
                 flight::types::QuadTransformType type = flight::String("vector2")) {
  return quadbatch::create_quad_batch_with_data(
      data_with(std::move(texture_atlas), count, std::move(type)));
}

Rectangle rectangle(double x = 0.0, double y = 0.0, double width = 0.0,
                    double height = 0.0) {
  auto out = flight::make_ref<flight::types::Rectangle>();
  out->entity_runtime_key = std::nullopt;
  out->x = x;
  out->y = y;
  out->width = width;
  out->height = height;
  return out;
}

flight::Ref<flight::types::Vector2> point(double x = 0.0, double y = 0.0) {
  auto out = flight::make_ref<flight::types::Vector2>();
  out->entity_runtime_key = std::nullopt;
  out->x = x;
  out->y = y;
  return out;
}

void construction_and_initializers() {
  auto batch = quadbatch::create_quad_batch();
  check(!batch->data->atlas.has_value(), "createQuadBatch: atlas defaults to null");
  check(batch->data->ids.size() == 0, "createQuadBatch: ids defaults to an empty Uint16Array");
  check(batch->data->instance_count == 0, "createQuadBatch: instanceCount defaults to zero");
  check(!batch->data->material_data.has_value(), "createQuadBatch: materialData defaults to null");
  check(batch->data->transforms.size() == 0, "createQuadBatch: transforms defaults to an empty Float32Array");
  check(batch->data->transform_type == flight::String("vector2"), "createQuadBatch: transformType defaults to vector2");
  check(batch->kind == flight::types::quad_batch_kind, "createQuadBatch: kind is QuadBatchKind");
  check(quadbatch::create_quad_batch() != batch, "createQuadBatch: each call returns a new object");

  auto supplied = data_with(atlas({region()}), 1000.0, flight::String("matrix3x2"));
  supplied->ids = flight::Uint16Array{3, 4};
  supplied->transforms = flight::Float32Array{1, 0, 0, 1, 5, 6};
  auto object = quadbatch::create_quad_batch_with_data(supplied);
  check(object->data == supplied, "createQuadBatch: pre-defined data owner is preserved");
  check(object->data->atlas == supplied->atlas, "createQuadBatch: pre-defined atlas is preserved");
  check(object->data->ids == supplied->ids, "createQuadBatch: pre-defined ids are preserved");
  check(object->data->instance_count == 1000, "createQuadBatch: pre-defined instanceCount is preserved");
  check(object->data->transforms == supplied->transforms, "createQuadBatch: pre-defined transforms are preserved");
  check(object->data->transform_type == flight::String("matrix3x2"), "createQuadBatch: pre-defined transformType is preserved");
  auto batch_options = flight::make_structural_ref<flight::RowReadonly<
      flight::RowPartial<flight::RowOf<Batch>>>>(
      flight::row_field<flight::RowKey<"data">>(std::optional<Data>{supplied}));
  auto option_object = quadbatch::create_quad_batch(batch_options);
  check(option_object != object, "createQuadBatch options are copied into a new batch owner");
  check(option_object->data == supplied, "createQuadBatch projects its pre-defined data option");

  auto data = quadbatch::create_quad_batch_data();
  check(!data->atlas.has_value(), "createQuadBatchData: atlas defaults to null");
  check(data->ids.size() == 0, "createQuadBatchData: ids is a zero-length Uint16Array");
  check(data->instance_count == 0, "createQuadBatchData: instanceCount defaults to zero");
  check(!data->material_data.has_value(), "createQuadBatchData: materialData defaults to null");
  check(data->transforms.size() == 0, "createQuadBatchData: transforms is a zero-length Float32Array");
  check(data->transform_type == flight::String("vector2"), "createQuadBatchData: transformType defaults to vector2");
  auto data_options = flight::make_structural_ref<flight::RowReadonly<
      flight::RowPartial<flight::RowOf<Data>>>>(
      flight::row_field<flight::RowKey<"instanceCount">>(std::optional<double>{10}));
  check(quadbatch::create_quad_batch_data(data_options)->instance_count == 10,
        "createQuadBatchData projects a pre-defined instanceCount");
  check(quadbatch::create_quad_batch_runtime() != nullptr, "createQuadBatchRuntime returns a non-null owner");
  check(quadbatch::create_quad_batch_signals() != nullptr, "createQuadBatchSignals returns a non-null owner");
}

void append_capacity_and_byte_layout() {
  auto batch = quadbatch::create_quad_batch();
  const auto index = quadbatch::append_quad_batch_instance(batch, 3, 10, 20);
  check(index == 0, "appendQuadBatchInstance returns the first index");
  check(batch->data->instance_count == 1, "appendQuadBatchInstance increments instanceCount");
  check(batch->data->ids.get_index(0) == 3, "appendQuadBatchInstance writes the region id");
  check(batch->data->transforms.get_index(0) == 10, "appendQuadBatchInstance writes x");
  check(batch->data->transforms.get_index(1) == 20, "appendQuadBatchInstance writes y");
  check(quadbatch::append_quad_batch_instance(batch, 1, 5, 5) == 1, "appendQuadBatchInstance returns sequential indices");
  check(batch->data->instance_count == 2, "two appends produce instanceCount two");

  auto grown = quadbatch::create_quad_batch();
  for (double i = 0; i < 10; ++i) quadbatch::append_quad_batch_instance(grown, i, i, i);
  check(grown->data->instance_count == 10, "appendQuadBatchInstance auto-grows to ten instances");
  check(quadbatch::get_quad_batch_capacity(readonly(grown)) >= 10, "appendQuadBatchInstance auto-grows sufficient capacity");

  auto reserved = quadbatch::create_quad_batch();
  quadbatch::reserve_quad_batch(reserved, 100);
  check(reserved->data->ids.size() == 100, "reserveQuadBatch allocates 100 Uint16 ids");
  check(reserved->data->transforms.size() == 200, "reserveQuadBatch allocates 100 vector2 transforms");
  check(reserved->data->ids.byte_offset == 0, "id buffer begins at byte offset zero");
  check(reserved->data->ids.byte_length == 100 * sizeof(std::uint16_t), "id buffer is exactly two bytes per instance");
  check(reserved->data->transforms.byte_offset == 0, "transform buffer begins at byte offset zero");
  check(reserved->data->transforms.byte_length == 200 * sizeof(float), "vector2 buffer is exactly eight bytes per instance");
  const auto id_buffer = reserved->data->ids.buffer;
  const auto transform_buffer = reserved->data->transforms.buffer;
  quadbatch::reserve_quad_batch(reserved, 50);
  check(reserved->data->ids.buffer == id_buffer, "reserveQuadBatch below capacity preserves id storage");
  check(reserved->data->transforms.buffer == transform_buffer, "reserveQuadBatch below capacity preserves transform storage");
  quadbatch::reserve_quad_batch(reserved, 100);
  check(reserved->data->ids.buffer == id_buffer, "reserveQuadBatch at capacity preserves id storage");
  check(reserved->data->transforms.buffer == transform_buffer, "reserveQuadBatch at capacity preserves transform storage");

  auto matrix = batch_with(std::nullopt, 0, flight::String("matrix3x2"));
  quadbatch::reserve_quad_batch(matrix, 7);
  check(matrix->data->ids.byte_length == 7 * sizeof(std::uint16_t), "matrix ids remain Uint16 packed");
  check(matrix->data->transforms.byte_length == 7 * 6 * sizeof(float), "matrix3x2 layout is six Float32 values per instance");

  auto misaligned = quadbatch::create_quad_batch();
  misaligned->data->ids = flight::Uint16Array(20.0);
  misaligned->data->transforms = flight::Float32Array(20.0);
  check(quadbatch::get_quad_batch_capacity(readonly(misaligned)) == 10,
        "capacity uses the smaller vector2 transform capacity");
  misaligned->data->transforms = flight::Float32Array(100.0);
  check(quadbatch::get_quad_batch_capacity(readonly(misaligned)) == 20,
        "capacity uses the smaller id capacity");

  quadbatch::clear_quad_batch(reserved);
  check(reserved->data->instance_count == 0, "clearQuadBatch sets instanceCount to zero");
  check(quadbatch::get_quad_batch_capacity(readonly(reserved)) == 100, "clearQuadBatch preserves capacity");
}

void clone_compaction_and_ordering() {
  auto texture_atlas = atlas({region()});
  auto source = batch_with(texture_atlas);
  quadbatch::append_quad_batch_instance(source, 0, 10, 20);
  auto clone = quadbatch::clone_quad_batch(readonly(source));
  check(clone != source, "cloneQuadBatch returns a distinct batch");
  check(clone->data->atlas.value() == texture_atlas, "cloneQuadBatch shares the atlas identity");
  check(clone->data->instance_count == 1, "cloneQuadBatch copies instanceCount");
  check(clone->data->transform_type == flight::String("vector2"), "cloneQuadBatch copies transformType");
  check(quadbatch::get_quad_batch_instance_id(readonly(clone), 0) == 0, "cloneQuadBatch copies ids");
  check(clone->data->transforms.get_index(0) == 10 && clone->data->transforms.get_index(1) == 20,
        "cloneQuadBatch copies transform values");
  check(clone->data->ids.buffer != source->data->ids.buffer, "cloneQuadBatch clones the id buffer");
  check(clone->data->transforms.buffer != source->data->transforms.buffer, "cloneQuadBatch clones the transform buffer");
  check(quadbatch::get_quad_batch_runtime(readonly(clone)) != quadbatch::get_quad_batch_runtime(readonly(source)),
        "cloneQuadBatch creates a fresh runtime");
  quadbatch::set_quad_batch_instance(clone, 0, 5, 99, 99);
  check(quadbatch::get_quad_batch_instance_id(readonly(source), 0) == 0, "clone id mutations do not leak to source");
  check(source->data->transforms.get_index(0) == 10, "clone transform mutations do not leak to source");
  auto material = flight::make_ref<flight::types::MaterialData>();
  material->entity_runtime_key = std::nullopt;
  source->data->material_data = flight::Array<
      std::optional<flight::Ref<flight::types::MaterialData>>>{material};
  auto material_clone = quadbatch::clone_quad_batch(readonly(source));
  check(material_clone->data->material_data.has_value(), "cloneQuadBatch copies present materialData");
  check(material_clone->data->material_data.value().identity() !=
            source->data->material_data.value().identity(),
        "cloneQuadBatch gives materialData independent array storage");
  check(material_clone->data->material_data.value().element(0).value() == material,
        "cloneQuadBatch shallow-copies material identities");

  auto compacted = quadbatch::create_quad_batch();
  quadbatch::compact_quad_batch(compacted);
  check(compacted->data->instance_count == 0, "compactQuadBatch no-ops on an empty batch");
  quadbatch::append_quad_batch_instance(compacted, 10, 1, 1);
  quadbatch::append_quad_batch_instance(compacted, 11, 2, 2);
  quadbatch::append_quad_batch_instance(compacted, 12, 3, 3);
  compacted->data->ids.set_index(1, quadbatch::quad_batch_deleted_id);
  quadbatch::compact_quad_batch(compacted);
  check(compacted->data->instance_count == 2, "compactQuadBatch removes a deleted sentinel");
  check(quadbatch::get_quad_batch_instance_id(readonly(compacted), 0) == 10, "compactQuadBatch preserves first live id order");
  check(quadbatch::get_quad_batch_instance_id(readonly(compacted), 1) == 12, "compactQuadBatch preserves second live id order");
  check(compacted->data->transforms.get_index(2) == 3, "compactQuadBatch moves the third x value into slot one");
  auto live = quadbatch::create_quad_batch();
  quadbatch::append_quad_batch_instance(live, 1, 0, 0);
  quadbatch::append_quad_batch_instance(live, 2, 0, 0);
  quadbatch::compact_quad_batch(live);
  check(live->data->instance_count == 2, "compactQuadBatch leaves a fully live buffer unchanged");
}

void transforms_iteration_and_ranges() {
  auto batch = quadbatch::create_quad_batch();
  quadbatch::resize_quad_batch(batch, 3);
  quadbatch::set_quad_batch_instance(batch, 1, 7, 100, 200);
  auto out = point(99, 99);
  check(quadbatch::get_quad_batch_instance_id(readonly(batch), 1) == 7, "setQuadBatchInstance writes id at a valid index");
  check(quadbatch::get_quad_batch_instance_transform(out, readonly(batch), 1), "getQuadBatchInstanceTransform succeeds at a valid index");
  check(out->x == 100 && out->y == 200, "getQuadBatchInstanceTransform returns vector2 x/y");
  quadbatch::set_quad_batch_instance(batch, -1, 9, 1, 1);
  quadbatch::set_quad_batch_instance(batch, 3, 9, 1, 1);
  check(batch->data->ids.get_index(0) == 0, "setQuadBatchInstance no-ops outside the live range");
  out->x = 99;
  out->y = 99;
  check(!quadbatch::get_quad_batch_instance_transform(out, readonly(batch), 3), "getQuadBatchInstanceTransform fails outside the live range");
  check(out->x == 99 && out->y == 99, "failed transform reads leave the destination unchanged");

  quadbatch::set_quad_batch_instance_range(batch, 1, 2, flight::Float32Array{10, 20, 30, 40});
  quadbatch::get_quad_batch_instance_transform(out, readonly(batch), 1);
  check(out->x == 10 && out->y == 20, "setQuadBatchInstanceRange writes the first vector2");
  quadbatch::get_quad_batch_instance_transform(out, readonly(batch), 2);
  check(out->x == 30 && out->y == 40, "setQuadBatchInstanceRange writes the second vector2");
  quadbatch::set_quad_batch_instance_range(batch, 1, 5, flight::Float32Array(10.0));
  check(batch->data->transforms.get_index(2) == 10, "an oversized range no-ops");
  quadbatch::set_quad_batch_instance_range(batch, 0, 0, flight::Float32Array{1, 2});
  quadbatch::set_quad_batch_instance_range(batch, -1, 1, flight::Float32Array{1, 2});
  check(batch->data->transforms.get_index(0) == 0, "non-positive and negative ranges no-op");

  auto iterated = quadbatch::create_quad_batch();
  quadbatch::append_quad_batch_instance(iterated, 7, 1, 2);
  quadbatch::append_quad_batch_instance(iterated, 8, 3, 4);
  std::vector<std::tuple<double, double, double, double>> seen;
  quadbatch::iterate_quad_batch_instances(
      readonly(iterated), [&](double index, double id, flight::Float32Array view) {
        seen.emplace_back(index, id, view.get_index(0), view.get_index(1));
        check(view.byte_length == 2 * sizeof(float), "vector2 iterator exposes an eight-byte view");
      });
  check(seen == std::vector<std::tuple<double, double, double, double>>{{0, 7, 1, 2}, {1, 8, 3, 4}},
        "iterateQuadBatchInstances visits live entries in stable order");
  int empty_calls = 0;
  quadbatch::iterate_quad_batch_instances(readonly(quadbatch::create_quad_batch()),
      [&](double, double, flight::Float32Array) { ++empty_calls; });
  check(empty_calls == 0, "iterateQuadBatchInstances does not visit an empty batch");

  auto matrix = batch_with(std::nullopt, 0, flight::String("matrix3x2"));
  quadbatch::resize_quad_batch(matrix, 1);
  quadbatch::set_quad_batch_instance_matrix(matrix, 0, 3, 1, 2, 3, 4, 5, 6);
  check(matrix->data->ids.get_index(0) == 3, "setQuadBatchInstanceMatrix writes id");
  for (double i = 0; i < 6; ++i) {
    check(matrix->data->transforms.get_index(i) == i + 1, "setQuadBatchInstanceMatrix writes every matrix component");
  }
  quadbatch::get_quad_batch_instance_transform(out, readonly(matrix), 0);
  check(out->x == 5 && out->y == 6, "matrix transform reads return tx/ty at offsets four/five");
  double matrix_view_length = -1;
  quadbatch::iterate_quad_batch_instances(
      readonly(matrix), [&](double, double, flight::Float32Array view) {
        matrix_view_length = static_cast<double>(view.size());
        check(view.byte_length == 6 * sizeof(float), "matrix iterator exposes a twenty-four-byte view");
      });
  check(matrix_view_length == 6, "matrix iterator view has the exact six-element stride");
  check(quadbatch::get_quad_batch_transform_stride(flight::String("vector2")) == 2, "vector2 stride is two");
  check(quadbatch::get_quad_batch_transform_stride(flight::String("matrix3x2")) == 6, "matrix3x2 stride is six");
  quadbatch::set_quad_batch_instance_range(
      matrix, 0, 1, flight::Float32Array{1, 0, 0, 1, 7, 8});
  check(matrix->data->transforms.get_index(4) == 7,
        "matrix range writes tx at the five-slot offset");
  check(matrix->data->transforms.get_index(5) == 8,
        "matrix range writes ty at the six-slot offset");
}

void transform_conversion() {
  auto batch = quadbatch::create_quad_batch();
  quadbatch::append_quad_batch_instance(batch, 0, 5, 6);
  quadbatch::append_quad_batch_instance(batch, 1, 7, 8);
  const auto original_buffer = batch->data->transforms.buffer;
  quadbatch::set_quad_batch_transform_type(batch, flight::String("vector2"));
  check(batch->data->transforms.buffer == original_buffer, "setting the current transform type is a no-op");
  quadbatch::set_quad_batch_transform_type(batch, flight::String("matrix3x2"));
  check(batch->data->transform_type == flight::String("matrix3x2"), "vector2 converts to matrix3x2");
  if (inject_transform_layout_mutation) batch->data->transforms.set_index(0, 2);
  const std::vector<double> expanded{1, 0, 0, 1, 5, 6, 1, 0, 0, 1, 7, 8};
  for (std::size_t i = 0; i < expanded.size(); ++i) {
    check(batch->data->transforms.get_index(static_cast<double>(i)) == expanded[i],
          "vector2 expansion preserves exact identity-plus-translation order");
  }
  check(batch->data->transforms.byte_length >= 12 * sizeof(float), "expanded matrices retain the exact Float32 byte stride");

  quadbatch::set_quad_batch_instance_matrix(batch, 0, 0, 2, 0, 0, 2, 10, 20);
  quadbatch::set_quad_batch_instance_matrix(batch, 1, 1, 2, 0, 0, 2, 30, 40);
  quadbatch::set_quad_batch_transform_type(batch, flight::String("vector2"));
  check(batch->data->transform_type == flight::String("vector2"), "matrix3x2 converts to vector2");
  check(batch->data->transforms.get_index(0) == 10 && batch->data->transforms.get_index(1) == 20,
        "matrix collapse preserves first translation");
  check(batch->data->transforms.get_index(2) == 30 && batch->data->transforms.get_index(3) == 40,
        "matrix collapse preserves second translation");
}

void bounds_and_hit_testing() {
  auto out = rectangle(1, 2, 3, 4);
  auto empty = quadbatch::create_quad_batch();
  quadbatch::compute_quad_batch_local_bounds_rectangle(out, readonly(empty));
  check(out->x == 0 && out->y == 0 && out->width == 0 && out->height == 0,
        "compute bounds returns the exact zero rectangle without an atlas");

  auto r = region(0, 32, 16);
  auto vector = batch_with(atlas({r}), 2);
  vector->data->ids = flight::Uint16Array{0, 0};
  vector->data->transforms = flight::Float32Array{10, 20, 50, 100};
  quadbatch::compute_quad_batch_local_bounds_rectangle(out, readonly(vector));
  check(out->x == 10 && out->y == 20, "vector bounds retain the minimum translation");
  check(out->width == 72 && out->height == 96, "vector bounds span every live quad");
  check(quadbatch::hit_test_quad_batch_point_xy(readonly(vector), 15, 25) == 0, "AABB hit returns the first vector quad");
  check(quadbatch::hit_test_quad_batch_point_xy(readonly(vector), 60, 110) == 1, "AABB hit returns the second vector quad");
  check(quadbatch::hit_test_quad_batch_point_xy(readonly(vector), 0, 0) == -1, "AABB miss returns -1");
  check(quadbatch::hit_test_quad_batch_point_xy(readonly(vector), 42, 20) == -1,
        "AABB right edge is exclusive");
  check(quadbatch::hit_test_quad_batch_point(readonly(vector), readonly(point(15, 25))) == 0,
        "point overload delegates to XY hit testing");
  check(quadbatch::hit_test_quad_batch_point_exact(readonly(vector), readonly(point(5, 5))) == -1,
        "exact point overload delegates to exact XY hit testing");

  auto rotated_region = region(0, 10, 20);
  rotated_region->rotation = flight::types::TextureAtlasRotation::Clockwise90;
  auto rotated = batch_with(atlas({rotated_region}), 1);
  rotated->data->ids = flight::Uint16Array{0};
  rotated->data->transforms = flight::Float32Array{5, 7};
  quadbatch::compute_quad_batch_local_bounds_rectangle(out, readonly(rotated));
  check(out->x == 5 && out->y == 7 && out->width == 20 && out->height == 10,
        "rotated atlas regions use their upright logical extent");
  check(quadbatch::hit_test_quad_batch_point_exact_xy(readonly(rotated), 15, 8) == 0,
        "exact vector hit uses the rotated logical extent");
  check(quadbatch::hit_test_quad_batch_point_exact_xy(readonly(rotated), 5, 18) == -1,
        "exact vector hit rejects the pre-rotation extent");

  auto invalid = batch_with(atlas({region()}), 1);
  invalid->data->ids = flight::Uint16Array{99};
  invalid->data->transforms = flight::Float32Array{0, 0};
  check(quadbatch::hit_test_quad_batch_point_xy(readonly(invalid), 5, 5) == -1,
        "AABB hit ignores out-of-range region ids");
  check(quadbatch::hit_test_quad_batch_point_exact_xy(readonly(invalid), 5, 5) == -1,
        "exact hit ignores out-of-range region ids");
  quadbatch::compute_quad_batch_local_bounds_rectangle(out, readonly(invalid));
  check(out->width == 0 && out->height == 0, "bounds ignore out-of-range region ids");

  auto matrix = batch_with(atlas({region(0, 10, 10)}), 1, flight::String("matrix3x2"));
  matrix->data->ids = flight::Uint16Array{0};
  matrix->data->transforms = flight::Float32Array{1, 0, 0, 1, 5, 5};
  quadbatch::compute_quad_batch_local_bounds_rectangle(out, readonly(matrix));
  check(out->x == 5 && out->y == 5 && out->width == 10 && out->height == 10,
        "matrix bounds compute the transformed quad AABB");
  check(quadbatch::hit_test_quad_batch_point_xy(readonly(matrix), 10, 10) == 0,
        "matrix AABB hit returns its instance index");

  const float half = 0.70710677f;
  auto diamond = batch_with(atlas({region(0, 100, 100)}), 1, flight::String("matrix3x2"));
  diamond->data->ids = flight::Uint16Array{0};
  diamond->data->transforms = flight::Float32Array{half, half, -half, half, 0, 0};
  check(quadbatch::hit_test_quad_batch_point_xy(readonly(diamond), 60, 5) == 0,
        "matrix AABB intentionally includes a rotated-quad corner");
  check(quadbatch::hit_test_quad_batch_point_exact_xy(readonly(diamond), 60, 5) == -1,
        "exact matrix hit rejects an AABB-only corner");
  check(quadbatch::hit_test_quad_batch_point_exact_xy(readonly(diamond), 0, 70) == 0,
        "exact matrix hit accepts a point inside the rotated quad");

  auto overlapping = batch_with(atlas({region()}), 2);
  overlapping->data->ids = flight::Uint16Array{0, 0};
  overlapping->data->transforms = flight::Float32Array{0, 0, 0, 0};
  check(quadbatch::hit_test_quad_batch_point_xy(readonly(overlapping), 10, 10) == 0,
        "overlapping quads return the first live index");
  check(quadbatch::hit_test_quad_batch_point_exact_xy(readonly(empty), 0, 0) == -1,
        "exact hit returns -1 without an atlas");
  auto atlas_without_instances = batch_with(atlas({region()}));
  check(quadbatch::hit_test_quad_batch_point_exact_xy(readonly(atlas_without_instances), 0, 0) == -1,
        "exact hit returns -1 when instanceCount is zero");
}

void removal_signals_and_failure_paths() {
  auto batch = quadbatch::create_quad_batch();
  quadbatch::append_quad_batch_instance(batch, 0, 0, 0);
  quadbatch::append_quad_batch_instance(batch, 1, 10, 10);
  quadbatch::append_quad_batch_instance(batch, 2, 20, 20);
  quadbatch::remove_quad_batch_instance(batch, 0);
  check(batch->data->instance_count == 2, "removeQuadBatchInstance decrements instanceCount");
  check(quadbatch::get_quad_batch_instance_id(readonly(batch), 0) == 2,
        "removeQuadBatchInstance swap-removes from the last slot");
  quadbatch::remove_quad_batch_instance(batch, -1);
  quadbatch::remove_quad_batch_instance(batch, 2);
  check(batch->data->instance_count == 2, "removeQuadBatchInstance no-ops outside the live range");

  auto last = quadbatch::create_quad_batch();
  quadbatch::append_quad_batch_instance(last, 5, 50, 50);
  const auto last_ids_buffer = last->data->ids.buffer;
  const auto last_transforms_buffer = last->data->transforms.buffer;
  auto last_signals = quadbatch::enable_quad_batch_signals(last);
  std::tuple<double, double> last_removed{-2, -2};
  flight::signals::connect_signal(
      last_signals->on_instance_removed,
      std::function<void(double, double)>(
          [&](double index, double swap) { last_removed = {index, swap}; }));
  quadbatch::remove_quad_batch_instance(last, 0);
  bool last_removal_transition =
      last->data->instance_count == 0 &&
      last->data->ids.buffer == last_ids_buffer &&
      last->data->transforms.buffer == last_transforms_buffer &&
      last_removed == std::tuple<double, double>{0, -1};
  const double reused_index =
      quadbatch::append_quad_batch_instance(last, 6, 60, 70);
  last_removal_transition =
      last_removal_transition && reused_index == 0 &&
      last->data->instance_count == 1 &&
      quadbatch::get_quad_batch_instance_id(readonly(last), 0) == 6 &&
      last->data->transforms.get_index(0) == 60 &&
      last->data->transforms.get_index(1) == 70 &&
      last->data->ids.buffer == last_ids_buffer &&
      last->data->transforms.buffer == last_transforms_buffer;
  if (inject_last_removal_mutation) last_removal_transition = false;
  check(last_removal_transition,
        "removeQuadBatchInstance transitions the last slot to empty, emits no swap source, and reuses storage");

  auto signaled = quadbatch::create_quad_batch();
  check(!quadbatch::get_quad_batch_signals(readonly(signaled)).has_value(), "signals are absent before enable");
  auto signals = quadbatch::enable_quad_batch_signals(signaled);
  check(signals == quadbatch::enable_quad_batch_signals(signaled), "enableQuadBatchSignals is identity-stable");
  check(quadbatch::get_quad_batch_signals(readonly(signaled)).value() == signals,
        "getQuadBatchSignals returns the attached group");
  double appended = -1;
  std::tuple<double, double> removed{-1, -1};
  bool cleared = false;
  flight::signals::connect_signal(
      signals->on_instance_appended,
      std::function<void(double)>([&](double index) { appended = index; }));
  flight::signals::connect_signal(
      signals->on_instance_removed,
      std::function<void(double, double)>([&](double index, double swap) { removed = {index, swap}; }));
  flight::signals::connect_signal(
      signals->on_cleared,
      std::function<void()>([&]() { cleared = true; }));
  quadbatch::append_quad_batch_instance(signaled, 1, 0, 0);
  quadbatch::append_quad_batch_instance(signaled, 2, 0, 0);
  check(appended == 1, "append signal receives the exact appended index");
  quadbatch::remove_quad_batch_instance(signaled, 0);
  check(removed == std::tuple<double, double>{0, 1}, "remove signal receives index and swap source");
  quadbatch::clear_quad_batch(signaled);
  check(cleared, "clear signal fires synchronously");

  auto resized = quadbatch::create_quad_batch();
  quadbatch::resize_quad_batch(resized, 100);
  check(resized->data->ids.size() == 100 && resized->data->transforms.size() == 200,
        "resizeQuadBatch allocates exact vector2 capacity");
  check(resized->data->instance_count == 100, "resizeQuadBatch sets instanceCount");
  auto shrinking = quadbatch::create_quad_batch();
  shrinking->data->instance_count = 200;
  quadbatch::resize_quad_batch(shrinking, 100);
  check(shrinking->data->ids.size() == 0 && shrinking->data->transforms.size() == 0,
        "resizeQuadBatch trusts a shrinking logical count and does not allocate");
  check(shrinking->data->instance_count == 100, "shrinking resize still updates instanceCount");

  auto huge_atlas = atlas({region()});
  auto texture = flight::make_ref<flight::types::Texture2D>();
  huge_atlas->texture = texture;
  auto huge = batch_with(huge_atlas);
  quadbatch::resize_quad_batch(huge, 300000);
  check(huge->data->instance_count == 300000, "resizeQuadBatch retains a 300k logical instance count");
  check(!huge->data->material_data.has_value(), "300k untinted instances allocate no material array");
  check(huge->data->atlas.value()->texture.value() == texture,
        "300k instances retain one atlas texture identity");
  check(huge->data->ids.size() == 300000, "300k ids remain a Uint16Array allocation");
  check(huge->data->transforms.size() == 600000,
        "300k vector transforms remain a Float32Array allocation");
}

void specialized_runtime_boundaries() {
  auto batch = batch_with(std::nullopt, 3);
  check(!batch->data->material_data.has_value(), "tint storage is lazy");
  quadbatch::set_quad_batch_instance_tint(batch, 1, 0x12345678);
  check(batch->data->material_data.has_value(), "setQuadBatchInstanceTint allocates material slots");
  check(batch->data->material_data.value().size() == 3, "tint material array matches instanceCount");
  check(!batch->data->material_data.value().element(0).has_value(), "untinted slot zero remains null");
  check(!batch->data->material_data.value().element(2).has_value(), "untinted slot two remains null");
  const auto tint = quadbatch::get_quad_batch_instance_tint(batch, 1);
  check(tint.has_value() && tint.value()->tint == 0x12345678,
        "the exact-owner tint accessor preserves packed RGBA");
  auto out_of_range = batch_with(std::nullopt, 1);
  quadbatch::set_quad_batch_instance_tint(out_of_range, 2, 0xffffffff);
  check(!out_of_range->data->material_data.has_value(), "out-of-range tint writes do not allocate");

  auto runtime = quadbatch::get_quad_batch_runtime(readonly(batch));
  check(runtime == quadbatch::get_quad_batch_runtime(readonly(batch)), "getQuadBatchRuntime is identity-stable");
  check(!runtime->local_bounds_rectangle.has_value(), "specialized local bounds start absent");
  const double revision = batch->entity_runtime_key.value()->local_bounds_id;
  auto source = rectangle(10, 20, 32, 16);
  quadbatch::set_quad_batch_local_bounds_rectangle(batch, readonly(source));
  source->width = 999;
  check(runtime->local_bounds_rectangle.has_value(), "setting local bounds creates specialized storage");
  check(runtime->local_bounds_rectangle.value()->x == 10 &&
            runtime->local_bounds_rectangle.value()->y == 20 &&
            runtime->local_bounds_rectangle.value()->width == 32 &&
            runtime->local_bounds_rectangle.value()->height == 16,
        "local bounds are copied exactly and do not alias the source rectangle");
  check(batch->entity_runtime_key.value()->local_bounds_id != revision,
        "setting local bounds invalidates the generic node revision");
  auto copied = rectangle();
  quadbatch::copy_local_bounds_rectangle(copied, readonly(batch));
  check(copied->x == 10 && copied->y == 20 && copied->width == 32 && copied->height == 16,
        "the exact-owner bounds callback copies stored bounds");
}

}  // namespace

int main(int argc, char** argv) {
  inject_transform_layout_mutation =
      argc == 2 && flight::String(argv[1]) == flight::String("--mutate-layout");
  inject_last_removal_mutation =
      argc == 2 &&
      flight::String(argv[1]) == flight::String("--mutate-last-removal");
  construction_and_initializers();
  append_capacity_and_byte_layout();
  clone_compaction_and_ordering();
  transforms_iteration_and_ranges();
  transform_conversion();
  bounds_and_hit_testing();
  removal_signals_and_failure_paths();
  specialized_runtime_boundaries();
  return failures == 0 ? 0 : 1;
}
