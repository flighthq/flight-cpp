// The generated @flighthq/physics2d-abi wire contract at a627849, exercised natively: the layout constants
// (written out literally, as upstream does, so changing one is a deliberate edit in two places), the readback
// and command buffers, and every command writer the module can link. Ported from physics2DAbiLayout.test.ts,
// physics2DAbiBuffer.test.ts and physics2DAbiCommand.test.ts.
//
// Not here, by design:
// - writePhysics2DAbiSetJointCommand. Its encodeJointFlags and getJointKindValues are refused, and the override
//   declares them without bodies (overrides/manifest.json, physics2d-abi-variant-shape-encoding), so a call does
//   not link. Upstream's three SetJoint cases wait on that.
// - Worlds, execution, readback and queries. physics2DAbi.ts and the query wrappers drive a Physics2DAbi, whose
//   only implementation is the reference ABI over @flighthq/physics2d. That header does not compile at this pin,
//   and a stand-in ABI would be invented physics.
// Bodies, colliders and solver configs are plain flight::types rows here, built with the values each upstream
// case asserts, because physics2d's constructors live in the same non-compiling core.
#include <flight/collision/collide_contact_manifold2_d.hpp>
#include <flight/collision/contact_manifold2_d.hpp>
#include <flight/physics2d_abi/physics2_dabi_buffer.hpp>
#include <flight/physics2d_abi/physics2_dabi_command.hpp>
#include <flight/physics2d_abi/physics2_dabi_layout.hpp>

#include <cmath>
#include <iostream>
#include <limits>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}

template <typename Function>
bool throws_range_error(Function&& function) {
  try {
    function();
  } catch (const flight::RangeError&) {
    return true;
  } catch (...) {
    return false;
  }
  return false;
}

namespace abi = flight::physics2d_abi;
namespace types = flight::types;
using Shape = types::CollisionBuiltInShape2D;

template <typename Pointer>
auto view(const Pointer& value) {
  using Element = typename Pointer::element_type;
  return flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<Element>>>>(value);
}

// --- shapes, rows -----------------------------------------------------------------------------------------

Shape circle(double x, double y, double radius) {
  return flight::make_ref<types::x_y_radius_kind_90fbb3b4c4569518>(types::x_y_radius_kind_90fbb3b4c4569518{.x = x, .y = y, .radius = radius, .kind = flight::String("circle")});
}
Shape aabb(double min_x, double min_y, double max_x, double max_y) {
  return flight::make_ref<types::min_x_min_y_max_x_max_y_kind_329d6080b4f31db4>(types::min_x_min_y_max_x_max_y_kind_329d6080b4f31db4{.min_x = min_x, .min_y = min_y, .max_x = max_x, .max_y = max_y, .kind = flight::String("aabb")});
}
Shape obb(double x, double y, double half_w, double half_h, double rotation) {
  return flight::make_ref<types::x_y_half_w_half_h_rotation_kind_8ffcea5c657a2268>(types::x_y_half_w_half_h_rotation_kind_8ffcea5c657a2268{.x = x, .y = y, .half_w = half_w, .half_h = half_h, .rotation = rotation, .kind = flight::String("obb")});
}
Shape capsule(double x0, double y0, double x1, double y1, double radius) {
  return flight::make_ref<types::x0_y0_x1_y1_radius_kind_768ba062d07f3ff6>(types::x0_y0_x1_y1_radius_kind_768ba062d07f3ff6{.x0 = x0, .y0 = y0, .x1 = x1, .y1 = y1, .radius = radius, .kind = flight::String("capsule")});
}
Shape polygon(std::vector<double> points) {
  flight::Array<double> array;
  for (double value : points) array.push(value);
  return flight::make_ref<types::points_kind_9b00ebdad27cdb4e>(types::points_kind_9b00ebdad27cdb4e{.points = array, .kind = flight::String("polygon")});
}
Shape segment(double x0, double y0, double x1, double y1) {
  return flight::make_ref<types::x0_y0_x1_y1_kind_d4f616534491406f>(types::x0_y0_x1_y1_kind_d4f616534491406f{.x0 = x0, .y0 = y0, .x1 = x1, .y1 = y1, .kind = flight::String("segment")});
}
Shape point(double x, double y) {
  return flight::make_ref<types::x_y_kind_35ad667397176b0f>(types::x_y_kind_35ad667397176b0f{.x = x, .y = y, .kind = flight::String("point")});
}

flight::Ref<types::Physics2DCollider> collider(const Shape& shape, bool sensor = false, double category = 1,
                                               double mask = 0xffff, double group = 0) {
  return flight::make_ref<types::Physics2DCollider>(types::Physics2DCollider{
      .local = shape,
      .world = shape,
      .material = flight::make_ref<types::Physics2DMaterial>(types::Physics2DMaterial{.density = 1, .friction = 0.3, .restitution = 0.1}),
      .filter = flight::make_ref<types::Physics2DCollisionFilter>(types::Physics2DCollisionFilter{.category_bits = category, .mask_bits = mask, .group_index = group}),
      .sensor = sensor,
  });
}

// Every scalar distinct, so a field written into a neighbour's slot cannot pass.
flight::Ref<types::RigidBody2D> body(const char* type = "dynamic") {
  return flight::make_ref<types::RigidBody2D>(types::RigidBody2D{
      .type = flight::String(type),
      .x = 3, .y = -4, .angle = 0.5,
      .velocity_x = 1, .velocity_y = 2, .angular_velocity = 3,
      .force_x = 4.25, .force_y = 5.25, .torque = 6.25,
      .mass = 7.5, .inverse_mass = 99, .inertia = 8.5, .inverse_inertia = 98,
      .center_x = 9.75, .center_y = 10.75,
      .linear_damping = 0.125, .angular_damping = 0.375, .gravity_scale = 1.5,
      .fixed_rotation = false, .bullet = false, .sleeping = false, .sleep_enabled = true,
      .sleep_timer = 0.0625,
  });
}

