// Runtime completion of scene3d/src/mesh.ts.
#pragma once

#include <optional>
#include <stdexcept>
#include <variant>

#include <flight/any.hpp>
#include <flight/mesh/mesh_geometry_deformation_clone.hpp>
#include <flight/node/node.hpp>
#include <flight/runtime.hpp>
#include <flight/scene3d/scene_node.hpp>
#include <flight/structural_ref.hpp>
#include <flight/types/material.hpp>
#include <flight/types/mesh.hpp>
#include <flight/types/mesh_deformer.hpp>
#include <flight/types/node_signals.hpp>

namespace flight::scene3d {

struct enabled_name_f1abece853ac935c : public flight::ReferenceEnabled {
  std::optional<bool> enabled;
  std::variant<flight::String, flight::Null, flight::Undefined> name =
      std::variant<flight::String, flight::Null, flight::Undefined>{
          std::in_place_type<flight::Undefined>, flight::undefined};
};

inline flight::Ref<flight::types::MeshRuntime> create_mesh_runtime() {
  auto runtime = flight::make_ref<flight::types::MeshRuntime>();
  initialize_node3_druntime<flight::types::MeshRuntime>(runtime);
  runtime->deformed_local_bounds =
      std::variant<flight::Ref<flight::types::Aabb>, flight::Null,
                   flight::Undefined>{
          std::in_place_type<flight::Undefined>, flight::undefined};
  return runtime;
}

inline flight::Ref<flight::types::Mesh> create_mesh(
    flight::Ref<flight::types::MeshGeometry> geometry,
    flight::Array<std::optional<flight::Ref<flight::types::Material>>> materials,
    std::optional<flight::String> kind = std::nullopt,
    std::optional<flight::Ref<enabled_name_f1abece853ac935c>> options =
        std::nullopt) {
  auto runtime = create_mesh_runtime();
  auto base_runtime = std::reinterpret_pointer_cast<Node3DBaseRuntime>(runtime);
  return flight::make_ref<flight::types::Mesh>(flight::types::Mesh{
      .data = std::nullopt,
      .enabled = detail::option_enabled(options).value_or(true),
      .kind = kind.value_or(flight::types::mesh_kind),
      .name = detail::option_name(options),
      .entity_runtime_key = std::move(base_runtime),
      .alpha = 1.0,
      .visible = true,
      .position = flight::geometry::create_vector3(
          std::nullopt, std::nullopt, std::nullopt),
      .rotation = flight::geometry::create_quaternion(
          std::nullopt, std::nullopt, std::nullopt, std::nullopt),
      .scale = flight::geometry::create_vector3(1.0, 1.0, 1.0),
      .geometry = std::move(geometry),
      .materials = std::move(materials),
      .morph = std::variant<flight::Ref<flight::types::MeshMorph>, flight::Null,
                            flight::Undefined>{
          std::in_place_type<flight::Undefined>, flight::undefined},
      .skin = std::variant<flight::Ref<flight::types::Skin>, flight::Null,
                           flight::Undefined>{
          std::in_place_type<flight::Undefined>, flight::undefined},
  });
}

inline flight::Ref<flight::types::MeshRuntime> get_mesh_runtime(
    const flight::Ref<flight::types::Mesh>& source) {
  if (!source->entity_runtime_key.has_value() ||
      !source->entity_runtime_key.value()) {
    throw std::logic_error("Mesh has no runtime owner");
  }
  return std::reinterpret_pointer_cast<flight::types::MeshRuntime>(
      source->entity_runtime_key.value());
}

template <typename Schema>
inline flight::Ref<flight::types::MeshRuntime> get_mesh_runtime(
    const flight::StructuralRef<Schema>& source) {
  const auto owner = source.shared_object();
  if (!owner) throw std::logic_error("Mesh structural view has no owner");
  return get_mesh_runtime(owner);
}

inline flight::Ref<flight::types::Mesh> clone_mesh(
    const flight::Ref<flight::types::Mesh>& source) {
  const bool has_skin =
      std::holds_alternative<flight::Ref<flight::types::Skin>>(source->skin);
  const bool has_morph =
      std::holds_alternative<flight::Ref<flight::types::MeshMorph>>(
          source->morph);
  auto geometry = (has_skin || has_morph)
                      ? flight::mesh::clone_mesh_geometry_for_deformation(
                            source->geometry)
                      : source->geometry;
  auto options = flight::make_ref<enabled_name_f1abece853ac935c>();
  options->enabled = source->enabled;
  options->name = source->name.has_value()
                      ? decltype(options->name){
                            std::in_place_type<flight::String>,
                            source->name.value()}
                      : decltype(options->name){
                            std::in_place_type<flight::Null>, flight::null};
  auto clone = create_mesh(geometry, source->materials.slice(), source->kind,
                           options);
  clone->alpha = source->alpha;
  clone->position->x = source->position->x;
  clone->position->y = source->position->y;
  clone->position->z = source->position->z;
  clone->rotation->x = source->rotation->x;
  clone->rotation->y = source->rotation->y;
  clone->rotation->z = source->rotation->z;
  clone->rotation->w = source->rotation->w;
  clone->scale->x = source->scale->x;
  clone->scale->y = source->scale->y;
  clone->scale->z = source->scale->z;
  auto clone_runtime = get_mesh_runtime(clone);
  ++clone_runtime->local_transform_id;

  const auto source_runtime = get_mesh_runtime(source);
  if (source_runtime->local_matrix4_detached &&
      source_runtime->local_matrix4.has_value()) {
    const auto& matrix = source_runtime->local_matrix4.value();
    flight::Float32Array values(static_cast<double>(matrix->m.size()));
    values.set(matrix->m);
    clone_runtime->local_matrix4 =
        flight::make_ref<flight::types::Matrix4>(flight::types::Matrix4{
            .entity_runtime_key = std::nullopt, .m = std::move(values)});
    ++clone_runtime->local_transform_id;
    clone_runtime->local_transform_using_local_transform_id =
        clone_runtime->local_transform_id;
    clone_runtime->local_matrix4_detached = true;
  }

  if (has_skin) clone->skin = source->skin;
  if (has_morph) {
    const auto& source_morph =
        std::get<flight::Ref<flight::types::MeshMorph>>(source->morph);
    flight::Float32Array weights(
        static_cast<double>(source_morph->weights.size()));
    weights.set(source_morph->weights);
    clone->morph = flight::make_ref<flight::types::MeshMorph>(
        flight::types::MeshMorph{.targets = source_morph->targets,
                                 .weights = std::move(weights)});
  }
  return clone;
}

template <typename Schema>
inline flight::Ref<flight::types::Mesh> clone_mesh(
    const flight::StructuralRef<Schema>& source) {
  const auto owner = source.shared_object();
  if (!owner) throw std::logic_error("Mesh structural view has no owner");
  return clone_mesh(owner);
}

inline flight::Ref<flight::types::NodeSignals> enable_mesh_signals(
    const flight::Ref<flight::types::Mesh>& source) {
  auto runtime = get_mesh_runtime(source);
  if (!runtime->node_signals.has_value()) {
    runtime->node_signals = flight::node::create_node_signals();
  }
  return runtime->node_signals.value();
}

inline std::optional<flight::Ref<flight::types::NodeSignals>> get_mesh_signals(
    const flight::Ref<flight::types::Mesh>& source) {
  return get_mesh_runtime(source)->node_signals;
}

inline flight::types::MeshDeformer get_mesh_deformer(
    const flight::Ref<flight::types::Mesh>& source) {
  if (std::holds_alternative<flight::Ref<flight::types::Skin>>(source->skin)) {
    return flight::types::mesh_deformer_skeletal;
  }
  if (std::holds_alternative<flight::Ref<flight::types::MeshMorph>>(
          source->morph)) {
    return flight::types::mesh_deformer_morph;
  }
  return flight::types::mesh_deformer_none;
}

template <typename Schema>
inline flight::types::MeshDeformer get_mesh_deformer(
    const flight::StructuralRef<Schema>& source) {
  const auto owner = source.shared_object();
  if (!owner) throw std::logic_error("Mesh structural view has no owner");
  return get_mesh_deformer(owner);
}

inline bool is_mesh(const flight::Ref<flight::types::Mesh>& source) {
  return static_cast<bool>(source) && static_cast<bool>(source->geometry);
}

inline bool is_mesh(const flight::Any& source) {
  const auto object = source.object_if<flight::types::Mesh>();
  return object && static_cast<bool>(object->geometry);
}

}  // namespace flight::scene3d
