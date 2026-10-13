#include <flight/mesh/mesh_geometry.hpp>
#include <flight/mesh/mesh_geometry_attributes.hpp>
#include <flight/mesh/mesh_geometry_builders.hpp>
#include <flight/mesh/mesh_geometry_compute.hpp>
#include <flight/mesh/mesh_geometry_index.hpp>
#include <flight/mesh/mesh_geometry_layout.hpp>
#include <flight/mesh/mesh_geometry_operations.hpp>
#include <flight/mesh/mesh_geometry_subset.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <functional>
#include <iostream>
#include <limits>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

namespace {

namespace mesh = flight::mesh;
namespace types = flight::types;

int failures = 0;
bool inject_owner_identity_mutation = false;
bool inject_bounds_mutation = false;
bool inject_index_order_mutation = false;
bool inject_failure_mutation = false;
bool inject_channel_order_mutation = false;
bool inject_stride_offset_mutation = false;
bool inject_normalization_mutation = false;
bool inject_group_bounds_mutation = false;
bool inject_builder_topology_mutation = false;
bool inject_builder_seam_mutation = false;
bool inject_builder_bounds_mutation = false;
bool inject_builder_guard_mutation = false;
bool inject_weld_equivalence_mutation = false;
bool inject_index_remap_mutation = false;
bool inject_subset_boundary_mutation = false;
bool inject_merge_aliasing_mutation = false;
bool inject_validation_branch_mutation = false;

void check(bool condition, const char* message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

bool near(double actual, double expected, double tolerance = 1.0e-5) {
  return std::abs(actual - expected) <= tolerance;
}

flight::Ref<types::VertexAttribute> vertex_attribute(
    const char* semantic, const char* format, double byte_offset) {
  auto attribute = flight::make_ref<types::VertexAttribute>();
  attribute->byte_offset = byte_offset;
  attribute->format = flight::String(format);
  attribute->semantic = flight::String(semantic);
  return attribute;
}

flight::Ref<types::VertexAttributeLayout> vertex_layout(
    flight::Array<flight::Ref<types::VertexAttribute>> attributes,
    double stride) {
  auto layout = flight::make_ref<types::VertexAttributeLayout>();
  layout->attributes = std::move(attributes);
  layout->stride = stride;
  return layout;
}

flight::Ref<types::VertexAttributeLayout> position_layout(double stride = 12.0) {
  return vertex_layout(
      flight::Array<flight::Ref<types::VertexAttribute>>{
          vertex_attribute("position", "float32x3", 0.0)},
      stride);
}

flight::Ref<types::MeshSubset> mesh_subset(
    double index_count, double index_offset) {
  return flight::make_ref<types::MeshSubset>(types::MeshSubset{
      .index_count = index_count, .index_offset = index_offset});
}

flight::Ref<types::MeshGeometry> geometry_from(
    const flight::Ref<types::VertexAttributeLayout>& layout,
    const flight::Float32Array& vertices) {
  auto options = flight::make_ref<types::MeshGeometryOptions>();
  options->layout = layout;
  options->vertices = vertices;
  return mesh::create_mesh_geometry(options);
}

flight::Ref<types::MeshGeometry> canonical_triangle(bool flipped_v = false) {
  flight::Float32Array vertices(36.0);
  const double positions[][2] = {{0.0, 0.0}, {1.0, 0.0}, {0.0, 1.0}};
  const double uvs[][2] = {
      {0.0, 0.0}, {1.0, 0.0}, {0.0, flipped_v ? -1.0 : 1.0}};
  for (double vertex = 0.0; vertex < 3.0; vertex += 1.0) {
    const auto index = static_cast<std::size_t>(vertex);
    const double base = vertex * 12.0;
    vertices.set_index(base, positions[index][0]);
    vertices.set_index(base + 1.0, positions[index][1]);
    vertices.set_index(base + 2.0, 0.0);
    vertices.set_index(base + 10.0, uvs[index][0]);
    vertices.set_index(base + 11.0, uvs[index][1]);
  }
  flight::Uint16Array indices{0, 1, 2};
  auto options = flight::make_ref<types::MeshGeometryOptions>();
  options->indices = flight::SequenceView<double>(indices);
  options->layout = mesh::canonical_mesh_geometry_layout;
  options->vertices = vertices;
  return mesh::create_mesh_geometry(options);
}

flight::Ref<types::MeshGeometry> seam_fold() {
  flight::Float32Array vertices(72.0);
  const double records[][5] = {
      {0.0, 0.0, 0.0, 0.0, 0.0},
      {2.0, 0.0, 0.0, 1.0, 0.0},
      {0.0, 2.0, 0.0, 0.0, 1.0},
      {0.0, 0.0, 0.0, 0.25, 0.25},
      {2.0, 0.0, 0.0, 0.75, 0.25},
      {0.0, 0.0, 1.0, 0.25, 0.75},
  };
  for (double vertex = 0.0; vertex < 6.0; vertex += 1.0) {
    const auto index = static_cast<std::size_t>(vertex);
    const double base = vertex * 12.0;
    vertices.set_index(base, records[index][0]);
    vertices.set_index(base + 1.0, records[index][1]);
    vertices.set_index(base + 2.0, records[index][2]);
    vertices.set_index(base + 10.0, records[index][3]);
    vertices.set_index(base + 11.0, records[index][4]);
  }
  flight::Uint16Array indices{0, 1, 2, 3, 4, 5};
  auto options = flight::make_ref<types::MeshGeometryOptions>();
  options->indices = flight::SequenceView<double>(indices);
  options->layout = mesh::canonical_mesh_geometry_layout;
  options->vertices = vertices;
  return mesh::create_mesh_geometry(options);
}

struct PackedFixture {
  flight::Float32Array backing;
  flight::Float32Array vertices;
  flight::Ref<types::VertexAttributeLayout> layout;
  flight::Ref<types::MeshGeometry> geometry;
};

PackedFixture packed_fixture() {
  flight::Float32Array backing(18.0);
  flight::DataView backing_view(backing.buffer);
  backing_view.set_uint8(0.0, 173.0);
  flight::Float32Array vertices(backing.buffer, 8.0, 16.0);
  auto layout = vertex_layout(
      flight::Array<flight::Ref<types::VertexAttribute>>{
          vertex_attribute("uv1", "float32x2", 20.0),
          vertex_attribute("weights0", "unorm8x4", 0.0),
          vertex_attribute("color0", "unorm8x4", 12.0),
          vertex_attribute("joints0", "uint16x4", 4.0),
      },
      32.0);
  flight::DataView view(vertices.buffer, static_cast<double>(vertices.byte_offset),
                        static_cast<double>(vertices.byte_length));
  const double base = 32.0;
  view.set_uint8(base, 255.0);
  view.set_uint8(base + 1.0, 128.0);
  view.set_uint8(base + 2.0, 64.0);
  view.set_uint8(base + 3.0, 0.0);
  view.set_uint16(base + 4.0, 3.0, true);
  view.set_uint16(base + 6.0, 400.0, true);
  view.set_uint16(base + 8.0, 65535.0, true);
  view.set_uint16(base + 10.0, 9.0, true);
  view.set_uint8(base + 12.0, 0.0);
  view.set_uint8(base + 13.0, 64.0);
  view.set_uint8(base + 14.0, 128.0);
  view.set_uint8(base + 15.0, 255.0);
  view.set_float32(base + 20.0, 0.25, true);
  view.set_float32(base + 24.0, 0.75, true);
  return PackedFixture{
      .backing = backing,
      .vertices = vertices,
      .layout = layout,
      .geometry = geometry_from(layout, vertices),
  };
}

flight::Ref<types::MeshGeometry> indexed_geometry() {
  flight::Float32Array vertices{
      -2.0F, 1.0F, 3.0F,
      4.0F, -5.0F, 6.0F,
      1.0F, 2.0F, -7.0F,
      3.0F, 0.0F, 2.0F,
  };
  flight::Uint16Array indices{2, 0, 1, 2, 1, 3};
  auto options = flight::make_ref<types::MeshGeometryOptions>();
  options->indices = flight::SequenceView<double>(indices);
  options->layout = position_layout();
  options->vertices = vertices;
  return mesh::create_mesh_geometry(options);
}

double index_at(const flight::Ref<types::MeshGeometry>& geometry, double index) {
  return std::visit(
      [index](const auto& values) { return values.get_index(index); },
      geometry->indices.value());
}

double variant_index_at(
    const std::variant<flight::Uint16Array, flight::Uint32Array>& indices,
    double index) {
  return std::visit(
      [index](const auto& values) { return values.get_index(index); }, indices);
}

const void* index_buffer_identity(
    const flight::Ref<types::MeshGeometry>& geometry) {
  if (!geometry->indices.has_value()) return nullptr;
  return std::visit(
      [](const auto& values) { return values.buffer.identity(); },
      geometry->indices.value());
}

bool has_finite_unit_frames(
    const flight::Ref<types::MeshGeometry>& geometry) {
  const double count = mesh::get_mesh_geometry_vertex_count(geometry);
  for (double vertex = 0.0; vertex < count; vertex += 1.0) {
    const double base = vertex * 12.0;
    for (double component = 0.0; component < 12.0; component += 1.0) {
      if (!std::isfinite(geometry->vertices.get_index(base + component))) {
        return false;
      }
    }
    const double nx = geometry->vertices.get_index(base + 3.0);
    const double ny = geometry->vertices.get_index(base + 4.0);
    const double nz = geometry->vertices.get_index(base + 5.0);
    const double tx = geometry->vertices.get_index(base + 6.0);
    const double ty = geometry->vertices.get_index(base + 7.0);
    const double tz = geometry->vertices.get_index(base + 8.0);
    const double tw = geometry->vertices.get_index(base + 9.0);
    if (!near(std::hypot(nx, ny, nz), 1.0, 1.0e-4) ||
        !near(std::hypot(tx, ty, tz), 1.0, 1.0e-4) ||
        !near(nx * tx + ny * ty + nz * tz, 0.0, 1.0e-4) ||
        std::abs(tw) != 1.0) {
      return false;
    }
  }
  return true;
}

bool has_tight_cached_bounds(
    const flight::Ref<types::MeshGeometry>& geometry) {
  if (!geometry->bounds.has_value() ||
      !geometry->entity_runtime_key.has_value()) {
    return false;
  }
  double min_x = std::numeric_limits<double>::infinity();
  double min_y = std::numeric_limits<double>::infinity();
  double min_z = std::numeric_limits<double>::infinity();
  double max_x = -std::numeric_limits<double>::infinity();
  double max_y = -std::numeric_limits<double>::infinity();
  double max_z = -std::numeric_limits<double>::infinity();
  const double count = mesh::get_mesh_geometry_vertex_count(geometry);
  for (double vertex = 0.0; vertex < count; vertex += 1.0) {
    const double base = vertex * 12.0;
    const double x = geometry->vertices.get_index(base);
    const double y = geometry->vertices.get_index(base + 1.0);
    const double z = geometry->vertices.get_index(base + 2.0);
    min_x = std::min(min_x, x);
    min_y = std::min(min_y, y);
    min_z = std::min(min_z, z);
    max_x = std::max(max_x, x);
    max_y = std::max(max_y, y);
    max_z = std::max(max_z, z);
  }
  const auto bounds = geometry->bounds.value();
  return bounds->min->x == min_x && bounds->min->y == min_y &&
         bounds->min->z == min_z && bounds->max->x == max_x &&
         bounds->max->y == max_y && bounds->max->z == max_z &&
         geometry->entity_runtime_key.value()->bounds_version ==
             geometry->version;
}

bool winding_matches_normals(
    const flight::Ref<types::MeshGeometry>& geometry) {
  auto corner = flight::make_ref<types::MeshTriangleVertexIndices>();
  bool saw_area = false;
  const double triangles = mesh::get_mesh_geometry_triangle_count(geometry);
  for (double triangle = 0.0; triangle < triangles; triangle += 1.0) {
    if (!mesh::get_mesh_geometry_triangle_vertex_indices(
            corner, geometry, triangle)) {
      return false;
    }
    const double indices[] = {corner->i0, corner->i1, corner->i2};
    double position[3][3]{};
    double average_normal[3]{};
    for (std::size_t slot = 0; slot < 3; ++slot) {
      const double base = indices[slot] * 12.0;
      for (std::size_t component = 0; component < 3; ++component) {
        position[slot][component] =
            geometry->vertices.get_index(base + component);
        average_normal[component] +=
            geometry->vertices.get_index(base + 3.0 + component);
      }
    }
    const double e1x = position[1][0] - position[0][0];
    const double e1y = position[1][1] - position[0][1];
    const double e1z = position[1][2] - position[0][2];
    const double e2x = position[2][0] - position[0][0];
    const double e2y = position[2][1] - position[0][1];
    const double e2z = position[2][2] - position[0][2];
    const double face_x = e1y * e2z - e1z * e2y;
    const double face_y = e1z * e2x - e1x * e2z;
    const double face_z = e1x * e2y - e1y * e2x;
    const double area = std::hypot(face_x, face_y, face_z);
    if (area <= 1.0e-10) continue;
    saw_area = true;
    if (face_x * average_normal[0] + face_y * average_normal[1] +
            face_z * average_normal[2] <=
        0.0) {
      return false;
    }
  }
  return saw_area;
}

template <typename Build>
bool throws_range_error(Build&& build) {
  try {
    build();
  } catch (const flight::RangeError&) {
    return true;
  } catch (...) {
    return false;
  }
  return false;
}

void construction_clone_and_owner_identity() {
  flight::Float32Array vertices{
      -2.0F, 1.0F, 3.0F,
      4.0F, -5.0F, 6.0F,
      1.0F, 2.0F, -7.0F,
      3.0F, 0.0F, 2.0F,
  };
  flight::Uint16Array authored_indices{2, 0, 1, 2, 1, 3};
  const auto layout = position_layout();
  auto options = flight::make_ref<types::MeshGeometryOptions>();
  options->indices = flight::SequenceView<double>(authored_indices);
  options->layout = layout;
  options->vertices = vertices;
  const auto source = mesh::create_mesh_geometry(options);
  const auto bounds = mesh::ensure_mesh_geometry_bounds(source);
  const auto clone = mesh::clone_mesh_geometry(source);

  bool identity =
      source->layout == layout &&
      source->vertices.buffer == vertices.buffer &&
      source->indices.has_value() &&
      std::holds_alternative<flight::Uint16Array>(source->indices.value()) &&
      std::get<flight::Uint16Array>(source->indices.value()).buffer !=
          authored_indices.buffer &&
      source->entity_runtime_key.has_value() &&
      clone != source && clone->layout == source->layout &&
      clone->vertices.buffer != source->vertices.buffer &&
      clone->indices.has_value() &&
      std::get<flight::Uint16Array>(clone->indices.value()).buffer !=
          std::get<flight::Uint16Array>(source->indices.value()).buffer &&
      clone->subsets.identity() != source->subsets.identity() &&
      clone->subsets.element(0.0) != source->subsets.element(0.0) &&
      clone->entity_runtime_key.has_value() &&
      clone->entity_runtime_key.value() != source->entity_runtime_key.value() &&
      bounds.has_value() && clone->bounds.has_value() &&
      clone->bounds.value() != source->bounds.value();

  clone->vertices.set_index(0.0, 99.0);
  std::get<flight::Uint16Array>(clone->indices.value()).set_index(0.0, 0.0);
  clone->subsets.element(0.0)->index_count = 3.0;
  const auto versioned = mesh::clone_mesh_geometry(source);
  versioned->version = 4294967295.0;
  mesh::invalidate_mesh_geometry(versioned);
  identity = identity && source->vertices.get_index(0.0) == -2.0 &&
             index_at(source, 0.0) == 2.0 &&
             source->subsets.element(0.0)->index_count == 6.0 &&
             versioned->version == 0.0;
  if (inject_owner_identity_mutation) identity = false;
  check(identity,
        "mesh construction retains authored owners while clone owns independent mutable storage, runtime and version state");
}

void bounds_cache_and_position_mutation() {
  const auto geometry = indexed_geometry();
  const auto first = mesh::ensure_mesh_geometry_bounds(geometry);
  const auto runtime = geometry->entity_runtime_key;
  bool bounds =
      first.has_value() && runtime.has_value() &&
      first.value() == geometry->bounds.value() &&
      first.value()->min->x == -2.0 && first.value()->min->y == -5.0 &&
      first.value()->min->z == -7.0 && first.value()->max->x == 4.0 &&
      first.value()->max->y == 2.0 && first.value()->max->z == 6.0 &&
      runtime.value()->bounds_version == geometry->version &&
      mesh::ensure_mesh_geometry_bounds(geometry).value() == first.value();

  const double before_version = geometry->version;
  bounds = bounds &&
           mesh::set_mesh_geometry_vertex_position(
               geometry, 0.0, -8.0, 9.0, -10.0) &&
           geometry->version == before_version + 1.0;
  const auto refreshed = mesh::ensure_mesh_geometry_bounds(geometry);
  bounds = bounds && refreshed.has_value() && refreshed.value() == first.value() &&
           refreshed.value()->min->x == -8.0 &&
           refreshed.value()->min->y == -5.0 &&
           refreshed.value()->min->z == -10.0 &&
           refreshed.value()->max->x == 4.0 &&
           refreshed.value()->max->y == 9.0 &&
           refreshed.value()->max->z == 6.0 &&
           runtime.value()->bounds_version == geometry->version;
  if (inject_bounds_mutation) bounds = false;
  check(bounds,
        "bounds are tight, cached by exact owner and version, then refreshed in place after position mutation");
}

void index_order_and_compaction() {
  const auto geometry = indexed_geometry();
  auto triangle = flight::make_ref<types::MeshTriangleVertexIndices>();
  bool order =
      mesh::get_mesh_geometry_triangle_count(geometry) == 2.0 &&
      mesh::get_mesh_geometry_triangle_vertex_indices(
          triangle, geometry, 0.0) &&
      triangle->i0 == 2.0 && triangle->i1 == 0.0 && triangle->i2 == 1.0 &&
      mesh::get_mesh_geometry_triangle_vertex_indices(
          triangle, geometry, 1.0) &&
      triangle->i0 == 2.0 && triangle->i1 == 1.0 && triangle->i2 == 3.0;

  const auto wireframe = mesh::compute_mesh_geometry_wireframe_indices(geometry);
  const double expected_wireframe[] = {
      2.0, 0.0, 0.0, 1.0, 1.0, 2.0,
      2.0, 1.0, 1.0, 3.0, 3.0, 2.0,
  };
  order = order && std::holds_alternative<flight::Uint16Array>(wireframe) &&
          std::get<flight::Uint16Array>(wireframe).size() == 12;
  for (double index = 0.0; index < 12.0; index += 1.0) {
    order = order && variant_index_at(wireframe, index) ==
                         expected_wireframe[static_cast<std::size_t>(index)];
  }

  const auto expanded = mesh::expand_mesh_geometry_indices(geometry);
  const double expected_expanded_x[] = {1.0, -2.0, 4.0, 1.0, 4.0, 3.0};
  order = order && !expanded->indices.has_value() &&
          mesh::get_mesh_geometry_vertex_count(expanded) == 6.0 &&
          expanded->layout == geometry->layout &&
          expanded->vertices.buffer != geometry->vertices.buffer;
  for (double vertex = 0.0; vertex < 6.0; vertex += 1.0) {
    order = order && expanded->vertices.get_index(vertex * 3.0) ==
                         expected_expanded_x[static_cast<std::size_t>(vertex)];
  }

  const auto compacted = mesh::compact_mesh_geometry_vertices(geometry);
  const double expected_compact_indices[] = {0.0, 1.0, 2.0, 0.0, 2.0, 3.0};
  const double expected_compact_x[] = {1.0, -2.0, 4.0, 3.0};
  order = order && compacted->indices.has_value() &&
          mesh::get_mesh_geometry_vertex_count(compacted) == 4.0;
  for (double index = 0.0; index < 6.0; index += 1.0) {
    order = order && index_at(compacted, index) ==
                         expected_compact_indices[static_cast<std::size_t>(index)];
  }
  for (double vertex = 0.0; vertex < 4.0; vertex += 1.0) {
    order = order && compacted->vertices.get_index(vertex * 3.0) ==
                         expected_compact_x[static_cast<std::size_t>(vertex)];
  }
  order = order && index_at(geometry, 0.0) == 2.0 &&
          geometry->vertices.get_index(0.0) == -2.0;
  if (inject_index_order_mutation) order = false;
  check(order,
        "triangle, wireframe, expansion and compaction preserve their exact source-defined index orders");
}

void failure_paths_are_non_mutating() {
  const auto geometry = indexed_geometry();
  auto triangle = flight::make_ref<types::MeshTriangleVertexIndices>();
  triangle->i0 = 7.0;
  triangle->i1 = 8.0;
  triangle->i2 = 9.0;
  const double version = geometry->version;
  const double position_x = geometry->vertices.get_index(0.0);
  bool failures_are_observed =
      !mesh::get_mesh_geometry_triangle_vertex_indices(
          triangle, geometry, -1.0) &&
      triangle->i0 == 7.0 && triangle->i1 == 8.0 && triangle->i2 == 9.0 &&
      !mesh::get_mesh_geometry_triangle_vertex_indices(
          triangle, geometry, 2.0) &&
      !mesh::set_mesh_geometry_vertex_position(
          geometry, -1.0, 10.0, 11.0, 12.0) &&
      !mesh::set_mesh_geometry_vertex_position(
          geometry, 4.0, 10.0, 11.0, 12.0) &&
      geometry->version == version &&
      geometry->vertices.get_index(0.0) == position_x;

  const auto invalid_index = mesh::clone_mesh_geometry(geometry);
  std::get<flight::Uint16Array>(invalid_index->indices.value())
      .set_index(0.0, 99.0);
  failures_are_observed = failures_are_observed &&
                          mesh::validate_mesh_geometry(geometry) &&
                          !mesh::validate_mesh_geometry(invalid_index);

  auto malformed_options = flight::make_ref<types::MeshGeometryOptions>();
  malformed_options->layout = position_layout(0.0);
  malformed_options->vertices = flight::Float32Array{1.0F, 2.0F, 3.0F};
  const auto malformed = mesh::create_mesh_geometry(malformed_options);
  failures_are_observed = failures_are_observed &&
                          mesh::get_mesh_geometry_vertex_count(malformed) == 0.0 &&
                          !mesh::validate_mesh_geometry(malformed);
  if (inject_failure_mutation) failures_are_observed = false;
  check(failures_are_observed,
        "invalid triangle, attribute, index and layout inputs fail without mutating valid geometry");
}

void weld_equivalence_and_attribute_sensitivity() {
  const auto layout = vertex_layout(
      flight::Array<flight::Ref<types::VertexAttribute>>{
          vertex_attribute("position", "float32x3", 0.0),
          vertex_attribute("color0", "unorm8x4", 12.0)},
      16.0);
  flight::Float32Array vertices(20.0);
  flight::DataView bytes(vertices.buffer);
  auto set_record = [&](double vertex, float x, double red, double green) {
    const double float_base = vertex * 4.0;
    const double byte_base = vertex * 16.0;
    vertices.set_index(float_base, x);
    vertices.set_index(float_base + 1.0, 2.0);
    vertices.set_index(float_base + 2.0, 3.0);
    bytes.set_uint8(byte_base + 12.0, red);
    bytes.set_uint8(byte_base + 13.0, green);
    bytes.set_uint8(byte_base + 14.0, 0.0);
    bytes.set_uint8(byte_base + 15.0, 255.0);
  };
  const float adjacent = std::nextafter(1.0F, 2.0F);
  set_record(0.0, 1.0F, 255.0, 0.0);
  set_record(1.0, 1.0F, 255.0, 0.0);
  set_record(2.0, adjacent, 255.0, 0.0);
  set_record(3.0, 1.0F, 0.0, 255.0);
  set_record(4.0, adjacent, 255.0, 0.0);
  auto options = flight::make_ref<types::MeshGeometryOptions>();
  options->layout = layout;
  options->topology = flight::String("line-strip");
  options->vertices = vertices;
  const auto source = mesh::create_mesh_geometry(options);
  const auto welded = mesh::weld_mesh_geometry_vertices(source);

  const double expected_indices[] = {0.0, 0.0, 1.0, 2.0, 1.0};
  bool equivalent =
      mesh::get_mesh_geometry_vertex_count(welded) == 3.0 &&
      welded->indices.has_value() &&
      std::holds_alternative<flight::Uint16Array>(welded->indices.value()) &&
      welded->topology == flight::String("line-strip") &&
      welded->vertices.buffer != source->vertices.buffer &&
      source->vertices.buffer == vertices.buffer && !source->indices.has_value();
  for (double index = 0.0; index < 5.0; index += 1.0) {
    equivalent = equivalent && index_at(welded, index) ==
                                   expected_indices[static_cast<std::size_t>(index)];
  }
  const flight::Uint8Array source_bytes(
      source->vertices.buffer, static_cast<double>(source->vertices.byte_offset),
      static_cast<double>(source->vertices.byte_length));
  const flight::Uint8Array welded_bytes(
      welded->vertices.buffer, static_cast<double>(welded->vertices.byte_offset),
      static_cast<double>(welded->vertices.byte_length));
  const double source_records[] = {0.0, 2.0, 3.0};
  for (double unique = 0.0; unique < 3.0; unique += 1.0) {
    for (double byte = 0.0; byte < 16.0; byte += 1.0) {
      equivalent = equivalent &&
                   welded_bytes.get_index(unique * 16.0 + byte) ==
                       source_bytes.get_index(
                           source_records[static_cast<std::size_t>(unique)] *
                               16.0 +
                           byte);
    }
  }
  equivalent = equivalent &&
               welded->vertices.get_index(4.0) == adjacent &&
               welded_bytes.get_index(44.0) == 0.0 &&
               welded_bytes.get_index(45.0) == 255.0;

  const auto empty = geometry_from(layout, flight::Float32Array(0.0));
  const auto welded_empty = mesh::weld_mesh_geometry_vertices(empty);
  equivalent = equivalent &&
               mesh::get_mesh_geometry_vertex_count(welded_empty) == 0.0 &&
               welded_empty->indices.has_value() &&
               std::visit([](const auto& values) { return values.empty(); },
                          welded_empty->indices.value());
  if (inject_weld_equivalence_mutation) equivalent = false;
  check(equivalent,
        "welding uses bit-exact complete records: exact duplicates collapse, adjacent float bits and differing attributes remain distinct, and empty input stays empty");
}

void weld_remap_order_and_index_width() {
  flight::Float32Array vertices{
      10.0F, 0.0F, 0.0F,
      20.0F, 0.0F, 0.0F,
      10.0F, 0.0F, 0.0F,
      30.0F, 0.0F, 0.0F,
      20.0F, 0.0F, 0.0F,
  };
  flight::Uint16Array authored{4, 2, 3, 0, 1, 4};
  auto options = flight::make_ref<types::MeshGeometryOptions>();
  options->indices = flight::SequenceView<double>(authored);
  options->layout = position_layout();
  options->subsets = flight::Array<flight::Ref<types::MeshSubset>>{
      mesh_subset(3.0, 0.0), mesh_subset(3.0, 3.0)};
  options->vertices = vertices;
  const auto source = mesh::create_mesh_geometry(options);
  const auto welded = mesh::weld_mesh_geometry_vertices(source);
  const double expected_indices[] = {1.0, 0.0, 2.0, 0.0, 1.0, 1.0};
  const double expected_vertices[] = {10.0, 20.0, 30.0};
  bool remapped = mesh::get_mesh_geometry_vertex_count(welded) == 3.0 &&
                  welded->indices.has_value() && welded->subsets.size() == 2 &&
                  welded->subsets.element(0.0)->index_count == 3.0 &&
                  welded->subsets.element(1.0)->index_offset == 3.0;
  for (double index = 0.0; index < 6.0; index += 1.0) {
    remapped = remapped && index_at(welded, index) ==
                               expected_indices[static_cast<std::size_t>(index)];
  }
  for (double vertex = 0.0; vertex < 3.0; vertex += 1.0) {
    remapped = remapped && welded->vertices.get_index(vertex * 3.0) ==
                               expected_vertices[static_cast<std::size_t>(vertex)];
  }

  const auto invalid = mesh::clone_mesh_geometry(source);
  std::get<flight::Uint16Array>(invalid->indices.value()).set_index(2.0, 99.0);
  const auto rejected = mesh::weld_mesh_geometry_vertices(invalid);
  remapped = remapped && rejected != invalid &&
             rejected->vertices.buffer != invalid->vertices.buffer &&
             index_buffer_identity(rejected) != index_buffer_identity(invalid) &&
             index_at(rejected, 2.0) == 99.0 && index_at(invalid, 2.0) == 99.0 &&
             rejected->subsets.identity() != invalid->subsets.identity();

  constexpr double narrow_count = 65535.0;
  constexpr double wide_count = 65536.0;
  flight::Float32Array narrow_vertices(narrow_count * 3.0);
  flight::Float32Array wide_vertices(wide_count * 3.0);
  for (double vertex = 0.0; vertex < narrow_count; vertex += 1.0) {
    narrow_vertices.set_index(vertex * 3.0, vertex);
  }
  for (double vertex = 0.0; vertex < wide_count; vertex += 1.0) {
    wide_vertices.set_index(vertex * 3.0, vertex);
  }
  const auto narrow_source = geometry_from(position_layout(), narrow_vertices);
  const auto wide_source = geometry_from(position_layout(), wide_vertices);
  const auto narrow_indexed = mesh::index_mesh_geometry_vertices(narrow_source);
  const auto wide_indexed = mesh::index_mesh_geometry_vertices(wide_source);
  remapped =
      remapped &&
      std::holds_alternative<flight::Uint16Array>(narrow_indexed->indices.value()) &&
      std::holds_alternative<flight::Uint32Array>(wide_indexed->indices.value()) &&
      index_at(narrow_indexed, narrow_count - 1.0) == narrow_count - 1.0 &&
      index_at(wide_indexed, wide_count - 1.0) == wide_count - 1.0 &&
      !narrow_source->indices.has_value() && !wide_source->indices.has_value();
  if (inject_index_remap_mutation) remapped = false;
  check(remapped,
        "welding retains first-record order and exact authored element order, rejects invalid remaps by deep clone, and sequential indexing switches width only above 65535 vertices");
}

void subset_boundaries_replacement_and_rebase() {
  const auto geometry = indexed_geometry();
  const double version = geometry->version;
  const auto vertex_owner = geometry->vertices.buffer;
  const void* index_owner = index_buffer_identity(geometry);
  const auto original_subsets = geometry->subsets;
  const auto appended = mesh_subset(3.0, 3.0);
  mesh::add_mesh_geometry_subset(geometry, appended);
  bool subsets = geometry->subsets.size() == 2 &&
                 geometry->subsets.identity() != original_subsets.identity() &&
                 geometry->subsets.element(0.0) != original_subsets.element(0.0) &&
                 geometry->subsets.element(1.0) != appended &&
                 geometry->subsets.element(1.0)->index_count == 3.0 &&
                 geometry->version == version &&
                 geometry->vertices.buffer == vertex_owner &&
                 index_buffer_identity(geometry) == index_owner;
  appended->index_offset = 99.0;
  subsets = subsets && geometry->subsets.element(1.0)->index_offset == 3.0;

  flight::Array<flight::Ref<types::MeshSubset>> replacement{
      mesh_subset(3.0, 0.0), mesh_subset(3.0, 3.0)};
  mesh::set_mesh_geometry_subsets(geometry, replacement);
  subsets = subsets && geometry->subsets.identity() != replacement.identity() &&
            geometry->subsets.element(0.0) != replacement.element(0.0) &&
            mesh::get_mesh_geometry_subset_triangle_count(geometry, 0.0) == 1.0 &&
            mesh::get_mesh_geometry_subset_triangle_count(geometry, 1.0) == 1.0 &&
            mesh::get_mesh_geometry_subset_triangle_count(geometry, -1.0) == 0.0 &&
            mesh::get_mesh_geometry_subset_triangle_count(geometry, 2.0) == 0.0 &&
            mesh::get_mesh_geometry_triangle_subset_index(geometry, 0.0) == 0.0 &&
            mesh::get_mesh_geometry_triangle_subset_index(geometry, 1.0) == 1.0 &&
            mesh::get_mesh_geometry_triangle_subset_index(geometry, 2.0) == -1.0 &&
            mesh::get_mesh_geometry_triangle_subset_index(geometry, -1.0) == -1.0;
  replacement.element(0.0)->index_count = 99.0;
  subsets = subsets && geometry->subsets.element(0.0)->index_count == 3.0;

  mesh::set_mesh_geometry_subsets(
      geometry, flight::Array<flight::Ref<types::MeshSubset>>{
                    mesh_subset(6.0, 0.0), mesh_subset(3.0, 3.0)});
  subsets = subsets &&
            mesh::get_mesh_geometry_triangle_subset_index(geometry, 1.0) == 0.0;
  mesh::set_mesh_geometry_subsets(
      geometry, flight::Array<flight::Ref<types::MeshSubset>>{
                    mesh_subset(2.0, 0.0), mesh_subset(3.0, 3.0)});
  subsets = subsets &&
            mesh::get_mesh_geometry_triangle_subset_index(geometry, 0.0) == -1.0 &&
            mesh::get_mesh_geometry_triangle_subset_index(geometry, 1.0) == 1.0;

  const auto strip = indexed_geometry();
  strip->topology = flight::String("triangle-strip");
  strip->indices = flight::Uint16Array{0, 1, 2, 3};
  mesh::set_mesh_geometry_subsets(
      strip, flight::Array<flight::Ref<types::MeshSubset>>{
                 mesh_subset(3.0, 0.0), mesh_subset(3.0, 1.0)});
  subsets = subsets &&
            mesh::get_mesh_geometry_subset_triangle_count(strip, 0.0) == 1.0 &&
            mesh::get_mesh_geometry_subset_triangle_count(strip, 1.0) == 1.0 &&
            mesh::get_mesh_geometry_triangle_subset_index(strip, 0.0) == 0.0 &&
            mesh::get_mesh_geometry_triangle_subset_index(strip, 1.0) == 1.0;
  strip->topology = flight::String("line-list");
  subsets = subsets &&
            mesh::get_mesh_geometry_subset_triangle_count(strip, 0.0) == 0.0 &&
            mesh::get_mesh_geometry_triangle_subset_index(strip, 0.0) == -1.0 &&
            geometry->version == version &&
            geometry->vertices.buffer == vertex_owner &&
            index_buffer_identity(geometry) == index_owner;
  if (inject_subset_boundary_mutation) subsets = false;
  check(subsets,
        "subset add/set deep-copy ranges without touching buffers or version, list and strip lookup require whole-triangle containment, overlap chooses the first declaration, and gaps stay unowned");
}

void merge_order_rebase_and_owner_isolation() {
  const auto first = indexed_geometry();
  first->subsets = flight::Array<flight::Ref<types::MeshSubset>>{
      mesh_subset(3.0, 0.0), mesh_subset(3.0, 3.0)};
  flight::Float32Array second_vertices{
      10.0F, 0.0F, 0.0F,
      11.0F, 0.0F, 0.0F,
      10.0F, 1.0F, 0.0F,
  };
  auto second_options = flight::make_ref<types::MeshGeometryOptions>();
  second_options->layout = first->layout;
  second_options->vertices = second_vertices;
  const auto second = mesh::create_mesh_geometry(second_options);
  const auto first_bounds = mesh::ensure_mesh_geometry_bounds(first);
  const auto second_bounds = mesh::ensure_mesh_geometry_bounds(second);
  const auto merged_optional = mesh::merge_mesh_geometries(
      flight::Array<flight::Ref<types::MeshGeometry>>{first, second});
  bool merged_ok = merged_optional.has_value();
  const auto merged = merged_optional.value();
  const double expected_indices[] = {
      2.0, 0.0, 1.0, 2.0, 1.0, 3.0, 4.0, 5.0, 6.0};
  merged_ok = merged_ok && merged->layout == first->layout &&
              merged->topology == flight::String("triangle-list") &&
              mesh::get_mesh_geometry_vertex_count(merged) == 7.0 &&
              mesh::get_mesh_geometry_index_count(merged) == 9.0 &&
              merged->vertices.buffer != first->vertices.buffer &&
              merged->vertices.buffer != second->vertices.buffer &&
              index_buffer_identity(merged) != index_buffer_identity(first) &&
              merged->entity_runtime_key.has_value() &&
              merged->entity_runtime_key.value() !=
                  first->entity_runtime_key.value() &&
              merged->entity_runtime_key.value() !=
                  second->entity_runtime_key.value() &&
              merged->subsets.size() == 3 &&
              merged->subsets.identity() != first->subsets.identity() &&
              merged->subsets.identity() != second->subsets.identity() &&
              merged->subsets.element(0.0) != first->subsets.element(0.0) &&
              merged->subsets.element(2.0) != second->subsets.element(0.0) &&
              merged->subsets.element(0.0)->index_offset == 0.0 &&
              merged->subsets.element(1.0)->index_offset == 3.0 &&
              merged->subsets.element(2.0)->index_offset == 6.0 &&
              merged->subsets.element(0.0)->index_count == 3.0 &&
              merged->subsets.element(1.0)->index_count == 3.0 &&
              merged->subsets.element(2.0)->index_count == 3.0 &&
              merged->bounds.has_value() &&
              first_bounds.has_value() && second_bounds.has_value() &&
              merged->bounds.value() != first_bounds.value() &&
              merged->bounds.value() != second_bounds.value() &&
              merged->bounds.value()->min->x == -2.0 &&
              merged->bounds.value()->min->y == -5.0 &&
              merged->bounds.value()->min->z == -7.0 &&
              merged->bounds.value()->max->x == 11.0 &&
              merged->bounds.value()->max->y == 2.0 &&
              merged->bounds.value()->max->z == 6.0;
  for (double index = 0.0; index < 9.0; index += 1.0) {
    merged_ok = merged_ok && index_at(merged, index) ==
                                 expected_indices[static_cast<std::size_t>(index)];
  }
  for (double value = 0.0;
       value < static_cast<double>(first->vertices.size()); value += 1.0) {
    merged_ok = merged_ok && merged->vertices.get_index(value) ==
                                 first->vertices.get_index(value);
  }
  for (double value = 0.0;
       value < static_cast<double>(second->vertices.size()); value += 1.0) {
    merged_ok = merged_ok &&
                merged->vertices.get_index(
                    static_cast<double>(first->vertices.size()) + value) ==
                    second->vertices.get_index(value);
  }

  const auto mismatched_layout = geometry_from(
      position_layout(16.0), flight::Float32Array(12.0));
  const auto mismatched_topology = mesh::clone_mesh_geometry(second);
  mismatched_topology->topology = flight::String("line-list");
  const auto first_vertex_owner = first->vertices.buffer;
  const void* first_index_owner = index_buffer_identity(first);
  const auto first_subset_owner = first->subsets.identity();
  const auto first_subset = first->subsets.element(0.0);
  const double first_version = first->version;
  merged_ok =
      merged_ok &&
      !mesh::merge_mesh_geometries(
           flight::Array<flight::Ref<types::MeshGeometry>>{}).has_value() &&
      !mesh::merge_mesh_geometries(
           flight::Array<flight::Ref<types::MeshGeometry>>{
               first, mismatched_layout}).has_value() &&
      !mesh::merge_mesh_geometries(
           flight::Array<flight::Ref<types::MeshGeometry>>{
               first, mismatched_topology}).has_value() &&
      first->vertices.buffer == first_vertex_owner &&
      index_buffer_identity(first) == first_index_owner &&
      first->subsets.identity() == first_subset_owner &&
      first->subsets.element(0.0) == first_subset &&
      first->subsets.element(0.0)->index_count == 3.0 &&
      first->version == first_version;

  const auto empty = geometry_from(first->layout, flight::Float32Array(0.0));
  const auto merged_empty = mesh::merge_mesh_geometries(
      flight::Array<flight::Ref<types::MeshGeometry>>{empty});
  merged_ok = merged_ok && merged_empty.has_value() &&
              merged_empty.value() != empty &&
              merged_empty.value()->vertices.buffer != empty->vertices.buffer &&
              mesh::get_mesh_geometry_vertex_count(merged_empty.value()) == 0.0 &&
              mesh::get_mesh_geometry_index_count(merged_empty.value()) == 0.0 &&
              merged_empty.value()->subsets.size() == 1 &&
              merged_empty.value()->subsets.element(0.0)->index_count == 0.0 &&
              mesh::validate_mesh_geometry(merged_empty.value());

  first->vertices.set_index(0.0, 90.0);
  merged->vertices.set_index(12.0, 80.0);
  std::visit([](auto& values) { values.set_index(0.0, 0.0); },
             merged->indices.value());
  first->subsets.element(0.0)->index_count = 6.0;
  merged_ok = merged_ok && merged->vertices.get_index(0.0) == -2.0 &&
              second->vertices.get_index(0.0) == 10.0 &&
              index_at(first, 0.0) == 2.0 &&
              merged->subsets.element(0.0)->index_count == 3.0;
  if (inject_merge_aliasing_mutation) merged_ok = false;
  check(merged_ok,
        "merge concatenates exact records and element order, rebases every source subset, recomputes union bounds, isolates all mutable owners, accepts empty geometry, and refuses empty/mismatched requests without mutation");
}

bool validation_result_without_mutation(
    const flight::Ref<types::MeshGeometry>& geometry, bool expected) {
  const auto vertex_owner = geometry->vertices.buffer;
  const void* indices_owner = index_buffer_identity(geometry);
  const auto subsets_owner = geometry->subsets.identity();
  const auto layout = geometry->layout;
  const auto bounds = geometry->bounds;
  const auto runtime = geometry->entity_runtime_key;
  const auto topology = geometry->topology;
  const double version = geometry->version;
  std::vector<double> vertices;
  for (double index = 0.0;
       index < static_cast<double>(geometry->vertices.size()); index += 1.0) {
    vertices.push_back(geometry->vertices.get_index(index));
  }
  std::vector<double> indices;
  if (geometry->indices.has_value()) {
    const double count = std::visit(
        [](const auto& values) { return static_cast<double>(values.size()); },
        geometry->indices.value());
    for (double index = 0.0; index < count; index += 1.0) {
      indices.push_back(index_at(geometry, index));
    }
  }
  std::vector<flight::Ref<types::MeshSubset>> subset_refs;
  std::vector<std::pair<double, double>> subset_values;
  for (const auto& subset : geometry->subsets) {
    subset_refs.push_back(subset);
    subset_values.emplace_back(subset->index_count, subset->index_offset);
  }

  bool unchanged = mesh::validate_mesh_geometry(geometry) == expected &&
                   geometry->vertices.buffer == vertex_owner &&
                   index_buffer_identity(geometry) == indices_owner &&
                   geometry->subsets.identity() == subsets_owner &&
                   geometry->layout == layout && geometry->bounds == bounds &&
                   geometry->entity_runtime_key == runtime &&
                   geometry->topology == topology && geometry->version == version &&
                   geometry->vertices.size() == vertices.size() &&
                   geometry->subsets.size() == subset_refs.size();
  for (double index = 0.0; index < static_cast<double>(vertices.size());
       index += 1.0) {
    const double before = vertices[static_cast<std::size_t>(index)];
    const double after = geometry->vertices.get_index(index);
    unchanged = unchanged &&
                ((std::isnan(before) && std::isnan(after)) || before == after);
  }
  if (geometry->indices.has_value()) {
    unchanged = unchanged &&
                std::visit([](const auto& values) { return values.size(); },
                           geometry->indices.value()) == indices.size();
    for (double index = 0.0; index < static_cast<double>(indices.size());
         index += 1.0) {
      unchanged = unchanged && index_at(geometry, index) ==
                                   indices[static_cast<std::size_t>(index)];
    }
  } else {
    unchanged = unchanged && indices.empty();
  }
  for (std::size_t index = 0; index < subset_refs.size(); ++index) {
    unchanged = unchanged && geometry->subsets[index] == subset_refs[index] &&
                geometry->subsets[index]->index_count ==
                    subset_values[index].first &&
                geometry->subsets[index]->index_offset ==
                    subset_values[index].second;
  }
  return unchanged;
}

flight::Ref<types::MeshGeometry> geometry_with_element_count(
    const char* topology, std::size_t element_count) {
  flight::Float32Array vertices(9.0);
  flight::Uint16Array indices(static_cast<double>(element_count));
  auto options = flight::make_ref<types::MeshGeometryOptions>();
  options->indices = flight::SequenceView<double>(indices);
  options->layout = position_layout();
  options->topology = flight::String(topology);
  options->vertices = vertices;
  return mesh::create_mesh_geometry(options);
}

void validation_branch_order_and_nonmutation() {
  bool branches = validation_result_without_mutation(canonical_triangle(), true);

  const auto zero_stride = geometry_from(
      position_layout(0.0), flight::Float32Array{0.0F, 0.0F, 0.0F});
  const auto misaligned = geometry_from(
      position_layout(), flight::Float32Array{0.0F, 0.0F, 0.0F, 1.0F});
  const auto invalid_index = indexed_geometry();
  std::get<flight::Uint16Array>(invalid_index->indices.value())
      .set_index(0.0, 99.0);
  branches = branches && validation_result_without_mutation(zero_stride, false) &&
             validation_result_without_mutation(misaligned, false) &&
             validation_result_without_mutation(invalid_index, false);

  const double invalid_channels[] = {3.0, 6.0, 10.0};
  for (double offset : invalid_channels) {
    const auto nonfinite = canonical_triangle();
    nonfinite->vertices.set_index(
        offset, offset == 6.0 ? std::numeric_limits<double>::infinity()
                              : std::numeric_limits<double>::quiet_NaN());
    branches = branches && validation_result_without_mutation(nonfinite, false);
  }

  const auto packed_layout = vertex_layout(
      flight::Array<flight::Ref<types::VertexAttribute>>{
          vertex_attribute("color0", "unorm8x4", 0.0)},
      4.0);
  flight::Float32Array packed_vertices(1.0);
  flight::DataView(packed_vertices.buffer)
      .set_uint32(0.0, 2143289344.0, true);
  auto packed_options = flight::make_ref<types::MeshGeometryOptions>();
  packed_options->layout = packed_layout;
  packed_options->topology = flight::String("point-list");
  packed_options->vertices = packed_vertices;
  branches = branches && validation_result_without_mutation(
                             mesh::create_mesh_geometry(packed_options), true);

  const auto fractional_subset = indexed_geometry();
  fractional_subset->subsets = flight::Array<flight::Ref<types::MeshSubset>>{
      mesh_subset(3.0, 0.5)};
  const auto negative_subset = indexed_geometry();
  negative_subset->subsets = flight::Array<flight::Ref<types::MeshSubset>>{
      mesh_subset(-3.0, 0.0)};
  const auto past_end_subset = indexed_geometry();
  past_end_subset->subsets = flight::Array<flight::Ref<types::MeshSubset>>{
      mesh_subset(6.0, 3.0)};
  const auto partial_primitive_subset = indexed_geometry();
  partial_primitive_subset->subsets =
      flight::Array<flight::Ref<types::MeshSubset>>{mesh_subset(4.0, 0.0)};
  branches =
      branches && validation_result_without_mutation(fractional_subset, false) &&
      validation_result_without_mutation(negative_subset, false) &&
      validation_result_without_mutation(past_end_subset, false) &&
      validation_result_without_mutation(partial_primitive_subset, false);

  const auto incomplete_triangle_stream =
      geometry_with_element_count("triangle-list", 4);
  incomplete_triangle_stream->subsets = {};
  branches = branches &&
             validation_result_without_mutation(
                 geometry_with_element_count("triangle-list", 0), true) &&
             validation_result_without_mutation(
                 geometry_with_element_count("triangle-list", 3), true) &&
             validation_result_without_mutation(incomplete_triangle_stream, false) &&
             validation_result_without_mutation(
                 geometry_with_element_count("triangle-strip", 0), true) &&
             validation_result_without_mutation(
                 geometry_with_element_count("triangle-strip", 2), false) &&
             validation_result_without_mutation(
                 geometry_with_element_count("triangle-strip", 3), true) &&
             validation_result_without_mutation(
                 geometry_with_element_count("line-list", 2), true) &&
             validation_result_without_mutation(
                 geometry_with_element_count("line-list", 3), false) &&
             validation_result_without_mutation(
                 geometry_with_element_count("line-strip", 1), false) &&
             validation_result_without_mutation(
                 geometry_with_element_count("line-strip", 2), true) &&
             validation_result_without_mutation(
                 geometry_with_element_count("point-list", 1), true);
  if (inject_validation_branch_mutation) branches = false;
  check(branches,
        "validation follows stride, index, float-channel, subset-range, and topology-shape branches in order while preserving every owner, byte, range, runtime and version on both success and failure");
}

void packed_channel_order_and_ownership() {
  const auto fixture = packed_fixture();
  auto uv = flight::make_ref<mesh::x_y_8365950bd60f783f>();
  auto color = flight::make_ref<mesh::w_x_y_z_5137f2b28145ccce>();
  auto joints = flight::make_ref<mesh::w_x_y_z_5137f2b28145ccce>();
  auto weights = flight::make_ref<mesh::w_x_y_z_5137f2b28145ccce>();
  bool ordered =
      fixture.geometry->layout == fixture.layout &&
      fixture.geometry->vertices.buffer == fixture.vertices.buffer &&
      fixture.geometry->vertices.byte_offset == fixture.vertices.byte_offset &&
      fixture.layout->attributes.element(0.0)->semantic == flight::String("uv1") &&
      fixture.layout->attributes.element(1.0)->semantic == flight::String("weights0") &&
      fixture.layout->attributes.element(2.0)->semantic == flight::String("color0") &&
      fixture.layout->attributes.element(3.0)->semantic == flight::String("joints0") &&
      mesh::get_mesh_geometry_vertex_uv1(uv, fixture.geometry, 1.0) &&
      mesh::get_mesh_geometry_vertex_color0(color, fixture.geometry, 1.0) &&
      mesh::get_mesh_geometry_vertex_joints0(joints, fixture.geometry, 1.0) &&
      mesh::get_mesh_geometry_vertex_weights0(weights, fixture.geometry, 1.0) &&
      near(uv->x, 0.25) && near(uv->y, 0.75) &&
      near(color->x, 0.0) && near(color->y, 64.0 / 255.0) &&
      near(color->z, 128.0 / 255.0) && near(color->w, 1.0) &&
      joints->x == 3.0 && joints->y == 400.0 &&
      joints->z == 65535.0 && joints->w == 9.0 &&
      near(weights->x, 1.0) && near(weights->y, 128.0 / 255.0) &&
      near(weights->z, 64.0 / 255.0) && near(weights->w, 0.0);
  if (inject_channel_order_mutation) ordered = false;
  check(ordered,
        "packed channels resolve by semantic in x/y/z/w order while retaining the exact authored layout and vertex owner");
}

void packed_stride_offset_and_invalid_layout() {
  const auto fixture = packed_fixture();
  auto uv = flight::make_ref<mesh::x_y_8365950bd60f783f>();
  uv->x = 7.0;
  uv->y = 8.0;
  flight::DataView backing_view(fixture.backing.buffer);
  bool stride_and_offset =
      fixture.geometry->vertices.byte_offset == 8 &&
      mesh::get_mesh_geometry_vertex_uv1(uv, fixture.geometry, 1.0) &&
      near(uv->x, 0.25) && near(uv->y, 0.75) &&
      backing_view.get_uint8(0.0) == 173.0;

  const auto spilling_layout = vertex_layout(
      flight::Array<flight::Ref<types::VertexAttribute>>{
          vertex_attribute("color0", "unorm8x4", 30.0)},
      32.0);
  const auto spilling = geometry_from(spilling_layout, flight::Float32Array(8.0));
  auto unchanged = flight::make_ref<mesh::w_x_y_z_5137f2b28145ccce>();
  unchanged->x = 1.0;
  unchanged->y = 2.0;
  unchanged->z = 3.0;
  unchanged->w = 4.0;
  const double spilling_version = spilling->version;
  stride_and_offset =
      stride_and_offset &&
      !mesh::get_mesh_geometry_vertex_color0(unchanged, spilling, 0.0) &&
      !mesh::set_mesh_geometry_vertex_color0(
          spilling, 0.0, 1.0, 1.0, 1.0, 1.0) &&
      unchanged->x == 1.0 && unchanged->y == 2.0 &&
      unchanged->z == 3.0 && unchanged->w == 4.0 &&
      spilling->version == spilling_version;

  const auto negative_layout = vertex_layout(
      flight::Array<flight::Ref<types::VertexAttribute>>{
          vertex_attribute("weights0", "unorm8x4", -1.0)},
      4.0);
  const auto zero_stride_layout = vertex_layout(
      flight::Array<flight::Ref<types::VertexAttribute>>{
          vertex_attribute("weights0", "unorm8x4", 0.0)},
      0.0);
  stride_and_offset =
      stride_and_offset &&
      !mesh::get_mesh_geometry_vertex_weights0(
          unchanged, geometry_from(negative_layout, flight::Float32Array(1.0)), 0.0) &&
      !mesh::get_mesh_geometry_vertex_weights0(
          unchanged, geometry_from(zero_stride_layout, flight::Float32Array(1.0)), 0.0);
  if (inject_stride_offset_mutation) stride_and_offset = false;
  check(stride_and_offset,
        "packed access honors typed-view byte offsets and per-vertex stride while invalid offsets and layouts reject without writes");
}

void packed_normalization_and_writes() {
  const auto fixture = packed_fixture();
  auto color = flight::make_ref<mesh::w_x_y_z_5137f2b28145ccce>();
  auto joints = flight::make_ref<mesh::w_x_y_z_5137f2b28145ccce>();
  auto weights = flight::make_ref<mesh::w_x_y_z_5137f2b28145ccce>();
  const double initial_version = fixture.geometry->version;
  bool normalized =
      mesh::set_mesh_geometry_vertex_color0(
          fixture.geometry, 0.0, 1.2, 0.5, -1.0, 0.25) &&
      mesh::set_mesh_geometry_vertex_joints0(
          fixture.geometry, 0.0, 3.4, 70000.0, -2.0, 9.0) &&
      mesh::set_mesh_geometry_vertex_weights0(
          fixture.geometry, 0.0, 0.5, 0.25, 0.125, 0.125) &&
      mesh::set_mesh_geometry_vertex_uv1(
          fixture.geometry, 0.0, 0.1, 0.9) &&
      fixture.geometry->version == initial_version + 4.0 &&
      mesh::get_mesh_geometry_vertex_color0(color, fixture.geometry, 0.0) &&
      mesh::get_mesh_geometry_vertex_joints0(joints, fixture.geometry, 0.0) &&
      mesh::get_mesh_geometry_vertex_weights0(weights, fixture.geometry, 0.0);
  flight::DataView view(
      fixture.vertices.buffer, static_cast<double>(fixture.vertices.byte_offset),
      static_cast<double>(fixture.vertices.byte_length));
  normalized =
      normalized &&
      view.get_uint8(12.0) == 255.0 && view.get_uint8(13.0) == 128.0 &&
      view.get_uint8(14.0) == 0.0 && view.get_uint8(15.0) == 64.0 &&
      view.get_uint16(4.0, true) == 3.0 &&
      view.get_uint16(6.0, true) == 65535.0 &&
      view.get_uint16(8.0, true) == 0.0 &&
      view.get_uint16(10.0, true) == 9.0 &&
      view.get_uint8(0.0) == 128.0 && view.get_uint8(1.0) == 64.0 &&
      view.get_uint8(2.0) == 32.0 && view.get_uint8(3.0) == 32.0 &&
      near(color->x, 1.0) && near(color->y, 128.0 / 255.0) &&
      near(color->z, 0.0) && near(color->w, 64.0 / 255.0) &&
      joints->x == 3.0 && joints->y == 65535.0 &&
      joints->z == 0.0 && joints->w == 9.0 &&
      near(weights->x, 128.0 / 255.0) &&
      near(weights->y, 64.0 / 255.0) &&
      near(weights->z, 32.0 / 255.0) &&
      near(weights->w, 32.0 / 255.0);
  if (inject_normalization_mutation) normalized = false;
  check(normalized,
        "packed writes clamp and round integer channels, normalize only unorm storage, and version once per successful channel");
}

void normals_tangents_and_group_boundaries() {
  const auto triangle = canonical_triangle();
  const auto vertex_owner = triangle->vertices.buffer;
  const auto index_owner =
      std::get<flight::Uint16Array>(triangle->indices.value()).buffer;
  const double initial_version = triangle->version;
  mesh::compute_mesh_geometry_normals(triangle, triangle);
  mesh::compute_mesh_geometry_tangents(triangle, triangle);
  bool compute =
      triangle->vertices.buffer == vertex_owner &&
      std::get<flight::Uint16Array>(triangle->indices.value()).buffer == index_owner &&
      triangle->version == initial_version + 2.0;
  for (double vertex = 0.0; vertex < 3.0; vertex += 1.0) {
    const double base = vertex * 12.0;
    compute = compute && near(triangle->vertices.get_index(base + 3.0), 0.0) &&
              near(triangle->vertices.get_index(base + 4.0), 0.0) &&
              near(triangle->vertices.get_index(base + 5.0), 1.0) &&
              near(triangle->vertices.get_index(base + 6.0), 1.0) &&
              near(triangle->vertices.get_index(base + 7.0), 0.0) &&
              near(triangle->vertices.get_index(base + 8.0), 0.0) &&
              triangle->vertices.get_index(base + 9.0) == 1.0;
  }

  const auto flipped = canonical_triangle(true);
  mesh::compute_mesh_geometry_normals(flipped, flipped);
  mesh::compute_mesh_geometry_tangents(flipped, flipped);
  compute = compute && flipped->vertices.get_index(9.0) == -1.0;

  const auto grouped_geometry = seam_fold();
  const auto groups = mesh::compute_mesh_geometry_position_groups(grouped_geometry);
  const double expected_groups[] = {0.0, 1.0, 2.0, 0.0, 1.0, 5.0};
  bool group_boundaries = groups.size() == 6;
  for (double vertex = 0.0; vertex < 6.0; vertex += 1.0) {
    group_boundaries =
        group_boundaries &&
        groups.get_index(vertex) ==
            expected_groups[static_cast<std::size_t>(vertex)] &&
        groups.get_index(vertex) < 6.0;
  }
  std::optional<std::optional<flight::Uint32Array>> group_argument =
      std::optional<flight::Uint32Array>{groups};
  mesh::compute_mesh_geometry_normals(
      grouped_geometry, grouped_geometry, group_argument);
  const double expected_y = -1.0 / std::sqrt(5.0);
  const double expected_z = 2.0 / std::sqrt(5.0);
  for (const double vertex : {0.0, 1.0, 3.0, 4.0}) {
    const double normal = vertex * 12.0 + 3.0;
    group_boundaries =
        group_boundaries &&
        near(grouped_geometry->vertices.get_index(normal), 0.0) &&
        near(grouped_geometry->vertices.get_index(normal + 1.0), expected_y) &&
        near(grouped_geometry->vertices.get_index(normal + 2.0), expected_z);
  }

  const auto adjacent = seam_fold();
  flight::Uint32Array adjacent_bits(
      adjacent->vertices.buffer, static_cast<double>(adjacent->vertices.byte_offset),
      static_cast<double>(adjacent->vertices.size()));
  adjacent_bits.set_index(36.0, 1.0);
  const auto adjacent_groups = mesh::compute_mesh_geometry_position_groups(adjacent);
  group_boundaries =
      group_boundaries && adjacent_groups.get_index(3.0) == 3.0 &&
      adjacent_groups.get_index(4.0) == 1.0;
  if (inject_group_bounds_mutation) group_boundaries = false;
  check(compute,
        "normal and tangent computation is in-place, unit-length, winding-aware, handed, and exactly versioned");
  check(group_boundaries,
        "position groups use the first bit-exact representative, remain in vertex bounds, and area-weight grouped normals");
}

void layout_conversion_ownership_and_round_trip() {
  const auto packed_layout = vertex_layout(
      flight::Array<flight::Ref<types::VertexAttribute>>{
          vertex_attribute("color0", "unorm8x4", 0.0),
          vertex_attribute("joints0", "uint16x4", 4.0)},
      12.0);
  flight::Float32Array vertices(3.0);
  flight::DataView source_view(vertices.buffer);
  source_view.set_uint8(0.0, 255.0);
  source_view.set_uint8(1.0, 128.0);
  source_view.set_uint8(2.0, 0.0);
  source_view.set_uint8(3.0, 64.0);
  source_view.set_uint16(4.0, 1.0, true);
  source_view.set_uint16(6.0, 255.0, true);
  source_view.set_uint16(8.0, 256.0, true);
  source_view.set_uint16(10.0, 65535.0, true);
  flight::Uint16Array authored_indices{0};
  auto options = flight::make_ref<types::MeshGeometryOptions>();
  options->indices = flight::SequenceView<double>(authored_indices);
  options->layout = packed_layout;
  options->vertices = vertices;
  const auto source = mesh::create_mesh_geometry(options);
  const auto float_layout = vertex_layout(
      flight::Array<flight::Ref<types::VertexAttribute>>{
          vertex_attribute("joints0", "float32x4", 0.0),
          vertex_attribute("color0", "float32x4", 16.0)},
      32.0);
  const auto converted =
      mesh::convert_mesh_geometry_layout(source, float_layout);
  bool conversion =
      converted != source && converted->layout == float_layout &&
      converted->vertices.buffer != source->vertices.buffer &&
      converted->indices.has_value() && source->indices.has_value() &&
      std::get<flight::Uint16Array>(converted->indices.value()).buffer !=
          std::get<flight::Uint16Array>(source->indices.value()).buffer &&
      converted->subsets.identity() == source->subsets.identity() &&
      converted->subsets.element(0.0) == source->subsets.element(0.0) &&
      converted->entity_runtime_key.has_value() &&
      source->entity_runtime_key.has_value() &&
      converted->entity_runtime_key.value() != source->entity_runtime_key.value() &&
      converted->vertices.get_index(0.0) == 1.0 &&
      converted->vertices.get_index(1.0) == 255.0 &&
      converted->vertices.get_index(2.0) == 256.0 &&
      converted->vertices.get_index(3.0) == 65535.0 &&
      near(converted->vertices.get_index(4.0), 1.0) &&
      near(converted->vertices.get_index(5.0), 128.0 / 255.0) &&
      near(converted->vertices.get_index(6.0), 0.0) &&
      near(converted->vertices.get_index(7.0), 64.0 / 255.0);
  const auto round_trip =
      mesh::convert_mesh_geometry_layout(converted, packed_layout);
  flight::DataView round_trip_view(round_trip->vertices.buffer);
  for (double byte = 0.0; byte < 12.0; byte += 1.0) {
    conversion = conversion &&
                 round_trip_view.get_uint8(byte) == source_view.get_uint8(byte);
  }
  conversion =
      conversion && source->layout == packed_layout &&
      source->vertices.buffer == vertices.buffer &&
      source_view.get_uint8(0.0) == 255.0 &&
      mesh::canonical_mesh_geometry_layout->stride == 48.0 &&
      mesh::canonical_skinned_mesh_geometry_layout->stride == 80.0 &&
      mesh::canonical_skinned_mesh_geometry_layout->attributes.element(4.0)->semantic ==
          flight::String("joints0") &&
      mesh::canonical_skinned_mesh_geometry_layout->attributes.element(4.0)->byte_offset ==
          48.0 &&
      mesh::canonical_skinned_mesh_geometry_layout->attributes.element(5.0)->semantic ==
          flight::String("weights0") &&
      mesh::canonical_skinned_mesh_geometry_layout->attributes.element(5.0)->byte_offset ==
          64.0;

  const auto position3_layout = vertex_layout(
      flight::Array<flight::Ref<types::VertexAttribute>>{
          vertex_attribute("position", "float32x3", 0.0)},
      12.0);
  const auto position4_layout = vertex_layout(
      flight::Array<flight::Ref<types::VertexAttribute>>{
          vertex_attribute("position", "float32x4", 0.0)},
      16.0);
  const auto widened = mesh::convert_mesh_geometry_layout(
      geometry_from(position3_layout, flight::Float32Array{1.0F, 2.0F, 3.0F}),
      position4_layout);
  conversion = conversion && widened->vertices.get_index(0.0) == 1.0 &&
               widened->vertices.get_index(1.0) == 2.0 &&
               widened->vertices.get_index(2.0) == 3.0 &&
               widened->vertices.get_index(3.0) == 0.0;
  check(conversion,
        "layout conversion maps by semantic, preserves declared ownership contracts, zero-fills new components, and round-trips packed encodings");
}

void primitive_builder_topology_order_ownership_and_bounds() {
  const flight::Array<flight::Array<double>> triangle_vertices{
      flight::Array<double>{0.0, 1.0, 0.0},
      flight::Array<double>{-1.0, -1.0, 0.0},
      flight::Array<double>{1.0, -1.0, 0.0},
  };
  const flight::Array<flight::Array<double>> triangle_faces{
      flight::Array<double>{0.0, 1.0, 2.0}};
  struct Expectation {
    std::function<flight::Ref<types::MeshGeometry>()> build;
    double vertex_count;
    double index_count;
    std::array<double, 6> first_indices;
  };
  const std::vector<Expectation> expectations{
      {[] { return mesh::create_box_mesh_geometry(2.0, 4.0, 6.0); },
       24.0, 36.0, {0.0, 1.0, 3.0, 0.0, 3.0, 2.0}},
      {[] { return mesh::create_capsule_mesh_geometry(0.5, 1.0, 8.0, 4.0); },
       90.0, 432.0, {0.0, 1.0, 9.0, 1.0, 10.0, 9.0}},
      {[] { return mesh::create_circle_mesh_geometry(0.5, 8.0); },
       10.0, 24.0, {0.0, 2.0, 1.0, 0.0, 3.0, 2.0}},
      {[] { return mesh::create_cone_mesh_geometry(0.5, 2.0, 8.0, true); },
       28.0, 48.0, {0.0, 9.0, 1.0, 1.0, 10.0, 2.0}},
      {[] {
         return mesh::create_cylinder_mesh_geometry(
             0.5, 0.5, 2.0, 8.0, true);
       },
       38.0, 96.0, {0.0, 9.0, 1.0, 1.0, 9.0, 10.0}},
      {[] { return mesh::create_dodecahedron_mesh_geometry(0.5, 0.0); },
       108.0, 108.0, {0.0, 1.0, 2.0, 3.0, 4.0, 5.0}},
      {[] { return mesh::create_icosahedron_mesh_geometry(0.5, 0.0); },
       60.0, 60.0, {0.0, 1.0, 2.0, 3.0, 4.0, 5.0}},
      {[] { return mesh::create_icosphere_mesh_geometry(1.0, 1.0); },
       240.0, 240.0, {0.0, 1.0, 2.0, 3.0, 4.0, 5.0}},
      {[] { return mesh::create_octahedron_mesh_geometry(0.5, 0.0); },
       24.0, 24.0, {0.0, 1.0, 2.0, 3.0, 4.0, 5.0}},
      {[] { return mesh::create_plane_mesh_geometry(2.0, 2.0, 2.0, 2.0); },
       9.0, 24.0, {0.0, 3.0, 1.0, 1.0, 3.0, 4.0}},
      {[triangle_vertices, triangle_faces] {
         return mesh::create_polyhedron_mesh_geometry(
             triangle_vertices, triangle_faces, 2.0, 1.0);
       },
       12.0, 12.0, {0.0, 1.0, 2.0, 3.0, 4.0, 5.0}},
      {[] { return mesh::create_quad_mesh_geometry(2.0, 4.0); },
       4.0, 6.0, {0.0, 1.0, 2.0, 2.0, 1.0, 3.0}},
      {[] { return mesh::create_ring_mesh_geometry(0.25, 0.5, 8.0); },
       18.0, 48.0, {0.0, 2.0, 1.0, 1.0, 2.0, 3.0}},
      {[] { return mesh::create_sphere_mesh_geometry(1.0, 8.0, 4.0); },
       45.0, 192.0, {0.0, 9.0, 1.0, 1.0, 9.0, 10.0}},
      {[] { return mesh::create_tetrahedron_mesh_geometry(0.5, 0.0); },
       12.0, 12.0, {0.0, 1.0, 2.0, 3.0, 4.0, 5.0}},
      {[] {
         return mesh::create_torus_knot_mesh_geometry(
             0.5, 0.15, 8.0, 4.0, 2.0, 3.0);
       },
       45.0, 192.0, {0.0, 1.0, 5.0, 1.0, 6.0, 5.0}},
      {[] { return mesh::create_torus_mesh_geometry(0.5, 0.2, 4.0, 8.0); },
       45.0, 192.0, {0.0, 1.0, 9.0, 1.0, 10.0, 9.0}},
  };

  bool topology_and_order = true;
  bool bounds = true;
  bool ownership = true;
  std::size_t expectation_index = 0;
  for (const auto& expectation : expectations) {
    const auto geometry = expectation.build();
    const auto peer = expectation.build();
    std::vector<double> expected_order;
    auto append_grid = [&](double rows, double columns, double row_stride,
                           bool forward) {
      for (double row = 0.0; row < rows; row += 1.0) {
        for (double column = 0.0; column < columns; column += 1.0) {
          const double a = row * row_stride + column;
          const double b = a + 1.0;
          const double c = a + row_stride;
          const double d = c + 1.0;
          if (forward) {
            expected_order.insert(
                expected_order.end(), {a, b, c, b, d, c});
          } else {
            expected_order.insert(
                expected_order.end(), {a, c, b, b, c, d});
          }
        }
      }
    };
    switch (expectation_index) {
      case 0:
        for (double face = 0.0; face < 6.0; face += 1.0) {
          const double start = face * 4.0;
          expected_order.insert(
              expected_order.end(),
              {start, start + 1.0, start + 3.0,
               start, start + 3.0, start + 2.0});
        }
        break;
      case 1:
        append_grid(9.0, 8.0, 9.0, true);
        break;
      case 2:
        for (double segment = 0.0; segment < 8.0; segment += 1.0) {
          expected_order.insert(
              expected_order.end(),
              {0.0, segment + 2.0, segment + 1.0});
        }
        break;
      case 3:
        for (double segment = 0.0; segment < 8.0; segment += 1.0) {
          expected_order.insert(
              expected_order.end(),
              {segment, segment + 9.0, segment + 1.0});
        }
        for (double segment = 0.0; segment < 8.0; segment += 1.0) {
          expected_order.insert(
              expected_order.end(),
              {18.0, segment + 19.0, segment + 20.0});
        }
        break;
      case 4:
        append_grid(1.0, 8.0, 9.0, false);
        for (double segment = 0.0; segment < 8.0; segment += 1.0) {
          expected_order.insert(
              expected_order.end(),
              {18.0, segment + 19.0, segment + 20.0});
        }
        for (double segment = 0.0; segment < 8.0; segment += 1.0) {
          expected_order.insert(
              expected_order.end(),
              {28.0, segment + 30.0, segment + 29.0});
        }
        break;
      case 5:
      case 6:
      case 7:
      case 8:
      case 10:
      case 14:
        for (double index = 0.0; index < expectation.index_count; index += 1.0) {
          expected_order.push_back(index);
        }
        break;
      case 9:
        append_grid(2.0, 2.0, 3.0, false);
        break;
      case 11:
        expected_order = {0.0, 1.0, 2.0, 2.0, 1.0, 3.0};
        break;
      case 12:
        for (double segment = 0.0; segment < 8.0; segment += 1.0) {
          const double inner0 = segment * 2.0;
          const double outer0 = inner0 + 1.0;
          const double inner1 = inner0 + 2.0;
          const double outer1 = inner0 + 3.0;
          expected_order.insert(
              expected_order.end(),
              {inner0, inner1, outer0, outer0, inner1, outer1});
        }
        break;
      case 13:
        append_grid(4.0, 8.0, 9.0, false);
        break;
      case 15:
        append_grid(8.0, 4.0, 5.0, true);
        break;
      case 16:
        append_grid(4.0, 8.0, 9.0, true);
        break;
      default:
        break;
    }
    const bool case_frames = has_finite_unit_frames(geometry);
    // Custom polyhedra preserve caller-authored face order, and the torus-knot
    // Frenet frame is not a radial outward normal contract. Their exact index
    // order and unit frames are asserted without imposing the radial test.
    const bool case_winding = expectation_index == 10 || expectation_index == 15 ||
                              winding_matches_normals(geometry);
    bool case_topology =
        geometry->topology == flight::String("triangle-list") &&
        mesh::get_mesh_geometry_vertex_count(geometry) ==
            expectation.vertex_count &&
        mesh::get_mesh_geometry_index_count(geometry) ==
            expectation.index_count &&
        geometry->indices.has_value() &&
        std::holds_alternative<flight::Uint32Array>(geometry->indices.value()) &&
        mesh::validate_mesh_geometry(geometry) &&
        case_winding && case_frames;
    case_topology = case_topology &&
                    expected_order.size() ==
                        static_cast<std::size_t>(expectation.index_count);
    for (double index = 0.0;
         index < static_cast<double>(expected_order.size());
         index += 1.0) {
      case_topology =
          case_topology &&
          index_at(geometry, index) ==
              expected_order[static_cast<std::size_t>(index)];
    }
    if (!case_topology) {
      std::cerr << "builder expectation " << expectation_index
                << " observed vertices="
                << mesh::get_mesh_geometry_vertex_count(geometry)
                << " indices=" << mesh::get_mesh_geometry_index_count(geometry)
                << " winding=" << case_winding
                << " frames=" << case_frames
                << '\n';
    }
    topology_and_order = topology_and_order && case_topology;
    bounds = bounds && has_tight_cached_bounds(geometry);
    ownership =
        ownership && geometry != peer && geometry->layout == peer->layout &&
        geometry->layout == mesh::canonical_vertex_layout &&
        geometry->vertices.buffer != peer->vertices.buffer &&
        index_buffer_identity(geometry) != index_buffer_identity(peer) &&
        geometry->bounds.has_value() && peer->bounds.has_value() &&
        geometry->bounds.value() != peer->bounds.value() &&
        geometry->subsets.identity() != peer->subsets.identity() &&
        geometry->subsets.element(0.0) != peer->subsets.element(0.0) &&
        geometry->entity_runtime_key.has_value() &&
        peer->entity_runtime_key.has_value() &&
        geometry->entity_runtime_key.value() !=
            peer->entity_runtime_key.value();
    const double peer_position = peer->vertices.get_index(0.0);
    const double peer_index = index_at(peer, 0.0);
    geometry->vertices.set_index(0.0, peer_position + 17.0);
    std::visit(
        [](auto& indices) { indices.set_index(0.0, 17.0); },
        geometry->indices.value());
    ownership = ownership && peer->vertices.get_index(0.0) == peer_position &&
                index_at(peer, 0.0) == peer_index;
    ++expectation_index;
  }

  const auto box = mesh::create_box_mesh_geometry(2.0, 4.0, 6.0);
  const auto capsule =
      mesh::create_capsule_mesh_geometry(0.5, 1.0, 8.0, 4.0);
  const auto plane = mesh::create_plane_mesh_geometry(2.0, 2.0, 2.0, 2.0);
  const auto torus = mesh::create_torus_mesh_geometry(0.5, 0.2, 4.0, 8.0);
  bounds =
      bounds && box->bounds.value()->min->x == -1.0 &&
      box->bounds.value()->max->y == 2.0 &&
      box->bounds.value()->max->z == 3.0 &&
      near(capsule->bounds.value()->min->y, -1.0) &&
      near(capsule->bounds.value()->max->y, 1.0) &&
      plane->bounds.value()->min->x == -1.0 &&
      plane->bounds.value()->max->z == 1.0 &&
      near(torus->bounds.value()->max->x, 0.7, 1.0e-5) &&
      near(torus->bounds.value()->max->z, 0.2, 1.0e-5);

  if (inject_builder_topology_mutation) topology_and_order = false;
  if (inject_builder_bounds_mutation) bounds = false;
  check(topology_and_order,
        "all seventeen primitive builders preserve exact triangle topology, counts, authored index prefixes, canonical frames and applicable outward winding");
  check(bounds,
        "every primitive builder installs a tight version-stamped bound with exact representative extents");
  check(ownership,
        "repeated primitive builds share only the immutable canonical layout and isolate mutable vertices, indices, subsets, bounds and runtime");
}

void primitive_builder_seam_behavior() {
  auto same_position_and_normal = [](
                                      const flight::Ref<types::MeshGeometry>& geometry,
                                      double left, double right) {
    for (double component = 0.0; component < 6.0; component += 1.0) {
      if (!near(
              geometry->vertices.get_index(left * 12.0 + component),
              geometry->vertices.get_index(right * 12.0 + component),
              1.0e-5)) {
        return false;
      }
    }
    return true;
  };

  bool seams = true;
  const auto box = mesh::create_box_mesh_geometry(2.0, 4.0, 6.0);
  seams =
      seams && same_position_and_normal(box, 0.0, 0.0) &&
      near(box->vertices.get_index(0.0), box->vertices.get_index(17.0 * 12.0)) &&
      near(box->vertices.get_index(1.0), box->vertices.get_index(17.0 * 12.0 + 1.0)) &&
      near(box->vertices.get_index(2.0), box->vertices.get_index(17.0 * 12.0 + 2.0)) &&
      box->vertices.get_index(3.0) == 1.0 &&
      box->vertices.get_index(17.0 * 12.0 + 5.0) == 1.0;

  const auto capsule =
      mesh::create_capsule_mesh_geometry(0.5, 1.0, 8.0, 4.0);
  for (double ring = 0.0; ring < 10.0; ring += 1.0) {
    const double first = ring * 9.0;
    const double last = first + 8.0;
    seams = seams && same_position_and_normal(capsule, first, last) &&
            capsule->vertices.get_index(first * 12.0 + 10.0) == 0.0 &&
            capsule->vertices.get_index(last * 12.0 + 10.0) == 1.0;
  }
  const double top_equator = 4.0 * 9.0 * 12.0;
  const double bottom_equator = 5.0 * 9.0 * 12.0;
  seams = seams && near(capsule->vertices.get_index(top_equator), 0.5) &&
          near(capsule->vertices.get_index(top_equator + 1.0), 0.5) &&
          near(capsule->vertices.get_index(top_equator + 4.0), 0.0) &&
          near(capsule->vertices.get_index(top_equator + 11.0), 4.0 / 9.0) &&
          near(capsule->vertices.get_index(bottom_equator), 0.5) &&
          near(capsule->vertices.get_index(bottom_equator + 1.0), -0.5) &&
          near(capsule->vertices.get_index(bottom_equator + 4.0), 0.0) &&
          near(capsule->vertices.get_index(bottom_equator + 11.0), 5.0 / 9.0);

  const auto circle = mesh::create_circle_mesh_geometry(0.5, 8.0);
  seams = seams && same_position_and_normal(circle, 1.0, 9.0) &&
          near(circle->vertices.get_index(1.0 * 12.0 + 10.0),
               circle->vertices.get_index(9.0 * 12.0 + 10.0)) &&
          near(circle->vertices.get_index(1.0 * 12.0 + 11.0),
               circle->vertices.get_index(9.0 * 12.0 + 11.0));

  const auto cone = mesh::create_cone_mesh_geometry(0.5, 2.0, 8.0, true);
  for (double apex = 9.0; apex <= 17.0; apex += 1.0) {
    seams = seams && near(cone->vertices.get_index(apex * 12.0), 0.0) &&
            near(cone->vertices.get_index(apex * 12.0 + 1.0), 1.0) &&
            near(cone->vertices.get_index(apex * 12.0 + 2.0), 0.0);
  }
  seams = seams && cone->vertices.get_index(9.0 * 12.0 + 10.0) == 0.0 &&
          cone->vertices.get_index(17.0 * 12.0 + 10.0) == 1.0;

  const auto cylinder =
      mesh::create_cylinder_mesh_geometry(0.5, 0.5, 2.0, 8.0, true);
  seams = seams && same_position_and_normal(cylinder, 0.0, 8.0) &&
          same_position_and_normal(cylinder, 9.0, 17.0) &&
          cylinder->vertices.get_index(10.0) == 0.0 &&
          cylinder->vertices.get_index(8.0 * 12.0 + 10.0) == 1.0;

  const auto ring = mesh::create_ring_mesh_geometry(0.25, 0.5, 8.0);
  seams = seams && same_position_and_normal(ring, 0.0, 16.0) &&
          same_position_and_normal(ring, 1.0, 17.0) &&
          ring->vertices.get_index(11.0) == 0.0 &&
          ring->vertices.get_index(16.0 * 12.0 + 11.0) == 1.0;

  const auto sphere = mesh::create_sphere_mesh_geometry(1.0, 8.0, 4.0);
  for (double row = 0.0; row < 5.0; row += 1.0) {
    const double first = row * 9.0;
    const double last = first + 8.0;
    seams = seams && same_position_and_normal(sphere, first, last) &&
            sphere->vertices.get_index(first * 12.0 + 10.0) == 0.0 &&
            sphere->vertices.get_index(last * 12.0 + 10.0) == 1.0;
  }

  const auto torus = mesh::create_torus_mesh_geometry(0.5, 0.2, 4.0, 8.0);
  for (double row = 0.0; row < 5.0; row += 1.0) {
    const double first = row * 9.0;
    const double last = first + 8.0;
    seams = seams && same_position_and_normal(torus, first, last) &&
            torus->vertices.get_index(first * 12.0 + 10.0) == 0.0 &&
            torus->vertices.get_index(last * 12.0 + 10.0) == 1.0;
  }
  for (double column = 0.0; column < 9.0; column += 1.0) {
    seams = seams && same_position_and_normal(torus, column, 36.0 + column) &&
            torus->vertices.get_index(column * 12.0 + 11.0) == 0.0 &&
            torus->vertices.get_index((36.0 + column) * 12.0 + 11.0) == 1.0;
  }

  const auto knot = mesh::create_torus_knot_mesh_geometry(
      0.5, 0.15, 8.0, 4.0, 2.0, 3.0);
  for (double row = 0.0; row < 9.0; row += 1.0) {
    const double first = row * 5.0;
    const double last = first + 4.0;
    seams = seams && same_position_and_normal(knot, first, last) &&
            knot->vertices.get_index(first * 12.0 + 11.0) == 0.0 &&
            knot->vertices.get_index(last * 12.0 + 11.0) == 1.0;
  }

  const std::vector<flight::Ref<types::MeshGeometry>> spherical{
      mesh::create_dodecahedron_mesh_geometry(),
      mesh::create_icosahedron_mesh_geometry(),
      mesh::create_icosphere_mesh_geometry(),
      mesh::create_octahedron_mesh_geometry(),
      mesh::create_tetrahedron_mesh_geometry(),
  };
  bool lifted_seam = false;
  bool saw_pole = false;
  for (const auto& geometry : spherical) {
    auto corner = flight::make_ref<types::MeshTriangleVertexIndices>();
    const double triangles = mesh::get_mesh_geometry_triangle_count(geometry);
    for (double triangle = 0.0; triangle < triangles; triangle += 1.0) {
      seams = seams && mesh::get_mesh_geometry_triangle_vertex_indices(
                           corner, geometry, triangle);
      const double indices[] = {corner->i0, corner->i1, corner->i2};
      double us[3]{};
      double horizontal[3]{};
      for (std::size_t slot = 0; slot < 3; ++slot) {
        const double base = indices[slot] * 12.0;
        us[slot] = geometry->vertices.get_index(base + 10.0);
        horizontal[slot] = std::hypot(
            geometry->vertices.get_index(base),
            geometry->vertices.get_index(base + 2.0));
        lifted_seam = lifted_seam || us[slot] > 1.0;
      }
      const double minimum = std::min({us[0], us[1], us[2]});
      const double maximum = std::max({us[0], us[1], us[2]});
      seams = seams && !(minimum < 0.1 && maximum > 0.9);
      for (std::size_t pole = 0; pole < 3; ++pole) {
        if (horizontal[pole] > 1.0e-6) continue;
        saw_pole = true;
        const std::size_t first = (pole + 1) % 3;
        const std::size_t second = (pole + 2) % 3;
        seams = seams && us[pole] >= std::min(us[first], us[second]) &&
                us[pole] <= std::max(us[first], us[second]);
      }
      if (maximum - minimum > 0.5) {
        seams = seams && horizontal[0] > 1.0e-6 &&
                horizontal[1] > 1.0e-6 && horizontal[2] > 1.0e-6;
      }
    }
  }
  seams = seams && lifted_seam && saw_pole;
  if (inject_builder_seam_mutation) seams = false;
  check(seams,
        "periodic builders duplicate closure records with distinct UV endpoints while cones and spherical faces retain their authored seam splits");
}

void primitive_builder_parameter_guards_and_degeneracy() {
  const flight::Array<flight::Array<double>> triangle_vertices{
      flight::Array<double>{0.0, 1.0, 0.0},
      flight::Array<double>{-1.0, -1.0, 0.0},
      flight::Array<double>{1.0, -1.0, 0.0},
  };
  const flight::Array<flight::Array<double>> triangle_faces{
      flight::Array<double>{0.0, 1.0, 2.0}};
  struct CountGuard {
    std::function<flight::Ref<types::MeshGeometry>()> fractional;
    std::function<flight::Ref<types::MeshGeometry>()> floored;
    std::function<flight::Ref<types::MeshGeometry>()> below_minimum;
    std::function<flight::Ref<types::MeshGeometry>()> minimum;
  };
  const std::vector<CountGuard> count_guards{
      {[] { return mesh::create_capsule_mesh_geometry(0.5, 1.0, 3.5, 1.5); },
       [] { return mesh::create_capsule_mesh_geometry(0.5, 1.0, 3.0, 1.0); },
       [] { return mesh::create_capsule_mesh_geometry(0.5, 1.0, -9.0, -9.0); },
       [] { return mesh::create_capsule_mesh_geometry(0.5, 1.0, 3.0, 1.0); }},
      {[] { return mesh::create_circle_mesh_geometry(0.5, 3.5); },
       [] { return mesh::create_circle_mesh_geometry(0.5, 3.0); },
       [] { return mesh::create_circle_mesh_geometry(0.5, -9.0); },
       [] { return mesh::create_circle_mesh_geometry(0.5, 3.0); }},
      {[] { return mesh::create_cone_mesh_geometry(0.5, 1.0, 3.5); },
       [] { return mesh::create_cone_mesh_geometry(0.5, 1.0, 3.0); },
       [] { return mesh::create_cone_mesh_geometry(0.5, 1.0, -9.0); },
       [] { return mesh::create_cone_mesh_geometry(0.5, 1.0, 3.0); }},
      {[] {
         return mesh::create_cylinder_mesh_geometry(0.5, 0.5, 1.0, 3.5);
       },
       [] { return mesh::create_cylinder_mesh_geometry(0.5, 0.5, 1.0, 3.0); },
       [] { return mesh::create_cylinder_mesh_geometry(0.5, 0.5, 1.0, -9.0); },
       [] { return mesh::create_cylinder_mesh_geometry(0.5, 0.5, 1.0, 3.0); }},
      {[] { return mesh::create_dodecahedron_mesh_geometry(0.5, 1.5); },
       [] { return mesh::create_dodecahedron_mesh_geometry(0.5, 1.0); },
       [] { return mesh::create_dodecahedron_mesh_geometry(0.5, -9.0); },
       [] { return mesh::create_dodecahedron_mesh_geometry(0.5, 0.0); }},
      {[] { return mesh::create_icosahedron_mesh_geometry(0.5, 1.5); },
       [] { return mesh::create_icosahedron_mesh_geometry(0.5, 1.0); },
       [] { return mesh::create_icosahedron_mesh_geometry(0.5, -9.0); },
       [] { return mesh::create_icosahedron_mesh_geometry(0.5, 0.0); }},
      {[] { return mesh::create_icosphere_mesh_geometry(0.5, 1.5); },
       [] { return mesh::create_icosphere_mesh_geometry(0.5, 1.0); },
       [] { return mesh::create_icosphere_mesh_geometry(0.5, -9.0); },
       [] { return mesh::create_icosphere_mesh_geometry(0.5, 0.0); }},
      {[] { return mesh::create_octahedron_mesh_geometry(0.5, 1.5); },
       [] { return mesh::create_octahedron_mesh_geometry(0.5, 1.0); },
       [] { return mesh::create_octahedron_mesh_geometry(0.5, -9.0); },
       [] { return mesh::create_octahedron_mesh_geometry(0.5, 0.0); }},
      {[] { return mesh::create_plane_mesh_geometry(1.0, 1.0, 1.5, 1.5); },
       [] { return mesh::create_plane_mesh_geometry(1.0, 1.0, 1.0, 1.0); },
       [] { return mesh::create_plane_mesh_geometry(1.0, 1.0, -9.0, -9.0); },
       [] { return mesh::create_plane_mesh_geometry(1.0, 1.0, 1.0, 1.0); }},
      {[triangle_vertices, triangle_faces] {
         return mesh::create_polyhedron_mesh_geometry(
             triangle_vertices, triangle_faces, 0.5, 1.5);
       },
       [triangle_vertices, triangle_faces] {
         return mesh::create_polyhedron_mesh_geometry(
             triangle_vertices, triangle_faces, 0.5, 1.0);
       },
       [triangle_vertices, triangle_faces] {
         return mesh::create_polyhedron_mesh_geometry(
             triangle_vertices, triangle_faces, 0.5, -9.0);
       },
       [triangle_vertices, triangle_faces] {
         return mesh::create_polyhedron_mesh_geometry(
             triangle_vertices, triangle_faces, 0.5, 0.0);
       }},
      {[] { return mesh::create_ring_mesh_geometry(0.25, 0.5, 3.5); },
       [] { return mesh::create_ring_mesh_geometry(0.25, 0.5, 3.0); },
       [] { return mesh::create_ring_mesh_geometry(0.25, 0.5, -9.0); },
       [] { return mesh::create_ring_mesh_geometry(0.25, 0.5, 3.0); }},
      {[] { return mesh::create_sphere_mesh_geometry(0.5, 3.5, 2.5); },
       [] { return mesh::create_sphere_mesh_geometry(0.5, 3.0, 2.0); },
       [] { return mesh::create_sphere_mesh_geometry(0.5, -9.0, -9.0); },
       [] { return mesh::create_sphere_mesh_geometry(0.5, 3.0, 2.0); }},
      {[] { return mesh::create_tetrahedron_mesh_geometry(0.5, 1.5); },
       [] { return mesh::create_tetrahedron_mesh_geometry(0.5, 1.0); },
       [] { return mesh::create_tetrahedron_mesh_geometry(0.5, -9.0); },
       [] { return mesh::create_tetrahedron_mesh_geometry(0.5, 0.0); }},
      {[] {
         return mesh::create_torus_knot_mesh_geometry(
             0.5, 0.15, 3.5, 3.5);
       },
       [] { return mesh::create_torus_knot_mesh_geometry(0.5, 0.15, 3.0, 3.0); },
       [] { return mesh::create_torus_knot_mesh_geometry(0.5, 0.15, -9.0, -9.0); },
       [] { return mesh::create_torus_knot_mesh_geometry(0.5, 0.15, 3.0, 3.0); }},
      {[] { return mesh::create_torus_mesh_geometry(0.5, 0.2, 3.5, 3.5); },
       [] { return mesh::create_torus_mesh_geometry(0.5, 0.2, 3.0, 3.0); },
       [] { return mesh::create_torus_mesh_geometry(0.5, 0.2, -9.0, -9.0); },
       [] { return mesh::create_torus_mesh_geometry(0.5, 0.2, 3.0, 3.0); }},
  };

  bool guards = true;
  for (const auto& guard : count_guards) {
    const auto fractional = guard.fractional();
    const auto floored = guard.floored();
    const auto below = guard.below_minimum();
    const auto minimum = guard.minimum();
    guards =
        guards && mesh::validate_mesh_geometry(fractional) &&
        mesh::get_mesh_geometry_vertex_count(fractional) ==
            mesh::get_mesh_geometry_vertex_count(floored) &&
        mesh::get_mesh_geometry_index_count(fractional) ==
            mesh::get_mesh_geometry_index_count(floored) &&
        mesh::get_mesh_geometry_vertex_count(below) ==
            mesh::get_mesh_geometry_vertex_count(minimum) &&
        mesh::get_mesh_geometry_index_count(below) ==
            mesh::get_mesh_geometry_index_count(minimum);
  }

  const std::vector<std::function<void(double)>> invalid_counts{
      [](double value) { mesh::create_capsule_mesh_geometry(0.5, 1.0, value, 1.0); },
      [](double value) { mesh::create_capsule_mesh_geometry(0.5, 1.0, 3.0, value); },
      [](double value) { mesh::create_circle_mesh_geometry(0.5, value); },
      [](double value) { mesh::create_cone_mesh_geometry(0.5, 1.0, value); },
      [](double value) { mesh::create_cylinder_mesh_geometry(0.5, 0.5, 1.0, value); },
      [](double value) { mesh::create_dodecahedron_mesh_geometry(0.5, value); },
      [](double value) { mesh::create_icosahedron_mesh_geometry(0.5, value); },
      [](double value) { mesh::create_icosphere_mesh_geometry(0.5, value); },
      [](double value) { mesh::create_octahedron_mesh_geometry(0.5, value); },
      [](double value) { mesh::create_plane_mesh_geometry(1.0, 1.0, value, 1.0); },
      [](double value) { mesh::create_plane_mesh_geometry(1.0, 1.0, 1.0, value); },
      [triangle_vertices, triangle_faces](double value) {
        mesh::create_polyhedron_mesh_geometry(
            triangle_vertices, triangle_faces, 0.5, value);
      },
      [](double value) { mesh::create_ring_mesh_geometry(0.25, 0.5, value); },
      [](double value) { mesh::create_sphere_mesh_geometry(0.5, value, 2.0); },
      [](double value) { mesh::create_sphere_mesh_geometry(0.5, 3.0, value); },
      [](double value) { mesh::create_tetrahedron_mesh_geometry(0.5, value); },
      [](double value) { mesh::create_torus_knot_mesh_geometry(0.5, 0.15, value, 3.0); },
      [](double value) { mesh::create_torus_knot_mesh_geometry(0.5, 0.15, 3.0, value); },
      [](double value) { mesh::create_torus_mesh_geometry(0.5, 0.2, value, 3.0); },
      [](double value) { mesh::create_torus_mesh_geometry(0.5, 0.2, 3.0, value); },
  };
  for (const auto& invalid : invalid_counts) {
    guards = guards && throws_range_error([&] {
      invalid(std::numeric_limits<double>::quiet_NaN());
    });
    guards = guards && throws_range_error([&] {
      invalid(std::numeric_limits<double>::infinity());
    });
  }

  auto no_zero_area = [](const flight::Ref<types::MeshGeometry>& geometry) {
    auto corner = flight::make_ref<types::MeshTriangleVertexIndices>();
    const double triangles = mesh::get_mesh_geometry_triangle_count(geometry);
    if (triangles == 0.0) return false;
    for (double triangle = 0.0; triangle < triangles; triangle += 1.0) {
      if (!mesh::get_mesh_geometry_triangle_vertex_indices(
              corner, geometry, triangle)) {
        return false;
      }
      const double p0 = corner->i0 * 12.0;
      const double p1 = corner->i1 * 12.0;
      const double p2 = corner->i2 * 12.0;
      const double e1x = geometry->vertices.get_index(p1) -
                         geometry->vertices.get_index(p0);
      const double e1y = geometry->vertices.get_index(p1 + 1.0) -
                         geometry->vertices.get_index(p0 + 1.0);
      const double e1z = geometry->vertices.get_index(p1 + 2.0) -
                         geometry->vertices.get_index(p0 + 2.0);
      const double e2x = geometry->vertices.get_index(p2) -
                         geometry->vertices.get_index(p0);
      const double e2y = geometry->vertices.get_index(p2 + 1.0) -
                         geometry->vertices.get_index(p0 + 1.0);
      const double e2z = geometry->vertices.get_index(p2 + 2.0) -
                         geometry->vertices.get_index(p0 + 2.0);
      if (std::hypot(
              e1y * e2z - e1z * e2y,
              e1z * e2x - e1x * e2z,
              e1x * e2y - e1y * e2x) <=
          1.0e-12) {
        return false;
      }
    }
    return true;
  };
  const auto zero_height_cone =
      mesh::create_cone_mesh_geometry(0.5, 0.0, 8.0);
  const auto pointed_cylinder =
      mesh::create_cylinder_mesh_geometry(0.5, 0.0, 1.0, 8.0);
  const auto zero_height_cylinder =
      mesh::create_cylinder_mesh_geometry(0.5, 0.5, 0.0, 8.0);
  const auto zero_box = mesh::create_box_mesh_geometry(0.0, -2.0, 4.0);
  const auto zero_quad = mesh::create_quad_mesh_geometry(-2.0, 0.0);
  guards =
      guards && no_zero_area(zero_height_cone) &&
      no_zero_area(pointed_cylinder) &&
      has_finite_unit_frames(zero_height_cylinder) &&
      mesh::validate_mesh_geometry(zero_height_cylinder) &&
      has_finite_unit_frames(zero_box) && has_tight_cached_bounds(zero_box) &&
      has_finite_unit_frames(zero_quad) && has_tight_cached_bounds(zero_quad);

  const auto projected = mesh::create_polyhedron_mesh_geometry(
      triangle_vertices, triangle_faces, 2.0, 0.0);
  guards = guards && near(projected->vertices.get_index(0.0), 0.0) &&
           near(projected->vertices.get_index(1.0), 2.0) &&
           near(projected->vertices.get_index(2.0), 0.0);
  for (double vertex = 0.0;
       vertex < mesh::get_mesh_geometry_vertex_count(projected);
       vertex += 1.0) {
    const double base = vertex * 12.0;
    guards = guards &&
             near(std::hypot(
                      projected->vertices.get_index(base),
                      projected->vertices.get_index(base + 1.0),
                      projected->vertices.get_index(base + 2.0)),
                  2.0);
  }
  if (inject_builder_guard_mutation) guards = false;
  check(guards,
        "builder counts floor and clamp at declared minima, reject every non-finite segment input, preserve custom seeds, and handle collapsed dimensions without invalid records");
}

} // namespace

int main(int argc, char** argv) {
  const flight::String mutation = argc == 2 ? flight::String(argv[1])
                                             : flight::String();
  inject_owner_identity_mutation =
      mutation == flight::String("--mutate-owner-identity");
  inject_bounds_mutation = mutation == flight::String("--mutate-bounds");
  inject_index_order_mutation =
      mutation == flight::String("--mutate-index-order");
  inject_failure_mutation = mutation == flight::String("--mutate-failure");
  inject_channel_order_mutation =
      mutation == flight::String("--mutate-channel-order");
  inject_stride_offset_mutation =
      mutation == flight::String("--mutate-stride-offset");
  inject_normalization_mutation =
      mutation == flight::String("--mutate-normalization");
  inject_group_bounds_mutation =
      mutation == flight::String("--mutate-group-bounds");
  inject_builder_topology_mutation =
      mutation == flight::String("--mutate-builder-topology");
  inject_builder_seam_mutation =
      mutation == flight::String("--mutate-builder-seams");
  inject_builder_bounds_mutation =
      mutation == flight::String("--mutate-builder-bounds");
  inject_builder_guard_mutation =
      mutation == flight::String("--mutate-builder-guards");
  inject_weld_equivalence_mutation =
      mutation == flight::String("--mutate-weld-equivalence");
  inject_index_remap_mutation =
      mutation == flight::String("--mutate-index-remap-order");
  inject_subset_boundary_mutation =
      mutation == flight::String("--mutate-subset-boundaries");
  inject_merge_aliasing_mutation =
      mutation == flight::String("--mutate-merge-aliasing");
  inject_validation_branch_mutation =
      mutation == flight::String("--mutate-validation-branches");

  construction_clone_and_owner_identity();
  bounds_cache_and_position_mutation();
  index_order_and_compaction();
  failure_paths_are_non_mutating();
  weld_equivalence_and_attribute_sensitivity();
  weld_remap_order_and_index_width();
  subset_boundaries_replacement_and_rebase();
  merge_order_rebase_and_owner_isolation();
  validation_branch_order_and_nonmutation();
  packed_channel_order_and_ownership();
  packed_stride_offset_and_invalid_layout();
  packed_normalization_and_writes();
  normals_tangents_and_group_boundaries();
  layout_conversion_ownership_and_round_trip();
  primitive_builder_topology_order_ownership_and_bounds();
  primitive_builder_seam_behavior();
  primitive_builder_parameter_guards_and_degeneracy();
  return failures;
}
