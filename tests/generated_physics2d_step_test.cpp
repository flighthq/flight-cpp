// The generated @flighthq/physics2d step at a627849, exercised natively: integration, the contact pipeline,
// continuous collision, contact hooks and their failure paths, sleeping, broadphase ordering and determinism.
// Ported from step.test.ts and stress.test.ts with the same inputs, assertions and tolerances.
//
// Colliders come from tests/physics2d_test_support.hpp's collider(), the documented fixture for the refused
// createPhysics2DCollider: its world shape is the plain arm, without the entity marker TypeScript gives it.
// Joints for custom solvers are plain Physics2DJoint rows, as step.test.ts's entityJoint builds them.
//
// Not ported, and why:
// - step.test.ts "stepPhysics2D with breakable joints" (six cases), and the "stepPhysics2D with joints" cases
//   built on jointedWorld or a Distance joint (the reversed-pair trace, the swapped-pair order, the repeat trace,
//   and the jointed-contact suppression), plus "holds a jointed body still once its island sleeps": all register
//   physics2DDistanceJointSolver or physics2DWeldJointSolver, which joints.ts refuses at a627849.
// - "scales common and kind-owned joint impulses into a changed timestep" writes a `motorImpulse` member that the
//   flattened Physics2DJoint row cannot carry (docs/compiler-requests/assertion-to-undeclared-row-member.md).
// - stress.test.ts "keeps a driven distance-joint chain bounded…" and "produces an exact repeat trace for a mixed
//   pile, joint, sleep, and bullet scene" need createPhysics2DDistanceJoint (joint_factories.hpp does not
//   compile) and the refused distance solver.
//
// "invokes no joint solver callback for a pair whose every end is asleep" leaves the joint's anchors and impulses
// undefined upstream; a native row stores them, so they are written as zero. The recording solver never reads
// them, and the validator accepts both.
//
// Identity: a Ref compares by object. An Array or Map has no identity comparison, so the workspace-retention
// case probes identity by mutating the retained handle and observing the change through the world's member.
#include "physics2d_test_support.hpp"
#include "upstream_test_harness.hpp"

#include <flight/physics2d/joint_registry.hpp>
#include <flight/physics2d/step.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <map>
#include <string>
#include <tuple>
#include <vector>

