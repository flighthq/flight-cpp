// Exact CPU mesh computations over the generated Ref and optional-index carriers.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <type_traits>
#include <unordered_map>
#include <variant>
#include <vector>

#include <flight/geometry/aabb.hpp>
#include <flight/mesh/mesh_geometry.hpp>
#include <flight/mesh/mesh_geometry_operations.hpp>
#include <flight/runtime.hpp>
#include <flight/types/aabb.hpp>
#include <flight/types/bounding_sphere.hpp>
#include <flight/types/mesh_triangle_vertex_indices.hpp>

namespace flight::mesh {

namespace compute_detail {

constexpr double position_offset = 0.0;
constexpr double normal_offset = 3.0;
constexpr double tangent_offset = 6.0;
constexpr double uv0_offset = 10.0;

[[nodiscard]] inline const flight::Uint32Array* groups_if_present(
    const std::optional<std::optional<flight::Uint32Array>>& groups) {
  return groups.has_value() && groups.value().has_value()
             ? &groups.value().value()
             : nullptr;
}

inline void accumulate(
    flight::Float64Array values, double vertex, double x, double y, double z) {
  const double base = vertex * 3.0;
  values.set_index(base, values.get_index(base) + x);
  values.set_index(base + 1.0, values.get_index(base + 1.0) + y);
  values.set_index(base + 2.0, values.get_index(base + 2.0) + z);
}

inline void accumulate_tangent(
    flight::Float64Array tangents, flight::Float64Array bitangents,
    double vertex, double tx, double ty, double tz,
    double bx, double by, double bz) {
  accumulate(tangents, vertex, tx, ty, tz);
  accumulate(bitangents, vertex, bx, by, bz);
}

inline void write_tangent(
    flight::Float32Array vertices, double stride, double output_vertex,
    flight::Float64Array tangents, flight::Float64Array bitangents,
    double accumulated_vertex, double tangent_sign) {
  const double normal = output_vertex * stride + normal_offset;
  const double nx = vertices.get_index(normal);
  const double ny = vertices.get_index(normal + 1.0);
  const double nz = vertices.get_index(normal + 2.0);
  double tx = tangents.get_index(accumulated_vertex * 3.0) * tangent_sign;
  double ty = tangents.get_index(accumulated_vertex * 3.0 + 1.0) * tangent_sign;
  double tz = tangents.get_index(accumulated_vertex * 3.0 + 2.0) * tangent_sign;
  const double dot = nx * tx + ny * ty + nz * tz;
  tx -= nx * dot;
  ty -= ny * dot;
  tz -= nz * dot;
  double length = std::sqrt(tx * tx + ty * ty + tz * tz);
  if (length > 0.0) {
    tx /= length;
    ty /= length;
    tz /= length;
  } else {
    if (std::abs(nx) <= std::abs(ny) && std::abs(nx) <= std::abs(nz)) {
      tx = 0.0;
      ty = nz;
      tz = -ny;
    } else if (std::abs(ny) <= std::abs(nz)) {
      tx = -nz;
      ty = 0.0;
      tz = nx;
    } else {
      tx = ny;
      ty = -nx;
      tz = 0.0;
    }
    length = std::sqrt(tx * tx + ty * ty + tz * tz);
    if (length > 0.0) {
      tx /= length;
      ty /= length;
      tz /= length;
    } else {
      tx = 1.0;
      ty = 0.0;
      tz = 0.0;
    }
  }
  const double cross_x = ny * tz - nz * ty;
  const double cross_y = nz * tx - nx * tz;
  const double cross_z = nx * ty - ny * tx;
  const double handedness =
      cross_x * bitangents.get_index(accumulated_vertex * 3.0) +
                  cross_y * bitangents.get_index(accumulated_vertex * 3.0 + 1.0) +
                  cross_z * bitangents.get_index(accumulated_vertex * 3.0 + 2.0) <
              0.0
          ? -1.0
          : 1.0;
  const double tangent = output_vertex * stride + tangent_offset;
  vertices.set_index(tangent, tx);
  vertices.set_index(tangent + 1.0, ty);
  vertices.set_index(tangent + 2.0, tz);
  vertices.set_index(tangent + 3.0, handedness);
}

template <typename Target>
inline void write_bounds(Target& target, const flight::Ref<MeshGeometry>& geometry) {
  const double stride = geometry->layout->stride / 4.0;
  const double count = stride > 0.0
                           ? std::floor(static_cast<double>(geometry->vertices.size()) / stride)
                           : 0.0;
  double min_x = std::numeric_limits<double>::infinity();
  double min_y = std::numeric_limits<double>::infinity();
  double min_z = std::numeric_limits<double>::infinity();
  double max_x = -std::numeric_limits<double>::infinity();
  double max_y = -std::numeric_limits<double>::infinity();
  double max_z = -std::numeric_limits<double>::infinity();
  for (double vertex = 0.0; vertex < count; vertex += 1.0) {
    const double base = vertex * stride;
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
  target->min->x = min_x;
  target->min->y = min_y;
  target->min->z = min_z;
  target->max->x = max_x;
  target->max->y = max_y;
  target->max->z = max_z;
}

} // namespace compute_detail

template <typename Bounds>
inline void compute_mesh_geometry_bounds(
    std::shared_ptr<Bounds> out, flight::Ref<MeshGeometry> geometry) {
  compute_detail::write_bounds(out, geometry);
}

inline void compute_mesh_geometry_bounding_sphere(
    flight::types::BoundingSphereLike out,
    flight::Ref<MeshGeometry> geometry) {
  const double stride = geometry->layout->stride / 4.0;
  const double count = stride > 0.0
                           ? std::floor(static_cast<double>(geometry->vertices.size()) / stride)
                           : 0.0;
  if (count == 0.0) {
    out->center->x = 0.0;
    out->center->y = 0.0;
    out->center->z = 0.0;
    out->radius = -1.0;
    return;
  }
  auto bounds = flight::geometry::create_aabb(
      std::nullopt, std::nullopt, std::nullopt,
      std::nullopt, std::nullopt, std::nullopt);
  compute_mesh_geometry_bounds(bounds, geometry);
  const double center_x = (bounds->min->x + bounds->max->x) * 0.5;
  const double center_y = (bounds->min->y + bounds->max->y) * 0.5;
  const double center_z = (bounds->min->z + bounds->max->z) * 0.5;
  double radius_squared = 0.0;
  for (double vertex = 0.0; vertex < count; vertex += 1.0) {
    const double base = vertex * stride;
    const double dx = geometry->vertices.get_index(base) - center_x;
    const double dy = geometry->vertices.get_index(base + 1.0) - center_y;
    const double dz = geometry->vertices.get_index(base + 2.0) - center_z;
    radius_squared = std::max(radius_squared, dx * dx + dy * dy + dz * dz);
  }
  out->center->x = center_x;
  out->center->y = center_y;
  out->center->z = center_z;
  out->radius = std::sqrt(radius_squared);
}

inline void compute_mesh_geometry_flat_normals(
    flight::Ref<MeshGeometry> out, flight::Ref<MeshGeometry> geometry) {
  const auto source = geometry->vertices;
  const double stride = geometry->layout->stride / 4.0;
  const double triangles = get_mesh_geometry_triangle_count(geometry);
  auto corner = flight::make_ref<MeshTriangleVertexIndices>(
      MeshTriangleVertexIndices{.i0 = 0.0, .i1 = 0.0, .i2 = 0.0});
  for (double triangle = 0.0; triangle < triangles; triangle += 1.0) {
    if (!get_mesh_geometry_triangle_vertex_indices(corner, geometry, triangle)) continue;
    const double p0 = corner->i0 * stride;
    const double p1 = corner->i1 * stride;
    const double p2 = corner->i2 * stride;
    const double e1x = source.get_index(p1) - source.get_index(p0);
    const double e1y = source.get_index(p1 + 1.0) - source.get_index(p0 + 1.0);
    const double e1z = source.get_index(p1 + 2.0) - source.get_index(p0 + 2.0);
    const double e2x = source.get_index(p2) - source.get_index(p0);
    const double e2y = source.get_index(p2 + 1.0) - source.get_index(p0 + 1.0);
    const double e2z = source.get_index(p2 + 2.0) - source.get_index(p0 + 2.0);
    double nx = e1y * e2z - e1z * e2y;
    double ny = e1z * e2x - e1x * e2z;
    double nz = e1x * e2y - e1y * e2x;
    const double length = std::sqrt(nx * nx + ny * ny + nz * nz);
    if (length > 0.0) {
      nx /= length;
      ny /= length;
      nz /= length;
    }
    for (const double vertex : {corner->i0, corner->i1, corner->i2}) {
      const double normal = vertex * stride + compute_detail::normal_offset;
      out->vertices.set_index(normal, nx);
      out->vertices.set_index(normal + 1.0, ny);
      out->vertices.set_index(normal + 2.0, nz);
    }
  }
  out->version += 1.0;
}

inline void compute_mesh_geometry_normals(
    flight::Ref<MeshGeometry> out, flight::Ref<MeshGeometry> geometry,
    std::optional<std::optional<flight::Uint32Array>> position_groups = std::nullopt) {
  const auto vertices = geometry->vertices;
  const double stride = geometry->layout->stride / 4.0;
  const double vertex_count = stride > 0.0
                                  ? std::floor(static_cast<double>(vertices.size()) / stride)
                                  : 0.0;
  flight::Float64Array accum(vertex_count * 3.0);
  const auto* groups = compute_detail::groups_if_present(position_groups);
  auto grouped = [&](double vertex) {
    return groups ? groups->get_index(vertex) : vertex;
  };
  auto corner = flight::make_ref<MeshTriangleVertexIndices>(
      MeshTriangleVertexIndices{.i0 = 0.0, .i1 = 0.0, .i2 = 0.0});
  const double triangle_count = get_mesh_geometry_triangle_count(geometry);
  for (double triangle = 0.0; triangle < triangle_count; triangle += 1.0) {
    if (!get_mesh_geometry_triangle_vertex_indices(corner, geometry, triangle)) continue;
    const double b0 = corner->i0 * stride;
    const double b1 = corner->i1 * stride;
    const double b2 = corner->i2 * stride;
    const double e1x = vertices.get_index(b1) - vertices.get_index(b0);
    const double e1y = vertices.get_index(b1 + 1.0) - vertices.get_index(b0 + 1.0);
    const double e1z = vertices.get_index(b1 + 2.0) - vertices.get_index(b0 + 2.0);
    const double e2x = vertices.get_index(b2) - vertices.get_index(b0);
    const double e2y = vertices.get_index(b2 + 1.0) - vertices.get_index(b0 + 1.0);
    const double e2z = vertices.get_index(b2 + 2.0) - vertices.get_index(b0 + 2.0);
    const double nx = e1y * e2z - e1z * e2y;
    const double ny = e1z * e2x - e1x * e2z;
    const double nz = e1x * e2y - e1y * e2x;
    compute_detail::accumulate(accum, grouped(corner->i0), nx, ny, nz);
    compute_detail::accumulate(accum, grouped(corner->i1), nx, ny, nz);
    compute_detail::accumulate(accum, grouped(corner->i2), nx, ny, nz);
  }
  for (double vertex = 0.0; vertex < vertex_count; vertex += 1.0) {
    const double accumulated = grouped(vertex) * 3.0;
    double nx = accum.get_index(accumulated);
    double ny = accum.get_index(accumulated + 1.0);
    double nz = accum.get_index(accumulated + 2.0);
    const double length = std::sqrt(nx * nx + ny * ny + nz * nz);
    if (length > 0.0) {
      nx /= length;
      ny /= length;
      nz /= length;
    }
    const double normal = vertex * stride + compute_detail::normal_offset;
    out->vertices.set_index(normal, nx);
    out->vertices.set_index(normal + 1.0, ny);
    out->vertices.set_index(normal + 2.0, nz);
  }
  out->version += 1.0;
}

inline flight::Uint32Array compute_mesh_geometry_position_groups(
    flight::Ref<MeshGeometry> geometry) {
  const double stride = geometry->layout->stride / 4.0;
  const double count = stride > 0.0
                           ? std::floor(static_cast<double>(geometry->vertices.size()) / stride)
                           : 0.0;
  flight::Uint32Array groups(count);
  flight::Uint32Array bits(
      geometry->vertices.buffer, static_cast<double>(geometry->vertices.byte_offset),
      static_cast<double>(geometry->vertices.size()));
  std::unordered_map<std::uint32_t, std::vector<std::uint32_t>> buckets;
  for (std::uint32_t vertex = 0; vertex < static_cast<std::uint32_t>(count); ++vertex) {
    const double position = static_cast<double>(vertex) * stride;
    const auto x = static_cast<std::uint32_t>(bits.get_index(position));
    const auto y = static_cast<std::uint32_t>(bits.get_index(position + 1.0));
    const auto z = static_cast<std::uint32_t>(bits.get_index(position + 2.0));
    std::uint32_t hash = 2166136261U;
    hash = (hash ^ x) * 16777619U;
    hash = (hash ^ y) * 16777619U;
    hash = (hash ^ z) * 16777619U;
    auto& bucket = buckets[hash];
    std::uint32_t representative = vertex;
    for (const auto candidate : bucket) {
      const double other = static_cast<double>(candidate) * stride;
      if (bits.get_index(position) == bits.get_index(other) &&
          bits.get_index(position + 1.0) == bits.get_index(other + 1.0) &&
          bits.get_index(position + 2.0) == bits.get_index(other + 2.0)) {
        representative = candidate;
        break;
      }
    }
    groups.set_index(static_cast<double>(vertex), static_cast<double>(representative));
    if (representative == vertex) bucket.push_back(vertex);
  }
  return groups;
}

inline void compute_mesh_geometry_tangents(
    flight::Ref<MeshGeometry> out, flight::Ref<MeshGeometry> geometry,
    std::optional<std::optional<flight::Uint32Array>> position_groups = std::nullopt) {
  const auto source_vertices = geometry->vertices;
  const double source_stride = geometry->layout->stride / 4.0;
  const double source_vertex_count =
      source_stride > 0.0
          ? std::floor(static_cast<double>(source_vertices.size()) / source_stride)
          : 0.0;
  const auto source_indices = geometry->indices;
  const double element_count = source_indices.has_value()
                                   ? static_cast<double>(std::visit(
                                         [](const auto& value) { return value.size(); },
                                         source_indices.value()))
                                   : source_vertex_count;
  auto source_index = [&](double element) {
    if (!source_indices.has_value()) return element;
    return std::visit(
        [element](const auto& value) { return value.get_index(element); },
        source_indices.value());
  };
  const auto* groups = compute_detail::groups_if_present(position_groups);
  const double triangle_count = get_mesh_geometry_triangle_count(geometry);
  flight::Uint32Array corners(triangle_count * 3.0);
  auto corner = flight::make_ref<MeshTriangleVertexIndices>(
      MeshTriangleVertexIndices{.i0 = 0.0, .i1 = 0.0, .i2 = 0.0});
  for (double triangle = 0.0; triangle < triangle_count; triangle += 1.0) {
    if (!get_mesh_geometry_triangle_vertex_indices(corner, geometry, triangle)) continue;
    corners.set_index(triangle * 3.0, corner->i0);
    corners.set_index(triangle * 3.0 + 1.0, corner->i1);
    corners.set_index(triangle * 3.0 + 2.0, corner->i2);
  }

  std::vector<int> triangle_signs(static_cast<std::size_t>(triangle_count), 0);
  std::vector<int> orientation(static_cast<std::size_t>(source_vertex_count), 0);
  double split_count = 0.0;
  if (source_indices.has_value() || groups) {
    for (double triangle = 0.0; triangle < triangle_count; triangle += 1.0) {
      const double i0 = corners.get_index(triangle * 3.0);
      const double i1 = corners.get_index(triangle * 3.0 + 1.0);
      const double i2 = corners.get_index(triangle * 3.0 + 2.0);
      const double uv0 = i0 * source_stride + compute_detail::uv0_offset;
      const double uv1 = i1 * source_stride + compute_detail::uv0_offset;
      const double uv2 = i2 * source_stride + compute_detail::uv0_offset;
      const double du1 = source_vertices.get_index(uv1) - source_vertices.get_index(uv0);
      const double dv1 = source_vertices.get_index(uv1 + 1.0) - source_vertices.get_index(uv0 + 1.0);
      const double du2 = source_vertices.get_index(uv2) - source_vertices.get_index(uv0);
      const double dv2 = source_vertices.get_index(uv2 + 1.0) - source_vertices.get_index(uv0 + 1.0);
      const double determinant = du1 * dv2 - du2 * dv1;
      const int sign = determinant < 0.0 ? -1 : determinant > 0.0 ? 1 : 0;
      triangle_signs[static_cast<std::size_t>(triangle)] = sign;
      if (sign == 0) continue;
      for (const double vertex : {i0, i1, i2}) {
        auto& state = orientation[static_cast<std::size_t>(vertex)];
        if (state == 0) state = sign;
        else if ((state < 0 ? -1 : 1) != sign) state = state < 0 ? -2 : 2;
      }
    }
    if (source_indices.has_value()) {
      for (const auto state : orientation) if (std::abs(state) == 2) split_count += 1.0;
    }
  }

  const double target_stride = out->layout->stride / 4.0;
  auto target_vertices = out->vertices;
  const double output_vertex_count = source_vertex_count + split_count;
  std::vector<std::uint32_t> remapped(static_cast<std::size_t>(element_count));
  bool did_remap = false;
  if (geometry->topology == flight::String("triangle-list") && split_count > 0.0 &&
      source_indices.has_value()) {
    std::vector<std::uint32_t> split_vertices(
        static_cast<std::size_t>(source_vertex_count), 0xffffffffU);
    flight::Float32Array expanded(output_vertex_count * target_stride);
    expanded.set(target_vertices.subarray(
        0, static_cast<std::ptrdiff_t>(source_vertex_count * target_stride)));
    double next = source_vertex_count;
    for (double vertex = 0.0; vertex < source_vertex_count; vertex += 1.0) {
      if (std::abs(orientation[static_cast<std::size_t>(vertex)]) != 2) continue;
      split_vertices[static_cast<std::size_t>(vertex)] = static_cast<std::uint32_t>(next);
      expanded.set(
          target_vertices.subarray(
              static_cast<std::ptrdiff_t>(vertex * target_stride),
              static_cast<std::ptrdiff_t>((vertex + 1.0) * target_stride)),
          static_cast<std::ptrdiff_t>(next * target_stride));
      next += 1.0;
    }
    target_vertices = expanded;
    out->vertices = expanded;
    const bool needs_uint32 = source_indices->index() == 1 ||
                              (out->indices.has_value() && out->indices->index() == 1) ||
                              output_vertex_count > 65535.0;
    flight::Uint16Array mapped16(needs_uint32 ? 0.0 : element_count);
    flight::Uint32Array mapped32(needs_uint32 ? element_count : 0.0);
    for (double element = 0.0; element < element_count; element += 1.0) {
      const auto source_vertex = static_cast<std::uint32_t>(source_index(element));
      const int sign = triangle_signs[static_cast<std::size_t>(std::floor(element / 3.0))];
      const int primary = orientation[source_vertex] < 0 ? -1 : 1;
      const auto output = sign != 0 && sign != primary
                              ? split_vertices[source_vertex]
                              : source_vertex;
      remapped[static_cast<std::size_t>(element)] = output;
      if (needs_uint32) mapped32.set_index(element, output);
      else mapped16.set_index(element, output);
    }
    out->indices = needs_uint32
                       ? std::optional<std::variant<flight::Uint16Array, flight::Uint32Array>>{
                             std::in_place, std::in_place_type<flight::Uint32Array>, mapped32}
                       : std::optional<std::variant<flight::Uint16Array, flight::Uint32Array>>{
                             std::in_place, std::in_place_type<flight::Uint16Array>, mapped16};
    did_remap = true;
  }

  flight::Float64Array tangents(output_vertex_count * 3.0);
  flight::Float64Array bitangents(output_vertex_count * 3.0);
  for (double triangle = 0.0; triangle < triangle_count; triangle += 1.0) {
    const double i0 = corners.get_index(triangle * 3.0);
    const double i1 = corners.get_index(triangle * 3.0 + 1.0);
    const double i2 = corners.get_index(triangle * 3.0 + 2.0);
    const std::size_t element = static_cast<std::size_t>(triangle * 3.0);
    const double o0 = did_remap ? remapped[element] : i0;
    const double o1 = did_remap ? remapped[element + 1] : i1;
    const double o2 = did_remap ? remapped[element + 2] : i2;
    const double p0 = i0 * source_stride;
    const double p1 = i1 * source_stride;
    const double p2 = i2 * source_stride;
    const double e1x = source_vertices.get_index(p1) - source_vertices.get_index(p0);
    const double e1y = source_vertices.get_index(p1 + 1.0) - source_vertices.get_index(p0 + 1.0);
    const double e1z = source_vertices.get_index(p1 + 2.0) - source_vertices.get_index(p0 + 2.0);
    const double e2x = source_vertices.get_index(p2) - source_vertices.get_index(p0);
    const double e2y = source_vertices.get_index(p2 + 1.0) - source_vertices.get_index(p0 + 1.0);
    const double e2z = source_vertices.get_index(p2 + 2.0) - source_vertices.get_index(p0 + 2.0);
    const double uv0 = i0 * source_stride + compute_detail::uv0_offset;
    const double uv1 = i1 * source_stride + compute_detail::uv0_offset;
    const double uv2 = i2 * source_stride + compute_detail::uv0_offset;
    const double du1 = source_vertices.get_index(uv1) - source_vertices.get_index(uv0);
    const double dv1 = source_vertices.get_index(uv1 + 1.0) - source_vertices.get_index(uv0 + 1.0);
    const double du2 = source_vertices.get_index(uv2) - source_vertices.get_index(uv0);
    const double dv2 = source_vertices.get_index(uv2 + 1.0) - source_vertices.get_index(uv0 + 1.0);
    const double determinant = du1 * dv2 - du2 * dv1;
    const double reciprocal = determinant != 0.0 ? 1.0 / determinant : 0.0;
    const double tx = (dv2 * e1x - dv1 * e2x) * reciprocal;
    const double ty = (dv2 * e1y - dv1 * e2y) * reciprocal;
    const double tz = (dv2 * e1z - dv1 * e2z) * reciprocal;
    const double bx = (du1 * e2x - du2 * e1x) * reciprocal;
    const double by = (du1 * e2y - du2 * e1y) * reciprocal;
    const double bz = (du1 * e2z - du2 * e1z) * reciprocal;
    const double sign = groups && determinant < 0.0 ? -1.0 : 1.0;
    const double a0 = groups ? groups->get_index(i0) : o0;
    const double a1 = groups ? groups->get_index(i1) : o1;
    const double a2 = groups ? groups->get_index(i2) : o2;
    compute_detail::accumulate_tangent(tangents, bitangents, a0, tx * sign, ty * sign, tz * sign, bx, by, bz);
    compute_detail::accumulate_tangent(tangents, bitangents, a1, tx * sign, ty * sign, tz * sign, bx, by, bz);
    compute_detail::accumulate_tangent(tangents, bitangents, a2, tx * sign, ty * sign, tz * sign, bx, by, bz);
  }
  if (!groups) {
    for (double output = 0.0; output < output_vertex_count; output += 1.0) {
      compute_detail::write_tangent(target_vertices, target_stride, output, tangents, bitangents, output, 1.0);
    }
  } else {
    for (double source = 0.0; source < source_vertex_count; source += 1.0) {
      const double sign = orientation[static_cast<std::size_t>(source)] < 0 ? -1.0 : 1.0;
      compute_detail::write_tangent(target_vertices, target_stride, source, tangents, bitangents,
                                    groups->get_index(source), sign);
    }
    if (did_remap) {
      for (double element = 0.0; element < element_count; element += 1.0) {
        const double output = remapped[static_cast<std::size_t>(element)];
        if (output < source_vertex_count) continue;
        const double source = source_index(element);
        const double sign = triangle_signs[static_cast<std::size_t>(std::floor(element / 3.0))] < 0 ? -1.0 : 1.0;
        compute_detail::write_tangent(target_vertices, target_stride, output, tangents, bitangents,
                                      groups->get_index(source), sign);
      }
    }
  }
  out->version += 1.0;
}

inline void refresh_mesh_geometry_bounds(flight::Ref<MeshGeometry> geometry) {
  if (!geometry->bounds.has_value()) {
    geometry->bounds = flight::geometry::create_aabb(
        std::nullopt, std::nullopt, std::nullopt,
        std::nullopt, std::nullopt, std::nullopt);
  }
  compute_mesh_geometry_bounds(geometry->bounds.value(), geometry);
  const auto runtime = get_mesh_geometry_runtime_if_present(geometry);
  if (runtime.has_value()) runtime.value()->bounds_version = geometry->version;
}

inline std::optional<flight::Ref<flight::types::Aabb>> ensure_mesh_geometry_bounds(
    flight::Ref<MeshGeometry> geometry) {
  const auto runtime = get_mesh_geometry_runtime_if_present(geometry);
  if (!geometry->bounds.has_value() || !runtime.has_value() ||
      runtime.value()->bounds_version != geometry->version) {
    refresh_mesh_geometry_bounds(geometry);
  }
  if (!geometry->bounds.has_value() ||
      geometry->bounds.value()->min->x > geometry->bounds.value()->max->x) {
    return std::nullopt;
  }
  return geometry->bounds;
}

} // namespace flight::mesh
