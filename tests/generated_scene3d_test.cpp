#include <flight/scene3d/scene_node.hpp>
#include <flight/materials/pbr_materials.hpp>
#include <flight/mesh/mesh_geometry_operations.hpp>
#include <flight/node/node_transform3d.hpp>
#include <flight/scene3d/mesh.hpp>

#include <cmath>
#include <iostream>

namespace {

int failures = 0;

void check(bool condition, const char* message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

struct NodeOptions : public flight::ReferenceEnabled {
  std::optional<double> alpha;
  std::optional<bool> enabled;
  std::variant<flight::String, flight::Null, flight::Undefined> name{
      std::in_place_type<flight::Undefined>, flight::undefined};
  std::optional<bool> visible;
};

} // namespace

int main() {
  namespace scene3d = flight::scene3d;
  namespace types = flight::types;

  const auto first = scene3d::create_node3_d();
  const auto second = scene3d::create_node3_d();
  check(first->kind == types::node3_dkind,
        "Node3D construction retains the upstream default kind");
  check(first->enabled && first->visible && first->alpha == 1.0,
        "Node3D construction applies the upstream appearance defaults");
  check(first->position->x == 0.0 && first->position->y == 0.0 &&
            first->position->z == 0.0 && first->rotation->w == 1.0 &&
            first->scale->x == 1.0 && first->scale->y == 1.0 &&
            first->scale->z == 1.0,
        "Node3D construction initializes identity transform storage");
  check(first->position != second->position,
        "each Node3D owns independent transform values");

  auto options = flight::make_ref<NodeOptions>();
  options->alpha = 0.25;
  options->enabled = false;
  options->name = flight::String("named");
  options->visible = false;
  const auto configured = scene3d::create_node3_d(flight::String("Configured"), options);
  check(configured->kind == flight::String("Configured") &&
            !configured->enabled && !configured->visible &&
            configured->alpha == 0.25 && configured->name ==
                std::optional<flight::String>{flight::String("named")},
        "Node3D construction forwards the complete declared option surface");

  const auto runtime = scene3d::get_node3_druntime(configured);
  check(runtime->traits == std::optional<flight::Symbol>{types::node3_dtraits_key},
        "Node3D runtime carries the upstream trait key");
  check(runtime->appearance_id == 0.0 && runtime->local_transform_id == 0.0 &&
            runtime->world_alpha_using_appearance_id == -1.0 &&
            runtime->world_alpha_using_parent_appearance_id == -1.0 &&
            !runtime->local_matrix4.has_value() &&
            !runtime->world_matrix4.has_value(),
        "Node3D runtime initializes base and 3D cache revisions");

  const auto signals = scene3d::enable_node3_dsignals(configured);
  check(signals != nullptr &&
            scene3d::get_node3_dsignals(configured) ==
                std::optional<flight::Ref<types::NodeSignals>>{signals} &&
            scene3d::enable_node3_dsignals(configured) == signals,
        "Node3D signals are allocated lazily and retain identity");

  auto geometry_options =
      flight::make_ref<types::MeshGeometryFromAttributesOptions>();
  geometry_options->positions =
      flight::Array<double>{0.0, 0.0, 0.0, 1.0, 0.0, 0.0,
                            0.0, 1.0, 0.0};
  geometry_options->uvs = flight::Array<double>{0.0, 0.0, 1.0, 0.0,
                                                 0.0, 1.0};
  geometry_options->indices = flight::Array<double>{0.0, 1.0, 2.0};
  const auto geometry =
      flight::mesh::create_mesh_geometry_from_attributes(geometry_options);
  check(flight::mesh::validate_mesh_geometry(geometry) &&
            flight::mesh::get_mesh_geometry_triangle_count(geometry) == 1.0,
        "attribute construction creates valid indexed triangle geometry");
  auto triangle = flight::make_ref<types::MeshTriangleVertexIndices>();
  check(flight::mesh::get_mesh_geometry_triangle_vertex_indices(
            triangle, geometry, 0.0) &&
            triangle->i0 == 0.0 && triangle->i1 == 1.0 &&
            triangle->i2 == 2.0,
        "triangle decoding retains the authored winding");
  check(std::abs(geometry->vertices.get_index(5.0) - 1.0) < 1e-6 &&
            std::abs(geometry->vertices.get_index(6.0) - 1.0) < 1e-6 &&
            std::abs(geometry->vertices.get_index(9.0) - 1.0) < 1e-6,
        "missing normals and tangents are derived from geometry and UVs");
  check(geometry->bounds.has_value() &&
            geometry->bounds.value()->min->x == 0.0 &&
            geometry->bounds.value()->min->y == 0.0 &&
            geometry->bounds.value()->max->x == 1.0 &&
            geometry->bounds.value()->max->y == 1.0,
        "attribute construction computes local bounds");

  const auto pbr_material = flight::materials::create_standard_pbr_material();
  flight::Array<std::optional<types::MaterialValue>> materials{
      std::optional<types::MaterialValue>{types::MaterialValue{
          std::in_place_type<flight::Ref<types::StandardPbrMaterial>>,
          pbr_material}}};
  const auto mesh = scene3d::create_mesh(geometry, materials);
  check(mesh->geometry == geometry &&
            mesh->materials.identity() == materials.identity() &&
            mesh->materials[0].has_value() &&
            std::holds_alternative<flight::Ref<types::StandardPbrMaterial>>(
                mesh->materials[0].value()) &&
            std::get<flight::Ref<types::StandardPbrMaterial>>(
                mesh->materials[0].value()) == pbr_material &&
            mesh->kind == types::mesh_kind && mesh->enabled && mesh->visible &&
            mesh->alpha == 1.0,
        "Mesh construction stores geometry and materials by reference with Node3D defaults");
  const auto mesh_runtime = scene3d::get_mesh_runtime(mesh);
  check(mesh_runtime->traits ==
            std::optional<flight::Symbol>{types::node3_dtraits_key} &&
            std::holds_alternative<flight::Undefined>(
                mesh_runtime->deformed_local_bounds),
        "Mesh owns its concrete runtime while retaining the Node3D trait contract");
  const auto mesh_signals = scene3d::enable_mesh_signals(mesh);
  check(mesh_signals == scene3d::enable_mesh_signals(mesh) &&
            scene3d::get_mesh_signals(mesh) ==
                std::optional<flight::Ref<types::NodeSignals>>{mesh_signals},
        "Mesh signals are lazily allocated on the concrete runtime");

  mesh->position->x = 4.0;
  const auto rigid_clone = scene3d::clone_mesh(mesh);
  check(rigid_clone != mesh && rigid_clone->geometry == geometry &&
            rigid_clone->materials.identity() != mesh->materials.identity() &&
            rigid_clone->position != mesh->position &&
            rigid_clone->position->x == 4.0,
        "rigid Mesh cloning copies node state and material slots while sharing geometry");

  flight::Float32Array weights(1.0);
  weights.set_index(0.0, 0.25);
  const flight::Array<flight::Ref<types::MorphTarget>> targets;
  mesh->morph = flight::make_ref<types::MeshMorph>(
      types::MeshMorph{.targets = targets, .weights = weights});
  const auto deforming_clone = scene3d::clone_mesh(mesh);
  const auto source_morph = std::get<flight::Ref<types::MeshMorph>>(mesh->morph);
  const auto cloned_morph =
      std::get<flight::Ref<types::MeshMorph>>(deforming_clone->morph);
  cloned_morph->weights.set_index(0.0, 0.75);
  check(deforming_clone->geometry != mesh->geometry &&
            cloned_morph != source_morph &&
            cloned_morph->targets.identity() == source_morph->targets.identity() &&
            source_morph->weights.get_index(0.0) == 0.25 &&
            scene3d::get_mesh_deformer(deforming_clone) ==
                types::mesh_deformer_morph,
        "deforming Mesh cloning isolates mutable geometry and weights but shares immutable targets");
  check(scene3d::is_mesh(mesh) &&
            scene3d::is_mesh(flight::Any::object(mesh)),
        "Mesh discrimination accepts concrete and erased references");

  first->position->x = 2.0;
  scene3d::get_node3_druntime(first)->local_transform_id += 1.0;
  mesh->position->x = 4.0;
  flight::node::invalidate_node_local_transform(mesh);
  flight::node::add_node_child(first, mesh);
  check(scene3d::get_node3_dchild_count(first) == 1.0 &&
            scene3d::get_node3_dparent(scene3d::node3_dview(mesh)) ==
                std::optional<scene3d::Node3DView>{
                    scene3d::node3_dview(first)},
        "Node3D hierarchy retains concrete Mesh children and parent identity");
  const auto local_matrix = flight::node::get_node_local_matrix4(mesh);
  const auto world_matrix = flight::node::get_node_world_matrix4(mesh);
  check(std::abs(local_matrix->m.get_index(12.0) - 4.0) < 1e-6 &&
            std::abs(world_matrix->m.get_index(12.0) - 6.0) < 1e-6,
        "3D transform caches compose a concrete child with its Node3D parent");

  auto detached = flight::geometry::create_matrix4(
      std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt,
      std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt,
      std::nullopt, std::nullopt, 9.0, 3.0, 1.0, std::nullopt);
  flight::node::set_node_local_matrix4(mesh, detached);
  check(flight::node::is_node_local_matrix4_detached(mesh) &&
            flight::node::get_node_local_matrix4(mesh) != detached &&
            flight::node::get_node_local_matrix4(mesh)->m.get_index(12.0) ==
                9.0,
        "direct local matrices are copied, retained, and marked detached");
  flight::node::sync_node_transform3_dfrom_matrix4(mesh);
  check(!flight::node::is_node_local_matrix4_detached(mesh) &&
            std::abs(mesh->position->x - 9.0) < 1e-6 &&
            std::abs(mesh->position->y - 3.0) < 1e-6,
        "detached matrices synchronize back into authored TRS fields");

  flight::node::remove_node_child(first, mesh);
  check(scene3d::get_node3_dchild_count(first) == 0.0 &&
            !scene3d::get_node3_dparent(scene3d::node3_dview(mesh)).has_value(),
        "Node3D removal clears both sides of the hierarchy");

  return failures == 0 ? 0 : 1;
}
