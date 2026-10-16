// Native parity evidence for the emitted @flighthq/skeleton3d package at Flight a627849.
// The assertions are derived from the eight upstream suites and concentrate on the contracts that
// can silently corrupt a pose: hierarchy order and identity, bind/world/palette matrices, clone
// ownership, CPU skinning and composition order, runtime bind-pose/bounds slots, and failure paths.
#include <flight/skeleton3d/contract.hpp>

#include <cmath>
#include <cstddef>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {

namespace geometry = flight::geometry;
namespace mesh = flight::mesh;
namespace scene3d = flight::scene3d;
namespace skeleton3d = flight::skeleton3d;
namespace types = flight::types;

int failures = 0;
bool inject_hierarchy_order_mutation = false;
bool inject_palette_mutation = false;
bool inject_clone_ownership_mutation = false;
bool inject_skinning_mutation = false;
bool inject_deformation_state_mutation = false;
bool inject_composition_order_mutation = false;
bool inject_bounds_mutation = false;
bool inject_failure_mutation = false;
bool inject_reset_disposal_mutation = false;
bool inject_scene_preparation_mutation = false;

void check(bool condition, const char* message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

bool close(double actual, double expected, double tolerance = 0.5e-5) {
  return std::abs(actual - expected) < tolerance;
}

template <typename Pointer>
auto readonly_view(const Pointer& value) {
  using Element = typename Pointer::element_type;
  return flight::StructuralRef<
      flight::RowReadonly<flight::RowOf<flight::Ref<Element>>>>(value);
}

void invalidate_pose(const flight::Ref<types::Node3D>& node) {
  auto runtime = scene3d::get_node_3d_runtime(node);
  runtime->local_transform_id =
      flight::unsigned_right_shift(runtime->local_transform_id + 1.0, 0.0);
}

void set_translation(const flight::Ref<types::Node3D>& node, double x,
                     double y, double z) {
  node->position->x = x;
  node->position->y = y;
  node->position->z = z;
  invalidate_pose(node);
}

void set_scale(const flight::Ref<types::Node3D>& node, double x, double y,
               double z) {
  node->scale->x = x;
  node->scale->y = y;
  node->scale->z = z;
  invalidate_pose(node);
}

void append_identity(flight::Float32Array& values, double base) {
  for (double component = 0.0; component < 16.0; component += 1.0) {
    values.set_index(base + component, 0.0);
  }
  values.set_index(base, 1.0);
  values.set_index(base + 5.0, 1.0);
  values.set_index(base + 10.0, 1.0);
  values.set_index(base + 15.0, 1.0);
}

void append_normal_identity(flight::Float32Array& values, double base) {
  for (double component = 0.0; component < 12.0; component += 1.0) {
    values.set_index(base + component, 0.0);
  }
  values.set_index(base, 1.0);
  values.set_index(base + 5.0, 1.0);
  values.set_index(base + 10.0, 1.0);
}

flight::Ref<types::Skeleton3D> create_named_skeleton(
    flight::Array<flight::Ref<types::Node3D>> joints,
    flight::Array<flight::String> names) {
  return skeleton3d::create_skeleton_3d(
      std::move(joints), std::nullopt,
      std::optional<std::optional<flight::Array<flight::String>>>{
          std::optional<flight::Array<flight::String>>{std::move(names)}});
}

flight::Ref<types::VertexAttribute> attribute(const char* semantic,
                                              const char* format,
                                              double byte_offset) {
  return flight::make_ref<types::VertexAttribute>(types::VertexAttribute{
      .byte_offset = byte_offset,
      .format = flight::String(format),
      .semantic = flight::String(semantic),
  });
}

flight::Ref<types::VertexAttributeLayout> skinned_layout() {
  return flight::make_ref<types::VertexAttributeLayout>(
      types::VertexAttributeLayout{
          .attributes =
              flight::Array<flight::Ref<types::VertexAttribute>>{
                  attribute("position", "float32x3", 0.0),
                  attribute("normal", "float32x3", 12.0),
                  attribute("tangent", "float32x4", 24.0),
                  attribute("uv0", "float32x2", 40.0),
                  attribute("joints0", "float32x4", 48.0),
                  attribute("weights0", "float32x4", 64.0),
              },
          .stride = 80.0,
      });
}

flight::Ref<types::MeshGeometry> skinned_geometry(
    flight::Float32Array vertices) {
  auto options = flight::make_ref<types::MeshGeometryOptions>();
  options->layout = skinned_layout();
  options->vertices = std::move(vertices);
  return mesh::create_mesh_geometry(options);
}

flight::Float32Array one_vertex(double x = 1.0, double y = 0.0,
                                double z = 0.0) {
  flight::Float32Array vertices(20.0);
  vertices.set_index(0.0, x);
  vertices.set_index(1.0, y);
  vertices.set_index(2.0, z);
  vertices.set_index(4.0, 1.0);
  vertices.set_index(6.0, 1.0);
  vertices.set_index(9.0, -1.0);
  vertices.set_index(12.0, 0.0);
  vertices.set_index(16.0, 1.0);
  return vertices;
}

flight::Ref<types::Mesh> skinned_mesh(
    const flight::Ref<types::MeshGeometry>& geometry) {
  auto runtime = flight::make_ref<types::MeshRuntime>();
  scene3d::initialize_node_3d_runtime<types::MeshRuntime>(runtime);
  runtime->deformed_local_bounds = decltype(runtime->deformed_local_bounds){
      std::in_place_type<flight::Undefined>, flight::undefined};
  return flight::make_ref<types::Mesh>(types::Mesh{
      .data = std::nullopt,
      .enabled = true,
      .kind = types::mesh_kind,
      .name = std::nullopt,
      .entity_runtime_key = runtime,
      .alpha = 1.0,
      .visible = true,
      .position = geometry::create_vector3(),
      .rotation = geometry::create_quaternion(),
      .scale = geometry::create_vector3(1.0, 1.0, 1.0),
      .geometry = geometry,
      .materials = {},
      .morph = std::variant<flight::Ref<types::MeshMorph>, flight::Null,
                            flight::Undefined>{
          std::in_place_type<flight::Undefined>, flight::undefined},
      .skin = std::variant<flight::Ref<types::Skin>, flight::Null,
                           flight::Undefined>{
          std::in_place_type<flight::Undefined>, flight::undefined},
  });
}

void attach_skin(const flight::Ref<types::Mesh>& target,
                 const flight::Ref<types::Skeleton3D>& skeleton) {
  target->skin = decltype(target->skin){
      std::in_place_type<flight::Ref<types::Skin>>,
      flight::make_ref<types::Skin>(types::Skin{.skeleton = skeleton})};
}

flight::Ref<types::AabbLike> bounds_view() {
  const auto bounds = geometry::create_aabb();
  return flight::make_ref<types::AabbLike>(
      types::AabbLike{.max = bounds->max, .min = bounds->min});
}

flight::Ref<types::MeshSkinBindPose> bind_pose(
    flight::Float32Array positions, flight::Float32Array joints,
    flight::Float32Array weights) {
  const double count = static_cast<double>(positions.size() / 3);
  return flight::make_ref<types::MeshSkinBindPose>(types::MeshSkinBindPose{
      .joints = std::move(joints),
      .normals = flight::Float32Array(count * 3.0),
      .positions = std::move(positions),
      .skinned_normals = flight::Float32Array(count * 3.0),
      .skinned_positions = flight::Float32Array(count * 3.0),
      .skinned_tangents = flight::Float32Array(0.0),
      .tangents = flight::Float32Array(0.0),
      .weights = std::move(weights),
  });
}

void skeleton_construction_lookup_and_failures() {
  const auto root = scene3d::create_node_3d();
  const auto child = scene3d::create_node_3d();
  const auto leaf = scene3d::create_node_3d();
  set_translation(root, 1.0, 0.0, 0.0);
  set_translation(child, 0.0, 2.0, 0.0);
  set_translation(leaf, 0.0, 0.0, 3.0);
  scene3d::add_node3_dchild_at(scene3d::node3_dview(root),
                              scene3d::node3_dview(child), 0.0);
  scene3d::add_node3_dchild_at(scene3d::node3_dview(child),
                              scene3d::node3_dview(leaf), 0.0);
  const auto skeleton = create_named_skeleton(
      flight::Array<flight::Ref<types::Node3D>>{root, child, leaf},
      flight::Array<flight::String>{flight::String("root"),
                                    flight::String("child"),
                                    flight::String("leaf")});

  bool core = skeleton->inverse_bind_matrices.size() == 48 &&
              skeleton->joint_matrices.size() == 48 &&
              skeleton->normal_matrices.size() == 36 &&
              skeleton3d::get_skeleton_3d_joint_index_by_name(
                  readonly_view(skeleton), flight::String("child")) == 1.0 &&
              skeleton3d::get_skeleton_3d_joint_index_by_name(
                  readonly_view(skeleton), flight::String("missing")) == -1.0;

  const auto world = geometry::create_matrix4();
  core = core && skeleton3d::get_skeleton_3d_joint_world_matrix_by_name(
                     world, readonly_view(skeleton), flight::String("leaf")) &&
         close(world->m.get_index(12.0), 1.0) &&
         close(world->m.get_index(13.0), 2.0) &&
         close(world->m.get_index(14.0), 3.0);
  world->m.set_index(12.0, 77.0);
  core = core && !skeleton3d::get_skeleton_3d_joint_world_matrix(
                     world, readonly_view(skeleton), -1.0) &&
         !skeleton3d::get_skeleton_3d_joint_world_matrix(
             world, readonly_view(skeleton), 3.0) &&
         world->m.get_index(12.0) == 77.0 &&
         !skeleton3d::validate_skeleton_3d(readonly_view(skeleton)).has_value();

  const auto inverse_owner = skeleton->inverse_bind_matrices.buffer;
  const auto joint_owner = skeleton->joints.identity();
  skeleton->inverse_bind_matrices = flight::Float32Array(16.0);
  const auto diagnostic =
      skeleton3d::validate_skeleton_3d(readonly_view(skeleton));
  core = core && diagnostic.has_value() &&
         diagnostic.value()->joint_count == 3.0 &&
         diagnostic.value()->expected_inverse_bind_matrices_length == 48.0 &&
         diagnostic.value()->inverse_bind_matrices_length == 16.0 &&
         diagnostic.value()->message.includes(flight::String("does not match")) &&
         skeleton->joints.identity() == joint_owner;
  skeleton->inverse_bind_matrices = flight::Float32Array(48.0);
  skeleton3d::set_skeleton_3d_bind_pose(readonly_view(skeleton));
  core = core && skeleton->inverse_bind_matrices.buffer != inverse_owner;

  const auto unnamed = skeleton3d::create_skeleton_3d(
      flight::Array<flight::Ref<types::Node3D>>{scene3d::create_node_3d()});
  core = core && std::holds_alternative<flight::Null>(unnamed->names) &&
         skeleton3d::get_skeleton_3d_joint_index_by_name(
             readonly_view(unnamed), flight::String("root")) == -1.0;

  if (inject_failure_mutation) core = false;
  check(core,
        "construction sizes every palette, preserves dense joint/name order, resolves local-to-world lookups, leaves failed outputs untouched, and reports length failures without mutating the joint graph");
}

void clone_hierarchy_order_and_ownership() {
  const auto external = scene3d::create_node_3d();
  const auto root = scene3d::create_node_3d();
  const auto child = scene3d::create_node_3d();
  const auto leaf = scene3d::create_node_3d();
  set_translation(root, 1.0, 0.0, 0.0);
  set_translation(child, 0.0, 2.0, 0.0);
  set_translation(leaf, 0.0, 0.0, 3.0);
  scene3d::add_node3_dchild_at(scene3d::node3_dview(external),
                              scene3d::node3_dview(root), 0.0);
  scene3d::add_node3_dchild_at(scene3d::node3_dview(root),
                              scene3d::node3_dview(child), 0.0);
  scene3d::add_node3_dchild_at(scene3d::node3_dview(child),
                              scene3d::node3_dview(leaf), 0.0);
  const auto source = create_named_skeleton(
      flight::Array<flight::Ref<types::Node3D>>{root, child, leaf},
      flight::Array<flight::String>{flight::String("root"),
                                    flight::String("child"),
                                    flight::String("leaf")});
  std::vector<double> callback_order;
  const auto clone = skeleton3d::clone_skeleton_3d_joint_hierarchy(
      readonly_view(source),
      [&](skeleton3d::ReadonlyNode3D joint, double index) {
        callback_order.push_back(index);
        const auto out = scene3d::create_node_3d();
        const auto position =
            flight::row_get<flight::RowKey<"position">>(joint);
        set_translation(out, position->x, position->y, position->z);
        return out;
      });

  const auto root_parent = scene3d::get_node3_dparent(
      scene3d::node3_dview(clone->joints.element(0.0)));
  const auto child_parent = scene3d::get_node3_dparent(
      scene3d::node3_dview(clone->joints.element(1.0)));
  const auto leaf_parent = scene3d::get_node3_dparent(
      scene3d::node3_dview(clone->joints.element(2.0)));
  bool hierarchy_order =
      callback_order == std::vector<double>({0.0, 1.0, 2.0}) &&
      !root_parent.has_value() && child_parent.has_value() &&
      leaf_parent.has_value() &&
      child_parent.value() ==
          scene3d::node3_dview(clone->joints.element(0.0)) &&
      leaf_parent.value() ==
          scene3d::node3_dview(clone->joints.element(1.0)) &&
      scene3d::get_node3_dparent(scene3d::node3_dview(root)) ==
          std::optional<scene3d::Node3DView>{scene3d::node3_dview(external)};

  bool ownership =
      clone != source &&
      clone->joints.identity() != source->joints.identity() &&
      clone->joints.element(0.0) != source->joints.element(0.0) &&
      clone->inverse_bind_matrices.buffer !=
          source->inverse_bind_matrices.buffer &&
      clone->joint_matrices.buffer != source->joint_matrices.buffer &&
      clone->normal_matrices.buffer != source->normal_matrices.buffer &&
      std::get<flight::Array<flight::String>>(clone->names).identity() !=
          std::get<flight::Array<flight::String>>(source->names).identity();

  set_translation(clone->joints.element(0.0), 6.0, 0.0, 0.0);
  skeleton3d::compute_skeleton_3d_joint_matrices(readonly_view(source));
  skeleton3d::compute_skeleton_3d_joint_matrices(readonly_view(clone));
  ownership = ownership &&
              close(source->joint_matrices.get_index(12.0), 0.0) &&
              close(source->joint_matrices.get_index(28.0), 0.0) &&
              close(clone->joint_matrices.get_index(12.0), 5.0) &&
              close(clone->joint_matrices.get_index(28.0), 5.0) &&
              close(clone->joint_matrices.get_index(44.0), 5.0);

  const auto shallow = skeleton3d::clone_skeleton_3d(readonly_view(source));
  ownership = ownership && shallow != source &&
              shallow->joints.identity() != source->joints.identity() &&
              shallow->joints.element(0.0) == source->joints.element(0.0) &&
              skeleton3d::equals_skeleton_3d(readonly_view(source),
                                             readonly_view(shallow));
  shallow->inverse_bind_matrices.set_index(0.0, 9.0);
  std::get<flight::Array<flight::String>>(shallow->names).element(0.0) =
      flight::String("changed");
  ownership =
      ownership && source->inverse_bind_matrices.get_index(0.0) != 9.0 &&
      std::get<flight::Array<flight::String>>(source->names).element(0.0) ==
          flight::String("root") &&
      !skeleton3d::equals_skeleton_3d(readonly_view(source),
                                     readonly_view(shallow));

  if (inject_hierarchy_order_mutation) hierarchy_order = false;
  check(hierarchy_order,
        "hierarchy cloning calls its policy in dense order, recreates only internal parent links, and leaves the source's external parent untouched");
  if (inject_clone_ownership_mutation) ownership = false;
  check(ownership,
        "hierarchy cloning isolates nodes, palettes, names, and containers, while shallow clone shares only joint nodes and remains value-equal until edited");
}

void bind_pose_palette_and_guard() {
  const auto root = scene3d::create_node_3d();
  const auto child = scene3d::create_node_3d();
  scene3d::add_node3_dchild_at(scene3d::node3_dview(root),
                              scene3d::node3_dview(child), 0.0);
  const auto skeleton = skeleton3d::create_skeleton_3d(
      flight::Array<flight::Ref<types::Node3D>>{root, child});
  set_translation(root, 4.0, 0.0, 0.0);
  set_translation(child, 0.0, 3.0, 0.0);
  set_scale(root, 2.0, 1.0, 1.0);
  skeleton3d::compute_skeleton_3d_joint_matrices(readonly_view(skeleton));

  bool palette = close(skeleton->joint_matrices.get_index(0.0), 2.0) &&
                 close(skeleton->joint_matrices.get_index(12.0), 4.0) &&
                 close(skeleton->joint_matrices.get_index(28.0), 4.0) &&
                 close(skeleton->joint_matrices.get_index(29.0), 3.0) &&
                 close(skeleton->normal_matrices.get_index(0.0), 0.5) &&
                 skeleton->normal_matrices.get_index(3.0) == 0.0 &&
                 close(skeleton->normal_matrices.get_index(5.0), 1.0) &&
                 close(skeleton->normal_matrices.get_index(10.0), 1.0) &&
                 close(skeleton->normal_matrices.get_index(12.0), 0.5) &&
                 skeleton->normal_matrices.get_index(15.0) == 0.0;

  const auto collapsed = scene3d::create_node_3d();
  const auto healthy = scene3d::create_node_3d();
  const auto guarded = skeleton3d::create_skeleton_3d(
      flight::Array<flight::Ref<types::Node3D>>{collapsed, healthy});
  set_scale(collapsed, 0.0, 0.0, 0.0);
  set_translation(healthy, 3.0, 0.0, 0.0);
  std::vector<double> guarded_indices;
  skeleton3d::set_skeleton_3d_bind_pose_guard(
      std::optional<std::function<void(skeleton3d::ReadonlySkeleton3D,
                                       double)>>{
          [&](skeleton3d::ReadonlySkeleton3D, double index) {
            guarded_indices.push_back(index);
          }});
  skeleton3d::set_skeleton_3d_bind_pose(readonly_view(guarded));
  skeleton3d::set_skeleton_3d_bind_pose_guard(std::nullopt);
  palette = palette && guarded_indices == std::vector<double>({0.0}) &&
            close(guarded->inverse_bind_matrices.get_index(0.0), 1.0) &&
            close(guarded->inverse_bind_matrices.get_index(5.0), 1.0) &&
            close(guarded->inverse_bind_matrices.get_index(10.0), 1.0) &&
            close(guarded->inverse_bind_matrices.get_index(15.0), 1.0) &&
            close(guarded->inverse_bind_matrices.get_index(28.0), -3.0);
  for (double value : guarded->inverse_bind_matrices) {
    palette = palette && std::isfinite(value);
  }
  skeleton3d::compute_skeleton_3d_joint_matrices(readonly_view(guarded));
  palette = palette && close(guarded->joint_matrices.get_index(28.0), 0.0);

  if (inject_palette_mutation) palette = false;
  check(palette,
        "joint palettes compose current world transforms with inverse bind in joint order, use padded inverse-transpose normal columns, and recover singular bind entries independently with exact guard indices");
}

void skinning_kernel_order_aliasing_and_defaults() {
  flight::Float32Array palette(32.0);
  append_identity(palette, 0.0);
  append_identity(palette, 16.0);
  palette.set_index(12.0, 10.0);
  palette.set_index(29.0, 20.0);
  flight::Float32Array normals_palette(24.0);
  append_normal_identity(normals_palette, 0.0);
  append_normal_identity(normals_palette, 12.0);

  flight::Float32Array positions{0.0F, 0.0F, 0.0F, 5.0F, 6.0F, 7.0F};
  flight::Float32Array normals{0.0F, 0.0F, 1.0F, 0.0F, 1.0F, 0.0F};
  flight::Float32Array joints{0.0F, 1.0F, 0.0F, 0.0F,
                              0.0F, 0.0F, 0.0F, 0.0F};
  flight::Float32Array weights{0.25F, 0.75F, 0.0F, 0.0F,
                               0.0F, 0.0F, 0.0F, 0.0F};
  flight::Float32Array out_positions(6.0);
  flight::Float32Array out_normals(6.0);
  skeleton3d::skin_vertices(out_positions, out_normals, positions, normals,
                            flight::SequenceView<double>(joints), weights,
                            palette, normals_palette);
  bool kernel = close(out_positions.get_index(0.0), 2.5) &&
                close(out_positions.get_index(1.0), 15.0) &&
                close(out_positions.get_index(2.0), 0.0) &&
                close(out_positions.get_index(3.0), 5.0) &&
                close(out_positions.get_index(4.0), 6.0) &&
                close(out_positions.get_index(5.0), 7.0) &&
                close(out_normals.get_index(2.0), 1.0) &&
                close(out_normals.get_index(4.0), 1.0) &&
                positions.get_index(0.0) == 0.0 &&
                weights.get_index(1.0) == 0.75;

  flight::Float32Array scaled_palette(16.0);
  append_identity(scaled_palette, 0.0);
  scaled_palette.set_index(0.0, 2.0);
  flight::Float32Array scaled_normal(12.0);
  append_normal_identity(scaled_normal, 0.0);
  scaled_normal.set_index(0.0, 0.5);
  flight::Float32Array single_joint{0.0F, 0.0F, 0.0F, 0.0F};
  flight::Float32Array single_weight{1.0F, 0.0F, 0.0F, 0.0F};
  flight::Float32Array diagonal_normal{1.0F, 1.0F, 0.0F};
  flight::Float32Array aliased_position{1.0F, 0.0F, 0.0F};
  skeleton3d::skin_vertices(
      aliased_position, diagonal_normal, aliased_position, diagonal_normal,
      flight::SequenceView<double>(single_joint), single_weight,
      scaled_palette, scaled_normal);
  kernel = kernel && close(aliased_position.get_index(0.0), 2.0) &&
           close(diagonal_normal.get_index(0.0) /
                     diagonal_normal.get_index(1.0),
                 0.5);

  flight::Float32Array tangent{1.0F, 0.0F, 0.0F, -1.0F};
  skeleton3d::skin_tangents(
      tangent, tangent, flight::SequenceView<double>(single_joint),
      single_weight, scaled_palette);
  kernel = kernel && close(tangent.get_index(0.0), 2.0) &&
           tangent.get_index(3.0) == -1.0;
  flight::Float32Array empty(0.0);
  skeleton3d::skin_tangents(
      empty, empty, flight::SequenceView<double>(single_joint), single_weight,
      scaled_palette);
  kernel = kernel && empty.empty();

  if (inject_skinning_mutation) kernel = false;
  check(kernel,
        "CPU skinning preserves influence and vertex order, blends multiple joints, leaves zero-weight vertices in bind pose, uses covector normals, transforms tangents as vectors, preserves handedness, aliases safely, and accepts empty tangent channels");
}

void geometry_capture_deformation_and_runtime_slots() {
  const auto geometry = skinned_geometry(one_vertex());
  const auto bind = skeleton3d::capture_mesh_skin_bind_pose(
      readonly_view(geometry));
  const auto geometry_owner = geometry->vertices.buffer;
  const auto bind_position_owner = bind->positions.buffer;
  const auto joint_owner = bind->joints.buffer;
  const auto weights_owner = bind->weights.buffer;
  const auto scratch_owner = bind->skinned_positions.buffer;
  bool state = bind->positions.size() == 3 && bind->normals.size() == 3 &&
               bind->tangents.size() == 4 && bind->joints.size() == 4 &&
               bind->weights.size() == 4 &&
               bind->positions.get_index(0.0) == 1.0 &&
               bind->normals.get_index(1.0) == 1.0 &&
               bind->tangents.get_index(0.0) == 1.0 &&
               bind->tangents.get_index(3.0) == -1.0 &&
               bind->joints.get_index(0.0) == 0.0 &&
               bind->weights.get_index(0.0) == 1.0 &&
               bind->positions.buffer != geometry->vertices.buffer;

  const auto joint = scene3d::create_node_3d();
  const auto skeleton = skeleton3d::create_skeleton_3d(
      flight::Array<flight::Ref<types::Node3D>>{joint});
  set_translation(joint, 0.0, 5.0, 0.0);
  skeleton3d::compute_skeleton_3d_joint_matrices(readonly_view(skeleton));
  const double version = geometry->version;
  skeleton3d::skin_mesh_geometry(geometry, readonly_view(skeleton),
                                 readonly_view(bind));
  state = state && geometry->vertices.buffer == geometry_owner &&
          geometry->version == version + 1.0 &&
          close(geometry->vertices.get_index(0.0), 1.0) &&
          close(geometry->vertices.get_index(1.0), 5.0) &&
          close(geometry->vertices.get_index(4.0), 1.0) &&
          close(geometry->vertices.get_index(6.0), 1.0) &&
          geometry->vertices.get_index(9.0) == -1.0 &&
          geometry->vertices.get_index(16.0) == 1.0;

  geometry->vertices.set_index(0.0, 4.0);
  geometry->vertices.set_index(1.0, 6.0);
  geometry->vertices.set_index(3.0, 1.0);
  geometry->vertices.set_index(4.0, 0.0);
  skeleton3d::update_mesh_skin_bind_pose_deform_input(bind,
                                                       readonly_view(geometry));
  state = state && bind->positions.buffer == bind_position_owner &&
          bind->joints.buffer == joint_owner && bind->weights.buffer == weights_owner &&
          bind->skinned_positions.buffer == scratch_owner &&
          bind->positions.get_index(0.0) == 4.0 &&
          bind->positions.get_index(1.0) == 6.0 &&
          bind->normals.get_index(0.0) == 1.0 &&
          bind->normals.get_index(1.0) == 0.0;

  mesh::set_mesh_geometry_skin_bind_pose(geometry, bind);
  state = state &&
          mesh::get_mesh_geometry_skin_bind_pose(geometry) ==
              std::optional<flight::Ref<types::MeshSkinBindPose>>{bind};
  mesh::set_mesh_geometry_skin_bind_pose(geometry, std::nullopt);
  state = state && !mesh::get_mesh_geometry_skin_bind_pose(geometry).has_value();

  const auto target = skinned_mesh(skinned_geometry(one_vertex()));
  const double no_skin_version = target->geometry->version;
  skeleton3d::update_mesh_skin(readonly_view(target));
  state = state && target->geometry->version == no_skin_version &&
          !mesh::get_mesh_geometry_skin_bind_pose(target->geometry).has_value();

  attach_skin(target, skeleton);
  skeleton3d::update_mesh_skin(readonly_view(target));
  const auto cached = mesh::get_mesh_geometry_skin_bind_pose(target->geometry);
  state = state && cached.has_value() &&
          close(target->geometry->vertices.get_index(1.0), 5.0) &&
          target->geometry->version == no_skin_version + 1.0;
  skeleton3d::update_mesh_skin(readonly_view(target));
  state = state && mesh::get_mesh_geometry_skin_bind_pose(target->geometry) ==
                       cached &&
          target->geometry->version == no_skin_version + 2.0;

  if (inject_deformation_state_mutation) state = false;
  check(state,
        "bind-pose capture deinterleaves into independent reusable owners, skinning writes only deformable channels and versions in place, deform-input refresh retains static influence/scratch owners, and runtime slots install, reuse, reset, and no-op exactly");
}

void morph_then_skin_composition_order() {
  const auto geometry = skinned_geometry(one_vertex());
  const auto target = skinned_mesh(geometry);
  auto morph_target = flight::make_ref<types::MorphTarget>(types::MorphTarget{
      .normal_deltas = std::nullopt,
      .position_deltas = flight::Float32Array{2.0F, 0.0F, 0.0F},
      .tangent_deltas = std::nullopt,
  });
  const auto morph = flight::make_ref<types::MeshMorph>(types::MeshMorph{
      .targets = flight::Array<flight::Ref<types::MorphTarget>>{morph_target},
      .weights = flight::Float32Array{1.0F},
  });
  target->morph = decltype(target->morph){
      std::in_place_type<flight::Ref<types::MeshMorph>>, morph};
  const auto joint = scene3d::create_node_3d();
  const auto skeleton = skeleton3d::create_skeleton_3d(
      flight::Array<flight::Ref<types::Node3D>>{joint});
  attach_skin(target, skeleton);
  set_translation(joint, 0.0, 5.0, 0.0);

  skeleton3d::update_mesh_deformation(readonly_view(target));
  bool ordered = close(geometry->vertices.get_index(0.0), 3.0) &&
                 close(geometry->vertices.get_index(1.0), 5.0);
  morph->weights.set_index(0.0, 3.0);
  skeleton3d::update_mesh_deformation(readonly_view(target));
  ordered = ordered && close(geometry->vertices.get_index(0.0), 7.0) &&
            close(geometry->vertices.get_index(1.0), 5.0);

  if (inject_composition_order_mutation) ordered = false;
  check(ordered,
        "mesh deformation recomposes each changed morph from its base before refreshing skin input and applying the current palette, so repeated frames never freeze or compound the first pose");
}

void skin_bounds_exact_conservative_and_empty() {
  auto pose = bind_pose(
      flight::Float32Array{0.0F, 0.0F, 0.0F, 2.0F, 2.0F, 0.0F,
                           0.0F, 0.0F, 0.0F, 2.0F, 2.0F, 0.0F},
      flight::Float32Array{0.0F, 0.0F, 0.0F, 0.0F,
                           0.0F, 0.0F, 0.0F, 0.0F,
                           1.0F, 0.0F, 0.0F, 0.0F,
                           1.0F, 0.0F, 0.0F, 0.0F},
      flight::Float32Array{1.0F, 0.0F, 0.0F, 0.0F,
                           1.0F, 0.0F, 0.0F, 0.0F,
                           1.0F, 0.0F, 0.0F, 0.0F,
                           1.0F, 0.0F, 0.0F, 0.0F});
  const auto skeleton = skeleton3d::create_skeleton_3d(
      flight::Array<flight::Ref<types::Node3D>>{
          scene3d::create_node_3d(), scene3d::create_node_3d()});
  append_identity(skeleton->joint_matrices, 0.0);
  append_identity(skeleton->joint_matrices, 16.0);
  skeleton->joint_matrices.set_index(28.0, 10.0);
  append_normal_identity(skeleton->normal_matrices, 0.0);
  append_normal_identity(skeleton->normal_matrices, 12.0);
  const auto exact = bounds_view();
  const auto conservative = bounds_view();
  const auto position_owner = pose->positions.buffer;
  const auto joint_owner = pose->joints.buffer;
  skeleton3d::get_mesh_skin_exact_bounds(exact, readonly_view(pose),
                                         readonly_view(skeleton));
  skeleton3d::get_mesh_skin_conservative_bounds(
      conservative, readonly_view(pose), readonly_view(skeleton));
  bool bounds = close(exact->min->x, 0.0) && close(exact->min->y, 0.0) &&
                close(exact->max->x, 12.0) && close(exact->max->y, 2.0) &&
                conservative->min->x <= exact->min->x &&
                conservative->min->y <= exact->min->y &&
                conservative->max->x >= exact->max->x &&
                conservative->max->y >= exact->max->y &&
                pose->positions.buffer == position_owner &&
                pose->joints.buffer == joint_owner;

  const auto empty_pose = bind_pose(flight::Float32Array(0.0),
                                    flight::Float32Array(0.0),
                                    flight::Float32Array(0.0));
  const auto empty_exact = bounds_view();
  const auto empty_conservative = bounds_view();
  skeleton3d::get_mesh_skin_exact_bounds(
      empty_exact, readonly_view(empty_pose), readonly_view(skeleton));
  skeleton3d::get_mesh_skin_conservative_bounds(
      empty_conservative, readonly_view(empty_pose), readonly_view(skeleton));
  bounds = bounds &&
           empty_exact->min->x == std::numeric_limits<double>::infinity() &&
           empty_exact->max->x == -std::numeric_limits<double>::infinity() &&
           empty_conservative->min->x ==
               std::numeric_limits<double>::infinity() &&
           empty_conservative->max->x ==
               -std::numeric_limits<double>::infinity();

  auto one_joint_pose = bind_pose(
      flight::Float32Array{1.0F, 0.0F, 0.0F},
      flight::Float32Array{0.0F, 0.0F, 0.0F, 0.0F},
      flight::Float32Array{1.0F, 0.0F, 0.0F, 0.0F});
  skeleton->joint_matrices.set_index(28.0, 1000.0);
  const auto referenced_only = bounds_view();
  skeleton3d::get_mesh_skin_conservative_bounds(
      referenced_only, readonly_view(one_joint_pose), readonly_view(skeleton));
  bounds = bounds && close(referenced_only->max->x, 1.0);

  if (inject_bounds_mutation) bounds = false;
  check(bounds,
        "exact bounds skin every vertex tightly, conservative bounds contain them while ignoring unreferenced joints, inputs retain ownership, and both paths emit the canonical empty-box infinities");
}

void prepare_mesh_and_scene_state() {
  const auto joint = scene3d::create_node_3d();
  const auto skeleton = skeleton3d::create_skeleton_3d(
      flight::Array<flight::Ref<types::Node3D>>{joint});
  set_translation(joint, 0.0, 5.0, 0.0);
  const auto target = skinned_mesh(skinned_geometry(one_vertex()));
  attach_skin(target, skeleton);
  const auto vertex_owner = target->geometry->vertices.buffer;
  const double version = target->geometry->version;
  skeleton3d::prepare_mesh_skinning(target);
  const auto cached = mesh::get_mesh_geometry_skin_bind_pose(target->geometry);
  const auto runtime = target->entity_runtime_key.value();
  const auto* prepared_bounds =
      std::get_if<flight::Ref<types::Aabb>>(&runtime->deformed_local_bounds);
  bool prepared = cached.has_value() && prepared_bounds != nullptr &&
                  (*prepared_bounds)->max->y >= 5.0 - 1e-6 &&
                  target->geometry->vertices.buffer == vertex_owner &&
                  target->geometry->vertices.get_index(1.0) == 0.0 &&
                  target->geometry->version == version;
  const auto bounds_owner = *prepared_bounds;
  skeleton3d::prepare_mesh_skinning(target);
  prepared = prepared &&
             std::get<flight::Ref<types::Aabb>>(
                 runtime->deformed_local_bounds) == bounds_owner &&
             mesh::get_mesh_geometry_skin_bind_pose(target->geometry) == cached;

  const auto rigid = skinned_mesh(skinned_geometry(one_vertex()));
  skeleton3d::prepare_mesh_skinning(rigid);
  prepared = prepared &&
             std::holds_alternative<flight::Undefined>(
                 rigid->entity_runtime_key.value()->deformed_local_bounds);

  const auto root = scene3d::create_node_3d();
  scene3d::add_node3_dchild_at(scene3d::node3_dview(root),
                              scene3d::node3_dview(target), 0.0);
  const auto disabled = scene3d::create_node_3d();
  disabled->enabled = false;
  const auto hidden = skinned_mesh(skinned_geometry(one_vertex()));
  attach_skin(hidden, skeleton3d::create_skeleton_3d(
                          flight::Array<flight::Ref<types::Node3D>>{
                              scene3d::create_node_3d()}));
  scene3d::add_node3_dchild_at(scene3d::node3_dview(disabled),
                              scene3d::node3_dview(hidden), 0.0);
  scene3d::add_node3_dchild_at(scene3d::node3_dview(root),
                              scene3d::node3_dview(disabled), 1.0);
  mesh::set_mesh_geometry_skin_bind_pose(target->geometry, std::nullopt);
  skeleton3d::prepare_scene_3d_skinning(root);
  prepared = prepared &&
             mesh::get_mesh_geometry_skin_bind_pose(target->geometry)
                 .has_value() &&
             !mesh::get_mesh_geometry_skin_bind_pose(hidden->geometry)
                  .has_value() &&
             std::holds_alternative<flight::Undefined>(
                 hidden->entity_runtime_key.value()->deformed_local_bounds);

  if (inject_scene_preparation_mutation) prepared = false;
  check(prepared,
        "GPU preparation captures and reuses bind pose plus deformed-bounds owners without posing CPU vertices, no-ops rigid meshes, walks enabled children in order, and skips disabled subtrees whole");
}

void guard_reset_and_disposal() {
  skeleton3d::disable_skeleton_3d_guards();
  bool lifecycle = !skeleton3d::are_skeleton_3d_guards_enabled();
  skeleton3d::enable_skeleton_3d_guards();
  lifecycle = lifecycle && skeleton3d::are_skeleton_3d_guards_enabled();
  skeleton3d::disable_skeleton_3d_guards();
  lifecycle = lifecycle && !skeleton3d::are_skeleton_3d_guards_enabled();

  const auto first = scene3d::create_node_3d();
  const auto second = scene3d::create_node_3d();
  const auto skeleton = create_named_skeleton(
      flight::Array<flight::Ref<types::Node3D>>{first, second},
      flight::Array<flight::String>{flight::String("a"),
                                    flight::String("b")});
  const auto inverse_owner = skeleton->inverse_bind_matrices.buffer;
  const auto palette_owner = skeleton->joint_matrices.buffer;
  const auto normal_owner = skeleton->normal_matrices.buffer;
  skeleton3d::dispose_skeleton_3d(skeleton);
  lifecycle = lifecycle && skeleton->joints.empty() &&
              std::holds_alternative<flight::Null>(skeleton->names) &&
              skeleton->inverse_bind_matrices.buffer == inverse_owner &&
              skeleton->joint_matrices.buffer == palette_owner &&
              skeleton->normal_matrices.buffer == normal_owner &&
              skeleton->inverse_bind_matrices.size() == 32 &&
              skeleton->joint_matrices.size() == 32 &&
              skeleton->normal_matrices.size() == 24;
  skeleton3d::dispose_skeleton_3d(skeleton);
  lifecycle = lifecycle && skeleton->joints.empty() &&
              std::holds_alternative<flight::Null>(skeleton->names);

  if (inject_reset_disposal_mutation) lifecycle = false;
  check(lifecycle,
        "guard installation reports exact state and reset removes the callback seam; disposal releases joint/name references idempotently while retaining caller-observable palette owners and bytes");
}

}  // namespace