// physics2d's createPhysics2DSolverConfig defaults at a627849.
flight::Ref<types::Physics2DSolverConfig> solver_config() {
  return flight::make_ref<types::Physics2DSolverConfig>(types::Physics2DSolverConfig{
      .allow_sleeping = true,
      .sleep_linear_threshold = 0.01,
      .sleep_angular_threshold = (2 * M_PI) / 180,
      .time_to_sleep = 0.5,
      .velocity_iterations = 10,
      .position_iterations = 3,
      .penetration_slop = 0.005,
      .position_correction = 0.2,
      .restitution_threshold = 1,
      .warm_starting = true,
      .continuous_collision = true,
      .max_ccd_substeps = 8,
      .max_ccd_rotation_substeps = 64,
  });
}

// --- reading the stream -----------------------------------------------------------------------------------

using Buffer = flight::Ref<types::Physics2DAbiCommandBuffer>;

flight::DataView data_view(const Buffer& buffer) {
  return flight::DataView(buffer->data.buffer, buffer->data.byte_offset, buffer->data.byte_length);
}

struct Record {
  flight::DataView view;
  double kind;
  double byte_length;
  double object_id;
  double related_id;
  double payload;
};

Record record(const Buffer& buffer, double at = abi::physics2_dabi_command_header_byte_length) {
  auto view = data_view(buffer);
  return Record{view, view.get_uint32(at, true), view.get_uint32(at + 4, true), view.get_uint32(at + 8, true),
                view.get_uint32(at + 12, true), at + abi::physics2_dabi_command_record_header_byte_length};
}

Buffer command_buffer(double capacity) { return abi::create_physics2_dabi_command_buffer(capacity); }

bool header_is(const Buffer& buffer, double byte_length, double count) {
  auto view = data_view(buffer);
  return view.get_uint32(0, true) == abi::physics2_dabi_command_magic && view.get_uint32(4, true) == 1 &&
         view.get_uint32(8, true) == byte_length && view.get_uint32(12, true) == count &&
         buffer->byte_length == byte_length && buffer->command_count == count;
}

// --- physics2DAbiLayout -----------------------------------------------------------------------------------

