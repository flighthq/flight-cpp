#include <flight/mesh/mesh_geometry.hpp>
#include <flight/mesh/mesh_geometry_attributes.hpp>
#include <flight/mesh/mesh_geometry_compute.hpp>
#include <flight/mesh/mesh_geometry_index.hpp>
#include <flight/mesh/mesh_geometry_operations.hpp>

#include <cmath>
#include <iostream>
#include <variant>

namespace {

namespace mesh = flight::mesh;
namespace types = flight::types;

int failures = 0;
bool inject_owner_identity_mutation = false;
bool inject_bounds_mutation = false;
bool inject_index_order_mutation = false;
bool inject_failure_mutation = false;

void check(bool condition, const char* message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

flight::Ref<types::VertexAttributeLayout> position_layout(double stride = 12.0) {
  auto position = flight::make_ref<types::VertexAttribute>();
  position->byte_offset = 0.0;
  position->format = flight::String("float32x3");
  position->semantic = flight::String("position");
  auto layout = flight::make_ref<types::VertexAttributeLayout>();
  layout->attributes = flight::Array<flight::Ref<types::VertexAttribute>>{position};
  layout->stride = stride;
  return layout;
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

  construction_clone_and_owner_identity();
  bounds_cache_and_position_mutation();
  index_order_and_compaction();
  failure_paths_are_non_mutating();
  return failures;
}