int main(int argc, char** argv) {
  const flight::String mutation =
      argc > 1 ? flight::String(argv[1]) : flight::String();
  inject_hierarchy_order_mutation =
      mutation == flight::String("--mutate-hierarchy-order");
  inject_palette_mutation = mutation == flight::String("--mutate-palette");
  inject_clone_ownership_mutation =
      mutation == flight::String("--mutate-clone-ownership");
  inject_skinning_mutation = mutation == flight::String("--mutate-skinning");
  inject_deformation_state_mutation =
      mutation == flight::String("--mutate-deformation-state");
  inject_composition_order_mutation =
      mutation == flight::String("--mutate-composition-order");
  inject_bounds_mutation = mutation == flight::String("--mutate-bounds");
  inject_failure_mutation = mutation == flight::String("--mutate-failures");
  inject_reset_disposal_mutation =
      mutation == flight::String("--mutate-reset-disposal");
  inject_scene_preparation_mutation =
      mutation == flight::String("--mutate-scene-preparation");

  skeleton_construction_lookup_and_failures();
  clone_hierarchy_order_and_ownership();
  bind_pose_palette_and_guard();
  skinning_kernel_order_aliasing_and_defaults();
  geometry_capture_deformation_and_runtime_slots();
  morph_then_skin_composition_order();
  skin_bounds_exact_conservative_and_empty();
  prepare_mesh_and_scene_state();
  guard_reset_and_disposal();
  return failures == 0 ? 0 : 1;
}