void layout() {
  const auto& body_flag = abi::physics2_dabi_body_flag;
  const auto& body_type = abi::physics2_dabi_body_type;
  check(body_flag->type_mask == 3 && body_flag->fixed_rotation == 4 && body_flag->bullet == 8 &&
            body_flag->sleeping == 16 && body_flag->sleep_enabled == 32,
        "body flags: the type in the low two bits, one bit each above");
  check(body_type->dynamic == 0 && body_type->kinematic == 1 && body_type->static_ == 2, "body types 0, 1, 2");
  for (double type : {body_type->dynamic, body_type->kinematic, body_type->static_}) {
    check((static_cast<std::uint32_t>(type) & static_cast<std::uint32_t>(body_flag->type_mask)) == type,
          "every body type fits the type mask");
  }

  const auto& value = abi::physics2_dabi_body_value;
  check(abi::physics2_dabi_body_value_stride == 17, "seventeen body scalars");
  const double order[] = {value->x, value->y, value->angle, value->velocity_x, value->velocity_y,
                          value->angular_velocity, value->force_x, value->force_y, value->torque, value->mass,
                          value->inertia, value->center_x, value->center_y, value->linear_damping,
                          value->angular_damping, value->gravity_scale, value->sleep_timer};
  bool in_order = true;
  for (int i = 0; i < 17; ++i) in_order = in_order && order[i] == i;
  check(in_order, "body scalars are indexed 0..16 in wire order");

  const auto& capability = abi::physics2_dabi_capability;
  check(capability->contact_hooks == 1 && capability->persistent_worlds == 2 && capability->queries == 4 &&
            capability->selective_readback == 8,
        "one bit per backend capability");

  const auto& length = abi::physics2_dabi_command_byte_length;
  const double header = abi::physics2_dabi_command_record_header_byte_length;
  bool aligned = true;
  for (double bytes : {length->set_gravity, length->set_solver_config, length->set_body, length->destroy_body,
                       length->set_collider_minimum, length->destroy_collider, length->set_joint, length->destroy_joint,
                       length->body_action, length->wake_body}) {
    aligned = aligned && std::fmod(bytes, 8) == 0 && bytes >= header;
  }
  check(aligned, "every record is eight-aligned and at least a header");
  check(length->set_gravity == header + 16 && length->set_body == header + 8 + 17 * 8 &&
            length->set_joint == header + 64 + 9 * 8 && length->set_solver_config == header + 32 + 6 * 8 &&
            length->body_action == header + 32 && length->set_collider_minimum == header + 40 + 16,
        "record lengths agree with their payload layouts");
  check(length->set_gravity == 32 && length->set_solver_config == 96 && length->set_body == 160 &&
            length->destroy_body == 16 && length->set_collider_minimum == 72 && length->destroy_collider == 16 &&
            length->set_joint == 152 && length->destroy_joint == 16 && length->body_action == 48 &&
            length->wake_body == 16,
        "record lengths, literally");

  // The payload offset tables, literally: the reader of a record indexes by these.
  const auto& body_payload = abi::physics2_dabi_set_body_payload_offset;
  check(body_payload->flags == 0 && body_payload->reserved == 4 && body_payload->values == 8, "SetBody payload offsets");
  const auto& collider_payload = abi::physics2_dabi_set_collider_payload_offset;
  check(collider_payload->flags == 0 && collider_payload->category_bits == 4 && collider_payload->mask_bits == 8 &&
            collider_payload->group_index == 12 && collider_payload->density == 16 && collider_payload->friction == 24 &&
            collider_payload->restitution == 32 && collider_payload->shape == 40,
        "SetCollider payload offsets");
  const auto& joint_payload = abi::physics2_dabi_set_joint_payload_offset;
  check(joint_payload->kind == 0 && joint_payload->body_a == 4 && joint_payload->body_b == 8 && joint_payload->flags == 12 &&
            joint_payload->common_values == 16 && joint_payload->kind_values == 64,
        "SetJoint payload offsets");
  const auto& solver_payload = abi::physics2_dabi_set_solver_config_payload_offset;
  check(solver_payload->flags == 0 && solver_payload->reserved0 == 4 && solver_payload->max_ccd_substeps == 8 &&
            solver_payload->max_ccd_rotation_substeps == 12 && solver_payload->velocity_iterations == 16 &&
            solver_payload->position_iterations == 20 && solver_payload->reserved1 == 24 &&
            solver_payload->reserved2 == 28 && solver_payload->values == 32,
        "SetSolverConfig payload offsets");

  const auto& kind = abi::physics2_dabi_command_kind;
  check(kind->set_gravity == 1 && kind->set_solver_config == 2 && kind->set_body == 3 && kind->destroy_body == 4 &&
            kind->set_collider == 5 && kind->destroy_collider == 6 && kind->set_joint == 7 &&
            kind->destroy_joint == 8 && kind->apply_force == 9 && kind->apply_force_at_point == 10 &&
            kind->apply_linear_impulse == 11 && kind->apply_linear_impulse_at_point == 12 &&
            kind->apply_torque == 13 && kind->wake_body == 14,
        "stable command discriminants");

  check(abi::physics2_dabi_command_magic == 0x41443250, "the magic");
  flight::Uint8Array magic(4.0);
  flight::DataView(magic.buffer).set_uint32(0, abi::physics2_dabi_command_magic, true);
  check(magic.element(0) == 'P' && magic.element(1) == '2' && magic.element(2) == 'D' && magic.element(3) == 'A',
        "the magic spells P2DA little-endian");
  check(abi::physics2_dabi_version == 1 && abi::physics2_dabi_command_header_byte_length == 16, "version and header");
  const auto& offset = abi::physics2_dabi_command_header_offset;
  const auto& record_offset = abi::physics2_dabi_command_record_offset;
  check(offset->magic == 0 && offset->version == 4 && offset->byte_length == 8 && offset->command_count == 12 &&
            record_offset->kind == 0 && record_offset->byte_length == 4 && record_offset->object_id == 8 &&
            record_offset->related_id == 12,
        "header and record offsets");

  const auto& contact_flag = abi::physics2_dabi_contact_flag;
  check(contact_flag->enabled == 1 && contact_flag->sensor == 2 && contact_flag->touching == 4, "contact flags");
  const auto& contact_id = abi::physics2_dabi_contact_id;
  const auto& contact_value = abi::physics2_dabi_contact_value;
  const auto& point_value = abi::physics2_dabi_contact_point_value;
  check(abi::physics2_dabi_max_contact_points == 2 && abi::physics2_dabi_contact_id_stride == 4 &&
            abi::physics2_dabi_contact_value_stride == 4 && abi::physics2_dabi_contact_point_value_stride == 7,
        "contact strides and the two-point planar bound");
  check(contact_id->body_a == 0 && contact_id->body_b == 1 && contact_id->collider_a == 2 &&
            contact_id->collider_b == 3 && contact_value->normal_x == 0 && contact_value->normal_y == 1 &&
            contact_value->friction == 2 && contact_value->restitution == 3 && point_value->x == 0 &&
            point_value->y == 1 && point_value->depth == 2 && point_value->relative_ax == 3 &&
            point_value->relative_ay == 4 && point_value->relative_bx == 5 && point_value->relative_by == 6,
        "contact id, value and point indices");

  const auto& joint_flag = abi::physics2_dabi_joint_flag;
  check(joint_flag->collide_connected == 1 && joint_flag->broken == 2 && joint_flag->enable_motor == 4 &&
            joint_flag->enable_limit == 8 && joint_flag->enable_limit_spring == 16,
        "joint flags");
  check(joint_flag->linear_coordinate_a == joint_flag->enable_motor &&
            joint_flag->linear_coordinate_b == joint_flag->enable_limit,
        "the gear coordinate bits overlay the enable bits");

  const auto& joint_kind = abi::physics2_dabi_joint_kind;
  check(joint_kind->distance == 1 && joint_kind->revolute == 2 && joint_kind->prismatic == 3 && joint_kind->weld == 4 &&
            joint_kind->wheel == 5 && joint_kind->rope == 6 && joint_kind->mouse == 7 && joint_kind->pulley == 8 &&
            joint_kind->gear == 9 && abi::physics2_dabi_joint_kind_value_count == 9,
        "all nine joint kinds");
  const auto& common = abi::physics2_dabi_joint_common_value;
  const auto& reaction = abi::physics2_dabi_joint_value;
  check(common->local_anchor_ax == 0 && common->local_anchor_ay == 1 && common->local_anchor_bx == 2 &&
            common->local_anchor_by == 3 && common->break_force == 4 && common->break_torque == 5 &&
            reaction->force_x == 0 && reaction->force_y == 1 && reaction->torque == 2 &&
            abi::physics2_dabi_joint_value_stride == 3,
        "joint common and reaction indices");

  const auto& query = abi::physics2_dabi_query_value;
  check(abi::physics2_dabi_query_value_stride == 5 && query->fraction == 0 && query->x == 1 && query->y == 2 &&
            query->normal_x == 3 && query->normal_y == 4,
        "query rows carry fraction, point and normal");

  const auto& shape = abi::physics2_dabi_shape_kind;
  const auto& shape_offset = abi::physics2_dabi_shape_header_offset;
  check(shape->circle == 1 && shape->aabb == 2 && shape->obb == 3 && shape->capsule == 4 && shape->polygon == 5 &&
            shape->segment == 6 && shape->point == 7,
        "all seven shape kinds, including the two area-less ones");
  check(abi::physics2_dabi_shape_header_byte_length == 16 && shape_offset->kind == 0 && shape_offset->scalar_count == 4 &&
            shape_offset->integer_count == 8 && shape_offset->version == 12 && shape_offset->scalars == 16,
        "the shape header");

  const auto& solver_flag = abi::physics2_dabi_solver_config_flag;
  const auto& solver_offset = abi::physics2_dabi_set_solver_config_payload_offset;
  const auto& solver_value = abi::physics2_dabi_solver_config_value;
  check(solver_flag->allow_sleeping == 1 && solver_flag->continuous_collision == 2 && solver_flag->warm_starting == 4 &&
            solver_offset->reserved0 == 4,
        "solver flags and the zero substep word");
  check(solver_value->sleep_linear_threshold == 0 && solver_value->sleep_angular_threshold == 1 &&
            solver_value->time_to_sleep == 2 && solver_value->penetration_slop == 3 &&
            solver_value->position_correction == 4 && solver_value->restitution_threshold == 5,
        "solver value indices");
}

