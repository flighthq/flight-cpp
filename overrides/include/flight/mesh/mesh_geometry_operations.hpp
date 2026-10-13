// Runtime completion of meshGeometryOperations over the generated optional index carrier.
#pragma once

#include <cmath>
#include <cstddef>
#include <optional>
#include <type_traits>
#include <variant>

#include <flight/mesh/mesh_geometry.hpp>
#include <flight/mesh/mesh_geometry_layout.hpp>
#include <flight/runtime.hpp>
#include <flight/types/mesh_geometry_from_attributes_options.hpp>
#include <flight/types/mesh_triangle_vertex_indices.hpp>

namespace flight::mesh {

using flight::types::MeshGeometry;
using flight::types::MeshSubset;
using flight::types::MeshTriangleVertexIndices;
using flight::types::PrimitiveTopology;
using flight::types::VertexAttributeLayout;

// These definitions live in meshGeometryCompute. Explicit declarations break its intentional
// operations/compute include cycle without changing either public signature.
inline void compute_mesh_geometry_normals(
    flight::Ref<MeshGeometry> out, flight::Ref<MeshGeometry> geometry,
    std::optional<std::optional<flight::Uint32Array>> position_groups);
inline void compute_mesh_geometry_tangents(
    flight::Ref<MeshGeometry> out, flight::Ref<MeshGeometry> geometry,
    std::optional<std::optional<flight::Uint32Array>> position_groups);
inline void refresh_mesh_geometry_bounds(flight::Ref<MeshGeometry> geometry);

namespace detail {

[[nodiscard]] inline double index_count(const flight::Ref<MeshGeometry>& geometry) {
  if (!geometry->indices.has_value()) return get_mesh_geometry_vertex_count(geometry);
  return static_cast<double>(std::visit(
      [](const auto& indices) { return indices.size(); }, geometry->indices.value()));
}

[[nodiscard]] inline double index_at(
    const flight::Ref<MeshGeometry>& geometry, double element) {
  if (!geometry->indices.has_value()) return element;
  return std::visit(
      [element](const auto& indices) { return indices.get_index(element); },
      geometry->indices.value());
}

[[nodiscard]] inline bool is_integer(double value) {
  return std::isfinite(value) && std::trunc(value) == value;
}

[[nodiscard]] inline double float32_component_count(const flight::String& format) {
  if (format == flight::String("float32")) return 1.0;
  if (format == flight::String("float32x2")) return 2.0;
  if (format == flight::String("float32x3")) return 3.0;
  if (format == flight::String("float32x4")) return 4.0;
  return 0.0;
}

[[nodiscard]] inline bool layouts_match(
    const flight::Ref<VertexAttributeLayout>& left,
    const flight::Ref<VertexAttributeLayout>& right) {
  if (left->stride != right->stride || left->attributes.size() != right->attributes.size()) {
    return false;
  }
  for (std::size_t index = 0; index < left->attributes.size(); ++index) {
    const auto& a = left->attributes[index];
    const auto& b = right->attributes[index];
    if (a->semantic != b->semantic || a->format != b->format ||
        a->byte_offset != b->byte_offset) {
      return false;
    }
  }
  return true;
}

template <typename Value>
[[nodiscard]] inline bool represented(const Value&) {
  return true;
}

[[nodiscard]] inline bool represented(const flight::Null&) { return false; }
[[nodiscard]] inline bool represented(const flight::Undefined&) { return false; }

template <typename Variant>
[[nodiscard]] inline bool variant_is_represented(const Variant& value) {
  return std::visit([](const auto& held) { return represented(held); }, value);
}

template <typename Variant>
[[nodiscard]] inline double variant_size(const Variant& value) {
  return std::visit(
      [](const auto& held) -> double {
        using Value = std::remove_cvref_t<decltype(held)>;
        if constexpr (std::same_as<Value, flight::Null> ||
                      std::same_as<Value, flight::Undefined>) {
          return 0.0;
        } else {
          return static_cast<double>(held.size());
        }
      },
      value);
}

template <typename Variant>
[[nodiscard]] inline double variant_number_at(const Variant& value, double index) {
  return std::visit(
      [index](const auto& held) -> double {
        using Value = std::remove_cvref_t<decltype(held)>;
        if constexpr (std::same_as<Value, flight::Null> ||
                      std::same_as<Value, flight::Undefined>) {
          return 0.0;
        } else if constexpr (requires { held.get_index(index); }) {
          return held.get_index(index);
        } else {
          return held.element(index);
        }
      },
      value);
}

} // namespace detail

inline double get_mesh_geometry_triangle_count(flight::Ref<MeshGeometry> geometry) {
  const double count = detail::index_count(geometry);
  if (geometry->topology == flight::String("triangle-list")) return std::floor(count / 3.0);
  if (geometry->topology == flight::String("triangle-strip")) {
    return count >= 3.0 ? count - 2.0 : 0.0;
  }
  return 0.0;
}

inline bool get_mesh_geometry_triangle_vertex_indices(
    flight::Ref<MeshTriangleVertexIndices> out,
    flight::Ref<MeshGeometry> geometry, double triangle_index) {
  if (triangle_index < 0.0 ||
      triangle_index >= get_mesh_geometry_triangle_count(geometry)) {
    return false;
  }
  double element0 = 0.0;
  double element1 = 0.0;
  double element2 = 0.0;
  if (geometry->topology == flight::String("triangle-list")) {
    element0 = triangle_index * 3.0;
    element1 = element0 + 1.0;
    element2 = element0 + 2.0;
  } else if (geometry->topology == flight::String("triangle-strip")) {
    element0 = triangle_index;
    element1 = triangle_index + 1.0;
    element2 = triangle_index + 2.0;
    if ((static_cast<std::uint32_t>(triangle_index) & 1U) != 0U) {
      std::swap(element0, element1);
    }
  } else {
    return false;
  }
  out->i0 = detail::index_at(geometry, element0);
  out->i1 = detail::index_at(geometry, element1);
  out->i2 = detail::index_at(geometry, element2);
  return true;
}

inline bool forms_whole_primitives(double count, PrimitiveTopology topology) {
  if (count == 0.0) return true;
  if (topology == flight::String("triangle-list")) return std::fmod(count, 3.0) == 0.0;
  if (topology == flight::String("triangle-strip")) return count >= 3.0;
  if (topology == flight::String("line-list")) return std::fmod(count, 2.0) == 0.0;
  if (topology == flight::String("line-strip")) return count >= 2.0;
  return topology == flight::String("point-list");
}

inline bool has_valid_draw_ranges(
    flight::Ref<MeshGeometry> geometry, double vertex_count) {
  const double element_count = geometry->indices.has_value()
                                   ? detail::index_count(geometry)
                                   : vertex_count;
  for (const auto& subset : geometry->subsets) {
    if (!detail::is_integer(subset->index_offset) ||
        !detail::is_integer(subset->index_count) || subset->index_offset < 0.0 ||
        subset->index_count < 0.0 ||
        subset->index_offset + subset->index_count > element_count ||
        !forms_whole_primitives(subset->index_count, geometry->topology)) {
      return false;
    }
  }
  return forms_whole_primitives(element_count, geometry->topology);
}

inline bool has_finite_vertex_channels(
    flight::Ref<MeshGeometry> geometry, double floats_per_vertex,
    double vertex_count) {
  for (const auto& attribute : geometry->layout->attributes) {
    if (!attribute->format.starts_with(flight::String("float32"))) continue;
    const double offset = attribute->byte_offset / 4.0;
    const double components = detail::float32_component_count(attribute->format);
    for (double vertex = 0.0; vertex < vertex_count; vertex += 1.0) {
      const double base = vertex * floats_per_vertex + offset;
      for (double component = 0.0; component < components; component += 1.0) {
        if (!std::isfinite(geometry->vertices.get_index(base + component))) return false;
      }
    }
  }
  return true;
}

inline bool validate_mesh_geometry(flight::Ref<MeshGeometry> geometry) {
  const double floats_per_vertex = geometry->layout->stride / 4.0;
  if (floats_per_vertex <= 0.0 ||
      std::fmod(static_cast<double>(geometry->vertices.size()), floats_per_vertex) != 0.0) {
    return false;
  }
  const double vertex_count =
      std::floor(static_cast<double>(geometry->vertices.size()) / floats_per_vertex);
  if (geometry->indices.has_value()) {
    const double count = detail::index_count(geometry);
    for (double index = 0.0; index < count; index += 1.0) {
      if (detail::index_at(geometry, index) >= vertex_count) return false;
    }
  }
  if (!has_finite_vertex_channels(geometry, floats_per_vertex, vertex_count)) return false;
  return has_valid_draw_ranges(geometry, vertex_count);
}

inline bool layouts_match(
    flight::Ref<VertexAttributeLayout> left,
    flight::Ref<VertexAttributeLayout> right) {
  return detail::layouts_match(left, right);
}

inline flight::Ref<MeshGeometry> create_mesh_geometry_from_attributes(
    flight::Ref<flight::types::MeshGeometryFromAttributesOptions> options) {
  constexpr double floats_per_vertex = 12.0;
  constexpr double uint16_ceiling = 65535.0;
  const double vertex_count = static_cast<double>(options->positions.size()) / 3.0;
  flight::Float32Array vertices(vertex_count * floats_per_vertex);
  const bool has_normals = detail::variant_is_represented(options->normals);
  const bool has_uvs = detail::variant_is_represented(options->uvs);
  for (double vertex = 0.0; vertex < vertex_count; vertex += 1.0) {
    const double base = vertex * floats_per_vertex;
    for (double component = 0.0; component < 3.0; component += 1.0) {
      vertices.set_index(
          base + component, options->positions.element(vertex * 3.0 + component));
      if (has_normals) {
        vertices.set_index(
            base + 3.0 + component,
            detail::variant_number_at(options->normals, vertex * 3.0 + component));
      }
    }
    if (has_uvs) {
      vertices.set_index(base + 10.0,
                         detail::variant_number_at(options->uvs, vertex * 2.0));
      vertices.set_index(base + 11.0,
                         detail::variant_number_at(options->uvs, vertex * 2.0 + 1.0));
    }
  }

  std::variant<flight::Uint16Array, flight::Uint32Array, flight::Null,
               flight::Undefined>
      indices{std::in_place_type<flight::Undefined>, flight::undefined};
  if (detail::variant_is_represented(options->indices)) {
    const double count = detail::variant_size(options->indices);
    if (vertex_count > uint16_ceiling) {
      flight::Uint32Array values(count);
      for (double index = 0.0; index < count; index += 1.0) {
        values.set_index(index, detail::variant_number_at(options->indices, index));
      }
      indices = decltype(indices){std::in_place_type<flight::Uint32Array>, values};
    } else {
      flight::Uint16Array values(count);
      for (double index = 0.0; index < count; index += 1.0) {
        values.set_index(index, detail::variant_number_at(options->indices, index));
      }
      indices = decltype(indices){std::in_place_type<flight::Uint16Array>, values};
    }
  }
  auto geometry = create_mesh_geometry_from_fields(
      std::move(indices), canonical_mesh_geometry_layout, std::nullopt,
      std::nullopt, std::move(vertices));
  if (!has_normals) {
    compute_mesh_geometry_normals(geometry, geometry, std::nullopt);
  }
  compute_mesh_geometry_tangents(geometry, geometry, std::nullopt);
  refresh_mesh_geometry_bounds(geometry);
  return geometry;
}

inline std::optional<flight::Ref<MeshGeometry>> merge_mesh_geometries(
    const flight::Array<flight::Ref<MeshGeometry>>& geometries) {
  if (geometries.empty()) return std::nullopt;
  const auto reference = geometries[0];
  const auto layout = reference->layout;
  const auto topology = reference->topology;
  for (std::size_t index = 1; index < geometries.size(); ++index) {
    if (!detail::layouts_match(layout, geometries[index]->layout) ||
        geometries[index]->topology != topology) {
      return std::nullopt;
    }
  }

  const double floats_per_vertex = layout->stride / 4.0;
  double total_vertex_floats = 0.0;
  double total_index_count = 0.0;
  bool all_indexed = true;
  for (const auto& geometry : geometries) {
    const double vertex_count = floats_per_vertex > 0.0
                                    ? std::floor(static_cast<double>(geometry->vertices.size()) /
                                                 floats_per_vertex)
                                    : 0.0;
    total_vertex_floats += vertex_count * floats_per_vertex;
    if (geometry->indices.has_value()) {
      total_index_count += detail::index_count(geometry);
    } else {
      all_indexed = false;
      total_index_count += vertex_count;
    }
  }

  flight::Float32Array merged_vertices(total_vertex_floats);
  const bool needs_uint32 =
      floats_per_vertex > 0.0 && total_vertex_floats / floats_per_vertex > 65535.0;
  flight::Uint16Array indices16(needs_uint32 ? 0.0 : total_index_count);
  flight::Uint32Array indices32(needs_uint32 ? total_index_count : 0.0);
  flight::Array<flight::Ref<MeshSubset>> subsets;
  double vertex_offset = 0.0;
  double index_offset = 0.0;
  double vertex_float_offset = 0.0;
  for (const auto& geometry : geometries) {
    const double vertex_count = floats_per_vertex > 0.0
                                    ? std::floor(static_cast<double>(geometry->vertices.size()) /
                                                 floats_per_vertex)
                                    : 0.0;
    merged_vertices.set(
        geometry->vertices.subarray(0, static_cast<std::ptrdiff_t>(
                                           vertex_count * floats_per_vertex)),
        static_cast<std::ptrdiff_t>(vertex_float_offset));
    const double source_count = geometry->indices.has_value()
                                    ? detail::index_count(geometry)
                                    : vertex_count;
    for (double index = 0.0; index < source_count; index += 1.0) {
      const double value = detail::index_at(geometry, index) + vertex_offset;
      if (needs_uint32) indices32.set_index(index_offset + index, value);
      else indices16.set_index(index_offset + index, value);
    }
    for (const auto& subset : geometry->subsets) {
      subsets.push(flight::make_ref<MeshSubset>(MeshSubset{
          .index_count = subset->index_count,
          .index_offset = subset->index_offset + index_offset,
      }));
    }
    index_offset += source_count;
    vertex_offset += vertex_count;
    vertex_float_offset += vertex_count * floats_per_vertex;
  }
  if (subsets.empty()) {
    subsets.push(flight::make_ref<MeshSubset>(MeshSubset{
        .index_count = total_index_count, .index_offset = 0.0}));
  }
  std::variant<flight::Uint16Array, flight::Uint32Array,
               flight::Null, flight::Undefined>
      index_variant{std::in_place_type<flight::Undefined>, flight::undefined};
  if (all_indexed || total_index_count > 0.0) {
    index_variant = needs_uint32
                        ? decltype(index_variant){
                              std::in_place_type<flight::Uint32Array>, indices32}
                        : decltype(index_variant){
                              std::in_place_type<flight::Uint16Array>, indices16};
  }
  auto merged = create_mesh_geometry_from_fields(
      std::move(index_variant), layout, std::optional{subsets},
      std::optional<PrimitiveTopology>{topology}, std::move(merged_vertices));
  refresh_mesh_geometry_bounds(merged);
  return merged;
}

} // namespace flight::mesh
