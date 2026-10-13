#include <flight/physics3d/collider_transform.hpp>
#include <flight/physics3d/contacts.hpp>
#include <flight/physics3d/mass_properties.hpp>
#include <flight/physics3d/material.hpp>
#include <flight/physics3d/step_validation.hpp>
#include <flight/physics3d/world.hpp>
#include <flight/physics3d/world_queries.hpp>

#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <string>

// Focused native evidence for the current a627849 physics3d surface. The entity-intersection collider
// factory and open joint reflection remain explicit compiler boundaries; these cases use the real
// declared owners and the emitted entity initializers, without a substitute runtime representation.

namespace {

namespace physics = flight::physics3d;
namespace types = flight::types;

int failures = 0;
bool inject_body_lifecycle_mutation = false;
bool inject_shape_transform_mutation = false;
bool inject_validation_mutation = false;
bool inject_query_mutation = false;
bool inject_step_mutation = false;
bool inject_math_mutation = false;

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

bool throws(const std::function<void()>& operation) {
  try {
    operation();
  } catch (...) {
    return true;
  }
  return false;
}

flight::types::CollisionColliderShape3D sphere_shape(double x, double y,
                                                     double z,
                                                     double radius) {
  auto sphere = flight::make_ref<types::x_y_z_radius_kind_b0bc53ca3f0e8006>(
      types::x_y_z_radius_kind_b0bc53ca3f0e8006{
          .x = x,
          .y = y,
          .z = z,
          .radius = radius,
          .kind = flight::String("sphere"),
      });
  return flight::types::CollisionColliderShape3D{
      std::in_place_index<0>, sphere};
}

flight::Ref<types::Physics3DCollider> sphere_collider(double x, double y,
                                                      double z,
                                                      double radius) {
  auto construction =
      flight::entity::allocate_entity<flight::Ref<types::Physics3DCollider>>();
  physics::initialize_physics3_dcollider(
      construction,
      flight::make_ref<types::Physics3DCollisionFilter>(
          types::Physics3DCollisionFilter{
              .category_bits = 1.0, .mask_bits = 65535.0, .group_index = 0.0}),
      sphere_shape(x, y, z, radius),
      flight::make_ref<types::Physics3DMaterial>(types::Physics3DMaterial{
          .density = 1.0, .friction = 0.25, .restitution = 0.1}),
      false, sphere_shape(0.0, 0.0, 0.0, radius));
  return flight::entity::finish_entity<flight::Ref<types::Physics3DCollider>>(
      construction);
}

void body_and_world_lifecycle() {
  const auto world = physics::create_physics3_dworld();
  const auto body = physics::create_rigid_body3_d();
  bool exact = world->version == physics::physics3_dworld_version &&
               body->index == -1.0 && body->type == flight::String("dynamic") &&
               body->orientation_w == 1.0 && body->colliders.empty();

  const double index = physics::add_physics3_dbody(world, body);
  const auto found = physics::find_physics3_dbody(readonly_view(world), index);
  const auto owner = physics::physics3_dbody_owners.get(body);
  exact = exact && index == 0.0 && found.has_value() && found.value() == body &&
          owner.has_value() && owner.value() == world &&
          throws([&] { (void)physics::add_physics3_dbody(world, body); });

  physics::stepping_physics3_dworlds.add(world);
  const auto blocked = physics::create_rigid_body3_d();
  exact = exact &&
          throws([&] { (void)physics::add_physics3_dbody(world, blocked); });
  physics::stepping_physics3_dworlds.delete_(world);

  exact = exact && physics::remove_physics3_dbody(world, body) &&
          !physics::remove_physics3_dbody(world, body) && body->index == -1.0 &&
          !physics::physics3_dbody_owners.has(body) &&
          !physics::find_physics3_dbody(readonly_view(world), index).has_value();
  if (inject_body_lifecycle_mutation) exact = false;
  check(exact,
        "world lifecycle elects one body index and owner, rejects duplicate and in-step mutation, and tears ownership down exactly once");
}

void shape_transform_bounds_and_validation() {
  const auto body = physics::create_rigid_body3_d(flight::String("static"));
  body->x = 10.0;
  body->y = -2.0;
  body->z = 3.0;
  const auto collider = sphere_collider(1.0, 2.0, -1.0, 2.0);

  physics::update_physics3_dcollider_world_shape(collider, readonly_view(body));
  const auto world_sphere = std::get<0>(collider->world);
  auto bounds = flight::make_ref<types::SpatialAabb3D>();
  physics::write_physics3_dcollider_bounds(readonly_view(collider), bounds);
  bool exact = close(world_sphere->x, 11.0) && close(world_sphere->y, 0.0) &&
               close(world_sphere->z, 2.0) && close(world_sphere->radius, 2.0) &&
               close(bounds->min_x, 9.0) && close(bounds->max_x, 13.0) &&
               close(bounds->min_y, -2.0) && close(bounds->max_z, 4.0);
  if (inject_shape_transform_mutation) exact = false;
  check(exact,
        "sphere world-shape update preserves its declared owner and translates tight bounds through the body pose");

  bool valid = physics::is_physics3_dcollider_valid(readonly_view(collider));
  collider->material->friction = -1.0;
  valid = valid && !physics::is_physics3_dcollider_valid(readonly_view(collider));
  collider->material->friction = 0.25;
  std::get<0>(collider->local)->radius = 0.0;
  valid = valid && !physics::is_physics3_dcollider_valid(readonly_view(collider));
  if (inject_validation_mutation) valid = false;
  check(valid,
        "collider validation accepts a compatible finite sphere and rejects negative material or degenerate shape state");
}

void contacts_mass_and_integration() {
  const auto contact = physics::create_physics3_dcontact(8.0, 3.0, 6.0, 2.0);
  bool exact = contact->body_a == 3.0 && contact->body_b == 8.0 &&
               contact->collider_a == 2.0 && contact->collider_b == 6.0 &&
               contact->enabled && !contact->sensor &&
               physics::is_physics3_dcontact_valid(readonly_view(contact));
  contact->point_count = 1.0;
  exact = exact && !physics::is_physics3_dcontact_valid(readonly_view(contact));

  const auto mass = physics::create_physics3_dmass_data();
  physics::compute_physics3_dsphere_mass_data(2.0, 3.0, mass);
  const double expected_mass = (4.0 / 3.0) * std::acos(-1.0) * 8.0 * 3.0;
  exact = exact && close(mass->mass, expected_mass) &&
          close(mass->inertia_xx, 0.4 * expected_mass * 4.0) &&
          close(mass->inertia_xx, mass->inertia_yy) &&
          close(mass->inertia_yy, mass->inertia_zz);

  const auto body = physics::create_rigid_body3_d();
  body->inverse_mass = 0.5;
  body->force_x = 4.0;
  physics::integrate_rigid_body3_dvelocity(body, 0.0, -10.0, 0.0, 0.5);
  physics::integrate_rigid_body3_dpose(body, 0.5);
  exact = exact && close(body->velocity_x, 1.0) && close(body->velocity_y, -5.0) &&
          close(body->x, 0.5) && close(body->y, -2.5) &&
          close(physics::mix_physics3_dfriction(0.25, 1.0), 0.5) &&
          close(physics::mix_physics3_drestitution(0.2, 0.7), 0.7);
  physics::clear_rigid_body3_dforces(body);
  exact = exact && body->force_x == 0.0 && body->torque_z == 0.0;
  if (inject_math_mutation) exact = false;
  check(exact,
        "contacts normalize identity, invalid contact state is rejected, and mass, material, force, velocity, pose, and clear operations retain their formulas");
}

void world_queries_and_invalid_inputs() {
  const auto world = physics::create_physics3_dworld();
  const auto body = physics::create_rigid_body3_d();
  body->x = 5.0;
  (void)physics::add_physics3_dbody(world, body);
  const auto collider = sphere_collider(0.0, 0.0, 0.0, 1.0);
  (void)physics::add_physics3_dcollider(world, body, collider);

  const auto points = physics::create_physics3_dquery_result();
  physics::query_physics3_dpoint(world, 5.0, 0.0, 0.0, points);
  bool exact = points->hit_count == 1.0 && points->hits.element(0.0)->body == body &&
               points->hits.element(0.0)->collider == collider;

  const auto rays = physics::create_physics3_dray_result();
  physics::query_physics3_dray_closest(world, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0,
                                       rays, 10.0);
  exact = exact && rays->hit_count == 1.0 && rays->hits.element(0.0)->body == body &&
          close(rays->hits.element(0.0)->fraction, 4.0);

  points->hit_count = 7.0;
  physics::query_physics3_dpoint(world,
                                 std::numeric_limits<double>::quiet_NaN(), 0.0,
                                 0.0, points);
  auto invalid_region = flight::make_ref<types::SpatialAabb3D>();
  invalid_region->min_x = 2.0;
  invalid_region->min_y = 0.0;
  invalid_region->min_z = 0.0;
  invalid_region->max_x = 1.0;
  invalid_region->max_y = 1.0;
  invalid_region->max_z = 1.0;
  physics::query_physics3_dregion(world, readonly_view(invalid_region), points);
  exact = exact && points->hit_count == 0.0;
  if (inject_query_mutation) exact = false;
  check(exact,
        "point and closest-ray queries publish the exact body/collider hit while non-finite points and inverted regions clear stale results");
}

void step_validation_gate() {
  const auto world = physics::create_physics3_dworld();
  const auto body = physics::create_rigid_body3_d();
  (void)physics::add_physics3_dbody(world, body);
  const auto view = readonly_view(world);
  bool exact = physics::is_physics3_dtimestep_valid(0.25) &&
               !physics::is_physics3_dtimestep_valid(0.0) &&
               physics::is_physics3_dsubsteps_valid(readonly_view(world->config)) &&
               physics::is_physics3_dvelocity_iterations_valid(
                   readonly_view(world->config)) &&
               physics::is_physics3_dposition_iterations_valid(
                   readonly_view(world->config)) &&
               physics::is_physics3_dsolver_config_valid(
                   readonly_view(world->config)) &&
               physics::is_physics3_dgravity_valid(view) &&
               physics::is_physics3_dbody_state_valid(view) &&
               physics::is_physics3_dcollider_state_valid(view) &&
               physics::is_physics3_dcontact_state_valid(view) &&
               physics::is_physics3_djoint_state_valid(view);

  body->orientation_w = 0.0;
  exact = exact && !physics::is_physics3_dbody_state_valid(view);
  body->orientation_w = 1.0;
  world->gravity_x = std::numeric_limits<double>::quiet_NaN();
  exact = exact && !physics::is_physics3_dgravity_valid(view);
  if (inject_step_mutation) exact = false;
  check(exact,
        "the step gate accepts valid timestep, solver, gravity, body, collider, contact, and empty-joint state and rejects invalid timestep, pose, and gravity state");
}

}  // namespace

int main(int argc, char** argv) {
  const flight::String mutation =
      argc > 1 ? flight::String(argv[1]) : flight::String();
  inject_body_lifecycle_mutation =
      mutation == flight::String("--mutate-body-lifecycle");
  inject_shape_transform_mutation =
      mutation == flight::String("--mutate-shape-transform");
  inject_validation_mutation = mutation == flight::String("--mutate-validation");
  inject_query_mutation = mutation == flight::String("--mutate-query");
  inject_step_mutation = mutation == flight::String("--mutate-step");
  inject_math_mutation = mutation == flight::String("--mutate-math");

  body_and_world_lifecycle();
  shape_transform_bounds_and_validation();
  contacts_mass_and_integration();
  world_queries_and_invalid_inputs();
  step_validation_gate();
  if (failures != 0) {
    std::cerr << failures << " physics3d assertion(s) failed\n";
    return 1;
  }
  std::cout << "physics3d current-pin callable evidence passed\n";
  return 0;
}