// Moves a shape by (dx, dy), additionally rotating the kinds that carry an angle (upstream's translateShape).
Shape translate(const Shape& shape, double dx, double dy, double rotation) {
  const double c = std::cos(rotation);
  const double s = std::sin(rotation);
  return std::visit(
      [&](const auto& value) -> Shape {
        using T = typename std::remove_cvref_t<decltype(value)>::element_type;
        if constexpr (std::is_same_v<T, types::x_y_radius_kind_90fbb3b4c4569518>) {
          return circle(value->x + dx, value->y + dy, value->radius);
        } else if constexpr (std::is_same_v<T, types::min_x_min_y_max_x_max_y_kind_329d6080b4f31db4>) {
          return aabb(value->min_x + dx, value->min_y + dy, value->max_x + dx, value->max_y + dy);
        } else if constexpr (std::is_same_v<T, types::x_y_half_w_half_h_rotation_kind_8ffcea5c657a2268>) {
          return obb(value->x + dx, value->y + dy, value->half_w, value->half_h, rotation);
        } else if constexpr (std::is_same_v<T, types::x0_y0_x1_y1_radius_kind_768ba062d07f3ff6>) {
          return capsule(value->x0 * c - value->y0 * s + dx, value->x0 * s + value->y0 * c + dy,
                         value->x1 * c - value->y1 * s + dx, value->x1 * s + value->y1 * c + dy, value->radius);
        } else if constexpr (std::is_same_v<T, types::points_kind_9b00ebdad27cdb4e>) {
          std::vector<double> points;
          for (std::size_t i = 0; i + 1 < value->points.size(); i += 2) {
            const double x = value->points.element(static_cast<double>(i));
            const double y = value->points.element(static_cast<double>(i + 1));
            points.push_back(x * c - y * s + dx);
            points.push_back(x * s + y * c + dy);
          }
          return polygon(points);
        } else {
          return shape;
        }
      },
      shape);
}

void max_contact_points_sweep() {
  // Swept rather than argued: slide every pair across each other on both axes and at several angles, and
  // the widest manifold must be exactly the two points the ABI reserves.
  const std::vector<Shape> shapes{
      circle(0, 0, 0.6), aabb(-0.6, -0.6, 0.6, 0.6), obb(0, 0, 0.6, 0.4, 0), capsule(-0.5, 0, 0.5, 0, 0.35),
      polygon({-0.6, -0.5, 0.6, -0.5, 0.7, 0.4, -0.4, 0.6})};
  auto manifold = flight::collision::create_collision_contact_manifold2_d();
  double widest = 0;
  int overlaps = 0;
  for (const auto& a : shapes) {
    for (const auto& b : shapes) {
      for (int step = -12; step <= 12; ++step) {
        const double offset = step * 0.1;
        for (double rotation : {0.0, 0.37, M_PI / 4, 1.2}) {
          if (!flight::collision::collide_contact_manifold2_d(a, translate(b, offset, offset * 0.5, rotation), manifold)) {
            continue;
          }
          ++overlaps;
          widest = std::max(widest, manifold->point_count);
        }
      }
    }
  }
  check(overlaps > 100, "the sweep actually collides shapes");
  check(widest == abi::physics2_dabi_max_contact_points, "no built-in 2D manifold exceeds two points, and some reach it");
}

// --- physics2DAbiBuffer -----------------------------------------------------------------------------------

