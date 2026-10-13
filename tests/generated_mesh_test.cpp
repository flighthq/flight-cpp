#include <flight/geometry/bounding_sphere.hpp>
#include <flight/geometry/matrix4.hpp>
#include <flight/mesh/mesh_geometry.hpp>
#include <flight/mesh/mesh_geometry_attributes.hpp>
#include <flight/mesh/mesh_geometry_builders.hpp>
#include <flight/mesh/mesh_geometry_compute.hpp>
#include <flight/mesh/mesh_geometry_deformation_clone.hpp>
#include <flight/mesh/mesh_geometry_index.hpp>
#include <flight/mesh/mesh_geometry_layout.hpp>
#include <flight/mesh/mesh_geometry_operations.hpp>
#include <flight/mesh/mesh_geometry_subset.hpp>
#include <flight/mesh/mesh_geometry_transforms.hpp>
#include <flight/mesh/mesh_geometry_uvs.hpp>
#include <flight/mesh/morph_mesh_geometry.hpp>
#include <flight/mesh/update_mesh_morph.hpp>
#include <flight/mesh/vertex_format.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <functional>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>
#include <variant>
#include <vector>

namespace {

namespace geometry = flight::geometry;
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
bool inject_transform_component_mutation = false;
bool inject_mirrored_transform_mutation = false;
bool inject_transform_bounds_mutation = false;
bool inject_uv_axes_mutation = false;
bool inject_morph_target_order_mutation = false;
bool inject_morph_weight_math_mutation = false;
bool inject_morph_reset_mutation = false;
bool inject_morph_aliasing_mutation = false;
bool inject_morph_bounds_mutation = false;
bool inject_morph_validation_mutation = false;
bool inject_format_component_mutation = false;
bool inject_accessor_offset_stride_mutation = false;
bool inject_accessor_normalization_mutation = false;
bool inject_accessor_bounds_mutation = false;
bool inject_accessor_missing_default_mutation = false;
bool inject_clone_aliasing_mutation = false;
bool inject_omitted_metadata_mutation = false;
bool inject_lifecycle_ordering_mutation = false;
bool inject_reset_disposal_mutation = false;
bool inject_cache_invalidation_mutation = false;
bool inject_sphere_math_mutation = false;
bool inject_flat_split_remap_mutation = false;
bool inject_refresh_invalidation_mutation = false;
bool inject_wireframe_edge_mutation = false;
bool inject_index_promotion_mutation = false;
bool inject_attribute_defaults_mutation = false;
bool inject_attribute_count_mutation = false;
bool inject_attribute_ownership_mutation = false;
bool inject_metadata_groups_mutation = false;
bool inject_operation_order_mutation = false;
bool inject_operation_rollback_mutation = false;

void check(bool condition, const char* message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

bool near(double actual, double expected, double tolerance = 1.0e-5) {
  return std::abs(actual - expected) <= tolerance;
}

bool has_pristine_mesh_runtime(
    const flight::Ref<types::MeshGeometryRuntime>& runtime,
    double bounds_version) {
  return !runtime->attribute_data_view.has_value() &&
         !runtime->binding.has_value() &&
         runtime->bounds_version == bounds_version &&
         !runtime->morph_bind_pose.has_value() &&
         !runtime->morph_blended_weights.has_value() &&
         !runtime->skin_bind_pose.has_value() && !runtime->uid.has_value() &&
         !runtime->webgl_data.has_value() &&
         !runtime->webgpu_data.has_value();
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

flight::Ref<types::MorphTarget> morph_target(
    const flight::Float32Array& position_deltas,
    std::optional<flight::Float32Array> normal_deltas = std::nullopt,
    std::optional<flight::Float32Array> tangent_deltas = std::nullopt) {
  return flight::make_ref<types::MorphTarget>(types::MorphTarget{
      .normal_deltas = std::move(normal_deltas),
      .position_deltas = position_deltas,
      .tangent_deltas = std::move(tangent_deltas),
  });
}

flight::Ref<types::MeshMorph> mesh_morph(
    flight::Array<flight::Ref<types::MorphTarget>> targets,
    const flight::Float32Array& weights) {
  return flight::make_ref<types::MeshMorph>(types::MeshMorph{
      .targets = std::move(targets), .weights = weights});
}

flight::Ref<types::Mesh> morph_carrier(
    const flight::Ref<types::MeshGeometry>& geometry,
    std::variant<flight::Ref<types::MeshMorph>, flight::Null,
                 flight::Undefined> morph) {
  auto carrier = flight::make_ref<types::Mesh>();
  carrier->geometry = geometry;
  carrier->morph = std::move(morph);
  return carrier;
}

flight::Ref<types::MeshGeometry> canonical_frame_vertex(
    double px, double py, double pz, double nx, double ny, double nz,
    double tx, double ty, double tz, double tw, double u = 0.0,
    double v = 0.0) {
  flight::Float32Array vertices{
      static_cast<float>(px), static_cast<float>(py),
      static_cast<float>(pz), static_cast<float>(nx),
      static_cast<float>(ny), static_cast<float>(nz),
      static_cast<float>(tx), static_cast<float>(ty),
      static_cast<float>(tz), static_cast<float>(tw),
      static_cast<float>(u), static_cast<float>(v),
  };
  return geometry_from(mesh::canonical_mesh_geometry_layout, vertices);
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

template <typename Call>
bool throws_std_range_error(Call&& call) {
  try {
    call();
  } catch (const std::range_error&) {
    return true;
  } catch (...) {
    return false;
  }
  return false;
}

template <typename Call>
bool throws_logic_error(Call&& call) {
  try {
    call();
  } catch (const std::logic_error&) {
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

void construction_defaults_metadata_and_absence_matrix() {
  const auto layout = position_layout();
  flight::Float32Array partial_vertices{1.0F, 2.0F, 3.0F, 99.0F};
  auto omitted_options = flight::make_ref<types::MeshGeometryOptions>();
  omitted_options->layout = layout;
  omitted_options->vertices = partial_vertices;
  const auto omitted = mesh::create_mesh_geometry(omitted_options);
  const auto omitted_runtime = omitted->entity_runtime_key.value();
  bool metadata = omitted->layout == layout &&
                  omitted->vertices.buffer == partial_vertices.buffer &&
                  mesh::get_mesh_geometry_vertex_count(omitted) == 1.0 &&
                  !omitted->indices.has_value() &&
                  omitted->topology == flight::String("triangle-list") &&
                  omitted->subsets.size() == 1 &&
                  omitted->subsets.element(0.0)->index_count == 1.0 &&
                  omitted->subsets.element(0.0)->index_offset == 0.0 &&
                  !omitted->bounds.has_value() && omitted->version == 0.0 &&
                  has_pristine_mesh_runtime(omitted_runtime, -1.0);

  auto null_options = flight::make_ref<types::MeshGeometryOptions>();
  null_options->indices =
      std::variant<flight::SequenceView<double>, flight::Null,
                   flight::Undefined>{std::in_place_type<flight::Null>,
                                      flight::null};
  null_options->layout = layout;
  null_options->vertices = flight::Float32Array(6.0);
  const auto explicit_null = mesh::create_mesh_geometry(null_options);
  metadata = metadata && !explicit_null->indices.has_value() &&
             explicit_null->subsets.element(0.0)->index_count == 2.0;

  flight::Uint16Array empty_indices(0.0);
  auto empty_index_options = flight::make_ref<types::MeshGeometryOptions>();
  empty_index_options->indices = flight::SequenceView<double>(empty_indices);
  empty_index_options->layout = layout;
  empty_index_options->vertices = flight::Float32Array(3.0);
  const auto present_empty = mesh::create_mesh_geometry(empty_index_options);
  metadata =
      metadata && present_empty->indices.has_value() &&
      std::holds_alternative<flight::Uint16Array>(
          present_empty->indices.value()) &&
      std::get<flight::Uint16Array>(present_empty->indices.value()).size() ==
          0 &&
      std::get<flight::Uint16Array>(present_empty->indices.value()).buffer !=
          empty_indices.buffer &&
      present_empty->subsets.element(0.0)->index_count == 0.0;

  flight::Uint32Array authored_u32{2, 1, 0};
  auto u32_options = flight::make_ref<types::MeshGeometryOptions>();
  u32_options->indices = flight::SequenceView<double>(authored_u32);
  u32_options->layout = layout;
  u32_options->vertices = flight::Float32Array(9.0);
  const auto u32_geometry = mesh::create_mesh_geometry(u32_options);
  metadata = metadata && u32_geometry->indices.has_value() &&
             std::holds_alternative<flight::Uint32Array>(
                 u32_geometry->indices.value()) &&
             std::get<flight::Uint32Array>(u32_geometry->indices.value())
                     .buffer != authored_u32.buffer &&
             index_at(u32_geometry, 0.0) == 2.0 &&
             index_at(u32_geometry, 1.0) == 1.0 &&
             index_at(u32_geometry, 2.0) == 0.0;

  flight::Uint16Array narrow_indices{0, 1, 2};
  auto promoted_options = flight::make_ref<types::MeshGeometryOptions>();
  promoted_options->indices = flight::SequenceView<double>(narrow_indices);
  promoted_options->layout = layout;
  promoted_options->vertices = flight::Float32Array(65536.0 * 3.0);
  const auto promoted = mesh::create_mesh_geometry(promoted_options);
  metadata = metadata && promoted->indices.has_value() &&
             std::holds_alternative<flight::Uint32Array>(
                 promoted->indices.value()) &&
             index_at(promoted, 2.0) == 2.0 &&
             narrow_indices.get_index(2.0) == 2.0;

  flight::Array<flight::Ref<types::MeshSubset>> authored_subsets{
      mesh_subset(2.0, 1.0), mesh_subset(1.0, 0.0)};
  auto authored_options = flight::make_ref<types::MeshGeometryOptions>();
  authored_options->layout = layout;
  authored_options->subsets = authored_subsets;
  authored_options->topology = flight::String("line-list");
  authored_options->vertices = flight::Float32Array(9.0);
  const auto authored = mesh::create_mesh_geometry(authored_options);
  metadata = metadata &&
             authored->subsets.identity() == authored_subsets.identity() &&
             authored->subsets.element(0.0) ==
                 authored_subsets.element(0.0) &&
             authored->topology == flight::String("line-list");
  authored->subsets.element(0.0)->index_offset = 7.0;
  metadata = metadata &&
             authored_subsets.element(0.0)->index_offset == 7.0;

  const auto zero_layout = position_layout(0.0);
  flight::Float32Array zero_vertices{4.0F, 5.0F, 6.0F};
  const auto zero_stride = geometry_from(zero_layout, zero_vertices);
  metadata = metadata &&
             mesh::get_mesh_geometry_vertex_count(zero_stride) == 0.0 &&
             zero_stride->subsets.element(0.0)->index_count == 0.0 &&
             zero_stride->vertices.buffer == zero_vertices.buffer &&
             zero_stride->vertices.get_index(0.0) == 4.0 &&
             zero_stride->version == 0.0;

  auto metadata_options = flight::make_ref<types::MeshGeometryOptions>();
  flight::Uint16Array metadata_indices{0, 1, 2};
  flight::Array<flight::Ref<types::MeshSubset>> metadata_subsets{
      mesh_subset(2.0, 1.0)};
  metadata_options->indices = flight::SequenceView<double>(metadata_indices);
  metadata_options->layout = layout;
  metadata_options->subsets = metadata_subsets;
  metadata_options->topology = flight::String("line-strip");
  metadata_options->vertices = flight::Float32Array{
      -1.0F, -2.0F, -3.0F, 4.0F, 5.0F, 6.0F, 7.0F, 8.0F, 9.0F};
  const auto source = mesh::create_mesh_geometry(metadata_options);
  mesh::ensure_mesh_geometry_bounds(source);
  source->version = 9.0;
  const auto source_runtime = source->entity_runtime_key.value();
  source_runtime->webgl_data =
      flight::make_ref<types::MeshGeometryGlData>();
  const auto copy = mesh::clone_mesh_geometry_metadata(source);
  const auto copy_runtime = copy->entity_runtime_key.value();
  const auto source_index_owner =
      std::get<flight::Uint16Array>(source->indices.value()).buffer;
  metadata =
      metadata && copy != source && copy->vertices.size() == 0 &&
      copy->vertices.buffer != source->vertices.buffer &&
      copy->layout == source->layout && copy->topology == source->topology &&
      copy->version == 0.0 && copy->indices.has_value() &&
      std::holds_alternative<flight::Uint16Array>(copy->indices.value()) &&
      std::get<flight::Uint16Array>(copy->indices.value()).buffer ==
          source_index_owner &&
      copy->subsets.identity() != source->subsets.identity() &&
      copy->subsets.element(0.0) != source->subsets.element(0.0) &&
      copy->bounds.has_value() && copy->bounds.value() != source->bounds.value() &&
      copy->bounds.value()->min != source->bounds.value()->min &&
      copy->bounds.value()->max != source->bounds.value()->max &&
      copy_runtime != source_runtime &&
      has_pristine_mesh_runtime(copy_runtime, 0.0) &&
      source_runtime->webgl_data.has_value();
  copy->subsets.element(0.0)->index_count = 77.0;
  copy->bounds.value()->min->x = -99.0;
  std::get<flight::Uint16Array>(copy->indices.value()).set_index(0.0, 2.0);
  metadata = metadata && source->subsets.element(0.0)->index_count == 2.0 &&
             source->bounds.value()->min->x == -1.0 &&
             index_at(source, 0.0) == 2.0;

  const auto absent_source = geometry_from(layout, flight::Float32Array(0.0));
  const auto absent_copy = mesh::clone_mesh_geometry_metadata(absent_source);
  metadata = metadata && !absent_copy->bounds.has_value() &&
             !absent_copy->indices.has_value() &&
             absent_copy->vertices.size() == 0 &&
             absent_copy->subsets.identity() !=
                 absent_source->subsets.identity() &&
             has_pristine_mesh_runtime(
                 absent_copy->entity_runtime_key.value(), -1.0);
  if (inject_omitted_metadata_mutation) metadata = false;
  check(metadata,
        "construction distinguishes omitted, null, present-empty, narrow, promoted, and Uint32 index inputs; retains authored vertex/layout/subset owners and defaults; floors partial records; and metadata cloning preserves only the upstream shared index/layout identities while isolating subsets, bounds, vertices, version, and runtime including absent metadata");
}

void attribute_construction_defaults_and_missing_matrix() {
  const flight::Array<double> positions{
      0.0, 0.0, 0.0,
      1.0, 0.0, 0.0,
      0.0, 1.0, 0.0,
  };
  auto missing_options =
      flight::make_ref<types::MeshGeometryFromAttributesOptions>();
  missing_options->positions = positions;
  const auto missing =
      mesh::create_mesh_geometry_from_attributes(missing_options);
  bool defaults = missing->layout == mesh::canonical_mesh_geometry_layout &&
                  !missing->indices.has_value() &&
                  missing->topology == flight::String("triangle-list") &&
                  missing->subsets.size() == 1 &&
                  missing->subsets.element(0.0)->index_count == 3.0 &&
                  missing->version == 2.0 && missing->bounds.has_value() &&
                  missing->bounds.value()->min->x == 0.0 &&
                  missing->bounds.value()->min->y == 0.0 &&
                  missing->bounds.value()->max->x == 1.0 &&
                  missing->bounds.value()->max->y == 1.0 &&
                  has_pristine_mesh_runtime(
                      missing->entity_runtime_key.value(), 2.0);
  for (double vertex = 0.0; vertex < 3.0; vertex += 1.0) {
    const double base = vertex * 12.0;
    defaults = defaults && missing->vertices.get_index(base + 3.0) == 0.0 &&
               missing->vertices.get_index(base + 4.0) == 0.0 &&
               missing->vertices.get_index(base + 5.0) == 1.0 &&
               missing->vertices.get_index(base + 6.0) == 0.0 &&
               missing->vertices.get_index(base + 7.0) == 1.0 &&
               missing->vertices.get_index(base + 8.0) == 0.0 &&
               missing->vertices.get_index(base + 9.0) == 1.0 &&
               missing->vertices.get_index(base + 10.0) == 0.0 &&
               missing->vertices.get_index(base + 11.0) == 0.0;
  }

  auto null_options =
      flight::make_ref<types::MeshGeometryFromAttributesOptions>();
  null_options->positions = positions;
  null_options->normals = flight::null;
  null_options->uvs = flight::null;
  const auto explicit_null =
      mesh::create_mesh_geometry_from_attributes(null_options);
  defaults = defaults && explicit_null->version == 2.0 &&
             !explicit_null->indices.has_value() &&
             explicit_null->vertices.get_index(5.0) == 1.0 &&
             explicit_null->vertices.get_index(7.0) == 1.0 &&
             explicit_null->vertices.get_index(10.0) == 0.0;

  auto normals_only_options =
      flight::make_ref<types::MeshGeometryFromAttributesOptions>();
  normals_only_options->positions = positions;
  normals_only_options->normals = flight::Array<double>{
      0.0, 0.0, -1.0,
      0.0, 0.0, -1.0,
      0.0, 0.0, -1.0,
  };
  const auto normals_only =
      mesh::create_mesh_geometry_from_attributes(normals_only_options);
  defaults = defaults && normals_only->version == 1.0 &&
             normals_only->vertices.get_index(5.0) == -1.0 &&
             normals_only->vertices.get_index(6.0) == 0.0 &&
             normals_only->vertices.get_index(7.0) == -1.0 &&
             normals_only->vertices.get_index(8.0) == 0.0 &&
             normals_only->vertices.get_index(9.0) == 1.0 &&
             normals_only->vertices.get_index(10.0) == 0.0 &&
             normals_only->vertices.get_index(11.0) == 0.0;

  auto uvs_only_options =
      flight::make_ref<types::MeshGeometryFromAttributesOptions>();
  uvs_only_options->indices = flight::Array<double>{0.0, 1.0, 2.0};
  uvs_only_options->positions = positions;
  uvs_only_options->uvs = flight::Array<double>{
      0.0, 0.0,
      1.0, 0.0,
      0.0, 1.0,
  };
  const auto uvs_only =
      mesh::create_mesh_geometry_from_attributes(uvs_only_options);
  defaults = defaults && uvs_only->indices.has_value() &&
             std::holds_alternative<flight::Uint16Array>(
                 uvs_only->indices.value()) &&
             uvs_only->version == 2.0 &&
             uvs_only->vertices.get_index(5.0) == 1.0 &&
             uvs_only->vertices.get_index(6.0) == 1.0 &&
             uvs_only->vertices.get_index(7.0) == 0.0 &&
             uvs_only->vertices.get_index(8.0) == 0.0 &&
             uvs_only->vertices.get_index(9.0) == 1.0;

  auto empty_options =
      flight::make_ref<types::MeshGeometryFromAttributesOptions>();
  empty_options->positions = flight::Array<double>{};
  const auto empty = mesh::create_mesh_geometry_from_attributes(empty_options);
  defaults = defaults && empty->vertices.empty() &&
             !empty->indices.has_value() && empty->version == 2.0 &&
             empty->subsets.size() == 1 &&
             empty->subsets.element(0.0)->index_count == 0.0 &&
             empty->bounds.has_value() &&
             empty->bounds.value()->min->x ==
                 std::numeric_limits<double>::infinity() &&
             empty->bounds.value()->max->x ==
                 -std::numeric_limits<double>::infinity() &&
             has_pristine_mesh_runtime(
                 empty->entity_runtime_key.value(), 2.0) &&
             mesh::validate_mesh_geometry(empty);
  if (inject_attribute_defaults_mutation) defaults = false;
  check(defaults,
        "attribute construction installs the canonical topology/layout/subset, derives only missing normals, always derives tangents, zero-fills missing UVs, distinguishes supplied normals and UV gradients, computes bounds/cache versions, and produces a valid explicit empty geometry");
}

void attribute_construction_count_and_layout_validation() {
  const flight::Array<double> positions{
      0.0, 0.0, 0.0,
      1.0, 0.0, 0.0,
      0.0, 1.0, 0.0,
  };
  const flight::Array<double> short_normals{
      0.0, 0.0, 1.0,
      0.0, 0.0, 1.0,
      0.0, 0.0,
  };
  auto short_normal_options =
      flight::make_ref<types::MeshGeometryFromAttributesOptions>();
  short_normal_options->positions = positions;
  short_normal_options->normals = short_normals;
  bool counts = throws_std_range_error([&] {
                  static_cast<void>(mesh::create_mesh_geometry_from_attributes(
                      short_normal_options));
                }) &&
                short_normal_options->positions.identity() ==
                    positions.identity() &&
                std::get<flight::Array<double>>(
                    short_normal_options->normals).identity() ==
                    short_normals.identity() &&
                short_normals.element(7.0) == 0.0;

  const flight::Array<double> short_uvs{0.0, 0.0, 1.0, 0.0, 0.0};
  auto short_uv_options =
      flight::make_ref<types::MeshGeometryFromAttributesOptions>();
  short_uv_options->positions = positions;
  short_uv_options->uvs = short_uvs;
  counts = counts && throws_std_range_error([&] {
             static_cast<void>(mesh::create_mesh_geometry_from_attributes(
                 short_uv_options));
           }) &&
           std::get<flight::Array<double>>(short_uv_options->uvs).identity() ==
               short_uvs.identity() &&
           short_uvs.element(4.0) == 0.0;

  const flight::Array<double> partial_positions{
      0.0, 0.0, 0.0,
      1.0, 0.0, 0.0,
      0.0, 1.0,
  };
  auto partial_options =
      flight::make_ref<types::MeshGeometryFromAttributesOptions>();
  partial_options->positions = partial_positions;
  counts = counts && throws_std_range_error([&] {
             static_cast<void>(mesh::create_mesh_geometry_from_attributes(
                 partial_options));
           }) &&
           partial_options->positions.identity() == partial_positions.identity();

  const flight::Array<double> long_normals{
      0.0, 0.0, 1.0,
      0.0, 0.0, 1.0,
      0.0, 0.0, 1.0,
      91.0, 92.0, 93.0,
  };
  const flight::Array<double> long_uvs{
      0.0, 0.0,
      1.0, 0.0,
      0.0, 1.0,
      94.0, 95.0,
  };
  auto long_options =
      flight::make_ref<types::MeshGeometryFromAttributesOptions>();
  long_options->indices =
      flight::Array<double>{0.0, 1.0, 2.0, 0.0};
  long_options->normals = long_normals;
  long_options->positions = positions;
  long_options->uvs = long_uvs;
  const auto long_geometry =
      mesh::create_mesh_geometry_from_attributes(long_options);
  counts = counts && mesh::get_mesh_geometry_vertex_count(long_geometry) == 3.0 &&
           mesh::get_mesh_geometry_index_count(long_geometry) == 4.0 &&
           long_geometry->vertices.size() == 36 &&
           long_geometry->vertices.get_index(29.0) == 1.0 &&
           long_geometry->vertices.get_index(34.0) == 0.0 &&
           long_geometry->vertices.get_index(35.0) == 1.0 &&
           long_normals.element(9.0) == 91.0 &&
           long_uvs.element(6.0) == 94.0 &&
           !mesh::validate_mesh_geometry(long_geometry);

  const auto reference = vertex_layout(
      flight::Array<flight::Ref<types::VertexAttribute>>{
          vertex_attribute("position", "float32x3", 0.0),
          vertex_attribute("normal", "float32x3", 12.0)},
      24.0);
  const auto equal = vertex_layout(
      flight::Array<flight::Ref<types::VertexAttribute>>{
          vertex_attribute("position", "float32x3", 0.0),
          vertex_attribute("normal", "float32x3", 12.0)},
      24.0);
  const auto fewer = vertex_layout(
      flight::Array<flight::Ref<types::VertexAttribute>>{
          vertex_attribute("position", "float32x3", 0.0)},
      24.0);
  const auto different_semantic = vertex_layout(
      flight::Array<flight::Ref<types::VertexAttribute>>{
          vertex_attribute("position", "float32x3", 0.0),
          vertex_attribute("uv0", "float32x3", 12.0)},
      24.0);
  const auto different_format = vertex_layout(
      flight::Array<flight::Ref<types::VertexAttribute>>{
          vertex_attribute("position", "float32x3", 0.0),
          vertex_attribute("normal", "float32x2", 12.0)},
      24.0);
  const auto different_offset = vertex_layout(
      flight::Array<flight::Ref<types::VertexAttribute>>{
          vertex_attribute("position", "float32x3", 0.0),
          vertex_attribute("normal", "float32x3", 8.0)},
      24.0);
  const auto different_order = vertex_layout(
      flight::Array<flight::Ref<types::VertexAttribute>>{
          vertex_attribute("normal", "float32x3", 12.0),
          vertex_attribute("position", "float32x3", 0.0)},
      24.0);
  const auto different_stride = vertex_layout(
      flight::Array<flight::Ref<types::VertexAttribute>>{
          vertex_attribute("position", "float32x3", 0.0),
          vertex_attribute("normal", "float32x3", 12.0)},
      28.0);
  counts = counts && mesh::layouts_match(reference, equal) &&
           reference != equal && !mesh::layouts_match(reference, fewer) &&
           !mesh::layouts_match(reference, different_semantic) &&
           !mesh::layouts_match(reference, different_format) &&
           !mesh::layouts_match(reference, different_offset) &&
           !mesh::layouts_match(reference, different_order) &&
           !mesh::layouts_match(reference, different_stride);
  if (inject_attribute_count_mutation) counts = false;
  check(counts,
        "attribute construction rejects short normal, UV, and partial-position domains before exposing output while ignoring complete trailing attribute records; geometry validation catches incomplete primitive counts, and merge compatibility requires exact attribute count, semantic, format, byte offset, order, and stride");
}

void attribute_construction_input_output_ownership() {
  flight::Array<double> positions{
      0.0, 0.0, 0.0,
      1.0, 0.0, 0.0,
      0.0, 1.0, 0.0,
  };
  flight::Array<double> normals{
      0.0, 0.0, 1.0,
      0.0, 0.0, 1.0,
      0.0, 0.0, 1.0,
  };
  flight::Array<double> uvs{
      0.0, 0.0,
      1.0, 0.0,
      0.0, 1.0,
  };
  flight::Uint32Array indices{2, 0, 1};
  const void* positions_owner = positions.identity();
  const void* normals_owner = normals.identity();
  const void* uvs_owner = uvs.identity();
  const auto indices_owner = indices.buffer;
  auto options = flight::make_ref<types::MeshGeometryFromAttributesOptions>();
  options->indices = indices;
  options->normals = normals;
  options->positions = positions;
  options->uvs = uvs;
  const auto geometry = mesh::create_mesh_geometry_from_attributes(options);
  const auto geometry_vertex_owner = geometry->vertices.buffer;
  const void* geometry_index_owner = index_buffer_identity(geometry);
  bool ownership = options->positions.identity() == positions_owner &&
                   std::get<flight::Array<double>>(options->normals).identity() ==
                       normals_owner &&
                   std::get<flight::Array<double>>(options->uvs).identity() ==
                       uvs_owner &&
                   std::get<flight::Uint32Array>(options->indices).buffer ==
                       indices_owner &&
                   geometry->layout == mesh::canonical_mesh_geometry_layout &&
                   geometry->indices.has_value() &&
                   std::holds_alternative<flight::Uint16Array>(
                       geometry->indices.value()) &&
                   geometry_index_owner != indices.buffer.identity() &&
                   index_at(geometry, 0.0) == 2.0 &&
                   index_at(geometry, 1.0) == 0.0 &&
                   index_at(geometry, 2.0) == 1.0 &&
                   geometry->version == 1.0 &&
                   geometry->vertices.get_index(0.0) == 0.0 &&
                   geometry->vertices.get_index(5.0) == 1.0 &&
                   geometry->vertices.get_index(10.0) == 0.0;

  positions.element(0.0) = 99.0;
  normals.element(2.0) = -7.0;
  uvs.element(0.0) = 0.75;
  indices.set_index(0.0, 0.0);
  ownership = ownership && geometry->vertices.buffer == geometry_vertex_owner &&
              index_buffer_identity(geometry) == geometry_index_owner &&
              geometry->vertices.get_index(0.0) == 0.0 &&
              geometry->vertices.get_index(5.0) == 1.0 &&
              geometry->vertices.get_index(10.0) == 0.0 &&
              index_at(geometry, 0.0) == 2.0;

  geometry->vertices.set_index(1.0, 111.0);
  std::get<flight::Uint16Array>(geometry->indices.value())
      .set_index(1.0, 2.0);
  ownership = ownership && positions.element(1.0) == 0.0 &&
              indices.get_index(1.0) == 0.0 &&
              normals.element(2.0) == -7.0 && uvs.element(0.0) == 0.75;
  if (inject_attribute_ownership_mutation) ownership = false;
  check(ownership,
        "attribute construction reads every caller-owned array without replacing it, owns fresh interleaved vertex and narrowed index storage, retains the shared canonical layout, and isolates subsequent mutations in both directions");
}

void clone_aliasing_width_and_runtime_matrix() {
  const auto layout = position_layout();
  flight::Float32Array vertices{
      -2.0F, 1.0F, 3.0F, 4.0F, -5.0F, 6.0F, 1.0F, 2.0F, -7.0F};
  flight::Uint32Array indices{2, 0, 1};
  flight::Array<flight::Ref<types::MeshSubset>> subsets{
      mesh_subset(3.0, 0.0)};
  auto options = flight::make_ref<types::MeshGeometryOptions>();
  options->indices = flight::SequenceView<double>(indices);
  options->layout = layout;
  options->subsets = subsets;
  options->topology = flight::String("triangle-strip");
  options->vertices = vertices;
  const auto source = mesh::create_mesh_geometry(options);
  mesh::ensure_mesh_geometry_bounds(source);
  source->version = 7.0;
  const auto source_runtime = source->entity_runtime_key.value();
  const auto gl_data = flight::make_ref<types::MeshGeometryGlData>();
  const auto wgpu_data = flight::make_ref<types::MeshGeometryWgpuData>();
  const auto morph_pose = flight::make_ref<types::MeshMorphBindPose>();
  const auto skin_pose = flight::make_ref<types::MeshSkinBindPose>();
  source_runtime->attribute_data_view =
      flight::make_ref<types::buffer_byte_offset_byte_length_view_cb5f900d5c8eb8e9>(
          types::buffer_byte_offset_byte_length_view_cb5f900d5c8eb8e9{
              .buffer = source->vertices.buffer,
              .byte_offset = static_cast<double>(source->vertices.byte_offset),
              .byte_length = static_cast<double>(source->vertices.byte_length),
              .view = flight::DataView(
                  source->vertices.buffer,
                  static_cast<double>(source->vertices.byte_offset),
                  static_cast<double>(source->vertices.byte_length)),
          });
  source_runtime->binding = flight::ErasedRef(gl_data);
  source_runtime->morph_bind_pose = morph_pose;
  source_runtime->morph_blended_weights = flight::Float32Array{0.25F, 0.75F};
  source_runtime->skin_bind_pose = skin_pose;
  source_runtime->uid = flight::String("source-runtime");
  source_runtime->webgl_data = gl_data;
  source_runtime->webgpu_data = wgpu_data;

  const auto clone = mesh::clone_mesh_geometry(source);
  const auto clone_runtime = clone->entity_runtime_key.value();
  bool aliasing =
      clone != source && clone->layout == source->layout &&
      clone->topology == source->topology && clone->version == 0.0 &&
      source->version == 7.0 && clone->vertices.buffer != source->vertices.buffer &&
      clone->indices.has_value() &&
      std::holds_alternative<flight::Uint32Array>(clone->indices.value()) &&
      std::get<flight::Uint32Array>(clone->indices.value()).buffer !=
          std::get<flight::Uint32Array>(source->indices.value()).buffer &&
      index_at(clone, 0.0) == 2.0 && index_at(clone, 1.0) == 0.0 &&
      index_at(clone, 2.0) == 1.0 &&
      clone->subsets.identity() != source->subsets.identity() &&
      clone->subsets.element(0.0) != source->subsets.element(0.0) &&
      clone->bounds.has_value() && clone->bounds.value() != source->bounds.value() &&
      clone->bounds.value()->min != source->bounds.value()->min &&
      clone->bounds.value()->max != source->bounds.value()->max &&
      clone_runtime != source_runtime &&
      has_pristine_mesh_runtime(clone_runtime, 0.0) &&
      source_runtime->attribute_data_view.has_value() &&
      source_runtime->binding.has_value() &&
      source_runtime->morph_bind_pose ==
          std::optional<flight::Ref<types::MeshMorphBindPose>>{morph_pose} &&
      source_runtime->skin_bind_pose ==
          std::optional<flight::Ref<types::MeshSkinBindPose>>{skin_pose} &&
      source_runtime->webgl_data ==
          std::optional<flight::Ref<types::MeshGeometryGlData>>{gl_data} &&
      source_runtime->webgpu_data ==
          std::optional<flight::Ref<types::MeshGeometryWgpuData>>{wgpu_data};
  clone->vertices.set_index(0.0, 99.0);
  std::get<flight::Uint32Array>(clone->indices.value()).set_index(0.0, 0.0);
  clone->subsets.element(0.0)->index_count = 1.0;
  clone->bounds.value()->min->x = -100.0;
  aliasing = aliasing && source->vertices.get_index(0.0) == -2.0 &&
             index_at(source, 0.0) == 2.0 &&
             source->subsets.element(0.0)->index_count == 3.0 &&
             source->bounds.value()->min->x == -2.0;

  flight::Uint32Array empty_indices(0.0);
  auto empty_options = flight::make_ref<types::MeshGeometryOptions>();
  empty_options->indices = flight::SequenceView<double>(empty_indices);
  empty_options->layout = layout;
  empty_options->vertices = flight::Float32Array(0.0);
  const auto empty_source = mesh::create_mesh_geometry(empty_options);
  const auto empty_clone = mesh::clone_mesh_geometry(empty_source);
  aliasing = aliasing && empty_clone->indices.has_value() &&
             std::holds_alternative<flight::Uint32Array>(
                 empty_clone->indices.value()) &&
             std::get<flight::Uint32Array>(empty_clone->indices.value())
                     .size() == 0 &&
             std::get<flight::Uint32Array>(empty_clone->indices.value())
                     .buffer !=
                 std::get<flight::Uint32Array>(empty_source->indices.value())
                     .buffer &&
             !empty_clone->bounds.has_value() &&
             has_pristine_mesh_runtime(
                 empty_clone->entity_runtime_key.value(), -1.0);
  if (inject_clone_aliasing_mutation) aliasing = false;
  check(aliasing,
        "full cloning preserves layout, topology, values, index width and present-empty state while deep-copying vertices, indices, subset records, bounds vectors and runtime; source runtime caches and deformation/upload slots are never inherited or mutated");
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

void runtime_lifecycle_ordering_and_idempotence() {
  const auto geometry = indexed_geometry();
  mesh::ensure_mesh_geometry_bounds(geometry);
  const auto runtime = geometry->entity_runtime_key.value();
  const auto gl_data = flight::make_ref<types::MeshGeometryGlData>();
  const auto wgpu_data = flight::make_ref<types::MeshGeometryWgpuData>();
  const auto morph_pose = flight::make_ref<types::MeshMorphBindPose>();
  const auto skin_pose = flight::make_ref<types::MeshSkinBindPose>();
  runtime->binding = flight::ErasedRef(gl_data);
  runtime->morph_blended_weights = flight::Float32Array{0.5F};
  runtime->uid = flight::String("lifecycle-runtime");
  runtime->webgl_data = gl_data;
  runtime->webgpu_data = wgpu_data;
  mesh::set_mesh_geometry_morph_bind_pose(geometry, morph_pose);
  mesh::set_mesh_geometry_skin_bind_pose(geometry, skin_pose);
  const auto vertex_owner = geometry->vertices.buffer;
  const auto index_owner = index_buffer_identity(geometry);
  const auto subset_owner = geometry->subsets.identity();
  const auto bounds_owner = geometry->bounds.value();
  const double version = geometry->version;
  const double bounds_version = runtime->bounds_version;

  mesh::destroy_mesh_geometry_gl_data(geometry);
  bool lifecycle =
      !runtime->webgl_data.has_value() &&
      runtime->webgpu_data ==
          std::optional<flight::Ref<types::MeshGeometryWgpuData>>{wgpu_data} &&
      runtime->morph_bind_pose ==
          std::optional<flight::Ref<types::MeshMorphBindPose>>{morph_pose} &&
      runtime->skin_bind_pose ==
          std::optional<flight::Ref<types::MeshSkinBindPose>>{skin_pose} &&
      runtime->morph_blended_weights.has_value() &&
      runtime->binding.has_value() && runtime->uid.has_value();
  mesh::destroy_mesh_geometry_gl_data(geometry);
  lifecycle = lifecycle && !runtime->webgl_data.has_value() &&
              runtime->webgpu_data.has_value();
  mesh::destroy_mesh_geometry_wgpu_data(geometry);
  mesh::destroy_mesh_geometry_wgpu_data(geometry);
  lifecycle = lifecycle && !runtime->webgl_data.has_value() &&
              !runtime->webgpu_data.has_value() &&
              mesh::get_mesh_geometry_morph_bind_pose(geometry) ==
                  std::optional<flight::Ref<types::MeshMorphBindPose>>{
                      morph_pose} &&
              mesh::get_mesh_geometry_skin_bind_pose(geometry) ==
                  std::optional<flight::Ref<types::MeshSkinBindPose>>{
                      skin_pose};

  runtime->webgl_data = gl_data;
  runtime->webgpu_data = wgpu_data;
  mesh::destroy_mesh_geometry_wgpu_data(geometry);
  lifecycle = lifecycle && runtime->webgl_data.has_value() &&
              !runtime->webgpu_data.has_value();
  mesh::destroy_mesh_geometry_gl_data(geometry);
  lifecycle = lifecycle && !runtime->webgl_data.has_value() &&
              !runtime->webgpu_data.has_value() &&
              geometry->vertices.buffer == vertex_owner &&
              index_buffer_identity(geometry) == index_owner &&
              geometry->subsets.identity() == subset_owner &&
              geometry->bounds.value() == bounds_owner &&
              geometry->version == version &&
              runtime->bounds_version == bounds_version &&
              runtime->binding.has_value() && runtime->uid.has_value() &&
              runtime->morph_blended_weights.has_value();
  if (inject_lifecycle_ordering_mutation) lifecycle = false;
  check(lifecycle,
        "GL and WebGPU upload teardown is independent in either order and idempotent on repetition; it preserves CPU buffers, descriptors, bounds/version cache, binding, uid, deformation slots, and blended-weight state");
}

void runtime_reset_disposal_and_missing_owner_paths() {
  const auto geometry = indexed_geometry();
  const auto runtime = geometry->entity_runtime_key.value();
  const auto morph_pose = flight::make_ref<types::MeshMorphBindPose>();
  const auto skin_pose = flight::make_ref<types::MeshSkinBindPose>();
  mesh::set_mesh_geometry_morph_bind_pose(geometry, morph_pose);
  mesh::set_mesh_geometry_skin_bind_pose(geometry, skin_pose);
  bool reset = mesh::get_mesh_geometry_morph_bind_pose(geometry) ==
                   std::optional<flight::Ref<types::MeshMorphBindPose>>{
                       morph_pose} &&
               mesh::get_mesh_geometry_skin_bind_pose(geometry) ==
                   std::optional<flight::Ref<types::MeshSkinBindPose>>{
                       skin_pose};
  mesh::set_mesh_geometry_morph_bind_pose(geometry, std::nullopt);
  mesh::set_mesh_geometry_morph_bind_pose(geometry, std::nullopt);
  reset = reset &&
          !mesh::get_mesh_geometry_morph_bind_pose(geometry).has_value() &&
          mesh::get_mesh_geometry_skin_bind_pose(geometry) ==
              std::optional<flight::Ref<types::MeshSkinBindPose>>{skin_pose};
  mesh::set_mesh_geometry_skin_bind_pose(geometry, std::nullopt);
  mesh::set_mesh_geometry_skin_bind_pose(geometry, std::nullopt);
  reset = reset &&
          !mesh::get_mesh_geometry_skin_bind_pose(geometry).has_value();

  const auto gl_data = flight::make_ref<types::MeshGeometryGlData>();
  const auto wgpu_data = flight::make_ref<types::MeshGeometryWgpuData>();
  runtime->webgl_data = gl_data;
  runtime->webgpu_data = wgpu_data;
  mesh::set_mesh_geometry_morph_bind_pose(geometry, morph_pose);
  mesh::set_mesh_geometry_skin_bind_pose(geometry, skin_pose);
  const auto vertex_owner = geometry->vertices.buffer;
  const auto index_owner = index_buffer_identity(geometry);
  const auto subset_owner = geometry->subsets.identity();
  const double version = geometry->version;
  geometry->entity_runtime_key = std::nullopt;
  reset = reset &&
          mesh::get_mesh_geometry_runtime_if_present(geometry) ==
              std::optional<flight::Ref<types::MeshGeometryRuntime>>{runtime} &&
          mesh::get_mesh_geometry_morph_bind_pose(geometry) ==
              std::optional<flight::Ref<types::MeshMorphBindPose>>{morph_pose} &&
          mesh::get_mesh_geometry_skin_bind_pose(geometry) ==
              std::optional<flight::Ref<types::MeshSkinBindPose>>{skin_pose};
  mesh::destroy_mesh_geometry_gl_data(geometry);
  mesh::destroy_mesh_geometry_wgpu_data(geometry);
  mesh::set_mesh_geometry_morph_bind_pose(geometry, std::nullopt);
  mesh::set_mesh_geometry_skin_bind_pose(geometry, std::nullopt);
  reset = reset && !runtime->webgl_data.has_value() &&
          !runtime->webgpu_data.has_value() &&
          !runtime->morph_bind_pose.has_value() &&
          !runtime->skin_bind_pose.has_value() &&
          geometry->vertices.buffer == vertex_owner &&
          index_buffer_identity(geometry) == index_owner &&
          geometry->subsets.identity() == subset_owner &&
          geometry->version == version;

  auto detached = flight::make_ref<types::MeshGeometry>();
  detached->bounds = std::nullopt;
  detached->entity_runtime_key = std::nullopt;
  detached->indices = std::nullopt;
  detached->layout = position_layout();
  detached->subsets =
      flight::Array<flight::Ref<types::MeshSubset>>{mesh_subset(1.0, 0.0)};
  detached->topology = flight::String("point-list");
  detached->version = 13.0;
  detached->vertices = flight::Float32Array{1.0F, 2.0F, 3.0F};
  const auto detached_vertex_owner = detached->vertices.buffer;
  const auto detached_subset_owner = detached->subsets.identity();
  mesh::set_mesh_geometry_morph_bind_pose(detached, morph_pose);
  mesh::set_mesh_geometry_skin_bind_pose(detached, skin_pose);
  mesh::destroy_mesh_geometry_gl_data(detached);
  mesh::destroy_mesh_geometry_wgpu_data(detached);
  reset = reset &&
          !mesh::get_mesh_geometry_runtime_if_present(detached).has_value() &&
          !mesh::get_mesh_geometry_morph_bind_pose(detached).has_value() &&
          !mesh::get_mesh_geometry_skin_bind_pose(detached).has_value() &&
          detached->vertices.buffer == detached_vertex_owner &&
          detached->vertices.get_index(0.0) == 1.0 &&
          detached->subsets.identity() == detached_subset_owner &&
          detached->version == 13.0;
  if (inject_reset_disposal_mutation) reset = false;
  check(reset,
        "bind-pose reset is independent and repeatable, upload teardown follows the retained weak runtime when the public symbol cell is absent, and every lifecycle call is a non-mutating no-op for an owner with no runtime");
}

void cache_invalidation_and_empty_bounds_lifecycle() {
  const auto geometry = indexed_geometry();
  const auto runtime = geometry->entity_runtime_key.value();
  const auto gl_data = flight::make_ref<types::MeshGeometryGlData>();
  const auto wgpu_data = flight::make_ref<types::MeshGeometryWgpuData>();
  runtime->webgl_data = gl_data;
  runtime->webgpu_data = wgpu_data;
  const auto first = mesh::ensure_mesh_geometry_bounds(geometry);
  const auto bounds_owner = first.value();
  const double first_stamp = runtime->bounds_version;
  geometry->vertices.set_index(0.0, -20.0);
  const auto stale = mesh::ensure_mesh_geometry_bounds(geometry);
  bool cache = stale.has_value() && stale.value() == bounds_owner &&
               stale.value()->min->x == -2.0 && geometry->version == 0.0 &&
               runtime->bounds_version == first_stamp &&
               runtime->webgl_data.has_value() &&
               runtime->webgpu_data.has_value();
  mesh::invalidate_mesh_geometry(geometry);
  cache = cache && geometry->version == 1.0 &&
          runtime->bounds_version == first_stamp &&
          geometry->bounds.value() == bounds_owner &&
          runtime->webgl_data.has_value() && runtime->webgpu_data.has_value();
  const auto refreshed = mesh::ensure_mesh_geometry_bounds(geometry);
  cache = cache && refreshed.has_value() && refreshed.value() == bounds_owner &&
          refreshed.value()->min->x == -20.0 &&
          runtime->bounds_version == geometry->version;
  geometry->vertices.set_index(3.0, 40.0);
  mesh::invalidate_mesh_geometry(geometry);
  mesh::invalidate_mesh_geometry(geometry);
  cache = cache && geometry->version == 3.0 &&
          runtime->bounds_version == 1.0 &&
          geometry->bounds.value() == bounds_owner;
  mesh::ensure_mesh_geometry_bounds(geometry);
  cache = cache && geometry->bounds.value()->max->x == 40.0 &&
          runtime->bounds_version == 3.0;

  const auto empty = geometry_from(position_layout(), flight::Float32Array(0.0));
  const auto empty_runtime = empty->entity_runtime_key.value();
  const auto empty_vertex_owner = empty->vertices.buffer;
  const auto first_empty = mesh::ensure_mesh_geometry_bounds(empty);
  const auto empty_bounds_owner = empty->bounds.value();
  const auto second_empty = mesh::ensure_mesh_geometry_bounds(empty);
  cache = cache && !first_empty.has_value() && !second_empty.has_value() &&
          empty->bounds.value() == empty_bounds_owner &&
          empty_runtime->bounds_version == empty->version &&
          empty->vertices.buffer == empty_vertex_owner &&
          empty->vertices.size() == 0 && empty->version == 0.0;
  if (inject_cache_invalidation_mutation) cache = false;
  check(cache,
        "direct CPU writes remain deliberately invisible to the bounds cache until invalidate advances the revision; refresh then reuses the bounds owner, repeated invalidation accumulates, upload slots remain installed for version-based reupload, and empty bounds stay absent and idempotent without touching CPU state");
}

void bounding_sphere_exact_math_and_degenerate_cases() {
  const auto source = geometry_from(
      position_layout(),
      flight::Float32Array{
          -2.0F, -1.0F, 3.0F,
          4.0F, 5.0F, -3.0F,
          1.0F, 2.0F, 0.0F,
      });
  const auto vertex_owner = source->vertices.buffer;
  const auto runtime_owner = source->entity_runtime_key.value();
  const auto sphere = geometry::create_bounding_sphere(91.0, 92.0, 93.0, 94.0);
  const auto center_owner = sphere->center;
  mesh::compute_mesh_geometry_bounding_sphere(sphere, source);
  bool exact = sphere->center == center_owner &&
               near(sphere->center->x, 1.0) &&
               near(sphere->center->y, 2.0) &&
               near(sphere->center->z, 0.0) &&
               near(sphere->radius, std::sqrt(27.0)) &&
               source->vertices.buffer == vertex_owner &&
               source->entity_runtime_key.value() == runtime_owner &&
               !source->bounds.has_value() && source->version == 0.0 &&
               runtime_owner->bounds_version == -1.0;

  const auto asymmetric = geometry_from(
      position_layout(),
      flight::Float32Array{
          -4.0F, 0.0F, 0.0F,
          2.0F, 0.0F, 0.0F,
          1.0F, 0.0F, 0.0F,
      });
  mesh::compute_mesh_geometry_bounding_sphere(sphere, asymmetric);
  exact = exact && sphere->center == center_owner &&
          sphere->center->x == -1.0 && sphere->center->y == 0.0 &&
          sphere->center->z == 0.0 && sphere->radius == 3.0;

  const auto repeated = geometry_from(
      position_layout(),
      flight::Float32Array{
          7.0F, -8.0F, 9.0F,
          7.0F, -8.0F, 9.0F,
      });
  mesh::compute_mesh_geometry_bounding_sphere(sphere, repeated);
  exact = exact && sphere->center->x == 7.0 &&
          sphere->center->y == -8.0 && sphere->center->z == 9.0 &&
          sphere->radius == 0.0;

  const auto empty =
      geometry_from(position_layout(), flight::Float32Array(0.0));
  mesh::compute_mesh_geometry_bounding_sphere(sphere, empty);
  exact = exact && sphere->center == center_owner &&
          sphere->center->x == 0.0 && sphere->center->y == 0.0 &&
          sphere->center->z == 0.0 && sphere->radius == -1.0;

  const auto zero_stride = geometry_from(
      position_layout(0.0), flight::Float32Array{1.0F, 2.0F, 3.0F});
  sphere->center->x = 12.0;
  sphere->radius = 13.0;
  mesh::compute_mesh_geometry_bounding_sphere(sphere, zero_stride);
  exact = exact && sphere->center->x == 0.0 &&
          sphere->center->y == 0.0 && sphere->center->z == 0.0 &&
          sphere->radius == -1.0 &&
          zero_stride->vertices.get_index(0.0) == 1.0 &&
          zero_stride->version == 0.0;
  if (inject_sphere_math_mutation) exact = false;
  check(exact,
        "bounding spheres use the exact AABB midpoint and farthest authored vertex, preserve the destination center owner and source state, return zero radius for repeated points, and reset empty or zero-stride inputs to the invalid sentinel");
}

void flat_normals_split_order_and_ownership() {
  flight::Float32Array vertices(48.0);
  const double positions[][3] = {
      {0.0, 0.0, 0.0},
      {1.0, 0.0, 0.0},
      {0.0, 1.0, 0.0},
      {0.0, 0.0, 1.0},
  };
  for (double vertex = 0.0; vertex < 4.0; vertex += 1.0) {
    for (double component = 0.0; component < 3.0; component += 1.0) {
      vertices.set_index(
          vertex * 12.0 + component,
          positions[static_cast<std::size_t>(vertex)]
                   [static_cast<std::size_t>(component)]);
    }
  }
  flight::Uint16Array indices{0, 1, 2, 0, 3, 1};
  auto options = flight::make_ref<types::MeshGeometryOptions>();
  options->indices = flight::SequenceView<double>(indices);
  options->layout = mesh::canonical_mesh_geometry_layout;
  options->vertices = vertices;
  const auto source = mesh::create_mesh_geometry(options);
  const auto separate = mesh::clone_mesh_geometry(source);
  const auto separate_vertex_owner = separate->vertices.buffer;
  const auto separate_index_owner = index_buffer_identity(separate);
  mesh::compute_mesh_geometry_flat_normals(separate, source);
  const auto direct = mesh::clone_mesh_geometry(source);
  const auto direct_vertex_owner = direct->vertices.buffer;
  const auto direct_index_owner = index_buffer_identity(direct);
  mesh::compute_mesh_geometry_flat_normals(direct, direct);
  const double direct_expected[][3] = {
      {0.0, 1.0, 0.0},
      {0.0, 1.0, 0.0},
      {0.0, 0.0, 1.0},
      {0.0, 1.0, 0.0},
  };
  bool split = direct->vertices.buffer == direct_vertex_owner &&
               index_buffer_identity(direct) == direct_index_owner &&
               direct->version == 1.0 &&
               separate->vertices.buffer == separate_vertex_owner &&
               index_buffer_identity(separate) == separate_index_owner &&
               separate->vertices.buffer != source->vertices.buffer &&
               separate->version == 1.0 && source->version == 0.0;
  for (double vertex = 0.0; vertex < 4.0; vertex += 1.0) {
    for (double component = 0.0; component < 3.0; component += 1.0) {
      const double expected =
          direct_expected[static_cast<std::size_t>(vertex)]
                         [static_cast<std::size_t>(component)];
      split = split &&
              near(direct->vertices.get_index(
                       vertex * 12.0 + 3.0 + component),
                   expected) &&
              near(separate->vertices.get_index(
                       vertex * 12.0 + 3.0 + component),
                   expected);
    }
  }

  const auto expanded = mesh::expand_mesh_geometry_indices(source);
  const auto expanded_owner = expanded->vertices.buffer;
  const auto expanded_runtime = expanded->entity_runtime_key.value();
  const double expected_positions[][3] = {
      {0.0, 0.0, 0.0},
      {1.0, 0.0, 0.0},
      {0.0, 1.0, 0.0},
      {0.0, 0.0, 0.0},
      {0.0, 0.0, 1.0},
      {1.0, 0.0, 0.0},
  };
  split = split && !expanded->indices.has_value() &&
          expanded->layout == source->layout &&
          expanded->vertices.buffer != source->vertices.buffer &&
          expanded->subsets.identity() != source->subsets.identity() &&
          expanded->entity_runtime_key.value() !=
              source->entity_runtime_key.value() &&
          mesh::get_mesh_geometry_vertex_count(expanded) == 6.0;
  for (double vertex = 0.0; vertex < 6.0; vertex += 1.0) {
    for (double component = 0.0; component < 3.0; component += 1.0) {
      split = split && expanded->vertices.get_index(
                             vertex * 12.0 + component) ==
                             expected_positions[static_cast<std::size_t>(vertex)]
                                               [static_cast<std::size_t>(component)];
    }
  }
  mesh::compute_mesh_geometry_flat_normals(expanded, expanded);
  split = split && expanded->vertices.buffer == expanded_owner &&
          expanded->entity_runtime_key.value() == expanded_runtime &&
          expanded->version == 1.0 && !expanded->indices.has_value();
  for (double vertex = 0.0; vertex < 6.0; vertex += 1.0) {
    const double expected_x = 0.0;
    const double expected_y = vertex < 3.0 ? 0.0 : 1.0;
    const double expected_z = vertex < 3.0 ? 1.0 : 0.0;
    split = split &&
            near(expanded->vertices.get_index(vertex * 12.0 + 3.0),
                 expected_x) &&
            near(expanded->vertices.get_index(vertex * 12.0 + 4.0),
                 expected_y) &&
            near(expanded->vertices.get_index(vertex * 12.0 + 5.0),
                 expected_z);
  }
  for (double vertex = 0.0; vertex < 4.0; vertex += 1.0) {
    split = split && source->vertices.get_index(vertex * 12.0 + 3.0) == 0.0 &&
            source->vertices.get_index(vertex * 12.0 + 4.0) == 0.0 &&
            source->vertices.get_index(vertex * 12.0 + 5.0) == 0.0;
  }

  const auto degenerate = canonical_triangle();
  degenerate->vertices.set_index(24.0, 2.0);
  degenerate->vertices.set_index(25.0, 0.0);
  mesh::compute_mesh_geometry_flat_normals(degenerate, degenerate);
  for (double vertex = 0.0; vertex < 3.0; vertex += 1.0) {
    split = split && degenerate->vertices.get_index(vertex * 12.0 + 3.0) == 0.0 &&
            degenerate->vertices.get_index(vertex * 12.0 + 4.0) == 0.0 &&
            degenerate->vertices.get_index(vertex * 12.0 + 5.0) == 0.0;
  }
  if (inject_flat_split_remap_mutation) split = false;
  check(split,
        "flat normals follow authored triangle order with the last shared face winning, while explicit index expansion splits complete records in corner order so each face owns an isolated unit normal; all operations retain their destination owners and degenerate faces write zero");
}

void explicit_refresh_vs_cached_bounds() {
  const auto geometry = indexed_geometry();
  const auto runtime = geometry->entity_runtime_key.value();
  const auto first = mesh::ensure_mesh_geometry_bounds(geometry);
  const auto bounds_owner = first.value();
  const auto vertex_owner = geometry->vertices.buffer;
  geometry->vertices.set_index(0.0, -20.0);
  const auto cached = mesh::ensure_mesh_geometry_bounds(geometry);
  bool refresh = cached.has_value() && cached.value() == bounds_owner &&
                 cached.value()->min->x == -2.0 && geometry->version == 0.0 &&
                 runtime->bounds_version == 0.0;
  mesh::refresh_mesh_geometry_bounds(geometry);
  refresh = refresh && geometry->bounds.value() == bounds_owner &&
            geometry->bounds.value()->min->x == -20.0 &&
            runtime->bounds_version == geometry->version &&
            geometry->vertices.buffer == vertex_owner;
  geometry->vertices.set_index(3.0, 40.0);
  const auto still_cached = mesh::ensure_mesh_geometry_bounds(geometry);
  refresh = refresh && still_cached.has_value() &&
            still_cached.value()->max->x == 4.0;
  mesh::refresh_mesh_geometry_bounds(geometry);
  refresh = refresh && geometry->bounds.value() == bounds_owner &&
            geometry->bounds.value()->max->x == 40.0 &&
            runtime->bounds_version == 0.0 && geometry->version == 0.0;

  geometry->vertices.set_index(6.0, -70.0);
  mesh::invalidate_mesh_geometry(geometry);
  const auto invalidated = mesh::ensure_mesh_geometry_bounds(geometry);
  refresh = refresh && invalidated.has_value() &&
            invalidated.value() == bounds_owner &&
            invalidated.value()->min->x == -70.0 &&
            runtime->bounds_version == geometry->version;

  const auto empty =
      geometry_from(position_layout(), flight::Float32Array(0.0));
  const auto empty_runtime = empty->entity_runtime_key.value();
  mesh::refresh_mesh_geometry_bounds(empty);
  const auto empty_owner = empty->bounds.value();
  const auto empty_result = mesh::ensure_mesh_geometry_bounds(empty);
  refresh = refresh && !empty_result.has_value() &&
            empty->bounds.value() == empty_owner &&
            empty->bounds.value()->min->x ==
                std::numeric_limits<double>::infinity() &&
            empty->bounds.value()->max->x ==
                -std::numeric_limits<double>::infinity() &&
            empty_runtime->bounds_version == empty->version;

  auto detached = flight::make_ref<types::MeshGeometry>();
  detached->bounds = std::nullopt;
  detached->entity_runtime_key = std::nullopt;
  detached->indices = std::nullopt;
  detached->layout = position_layout();
  detached->subsets = flight::Array<flight::Ref<types::MeshSubset>>{};
  detached->topology = flight::String("point-list");
  detached->version = 7.0;
  detached->vertices = flight::Float32Array{3.0F, 4.0F, 5.0F};
  mesh::refresh_mesh_geometry_bounds(detached);
  refresh = refresh && detached->bounds.has_value() &&
            detached->bounds.value()->min->x == 3.0 &&
            detached->bounds.value()->max->z == 5.0 &&
            !mesh::get_mesh_geometry_runtime_if_present(detached).has_value() &&
            detached->version == 7.0;
  if (inject_refresh_invalidation_mutation) refresh = false;
  check(refresh,
        "ensure honors a matching cache stamp after direct writes, explicit refresh bypasses that cache and reuses the AABB owner, invalidation makes ensure refresh, and empty or runtime-less owners receive exact bounds without synthetic runtime state");
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

void wireframe_edge_order_width_and_duplication() {
  const auto layout = position_layout();
  flight::Float32Array vertices{
      0.0F, 0.0F, 0.0F,
      1.0F, 0.0F, 0.0F,
      0.0F, 1.0F, 0.0F,
      1.0F, 1.0F, 0.0F,
      2.0F, 0.0F, 0.0F,
      2.0F, 1.0F, 0.0F,
  };
  auto make_indexed = [&](auto indices, const char* topology) {
    auto options = flight::make_ref<types::MeshGeometryOptions>();
    options->indices = flight::SequenceView<double>(indices);
    options->layout = layout;
    options->topology = flight::String(topology);
    options->vertices = vertices;
    return mesh::create_mesh_geometry(options);
  };

  flight::Uint16Array quad_indices{0, 1, 2, 2, 1, 3, 5};
  const auto quad = make_indexed(quad_indices, "triangle-list");
  const auto quad_owner = index_buffer_identity(quad);
  const auto narrow = mesh::compute_mesh_geometry_wireframe_indices(quad);
  const double expected_quad[] = {
      0.0, 1.0, 1.0, 2.0, 2.0, 0.0,
      2.0, 1.0, 1.0, 3.0, 3.0, 2.0,
  };
  bool edges = std::holds_alternative<flight::Uint16Array>(narrow) &&
               std::get<flight::Uint16Array>(narrow).size() == 12 &&
               index_buffer_identity(quad) == quad_owner &&
               index_at(quad, 6.0) == 5.0;
  for (double index = 0.0; index < 12.0; index += 1.0) {
    edges = edges && variant_index_at(narrow, index) ==
                         expected_quad[static_cast<std::size_t>(index)];
  }
  edges = edges && variant_index_at(narrow, 2.0) == 1.0 &&
          variant_index_at(narrow, 3.0) == 2.0 &&
          variant_index_at(narrow, 6.0) == 2.0 &&
          variant_index_at(narrow, 7.0) == 1.0;

  flight::Uint32Array wide_indices{0, 1, 2, 2, 1, 3};
  const auto wide = mesh::compute_mesh_geometry_wireframe_indices(
      make_indexed(wide_indices, "triangle-list"));
  edges = edges && std::holds_alternative<flight::Uint32Array>(wide) &&
          std::get<flight::Uint32Array>(wide).size() == 12;
  for (double index = 0.0; index < 12.0; index += 1.0) {
    edges = edges && variant_index_at(wide, index) ==
                         expected_quad[static_cast<std::size_t>(index)];
  }

  flight::Uint16Array strip_indices{0, 1, 2, 3};
  const auto strip = mesh::compute_mesh_geometry_wireframe_indices(
      make_indexed(strip_indices, "triangle-strip"));
  const double expected_strip[] = {
      0.0, 1.0, 1.0, 2.0, 2.0, 0.0,
      1.0, 2.0, 2.0, 3.0, 3.0, 1.0,
  };
  edges = edges && std::holds_alternative<flight::Uint16Array>(strip) &&
          std::get<flight::Uint16Array>(strip).size() == 12;
  for (double index = 0.0; index < 12.0; index += 1.0) {
    edges = edges && variant_index_at(strip, index) ==
                         expected_strip[static_cast<std::size_t>(index)];
  }

  auto sequential_options = flight::make_ref<types::MeshGeometryOptions>();
  sequential_options->layout = layout;
  sequential_options->topology = flight::String("triangle-list");
  sequential_options->vertices = vertices;
  const auto sequential = mesh::create_mesh_geometry(sequential_options);
  const auto sequential_edges =
      mesh::compute_mesh_geometry_wireframe_indices(sequential);
  const double expected_sequential[] = {
      0.0, 1.0, 1.0, 2.0, 2.0, 0.0,
      3.0, 4.0, 4.0, 5.0, 5.0, 3.0,
  };
  edges = edges && std::holds_alternative<flight::Uint32Array>(sequential_edges) &&
          std::get<flight::Uint32Array>(sequential_edges).size() == 12 &&
          !sequential->indices.has_value();
  for (double index = 0.0; index < 12.0; index += 1.0) {
    edges = edges && variant_index_at(sequential_edges, index) ==
                         expected_sequential[static_cast<std::size_t>(index)];
  }

  flight::Uint16Array invalid_indices{0, 99, 2};
  const auto invalid = mesh::compute_mesh_geometry_wireframe_indices(
      make_indexed(invalid_indices, "triangle-list"));
  const double expected_invalid[] = {0.0, 99.0, 99.0, 2.0, 2.0, 0.0};
  for (double index = 0.0; index < 6.0; index += 1.0) {
    edges = edges && variant_index_at(invalid, index) ==
                         expected_invalid[static_cast<std::size_t>(index)];
  }

  const auto point_indexed =
      make_indexed(flight::Uint16Array{0, 1}, "point-list");
  const auto empty_narrow =
      mesh::compute_mesh_geometry_wireframe_indices(point_indexed);
  auto point_options = flight::make_ref<types::MeshGeometryOptions>();
  point_options->layout = layout;
  point_options->topology = flight::String("point-list");
  point_options->vertices = flight::Float32Array{0.0F, 0.0F, 0.0F};
  const auto empty_wide = mesh::compute_mesh_geometry_wireframe_indices(
      mesh::create_mesh_geometry(point_options));
  edges = edges && std::holds_alternative<flight::Uint16Array>(empty_narrow) &&
          std::get<flight::Uint16Array>(empty_narrow).empty() &&
          std::holds_alternative<flight::Uint32Array>(empty_wide) &&
          std::get<flight::Uint32Array>(empty_wide).empty();
  if (inject_wireframe_edge_mutation) edges = false;
  check(edges,
        "wireframe extraction emits every directed face edge in authored list or sliding-strip order, deliberately retains shared edges instead of deduplicating them, ignores incomplete trailing triangles, mirrors the source index width (or Uint32 for sequential input), and transports invalid index values without mutating the source");
}

void compaction_index_promotion_and_invalid_domains() {
  const auto layout = position_layout();
  flight::Float32Array small_vertices{
      10.0F, 0.0F, 0.0F,
      20.0F, 0.0F, 0.0F,
      30.0F, 0.0F, 0.0F,
      40.0F, 0.0F, 0.0F,
  };
  flight::Uint32Array small_indices{3, 1, 3, 2};
  auto small_options = flight::make_ref<types::MeshGeometryOptions>();
  small_options->indices = flight::SequenceView<double>(small_indices);
  small_options->layout = layout;
  small_options->topology = flight::String("line-list");
  small_options->vertices = small_vertices;
  const auto small = mesh::create_mesh_geometry(small_options);
  const auto compact = mesh::compact_mesh_geometry_vertices(small);
  const double expected_indices[] = {0.0, 1.0, 0.0, 2.0};
  const double expected_positions[] = {40.0, 20.0, 30.0};
  bool domains = compact->indices.has_value() &&
                 std::holds_alternative<flight::Uint16Array>(
                     compact->indices.value()) &&
                 mesh::get_mesh_geometry_vertex_count(compact) == 3.0 &&
                 compact->vertices.buffer != small->vertices.buffer &&
                 index_buffer_identity(compact) != index_buffer_identity(small) &&
                 compact->subsets.identity() != small->subsets.identity() &&
                 std::holds_alternative<flight::Uint32Array>(
                     small->indices.value());
  for (double index = 0.0; index < 4.0; index += 1.0) {
    domains = domains && index_at(compact, index) ==
                           expected_indices[static_cast<std::size_t>(index)] &&
              index_at(small, index) == small_indices.get_index(index);
  }
  for (double vertex = 0.0; vertex < 3.0; vertex += 1.0) {
    domains = domains && compact->vertices.get_index(vertex * 3.0) ==
                           expected_positions[static_cast<std::size_t>(vertex)];
  }

  auto invalid_clone = [&](const flight::Ref<types::MeshGeometry>& source) {
    const auto result = mesh::compact_mesh_geometry_vertices(source);
    bool cloned = result != source && result->layout == source->layout &&
                  result->vertices.buffer != source->vertices.buffer &&
                  result->vertices.byte_length == source->vertices.byte_length &&
                  result->subsets.identity() != source->subsets.identity() &&
                  result->topology == source->topology &&
                  result->version == source->version &&
                  result->indices.has_value() == source->indices.has_value();
    const flight::Uint8Array source_bytes(
        source->vertices.buffer,
        static_cast<double>(source->vertices.byte_offset),
        static_cast<double>(source->vertices.byte_length));
    const flight::Uint8Array result_bytes(
        result->vertices.buffer,
        static_cast<double>(result->vertices.byte_offset),
        static_cast<double>(result->vertices.byte_length));
    for (double byte = 0.0;
         byte < static_cast<double>(source->vertices.byte_length);
         byte += 1.0) {
      cloned = cloned &&
               result_bytes.get_index(byte) == source_bytes.get_index(byte);
    }
    if (source->indices.has_value()) {
      cloned = cloned &&
               index_buffer_identity(result) != index_buffer_identity(source) &&
               result->indices.value().index() == source->indices.value().index() &&
               std::visit([](const auto& values) { return values.size(); },
                          result->indices.value()) ==
                   std::visit([](const auto& values) { return values.size(); },
                              source->indices.value());
      const double count = std::visit(
          [](const auto& values) { return static_cast<double>(values.size()); },
          source->indices.value());
      for (double index = 0.0; index < count; index += 1.0) {
        cloned = cloned && index_at(result, index) == index_at(source, index);
      }
    }
    return cloned;
  };

  const auto nonindexed = geometry_from(layout, small_vertices);
  domains = domains && invalid_clone(nonindexed) &&
            !mesh::compact_mesh_geometry_vertices(nonindexed)
                 ->indices.has_value();

  auto indexed_with_layout = [&](double stride,
                                 const flight::Float32Array& values,
                                 const flight::Uint16Array& indices) {
    auto options = flight::make_ref<types::MeshGeometryOptions>();
    options->indices = flight::SequenceView<double>(indices);
    options->layout = position_layout(stride);
    options->vertices = values;
    return mesh::create_mesh_geometry(options);
  };
  const auto zero_stride = indexed_with_layout(
      0.0, flight::Float32Array{1.0F, 2.0F, 3.0F},
      flight::Uint16Array{0});
  const auto misaligned_stride = indexed_with_layout(
      14.0, flight::Float32Array(7.0), flight::Uint16Array{0});
  const auto partial_record = indexed_with_layout(
      12.0, flight::Float32Array{1.0F, 2.0F, 3.0F, 99.0F},
      flight::Uint16Array{0});
  const auto out_of_range = indexed_with_layout(
      12.0, flight::Float32Array{1.0F, 2.0F, 3.0F},
      flight::Uint16Array{1});
  domains = domains && invalid_clone(zero_stride) &&
            invalid_clone(misaligned_stride) &&
            invalid_clone(partial_record) && invalid_clone(out_of_range) &&
            index_at(out_of_range, 0.0) == 1.0;

  const auto partial_indexed = mesh::index_mesh_geometry_vertices(
      geometry_from(layout,
                    flight::Float32Array{1.0F, 2.0F, 3.0F, 99.0F}));
  const auto zero_indexed = mesh::index_mesh_geometry_vertices(
      geometry_from(position_layout(0.0),
                    flight::Float32Array{1.0F, 2.0F, 3.0F}));
  domains = domains && partial_indexed->indices.has_value() &&
            std::holds_alternative<flight::Uint16Array>(
                partial_indexed->indices.value()) &&
            std::get<flight::Uint16Array>(partial_indexed->indices.value())
                    .size() == 1 &&
            index_at(partial_indexed, 0.0) == 0.0 &&
            zero_indexed->indices.has_value() &&
            std::holds_alternative<flight::Uint16Array>(
                zero_indexed->indices.value()) &&
            std::get<flight::Uint16Array>(zero_indexed->indices.value())
                .empty();

  const auto already_indexed = mesh::index_mesh_geometry_vertices(small);
  domains = domains && already_indexed != small &&
            already_indexed->vertices.buffer != small->vertices.buffer &&
            index_buffer_identity(already_indexed) != index_buffer_identity(small) &&
            std::holds_alternative<flight::Uint32Array>(
                already_indexed->indices.value());
  for (double index = 0.0; index < 4.0; index += 1.0) {
    domains = domains && index_at(already_indexed, index) ==
                           small_indices.get_index(index);
  }

  constexpr double wide_count = 65536.0;
  flight::Float32Array wide_vertices(wide_count * 3.0);
  flight::Uint32Array wide_indices(wide_count);
  for (double vertex = 0.0; vertex < wide_count; vertex += 1.0) {
    wide_vertices.set_index(vertex * 3.0, vertex);
    wide_indices.set_index(vertex, vertex);
  }
  auto wide_options = flight::make_ref<types::MeshGeometryOptions>();
  wide_options->indices = flight::SequenceView<double>(wide_indices);
  wide_options->layout = layout;
  wide_options->topology = flight::String("point-list");
  wide_options->vertices = wide_vertices;
  const auto wide_source = mesh::create_mesh_geometry(wide_options);
  const auto wide_compact = mesh::compact_mesh_geometry_vertices(wide_source);
  domains = domains && wide_compact->indices.has_value() &&
            std::holds_alternative<flight::Uint32Array>(
                wide_compact->indices.value()) &&
            mesh::get_mesh_geometry_vertex_count(wide_compact) == wide_count &&
            index_at(wide_compact, 0.0) == 0.0 &&
            index_at(wide_compact, wide_count - 1.0) == wide_count - 1.0 &&
            wide_compact->vertices.get_index((wide_count - 1.0) * 3.0) ==
                wide_count - 1.0 &&
            index_at(wide_source, wide_count - 1.0) == wide_count - 1.0;
  if (inject_index_promotion_mutation) domains = false;
  check(domains,
        "compaction assigns first-reference-stable records, down-converts a wide source from compact cardinality, promotes only beyond 65,535 unique vertices, and deep-clones nonindexed, invalid-stride, partial-record, or out-of-range domains; indexing floors partial records, yields empty narrow indices for zero stride, and preserves an authored wide index representation by deep clone");
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

void merge_metadata_groups_and_empty_semantics() {
  const auto first_layout = position_layout();
  flight::Float32Array first_vertices{
      0.0F, 0.0F, 0.0F,
      1.0F, 0.0F, 0.0F,
      2.0F, 0.0F, 0.0F,
  };
  flight::Uint16Array first_indices{2, 0, 0, 1};
  auto first_options = flight::make_ref<types::MeshGeometryOptions>();
  first_options->indices = flight::SequenceView<double>(first_indices);
  first_options->layout = first_layout;
  first_options->subsets =
      flight::Array<flight::Ref<types::MeshSubset>>{
          mesh_subset(2.0, 0.0), mesh_subset(2.0, 2.0)};
  first_options->topology = flight::String("line-list");
  first_options->vertices = first_vertices;
  const auto first = mesh::create_mesh_geometry(first_options);
  mesh::ensure_mesh_geometry_bounds(first);
  first->version = 7.0;

  auto empty_options = flight::make_ref<types::MeshGeometryOptions>();
  empty_options->layout = position_layout();
  empty_options->topology = flight::String("line-list");
  empty_options->vertices = flight::Float32Array(0.0);
  const auto empty = mesh::create_mesh_geometry(empty_options);

  auto third_options = flight::make_ref<types::MeshGeometryOptions>();
  third_options->layout = position_layout();
  third_options->topology = flight::String("line-list");
  third_options->vertices = flight::Float32Array{
      0.0F, 0.0F, 0.0F,
      11.0F, 0.0F, 0.0F,
  };
  const auto third = mesh::create_mesh_geometry(third_options);
  const auto first_vertex_owner = first->vertices.buffer;
  const void* first_index_owner = index_buffer_identity(first);
  const auto first_subset_owner = first->subsets.identity();
  const auto empty_vertex_owner = empty->vertices.buffer;
  const auto third_vertex_owner = third->vertices.buffer;
  const auto merged_optional = mesh::merge_mesh_geometries(
      flight::Array<flight::Ref<types::MeshGeometry>>{first, empty, third});
  bool groups = merged_optional.has_value();
  const auto merged = merged_optional.value();
  const double expected_indices[] = {2.0, 0.0, 0.0, 1.0, 3.0, 4.0};
  groups = groups && merged->layout == first_layout &&
           merged->topology == flight::String("line-list") &&
           mesh::get_mesh_geometry_vertex_count(merged) == 5.0 &&
           mesh::get_mesh_geometry_index_count(merged) == 6.0 &&
           std::holds_alternative<flight::Uint16Array>(
               merged->indices.value()) &&
           merged->subsets.size() == 4 &&
           merged->subsets.element(0.0)->index_count == 2.0 &&
           merged->subsets.element(0.0)->index_offset == 0.0 &&
           merged->subsets.element(1.0)->index_count == 2.0 &&
           merged->subsets.element(1.0)->index_offset == 2.0 &&
           merged->subsets.element(2.0)->index_count == 0.0 &&
           merged->subsets.element(2.0)->index_offset == 4.0 &&
           merged->subsets.element(3.0)->index_count == 2.0 &&
           merged->subsets.element(3.0)->index_offset == 4.0 &&
           merged->subsets.identity() != first->subsets.identity() &&
           merged->subsets.element(0.0) != first->subsets.element(0.0) &&
           merged->version == 0.0 && merged->bounds.has_value() &&
           merged->bounds.value()->min->x == 0.0 &&
           merged->bounds.value()->max->x == 11.0 &&
           has_pristine_mesh_runtime(
               merged->entity_runtime_key.value(), 0.0);
  for (double index = 0.0; index < 6.0; index += 1.0) {
    groups = groups && index_at(merged, index) ==
                           expected_indices[static_cast<std::size_t>(index)];
  }
  const auto position_groups =
      mesh::compute_mesh_geometry_position_groups(merged);
  const double expected_groups[] = {0.0, 1.0, 2.0, 0.0, 4.0};
  for (double vertex = 0.0; vertex < 5.0; vertex += 1.0) {
    groups = groups && position_groups.get_index(vertex) ==
                           expected_groups[static_cast<std::size_t>(vertex)];
  }
  groups = groups && first->vertices.buffer == first_vertex_owner &&
           index_buffer_identity(first) == first_index_owner &&
           first->subsets.identity() == first_subset_owner &&
           first->version == 7.0 && empty->vertices.buffer == empty_vertex_owner &&
           third->vertices.buffer == third_vertex_owner;

  const auto ungrouped_first = mesh::clone_mesh_geometry(first);
  const auto ungrouped_third = mesh::clone_mesh_geometry(third);
  ungrouped_first->subsets = {};
  ungrouped_third->subsets = {};
  const auto fallback_optional = mesh::merge_mesh_geometries(
      flight::Array<flight::Ref<types::MeshGeometry>>{
          ungrouped_first, ungrouped_third});
  groups = groups && fallback_optional.has_value() &&
           fallback_optional.value()->subsets.size() == 1 &&
           fallback_optional.value()->subsets.element(0.0)->index_count == 6.0 &&
           fallback_optional.value()->subsets.element(0.0)->index_offset == 0.0;

  const auto merged_empty_optional = mesh::merge_mesh_geometries(
      flight::Array<flight::Ref<types::MeshGeometry>>{empty});
  groups = groups && merged_empty_optional.has_value() &&
           !merged_empty_optional.value()->indices.has_value() &&
           merged_empty_optional.value()->subsets.size() == 1 &&
           merged_empty_optional.value()->subsets.element(0.0)->index_count ==
               0.0 &&
           !mesh::ensure_mesh_geometry_bounds(
                merged_empty_optional.value()).has_value();

  flight::Uint32Array empty_indices(0.0);
  auto indexed_empty_options = flight::make_ref<types::MeshGeometryOptions>();
  indexed_empty_options->indices =
      flight::SequenceView<double>(empty_indices);
  indexed_empty_options->layout = position_layout();
  indexed_empty_options->topology = flight::String("line-list");
  indexed_empty_options->vertices = flight::Float32Array(0.0);
  const auto indexed_empty =
      mesh::create_mesh_geometry(indexed_empty_options);
  const auto merged_indexed_empty = mesh::merge_mesh_geometries(
      flight::Array<flight::Ref<types::MeshGeometry>>{indexed_empty});
  groups = groups && merged_indexed_empty.has_value() &&
           merged_indexed_empty.value()->indices.has_value() &&
           std::holds_alternative<flight::Uint16Array>(
               merged_indexed_empty.value()->indices.value()) &&
           std::get<flight::Uint16Array>(
               merged_indexed_empty.value()->indices.value()).empty();
  if (inject_metadata_groups_mutation) groups = false;
  check(groups,
        "merge preserves topology, first-layout identity, exact position groups, every source subset as an ordered rebased material group (including empty groups), fresh runtime/bounds ownership, and fallback grouping; empty nonindexed input stays nonindexed while indexed-empty input remains explicitly indexed");
}

void operation_order_across_topologies_and_sources() {
  const auto layout = position_layout();
  flight::Float32Array strip_vertices(15.0);
  flight::Uint16Array strip_indices{4, 2, 0, 3, 1};
  auto indexed_options = flight::make_ref<types::MeshGeometryOptions>();
  indexed_options->indices = flight::SequenceView<double>(strip_indices);
  indexed_options->layout = layout;
  indexed_options->topology = flight::String("triangle-strip");
  indexed_options->vertices = strip_vertices;
  const auto indexed = mesh::create_mesh_geometry(indexed_options);
  auto triangle = flight::make_ref<types::MeshTriangleVertexIndices>();
  const double expected_indexed[][3] = {
      {4.0, 2.0, 0.0},
      {0.0, 2.0, 3.0},
      {0.0, 3.0, 1.0},
  };
  bool ordered = mesh::get_mesh_geometry_triangle_count(indexed) == 3.0;
  for (double face = 0.0; face < 3.0; face += 1.0) {
    ordered = ordered && mesh::get_mesh_geometry_triangle_vertex_indices(
                             triangle, indexed, face) &&
              triangle->i0 == expected_indexed[static_cast<std::size_t>(face)][0] &&
              triangle->i1 == expected_indexed[static_cast<std::size_t>(face)][1] &&
              triangle->i2 == expected_indexed[static_cast<std::size_t>(face)][2];
  }

  auto sequential_options = flight::make_ref<types::MeshGeometryOptions>();
  sequential_options->layout = position_layout();
  sequential_options->topology = flight::String("triangle-strip");
  sequential_options->vertices = flight::Float32Array(15.0);
  const auto sequential = mesh::create_mesh_geometry(sequential_options);
  const double expected_sequential[][3] = {
      {0.0, 1.0, 2.0},
      {2.0, 1.0, 3.0},
      {2.0, 3.0, 4.0},
  };
  for (double face = 0.0; face < 3.0; face += 1.0) {
    ordered = ordered && mesh::get_mesh_geometry_triangle_vertex_indices(
                             triangle, sequential, face) &&
              triangle->i0 ==
                  expected_sequential[static_cast<std::size_t>(face)][0] &&
              triangle->i1 ==
                  expected_sequential[static_cast<std::size_t>(face)][1] &&
              triangle->i2 ==
                  expected_sequential[static_cast<std::size_t>(face)][2];
  }

  const auto subsets = indexed_geometry();
  flight::Array<flight::Ref<types::MeshSubset>> authored{
      mesh_subset(6.0, 0.0), mesh_subset(3.0, 3.0)};
  mesh::set_mesh_geometry_subsets(subsets, authored);
  const auto appended = mesh_subset(3.0, 0.0);
  mesh::add_mesh_geometry_subset(subsets, appended);
  ordered = ordered && subsets->subsets.size() == 3 &&
            subsets->subsets.element(0.0)->index_count == 6.0 &&
            subsets->subsets.element(1.0)->index_offset == 3.0 &&
            subsets->subsets.element(2.0)->index_offset == 0.0 &&
            mesh::get_mesh_geometry_triangle_subset_index(subsets, 1.0) == 0.0;
  mesh::set_mesh_geometry_subsets(
      subsets, flight::Array<flight::Ref<types::MeshSubset>>{
                   mesh_subset(3.0, 3.0), mesh_subset(6.0, 0.0)});
  ordered = ordered && subsets->subsets.element(0.0)->index_offset == 3.0 &&
            subsets->subsets.element(1.0)->index_offset == 0.0 &&
            mesh::get_mesh_geometry_triangle_subset_index(subsets, 0.0) == 1.0 &&
            mesh::get_mesh_geometry_triangle_subset_index(subsets, 1.0) == 0.0;

  auto point = [&](float x) {
    auto options = flight::make_ref<types::MeshGeometryOptions>();
    options->layout = position_layout();
    options->topology = flight::String("point-list");
    options->vertices = flight::Float32Array{x, 0.0F, 0.0F};
    return mesh::create_mesh_geometry(options);
  };
  const auto thirty = point(30.0F);
  const auto ten = point(10.0F);
  const auto twenty = point(20.0F);
  const auto merged = mesh::merge_mesh_geometries(
      flight::Array<flight::Ref<types::MeshGeometry>>{
          thirty, ten, twenty});
  ordered = ordered && merged.has_value() &&
            merged.value()->vertices.get_index(0.0) == 30.0 &&
            merged.value()->vertices.get_index(3.0) == 10.0 &&
            merged.value()->vertices.get_index(6.0) == 20.0 &&
            index_at(merged.value(), 0.0) == 0.0 &&
            index_at(merged.value(), 1.0) == 1.0 &&
            index_at(merged.value(), 2.0) == 2.0 &&
            merged.value()->subsets.size() == 3 &&
            merged.value()->subsets.element(0.0)->index_offset == 0.0 &&
            merged.value()->subsets.element(1.0)->index_offset == 1.0 &&
            merged.value()->subsets.element(2.0)->index_offset == 2.0 &&
            thirty->vertices.get_index(0.0) == 30.0 &&
            ten->vertices.get_index(0.0) == 10.0 &&
            twenty->vertices.get_index(0.0) == 20.0;
  if (inject_operation_order_mutation) ordered = false;
  check(ordered,
        "operation order is exact across indexed and sequential strip winding, subset replacement/append declaration order and first-owner overlap, and arbitrary merge source order with matching element and material-group order");
}

void operation_failure_atomicity_and_rollback() {
  struct Snapshot {
    flight::Ref<types::MeshGeometry> geometry;
    flight::ArrayBufferLike vertex_owner;
    const void* index_owner;
    const void* subset_owner;
    flight::Ref<types::VertexAttributeLayout> layout;
    std::optional<flight::Ref<types::Aabb>> bounds;
    std::optional<flight::Ref<types::MeshGeometryRuntime>> runtime;
    flight::String topology;
    double version;
    std::vector<double> vertices;
    std::vector<double> indices;
    std::vector<flight::Ref<types::MeshSubset>> subset_refs;
    std::vector<std::pair<double, double>> subset_values;
  };
  auto capture = [](const flight::Ref<types::MeshGeometry>& geometry) {
    Snapshot result{
        .geometry = geometry,
        .vertex_owner = geometry->vertices.buffer,
        .index_owner = index_buffer_identity(geometry),
        .subset_owner = geometry->subsets.identity(),
        .layout = geometry->layout,
        .bounds = geometry->bounds,
        .runtime = geometry->entity_runtime_key,
        .topology = geometry->topology,
        .version = geometry->version,
    };
    for (double index = 0.0;
         index < static_cast<double>(geometry->vertices.size());
         index += 1.0) {
      result.vertices.push_back(geometry->vertices.get_index(index));
    }
    if (geometry->indices.has_value()) {
      const double count = std::visit(
          [](const auto& values) { return static_cast<double>(values.size()); },
          geometry->indices.value());
      for (double index = 0.0; index < count; index += 1.0) {
        result.indices.push_back(index_at(geometry, index));
      }
    }
    for (const auto& subset : geometry->subsets) {
      result.subset_refs.push_back(subset);
      result.subset_values.emplace_back(
          subset->index_count, subset->index_offset);
    }
    return result;
  };
  auto unchanged = [](const Snapshot& before) {
    const auto& geometry = before.geometry;
    bool same = geometry->vertices.buffer == before.vertex_owner &&
                index_buffer_identity(geometry) == before.index_owner &&
                geometry->subsets.identity() == before.subset_owner &&
                geometry->layout == before.layout &&
                geometry->bounds == before.bounds &&
                geometry->entity_runtime_key == before.runtime &&
                geometry->topology == before.topology &&
                geometry->version == before.version &&
                geometry->vertices.size() == before.vertices.size() &&
                geometry->subsets.size() == before.subset_refs.size();
    for (double index = 0.0;
         index < static_cast<double>(before.vertices.size());
         index += 1.0) {
      same = same && geometry->vertices.get_index(index) ==
                         before.vertices[static_cast<std::size_t>(index)];
    }
    if (geometry->indices.has_value()) {
      const double count = std::visit(
          [](const auto& values) { return static_cast<double>(values.size()); },
          geometry->indices.value());
      same = same && count == static_cast<double>(before.indices.size());
      for (double index = 0.0; index < count; index += 1.0) {
        same = same && index_at(geometry, index) ==
                           before.indices[static_cast<std::size_t>(index)];
      }
    } else {
      same = same && before.indices.empty();
    }
    for (std::size_t index = 0; index < before.subset_refs.size(); ++index) {
      same = same && geometry->subsets[index] == before.subset_refs[index] &&
             geometry->subsets[index]->index_count ==
                 before.subset_values[index].first &&
             geometry->subsets[index]->index_offset ==
                 before.subset_values[index].second;
    }
    return same;
  };

  const auto first = indexed_geometry();
  const auto second = mesh::clone_mesh_geometry(first);
  const auto mismatched = geometry_from(
      position_layout(16.0), flight::Float32Array(12.0));
  mesh::ensure_mesh_geometry_bounds(first);
  mesh::ensure_mesh_geometry_bounds(second);
  mesh::ensure_mesh_geometry_bounds(mismatched);
  first->version = 3.0;
  second->version = 4.0;
  mismatched->version = 5.0;
  const auto first_before = capture(first);
  const auto second_before = capture(second);
  const auto mismatch_before = capture(mismatched);
  bool rollback = !mesh::merge_mesh_geometries(
                       flight::Array<flight::Ref<types::MeshGeometry>>{
                           first, second, mismatched}).has_value() &&
                  unchanged(first_before) && unchanged(second_before) &&
                  unchanged(mismatch_before);

  const auto topology_mismatch = mesh::clone_mesh_geometry(second);
  topology_mismatch->topology = flight::String("line-list");
  const auto topology_before = capture(topology_mismatch);
  rollback = rollback &&
             !mesh::merge_mesh_geometries(
                  flight::Array<flight::Ref<types::MeshGeometry>>{
                      first, second, topology_mismatch}).has_value() &&
             unchanged(first_before) && unchanged(second_before) &&
             unchanged(topology_before);

  flight::Array<double> positions{
      0.0, 0.0, 0.0,
      1.0, 0.0, 0.0,
      0.0, 1.0, 0.0,
  };
  flight::Array<double> normals{
      0.0, 0.0, 1.0,
      0.0, 0.0, 1.0,
      0.0, 0.0, 1.0,
  };
  flight::Array<double> short_uvs{0.0, 0.0, 1.0, 0.0, 0.0};
  flight::Uint16Array indices{0, 1, 2};
  auto options = flight::make_ref<types::MeshGeometryFromAttributesOptions>();
  options->indices = indices;
  options->normals = normals;
  options->positions = positions;
  options->uvs = short_uvs;
  const void* position_owner = positions.identity();
  const void* normal_owner = normals.identity();
  const void* uv_owner = short_uvs.identity();
  const auto input_index_owner = indices.buffer;
  rollback = rollback && throws_std_range_error([&] {
               static_cast<void>(mesh::create_mesh_geometry_from_attributes(
                   options));
             }) &&
             options->positions.identity() == position_owner &&
             std::get<flight::Array<double>>(options->normals).identity() ==
                 normal_owner &&
             std::get<flight::Array<double>>(options->uvs).identity() ==
                 uv_owner &&
             std::get<flight::Uint16Array>(options->indices).buffer ==
                 input_index_owner &&
             positions.element(8.0) == 0.0 && normals.element(8.0) == 1.0 &&
             short_uvs.element(4.0) == 0.0 && indices.get_index(2.0) == 2.0;

  auto triangle = flight::make_ref<types::MeshTriangleVertexIndices>(
      types::MeshTriangleVertexIndices{.i0 = 71.0, .i1 = 72.0, .i2 = 73.0});
  rollback = rollback &&
             !mesh::get_mesh_geometry_triangle_vertex_indices(
                  triangle, first, 99.0) &&
             triangle->i0 == 71.0 && triangle->i1 == 72.0 &&
             triangle->i2 == 73.0 && unchanged(first_before);
  if (inject_operation_rollback_mutation) rollback = false;
  check(rollback,
        "late layout or topology merge rejection rolls every earlier source back byte-for-byte with all owners and metadata intact, short attribute construction exposes no partial result or input write, and invalid triangle queries preserve their caller-owned output");
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

void vertex_format_component_table_and_storage_interpretation() {
  struct FormatCase {
    const char* format;
    double byte_length;
    double component_count;
    std::array<double, 4> input;
    std::array<double, 4> expected;
  };
  const std::array<FormatCase, 6> cases{{
      {"float32x2", 8.0, 2.0, {1.25, -2.5, 77.0, 88.0},
       {1.25, -2.5, 0.0, 0.0}},
      {"float32x3", 12.0, 3.0, {1.25, -2.5, 3.75, 88.0},
       {1.25, -2.5, 3.75, 0.0}},
      {"float32x4", 16.0, 4.0, {1.25, -2.5, 3.75, -4.5},
       {1.25, -2.5, 3.75, -4.5}},
      {"uint16x4", 8.0, 4.0, {-1.0, 3.4, 70000.0, 9.5},
       {0.0, 3.0, 65535.0, 10.0}},
      {"uint8x4", 4.0, 4.0, {-1.0, 3.4, 300.0, 9.5},
       {0.0, 3.0, 255.0, 10.0}},
      {"unorm8x4", 4.0, 4.0, {-0.1, 0.5, 1.2, 0.25},
       {0.0, 128.0 / 255.0, 1.0, 64.0 / 255.0}},
  }};

  bool table = true;
  for (const auto& entry : cases) {
    flight::Float32Array storage(8.0);
    flight::DataView view(storage.buffer);
    for (double byte = 0.0; byte < 32.0; byte += 1.0) {
      view.set_uint8(byte, 165.0);
    }
    const double base = 3.0;
    const flight::String format(entry.format);
    table = table && mesh::get_vertex_format_byte_length(format) ==
                         entry.byte_length &&
            mesh::get_vertex_format_component_count(format) ==
                entry.component_count;
    for (double component = 0.0; component < entry.component_count;
         component += 1.0) {
      const auto index = static_cast<std::size_t>(component);
      mesh::write_vertex_format_component(
          view, base, format, component, entry.input[index]);
      table = table &&
              near(mesh::read_vertex_format_component(
                       view, base, format, component),
                   entry.expected[index]);
    }
    for (double byte = 0.0; byte < base; byte += 1.0) {
      table = table && view.get_uint8(byte) == 165.0;
    }
    for (double byte = base + entry.byte_length; byte < 32.0;
         byte += 1.0) {
      table = table && view.get_uint8(byte) == 165.0;
    }
    if (format == flight::String("float32x2")) {
      table = table && view.get_uint8(base) == 0.0 &&
              view.get_uint8(base + 1.0) == 0.0 &&
              view.get_uint8(base + 2.0) == 160.0 &&
              view.get_uint8(base + 3.0) == 63.0;
    } else if (format == flight::String("uint16x4")) {
      const double expected_bytes[] = {0.0, 0.0, 3.0, 0.0,
                                       255.0, 255.0, 10.0, 0.0};
      for (double byte = 0.0; byte < 8.0; byte += 1.0) {
        table = table && view.get_uint8(base + byte) ==
                             expected_bytes[static_cast<std::size_t>(byte)];
      }
    } else if (format == flight::String("uint8x4")) {
      table = table && view.get_uint8(base) == 0.0 &&
              view.get_uint8(base + 1.0) == 3.0 &&
              view.get_uint8(base + 2.0) == 255.0 &&
              view.get_uint8(base + 3.0) == 10.0;
    } else if (format == flight::String("unorm8x4")) {
      table = table && view.get_uint8(base) == 0.0 &&
              view.get_uint8(base + 1.0) == 128.0 &&
              view.get_uint8(base + 2.0) == 255.0 &&
              view.get_uint8(base + 3.0) == 64.0;
    }
  }
  if (inject_format_component_mutation) table = false;
  check(table,
        "the complete six-format table reports exact byte/component counts and reads each component from its emitted little-endian float, clamped integer, or normalized representation without touching adjacent bytes");
}

void accessor_offset_stride_interleaving_and_format_round_trip() {
  struct FormatRow {
    const char* semantic;
    const char* format;
    double byte_offset;
    double component_count;
  };
  const std::array<FormatRow, 6> rows{{
      {"format-f2", "float32x2", 0.0, 2.0},
      {"format-f3", "float32x3", 8.0, 3.0},
      {"format-f4", "float32x4", 20.0, 4.0},
      {"format-u16", "uint16x4", 37.0, 4.0},
      {"format-u8", "uint8x4", 45.0, 4.0},
      {"format-unorm", "unorm8x4", 49.0, 4.0},
  }};
  flight::Array<flight::Ref<types::VertexAttribute>> source_attributes;
  const std::array<std::size_t, 6> source_order{{4, 1, 5, 0, 3, 2}};
  for (const auto index : source_order) {
    const auto& row = rows[index];
    source_attributes.push(
        vertex_attribute(row.semantic, row.format, row.byte_offset));
  }
  const auto source_layout =
      vertex_layout(std::move(source_attributes), 56.0);
  flight::Float32Array backing(36.0);
  flight::DataView backing_view(backing.buffer);
  backing_view.set_uint8(0.0, 173.0);
  backing_view.set_uint8(140.0, 174.0);
  flight::Float32Array vertices(backing.buffer, 8.0, 28.0);
  flight::DataView source_view(
      vertices.buffer, static_cast<double>(vertices.byte_offset),
      static_cast<double>(vertices.byte_length));
  for (double vertex = 0.0; vertex < 2.0; vertex += 1.0) {
    for (std::size_t row_index = 0; row_index < rows.size(); ++row_index) {
      const auto& row = rows[row_index];
      for (double component = 0.0; component < row.component_count;
           component += 1.0) {
        double value = vertex * 100.0 +
                       static_cast<double>(row_index) * 10.0 + component +
                       0.25;
        if (flight::String(row.format) == flight::String("unorm8x4")) {
          value = (component + vertex + 1.0) / 8.0;
        }
        mesh::write_vertex_format_component(
            source_view, vertex * 56.0 + row.byte_offset,
            flight::String(row.format), component, value);
      }
    }
  }
  const auto source = geometry_from(source_layout, vertices);
  flight::Array<flight::Ref<types::VertexAttribute>> target_attributes;
  for (std::size_t reverse = rows.size(); reverse > 0; --reverse) {
    const std::size_t row_index = reverse - 1;
    target_attributes.push(vertex_attribute(
        rows[row_index].semantic, "float32x4",
        static_cast<double>(row_index) * 16.0));
  }
  const auto target_layout =
      vertex_layout(std::move(target_attributes), 96.0);
  const auto converted =
      mesh::convert_mesh_geometry_layout(source, target_layout);
  const auto round_trip =
      mesh::convert_mesh_geometry_layout(converted, source_layout);
  flight::DataView round_trip_view(round_trip->vertices.buffer);
  bool interleaved = source->vertices.buffer == vertices.buffer &&
                     source->vertices.byte_offset == 8 &&
                     converted->layout == target_layout &&
                     converted->vertices.buffer != source->vertices.buffer &&
                     round_trip->layout == source_layout &&
                     round_trip->vertices.buffer != converted->vertices.buffer &&
                     backing_view.get_uint8(0.0) == 173.0 &&
                     backing_view.get_uint8(140.0) == 174.0;
  for (double vertex = 0.0; vertex < 2.0; vertex += 1.0) {
    for (std::size_t row_index = 0; row_index < rows.size(); ++row_index) {
      const auto& row = rows[row_index];
      for (double component = 0.0; component < 4.0; component += 1.0) {
        const double converted_value = converted->vertices.get_index(
            vertex * 24.0 + static_cast<double>(row_index) * 4.0 +
            component);
        if (component < row.component_count) {
          const double source_value = mesh::read_vertex_format_component(
              source_view, vertex * 56.0 + row.byte_offset,
              flight::String(row.format), component);
          const double round_trip_value = mesh::read_vertex_format_component(
              round_trip_view, vertex * 56.0 + row.byte_offset,
              flight::String(row.format), component);
          interleaved = interleaved && near(converted_value, source_value) &&
                        near(round_trip_value, source_value);
        } else {
          interleaved = interleaved && converted_value == 0.0;
        }
      }
    }
  }
  converted->vertices.set_index(0.0, -999.0);
  interleaved = interleaved &&
                near(mesh::read_vertex_format_component(
                         source_view, rows[0].byte_offset,
                         flight::String(rows[0].format), 0.0),
                     0.25);
  if (inject_accessor_offset_stride_mutation) interleaved = false;
  check(interleaved,
        "a systematic two-record interleaved layout resolves every format across non-zero view, stride, padding, declaration-order, and unaligned packed offsets, converts by semantic to float4, zero-fills missing components, and round-trips without aliasing the source");
}

void accessor_normalized_integer_and_float_matrix() {
  struct AccessorCase {
    const char* semantic;
    const char* format;
    std::array<double, 4> input;
    std::array<double, 4> expected;
  };
  const std::array<double, 4> float_input{{1.25, -2.5, 3.75, -4.5}};
  const std::array<double, 4> float_expected = float_input;
  const std::array<double, 4> uint16_input{{-1.0, 3.4, 70000.0, 9.5}};
  const std::array<double, 4> uint16_expected{{0.0, 3.0, 65535.0, 10.0}};
  const std::array<double, 4> uint8_input{{-1.0, 3.4, 300.0, 9.5}};
  const std::array<double, 4> uint8_expected{{0.0, 3.0, 255.0, 10.0}};
  const std::array<double, 4> unorm_input{{-0.1, 0.5, 1.2, 0.25}};
  const std::array<double, 4> unorm_expected{{
      0.0, 128.0 / 255.0, 1.0, 64.0 / 255.0}};
  const std::array<AccessorCase, 13> cases{{
      {"color0", "float32x3", float_input,
       {float_expected[0], float_expected[1], float_expected[2], 1.0}},
      {"color0", "float32x4", float_input, float_expected},
      {"color0", "uint16x4", uint16_input, uint16_expected},
      {"color0", "uint8x4", uint8_input, uint8_expected},
      {"color0", "unorm8x4", unorm_input, unorm_expected},
      {"joints0", "float32x4", float_input, float_expected},
      {"joints0", "uint16x4", uint16_input, uint16_expected},
      {"joints0", "uint8x4", uint8_input, uint8_expected},
      {"joints0", "unorm8x4", unorm_input, unorm_expected},
      {"weights0", "float32x4", float_input, float_expected},
      {"weights0", "uint16x4", uint16_input, uint16_expected},
      {"weights0", "uint8x4", uint8_input, uint8_expected},
      {"weights0", "unorm8x4", unorm_input, unorm_expected},
  }};
  auto write = [](const AccessorCase& entry,
                  const flight::Ref<types::MeshGeometry>& geometry) {
    if (flight::String(entry.semantic) == flight::String("color0")) {
      return mesh::set_mesh_geometry_vertex_color0(
          geometry, 0.0, entry.input[0], entry.input[1], entry.input[2],
          entry.input[3]);
    }
    if (flight::String(entry.semantic) == flight::String("joints0")) {
      return mesh::set_mesh_geometry_vertex_joints0(
          geometry, 0.0, entry.input[0], entry.input[1], entry.input[2],
          entry.input[3]);
    }
    return mesh::set_mesh_geometry_vertex_weights0(
        geometry, 0.0, entry.input[0], entry.input[1], entry.input[2],
        entry.input[3]);
  };
  auto read = [](const AccessorCase& entry,
                 const flight::Ref<mesh::w_x_y_z_5137f2b28145ccce>& out,
                 const flight::Ref<types::MeshGeometry>& geometry) {
    if (flight::String(entry.semantic) == flight::String("color0")) {
      return mesh::get_mesh_geometry_vertex_color0(out, geometry, 0.0);
    }
    if (flight::String(entry.semantic) == flight::String("joints0")) {
      return mesh::get_mesh_geometry_vertex_joints0(out, geometry, 0.0);
    }
    return mesh::get_mesh_geometry_vertex_weights0(out, geometry, 0.0);
  };

  bool matrix = true;
  for (const auto& entry : cases) {
    const double byte_length =
        mesh::get_vertex_format_byte_length(flight::String(entry.format));
    const double stride = std::ceil((3.0 + byte_length + 3.0) / 4.0) * 4.0;
    flight::Float32Array vertices(stride / 4.0);
    flight::DataView view(vertices.buffer);
    for (double byte = 0.0; byte < stride; byte += 1.0) {
      view.set_uint8(byte, 204.0);
    }
    const auto layout = vertex_layout(
        flight::Array<flight::Ref<types::VertexAttribute>>{vertex_attribute(
            entry.semantic, entry.format, 3.0)},
        stride);
    const auto geometry = geometry_from(layout, vertices);
    const auto vertex_owner = geometry->vertices.buffer;
    const double version = geometry->version;
    auto out = flight::make_ref<mesh::w_x_y_z_5137f2b28145ccce>();
    matrix = matrix && write(entry, geometry) && read(entry, out, geometry) &&
             geometry->vertices.buffer == vertex_owner &&
             geometry->layout == layout && geometry->version == version + 1.0 &&
             near(out->x, entry.expected[0]) &&
             near(out->y, entry.expected[1]) &&
             near(out->z, entry.expected[2]) &&
             near(out->w, entry.expected[3]);
    for (double byte = 0.0; byte < 3.0; byte += 1.0) {
      matrix = matrix && view.get_uint8(byte) == 204.0;
    }
    for (double byte = 3.0 + byte_length; byte < stride; byte += 1.0) {
      matrix = matrix && view.get_uint8(byte) == 204.0;
    }
  }

  const auto uv_layout = vertex_layout(
      flight::Array<flight::Ref<types::VertexAttribute>>{
          vertex_attribute("uv1", "float32x2", 4.0)},
      16.0);
  const auto uv_geometry =
      geometry_from(uv_layout, flight::Float32Array(4.0));
  const auto uv_owner = uv_geometry->vertices.buffer;
  const double uv_version = uv_geometry->version;
  auto uv = flight::make_ref<mesh::x_y_8365950bd60f783f>();
  matrix = matrix &&
           mesh::set_mesh_geometry_vertex_uv1(
               uv_geometry, 0.0, 1.25, -2.5) &&
           mesh::get_mesh_geometry_vertex_uv1(uv, uv_geometry, 0.0) &&
           uv_geometry->vertices.buffer == uv_owner &&
           uv_geometry->version == uv_version + 1.0 &&
           uv->x == 1.25 && uv->y == -2.5;
  if (inject_accessor_normalization_mutation) matrix = false;
  check(matrix,
        "the systematic accessor matrix round-trips every four-component float, uint16, uint8, and unorm format across color, joints, and weights, applies float3 color alpha=1, preserves float2 UV values, clamps only integer domains, and retains layout and vertex ownership with one version bump");
}

void accessor_bounds_invalid_domains_and_failure_atomicity() {
  const flight::String invalid_format("float16x4");
  flight::Float32Array invalid_storage(4.0);
  flight::DataView invalid_view(invalid_storage.buffer);
  for (double byte = 0.0; byte < 16.0; byte += 1.0) {
    invalid_view.set_uint8(byte, 90.0);
  }
  bool bounds = throws_logic_error([&] {
                  static_cast<void>(
                      mesh::get_vertex_format_byte_length(invalid_format));
                }) &&
                throws_logic_error([&] {
                  static_cast<void>(
                      mesh::get_vertex_format_component_count(invalid_format));
                }) &&
                throws_logic_error([&] {
                  static_cast<void>(mesh::read_vertex_format_component(
                      invalid_view, 0.0, invalid_format, 0.0));
                });
  mesh::write_vertex_format_component(
      invalid_view, 0.0, invalid_format, 0.0, 123.0);
  for (double byte = 0.0; byte < 16.0; byte += 1.0) {
    bounds = bounds && invalid_view.get_uint8(byte) == 90.0;
  }

  flight::Float32Array exact_storage(1.0);
  flight::DataView exact_view(exact_storage.buffer);
  exact_view.set_uint32(0.0, 2863311530.0, true);
  bounds = bounds &&
           throws_std_range_error([&] {
             mesh::write_vertex_format_component(
                 exact_view, 0.0, flight::String("uint8x4"), 4.0, 7.0);
           }) &&
           exact_view.get_uint32(0.0, true) == 2863311530.0 &&
           throws_std_range_error([&] {
             static_cast<void>(mesh::read_vertex_format_component(
                 exact_view, 0.0, flight::String("uint8x4"), -1.0));
           }) &&
           mesh::read_vertex_format_component(
               exact_view, 0.0, flight::String("uint8x4"), 0.5) == 170.0 &&
           exact_view.get_uint32(0.0, true) == 2863311530.0;
  flight::Float32Array float2_storage(2.0);
  flight::DataView float2_view(float2_storage.buffer);
  bounds = bounds && throws_std_range_error([&] {
             static_cast<void>(mesh::read_vertex_format_component(
                 float2_view, 0.0, flight::String("float32x2"), 2.0));
           });

  const auto invalid_layout = vertex_layout(
      flight::Array<flight::Ref<types::VertexAttribute>>{
          vertex_attribute("color0", "float16x4", 0.0)},
      8.0);
  const auto invalid_geometry =
      geometry_from(invalid_layout, flight::Float32Array(2.0));
  const auto invalid_owner = invalid_geometry->vertices.buffer;
  const double invalid_version = invalid_geometry->version;
  auto four = flight::make_ref<mesh::w_x_y_z_5137f2b28145ccce>();
  four->x = 1.0;
  four->y = 2.0;
  four->z = 3.0;
  four->w = 4.0;
  bounds = bounds &&
           throws_logic_error([&] {
             static_cast<void>(mesh::get_mesh_geometry_vertex_color0(
                 four, invalid_geometry, 0.0));
           }) &&
           throws_logic_error([&] {
             static_cast<void>(mesh::set_mesh_geometry_vertex_color0(
                 invalid_geometry, 0.0, 5.0, 6.0, 7.0, 8.0));
           }) &&
           invalid_geometry->vertices.buffer == invalid_owner &&
           invalid_geometry->vertices.get_index(0.0) == 0.0 &&
           invalid_geometry->vertices.get_index(1.0) == 0.0 &&
           invalid_geometry->version == invalid_version && four->x == 1.0 &&
           four->y == 2.0 && four->z == 3.0 && four->w == 4.0;

  const auto negative_layout = vertex_layout(
      flight::Array<flight::Ref<types::VertexAttribute>>{
          vertex_attribute("color0", "uint8x4", -1.0)},
      4.0);
  const auto spilling_layout = vertex_layout(
      flight::Array<flight::Ref<types::VertexAttribute>>{
          vertex_attribute("color0", "uint16x4", 2.0)},
      8.0);
  const auto zero_stride_layout = vertex_layout(
      flight::Array<flight::Ref<types::VertexAttribute>>{
          vertex_attribute("color0", "uint8x4", 0.0)},
      0.0);
  const auto negative =
      geometry_from(negative_layout, flight::Float32Array(1.0));
  const auto spilling =
      geometry_from(spilling_layout, flight::Float32Array(2.0));
  const auto zero_stride =
      geometry_from(zero_stride_layout, flight::Float32Array(1.0));
  const auto partial_layout = vertex_layout(
      flight::Array<flight::Ref<types::VertexAttribute>>{
          vertex_attribute("color0", "uint8x4", 0.0)},
      8.0);
  const auto partial =
      geometry_from(partial_layout, flight::Float32Array(3.0));
  const double partial_version = partial->version;
  bounds = bounds &&
           !mesh::get_mesh_geometry_vertex_color0(four, negative, 0.0) &&
           !mesh::set_mesh_geometry_vertex_color0(
               spilling, 0.0, 1.0, 2.0, 3.0, 4.0) &&
           !mesh::get_mesh_geometry_vertex_color0(four, zero_stride, 0.0) &&
           !mesh::get_mesh_geometry_vertex_color0(four, partial, -1.0) &&
           !mesh::get_mesh_geometry_vertex_color0(four, partial, 1.0) &&
           !mesh::set_mesh_geometry_vertex_color0(
               partial, 1.0, 1.0, 2.0, 3.0, 4.0) &&
           partial->version == partial_version;

  const auto misaligned_layout = vertex_layout(
      flight::Array<flight::Ref<types::VertexAttribute>>{
          vertex_attribute("position", "float32x3", 2.0)},
      16.0);
  auto misaligned_options = flight::make_ref<types::MeshGeometryOptions>();
  misaligned_options->layout = misaligned_layout;
  misaligned_options->vertices = flight::Float32Array(12.0);
  const auto misaligned = mesh::create_mesh_geometry(misaligned_options);
  bounds = bounds && validation_result_without_mutation(misaligned, false);
  if (inject_accessor_bounds_mutation) bounds = false;
  check(bounds,
        "unknown formats, negative and past-end component counts, fractional component coercion, negative/spilling offsets, zero stride, partial records, invalid vertex indices, and misaligned float attributes follow their declared domains without changing outputs, bytes, owners, versions, or validation inputs");
}

void accessor_missing_default_and_wrong_format_paths() {
  const auto empty_layout = vertex_layout({}, 12.0);
  const auto geometry =
      geometry_from(empty_layout, flight::Float32Array{1.0F, 2.0F, 3.0F});
  const auto vertex_owner = geometry->vertices.buffer;
  const double version = geometry->version;
  auto two = flight::make_ref<mesh::x_y_8365950bd60f783f>();
  two->x = 11.0;
  two->y = 12.0;
  auto three = flight::make_ref<mesh::x_y_z_82615fd0df8619c4>();
  three->x = 21.0;
  three->y = 22.0;
  three->z = 23.0;
  auto four = flight::make_ref<mesh::w_x_y_z_5137f2b28145ccce>();
  four->x = 31.0;
  four->y = 32.0;
  four->z = 33.0;
  four->w = 34.0;
  bool missing =
      !mesh::get_mesh_geometry_vertex_position(three, geometry, 0.0) &&
      !mesh::get_mesh_geometry_vertex_normal(three, geometry, 0.0) &&
      !mesh::get_mesh_geometry_vertex_tangent(four, geometry, 0.0) &&
      !mesh::get_mesh_geometry_vertex_uv0(two, geometry, 0.0) &&
      !mesh::get_mesh_geometry_vertex_uv1(two, geometry, 0.0) &&
      !mesh::get_mesh_geometry_vertex_color0(four, geometry, 0.0) &&
      !mesh::get_mesh_geometry_vertex_joints0(four, geometry, 0.0) &&
      !mesh::get_mesh_geometry_vertex_weights0(four, geometry, 0.0) &&
      !mesh::set_mesh_geometry_vertex_position(
          geometry, 0.0, 1.0, 2.0, 3.0) &&
      !mesh::set_mesh_geometry_vertex_normal(
          geometry, 0.0, 1.0, 2.0, 3.0) &&
      !mesh::set_mesh_geometry_vertex_tangent(
          geometry, 0.0, 1.0, 2.0, 3.0, 4.0) &&
      !mesh::set_mesh_geometry_vertex_uv0(geometry, 0.0, 1.0, 2.0) &&
      !mesh::set_mesh_geometry_vertex_uv1(geometry, 0.0, 1.0, 2.0) &&
      !mesh::set_mesh_geometry_vertex_color0(
          geometry, 0.0, 1.0, 2.0, 3.0, 4.0) &&
      !mesh::set_mesh_geometry_vertex_joints0(
          geometry, 0.0, 1.0, 2.0, 3.0, 4.0) &&
      !mesh::set_mesh_geometry_vertex_weights0(
          geometry, 0.0, 1.0, 2.0, 3.0, 4.0) &&
      geometry->vertices.buffer == vertex_owner &&
      geometry->version == version &&
      geometry->vertices.get_index(0.0) == 1.0 &&
      geometry->vertices.get_index(1.0) == 2.0 &&
      geometry->vertices.get_index(2.0) == 3.0 &&
      two->x == 11.0 && two->y == 12.0 && three->x == 21.0 &&
      three->y == 22.0 && three->z == 23.0 && four->x == 31.0 &&
      four->y == 32.0 && four->z == 33.0 && four->w == 34.0;

  const auto color2_layout = vertex_layout(
      flight::Array<flight::Ref<types::VertexAttribute>>{
          vertex_attribute("color0", "float32x2", 0.0)},
      8.0);
  const auto joints3_layout = vertex_layout(
      flight::Array<flight::Ref<types::VertexAttribute>>{
          vertex_attribute("joints0", "float32x3", 0.0)},
      12.0);
  const auto uv3_layout = vertex_layout(
      flight::Array<flight::Ref<types::VertexAttribute>>{
          vertex_attribute("uv1", "float32x3", 0.0)},
      12.0);
  const auto color2 = geometry_from(color2_layout, flight::Float32Array(2.0));
  const auto joints3 = geometry_from(joints3_layout, flight::Float32Array(3.0));
  const auto uv3 = geometry_from(uv3_layout, flight::Float32Array(3.0));
  const auto color2_owner = color2->vertices.buffer;
  const auto joints3_owner = joints3->vertices.buffer;
  const auto uv3_owner = uv3->vertices.buffer;
  const double color2_version = color2->version;
  const double joints3_version = joints3->version;
  const double uv3_version = uv3->version;
  missing = missing &&
            !mesh::get_mesh_geometry_vertex_color0(four, color2, 0.0) &&
            !mesh::set_mesh_geometry_vertex_color0(
                color2, 0.0, 1.0, 2.0, 3.0, 4.0) &&
            !mesh::get_mesh_geometry_vertex_joints0(four, joints3, 0.0) &&
            !mesh::set_mesh_geometry_vertex_joints0(
                joints3, 0.0, 1.0, 2.0, 3.0, 4.0) &&
            !mesh::get_mesh_geometry_vertex_uv1(two, uv3, 0.0) &&
            !mesh::set_mesh_geometry_vertex_uv1(uv3, 0.0, 1.0, 2.0) &&
            color2->vertices.buffer == color2_owner &&
            joints3->vertices.buffer == joints3_owner &&
            uv3->vertices.buffer == uv3_owner &&
            color2->vertices.get_index(0.0) == 0.0 &&
            joints3->vertices.get_index(0.0) == 0.0 &&
            uv3->vertices.get_index(0.0) == 0.0 &&
            color2->version == color2_version &&
            joints3->version == joints3_version &&
            uv3->version == uv3_version;

  const auto first = vertex_attribute("color0", "uint8x4", 0.0);
  const auto second = vertex_attribute("color0", "float32x4", 4.0);
  const auto duplicate_layout = vertex_layout(
      flight::Array<flight::Ref<types::VertexAttribute>>{first, second},
      20.0);
  missing = missing &&
            mesh::get_vertex_attribute(
                duplicate_layout, flight::String("color0")) ==
                std::optional<flight::Ref<types::VertexAttribute>>{first} &&
            !mesh::get_vertex_attribute(
                 duplicate_layout, flight::String("normal"))
                 .has_value() &&
            mesh::get_vertex_attribute_float_offset(
                duplicate_layout, flight::String("color0")) == -1.0 &&
            mesh::get_vertex_attribute_float_offset(
                duplicate_layout, flight::String("normal")) == -1.0;
  if (inject_accessor_missing_default_mutation) missing = false;
  check(missing,
        "all eight accessor families leave output, bytes, owners, and version untouched when their semantic is missing; wrong component formats reject the same way, semantic lookup chooses the first declaration, absent lookup returns none, and packed or absent float offsets return -1");
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

void transform_component_order_aliasing_and_failures() {
  const double diagonal = std::sqrt(0.5);
  const auto source = canonical_frame_vertex(
      1.0, 2.0, 3.0, diagonal, diagonal, 0.0, diagonal, -diagonal,
      0.0, 1.0, 0.25, 0.75);
  const auto out = canonical_frame_vertex(
      0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, -1.0);
  const auto out_vertex_owner = out->vertices.buffer;
  const auto out_bounds = mesh::ensure_mesh_geometry_bounds(out);
  const double out_version = out->version;
  const double source_version = source->version;
  const auto source_vertex_owner = source->vertices.buffer;
  std::array<double, 12> source_values{};
  for (double index = 0.0; index < 12.0; index += 1.0) {
    source_values[static_cast<std::size_t>(index)] =
        source->vertices.get_index(index);
  }

  const auto matrix = geometry::create_matrix4();
  geometry::set_matrix4(
      matrix,
      2.0, 1.0, 0.0, 0.0,
      -1.0, 3.0, 0.0, 0.0,
      0.0, 0.0, 4.0, 0.0,
      5.0, 6.0, 7.0, 1.0);
  bool components = mesh::transform_mesh_geometry_into(out, source, matrix) &&
                    out->vertices.buffer == out_vertex_owner &&
                    out->version == out_version + 1.0 &&
                    near(out->vertices.get_index(0.0), 5.0) &&
                    near(out->vertices.get_index(1.0), 13.0) &&
                    near(out->vertices.get_index(2.0), 19.0) &&
                    out->vertices.get_index(9.0) == 1.0 &&
                    out->vertices.get_index(10.0) == 0.25 &&
                    out->vertices.get_index(11.0) == 0.75 &&
                    out_bounds.has_value() && out->bounds == out_bounds &&
                    near(out_bounds.value()->min->x, 5.0) &&
                    near(out_bounds.value()->min->y, 13.0) &&
                    near(out_bounds.value()->min->z, 19.0) &&
                    near(out_bounds.value()->max->x, 5.0) &&
                    near(out_bounds.value()->max->y, 13.0) &&
                    near(out_bounds.value()->max->z, 19.0);
  for (double index = 0.0; index < 12.0; index += 1.0) {
    components = components && source->vertices.get_index(index) ==
                                   source_values[static_cast<std::size_t>(index)];
  }
  components = components && source->vertices.buffer == source_vertex_owner &&
               source->version == source_version;

  const auto alias = canonical_frame_vertex(
      1.0, 2.0, 3.0, diagonal, diagonal, 0.0, diagonal, -diagonal,
      0.0, 1.0, 0.25, 0.75);
  const double alias_version = alias->version;
  components = components && mesh::transform_mesh_geometry(alias, matrix) &&
               alias->version == alias_version + 1.0 &&
               near(alias->vertices.get_index(0.0), 5.0) &&
               near(alias->vertices.get_index(1.0), 13.0) &&
               near(alias->vertices.get_index(2.0), 19.0);

  const auto matrix_scaled = canonical_frame_vertex(
      1.0, 2.0, 3.0, diagonal, diagonal, 0.0, diagonal, -diagonal,
      0.0, 1.0);
  const auto diagonal_matrix = geometry::create_matrix4();
  diagonal_matrix->m.set_index(0.0, 2.0);
  components = components &&
               mesh::transform_mesh_geometry(matrix_scaled, diagonal_matrix) &&
               matrix_scaled->vertices.get_index(0.0) == 2.0 &&
               matrix_scaled->vertices.get_index(1.0) == 2.0 &&
               matrix_scaled->vertices.get_index(2.0) == 3.0 &&
               near(matrix_scaled->vertices.get_index(3.0),
                    1.0 / std::sqrt(5.0)) &&
               near(matrix_scaled->vertices.get_index(4.0),
                    2.0 / std::sqrt(5.0)) &&
               near(matrix_scaled->vertices.get_index(6.0),
                    2.0 / std::sqrt(5.0)) &&
               near(matrix_scaled->vertices.get_index(7.0),
                    -1.0 / std::sqrt(5.0)) &&
               near(matrix_scaled->vertices.get_index(3.0) *
                            matrix_scaled->vertices.get_index(6.0) +
                        matrix_scaled->vertices.get_index(4.0) *
                            matrix_scaled->vertices.get_index(7.0),
                    0.0);

  const auto scaled = canonical_frame_vertex(
      1.0, 2.0, 3.0, diagonal, diagonal, 0.0, diagonal, -diagonal,
      0.0, 1.0);
  const double scaled_version = scaled->version;
  mesh::scale_mesh_geometry(scaled, 2.0, 1.0, 1.0);
  components = components && scaled->version == scaled_version + 1.0 &&
               scaled->vertices.get_index(0.0) == 2.0 &&
               scaled->vertices.get_index(1.0) == 2.0 &&
               scaled->vertices.get_index(2.0) == 3.0 &&
               near(scaled->vertices.get_index(3.0), 1.0 / std::sqrt(5.0)) &&
               near(scaled->vertices.get_index(4.0), 2.0 / std::sqrt(5.0)) &&
               near(scaled->vertices.get_index(6.0), 2.0 / std::sqrt(5.0)) &&
               near(scaled->vertices.get_index(7.0), -1.0 / std::sqrt(5.0)) &&
               near(scaled->vertices.get_index(3.0) *
                            scaled->vertices.get_index(6.0) +
                        scaled->vertices.get_index(4.0) *
                            scaled->vertices.get_index(7.0),
                    0.0);

  const auto failure_source = canonical_frame_vertex(
      1.0, 2.0, 3.0, diagonal, diagonal, 0.0, diagonal, -diagonal,
      0.0, 1.0);
  const auto failure_out = canonical_frame_vertex(
      -4.0, 5.0, -6.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, -1.0,
      0.4, 0.6);
  const auto failure_bounds = mesh::ensure_mesh_geometry_bounds(failure_out);
  const auto failure_owner = failure_out->vertices.buffer;
  const double failure_version = failure_out->version;
  std::array<double, 12> failure_values{};
  for (double index = 0.0; index < 12.0; index += 1.0) {
    failure_values[static_cast<std::size_t>(index)] =
        failure_out->vertices.get_index(index);
  }
  const auto singular = geometry::create_matrix4();
  singular->m.fill(0.0);
  components = components &&
               !mesh::transform_mesh_geometry_into(
                   failure_out, failure_source, singular) &&
               failure_out->vertices.buffer == failure_owner &&
               failure_out->version == failure_version &&
               failure_out->bounds == failure_bounds;
  for (double index = 0.0; index < 12.0; index += 1.0) {
    components = components && failure_out->vertices.get_index(index) ==
                                   failure_values[static_cast<std::size_t>(index)];
  }

  const auto nearly_singular = geometry::create_matrix4();
  nearly_singular->m.set_index(0.0, 1.0e-12);
  components = components &&
               !mesh::transform_mesh_geometry(failure_source, nearly_singular);
  const double failure_source_version = failure_source->version;
  const auto barely_invertible = geometry::create_matrix4();
  barely_invertible->m.set_index(0.0, 2.0e-10);
  components = components &&
               mesh::transform_mesh_geometry(failure_source, barely_invertible) &&
               failure_source->version == failure_source_version + 1.0 &&
               std::isfinite(failure_source->vertices.get_index(0.0));

  const auto zero_frame = canonical_frame_vertex(
      1.0, 2.0, 3.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, -1.0);
  const auto identity = geometry::create_matrix4();
  components = components && mesh::transform_mesh_geometry(zero_frame, identity) &&
               zero_frame->vertices.get_index(3.0) == 0.0 &&
               zero_frame->vertices.get_index(4.0) == 0.0 &&
               zero_frame->vertices.get_index(5.0) == 0.0 &&
               zero_frame->vertices.get_index(6.0) == 0.0 &&
               zero_frame->vertices.get_index(7.0) == 0.0 &&
               zero_frame->vertices.get_index(8.0) == 0.0 &&
               zero_frame->vertices.get_index(9.0) == -1.0;
  if (inject_transform_component_mutation) components = false;
  check(components,
        "matrix transforms use column-major point order, inverse-transpose normals, plain-matrix tangents, alias-safe locals, retained output owners, and non-mutating singular rejection at the declared threshold");
}

void mirrored_transform_winding_and_handedness() {
  auto indexed_triangle_list = [] {
    flight::Float32Array vertices(48.0);
    for (double vertex = 0.0; vertex < 4.0; vertex += 1.0) {
      const double base = vertex * 12.0;
      vertices.set_index(base, vertex + 1.0);
      vertices.set_index(base + 7.0, 1.0);
      vertices.set_index(base + 9.0, 1.0);
      vertices.set_index(base + 10.0, 10.0 + vertex);
    }
    flight::Uint16Array indices{0, 1, 2, 2, 1, 3};
    auto options = flight::make_ref<types::MeshGeometryOptions>();
    options->indices = flight::SequenceView<double>(indices);
    options->layout = mesh::canonical_mesh_geometry_layout;
    options->vertices = vertices;
    return mesh::create_mesh_geometry(options);
  };

  const auto indexed = indexed_triangle_list();
  const void* indexed_owner = index_buffer_identity(indexed);
  const double indexed_version = indexed->version;
  const auto mirror = geometry::create_matrix4();
  mirror->m.set_index(0.0, -1.0);
  bool mirrored = mesh::transform_mesh_geometry(indexed, mirror) &&
                  indexed->version == indexed_version + 1.0 &&
                  index_buffer_identity(indexed) == indexed_owner;
  const double mirrored_indices[] = {0.0, 2.0, 1.0, 2.0, 3.0, 1.0};
  for (double index = 0.0; index < 6.0; index += 1.0) {
    mirrored = mirrored && index_at(indexed, index) ==
                               mirrored_indices[static_cast<std::size_t>(index)];
  }
  for (double vertex = 0.0; vertex < 4.0; vertex += 1.0) {
    mirrored = mirrored &&
               indexed->vertices.get_index(vertex * 12.0) == -(vertex + 1.0) &&
               indexed->vertices.get_index(vertex * 12.0 + 9.0) == -1.0;
  }

  flight::Float32Array sequential_vertices(72.0);
  for (double vertex = 0.0; vertex < 6.0; vertex += 1.0) {
    const double base = vertex * 12.0;
    sequential_vertices.set_index(base, vertex + 1.0);
    sequential_vertices.set_index(base + 7.0, 1.0);
    sequential_vertices.set_index(base + 9.0, 1.0);
    sequential_vertices.set_index(base + 10.0, 10.0 + vertex);
  }
  const auto sequential = geometry_from(
      mesh::canonical_mesh_geometry_layout, sequential_vertices);
  const auto sequential_owner = sequential->vertices.buffer;
  const double sequential_version = sequential->version;
  mesh::scale_mesh_geometry(sequential, -1.0, 1.0, 1.0);
  const double expected_x[] = {-1.0, -3.0, -2.0, -4.0, -6.0, -5.0};
  const double expected_u[] = {10.0, 12.0, 11.0, 13.0, 15.0, 14.0};
  mirrored = mirrored && !sequential->indices.has_value() &&
             sequential->vertices.buffer == sequential_owner &&
             sequential->version == sequential_version + 1.0;
  for (double vertex = 0.0; vertex < 6.0; vertex += 1.0) {
    const double base = vertex * 12.0;
    mirrored = mirrored && sequential->vertices.get_index(base) ==
                               expected_x[static_cast<std::size_t>(vertex)] &&
               sequential->vertices.get_index(base + 9.0) == -1.0 &&
               sequential->vertices.get_index(base + 10.0) ==
                   expected_u[static_cast<std::size_t>(vertex)];
  }

  const auto strip = indexed_triangle_list();
  strip->topology = flight::String("triangle-strip");
  strip->indices = flight::Uint16Array{0, 1, 2, 3};
  const void* strip_owner = index_buffer_identity(strip);
  mesh::scale_mesh_geometry(strip, -1.0, 1.0, 1.0);
  for (double index = 0.0; index < 4.0; index += 1.0) {
    mirrored = mirrored && index_at(strip, index) == index &&
               strip->vertices.get_index(index * 12.0 + 9.0) == -1.0;
  }
  mirrored = mirrored && index_buffer_identity(strip) == strip_owner;

  const auto positive = indexed_triangle_list();
  const void* positive_owner = index_buffer_identity(positive);
  mesh::scale_mesh_geometry(positive, 2.0, 3.0, 4.0);
  const double authored_indices[] = {0.0, 1.0, 2.0, 2.0, 1.0, 3.0};
  for (double index = 0.0; index < 6.0; index += 1.0) {
    mirrored = mirrored && index_at(positive, index) ==
                               authored_indices[static_cast<std::size_t>(index)];
  }
  for (double vertex = 0.0; vertex < 4.0; vertex += 1.0) {
    mirrored = mirrored &&
               positive->vertices.get_index(vertex * 12.0 + 9.0) == 1.0;
  }
  mirrored = mirrored && index_buffer_identity(positive) == positive_owner;
  if (inject_mirrored_transform_mutation) mirrored = false;
  check(mirrored,
        "negative determinants reverse each indexed triangle or swap complete non-indexed records and negate handedness, while strips retain element order and positive determinants retain both winding and handedness");
}

void transform_bounds_center_and_cache_invalidation() {
  flight::Float32Array vertices(24.0);
  const double positions[][3] = {{0.0, -2.0, 1.0}, {4.0, 2.0, 5.0}};
  for (double vertex = 0.0; vertex < 2.0; vertex += 1.0) {
    const double base = vertex * 12.0;
    const auto index = static_cast<std::size_t>(vertex);
    vertices.set_index(base, positions[index][0]);
    vertices.set_index(base + 1.0, positions[index][1]);
    vertices.set_index(base + 2.0, positions[index][2]);
    vertices.set_index(base + 3.0, 1.0);
    vertices.set_index(base + 7.0, 1.0);
    vertices.set_index(base + 9.0, 1.0);
    vertices.set_index(base + 10.0, 0.25 + vertex);
    vertices.set_index(base + 11.0, 0.75 - vertex);
  }
  const auto transformed = geometry_from(
      mesh::canonical_mesh_geometry_layout, vertices);
  const auto bounds = mesh::ensure_mesh_geometry_bounds(transformed);
  const auto runtime = transformed->entity_runtime_key.value();
  const double original_stamp = runtime->bounds_version;
  const double original_version = transformed->version;
  mesh::translate_mesh_geometry(transformed, 5.0, -1.0, 2.0);
  bool cache = bounds.has_value() && transformed->bounds == bounds &&
               transformed->version == original_version + 1.0 &&
               runtime->bounds_version == original_stamp &&
               near(bounds.value()->min->x, 5.0) &&
               near(bounds.value()->min->y, -3.0) &&
               near(bounds.value()->min->z, 3.0) &&
               near(bounds.value()->max->x, 9.0) &&
               near(bounds.value()->max->y, 1.0) &&
               near(bounds.value()->max->z, 7.0);
  for (double vertex = 0.0; vertex < 2.0; vertex += 1.0) {
    const double base = vertex * 12.0;
    cache = cache && transformed->vertices.get_index(base + 3.0) == 1.0 &&
            transformed->vertices.get_index(base + 4.0) == 0.0 &&
            transformed->vertices.get_index(base + 7.0) == 1.0 &&
            transformed->vertices.get_index(base + 9.0) == 1.0 &&
            transformed->vertices.get_index(base + 10.0) == 0.25 + vertex &&
            transformed->vertices.get_index(base + 11.0) == 0.75 - vertex;
  }
  cache = cache && mesh::ensure_mesh_geometry_bounds(transformed) == bounds &&
          runtime->bounds_version == transformed->version;

  const double translated_version = transformed->version;
  const double translated_stamp = runtime->bounds_version;
  mesh::scale_mesh_geometry(transformed, 2.0, 3.0, 4.0);
  cache = cache && transformed->bounds == bounds &&
          transformed->version == translated_version + 1.0 &&
          runtime->bounds_version == translated_stamp &&
          near(bounds.value()->min->x, 10.0) &&
          near(bounds.value()->min->y, -9.0) &&
          near(bounds.value()->min->z, 12.0) &&
          near(bounds.value()->max->x, 18.0) &&
          near(bounds.value()->max->y, 3.0) &&
          near(bounds.value()->max->z, 28.0) &&
          mesh::ensure_mesh_geometry_bounds(transformed) == bounds &&
          runtime->bounds_version == transformed->version;

  flight::Float32Array center_vertices(24.0);
  center_vertices.set_index(12.0, 4.0);
  center_vertices.set_index(13.0, 2.0);
  center_vertices.set_index(14.0, -2.0);
  const auto centered = geometry_from(
      mesh::canonical_mesh_geometry_layout, center_vertices);
  const auto centered_owner = centered->vertices.buffer;
  const double centered_version = centered->version;
  mesh::center_mesh_geometry(centered);
  cache = cache && centered->vertices.buffer == centered_owner &&
          centered->version == centered_version + 1.0 &&
          centered->bounds.has_value() &&
          centered->vertices.get_index(0.0) == -2.0 &&
          centered->vertices.get_index(1.0) == -1.0 &&
          centered->vertices.get_index(2.0) == 1.0 &&
          centered->vertices.get_index(12.0) == 2.0 &&
          centered->vertices.get_index(13.0) == 1.0 &&
          centered->vertices.get_index(14.0) == -1.0 &&
          centered->bounds.value()->min->x == -2.0 &&
          centered->bounds.value()->min->y == -1.0 &&
          centered->bounds.value()->min->z == -1.0 &&
          centered->bounds.value()->max->x == 2.0 &&
          centered->bounds.value()->max->y == 1.0 &&
          centered->bounds.value()->max->z == 1.0 &&
          centered->entity_runtime_key.value()->bounds_version !=
              centered->version;
  const auto centered_bounds = mesh::ensure_mesh_geometry_bounds(centered);
  const double no_op_version = centered->version;
  std::array<double, 24> centered_values{};
  for (double index = 0.0; index < 24.0; index += 1.0) {
    centered_values[static_cast<std::size_t>(index)] =
        centered->vertices.get_index(index);
  }
  mesh::center_mesh_geometry(centered);
  cache = cache && centered->version == no_op_version &&
          centered->bounds == centered_bounds;
  for (double index = 0.0; index < 24.0; index += 1.0) {
    cache = cache && centered->vertices.get_index(index) ==
                         centered_values[static_cast<std::size_t>(index)];
  }

  const auto no_position_layout = vertex_layout(
      flight::Array<flight::Ref<types::VertexAttribute>>{
          vertex_attribute("uv0", "float32x2", 0.0)},
      8.0);
  const auto no_position = geometry_from(
      no_position_layout, flight::Float32Array{0.25F, 0.75F});
  const auto no_position_owner = no_position->vertices.buffer;
  const double no_position_version = no_position->version;
  mesh::translate_mesh_geometry(no_position, 5.0, 6.0, 7.0);
  cache = cache && no_position->vertices.buffer == no_position_owner &&
          no_position->version == no_position_version &&
          no_position->vertices.get_index(0.0) == 0.25 &&
          no_position->vertices.get_index(1.0) == 0.75;
  if (inject_transform_bounds_mutation) cache = false;
  check(cache,
        "translate and scale update cached bounds in place while center constructs missing bounds; each leaves cache stamps stale until ensure, preserves non-position channels, and makes centered or position-less calls exact no-ops");
}

void uv_axes_order_ownership_and_noops() {
  const auto layout = vertex_layout(
      flight::Array<flight::Ref<types::VertexAttribute>>{
          vertex_attribute("uv0", "float32x2", 16.0),
          vertex_attribute("position", "float32x3", 0.0)},
      32.0);
  flight::Float32Array vertices{
      1.0F, 2.0F, 3.0F, 99.0F, 0.25F, 0.75F, 98.0F, 97.0F,
      -4.0F, 5.0F, -6.0F, 88.0F, -0.5F, 2.0F, 87.0F, 86.0F,
  };
  const auto reverse_vertices = vertices.slice();
  const auto geometry = geometry_from(layout, vertices);
  const auto vertex_owner = geometry->vertices.buffer;
  const auto bounds = mesh::ensure_mesh_geometry_bounds(geometry);
  const auto runtime = geometry->entity_runtime_key.value();
  const double bounds_stamp = runtime->bounds_version;
  const double version = geometry->version;
  mesh::offset_mesh_geometry_uvs(geometry, 0.1, -0.2);
  mesh::scale_mesh_geometry_uvs(geometry, -2.0, 0.5);
  bool uvs = geometry->layout == layout &&
             geometry->vertices.buffer == vertex_owner &&
             geometry->version == version + 2.0 &&
             near(geometry->vertices.get_index(4.0), -0.7) &&
             near(geometry->vertices.get_index(5.0), 0.275) &&
             near(geometry->vertices.get_index(12.0), 0.8) &&
             near(geometry->vertices.get_index(13.0), 0.9) &&
             geometry->vertices.get_index(0.0) == 1.0 &&
             geometry->vertices.get_index(1.0) == 2.0 &&
             geometry->vertices.get_index(2.0) == 3.0 &&
             geometry->vertices.get_index(3.0) == 99.0 &&
             geometry->vertices.get_index(6.0) == 98.0 &&
             geometry->vertices.get_index(7.0) == 97.0 &&
             geometry->vertices.get_index(8.0) == -4.0 &&
             geometry->vertices.get_index(9.0) == 5.0 &&
             geometry->vertices.get_index(10.0) == -6.0 &&
             geometry->vertices.get_index(11.0) == 88.0 &&
             geometry->vertices.get_index(14.0) == 87.0 &&
             geometry->vertices.get_index(15.0) == 86.0 &&
             geometry->bounds == bounds &&
             runtime->bounds_version == bounds_stamp &&
             bounds.value()->min->x == -4.0 &&
             bounds.value()->max->x == 1.0 &&
             mesh::ensure_mesh_geometry_bounds(geometry) == bounds &&
             runtime->bounds_version == geometry->version;

  const auto reverse_order = geometry_from(layout, reverse_vertices);
  mesh::scale_mesh_geometry_uvs(reverse_order, -2.0, 0.5);
  mesh::offset_mesh_geometry_uvs(reverse_order, 0.1, -0.2);
  uvs = uvs && near(reverse_order->vertices.get_index(4.0), -0.4) &&
        near(reverse_order->vertices.get_index(5.0), 0.175) &&
        near(reverse_order->vertices.get_index(12.0), 1.1) &&
        near(reverse_order->vertices.get_index(13.0), 0.8);

  flight::Float32Array tangent_vertices(36.0);
  for (double vertex = 0.0; vertex < 3.0; vertex += 1.0) {
    const double base = vertex * 12.0;
    tangent_vertices.set_index(base + 6.0, 0.25 + vertex);
    tangent_vertices.set_index(base + 7.0, 0.5 + vertex);
    tangent_vertices.set_index(base + 8.0, 0.75 + vertex);
    tangent_vertices.set_index(base + 9.0, vertex == 1.0 ? -1.0 : 1.0);
    tangent_vertices.set_index(base + 10.0, 0.2 + vertex);
    tangent_vertices.set_index(base + 11.0, 0.4 + vertex);
  }
  flight::Uint16Array authored_indices{0, 1, 2};
  auto tangent_options = flight::make_ref<types::MeshGeometryOptions>();
  tangent_options->indices = flight::SequenceView<double>(authored_indices);
  tangent_options->layout = mesh::canonical_mesh_geometry_layout;
  tangent_options->vertices = tangent_vertices;
  const auto tangent_geometry = mesh::create_mesh_geometry(tangent_options);
  std::array<double, 12> tangent_values{};
  for (double vertex = 0.0; vertex < 3.0; vertex += 1.0) {
    for (double component = 0.0; component < 4.0; component += 1.0) {
      tangent_values[static_cast<std::size_t>(vertex * 4.0 + component)] =
          tangent_geometry->vertices.get_index(vertex * 12.0 + 6.0 + component);
    }
  }
  const void* tangent_index_owner = index_buffer_identity(tangent_geometry);
  mesh::scale_mesh_geometry_uvs(tangent_geometry, -1.0, 1.0);
  for (double vertex = 0.0; vertex < 3.0; vertex += 1.0) {
    uvs = uvs && index_at(tangent_geometry, vertex) == vertex;
    for (double component = 0.0; component < 4.0; component += 1.0) {
      uvs = uvs &&
            tangent_geometry->vertices.get_index(
                vertex * 12.0 + 6.0 + component) ==
                tangent_values[static_cast<std::size_t>(
                    vertex * 4.0 + component)];
    }
  }
  uvs = uvs && index_buffer_identity(tangent_geometry) == tangent_index_owner;

  const auto no_uv = geometry_from(
      position_layout(), flight::Float32Array{1.0F, 2.0F, 3.0F});
  const auto no_uv_owner = no_uv->vertices.buffer;
  const double no_uv_version = no_uv->version;
  mesh::offset_mesh_geometry_uvs(no_uv, 5.0, 6.0);
  mesh::scale_mesh_geometry_uvs(no_uv, 7.0, 8.0);
  const auto empty = geometry_from(
      mesh::canonical_mesh_geometry_layout, flight::Float32Array(0.0));
  const double empty_version = empty->version;
  mesh::offset_mesh_geometry_uvs(empty, 1.0, 2.0);
  mesh::scale_mesh_geometry_uvs(empty, 3.0, 4.0);
  uvs = uvs && no_uv->vertices.buffer == no_uv_owner &&
        no_uv->version == no_uv_version &&
        no_uv->vertices.get_index(0.0) == 1.0 &&
        no_uv->vertices.get_index(1.0) == 2.0 &&
        no_uv->vertices.get_index(2.0) == 3.0 &&
        empty->version == empty_version && empty->vertices.empty();
  if (inject_uv_axes_mutation) uvs = false;
  check(uvs,
        "UV offset and scale resolve uv0 by semantic, preserve u/v axes and call order in a custom stride, retain every non-UV owner and channel, invalidate only the generic version cache, and no-op for absent or empty UV streams");
}

void morph_target_channel_order_and_capture_ownership() {
  const auto layout = vertex_layout(
      flight::Array<flight::Ref<types::VertexAttribute>>{
          vertex_attribute("tangent", "float32x4", 24.0),
          vertex_attribute("position", "float32x3", 0.0),
          vertex_attribute("uv0", "float32x2", 40.0),
          vertex_attribute("normal", "float32x3", 12.0)},
      48.0);
  const flight::Float32Array vertices{
      1.0F, 2.0F, 3.0F, 4.0F, 5.0F, 6.0F,
      7.0F, 8.0F, 9.0F, 10.0F, 11.0F, 12.0F,
      -1.0F, -2.0F, -3.0F, -4.0F, -5.0F, -6.0F,
      -7.0F, -8.0F, -9.0F, -10.0F, -11.0F, -12.0F,
  };
  const auto geometry = geometry_from(layout, vertices);
  const auto vertex_owner = geometry->vertices.buffer;
  const auto bind_pose = mesh::capture_mesh_morph_bind_pose(geometry);
  const double expected_positions[] = {1.0, 2.0, 3.0, -1.0, -2.0, -3.0};
  const double expected_normals[] = {4.0, 5.0, 6.0, -4.0, -5.0, -6.0};
  const double expected_tangents[] = {7.0, 8.0, 9.0, -7.0, -8.0, -9.0};
  bool ordered = bind_pose->positions.size() == 6 &&
                 bind_pose->blended_positions.size() == 6 &&
                 bind_pose->normals.has_value() &&
                 bind_pose->blended_normals.has_value() &&
                 bind_pose->tangents.has_value() &&
                 bind_pose->blended_tangents.has_value() &&
                 bind_pose->positions.buffer != geometry->vertices.buffer &&
                 bind_pose->blended_positions.buffer !=
                     bind_pose->positions.buffer &&
                 bind_pose->normals.value().buffer !=
                     bind_pose->blended_normals.value().buffer &&
                 bind_pose->tangents.value().buffer !=
                     bind_pose->blended_tangents.value().buffer;
  for (double index = 0.0; index < 6.0; index += 1.0) {
    const auto i = static_cast<std::size_t>(index);
    ordered = ordered &&
              bind_pose->positions.get_index(index) == expected_positions[i] &&
              bind_pose->normals.value().get_index(index) ==
                  expected_normals[i] &&
              bind_pose->tangents.value().get_index(index) ==
                  expected_tangents[i];
  }

  const auto first = morph_target(
      flight::Float32Array{1.0F, 0.0F, 0.0F, 0.0F, 2.0F, 0.0F},
      flight::Float32Array{0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 3.0F});
  const auto second = morph_target(
      flight::Float32Array{0.0F, 0.0F, 4.0F, 5.0F, 0.0F, 0.0F},
      std::nullopt,
      flight::Float32Array{1.0F, 2.0F, 3.0F, 4.0F, 5.0F, 6.0F});
  const auto first_position_owner = first->position_deltas.buffer;
  const auto second_tangent_owner = second->tangent_deltas.value().buffer;
  const auto morph = mesh_morph(
      flight::Array<flight::Ref<types::MorphTarget>>{first, second},
      flight::Float32Array{2.0F, -0.5F});
  const auto target_owner = morph->targets.identity();
  const auto weight_owner = morph->weights.buffer;
  geometry->vertices.set_index(0.0, 99.0);
  const double version = geometry->version;
  mesh::blend_mesh_geometry_morph(geometry, morph, bind_pose);
  const double expected_vertices[] = {
      3.0, 2.0, 1.0, 4.0, 7.0, 6.0,
      6.5, 7.0, 7.5, 10.0, 11.0, 12.0,
      -3.5, 2.0, -3.0, -4.0, -5.0, 0.0,
      -9.0, -10.5, -12.0, -10.0, -11.0, -12.0,
  };
  ordered = ordered && geometry->vertices.buffer == vertex_owner &&
            geometry->layout == layout && geometry->version == version + 1.0 &&
            morph->targets.identity() == target_owner &&
            morph->weights.buffer == weight_owner &&
            first->position_deltas.buffer == first_position_owner &&
            second->tangent_deltas.value().buffer == second_tangent_owner &&
            bind_pose->positions.get_index(0.0) == 1.0;
  for (double index = 0.0; index < 24.0; index += 1.0) {
    ordered = ordered && geometry->vertices.get_index(index) ==
                               expected_vertices[static_cast<std::size_t>(index)];
  }
  if (inject_morph_target_order_mutation) ordered = false;
  check(ordered,
        "morph capture resolves channels by semantic into independent ordered base and scratch arrays, then pairs each target with its authored weight while retaining every geometry, target, and weight owner and preserving tangent w and UVs");
}

void morph_weight_math_sparse_targets_and_limits() {
  const auto geometry = geometry_from(
      position_layout(),
      flight::Float32Array{10.0F, 20.0F, 30.0F, 40.0F, 50.0F, 60.0F});
  const auto bind_pose = mesh::capture_mesh_morph_bind_pose(geometry);
  const auto zero = morph_target(flight::Float32Array{
      90.0F, 90.0F, 90.0F, 90.0F, 90.0F, 90.0F});
  const auto negative = morph_target(flight::Float32Array{
      1.0F, 2.0F, 3.0F, 4.0F, 5.0F, 6.0F});
  const auto sparse = morph_target(flight::Float32Array{10.0F, 20.0F});
  const auto ignored = morph_target(flight::Float32Array{
      100.0F, 100.0F, 100.0F, 100.0F, 100.0F, 100.0F});
  const auto morph = mesh_morph(
      flight::Array<flight::Ref<types::MorphTarget>>{
          zero, negative, sparse, ignored},
      flight::Float32Array{0.0F, -2.0F, 1.5F});
  const auto weights_owner = morph->weights.buffer;
  const auto zero_owner = zero->position_deltas.buffer;
  const auto sparse_owner = sparse->position_deltas.buffer;
  mesh::blend_mesh_geometry_morph(geometry, morph, bind_pose);
  const double expected[] = {23.0, 46.0, 24.0, 32.0, 40.0, 48.0};
  bool weights = morph->weights.buffer == weights_owner &&
                 zero->position_deltas.buffer == zero_owner &&
                 sparse->position_deltas.buffer == sparse_owner &&
                 morph->weights.get_index(0.0) == 0.0 &&
                 morph->weights.get_index(1.0) == -2.0 &&
                 morph->weights.get_index(2.0) == 1.5 &&
                 ignored->position_deltas.get_index(0.0) == 100.0;
  for (double index = 0.0; index < 6.0; index += 1.0) {
    weights = weights && geometry->vertices.get_index(index) ==
                           expected[static_cast<std::size_t>(index)];
  }

  const auto extra_weight_geometry = geometry_from(
      position_layout(), flight::Float32Array{1.0F, 2.0F, 3.0F});
  const auto extra_weight_pose =
      mesh::capture_mesh_morph_bind_pose(extra_weight_geometry);
  const auto extra_weight_morph = mesh_morph(
      flight::Array<flight::Ref<types::MorphTarget>>{
          morph_target(flight::Float32Array{2.0F, 3.0F, 4.0F})},
      flight::Float32Array{2.0F, 99.0F});
  mesh::blend_mesh_geometry_morph(
      extra_weight_geometry, extra_weight_morph, extra_weight_pose);
  weights = weights && extra_weight_geometry->vertices.get_index(0.0) == 5.0 &&
            extra_weight_geometry->vertices.get_index(1.0) == 8.0 &&
            extra_weight_geometry->vertices.get_index(2.0) == 11.0;
  if (inject_morph_weight_math_mutation) weights = false;
  check(weights,
        "morph weights are unbounded additive coefficients: zero skips, negative and greater-than-one values apply exactly, sparse deltas affect only their prefix, and unmatched extra targets or weights are ignored without mutating any input array");
}

void morph_accumulation_reset_and_repeated_updates() {
  const auto geometry = geometry_from(
      position_layout(), flight::Float32Array{1.0F, 2.0F, 3.0F});
  const auto bind_pose = mesh::capture_mesh_morph_bind_pose(geometry);
  const auto scratch_owner = bind_pose->blended_positions.buffer;
  const auto target = morph_target(
      flight::Float32Array{2.0F, 4.0F, 6.0F});
  const auto morph = mesh_morph(
      flight::Array<flight::Ref<types::MorphTarget>>{target},
      flight::Float32Array{1.0F});
  const double version = geometry->version;
  mesh::blend_mesh_geometry_morph(geometry, morph, bind_pose);
  bool reset = geometry->vertices.get_index(0.0) == 3.0 &&
               geometry->vertices.get_index(1.0) == 6.0 &&
               geometry->vertices.get_index(2.0) == 9.0 &&
               geometry->version == version + 1.0;
  mesh::blend_mesh_geometry_morph(geometry, morph, bind_pose);
  reset = reset && geometry->vertices.get_index(0.0) == 3.0 &&
          geometry->vertices.get_index(1.0) == 6.0 &&
          geometry->vertices.get_index(2.0) == 9.0 &&
          geometry->version == version + 2.0;
  morph->weights.set_index(0.0, -1.0);
  mesh::blend_mesh_geometry_morph(geometry, morph, bind_pose);
  reset = reset && geometry->vertices.get_index(0.0) == -1.0 &&
          geometry->vertices.get_index(1.0) == -2.0 &&
          geometry->vertices.get_index(2.0) == -3.0;
  morph->weights.set_index(0.0, 0.0);
  mesh::blend_mesh_geometry_morph(geometry, morph, bind_pose);
  reset = reset && geometry->vertices.get_index(0.0) == 1.0 &&
          geometry->vertices.get_index(1.0) == 2.0 &&
          geometry->vertices.get_index(2.0) == 3.0 &&
          bind_pose->positions.get_index(0.0) == 1.0 &&
          bind_pose->positions.get_index(1.0) == 2.0 &&
          bind_pose->positions.get_index(2.0) == 3.0 &&
          bind_pose->blended_positions.buffer == scratch_owner;
  if (inject_morph_reset_mutation) reset = false;
  check(reset,
        "every direct morph blend resets reusable scratch from the immutable captured base before accumulation, so repeated calls are value-idempotent, changed and zero weights never compound, and only the version advances per call");
}

void morph_deformation_clone_aliasing_and_bind_pose_precedence() {
  const auto source = canonical_frame_vertex(
      1.0, 2.0, 3.0, 0.0, 1.0, 0.0, 1.0, 0.0, 0.0, 1.0,
      0.25, 0.75);
  const auto morph_pose = mesh::capture_mesh_morph_bind_pose(source);
  mesh::set_mesh_geometry_morph_bind_pose(source, morph_pose);
  const auto skin_pose = flight::make_ref<types::MeshSkinBindPose>();
  skin_pose->positions = flight::Float32Array{90.0F, 91.0F, 92.0F};
  skin_pose->normals = flight::Float32Array{9.0F, 9.0F, 9.0F};
  skin_pose->tangents = flight::Float32Array{8.0F, 8.0F, 8.0F};
  skin_pose->joints = flight::Float32Array(4.0);
  skin_pose->weights = flight::Float32Array(4.0);
  skin_pose->skinned_positions = flight::Float32Array(3.0);
  skin_pose->skinned_normals = flight::Float32Array(3.0);
  skin_pose->skinned_tangents = flight::Float32Array(3.0);
  mesh::set_mesh_geometry_skin_bind_pose(source, skin_pose);
  const auto morph = mesh_morph(
      flight::Array<flight::Ref<types::MorphTarget>>{morph_target(
          flight::Float32Array{5.0F, 0.0F, 0.0F},
          flight::Float32Array{1.0F, 0.0F, 0.0F},
          flight::Float32Array{0.0F, 2.0F, 0.0F})},
      flight::Float32Array{1.0F});
  mesh::blend_mesh_geometry_morph(source, morph, morph_pose);
  const auto source_vertex_owner = source->vertices.buffer;
  const auto source_runtime = source->entity_runtime_key.value();
  const auto clone = mesh::clone_mesh_geometry_for_deformation(source);
  const auto clone_runtime = clone->entity_runtime_key.value();
  bool aliasing = source->vertices.get_index(0.0) == 6.0 &&
                  source->vertices.get_index(3.0) == 1.0 &&
                  source->vertices.get_index(7.0) == 2.0 &&
                  mesh::get_mesh_geometry_morph_bind_pose(source) ==
                      std::optional<flight::Ref<types::MeshMorphBindPose>>{
                          morph_pose} &&
                  mesh::get_mesh_geometry_skin_bind_pose(source) ==
                      std::optional<flight::Ref<types::MeshSkinBindPose>>{
                          skin_pose} &&
                  clone != source && clone->vertices.buffer != source_vertex_owner &&
                  clone->layout == source->layout && clone_runtime != source_runtime &&
                  clone->vertices.get_index(0.0) == 1.0 &&
                  clone->vertices.get_index(1.0) == 2.0 &&
                  clone->vertices.get_index(2.0) == 3.0 &&
                  clone->vertices.get_index(3.0) == 0.0 &&
                  clone->vertices.get_index(4.0) == 1.0 &&
                  clone->vertices.get_index(5.0) == 0.0 &&
                  clone->vertices.get_index(6.0) == 1.0 &&
                  clone->vertices.get_index(7.0) == 0.0 &&
                  clone->vertices.get_index(8.0) == 0.0 &&
                  clone->vertices.get_index(9.0) == 1.0 &&
                  clone->vertices.get_index(10.0) == 0.25 &&
                  clone->vertices.get_index(11.0) == 0.75 &&
                  !mesh::get_mesh_geometry_morph_bind_pose(clone).has_value() &&
                  !mesh::get_mesh_geometry_skin_bind_pose(clone).has_value() &&
                  clone->bounds.has_value() &&
                  clone->bounds.value()->min->x == 1.0 &&
                  clone->bounds.value()->max->x == 1.0;
  clone->vertices.set_index(0.0, -100.0);
  aliasing = aliasing && source->vertices.get_index(0.0) == 6.0 &&
             morph_pose->positions.get_index(0.0) == 1.0;

  const auto skin_source = canonical_frame_vertex(
      9.0, 9.0, 9.0, 9.0, 9.0, 9.0, 8.0, 8.0, 8.0, -1.0);
  const auto skin_only = flight::make_ref<types::MeshSkinBindPose>();
  skin_only->positions = flight::Float32Array{2.0F, 3.0F, 4.0F};
  skin_only->normals = flight::Float32Array{0.0F, 1.0F, 0.0F};
  skin_only->tangents = flight::Float32Array{7.0F, 7.0F, 7.0F};
  skin_only->joints = flight::Float32Array(4.0);
  skin_only->weights = flight::Float32Array(4.0);
  skin_only->skinned_positions = flight::Float32Array(3.0);
  skin_only->skinned_normals = flight::Float32Array(3.0);
  skin_only->skinned_tangents = flight::Float32Array(3.0);
  mesh::set_mesh_geometry_skin_bind_pose(skin_source, skin_only);
  const auto skin_clone =
      mesh::clone_mesh_geometry_for_deformation(skin_source);
  aliasing = aliasing && skin_clone->vertices.get_index(0.0) == 2.0 &&
             skin_clone->vertices.get_index(1.0) == 3.0 &&
             skin_clone->vertices.get_index(2.0) == 4.0 &&
             skin_clone->vertices.get_index(3.0) == 0.0 &&
             skin_clone->vertices.get_index(4.0) == 1.0 &&
             skin_clone->vertices.get_index(5.0) == 0.0 &&
             skin_clone->vertices.get_index(6.0) == 8.0 &&
             skin_clone->vertices.get_index(7.0) == 8.0 &&
             skin_clone->vertices.get_index(8.0) == 8.0 &&
             skin_source->vertices.get_index(0.0) == 9.0 &&
             !mesh::get_mesh_geometry_skin_bind_pose(skin_clone).has_value();
  if (inject_morph_aliasing_mutation) aliasing = false;
  check(aliasing,
        "deformation cloning restores the deeper morph bind pose ahead of a skin pose, falls back to skin position and normal only, recomputes exact bounds, and isolates vertex, runtime, and bind-pose state without changing the deformed source");
}

void morph_update_bounds_runtime_slots_and_settled_noops() {
  const auto geometry = geometry_from(
      position_layout(),
      flight::Float32Array{0.0F, 0.0F, 0.0F, 2.0F, 0.0F, 0.0F});
  const auto bounds = mesh::ensure_mesh_geometry_bounds(geometry);
  const auto runtime = geometry->entity_runtime_key.value();
  const double initial_version = geometry->version;
  const double initial_stamp = runtime->bounds_version;
  const auto morph = mesh_morph(
      flight::Array<flight::Ref<types::MorphTarget>>{morph_target(
          flight::Float32Array{0.0F, 4.0F, 0.0F, 3.0F, 0.0F, 0.0F})},
      flight::Float32Array{0.5F});
  const auto carrier = morph_carrier(
      geometry,
      std::variant<flight::Ref<types::MeshMorph>, flight::Null,
                   flight::Undefined>{
          std::in_place_type<flight::Ref<types::MeshMorph>>, morph});
  mesh::update_mesh_morph(carrier);
  const auto bind_pose = mesh::get_mesh_geometry_morph_bind_pose(geometry);
  bool bounds_and_slots = bounds.has_value() && bind_pose.has_value() &&
                          geometry->version == initial_version + 1.0 &&
                          geometry->bounds == bounds &&
                          runtime->bounds_version == initial_stamp &&
                          geometry->vertices.get_index(1.0) == 2.0 &&
                          geometry->vertices.get_index(3.0) == 3.5 &&
                          runtime->morph_blended_weights.has_value() &&
                          runtime->morph_blended_weights.value().buffer !=
                              morph->weights.buffer &&
                          runtime->morph_blended_weights.value().get_index(0.0) ==
                              0.5;
  const auto weight_snapshot_owner =
      runtime->morph_blended_weights.value().buffer;
  const auto scratch_owner = bind_pose.value()->blended_positions.buffer;
  bounds_and_slots =
      bounds_and_slots &&
      mesh::ensure_mesh_geometry_bounds(geometry) == bounds &&
      bounds.value()->min->y == 0.0 && bounds.value()->max->y == 2.0 &&
      bounds.value()->max->x == 3.5 &&
      runtime->bounds_version == geometry->version;
  const double settled_version = geometry->version;
  const double settled_stamp = runtime->bounds_version;
  mesh::update_mesh_morph(carrier);
  mesh::update_mesh_morph(carrier);
  bounds_and_slots = bounds_and_slots &&
                     geometry->version == settled_version &&
                     runtime->bounds_version == settled_stamp &&
                     geometry->bounds == bounds &&
                     bind_pose.value()->blended_positions.buffer == scratch_owner;

  morph->weights.set_index(0.0, 2.0);
  mesh::update_mesh_morph(carrier);
  bounds_and_slots = bounds_and_slots &&
                     geometry->version == settled_version + 1.0 &&
                     geometry->vertices.get_index(1.0) == 8.0 &&
                     geometry->vertices.get_index(3.0) == 8.0 &&
                     runtime->morph_blended_weights.value().buffer ==
                         weight_snapshot_owner &&
                     runtime->morph_blended_weights.value().get_index(0.0) ==
                         2.0 &&
                     runtime->bounds_version == settled_stamp;
  morph->weights.set_index(0.0, 3.0);
  bounds_and_slots =
      bounds_and_slots &&
      runtime->morph_blended_weights.value().get_index(0.0) == 2.0;
  morph->weights = flight::Float32Array{3.0F, 99.0F};
  mesh::update_mesh_morph(carrier);
  bounds_and_slots = bounds_and_slots &&
                     geometry->vertices.get_index(1.0) == 12.0 &&
                     geometry->vertices.get_index(3.0) == 11.0 &&
                     runtime->morph_blended_weights.value().size() == 2 &&
                     runtime->morph_blended_weights.value().buffer !=
                         weight_snapshot_owner &&
                     runtime->morph_blended_weights.value().get_index(0.0) ==
                         3.0 &&
                     runtime->morph_blended_weights.value().get_index(1.0) ==
                         99.0;
  bounds_and_slots =
      bounds_and_slots &&
      mesh::ensure_mesh_geometry_bounds(geometry) == bounds &&
      bounds.value()->min->y == 0.0 && bounds.value()->max->y == 12.0 &&
      bounds.value()->max->x == 11.0 &&
      runtime->bounds_version == geometry->version;

  const auto null_geometry = geometry_from(
      position_layout(), flight::Float32Array{5.0F, 6.0F, 7.0F});
  const auto null_owner = null_geometry->vertices.buffer;
  const double null_version = null_geometry->version;
  const auto null_carrier = morph_carrier(
      null_geometry,
      std::variant<flight::Ref<types::MeshMorph>, flight::Null,
                   flight::Undefined>{
          std::in_place_type<flight::Null>, flight::null});
  mesh::update_mesh_morph(null_carrier);
  const auto undefined_carrier = morph_carrier(
      null_geometry,
      std::variant<flight::Ref<types::MeshMorph>, flight::Null,
                   flight::Undefined>{
          std::in_place_type<flight::Undefined>, flight::undefined});
  mesh::update_mesh_morph(undefined_carrier);
  bounds_and_slots = bounds_and_slots &&
                     null_geometry->vertices.buffer == null_owner &&
                     null_geometry->version == null_version &&
                     null_geometry->vertices.get_index(0.0) == 5.0 &&
                     !mesh::get_mesh_geometry_morph_bind_pose(null_geometry)
                          .has_value();
  if (inject_morph_bounds_mutation) bounds_and_slots = false;
  check(bounds_and_slots,
        "updateMeshMorph lazily installs one exact bind-pose owner, snapshots weights without aliasing them, invalidates but defers bounds recomputation, skips settled weights exactly, reallocates only when weight length changes, and no-ops for null or undefined morph slots");
}

void morph_validation_and_non_mutating_failures() {
  const auto geometry = geometry_from(
      position_layout(),
      flight::Float32Array{1.0F, 2.0F, 3.0F, 4.0F, 5.0F, 6.0F});
  const auto bounds = mesh::ensure_mesh_geometry_bounds(geometry);
  const auto runtime = geometry->entity_runtime_key.value();
  const auto vertex_owner = geometry->vertices.buffer;
  const double version = geometry->version;
  const double stamp = runtime->bounds_version;
  std::array<double, 6> values{};
  for (double index = 0.0; index < 6.0; index += 1.0) {
    values[static_cast<std::size_t>(index)] =
        geometry->vertices.get_index(index);
  }
  const auto invalid_pose = flight::make_ref<types::MeshMorphBindPose>();
  invalid_pose->positions =
      flight::Float32Array{1.0F, 2.0F, 3.0F, 4.0F, 5.0F, 6.0F};
  invalid_pose->blended_positions = flight::Float32Array(3.0);
  invalid_pose->normals = std::nullopt;
  invalid_pose->blended_normals = std::nullopt;
  invalid_pose->tangents = std::nullopt;
  invalid_pose->blended_tangents = std::nullopt;
  const auto target = morph_target(
      flight::Float32Array{9.0F, 9.0F, 9.0F, 9.0F, 9.0F, 9.0F});
  const auto morph = mesh_morph(
      flight::Array<flight::Ref<types::MorphTarget>>{target},
      flight::Float32Array{1.0F});
  bool threw = false;
  try {
    mesh::blend_mesh_geometry_morph(geometry, morph, invalid_pose);
  } catch (const std::range_error&) {
    threw = true;
  } catch (...) {
  }
  bool validation = threw && geometry->vertices.buffer == vertex_owner &&
                    geometry->version == version && geometry->bounds == bounds &&
                    runtime->bounds_version == stamp &&
                    target->position_deltas.get_index(0.0) == 9.0 &&
                    morph->weights.get_index(0.0) == 1.0;
  for (double index = 0.0; index < 6.0; index += 1.0) {
    validation = validation && geometry->vertices.get_index(index) ==
                                   values[static_cast<std::size_t>(index)];
  }

  const auto channel_geometry = canonical_frame_vertex(
      1.0, 2.0, 3.0, 0.0, 1.0, 0.0, 1.0, 0.0, 0.0, -1.0,
      0.25, 0.75);
  const auto channel_pose =
      mesh::capture_mesh_morph_bind_pose(channel_geometry);
  const auto position_only = mesh_morph(
      flight::Array<flight::Ref<types::MorphTarget>>{morph_target(
          flight::Float32Array{5.0F, 6.0F, 7.0F})},
      flight::Float32Array{1.0F});
  mesh::blend_mesh_geometry_morph(
      channel_geometry, position_only, channel_pose);
  validation = validation && channel_geometry->vertices.get_index(0.0) == 6.0 &&
               channel_geometry->vertices.get_index(1.0) == 8.0 &&
               channel_geometry->vertices.get_index(2.0) == 10.0 &&
               channel_geometry->vertices.get_index(3.0) == 0.0 &&
               channel_geometry->vertices.get_index(4.0) == 1.0 &&
               channel_geometry->vertices.get_index(5.0) == 0.0 &&
               channel_geometry->vertices.get_index(6.0) == 1.0 &&
               channel_geometry->vertices.get_index(7.0) == 0.0 &&
               channel_geometry->vertices.get_index(8.0) == 0.0 &&
               channel_geometry->vertices.get_index(9.0) == -1.0 &&
               channel_geometry->vertices.get_index(10.0) == 0.25 &&
               channel_geometry->vertices.get_index(11.0) == 0.75;
  if (inject_morph_validation_mutation) validation = false;
  check(validation,
        "an undersized bind-pose scratch rejects before changing geometry, bounds, cache state, targets, or weights, while absent optional normal and tangent deltas validly preserve those channels and all unrelated data");
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
  inject_transform_component_mutation =
      mutation == flight::String("--mutate-transform-components");
  inject_mirrored_transform_mutation =
      mutation == flight::String("--mutate-mirrored-transform");
  inject_transform_bounds_mutation =
      mutation == flight::String("--mutate-transform-bounds");
  inject_uv_axes_mutation =
      mutation == flight::String("--mutate-uv-axes-order");
  inject_morph_target_order_mutation =
      mutation == flight::String("--mutate-morph-target-order");
  inject_morph_weight_math_mutation =
      mutation == flight::String("--mutate-morph-weight-math");
  inject_morph_reset_mutation =
      mutation == flight::String("--mutate-morph-reset");
  inject_morph_aliasing_mutation =
      mutation == flight::String("--mutate-morph-aliasing");
  inject_morph_bounds_mutation =
      mutation == flight::String("--mutate-morph-bounds");
  inject_morph_validation_mutation =
      mutation == flight::String("--mutate-morph-validation");
  inject_format_component_mutation =
      mutation == flight::String("--mutate-format-components");
  inject_accessor_offset_stride_mutation =
      mutation == flight::String("--mutate-accessor-offset-stride");
  inject_accessor_normalization_mutation =
      mutation == flight::String("--mutate-accessor-normalization");
  inject_accessor_bounds_mutation =
      mutation == flight::String("--mutate-accessor-bounds");
  inject_accessor_missing_default_mutation =
      mutation == flight::String("--mutate-accessor-missing-defaults");
  inject_clone_aliasing_mutation =
      mutation == flight::String("--mutate-clone-aliasing");
  inject_omitted_metadata_mutation =
      mutation == flight::String("--mutate-omitted-metadata");
  inject_lifecycle_ordering_mutation =
      mutation == flight::String("--mutate-lifecycle-ordering");
  inject_reset_disposal_mutation =
      mutation == flight::String("--mutate-reset-disposal");
  inject_cache_invalidation_mutation =
      mutation == flight::String("--mutate-cache-invalidation");
  inject_sphere_math_mutation =
      mutation == flight::String("--mutate-sphere-math");
  inject_flat_split_remap_mutation =
      mutation == flight::String("--mutate-flat-split-remap");
  inject_refresh_invalidation_mutation =
      mutation == flight::String("--mutate-refresh-invalidation");
  inject_wireframe_edge_mutation =
      mutation == flight::String("--mutate-wireframe-edges");
  inject_index_promotion_mutation =
      mutation == flight::String("--mutate-index-promotion");
  inject_attribute_defaults_mutation =
      mutation == flight::String("--mutate-attribute-defaults");
  inject_attribute_count_mutation =
      mutation == flight::String("--mutate-attribute-counts");
  inject_attribute_ownership_mutation =
      mutation == flight::String("--mutate-attribute-ownership");
  inject_metadata_groups_mutation =
      mutation == flight::String("--mutate-metadata-groups");
  inject_operation_order_mutation =
      mutation == flight::String("--mutate-operation-order");
  inject_operation_rollback_mutation =
      mutation == flight::String("--mutate-operation-rollback");

  construction_clone_and_owner_identity();
  construction_defaults_metadata_and_absence_matrix();
  attribute_construction_defaults_and_missing_matrix();
  attribute_construction_count_and_layout_validation();
  attribute_construction_input_output_ownership();
  clone_aliasing_width_and_runtime_matrix();
  bounds_cache_and_position_mutation();
  runtime_lifecycle_ordering_and_idempotence();
  runtime_reset_disposal_and_missing_owner_paths();
  cache_invalidation_and_empty_bounds_lifecycle();
  bounding_sphere_exact_math_and_degenerate_cases();
  flat_normals_split_order_and_ownership();
  explicit_refresh_vs_cached_bounds();
  index_order_and_compaction();
  wireframe_edge_order_width_and_duplication();
  compaction_index_promotion_and_invalid_domains();
  failure_paths_are_non_mutating();
  weld_equivalence_and_attribute_sensitivity();
  weld_remap_order_and_index_width();
  subset_boundaries_replacement_and_rebase();
  merge_order_rebase_and_owner_isolation();
  merge_metadata_groups_and_empty_semantics();
  operation_order_across_topologies_and_sources();
  operation_failure_atomicity_and_rollback();
  validation_branch_order_and_nonmutation();
  packed_channel_order_and_ownership();
  packed_stride_offset_and_invalid_layout();
  packed_normalization_and_writes();
  vertex_format_component_table_and_storage_interpretation();
  accessor_offset_stride_interleaving_and_format_round_trip();
  accessor_normalized_integer_and_float_matrix();
  accessor_bounds_invalid_domains_and_failure_atomicity();
  accessor_missing_default_and_wrong_format_paths();
  normals_tangents_and_group_boundaries();
  layout_conversion_ownership_and_round_trip();
  transform_component_order_aliasing_and_failures();
  mirrored_transform_winding_and_handedness();
  transform_bounds_center_and_cache_invalidation();
  uv_axes_order_ownership_and_noops();
  morph_target_channel_order_and_capture_ownership();
  morph_weight_math_sparse_targets_and_limits();
  morph_accumulation_reset_and_repeated_updates();
  morph_deformation_clone_aliasing_and_bind_pose_precedence();
  morph_update_bounds_runtime_slots_and_settled_noops();
  morph_validation_and_non_mutating_failures();
  primitive_builder_topology_order_ownership_and_bounds();
  primitive_builder_seam_behavior();
  primitive_builder_parameter_guards_and_degeneracy();
  return failures;
}