namespace {

namespace p = flight::physics2d;
namespace t = flight::physics2d_test;
namespace types = flight::types;
using flight::upstream_test::AssertionFailure;
using flight::upstream_test::expect;

constexpr double INF = std::numeric_limits<double>::infinity();
constexpr double NaN = std::numeric_limits<double>::quiet_NaN();

auto stone() { return t::material(1, 0.3, 0); }

// createPhysics2DWorld() with no arguments: gravity (0, -9.81). The shared support header's world() defaults to
// zero gravity, so the no-argument upstream form goes straight to the generated constructor.
t::World default_world() { return p::create_physics2_dworld(); }

t::Body add(const t::World& world, const t::Body& body) { return p::add_physics2_dbody(world, body); }
void push(const t::Body& body, const t::Collider& collider) { body->colliders.push(collider); }

void expect_true(bool condition, const char* what) {
  if (!condition) throw AssertionFailure(what);
}

double count_of(const auto& array) { return static_cast<double>(array.size()); }

// toThrow(pattern): something is thrown whose message contains `fragment`, and it is a flight::Error, since
// every runtime and source throw here is one.
template <typename Function>
void expect_throw_containing(Function&& function, const std::string& fragment) {
  try {
    function();
  } catch (const AssertionFailure&) {
    throw;
  } catch (const flight::Error& error) {
    if (std::string(error.what()).find(fragment) == std::string::npos) {
      throw AssertionFailure("thrown message '" + std::string(error.what()) + "' lacks '" + fragment + "'");
    }
    return;
  } catch (const std::exception& error) {
    throw AssertionFailure("expected a flight::Error carrying '" + fragment + "', got a std::exception: " + error.what());
  }
  throw AssertionFailure("expected a throw containing '" + fragment + "'");
}

t::Body ground(const t::World& world) {
  auto body = t::body("static", 0, 0);
  push(body, t::collider(t::aabb(-50, -1, 50, 0), stone()));
  return add(world, body);
}

t::Body box(const t::World& world, double x, double y, double half = 0.5) {
  auto body = t::body("dynamic", x, y);
  push(body, t::collider(t::aabb(-half, -half, half, half), stone()));
  return add(world, body);
}

using Trace = std::vector<std::tuple<double, double, double, double, double, double, double>>;

// step.test.ts's traceWorld: every body's full state, ordered by index. Exact double comparison is the
// string comparison's meaning (String(-0) and String(0) agree, as -0.0 == 0.0 does).
Trace trace_world(const t::World& world) {
  Trace trace;
  for (const auto& b : world->bodies) {
    trace.emplace_back(b->index, b->x, b->y, b->angle, b->velocity_x, b->velocity_y, b->angular_velocity);
  }
  std::sort(trace.begin(), trace.end(), [](const auto& left, const auto& right) { return std::get<0>(left) < std::get<0>(right); });
  return trace;
}

flight::Ref<types::SpatialIndexBackend2D> grid_copy() {
  flight::Ref<types::SpatialIndexBackend2D> inner = flight::spatial::create_uniform_grid_spatial_backend2_d(1.0);
  return flight::make_ref<types::SpatialIndexBackend2D>(*inner);
}

// The real grid with its pairs reversed, and with each pair's ids swapped: the candidate set is unchanged and
// only its presentation differs.
flight::Ref<types::SpatialIndexBackend2D> reversed_pair_backend() {
  auto backend = grid_copy();
  auto inner = backend->query_spatial_pairs;
  backend->query_spatial_pairs = [inner](flight::Array<flight::Ref<types::SpatialPair>> out) {
    inner(out);
    out.reverse();
  };
  return backend;
}

flight::Ref<types::SpatialIndexBackend2D> swapped_pair_backend() {
  auto backend = grid_copy();
  auto inner = backend->query_spatial_pairs;
  backend->query_spatial_pairs = [inner](flight::Array<flight::Ref<types::SpatialPair>> out) {
    inner(out);
    for (const auto& pair : out) {
      const double a = pair->a;
      pair->a = pair->b;
      pair->b = a;
    }
  };
  return backend;
}

void run_steps(const t::World& world, int count) {
  for (int i = 0; i < count; i++) p::step_physics2_d(world, 1.0 / 60);
}

flight::Ref<types::Physics2DJoint> entity_joint(const char* kind, double body_a, double body_b, bool collide_connected = false) {
  auto row = flight::entity::finish_entity<flight::Ref<types::Physics2DJoint>>(
      flight::entity::allocate_entity<flight::Ref<types::Physics2DJoint>>());
  row->kind = flight::String(kind);
  row->body_a = body_a;
  row->body_b = body_b;
  row->local_anchor_ax = 0;
  row->local_anchor_ay = 0;
  row->local_anchor_bx = 0;
  row->local_anchor_by = 0;
  row->collide_connected = collide_connected;
  row->break_force = INF;
  row->break_torque = INF;
  row->impulse0 = 0;
  row->impulse1 = 0;
  row->impulse2 = 0;
  row->r_ax = 0;
  row->r_ay = 0;
  row->r_bx = 0;
  row->r_by = 0;
  return row;
}

using Hook = std::function<void(flight::Ref<types::Physics2DWorld>, flight::Ref<types::Physics2DContact>)>;

double contact_count(const t::World& world) { return count_of(world->contacts); }

// Resets the module-level step seams however a case exits, as the upstream afterEach does.
struct SeamReset {
  ~SeamReset() {
    p::set_physics2_dcontact_intake_guard(std::nullopt);
    p::set_physics2_djoint_resolution_guard(std::nullopt);
    p::set_physics2_dstep_guard(std::nullopt);
  }
};

void declined_body(flight::upstream_test::Runner& runner) {
  runner.describe("a body the step declines leaves the broadphase", [&] {
    auto dynamic_box = [](const t::World& world, double x) {
      auto body = t::body("dynamic", x, 0);
      push(body, t::collider(t::aabb(-0.5, -0.5, 0.5, 0.5), stone()));
      return add(world, body);
    };
    auto local_box = [](const t::Body& body) {
      return std::get<flight::Ref<types::min_x_min_y_max_x_max_y_kind_329d6080b4f31db4>>(body->colliders.element(0)->local);
    };
    auto widen_past_limit = [=](const t::Body& body) {
      auto local = local_box(body);
      local->min_x = -1e8;
      local->min_y = -1e8;
      local->max_x = 1e8;
      local->max_y = 1e8;
    };
    auto region_ids = [](const t::World& world) {
      flight::Array<double> out;
      world->index->query_spatial_region(
          t::view(flight::make_ref<types::SpatialAabb2D>(types::SpatialAabb2D{.min_x = -1, .min_y = -1, .max_x = 1, .max_y = 1})), out);
      return out;
    };
    auto contains = [](const flight::Array<double>& ids, double id) {
      for (double value : ids) {
        if (value == id) return true;
      }
      return false;
    };

    runner.it("drops the contact of a body that diverges past the simulated extent", [=] {
      auto world = t::world(0, 0);
      auto diverging = dynamic_box(world, 0);
      dynamic_box(world, 0.5);
      p::step_physics2_d(world, 1.0 / 60);
      expect(contact_count(world)).to_be(1.0);

      widen_past_limit(diverging);
      p::step_physics2_d(world, 1.0 / 60);
      expect(contact_count(world)).to_be(0.0);
    });

    runner.it("leaves the rest of the world simulating after one body diverges", [=] {
      auto world = t::world(0, 0);
      auto diverging = dynamic_box(world, 0);
      auto near = dynamic_box(world, 0.5);
      auto other = dynamic_box(world, 20);
      auto also_other = dynamic_box(world, 20.5);
      p::step_physics2_d(world, 1.0 / 60);
      widen_past_limit(diverging);
      p::step_physics2_d(world, 1.0 / 60);

      expect(contact_count(world)).to_be(1.0);
      expect(world->contacts.element(0)->body_a).to_be(other->index);
      expect(world->contacts.element(0)->body_b).to_be(also_other->index);
      expect(near->x).to_be_greater_than(0.5);
      expect(std::isfinite(near->x)).to_be(true);
    });

    runner.it("withdraws a body whose colliders stop producing bounds from the index", [=] {
      auto world = t::world(0, 0);
      auto emptied = dynamic_box(world, 0);
      dynamic_box(world, 0.5);
      p::step_physics2_d(world, 1.0 / 60);

      expect(contains(region_ids(world), emptied->index)).to_be(true);

      (void)emptied->colliders.splice(0);
      p::step_physics2_d(world, 1.0 / 60);

      expect(contains(region_ids(world), emptied->index)).to_be(false);
      expect(contact_count(world)).to_be(0.0);
    });

    runner.it("withdraws a diverged body from the index, not merely from the update", [=] {
      auto world = t::world(0, 0);
      auto diverging = dynamic_box(world, 0);
      dynamic_box(world, 0.5);
      p::step_physics2_d(world, 1.0 / 60);

      widen_past_limit(diverging);
      p::step_physics2_d(world, 1.0 / 60);

      expect(contains(region_ids(world), diverging->index)).to_be(false);
    });

    runner.it("re-enters the broadphase when the body comes back inside the limit", [=] {
      auto world = t::world(0, 0);
      auto diverging = dynamic_box(world, 0);
      dynamic_box(world, 0.5);
      p::step_physics2_d(world, 1.0 / 60);
      widen_past_limit(diverging);
      p::step_physics2_d(world, 1.0 / 60);
      expect(contact_count(world)).to_be(0.0);

      auto local = local_box(diverging);
      local->min_x = -0.5;
      local->min_y = -0.5;
      local->max_x = 0.5;
      local->max_y = 0.5;
      p::step_physics2_d(world, 1.0 / 60);
      expect(contact_count(world)).to_be(1.0);
    });
  });
}

void bullets(flight::upstream_test::Runner& runner) {
  runner.describe("bullet continuous collision detection", [&] {
    auto wall = [](const t::World& world, double min_x, double max_x, double restitution = 0, bool sensor = false) {
      auto body = t::body("static", 0, 0);
      push(body, t::collider(t::aabb(min_x, -10, max_x, 10), t::material(1, 0, restitution), sensor));
      return add(world, body);
    };
    auto projectile = [](const t::World& world, double speed, double restitution = 0, bool bullet = true) {
      auto body = t::body("dynamic", 0, 0);
      body->bullet = bullet;
      body->velocity_x = speed;
      push(body, t::collider(t::circle(0, 0, 0.5), t::material(1, 0, restitution)));
      return add(world, body);
    };

    runner.it("stops a bullet at a thin wall that the discrete path tunnels through", [=] {
      auto continuous = t::world(0, 0);
      wall(continuous, 5, 5.1);
      auto bullet = projectile(continuous, 100);
      p::step_physics2_d(continuous, 0.1);
      expect(bullet->x).to_be_close_to(4.5);
      expect(bullet->velocity_x).to_be_close_to(0);

      auto discrete = t::world(0, 0);
      wall(discrete, 5, 5.1);
      auto ordinary = projectile(discrete, 100, 0, false);
      p::step_physics2_d(discrete, 0.1);
      expect(ordinary->x).to_be_close_to(10);
    });

    runner.it("applies restitution at impact and advances through the remaining time", [=] {
      auto world = t::world(0, 0);
      wall(world, 5, 5.1, 1);
      auto bullet = projectile(world, 100, 1);

      p::step_physics2_d(world, 0.1);

      expect(bullet->velocity_x).to_be_close_to(-100);
      expect(bullet->x).to_be_close_to(-1);
    });

    runner.it("resolves multiple impacts chronologically within the configured bound", [=] {
      auto world = t::world(0, 0);
      wall(world, -5.1, -5, 1);
      wall(world, 5, 5.1, 1);
      auto bullet = projectile(world, 100, 1);

      p::step_physics2_d(world, 0.2);

      expect(bullet->x).to_be_greater_than_or_equal(-4.5);
      expect(bullet->x).to_be_less_than_or_equal(4.5);
      expect(std::abs(bullet->velocity_x)).to_be_close_to(100);
    });

    runner.it("uses relative motion, transfers impulse, and wakes a sleeping dynamic target", [=] {
      auto world = t::world(0, 0);
      auto bullet = projectile(world, 100);
      auto target = t::body("dynamic", 6, 0);
      target->sleeping = true;
      target->sleep_timer = 5;
      push(target, t::collider(t::circle(0, 0, 0.5), t::material(1, 0, 0)));
      add(world, target);

      p::step_physics2_d(world, 0.1);

      expect(target->sleeping).to_be(false);
      expect(target->velocity_x).to_be_greater_than(0.0);
      expect(bullet->x).to_be_less_than(target->x);
      expect(target->x - bullet->x).to_be_close_to(1);
    });

    runner.it("honours the world switch and ignores sensor-only crossings", [=] {
      auto disabled = t::world(0, 0);
      disabled->config->continuous_collision = false;
      wall(disabled, 5, 5.1);
      auto unchecked = projectile(disabled, 100);
      p::step_physics2_d(disabled, 0.1);
      expect(unchecked->x).to_be_close_to(10);

      auto sensor_world = t::world(0, 0);
      wall(sensor_world, 5, 5.1, 0, true);
      auto sensor_bullet = projectile(sensor_world, 100);
      p::step_physics2_d(sensor_world, 0.1);
      expect(sensor_bullet->x).to_be_close_to(10);
    });

    runner.it("publishes swept impacts through contact lifecycle and solve hooks", [=] {
      auto world = t::world(0, 0);
      wall(world, 5, 5.1);
      auto bullet = projectile(world, 100);
      std::vector<std::string> order;
      world->contact_hooks->pre_solve = Hook([&order](auto, flight::Ref<types::Physics2DContact> contact) {
        order.push_back("pre");
        expect(contact->points.element(0)->x).to_be_close_to(5);
      });
      world->contact_hooks->post_solve = Hook([&order](auto, flight::Ref<types::Physics2DContact> contact) {
        order.push_back("post");
        expect(contact->points.element(0)->normal_impulse).to_be_greater_than(0.0);
      });

      p::step_physics2_d(world, 0.1);

      expect_true(order == std::vector<std::string>{"pre", "post"}, "pre then post");
      expect(count_of(world->events->began)).to_be(1.0);
      expect_true(world->events->began.element(0) == world->contacts.element(0), "the began contact is the cached one");
      expect(count_of(world->events->ended)).to_be(0.0);
      expect(bullet->x).to_be_close_to(4.5);

      world->contact_hooks->pre_solve = std::nullopt;
      world->contact_hooks->post_solve = std::nullopt;
      p::step_physics2_d(world, 0.1);
      expect(count_of(world->events->ended)).to_be(0.0);
    });

    runner.it("catches a thin bullet rotating through an obstacle between matching endpoint poses", [] {
      auto slick = [] { return t::material(1, 0, 0); };
      auto continuous = t::world(0, 0);
      auto obstacle = t::body("static", 0, 1.5);
      push(obstacle, t::collider(t::circle(0, 0, 0.15), slick()));
      add(continuous, obstacle);
      auto rod = t::body("dynamic", 0, 0);
      rod->bullet = true;
      rod->angular_velocity = M_PI / 0.1;
      push(rod, t::collider(t::aabb(-2, -0.025, 2, 0.025), slick()));
      add(continuous, rod);

      p::step_physics2_d(continuous, 0.1);

      expect(count_of(continuous->events->began)).to_be(1.0);
      expect(contact_count(continuous)).to_be(1.0);
      expect(std::abs(rod->angular_velocity)).to_be_less_than(M_PI / 0.1);

      auto discrete = t::world(0, 0);
      auto discrete_obstacle = t::body("static", 0, 1.5);
      push(discrete_obstacle, t::collider(t::circle(0, 0, 0.15), slick()));
      add(discrete, discrete_obstacle);
      auto unchecked = t::body("dynamic", 0, 0);
      unchecked->angular_velocity = M_PI / 0.1;
      push(unchecked, t::collider(t::aabb(-2, -0.025, 2, 0.025), slick()));
      add(discrete, unchecked);

      p::step_physics2_d(discrete, 0.1);

      expect(contact_count(discrete)).to_be(0.0);
      expect(unchecked->angle).to_be_close_to(M_PI);
    });

    runner.it("lets pre-solve disable a swept impact without consuming the CCD budget at zero time", [=] {
      auto world = t::world(0, 0);
      wall(world, 5, 5.1);
      auto bullet = projectile(world, 100);
      int calls = 0;
      world->contact_hooks->pre_solve = Hook([&calls](auto, flight::Ref<types::Physics2DContact> contact) {
        calls++;
        contact->enabled = false;
      });

      p::step_physics2_d(world, 0.1);

      expect(calls).to_be(1);
      expect(bullet->x).to_be_close_to(10);
      expect(contact_count(world)).to_be(1.0);
      expect(world->contacts.element(0)->enabled).to_be(false);
    });
  });
}

void centre_of_mass(flight::upstream_test::Runner& runner) {
  runner.describe("centre of mass integration", [&] {
    auto off_centre_body = [](const t::World& world, double offset_x, double offset_y) {
      auto body = t::body("dynamic", 0, 0);
      push(body, t::collider(t::circle(offset_x, offset_y, 0.25), stone()));
      return add(world, body);
    };
    auto com_x = [](const t::Body& body) {
      return body->x + body->center_x * std::cos(body->angle) - body->center_y * std::sin(body->angle);
    };
    auto com_y = [](const t::Body& body) {
      return body->y + body->center_x * std::sin(body->angle) + body->center_y * std::cos(body->angle);
    };

    runner.it("leaves the centre of mass exactly where it was when a free body only spins", [=] {
      auto world = t::world(0, 0);
      auto body = off_centre_body(world, 1, 0);
      expect(body->center_x).to_be(1.0);
      expect(body->center_y).to_be(0.0);
      body->angular_velocity = 1;

      for (int step = 0; step < 50; step++) p::step_physics2_d(world, 0.1);

      expect(com_x(body)).to_be_close_to(1, 12);
      expect(com_y(body)).to_be_close_to(0, 12);
      expect(body->angle).to_be_close_to(5, 12);
      expect(body->x).to_be_close_to(1 - std::cos(5.0), 12);
      expect(body->y).to_be_close_to(-std::sin(5.0), 12);
    });

    runner.it("drops an off-centre spinning body along the same path as a centred one", [=] {
      auto world = t::world(0, -10);
      auto spinning = off_centre_body(world, 0.75, -0.5);
      spinning->angular_velocity = 7;
      auto centred = off_centre_body(world, 0, 0);

      const double start_x = com_x(spinning);
      const double start_y = com_y(spinning);
      for (int step = 0; step < 60; step++) p::step_physics2_d(world, 1.0 / 60);

      expect(com_x(spinning) - start_x).to_be_close_to(centred->x, 12);
      expect(com_y(spinning) - start_y).to_be_close_to(centred->y, 12);
    });

    runner.it("translates an off-centre body by exactly its velocity when it is not rotating", [=] {
      auto world = t::world(0, 0);
      auto body = off_centre_body(world, 2, -3);
      body->velocity_x = 5;
      body->velocity_y = -2;

      p::step_physics2_d(world, 0.25);

      expect(body->x).to_be_close_to(1.25, 12);
      expect(body->y).to_be_close_to(-0.5, 12);
      expect(body->angle).to_be(0.0);
    });

    runner.it("translates a fixed-rotation off-centre body without swinging its origin", [=] {
      auto world = t::world(0, 0);
      auto body = off_centre_body(world, 1.5, 0.5);
      body->fixed_rotation = true;
      body->angular_velocity = 3;
      body->velocity_x = 4;

      p::step_physics2_d(world, 0.5);

      expect(body->angle).to_be(0.0);
      expect(body->x).to_be_close_to(2, 12);
      expect(body->y).to_be_close_to(0, 12);
    });

    runner.it("never moves the centre of mass sideways when every force on it points straight up", [=] {
      auto slick = [] { return t::material(1, 0, 0); };
      auto world = t::world(0, 0);
      auto floor = t::body("static", 0, 0);
      push(floor, t::collider(t::aabb(-50, -1, 50, 0), slick()));
      add(world, floor);

      auto body = t::body("dynamic", 0, 0.5);
      body->angle = 0.3;
      push(body, t::collider(t::polygon({-0.5, -0.5, 0.5, -0.5, 0.5, 0.5, -0.5, 0.5}), slick()));
      push(body, t::collider(t::aabb(3, 3, 4, 4), slick()));
      add(world, body);

      const double start_x = com_x(body);
      for (int step = 0; step < 8; step++) {
        p::step_physics2_d(world, 1.0 / 60);
        expect(com_x(body)).to_be(start_x);
      }
      expect(body->angle).to_be_less_than(0.29);
    });

    runner.it("conserves the centre of mass of an off-centre body resting on the ground", [=] {
      auto world = t::world(0, -10);
      ground(world);
      auto body = t::body("dynamic", 0, 1);
      push(body, t::collider(t::aabb(1.5, -0.5, 2.5, 0.5), stone()));
      add(world, body);
      expect(body->center_x).to_be_close_to(2, 12);

      for (int step = 0; step < 400; step++) p::step_physics2_d(world, 1.0 / 60);

      expect(com_y(body)).to_be_greater_than(0.4);
      expect(com_y(body)).to_be_less_than(0.6);
      expect(std::abs(com_x(body) - 2)).to_be_less_than(0.05);
      expect(std::abs(body->angle)).to_be_less_than(0.05);
    });
  });
}

void filtering(flight::upstream_test::Runner& runner) {
  runner.describe("collision filtering", [&] {
    auto filtered_box = [](const t::World& world, const char* type, double category, double mask, double group = 0) {
      auto body = t::body(type, 0, 0);
      push(body, t::collider(t::aabb(-0.5, -0.5, 0.5, 0.5), stone(), false, t::filter(category, mask, group)));
      return add(world, body);
    };

    runner.it("requires each collider mask to include the other collider category", [=] {
      auto world = t::world(0, 0);
      filtered_box(world, "static", 0x0001, 0x0002);
      filtered_box(world, "dynamic", 0x0002, 0x0004);
      p::step_physics2_d(world, 1.0 / 60);
      expect(contact_count(world)).to_be(0.0);
    });

    runner.it("creates a contact when both category and mask tests pass", [=] {
      auto world = t::world(0, 0);
      filtered_box(world, "static", 0x0001, 0x0002);
      filtered_box(world, "dynamic", 0x0002, 0x0001);
      p::step_physics2_d(world, 1.0 / 60);
      expect(contact_count(world)).to_be(1.0);
    });

    runner.it("forces a matching positive group to collide regardless of masks", [=] {
      auto world = t::world(0, 0);
      filtered_box(world, "static", 0x0001, 0, 7);
      filtered_box(world, "dynamic", 0x0002, 0, 7);
      p::step_physics2_d(world, 1.0 / 60);
      expect(contact_count(world)).to_be(1.0);
    });

    runner.it("prevents a matching negative group from colliding regardless of masks", [=] {
      auto world = t::world(0, 0);
      filtered_box(world, "static", 0x0001, 0xffffffff, -7);
      filtered_box(world, "dynamic", 0x0002, 0xffffffff, -7);
      p::step_physics2_d(world, 1.0 / 60);
      expect(contact_count(world)).to_be(0.0);
    });
  });
}

void hooks(flight::upstream_test::Runner& runner) {
  runner.describe("contact solve hooks", [&] {
    runner.it("calls pre-solve before post-solve exactly once per solved contact", [] {
      auto world = t::world(0, 0);
      ground(world);
      auto crate = box(world, 0, 0.4);
      crate->velocity_y = -1;
      std::vector<std::string> order;
      world->contact_hooks->pre_solve = Hook([&order](auto, flight::Ref<types::Physics2DContact> contact) {
        order.push_back("pre");
        contact->friction = 0;
      });
      world->contact_hooks->post_solve = Hook([&order](auto, flight::Ref<types::Physics2DContact> contact) {
        order.push_back("post");
        bool any = false;
        for (double i = 0; i < contact->point_count; ++i) any = any || contact->points.element(i)->normal_impulse > 0;
        expect(any).to_be(true);
      });

      p::step_physics2_d(world, 1.0 / 60);

      expect_true(order == std::vector<std::string>{"pre", "post"}, "pre then post, once each");
    });

    runner.it("lets pre-solve disable resolution for one step and resets enabled on the next", [] {
      auto world = t::world(0, 0);
      ground(world);
      auto crate = box(world, 0, 0.4);
      int post_solve_calls = 0;
      world->contact_hooks->pre_solve = Hook([](auto, flight::Ref<types::Physics2DContact> contact) { contact->enabled = false; });
      world->contact_hooks->post_solve = Hook([&post_solve_calls](auto, auto) { post_solve_calls++; });

      p::step_physics2_d(world, 1.0 / 60);

      expect(crate->y).to_be(0.4);
      expect(world->contacts.element(0)->enabled).to_be(false);
      expect(post_solve_calls).to_be(0);
      for (const auto& point : world->contacts.element(0)->points) {
        expect(point->normal_impulse).to_be(0.0);
        expect(point->tangent_impulse).to_be(0.0);
      }

      world->contact_hooks->pre_solve = std::nullopt;
      p::step_physics2_d(world, 1.0 / 60);

      expect(world->contacts.element(0)->enabled).to_be(true);
      expect(crate->y).to_be_greater_than(0.4);
      expect(post_solve_calls).to_be(1);
    });

    runner.it("does not invoke solve hooks for sensor contacts", [] {
      auto world = t::world(0, 0);
      auto trigger = t::body("static", 0, 0);
      push(trigger, t::collider(t::aabb(-1, -1, 1, 1), stone(), true));
      add(world, trigger);
      box(world, 0, 0);
      int calls = 0;
      world->contact_hooks->pre_solve = Hook([&calls](auto, auto) { calls++; });
      world->contact_hooks->post_solve = Hook([&calls](auto, auto) { calls++; });

      p::step_physics2_d(world, 1.0 / 60);

      expect(contact_count(world)).to_be(1.0);
      expect(world->contacts.element(0)->sensor).to_be(true);
      expect(calls).to_be(0);
    });

    runner.it("rejects recursive stepping and releases the guard after the callback fails", [] {
      auto world = t::world(0, 0);
      ground(world);
      box(world, 0, 0.4);
      world->contact_hooks->pre_solve = Hook([](flight::Ref<types::Physics2DWorld> current, auto) { p::step_physics2_d(current, 1.0 / 60); });

      expect_throw_containing([&] { p::step_physics2_d(world, 1.0 / 60); }, "recursively");
      expect(world->previous_timestep).to_be(0.0);

      world->contact_hooks->pre_solve = std::nullopt;
      p::step_physics2_d(world, 1.0 / 60);
      expect(world->previous_timestep).to_be(1.0 / 60);
    });

    runner.it("rejects topology mutation before a hook can partially add an entity", [] {
      auto world = t::world(0, 0);
      ground(world);
      box(world, 0, 0.4);
      auto pending = t::body("dynamic", 10, 10);
      world->contact_hooks->pre_solve = Hook([pending](flight::Ref<types::Physics2DWorld> current, auto) { p::add_physics2_dbody(current, pending); });

      expect_throw_containing([&] { p::step_physics2_d(world, 1.0 / 60); }, "while it is stepping");
      expect(pending->index).to_be(-1.0);
      expect(count_of(world->bodies)).to_be(2.0);

      world->contact_hooks->pre_solve = std::nullopt;
      expect_true(p::add_physics2_dbody(world, pending) == pending, "the pending body is added afterwards");
    });

    runner.it("rejects body actions from hooks instead of applying them at a phase-dependent time", [] {
      auto world = t::world(0, 0);
      ground(world);
      auto crate = box(world, 0, 0.4);
      world->contact_hooks->post_solve = Hook([crate](auto, auto) { p::apply_physics2_dforce(crate, 10, 0); });

      expect_throw_containing([&] { p::step_physics2_d(world, 1.0 / 60); }, "while it is stepping");
      expect(crate->force_x).to_be(0.0);

      world->contact_hooks->post_solve = std::nullopt;
      expect(p::apply_physics2_dforce(crate, 10, 0)).to_be(true);
    });

    runner.it("rejects invalid pre- and post-solve output before it can poison a later step", [] {
      auto world = t::world(0, 0);
      ground(world);
      box(world, 0, 0.4)->velocity_y = -1;
      world->contact_hooks->pre_solve = Hook([](auto, flight::Ref<types::Physics2DContact> contact) { contact->friction = NaN; });

      expect_throw_containing([&] { p::step_physics2_d(world, 1.0 / 60); }, "pre-solve hook produced invalid");
      expect(world->contacts.element(0)->friction).to_be_close_to(0.3);
      world->contact_hooks->pre_solve = std::nullopt;
      world->contact_hooks->post_solve = Hook([](auto, flight::Ref<types::Physics2DContact> contact) { contact->restitution = INF; });

      expect_throw_containing([&] { p::step_physics2_d(world, 1.0 / 60); }, "post-solve hook produced invalid");
      expect(world->contacts.element(0)->restitution).to_be(0.0);
      world->contact_hooks->post_solve = std::nullopt;
      p::step_physics2_d(world, 1.0 / 60);
    });

    runner.it("does not scale a warm cache until pre-solve completes successfully", [] {
      auto world = t::world(0, 0);
      world->config->allow_sleeping = false;
      world->config->velocity_iterations = 0;
      world->config->position_iterations = 0;
      ground(world);
      box(world, 0, 0.49);
      p::step_physics2_d(world, 1.0 / 60);
      auto point = world->contacts.element(0)->points.element(0);
      point->normal_impulse = 3;
      world->contact_hooks->pre_solve = Hook([](auto, auto) { throw flight::Error(flight::String("stop")); });

      expect_throw_containing([&] { p::step_physics2_d(world, 1.0 / 30); }, "stop");
      expect(point->normal_impulse).to_be(3.0);
      expect(world->previous_timestep).to_be(1.0 / 60);

      world->contact_hooks->pre_solve = std::nullopt;
      p::step_physics2_d(world, 1.0 / 30);
      expect(point->normal_impulse).to_be(6.0);
    });

    runner.it("commits integration and cleanup before reporting a post-solve exception", [] {
      auto world = t::world(0, 0);
      ground(world);
      auto crate = box(world, 0, 0.4);
      crate->velocity_x = 1;
      crate->force_x = 2;
      world->contact_hooks->post_solve = Hook([](auto, auto) { throw flight::Error(flight::String("observer failed")); });

      expect_throw_containing([&] { p::step_physics2_d(world, 1.0 / 60); }, "observer failed");
      expect(crate->x).to_be_greater_than(0.0);
      expect(crate->force_x).to_be(0.0);
      expect(world->previous_timestep).to_be(1.0 / 60);

      world->contact_hooks->post_solve = std::nullopt;
      p::step_physics2_d(world, 1.0 / 60);
    });
  });
}

void immovable_sensors(flight::upstream_test::Runner& runner) {
  runner.describe("sensor reporting between immovable bodies", [&] {
    auto immovable = [](const t::World& world, double x, bool sensor) {
      auto body = t::body("static", x, 0);
      push(body, t::collider(t::aabb(-0.5, -0.5, 0.5, 0.5), stone(), sensor));
      return add(world, body);
    };
    auto mixed = [](const t::World& world, double x) {
      auto body = t::body("static", x, 0);
      push(body, t::collider(t::aabb(99.5, -0.5, 100.5, 0.5), stone(), true));
      push(body, t::collider(t::aabb(-0.5, -0.5, 0.5, 0.5), stone(), false));
      return add(world, body);
    };

    runner.it("reports a static sensor overlapping a static collider", [=] {
      auto world = t::world(0, 0);
      immovable(world, 0, true);
      immovable(world, 0, false);
      p::step_physics2_d(world, 1.0 / 60);
      expect(count_of(world->events->began)).to_be(1.0);
      expect(contact_count(world)).to_be(1.0);
      expect(world->contacts.element(0)->sensor).to_be(true);
    });

    runner.it("still skips two immovable bodies when neither senses", [=] {
      auto world = t::world(0, 0);
      immovable(world, 0, false);
      immovable(world, 0, false);
      p::step_physics2_d(world, 1.0 / 60);
      expect(count_of(world->events->began)).to_be(0.0);
      expect(contact_count(world)).to_be(0.0);
    });

    runner.it("reports nothing when the overlapping colliders are both solid and both bodies are immovable", [=] {
      auto world = t::world(0, 0);
      mixed(world, 0);
      immovable(world, 0, false);
      p::step_physics2_d(world, 1.0 / 60);
      expect(count_of(world->events->began)).to_be(0.0);
      expect(contact_count(world)).to_be(0.0);
    });

    runner.it("still reports the sensor collider of a mixed body when that collider overlaps", [=] {
      auto world = t::world(0, 0);
      mixed(world, 0);
      immovable(world, 100, false);
      p::step_physics2_d(world, 1.0 / 60);
      expect(contact_count(world)).to_be(1.0);
      expect(world->contacts.element(0)->sensor).to_be(true);
    });

    runner.it("keeps a solid contact when one body can move", [=] {
      auto world = t::world(0, 0);
      mixed(world, 0);
      box(world, 0, 0);
      p::step_physics2_d(world, 1.0 / 60);
      bool any_solid = false;
      for (const auto& contact : world->contacts) any_solid = any_solid || !contact->sensor;
      expect(any_solid).to_be(true);
    });

    runner.it("resolves nothing for a static sensor pair — reporting is not colliding", [=] {
      auto world = t::world(0, 0);
      auto sensor = immovable(world, 0, true);
      auto scenery = immovable(world, 0, false);
      p::step_physics2_d(world, 1.0 / 60);
      expect(sensor->x).to_be(0.0);
      expect(scenery->x).to_be(0.0);
      expect(sensor->velocity_x).to_be(0.0);
    });

    runner.it("ends a static sensor contact when the overlap stops", [=] {
      auto world = t::world(0, 0);
      auto sensor = immovable(world, 0, true);
      immovable(world, 0, false);
      p::step_physics2_d(world, 1.0 / 60);
      expect(count_of(world->events->began)).to_be(1.0);
      sensor->x = 100;
      p::step_physics2_d(world, 1.0 / 60);
      expect(count_of(world->events->ended)).to_be(1.0);
      expect(contact_count(world)).to_be(0.0);
    });
  });
}

void seams(flight::upstream_test::Runner& runner) {
  runner.describe("setPhysics2DContactIntakeGuard", [&] {
    runner.it("consults the seam once per step that will actually run, and not on one that declines", [] {
      SeamReset reset;
      auto world = t::world(0, -10);
      box(world, 0, 0);
      int calls = 0;
      p::set_physics2_dcontact_intake_guard([&calls](auto) { calls++; });

      p::step_physics2_d(world, 1.0 / 60);
      p::step_physics2_d(world, 1.0 / 60);
      expect(calls).to_be(2);

      world->config->velocity_iterations = -1;
      p::step_physics2_d(world, 1.0 / 60);
      expect(calls).to_be(2);
    });

    runner.it("removes the seam when passed null", [] {
      SeamReset reset;
      auto world = t::world(0, -10);
      box(world, 0, 0);
      int calls = 0;
      p::set_physics2_dcontact_intake_guard([&calls](auto) { calls++; });
      p::set_physics2_dcontact_intake_guard(std::nullopt);

      p::step_physics2_d(world, 1.0 / 60);

      expect(calls).to_be(0);
    });
  });

  runner.describe("setPhysics2DJointResolutionGuard", [&] {
    runner.it("consults the seam once per step that will actually run", [] {
      SeamReset reset;
      auto world = t::world(0, -10);
      box(world, 0, 0);
      int calls = 0;
      p::set_physics2_djoint_resolution_guard([&calls](auto) { calls++; });

      p::step_physics2_d(world, 1.0 / 60);
      expect(calls).to_be(0);

      auto first = world->bodies.element(0)->index;
      p::add_physics2_djoint(world, entity_joint("acme.Unregistered", first, first));
      p::step_physics2_d(world, 1.0 / 60);
      p::step_physics2_d(world, 1.0 / 60);
      expect(calls).to_be(2);
    });

    runner.it("leaves the joint seam alone on a step that declines its preconditions", [] {
      SeamReset reset;
      auto world = t::world(0, -10);
      box(world, 0, 0);
      auto first = world->bodies.element(0)->index;
      p::add_physics2_djoint(world, entity_joint("acme.Unregistered", first, first));
      world->config->velocity_iterations = -1;
      int calls = 0;
      p::set_physics2_djoint_resolution_guard([&calls](auto) { calls++; });

      p::step_physics2_d(world, 1.0 / 60);

      expect(calls).to_be(0);
    });

    runner.it("removes the seam when passed null", [] {
      SeamReset reset;
      auto world = t::world(0, -10);
      box(world, 0, 0);
      auto first = world->bodies.element(0)->index;
      p::add_physics2_djoint(world, entity_joint("acme.Unregistered", first, first));
      int calls = 0;
      p::set_physics2_djoint_resolution_guard([&calls](auto) { calls++; });
      p::set_physics2_djoint_resolution_guard(std::nullopt);

      p::step_physics2_d(world, 1.0 / 60);

      expect(calls).to_be(0);
    });
  });

  runner.describe("setPhysics2DStepGuard", [&] {
    runner.it("consults the seam with the rejected timestep only when the step declines", [] {
      SeamReset reset;
      auto world = t::world(0, -10);
      box(world, 0, 0);
      std::vector<double> seen;
      p::set_physics2_dstep_guard([&seen](auto, double dt) { seen.push_back(dt); });

      p::step_physics2_d(world, 1.0 / 60);
      expect(static_cast<double>(seen.size())).to_be(0.0);

      p::step_physics2_d(world, NaN);
      expect(static_cast<double>(seen.size())).to_be(1.0);
      expect(std::isnan(seen[0])).to_be(true);
    });

    runner.it("removes the seam when passed null", [] {
      SeamReset reset;
      auto world = t::world(0, -10);
      int calls = 0;
      p::set_physics2_dstep_guard([&calls](auto, double) { calls++; });
      p::set_physics2_dstep_guard(std::nullopt);

      p::step_physics2_d(world, -1);

      expect(calls).to_be(0);
    });
  });
}

void step_physics(flight::upstream_test::Runner& runner) {
  runner.describe("stepPhysics2D", [&] {
    runner.it("keeps step scratch isolated when a broadphase callback steps another world", [] {
      auto world = t::world(0, 0);
      world->config->continuous_collision = false;
      auto fixed = ground(world);
      auto dynamic = box(world, 0, 0.4);
      auto nested_world = t::world(0, 0);
      nested_world->config->continuous_collision = false;
      int nested_calls = 0;
      world->index->query_spatial_pairs = [&, fixed_index = fixed->index, dynamic_index = dynamic->index](
                                              flight::Array<flight::Ref<types::SpatialPair>> out) {
        (void)out.splice(0);
        out.push(flight::make_ref<types::SpatialPair>(types::SpatialPair{.a = fixed_index, .b = dynamic_index}));
        if (nested_calls == 0) {
          nested_calls++;
          p::step_physics2_d(nested_world, 1.0 / 60);
        }
      };

      p::step_physics2_d(world, 1.0 / 60);

      expect(nested_calls).to_be(1);
      expect(contact_count(world)).to_be(1.0);
    });

    runner.it("rejects invalid solver state before mutating the world", [] {
      auto world = default_world();
      auto crate = box(world, 0, 2);
      crate->velocity_x = 3;
      crate->force_y = 4;
      world->config->position_correction = NaN;

      p::step_physics2_d(world, 1.0 / 60);

      expect(crate->x).to_be(0.0);
      expect(crate->y).to_be(2.0);
      expect(crate->velocity_x).to_be(3.0);
      expect(crate->force_y).to_be(4.0);
      expect(contact_count(world)).to_be(0.0);
      expect(world->previous_timestep).to_be(0.0);
    });

    runner.it("scales cached contact impulses into a changed timestep", [] {
      auto world = t::world(0, 0);
      world->config->allow_sleeping = false;
      world->config->velocity_iterations = 0;
      world->config->position_iterations = 0;
      ground(world);
      box(world, 0, 0.49);
      p::step_physics2_d(world, 1.0 / 60);
      auto point = world->contacts.element(0)->points.element(0);
      point->normal_impulse = 3;
      point->tangent_impulse = 4;

      p::step_physics2_d(world, 1.0 / 30);

      expect(point->normal_impulse).to_be(6.0);
      expect(point->tangent_impulse).to_be(8.0);
      expect(world->previous_timestep).to_be(1.0 / 30);
    });

    runner.it("rests a box on the ground instead of sinking through it", [] {
      auto world = default_world();
      ground(world);
      auto crate = box(world, 0, 2);
      run_steps(world, 180);
      expect(crate->y).to_be_greater_than(0.48);
      expect(crate->y).to_be_less_than(0.52);
      expect(std::abs(crate->velocity_y)).to_be_less_than(0.05);
    });

    runner.it("keeps a stack standing rather than letting the lower boxes be compressed through each other", [] {
      auto world = default_world();
      ground(world);
      auto bottom = box(world, 0, 0.5);
      auto middle = box(world, 0, 1.5);
      auto top = box(world, 0, 2.5);
      run_steps(world, 240);
      expect(bottom->y).to_be_greater_than(0.45);
      expect(middle->y).to_be_greater_than(1.4);
      expect(top->y).to_be_greater_than(2.35);
      expect(middle->y - bottom->y).to_be_greater_than(0.9);
      expect(top->y - middle->y).to_be_greater_than(0.9);
    });

    runner.it("uses positionIterations to project overlap without adding separating velocity", [] {
      auto without_projection = t::world(0, 0);
      without_projection->config->position_iterations = 0;
      auto fixed_without = t::body("static", 0, 0);
      push(fixed_without, t::collider(t::aabb(-0.5, -0.5, 0.5, 0.5), stone()));
      add(without_projection, fixed_without);
      auto body_without = box(without_projection, 0.75, 0);

      auto with_projection = t::world(0, 0);
      with_projection->config->position_iterations = 3;
      auto fixed_with = t::body("static", 0, 0);
      push(fixed_with, t::collider(t::aabb(-0.5, -0.5, 0.5, 0.5), stone()));
      add(with_projection, fixed_with);
      auto body_with = box(with_projection, 0.75, 0);

      p::step_physics2_d(without_projection, 1.0 / 60);
      p::step_physics2_d(with_projection, 1.0 / 60);

      expect(body_without->x - fixed_without->x).to_be(0.75);
      expect(body_with->x - fixed_with->x).to_be_greater_than(0.84);
      expect(body_with->velocity_x).to_be(0.0);
    });

    runner.it("clears contact accumulators when warm starting is switched off", [] {
      auto world = default_world();
      world->config->allow_sleeping = false;
      ground(world);
      box(world, 0, 0.5);
      run_steps(world, 60);
      expect(world->contacts.element(0)->points.element(0)->normal_impulse).to_be_greater_than(0.0);

      world->config->warm_starting = false;
      world->config->velocity_iterations = 0;
      world->config->position_iterations = 0;
      p::step_physics2_d(world, 1.0 / 60);

      for (const auto& contact : world->contacts) {
        for (double i = 0; i < contact->point_count; i++) {
          expect(contact->points.element(i)->normal_impulse).to_be(0.0);
          expect(contact->points.element(i)->tangent_impulse).to_be(0.0);
        }
      }
    });

    runner.it("produces a bitwise-identical trace for the same scene stepped twice", [] {
      auto first = default_world();
      ground(first);
      box(first, 0.1, 2);
      box(first, -0.3, 3.2);
      run_steps(first, 120);

      auto second = default_world();
      ground(second);
      box(second, 0.1, 2);
      box(second, -0.3, 3.2);
      run_steps(second, 120);

      expect_true(trace_world(second) == trace_world(first), "identical traces");
    });

    runner.it("produces the same trace when the broadphase reports its pairs in the opposite order", [] {
      auto plain = default_world();
      ground(plain);
      auto plain_left = box(plain, -0.9, 0.5);
      auto plain_right = box(plain, 0.9, 0.5);
      auto plain_top = box(plain, 0, 1.6);

      auto reversed = flight::physics2d::create_physics2_dworld(0.0, -9.81, reversed_pair_backend());
      ground(reversed);
      auto reversed_left = box(reversed, -0.9, 0.5);
      auto reversed_right = box(reversed, 0.9, 0.5);
      auto reversed_top = box(reversed, 0, 1.6);

      run_steps(plain, 90);
      run_steps(reversed, 90);

      expect_true(trace_world(reversed) == trace_world(plain), "identical traces");
      expect(reversed_left->y).to_be(plain_left->y);
      expect(reversed_right->y).to_be(plain_right->y);
      expect(reversed_top->y).to_be(plain_top->y);
    });

    runner.it("orders every contact pair by body index however the broadphase hands it over", [] {
      auto swapped = flight::physics2d::create_physics2_dworld(0.0, -9.81, swapped_pair_backend());
      ground(swapped);
      auto crate = box(swapped, 0.2, 1.4);
      run_steps(swapped, 120);

      for (const auto& contact : swapped->contacts) expect(contact->body_a).to_be_less_than(contact->body_b);

      auto plain = default_world();
      ground(plain);
      auto plain_crate = box(plain, 0.2, 1.4);
      run_steps(plain, 120);

      expect(crate->y).to_be(plain_crate->y);
      expect(crate->x).to_be(plain_crate->x);
      expect(crate->angle).to_be(plain_crate->angle);
    });

    runner.it("keeps the contact list in a canonical order after a step", [] {
      auto world = default_world();
      ground(world);
      box(world, -0.9, 0.5);
      box(world, 0.9, 0.5);
      box(world, 0, 1.6);
      run_steps(world, 60);

      expect(contact_count(world)).to_be_greater_than(1.0);
      for (double i = 1; i < contact_count(world); i++) {
        auto previous = world->contacts.element(i - 1);
        auto current = world->contacts.element(i);
        const bool ordered = previous->body_a < current->body_a ||
                             (previous->body_a == current->body_a &&
                              (previous->body_b < current->body_b ||
                               (previous->body_b == current->body_b && previous->collider_a <= current->collider_a)));
        expect(ordered).to_be(true);
      }
    });

    runner.it("tips a box that overhangs a ledge instead of sliding off level", [] {
      auto world = default_world();
      auto ledge = t::body("static", 0, 0);
      push(ledge, t::collider(t::aabb(-5, -1, 0, 0), stone()));
      add(world, ledge);
      auto crate = box(world, 0.2, 0.5);
      run_steps(world, 120);
      expect(std::abs(crate->angle)).to_be_greater_than(0.05);
    });

    runner.it("leaves a sensor collider overlapping without pushing anything out of it", [] {
      auto world = default_world();
      auto trigger = t::body("static", 0, 0);
      push(trigger, t::collider(t::aabb(-2, -2, 2, 2), stone(), true));
      add(world, trigger);
      auto crate = box(world, 0, 0);
      run_steps(world, 30);

      bool any_sensor = false;
      for (const auto& contact : world->contacts) any_sensor = any_sensor || contact->sensor;
      expect(any_sensor).to_be(true);
      expect(crate->velocity_y).to_be_less_than(-0.1);
    });

    runner.it("rejects a non-positive or non-finite timestep before mutating the world", [] {
      auto world = default_world();
      ground(world);
      auto crate = box(world, 0, 2);
      const auto before = trace_world(world);
      for (double dt : {0.0, -1.0 / 60, NaN, INF, -INF}) {
        p::step_physics2_d(world, dt);
        expect_true(trace_world(world) == before, "the world is untouched");
      }
      expect(crate->y).to_be(2.0);
    });

    struct InvalidIteration {
      const char* field;
      double value;
    };
    for (InvalidIteration row : {InvalidIteration{"velocityIterations", INF}, InvalidIteration{"velocityIterations", -1},
                                 InvalidIteration{"velocityIterations", 1.5}, InvalidIteration{"positionIterations", INF},
                                 InvalidIteration{"positionIterations", -1}, InvalidIteration{"positionIterations", 1.5}}) {
      runner.it(std::string("rejects invalid ") + row.field + " value " + std::to_string(row.value) + " before mutating the world", [row] {
        auto world = default_world();
        ground(world);
        box(world, 0, 2);
        const auto before = trace_world(world);
        if (std::string(row.field) == "velocityIterations") {
          world->config->velocity_iterations = row.value;
        } else {
          world->config->position_iterations = row.value;
        }

        p::step_physics2_d(world, 1.0 / 60);

        expect_true(trace_world(world) == before, "the world is untouched");
        expect(contact_count(world)).to_be(0.0);
      });
    }
  });

  runner.describe("stepPhysics2D contact cache identity", [&] {
    runner.it("preserves every cached point impulse when manifold feature order changes", [] {
      auto world = t::world(0, 0);
      world->config->position_iterations = 0;
      world->config->velocity_iterations = 0;
      box(world, 0, 0);
      box(world, 0, 0.75);
      p::step_physics2_d(world, 1.0 / 60);
      auto contact = world->contacts.element(0);
      expect(contact->point_count).to_be(2.0);
      std::map<double, std::pair<double, double>> expected;
      for (double i = 0; i < contact->point_count; i++) {
        auto point = contact->points.element(i);
        point->normal_impulse = i + 1;
        point->tangent_impulse = -(i + 1);
        expected[point->feature_id] = {point->normal_impulse, point->tangent_impulse};
      }
      contact->points.reverse();

      p::step_physics2_d(world, 1.0 / 60);

      for (double i = 0; i < contact->point_count; i++) {
        auto point = contact->points.element(i);
        auto cached = expected.find(point->feature_id);
        expect(cached != expected.end()).to_be(true);
        expect(point->normal_impulse).to_be(cached->second.first);
        expect(point->tangent_impulse).to_be(cached->second.second);
      }
    });
  });

  runner.describe("stepPhysics2D contact events", [&] {
    runner.it("reports a contact beginning and ending, read off the cache transitions", [] {
      auto world = default_world();
      ground(world);
      auto crate = box(world, 0, 3);
      run_steps(world, 1);
      expect(count_of(world->events->began)).to_be(0.0);

      double began = 0;
      for (int i = 0; i < 200 && began == 0; i++) {
        p::step_physics2_d(world, 1.0 / 60);
        began += count_of(world->events->began);
      }
      expect(began).to_be(1.0);

      crate->y = 50;
      crate->velocity_y = 0;
      p::step_physics2_d(world, 1.0 / 60);
      expect(count_of(world->events->ended)).to_be(1.0);
      expect(contact_count(world)).to_be(0.0);
    });

    runner.it("clears its event buffers each step rather than accumulating", [] {
      auto world = default_world();
      ground(world);
      box(world, 0, 0.4);
      run_steps(world, 5);
      expect(count_of(world->events->began)).to_be(0.0);
      expect(count_of(world->events->ended)).to_be(0.0);
    });

    runner.it("reports begin and end events for a point sensor without inventing a manifold", [] {
      auto world = t::world(0, 0);
      auto trigger = t::body("static", 0, 0);
      push(trigger, t::collider(t::point(0, 0), stone(), true));
      add(world, trigger);
      auto target = t::body("static", 0, 0);
      push(target, t::collider(t::aabb(-0.5, -0.5, 0.5, 0.5), stone()));
      add(world, target);

      p::step_physics2_d(world, 1.0 / 60);

      expect(count_of(world->events->began)).to_be(1.0);
      expect(world->contacts.element(0)->point_count).to_be(0.0);
      expect(world->contacts.element(0)->sensor).to_be(true);
      expect(world->contacts.element(0)->touching).to_be(true);

      target->x = 2;
      p::step_physics2_d(world, 1.0 / 60);

      expect(count_of(world->events->ended)).to_be(1.0);
      expect(contact_count(world)).to_be(0.0);
    });

    runner.it("reports a segment overlap when the area-bearing collider is the sensor", [] {
      auto world = t::world(0, 0);
      auto segment = t::body("static", 0, 0);
      push(segment, t::collider(t::segment(-2, 0, 2, 0), stone()));
      add(world, segment);
      auto trigger = t::body("static", 0, 0);
      push(trigger, t::collider(t::circle(0, 0, 0.5), stone(), true));
      add(world, trigger);

      p::step_physics2_d(world, 1.0 / 60);

      expect(count_of(world->events->began)).to_be(1.0);
      expect(world->contacts.element(0)->point_count).to_be(0.0);
      expect(world->contacts.element(0)->sensor).to_be(true);
      expect(world->contacts.element(0)->touching).to_be(true);
    });
  });
}

void capsules(flight::upstream_test::Runner& runner) {
  runner.describe("stepPhysics2D with capsule colliders", [&] {
    runner.it("settles a horizontal capsule flat on the ground without rocking, and lets it sleep", [] {
      auto world = t::world(0, -10);
      auto floor = t::body("static", 0, 0);
      push(floor, t::collider(t::aabb(-20, -1, 20, 0), stone()));
      add(world, floor);
      auto capsule = t::body("dynamic", 0, 2);
      push(capsule, t::collider(t::capsule(-1, 0, 1, 0, 0.5), stone()));
      add(world, capsule);

      double worst_angle_after_settling = 0;
      for (int step = 0; step < 900; step++) {
        p::step_physics2_d(world, 1.0 / 240);
        if (step > 500) worst_angle_after_settling = std::max(worst_angle_after_settling, std::abs(capsule->angle));
      }

      expect(capsule->y).to_be_close_to(0.5, 2);
      expect(worst_angle_after_settling).to_be_less_than(1e-6);
      expect(capsule->sleeping).to_be(true);
    });

    runner.it("stops a capsule bullet at a thin wall instead of tunnelling through it", [] {
      auto world = t::world(0, 0);
      world->config->continuous_collision = true;
      auto wall = t::body("static", 0, 0);
      push(wall, t::collider(t::aabb(9.9, -5, 10, 5), stone()));
      add(world, wall);
      auto bullet = t::body("dynamic", 0, 0);
      bullet->bullet = true;
      push(bullet, t::collider(t::capsule(-0.2, 0, 0.2, 0, 0.1), stone()));
      add(world, bullet);
      bullet->velocity_x = 600;

      for (int step = 0; step < 10; step++) p::step_physics2_d(world, 1.0 / 60);

      expect(bullet->x).to_be_less_than(10.0);
    });

    runner.it("rolls a capsule down a slope rather than sticking on a corner", [] {
      auto world = t::world(0, -10);
      auto slope = t::body("static", 0, 0);
      slope->angle = -0.3;
      push(slope, t::collider(t::aabb(-20, -1, 20, 0), stone()));
      add(world, slope);
      auto capsule = t::body("dynamic", -2, 2);
      push(capsule, t::collider(t::capsule(-0.4, 0, 0.4, 0, 0.3), stone()));
      add(world, capsule);

      for (int step = 0; step < 600; step++) p::step_physics2_d(world, 1.0 / 240);

      expect(std::isfinite(capsule->x)).to_be(true);
      expect(capsule->x).to_be_greater_than(-2.0);
      expect(capsule->y).to_be_greater_than(-2.0);
    });

    runner.it("stacks a capsule on a capsule without either falling through", [] {
      auto world = t::world(0, -10);
      auto floor = t::body("static", 0, 0);
      push(floor, t::collider(t::aabb(-20, -1, 20, 0), stone()));
      add(world, floor);
      auto lower = t::body("dynamic", 0, 1);
      push(lower, t::collider(t::capsule(-1, 0, 1, 0, 0.5), stone()));
      add(world, lower);
      auto upper = t::body("dynamic", 0, 2.5);
      push(upper, t::collider(t::capsule(-1, 0, 1, 0, 0.5), stone()));
      add(world, upper);

      for (int step = 0; step < 1200; step++) p::step_physics2_d(world, 1.0 / 240);

      expect(lower->y).to_be_close_to(0.5, 1);
      expect(upper->y).to_be_close_to(1.5, 1);
      expect(upper->y).to_be_greater_than(lower->y + 0.9);
    });
  });
}

void joints_and_sleep(flight::upstream_test::Runner& runner) {
  runner.describe("stepPhysics2D with joints", [&] {
    runner.it("does not suppress collision through a one-body joint placeholder", [] {
      auto world = t::world(0, 0);
      auto one_body = flight::make_ref<types::Physics2DJointSolver>();
      one_body->prepare = [](auto, auto, double) {};
      one_body->solve = [](auto, auto) {};
      one_body->uses_body_a = false;
      p::register_physics2_djoint_solver(world, flight::String("OneBody"), one_body);
      auto placeholder = box(world, 0, 0);
      auto constrained = box(world, 0.2, 0);
      p::add_physics2_djoint(world, entity_joint("OneBody", placeholder->index, constrained->index));

      p::step_physics2_d(world, 1.0 / 60);

      expect(contact_count(world)).to_be(1.0);
    });
  });

  runner.describe("stepPhysics2D with sleeping", [&] {
    runner.it("settles a resting crate to sleep and then holds its pose exactly", [] {
      auto world = default_world();
      ground(world);
      auto crate = box(world, 0, 2);
      run_steps(world, 240);
      expect(crate->sleeping).to_be(true);

      const double resting_y = crate->y;
      run_steps(world, 120);

      expect(crate->y).to_be(resting_y);
      expect(crate->velocity_y).to_be(0.0);
    });

    runner.it("wakes a sleeping crate when another body lands on it", [] {
      auto world = default_world();
      ground(world);
      auto settled = box(world, 0, 2);
      run_steps(world, 240);
      expect(settled->sleeping).to_be(true);

      box(world, 0, 4);
      run_steps(world, 60);

      expect(settled->sleeping).to_be(false);
    });

    runner.it("wakes a sleeping crate in the same step a force is applied to it", [] {
      auto world = default_world();
      ground(world);
      auto crate = box(world, 0, 2);
      run_steps(world, 240);
      expect(crate->sleeping).to_be(true);

      crate->force_x = 500;
      p::step_physics2_d(world, 1.0 / 60);

      expect(crate->sleeping).to_be(false);
      expect(crate->velocity_x).to_be_greater_than(0.0);
    });

    runner.it("never leaves a sleeping body holding velocity, however it came to rest", [] {
      auto world = default_world();
      ground(world);
      auto settled = box(world, 0, 2);
      run_steps(world, 240);
      expect(settled->sleeping).to_be(true);
      box(world, 0.1, 4);

      int sleeping_samples = 0;
      for (int i = 0; i < 600; i++) {
        p::step_physics2_d(world, 1.0 / 60);
        for (const auto& body : world->bodies) {
          if (!body->sleeping) continue;
          sleeping_samples++;
          expect(body->velocity_x).to_be(0.0);
          expect(body->velocity_y).to_be(0.0);
          expect(body->angular_velocity).to_be(0.0);
        }
      }

      expect(sleeping_samples).to_be_greater_than(0);
    });

    runner.it("invokes no joint solver callback for a pair whose every end is asleep", [] {
      std::vector<std::string> calls;
      auto world = default_world();
      auto recording = flight::make_ref<types::Physics2DJointSolver>();
      recording->prepare = [&calls](auto, auto, double) { calls.push_back("prepare"); };
      recording->warm_start = [&calls](flight::Ref<types::Physics2DWorld>, flight::Ref<types::Physics2DJoint>) { calls.push_back("warmStart"); };
      recording->solve = [&calls](auto, auto) { calls.push_back("solve"); };
      recording->clear_accumulated_impulses = [&calls](flight::Ref<types::Physics2DJoint>) { calls.push_back("clear"); };
      p::register_physics2_djoint_solver(world, flight::String("Recording"), recording);
      ground(world);
      auto left = box(world, -1, 2);
      auto right = box(world, 1, 2);
      p::add_physics2_djoint(world, entity_joint("Recording", left->index, right->index, true));
      run_steps(world, 240);
      expect(left->sleeping).to_be(true);
      expect(right->sleeping).to_be(true);

      calls.clear();
      run_steps(world, 30);

      expect(static_cast<double>(calls.size())).to_be(0.0);
    });

    runner.it("never sleeps a resting crate when allowSleeping is off", [] {
      auto world = default_world();
      world->config->allow_sleeping = false;
      ground(world);
      auto crate = box(world, 0, 2);
      run_steps(world, 240);
      expect(crate->sleeping).to_be(false);
    });
  });
}

void stress(flight::upstream_test::Runner& runner) {
  runner.describe("physics2d stress qualification", [&] {
    auto material = [] { return t::material(1, 0.4, 0); };
    auto add_box = [=](const t::World& world, const char* type, double x, double y, double half = 0.5) {
      auto body = t::body(type, x, y);
      push(body, t::collider(t::aabb(-half, -half, half, half), material()));
      return add(world, body);
    };
    auto expect_finite_body = [](const t::Body& body) {
      for (double value : {body->x, body->y, body->angle, body->velocity_x, body->velocity_y, body->angular_velocity,
                           body->mass, body->inertia}) {
        expect(std::isfinite(value)).to_be(true);
      }
    };

    runner.it("keeps a tall pile finite, ordered, and supported over a long settle horizon", [=] {
      auto world = t::world(0, -10);
      auto floor = t::body("static", 0, 0);
      push(floor, t::collider(t::aabb(-20, -1, 20, 0), material()));
      add(world, floor);
      for (auto [min_x, max_x] : std::vector<std::pair<double, double>>{{-1, -0.51}, {0.51, 1}}) {
        auto wall = t::body("static", 0, 0);
        push(wall, t::collider(t::aabb(min_x, 0, max_x, 20), material()));
        add(world, wall);
      }
      std::vector<t::Body> pile;
      for (int i = 0; i < 16; i++) pile.push_back(add_box(world, "dynamic", 0, 0.5 + i));

      for (int i = 0; i < 900; i++) p::step_physics2_d(world, 1.0 / 60);

      for (const auto& body : pile) expect_finite_body(body);
      for (std::size_t i = 1; i < pile.size(); i++) expect(pile[i]->y).to_be_greater_than(pile[i - 1]->y);
      expect(pile[0]->y).to_be_greater_than(0.4);
      expect(pile.back()->y).to_be_greater_than(13.5);
      bool all_sleeping = true;
      for (const auto& body : pile) all_sleeping = all_sleeping && body->sleeping;
      expect(all_sleeping).to_be(true);
    });

    runner.it("retains stable-topology contacts and every world-owned island workspace object", [=] {
      auto world = t::world(0, -10);
      auto floor = t::body("static", 0, 0);
      push(floor, t::collider(t::aabb(-5, -1, 5, 0), material()));
      add(world, floor);
      add_box(world, "dynamic", 0, 0.5);
      for (int i = 0; i < 240; i++) p::step_physics2_d(world, 1.0 / 60);
      expect(contact_count(world)).to_be(1.0);
      auto contact = world->contacts.element(0);
      auto points = contact->points;
      auto island_parents = world->island_parents;
      auto island_sleep_timers = world->island_sleep_timers;
      auto solve_island_by_root = world->solve_island_by_root;
      std::vector<flight::Array<double>> arrays{
          world->solve_island_roots,          world->solve_island_body_starts,   world->solve_island_body_counts,
          world->solve_island_contact_starts, world->solve_island_contact_counts, world->solve_island_joint_starts,
          world->solve_island_joint_counts,   world->solve_island_body_indices,  world->solve_island_contact_indices,
          world->solve_island_joint_indices,  world->solve_island_cursors};

      for (int i = 0; i < 600; i++) p::step_physics2_d(world, 1.0 / 60);

      expect_true(world->contacts.element(0) == contact, "the same contact object");
      // Identity of the retained Array/Map handles, probed by a write through the retained handle.
      const auto same_points = [&] {
        const auto before = world->contacts.element(0)->points.size();
        points.push(points.element(0));
        const bool seen = world->contacts.element(0)->points.size() == before + 1;
        (void)points.pop();
        return seen;
      };
      expect(same_points()).to_be(true);
      std::vector<flight::Array<double>> retained{
          world->solve_island_roots,          world->solve_island_body_starts,   world->solve_island_body_counts,
          world->solve_island_contact_starts, world->solve_island_contact_counts, world->solve_island_joint_starts,
          world->solve_island_joint_counts,   world->solve_island_body_indices,  world->solve_island_contact_indices,
          world->solve_island_joint_indices,  world->solve_island_cursors};
      for (std::size_t i = 0; i < arrays.size(); i++) {
        const auto before = retained[i].size();
        arrays[i].push(-12345.0);
        const bool seen = retained[i].size() == before + 1;
        (void)arrays[i].pop();
        expect_true(seen, "a retained island workspace array is the world's own");
      }
      const auto probe_map = [](flight::Map<double, double> saved, flight::Map<double, double> current) {
        saved.set(-12345.0, 1.0);
        const bool seen = current.has(-12345.0);
        (void)saved.erase(-12345.0);
        return seen;
      };
      expect_true(probe_map(island_parents, world->island_parents), "islandParents retained");
      expect_true(probe_map(island_sleep_timers, world->island_sleep_timers), "islandSleepTimers retained");
      expect_true(probe_map(solve_island_by_root, world->solve_island_by_root), "solveIslandByRoot retained");
    });
  });
}

}  // namespace

int main() {
  flight::upstream_test::Runner runner;
  declined_body(runner);
  bullets(runner);
  centre_of_mass(runner);
  filtering(runner);
  hooks(runner);
  immovable_sensors(runner);
  seams(runner);
  step_physics(runner);
  capsules(runner);
  joints_and_sleep(runner);
  stress(runner);
  return runner.run();
}