void buffers() {
  auto buffer = command_buffer(256);
  check(abi::write_physics2_dabi_set_gravity_command(buffer, 0, -9.81) && buffer->command_count == 1, "one command");
  abi::clear_physics2_dabi_command_buffer(buffer);
  check(header_is(buffer, 16, 0), "clear rewrites the header so a reused buffer carries no stale count");
  check(abi::write_physics2_dabi_set_gravity_command(buffer, 0, -9.81) && header_is(buffer, 48, 1),
        "a cleared buffer writes from the first record again");

  auto bodies = abi::create_physics2_dabi_body_buffer(3);
  check(bodies->ids.length == 3 && bodies->flags.length == 3 && bodies->values.length == 3 * 17 &&
            bodies->count == 0 && bodies->required_count == 0,
        "a body buffer is sized to its capacity");
  check(throws_range_error([] { (void)abi::create_physics2_dabi_body_buffer(-1); }) &&
            throws_range_error([] { (void)abi::create_physics2_dabi_body_buffer(1.5); }) &&
            throws_range_error([] { (void)abi::create_physics2_dabi_body_buffer(std::nan("")); }) &&
            throws_range_error([] { (void)abi::create_physics2_dabi_body_buffer(9007199254740992.0); }),
        "a body capacity must be a non-negative safe integer");
  auto empty = abi::create_physics2_dabi_body_buffer(0);
  check(empty->ids.length == 0 && empty->values.length == 0, "a zero capacity is allowed");

  auto fresh = command_buffer(64);
  check(fresh->data.byte_length == 64 && header_is(fresh, 16, 0), "a new command buffer is a valid empty stream");
  check(abi::create_physics2_dabi_command_buffer()->data.byte_length == 4096, "the default capacity is 4096");
  check(throws_range_error([] { (void)command_buffer(15); }) && !throws_range_error([] { (void)command_buffer(16); }) &&
            throws_range_error([] { (void)command_buffer(16.5); }),
        "a command capacity must hold the header and be an integer");

  auto contacts = abi::create_physics2_dabi_contact_buffer(2, 5);
  check(contacts->ids.length == 8 && contacts->flags.length == 2 && contacts->point_starts.length == 2 &&
            contacts->point_counts.length == 2 && contacts->values.length == 8 &&
            contacts->point_feature_ids.length == 5 && contacts->point_values.length == 35 && contacts->count == 0 &&
            contacts->point_count == 0 && contacts->required_count == 0 && contacts->required_point_count == 0,
        "contact rows and point rows are sized independently");
  check(throws_range_error([] { (void)abi::create_physics2_dabi_contact_buffer(1, -1); }),
        "a point capacity is checked too");

  auto result = abi::create_physics2_dabi_execution_result();
  check(result->status == flight::String("Complete") && result->byte_offset == 16 && result->command_index == 0 &&
            result->command_kind == 0,
        "an execution result starts at the first record");

  auto joints = abi::create_physics2_dabi_joint_buffer(4);
  check(joints->ids.length == 4 && joints->flags.length == 4 && joints->values.length == 12, "three reaction values per joint");
  auto queries = abi::create_physics2_dabi_query_buffer(4);
  check(queries->body_ids.length == 4 && queries->collider_ids.length == 4 && queries->values.length == 20,
        "five geometric values per hit");
  check(throws_range_error([] { (void)abi::create_physics2_dabi_joint_buffer(-2); }) &&
            throws_range_error([] { (void)abi::create_physics2_dabi_query_buffer(0.5); }),
        "joint and query capacities are checked");

  auto shrinking = command_buffer(64);
  const double before = abi::get_physics2_dabi_command_buffer_remaining_byte_length(view(shrinking));
  check(before == 48, "the remaining length excludes the header");
  abi::write_physics2_dabi_set_gravity_command(shrinking, 0, -9.81);
  check(abi::get_physics2_dabi_command_buffer_remaining_byte_length(view(shrinking)) == before - 32,
        "the remaining length shrinks by the record");
  shrinking->byte_length = 80;
  check(abi::get_physics2_dabi_command_buffer_remaining_byte_length(view(shrinking)) == 0,
        "the remaining length never goes negative");

  // Each constructor returns its own storage: two buffers never share an array.
  auto first = abi::create_physics2_dabi_body_buffer(1);
  auto second = abi::create_physics2_dabi_body_buffer(1);
  first->values.element(0) = 5.0;
  check(first != second && second->values.element(0) == 0, "two body buffers share nothing");
  auto stream_a = command_buffer(64);
  auto stream_b = command_buffer(64);
  abi::write_physics2_dabi_wake_body_command(stream_a, 1);
  check(stream_b->command_count == 0 && header_is(stream_b, 16, 0), "two command buffers share nothing");
}

// --- physics2DAbiCommand ----------------------------------------------------------------------------------

void collider_lengths() {
  const std::vector<std::pair<Shape, double>> shapes{
      {circle(0, 0, 1), 3},       {aabb(-1, -1, 1, 1), 4},    {obb(0, 0, 1, 2, 0.5), 5}, {capsule(-1, 0, 1, 0, 0.5), 5},
      {polygon({0, 0, 1, 0, 0, 1}), 6}, {segment(0, 0, 1, 1), 4}, {point(2, 3), 2}};
  for (const auto& [shape, scalars] : shapes) {
    auto value = collider(shape);
    const double expected = abi::physics2_dabi_command_byte_length->set_collider_minimum + scalars * 8;
    check(abi::get_physics2_dabi_set_collider_command_byte_length(view(value)) == expected,
          "the collider length is the minimum plus its scalars");
    auto buffer = command_buffer(1024);
    check(abi::write_physics2_dabi_set_collider_command(buffer, 1, 1, view(value)) &&
              buffer->byte_length - 16 == expected,
          "the writer consumes exactly that length");
    auto written = record(buffer);
    check(written.byte_length == expected && written.view.get_uint32(written.payload + 44, true) == scalars &&
              written.view.get_uint32(written.payload + 48, true) == 0 &&
              written.view.get_uint32(written.payload + 52, true) == 0,
          "the shape header names its scalar count, no integers, version zero");
  }
}

