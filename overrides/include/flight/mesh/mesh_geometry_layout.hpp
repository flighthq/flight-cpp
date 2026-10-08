// Corrected Ref access and optional-index conversion for mesh geometry layout conversion.
#pragma once

#include <cmath>
#include <flight/data_view.hpp>
#include <flight/runtime.hpp>
#include <flight/mesh/mesh_geometry.hpp>
#include <flight/mesh/vertex_format.hpp>
#include <flight/types/mesh_geometry.hpp>
#include <flight/types/mesh_geometry_options.hpp>

namespace flight::mesh {

struct AttributeMapping : public flight::ReferenceEnabled {
  flight::Ref<flight::types::VertexAttribute> destination;
  flight::Ref<flight::types::VertexAttribute> source;
  double source_byte_length;
};

inline flight::Ref<flight::types::MeshGeometry> convert_mesh_geometry_layout(
    flight::Ref<flight::types::MeshGeometry> source,
    flight::Ref<flight::types::VertexAttributeLayout> target_layout) {
  const double src_stride = source->layout->stride;
  const double dst_stride = target_layout->stride;
  const double vertex_count = src_stride > 0.0
                                  ? std::floor(static_cast<double>(source->vertices.byte_length) /
                                               src_stride)
                                  : 0.0;
  flight::Float32Array dst_vertices((vertex_count * dst_stride) / 4.0);
  flight::Uint8Array source_bytes(source->vertices.buffer,
                                  static_cast<double>(source->vertices.byte_offset),
                                  static_cast<double>(source->vertices.byte_length));
  flight::Uint8Array destination_bytes(dst_vertices.buffer);
  flight::DataView source_view(source_bytes.buffer, static_cast<double>(source_bytes.byte_offset),
                               static_cast<double>(source_bytes.byte_length));
  flight::DataView destination_view(destination_bytes.buffer);
  flight::Array<flight::Ref<AttributeMapping>> mappings;

  for (const auto& destination : target_layout->attributes) {
    auto source_attribute = source->layout->attributes.find(
        [&](const auto& attribute) { return attribute->semantic == destination->semantic; });
    if (!source_attribute.has_value()) continue;
    const double source_length = get_vertex_format_byte_length(source_attribute.value()->format);
    const double destination_length = get_vertex_format_byte_length(destination->format);
    if (source_length == 0.0 || destination_length == 0.0 ||
        source_attribute.value()->byte_offset < 0.0 || destination->byte_offset < 0.0 ||
        source_attribute.value()->byte_offset + source_length > src_stride ||
        destination->byte_offset + destination_length > dst_stride) {
      continue;
    }
    mappings.push(flight::make_ref<AttributeMapping>(AttributeMapping{
        .destination = destination,
        .source = source_attribute.value(),
        .source_byte_length = source_length,
    }));
  }

  for (double vertex = 0.0; vertex < vertex_count; vertex += 1.0) {
    for (const auto& mapping : mappings) {
      const double source_offset = vertex * src_stride + mapping->source->byte_offset;
      const double destination_offset = vertex * dst_stride + mapping->destination->byte_offset;
      if (mapping->source->format == mapping->destination->format) {
        destination_bytes.set(
            source_bytes.subarray(
                static_cast<std::ptrdiff_t>(source_offset),
                static_cast<std::ptrdiff_t>(source_offset + mapping->source_byte_length)),
            static_cast<std::ptrdiff_t>(destination_offset));
        continue;
      }
      const double component_count = flight::minimum(
          get_vertex_format_component_count(mapping->source->format),
          get_vertex_format_component_count(mapping->destination->format));
      for (double component = 0.0; component < component_count; component += 1.0) {
        write_vertex_format_component(
            destination_view, destination_offset, mapping->destination->format, component,
            read_vertex_format_component(source_view, source_offset, mapping->source->format,
                                         component));
      }
    }
  }

  auto indices = std::variant<flight::Uint16Array, flight::Uint32Array, flight::Null,
                              flight::Undefined>{std::in_place_type<flight::Null>, flight::null};
  if (source->indices.has_value()) {
    std::visit(
        [&](const auto& value) {
          using Value = std::remove_cvref_t<decltype(value)>;
          indices = decltype(indices){std::in_place_type<Value>, value};
        },
        source->indices.value());
  }
  return create_mesh_geometry(flight::make_ref<flight::types::MeshGeometryOptions>(
      flight::types::MeshGeometryOptions{
          .indices = std::move(indices),
          .layout = std::move(target_layout),
          .subsets = source->subsets,
          .topology = source->topology,
          .vertices = std::move(dst_vertices),
      }));
}

inline flight::Ref<flight::types::VertexAttributeLayout> canonical_mesh_geometry_layout =
    flight::make_ref<flight::types::VertexAttributeLayout>(flight::types::VertexAttributeLayout{
        .attributes = flight::Array{
            flight::make_ref<flight::types::VertexAttribute>(flight::types::VertexAttribute{
                .byte_offset = 0.0, .format = flight::String("float32x3"),
                .semantic = flight::String("position")}),
            flight::make_ref<flight::types::VertexAttribute>(flight::types::VertexAttribute{
                .byte_offset = 12.0, .format = flight::String("float32x3"),
                .semantic = flight::String("normal")}),
            flight::make_ref<flight::types::VertexAttribute>(flight::types::VertexAttribute{
                .byte_offset = 24.0, .format = flight::String("float32x4"),
                .semantic = flight::String("tangent")}),
            flight::make_ref<flight::types::VertexAttribute>(flight::types::VertexAttribute{
                .byte_offset = 40.0, .format = flight::String("float32x2"),
                .semantic = flight::String("uv0")}),
        },
        .stride = 48.0,
    });

inline flight::Ref<flight::types::VertexAttributeLayout> canonical_skinned_mesh_geometry_layout =
    flight::make_ref<flight::types::VertexAttributeLayout>(flight::types::VertexAttributeLayout{
        .attributes = flight::Array{
            flight::make_ref<flight::types::VertexAttribute>(flight::types::VertexAttribute{
                .byte_offset = 0.0, .format = flight::String("float32x3"),
                .semantic = flight::String("position")}),
            flight::make_ref<flight::types::VertexAttribute>(flight::types::VertexAttribute{
                .byte_offset = 12.0, .format = flight::String("float32x3"),
                .semantic = flight::String("normal")}),
            flight::make_ref<flight::types::VertexAttribute>(flight::types::VertexAttribute{
                .byte_offset = 24.0, .format = flight::String("float32x4"),
                .semantic = flight::String("tangent")}),
            flight::make_ref<flight::types::VertexAttribute>(flight::types::VertexAttribute{
                .byte_offset = 40.0, .format = flight::String("float32x2"),
                .semantic = flight::String("uv0")}),
            flight::make_ref<flight::types::VertexAttribute>(flight::types::VertexAttribute{
                .byte_offset = 48.0, .format = flight::String("float32x4"),
                .semantic = flight::String("joints0")}),
            flight::make_ref<flight::types::VertexAttribute>(flight::types::VertexAttribute{
                .byte_offset = 64.0, .format = flight::String("float32x4"),
                .semantic = flight::String("weights0")}),
        },
        .stride = 80.0,
    });

} // namespace flight::mesh
