#include <flight/physics3d_abi/contract.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>

namespace {

namespace abi = flight::physics3d_abi;
namespace physics = flight::physics3d;
namespace types = flight::types;

int failures = 0;
bool mutate_layout = false;
bool mutate_transaction = false;
bool mutate_identity = false;
bool mutate_free_flight = false;
bool mutate_shapes = false;
bool mutate_unsupported = false;

void check(bool condition, const std::string& message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

bool close(double actual, double expected, double tolerance = 1e-10) {
  return std::abs(actual - expected) <= tolerance;
}

template <typename Pointer>
auto readonly_view(const Pointer& value) {
  using T = typename Pointer::element_type;
  using View = flight::StructuralRef<
      flight::RowReadonly<flight::RowOf<flight::Ref<T>>>>;
  return flight::structural_ref_cast<View>(
      flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<T>>>>(
          value));
}

types::CollisionColliderShape3D sphere_shape(double radius = 1.0) {
  return types::CollisionColliderShape3D{
      std::in_place_index<0>,
      flight::make_ref<types::x_y_z_radius_kind_b0bc53ca3f0e8006>(
          types::x_y_z_radius_kind_b0bc53ca3f0e8006{
              .x = 0.0,
              .y = 0.0,
              .z = 0.0,
              .radius = radius,
              .kind = flight::String("sphere")})};
}

types::CollisionColliderShape3D aabb_shape() {
  return types::CollisionColliderShape3D{
      std::in_place_index<3>,
      flight::make_ref<
          types::min_x_min_y_min_z_max_x_max_y_max_z_kind_748c675e8e0cf88a>(
          types::min_x_min_y_min_z_max_x_max_y_max_z_kind_748c675e8e0cf88a{
              .min_x = -1.0,
              .min_y = -2.0,
              .min_z = -3.0,
              .max_x = 1.0,
              .max_y = 2.0,
              .max_z = 3.0,
              .kind = flight::String("aabb")})};
}

types::CollisionColliderShape3D box_shape() {
  return types::CollisionColliderShape3D{
      std::in_place_index<2>,
      flight::make_ref<types::
          x_y_z_half_x_half_y_half_z_rotation_x_rotation_y_rotation_z_rotation_w_kind_bdd50a8e154d5a6a>(
          types::
              x_y_z_half_x_half_y_half_z_rotation_x_rotation_y_rotation_z_rotation_w_kind_bdd50a8e154d5a6a{
                  .x = 1.0,
                  .y = 2.0,
                  .z = 3.0,
                  .half_x = 4.0,
                  .half_y = 5.0,
                  .half_z = 6.0,
                  .rotation_x = 0.0,
                  .rotation_y = 0.0,
                  .rotation_z = 0.0,
                  .rotation_w = 1.0,
                  .kind = flight::String("box")})};
}

types::CollisionColliderShape3D capsule_shape() {
  return types::CollisionColliderShape3D{
      std::in_place_index<5>,
      flight::make_ref<
          types::x0_y0_z0_x1_y1_z1_radius_kind_3ca9ada7b527e64b>(
          types::x0_y0_z0_x1_y1_z1_radius_kind_3ca9ada7b527e64b{
              .x0 = 0.0,
              .y0 = 1.0,
              .z0 = 2.0,
              .x1 = 3.0,
              .y1 = 4.0,
              .z1 = 5.0,
              .radius = 0.5,
              .kind = flight::String("capsule")})};
}

types::CollisionColliderShape3D cylinder_shape() {
  return types::CollisionColliderShape3D{
      std::in_place_index<6>,
      flight::make_ref<
          types::x0_y0_z0_x1_y1_z1_radius_kind_aef60734fa17cff8>(
          types::x0_y0_z0_x1_y1_z1_radius_kind_aef60734fa17cff8{
              .x0 = 0.0,
              .y0 = 1.0,
              .z0 = 2.0,
              .x1 = 3.0,
              .y1 = 4.0,
              .z1 = 5.0,
              .radius = 0.75,
              .kind = flight::String("cylinder")})};
}

types::CollisionColliderShape3D cone_shape() {
  return types::CollisionColliderShape3D{
      std::in_place_index<1>,
      flight::make_ref<types::
          apex_x_apex_y_apex_z_base_x_base_y_base_z_radius_kind_21c650ad310bf539>(
          types::
              apex_x_apex_y_apex_z_base_x_base_y_base_z_radius_kind_21c650ad310bf539{
                  .apex_x = 0.0,
                  .apex_y = 2.0,
                  .apex_z = 0.0,
                  .base_x = 0.0,
                  .base_y = 0.0,
                  .base_z = 0.0,
                  .radius = 1.0,
                  .kind = flight::String("cone")})};
}

types::CollisionColliderShape3D convex_shape() {
  return types::CollisionColliderShape3D{
      std::in_place_index<4>,
      flight::make_ref<types::points_kind_35a8f5f2a3a8bd66>(
          types::points_kind_35a8f5f2a3a8bd66{
              .points = flight::Array<double>{0.0, 0.0, 0.0, 1.0, 0.0,
                                               0.0},
              .kind = flight::String("convex")})};
}

types::CollisionColliderShape3D triangle_mesh_shape() {
  return types::CollisionColliderShape3D{
      std::in_place_index<8>,
      flight::make_ref<types::CollisionTriangleMesh3D>(
          types::CollisionTriangleMesh3D{
              .entity_runtime_key = std::nullopt,
              .kind = flight::String("triangle-mesh"),
              .points = flight::Array<double>{0.0, 0.0, 0.0, 1.0, 0.0, 0.0,
                                               0.0, 1.0, 0.0},
              .indices = flight::Array<double>{0.0, 1.0, 2.0},
              .version = 7.0,
              .x = 1.0,
              .y = 2.0,
              .z = 3.0,
              .rotation_x = 0.0,
              .rotation_y = 0.0,
              .rotation_z = 0.0,
              .rotation_w = 1.0})};
}

types::CollisionColliderShape3D heightfield_shape() {
  return types::CollisionColliderShape3D{
      std::in_place_index<7>,
      flight::make_ref<types::CollisionHeightfield3D>(
          types::CollisionHeightfield3D{
              .entity_runtime_key = std::nullopt,
              .kind = flight::String("heightfield"),
              .columns = 2.0,
              .rows = 2.0,
              .heights = flight::Array<double>{1.0, 2.0, 3.0, 4.0},
              .cell_size_x = 0.5,
              .cell_size_z = 0.75,
              .version = 9.0,
              .x = 1.0,
              .y = 2.0,
              .z = 3.0,
              .rotation_x = 0.0,
              .rotation_y = 0.0,
              .rotation_z = 0.0,
              .rotation_w = 1.0})};
}

flight::Ref<types::RigidBody3D> test_body(double x, double velocity_x,
                                          flight::String type =
                                              flight::String("dynamic")) {
  auto body = physics::create_rigid_body3_d(type);
  body->x = x;
  body->velocity_x = velocity_x;
  body->mass = 2.0;
  body->inertia_xx = 1.0;
  body->inertia_yy = 1.0;
  body->inertia_zz = 1.0;
  body->linear_damping = 0.0;
  body->angular_damping = 0.0;
  body->gravity_scale = 1.0;
  return body;
}

void wire_layout_and_shape_encoding() {
  auto commands = abi::create_physics3_dabi_command_buffer(512.0);
  flight::DataView view(commands->data.buffer,
                        static_cast<double>(commands->data.byte_offset),
                        static_cast<double>(commands->data.byte_length));
  bool exact = commands->byte_length == 16.0 && commands->command_count == 0.0 &&
               view.get_uint32(0.0, true) == abi::physics3_dabi_command_magic &&
               view.get_uint32(4.0, true) == abi::physics3_dabi_version &&
               view.get_uint32(8.0, true) == 16.0 &&
               view.get_uint32(12.0, true) == 0.0;

  exact = exact &&
          abi::write_physics3_dabi_set_gravity_command(commands, 1.25, -2.5,
                                                       3.75) &&
          commands->byte_length == 56.0 && commands->command_count == 1.0 &&
          view.get_uint32(16.0, true) ==
              abi::physics3_dabi_command_kind->set_gravity &&
          view.get_uint32(20.0, true) == 40.0 &&
          view.get_uint32(24.0, true) == 0.0 &&
          view.get_uint32(28.0, true) == 0.0 &&
          close(view.get_float64(32.0, true), 1.25) &&
          close(view.get_float64(40.0, true), -2.5) &&
          close(view.get_float64(48.0, true), 3.75);
  if (mutate_layout) exact = false;
  check(exact,
        "command buffers preserve magic, version, lengths, counts, record offsets, and little-endian gravity payload values");

  struct ShapeCase {
    types::CollisionColliderShape3D shape;
    double kind;
    double scalar_count;
    double integer_count;
    double version;
  };
  const std::array<ShapeCase, 9> cases{{
      {sphere_shape(), abi::physics3_dabi_shape_kind->sphere, 4.0, 0.0, 0.0},
      {aabb_shape(), abi::physics3_dabi_shape_kind->aabb, 6.0, 0.0, 0.0},
      {box_shape(), abi::physics3_dabi_shape_kind->box, 10.0, 0.0, 0.0},
      {capsule_shape(), abi::physics3_dabi_shape_kind->capsule, 7.0, 0.0,
       0.0},
      {cylinder_shape(), abi::physics3_dabi_shape_kind->cylinder, 7.0, 0.0,
       0.0},
      {cone_shape(), abi::physics3_dabi_shape_kind->cone, 7.0, 0.0, 0.0},
      {convex_shape(), abi::physics3_dabi_shape_kind->convex, 6.0, 0.0, 0.0},
      {triangle_mesh_shape(), abi::physics3_dabi_shape_kind->triangle_mesh,
       16.0, 3.0, 7.0},
      {heightfield_shape(), abi::physics3_dabi_shape_kind->heightfield, 13.0,
       2.0, 9.0},
  }};

  bool shapes_exact = true;
  for (std::size_t index = 0; index < cases.size(); ++index) {
    auto shape_commands = abi::create_physics3_dabi_command_buffer(512.0);
    auto collider = abi::create_abi_collider(cases[index].shape, 2.0, 0.25,
                                             0.5, 3.0, 5.0, -7.0, true);
    shapes_exact =
        shapes_exact && abi::write_physics3_dabi_set_collider_command(
                            shape_commands, static_cast<double>(index + 1), 1.0,
                            readonly_view(collider));
    flight::DataView shape_view(
        shape_commands->data.buffer,
        static_cast<double>(shape_commands->data.byte_offset),
        static_cast<double>(shape_commands->data.byte_length));
    constexpr double shape_offset = 72.0;
    shapes_exact =
        shapes_exact &&
        shape_view.get_uint32(shape_offset, true) == cases[index].kind &&
        shape_view.get_uint32(shape_offset + 4.0, true) ==
            cases[index].scalar_count &&
        shape_view.get_uint32(shape_offset + 8.0, true) ==
            cases[index].integer_count &&
        shape_view.get_uint32(shape_offset + 12.0, true) ==
            cases[index].version &&
        std::fmod(shape_commands->byte_length, 8.0) == 0.0 &&
        shape_view.get_uint32(16.0 + 4.0, true) ==
            shape_commands->byte_length - 16.0;
  }
  if (mutate_shapes) shapes_exact = false;
  check(shapes_exact,
        "all nine collider variants retain kind, scalar count, integer count, version, record length, and eight-byte alignment");

  auto too_small = abi::create_physics3_dabi_command_buffer(32.0);
  const double before_length = too_small->byte_length;
  const double before_count = too_small->command_count;
  check(!abi::write_physics3_dabi_set_gravity_command(too_small, 1.0, 2.0,
                                                      3.0) &&
            too_small->byte_length == before_length &&
            too_small->command_count == before_count,
        "an undersized command buffer rejects the write without publishing a partial record");
}

void persistent_worlds_transactions_and_step() {
  auto first_abi = abi::create_reference_physics3_dabi();
  auto second_abi = abi::create_reference_physics3_dabi();
  const double first = first_abi->create_world();
  const double independent = second_abi->create_world();
  bool identity = first == 1.0 && independent == 1.0 &&
                  first_abi->get_world_status(first) == flight::String("Ready") &&
                  first_abi->destroy_world(first) &&
                  first_abi->get_world_status(first) == flight::String("Stale");
  const double world = first_abi->create_world();
  identity = identity && world == 2.0 && !first_abi->destroy_world(first);
  if (mutate_identity) identity = false;
  check(identity,
        "world handles are instance-local, monotonic, non-reused, and stale immediately after destruction");

  const double expected_capabilities = flight::bitwise_or(
      flight::bitwise_or(abi::physics3_dabi_capability->persistent_worlds,
                         abi::physics3_dabi_capability->queries),
      abi::physics3_dabi_capability->selective_readback);
  check(first_abi->version == abi::physics3_dabi_version &&
            first_abi->capabilities == expected_capabilities &&
            !first_abi->query_shape_cast,
        "the runtime advertises only persistent worlds, supported queries, and selective readback while shape cast remains unavailable");

  auto transaction = abi::create_physics3_dabi_command_buffer(1024.0);
  auto body_22 = test_body(10.0, 0.0, flight::String("static"));
  bool wrote = abi::write_physics3_dabi_set_body_command(
                   transaction, 22.0, readonly_view(body_22)) &&
               abi::write_physics3_dabi_apply_force_command(
                   transaction, 999.0, 1.0, 0.0, 0.0);
  auto result = abi::create_physics3_dabi_execution_result();
  const bool transaction_result =
      first_abi->execute(world, readonly_view(transaction), result);
  auto bodies = abi::create_physics3_dabi_body_buffer(1.0);
  const bool read_after_failure =
      first_abi->read_bodies(world, std::nullopt, bodies);
  bool transaction_exact = wrote && !transaction_result && read_after_failure &&
                           result->status == flight::String("MissingBody") &&
                           result->command_index == 1.0 &&
                           result->command_kind ==
                               abi::physics3_dabi_command_kind->apply_force &&
                           bodies->required_count == 1.0 && bodies->count == 1.0 &&
                           bodies->ids.get_index(0.0) == 22.0;
  if (mutate_transaction) transaction_exact = false;
  check(transaction_exact,
        "execution commits the valid prefix and reports the exact failing command index and kind");

  auto setup = abi::create_physics3_dabi_command_buffer(1024.0);
  auto body_11 = test_body(1.0, 2.0);
  wrote = abi::write_physics3_dabi_set_gravity_command(setup, 0.0, 0.0, 0.0) &&
          abi::write_physics3_dabi_set_body_command(setup, 11.0,
                                                    readonly_view(body_11));
  check(wrote && first_abi->execute(world, readonly_view(setup), result) &&
            result->status == flight::String("Complete"),
        "valid gravity and body records execute to completion");

  bodies = abi::create_physics3_dabi_body_buffer(1.0);
  check(first_abi->read_bodies(world, std::nullopt, bodies) &&
            bodies->required_count == 2.0 && bodies->count == 1.0 &&
            bodies->ids.get_index(0.0) == 11.0,
        "bounded readback reports the full required count and returns sorted ID prefixes");

  flight::Uint32Array selection(3.0);
  selection.set_index(0.0, 22.0);
  selection.set_index(1.0, 999.0);
  selection.set_index(2.0, 11.0);
  auto selected = abi::create_physics3_dabi_body_buffer(2.0);
  check(first_abi->read_bodies(world, std::optional{selection}, selected) &&
            selected->required_count == 2.0 && selected->count == 2.0 &&
            selected->ids.get_index(0.0) == 22.0 &&
            selected->ids.get_index(1.0) == 11.0,
        "selective readback skips missing IDs and preserves caller order");

  const auto step_status = first_abi->step(world, 0.5, std::nullopt);
  auto stepped = abi::create_physics3_dabi_body_buffer(1.0);
  flight::Uint32Array only_11(1.0);
  only_11.set_index(0.0, 11.0);
  const bool read_stepped = first_abi->read_bodies(
      world, std::optional{only_11}, stepped);
  bool free_flight = step_status == flight::String("Complete") && read_stepped &&
                     stepped->count == 1.0 &&
                     close(stepped->values.get_index(
                               abi::physics3_dabi_body_value->x),
                           2.0) &&
                     close(stepped->values.get_index(
                               abi::physics3_dabi_body_value->velocity_x),
                           2.0) &&
                     stepped->values.get_index(
                         abi::physics3_dabi_body_value->force_x) == 0.0;
  if (mutate_free_flight) free_flight = false;
  check(free_flight,
        "collider-free stepping integrates velocity and pose for the requested timestep and clears forces");

  auto corrupt = abi::create_physics3_dabi_command_buffer(512.0);
  check(abi::write_physics3_dabi_set_body_command(corrupt, 33.0,
                                                  readonly_view(body_11)),
        "the invalid-buffer fixture first writes a valid body command");
  flight::DataView corrupt_view(
      corrupt->data.buffer, static_cast<double>(corrupt->data.byte_offset),
      static_cast<double>(corrupt->data.byte_length));
  corrupt_view.set_uint32(0.0, 0.0, true);
  check(!first_abi->execute(world, readonly_view(corrupt), result) &&
            result->status == flight::String("InvalidBuffer"),
        "a corrupt command header is rejected before it can mutate world state");
  bodies = abi::create_physics3_dabi_body_buffer(3.0);
  check(first_abi->read_bodies(world, std::nullopt, bodies) &&
            bodies->required_count == 2.0,
        "invalid-buffer rejection leaves the existing body set unchanged");
}

void collider_queries_and_honest_joint_boundary() {
  auto runtime = abi::create_reference_physics3_dabi();
  const double world = runtime->create_world();
  auto body = test_body(2.0, 0.0);
  auto setup = abi::create_physics3_dabi_command_buffer(1024.0);
  auto collider = abi::create_abi_collider(sphere_shape(), 1.0, 0.2, 0.0,
                                           1.0, 65535.0, 0.0, false);
  const bool wrote = abi::write_physics3_dabi_set_body_command(
                         setup, 7.0, readonly_view(body)) &&
                     abi::write_physics3_dabi_set_collider_command(
                         setup, 9.0, 7.0, readonly_view(collider));
  auto result = abi::create_physics3_dabi_execution_result();
  check(wrote && runtime->execute(world, readonly_view(setup), result),
        "a body and exact-owner sphere collider execute through the ABI");

  auto hits = abi::create_physics3_dabi_query_buffer(2.0);
  const bool point_ok =
      abi::query_physics3_dabi_point(readonly_view(runtime), world, 2.0, 0.0,
                                     0.0, hits);
  bool query_exact = point_ok && hits->required_count == 1.0 &&
                     hits->count == 1.0 &&
                     hits->body_ids.get_index(0.0) == 7.0 &&
                     hits->collider_ids.get_index(0.0) == 9.0;
  hits = abi::create_physics3_dabi_query_buffer(2.0);
  query_exact =
      query_exact && abi::query_physics3_dabi_ray_closest(
                         readonly_view(runtime), world, -5.0, 0.0, 0.0, 1.0,
                         0.0, 0.0, hits, 20.0) &&
      hits->count == 1.0 && hits->body_ids.get_index(0.0) == 7.0 &&
      hits->collider_ids.get_index(0.0) == 9.0 &&
      close(hits->values.get_index(abi::physics3_dabi_query_value->fraction),
            6.0);
  check(query_exact,
        "point and closest-ray queries preserve ABI body/collider identity and hit values");
  check(runtime->step(world, 0.25, std::nullopt) == flight::String("Declined"),
        "a collider-bearing world declines stepping while core collision stepping is unavailable");

  auto joint = abi::create_physics3_dabi_command_buffer(512.0);
  auto pending = abi::begin_command(
      joint, abi::physics3_dabi_command_kind->set_joint,
      abi::physics3_dabi_command_byte_length->set_joint, 5.0, 0.0);
  check(pending.has_value(),
        "the unsupported-joint fixture reserves an exact-size wire record");
  if (pending.has_value()) {
    abi::finish_command(joint, readonly_view(pending.value()));
  }
  const bool joint_result = runtime->execute(world, readonly_view(joint), result);
  auto joints = abi::create_physics3_dabi_joint_buffer(1.0);
  bool unsupported = !joint_result &&
                     result->status == flight::String("UnsupportedJoint") &&
                     result->command_index == 0.0 &&
                     runtime->read_joints(world, joints) &&
                     joints->count == 0.0 && joints->required_count == 0.0;
  if (mutate_unsupported) unsupported = false;
  check(unsupported,
        "SetJoint is reported as unsupported before mutation and readback does not publish a fabricated joint");
}

}  // namespace

int main(int argc, char** argv) {
  const flight::String mutation =
      argc > 1 ? flight::String(argv[1]) : flight::String();
  mutate_layout = mutation == flight::String("--mutate-layout");
  mutate_transaction = mutation == flight::String("--mutate-transaction");
  mutate_identity = mutation == flight::String("--mutate-identity");
  mutate_free_flight = mutation == flight::String("--mutate-free-flight");
  mutate_shapes = mutation == flight::String("--mutate-shapes");
  mutate_unsupported = mutation == flight::String("--mutate-unsupported");

  wire_layout_and_shape_encoding();
  persistent_worlds_transactions_and_step();
  collider_queries_and_honest_joint_boundary();

  if (failures != 0) {
    std::cerr << failures << " physics3d-abi assertion(s) failed\n";
    return 1;
  }
  std::cout << "physics3d-abi current-pin parity evidence passed\n";
  return 0;
}