void shape_encoding() {
  // Each kind's discriminant and its scalars in source order.
  struct Case {
    Shape shape;
    double kind;
    std::vector<double> scalars;
  };
  const std::vector<Case> cases{
      {circle(1, 2, 3), 1, {1, 2, 3}},
      {aabb(-1, -2, 3, 4), 2, {-1, -2, 3, 4}},
      {obb(1, 2, 3, 4, 0.5), 3, {1, 2, 3, 4, 0.5}},
      {capsule(1, 2, 3, 4, 0.25), 4, {1, 2, 3, 4, 0.25}},
      {polygon({0, 0, 2, 0, 1, 3, -1, 1}), 5, {0, 0, 2, 0, 1, 3, -1, 1}},
      {segment(1, 2, 3, 4), 6, {1, 2, 3, 4}},
      {point(5, 6), 7, {5, 6}}};
  for (const auto& c : cases) {
    auto buffer = command_buffer(1024);
    abi::write_physics2_dabi_set_collider_command(buffer, 1, 1, view(collider(c.shape)));
    auto written = record(buffer);
    const double at = written.payload + 40;
    bool same = written.view.get_uint32(at, true) == c.kind;
    for (std::size_t i = 0; i < c.scalars.size(); ++i) {
      same = same && written.view.get_float64(at + 16 + 8 * static_cast<double>(i), true) == c.scalars[i];
    }
    check(same, "a shape writes its kind and its scalars in source order");
  }

  // A polygon with an odd number of coordinates is not a point list: the record is refused, not truncated.
  auto odd = command_buffer(1024);
  check(abi::get_physics2_dabi_set_collider_command_byte_length(view(collider(polygon({0, 0, 1})))) < 0 &&
            !abi::write_physics2_dabi_set_collider_command(odd, 1, 1, view(collider(polygon({0, 0, 1})))) &&
            header_is(odd, 16, 0),
        "an odd point list is refused");
  auto empty_polygon = command_buffer(1024);
  check(abi::write_physics2_dabi_set_collider_command(empty_polygon, 1, 1, view(collider(polygon({})))) &&
            record(empty_polygon).byte_length == 72,
        "an empty point list is zero scalars, not a refusal");
}

void body_actions() {
  auto at_point = command_buffer(256);
  check(abi::write_physics2_dabi_apply_force_at_point_command(at_point, 2, 5, 6, 7, 8), "force at a point");
  auto written = record(at_point);
  check(written.kind == 10 && written.byte_length == 48 && written.object_id == 2 && written.related_id == 0 &&
            written.view.get_float64(written.payload, true) == 5 && written.view.get_float64(written.payload + 8, true) == 6 &&
            written.view.get_float64(written.payload + 16, true) == 7 &&
            written.view.get_float64(written.payload + 24, true) == 8,
        "the world point is the second pair");

  auto force = command_buffer(256);
  check(abi::write_physics2_dabi_apply_force_command(force, 2, 5, 6), "force");
  written = record(force);
  check(written.kind == 9 && written.view.get_float64(written.payload, true) == 5 &&
            written.view.get_float64(written.payload + 8, true) == 6 &&
            written.view.get_float64(written.payload + 16, true) == 0 &&
            written.view.get_float64(written.payload + 24, true) == 0,
        "a pointless action zeroes the point slots");

  auto impulse_at = command_buffer(256);
  abi::write_physics2_dabi_apply_linear_impulse_at_point_command(impulse_at, 1, 1, 1, 2, 2);
  check(record(impulse_at).kind == 12, "impulse at a point has its own discriminant");
  auto impulse = command_buffer(256);
  abi::write_physics2_dabi_apply_linear_impulse_command(impulse, 1, 1, 1);
  check(record(impulse).kind == 11, "impulse has its own discriminant");

  auto torque = command_buffer(256);
  check(abi::write_physics2_dabi_apply_torque_command(torque, 1, 12), "torque");
  written = record(torque);
  check(written.kind == 13 && written.view.get_float64(written.payload, true) == 12 &&
            written.view.get_float64(written.payload + 8, true) == 0 &&
            written.view.get_float64(written.payload + 16, true) == 0 &&
            written.view.get_float64(written.payload + 24, true) == 0,
        "torque leaves the three unused slots zero");

  auto refused = command_buffer(256);
  check(!abi::write_physics2_dabi_apply_force_command(refused, 0, 1, 1) &&
            !abi::write_physics2_dabi_apply_torque_command(refused, 1.5, 1) && header_is(refused, 16, 0),
        "a body action needs a valid body id");
}

void empty_records() {
  auto destroy_body = command_buffer(256);
  check(abi::write_physics2_dabi_destroy_body_command(destroy_body, 3), "destroy body");
  check(record(destroy_body).byte_length == 16 && record(destroy_body).object_id == 3 && record(destroy_body).kind == 4,
        "destroy body is header-only");
  auto destroy_collider = command_buffer(256);
  abi::write_physics2_dabi_destroy_collider_command(destroy_collider, 4);
  check(record(destroy_collider).kind == 6 && record(destroy_collider).byte_length == 16, "destroy collider");
  auto destroy_joint = command_buffer(256);
  abi::write_physics2_dabi_destroy_joint_command(destroy_joint, 5);
  check(record(destroy_joint).kind == 8 && record(destroy_joint).object_id == 5, "destroy joint");
  auto wake = command_buffer(256);
  check(!abi::write_physics2_dabi_wake_body_command(wake, 0) && abi::write_physics2_dabi_wake_body_command(wake, 6),
        "wake rejects a zero id");
  check(record(wake).kind == 14 && header_is(wake, 32, 1), "wake writes one header-only record");
}

