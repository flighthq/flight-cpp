#include <flight/mesh/mesh_geometry.hpp>
#include <flight/mesh/mesh_geometry_attributes.hpp>
#include <flight/mesh/mesh_geometry_compute.hpp>
#include <flight/mesh/mesh_geometry_index.hpp>
#include <flight/mesh/mesh_geometry_layout.hpp>
#include <flight/mesh/mesh_geometry_operations.hpp>

#include <cmath>
#include <cstddef>
#include <iostream>
#include <optional>
#include <utility>
#include <variant>

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

  construction_clone_and_owner_identity();
  bounds_cache_and_position_mutation();
  index_order_and_compaction();
  failure_paths_are_non_mutating();
  packed_channel_order_and_ownership();
  packed_stride_offset_and_invalid_layout();
  packed_normalization_and_writes();
  normals_tangents_and_group_boundaries();
  layout_conversion_ownership_and_round_trip();
  return failures;
}