void set_body() {
  auto buffer = command_buffer(1024);
  check(abi::write_physics2_dabi_set_body_command(buffer, 9, view(body())), "set body");
  auto written = record(buffer);
  check(written.kind == 3 && written.byte_length == 160 && written.object_id == 9 && written.related_id == 0,
        "a fixed-length record naming the body");
  check(written.view.get_uint32(written.payload, true) == 32 && written.view.get_uint32(written.payload + 4, true) == 0,
        "dynamic with sleep enabled, reserved word zero");
  const double expected[] = {3, -4, 0.5, 1, 2, 3, 4.25, 5.25, 6.25, 7.5, 8.5, 9.75, 10.75, 0.125, 0.375, 1.5, 0.0625};
  bool in_order = true;
  for (int i = 0; i < 17; ++i) {
    in_order = in_order && written.view.get_float64(written.payload + 8 + 8 * i, true) == expected[i];
  }
  check(in_order, "the seventeen authored scalars in wire order, without the derived inverses");

  auto flagged = body("static");
  flagged->fixed_rotation = true;
  flagged->bullet = true;
  flagged->sleeping = true;
  flagged->sleep_enabled = false;
  auto flags_buffer = command_buffer(1024);
  abi::write_physics2_dabi_set_body_command(flags_buffer, 1, view(flagged));
  check(record(flags_buffer).view.get_uint32(record(flags_buffer).payload, true) == (2 | 4 | 8 | 16),
        "static, fixed rotation, bullet and sleeping pack into the flag word");
  auto kinematic = command_buffer(1024);
  abi::write_physics2_dabi_set_body_command(kinematic, 1, view(body("kinematic")));
  check(record(kinematic).view.get_uint32(record(kinematic).payload, true) == (1 | 32), "kinematic is type 1");

  auto ids = command_buffer(1024);
  check(!abi::write_physics2_dabi_set_body_command(ids, 0, view(body())) &&
            !abi::write_physics2_dabi_set_body_command(ids, 1.5, view(body())) &&
            !abi::write_physics2_dabi_set_body_command(ids, -1, view(body())) &&
            !abi::write_physics2_dabi_set_body_command(ids, 4294967296.0, view(body())) &&
            !abi::write_physics2_dabi_set_body_command(ids, std::nan(""), view(body())) &&
            ids->command_count == 0,
        "an id outside the non-zero uint32 range is refused");
  check(abi::write_physics2_dabi_set_body_command(ids, 4294967295.0, view(body())) &&
            record(ids).object_id == 4294967295.0,
        "the largest uint32 id is accepted");

  auto unknown = command_buffer(1024);
  check(!abi::write_physics2_dabi_set_body_command(unknown, 1, view(body("ghost"))) && header_is(unknown, 16, 0),
        "a body type with no wire discriminant is refused");

  auto tight = command_buffer(16 + 8);
  check(!abi::write_physics2_dabi_set_body_command(tight, 1, view(body())) && header_is(tight, 16, 0),
        "a record that does not fit leaves the stream untouched");
  auto exact = command_buffer(16 + 160);
  check(abi::write_physics2_dabi_set_body_command(exact, 1, view(body())) &&
            abi::get_physics2_dabi_command_buffer_remaining_byte_length(view(exact)) == 0,
        "a record that exactly fits is written");
}

void set_collider() {
  auto buffer = command_buffer(1024);
  auto value = collider(circle(1, 2, 3), true, 5, 6, -7);
  check(abi::write_physics2_dabi_set_collider_command(buffer, 4, 8, view(value)), "set collider");
  auto written = record(buffer);
  check(written.kind == 5 && written.object_id == 4 && written.related_id == 8, "the body is the related id");
  check(written.view.get_uint32(written.payload, true) == 1 && written.view.get_uint32(written.payload + 4, true) == 5 &&
            written.view.get_uint32(written.payload + 8, true) == 6 &&
            written.view.get_int32(written.payload + 12, true) == -7,
        "sensor, category, mask and a signed group");
  check(written.view.get_float64(written.payload + 16, true) == 1 &&
            written.view.get_float64(written.payload + 24, true) == 0.3 &&
            written.view.get_float64(written.payload + 32, true) == 0.1,
        "density, friction, restitution");
  check(written.view.get_uint32(written.payload + 40, true) == 1 && written.view.get_uint32(written.payload + 44, true) == 3 &&
            written.view.get_uint32(written.payload + 48, true) == 0 &&
            written.view.get_float64(written.payload + 56, true) == 1,
        "the shape follows inline");

  auto local = collider(circle(1, 1, 1));
  local->world = circle(50, 50, 9);
  auto local_buffer = command_buffer(1024);
  abi::write_physics2_dabi_set_collider_command(local_buffer, 1, 1, view(local));
  check(record(local_buffer).view.get_float64(record(local_buffer).payload + 56, true) == 1 &&
            record(local_buffer).view.get_float64(record(local_buffer).payload + 72, true) == 1,
        "the wire carries the local shape, not the world one");

  auto refused = command_buffer(1024);
  check(!abi::write_physics2_dabi_set_collider_command(refused, 1, 1, view(collider(circle(0, 0, 1), false, -1))),
        "a negative category is refused");
  check(!abi::write_physics2_dabi_set_collider_command(refused, 1, 1, view(collider(circle(0, 0, 1), false, 1, 4294967296.0))),
        "a mask past uint32 is refused");
  check(!abi::write_physics2_dabi_set_collider_command(refused, 1, 1, view(collider(circle(0, 0, 1), false, 1, 1, 2147483648.0))) &&
            !abi::write_physics2_dabi_set_collider_command(refused, 1, 1, view(collider(circle(0, 0, 1), false, 1, 1, 0.5))),
        "a group outside int32 is refused");
  check(!abi::write_physics2_dabi_set_collider_command(refused, 0, 1, view(collider(circle(0, 0, 1)))) &&
            !abi::write_physics2_dabi_set_collider_command(refused, 1, 0, view(collider(circle(0, 0, 1)))) &&
            header_is(refused, 16, 0),
        "both ids must be valid");
  auto edges = command_buffer(1024);
  check(abi::write_physics2_dabi_set_collider_command(
            edges, 1, 1, view(collider(circle(0, 0, 1), false, 4294967295.0, 0, -2147483648.0))) &&
            record(edges).view.get_uint32(record(edges).payload + 4, true) == 4294967295.0 &&
            record(edges).view.get_int32(record(edges).payload + 12, true) == -2147483648.0,
        "the full uint32 category and the int32 minimum group are accepted");
}

void gravity_and_solver() {
  auto gravity = command_buffer(256);
  check(abi::write_physics2_dabi_set_gravity_command(gravity, 1, -9.81), "gravity");
  auto written = record(gravity);
  check(written.kind == 1 && written.byte_length == 32 && written.object_id == 0 && written.related_id == 0 &&
            written.view.get_float64(written.payload, true) == 1 &&
            written.view.get_float64(written.payload + 8, true) == -9.81,
        "two components, no object");

  auto solver = command_buffer(256);
  auto config = solver_config();
  check(abi::write_physics2_dabi_set_solver_config_command(solver, view(config)), "solver config");
  written = record(solver);
  check(written.kind == 2 && written.byte_length == 96, "a fixed-length solver record");
  check(written.view.get_uint32(written.payload, true) == (1 | 2 | 4) &&
            written.view.get_uint32(written.payload + 4, true) == 0,
        "three booleans in the flag word, the substep word zero");
  check(written.view.get_uint32(written.payload + 8, true) == 8 && written.view.get_uint32(written.payload + 12, true) == 64 &&
            written.view.get_uint32(written.payload + 16, true) == 10 &&
            written.view.get_uint32(written.payload + 20, true) == 3 &&
            written.view.get_uint32(written.payload + 24, true) == 0 &&
            written.view.get_uint32(written.payload + 28, true) == 0,
        "ccd substeps, iterations, and two reserved words");
  const double values[] = {0.01, (2 * M_PI) / 180, 0.5, 0.005, 0.2, 1};
  bool in_order = true;
  for (int i = 0; i < 6; ++i) in_order = in_order && written.view.get_float64(written.payload + 32 + 8 * i, true) == values[i];
  check(in_order, "the six solver scalars in wire order");

  auto partial = solver_config();
  partial->allow_sleeping = false;
  partial->warm_starting = false;
  auto partial_buffer = command_buffer(256);
  abi::write_physics2_dabi_set_solver_config_command(partial_buffer, view(partial));
  check(record(partial_buffer).view.get_uint32(record(partial_buffer).payload, true) == 2, "continuous collision alone");

  for (auto mutate : std::vector<void (*)(types::Physics2DSolverConfig&)>{
           [](types::Physics2DSolverConfig& c) { c.velocity_iterations = -1; },
           [](types::Physics2DSolverConfig& c) { c.position_iterations = 1.5; },
           [](types::Physics2DSolverConfig& c) { c.max_ccd_substeps = 4294967296.0; },
           [](types::Physics2DSolverConfig& c) { c.max_ccd_rotation_substeps = std::nan(""); }}) {
    auto bad = solver_config();
    mutate(*bad);
    auto refused = command_buffer(256);
    check(!abi::write_physics2_dabi_set_solver_config_command(refused, view(bad)) && header_is(refused, 16, 0),
          "an iteration or substep count that is not a uint32 is refused");
  }
}

void stream_integrity() {
  // Records append in call order, and the header tracks them.
  auto stream = command_buffer(1024);
  abi::write_physics2_dabi_set_gravity_command(stream, 0, -10);
  abi::write_physics2_dabi_set_body_command(stream, 1, view(body()));
  abi::write_physics2_dabi_wake_body_command(stream, 1);
  check(header_is(stream, 16 + 32 + 160 + 16, 3), "the header counts three records and their bytes");
  check(record(stream, 16).kind == 1 && record(stream, 48).kind == 3 && record(stream, 208).kind == 14,
        "records are laid end to end in call order");

  // The header is re-read, not trusted: a buffer whose header disagrees with its fields is refused.
  auto corrupt_magic = command_buffer(256);
  data_view(corrupt_magic).set_uint32(0, 0x41443350, true);  // P3DA
  check(!abi::write_physics2_dabi_wake_body_command(corrupt_magic, 1), "a 3D magic is refused");
  auto corrupt_version = command_buffer(256);
  data_view(corrupt_version).set_uint32(4, 2, true);
  check(!abi::write_physics2_dabi_wake_body_command(corrupt_version, 1), "another version is refused");
  auto stale_count = command_buffer(256);
  stale_count->command_count = 1;
  check(!abi::write_physics2_dabi_wake_body_command(stale_count, 1), "a count the header does not record is refused");
  auto stale_length = command_buffer(256);
  stale_length->byte_length = 24;
  check(!abi::write_physics2_dabi_wake_body_command(stale_length, 1), "a length the header does not record is refused");
  auto under_header = command_buffer(256);
  under_header->byte_length = 8;
  data_view(under_header).set_uint32(8, 8, true);
  check(!abi::write_physics2_dabi_wake_body_command(under_header, 1), "a length inside the header is refused");
  auto negative = command_buffer(256);
  negative->command_count = -1;
  check(!abi::write_physics2_dabi_wake_body_command(negative, 1), "a negative count is refused");
}

}  // namespace

int main() {
  layout();
  max_contact_points_sweep();
  buffers();
  collider_lengths();
  shape_encoding();
  body_actions();
  empty_records();
  set_body();
  set_collider();
  gravity_and_solver();
  stream_integrity();
  return failures == 0 ? 0 : 1;
}
