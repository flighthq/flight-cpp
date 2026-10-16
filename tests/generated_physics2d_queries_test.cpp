// The generated @flighthq/physics2d world queries, joint registry and joint reactions at a627849, exercised
// natively. Ported from worldQueries.test.ts, jointRegistry.test.ts and jointReactions.test.ts with the same
// inputs, assertions and tolerances.
//
// Colliders come from tests/physics2d_test_support.hpp's collider(), the documented fixture for the refused
// createPhysics2DCollider: its world shape is the plain arm, without the entity marker TypeScript gives it.
// Joints are plain Physics2DJoint rows built field by field, exactly as the upstream jointRegistry and
// jointReactions files build theirs with allocateEntity.
//
// Not ported, and why:
// - The three initialize*Result and initializePhysics2DJointReaction cases only check `typeof … === 'function'`;
//   compiling against the functions is that check.
// - The Object.hasOwn(result, EntityRuntimeKey) halves of the create*Result and createPhysics2DJointReaction
//   cases ask about a JavaScript own symbol property; the remaining field checks are ported.
// - queryPhysics2DPoint "keeps broadphase candidates isolated when a filter getter starts a nested query"
//   installs an accessor property with Object.defineProperty; a native filter's fields are plain members.
// - jointRegistry "canonicalizes an already-added joint when its solver registers later, reversing direction"
//   writes a `direction` member the flattened Physics2DJoint row cannot carry (the joint downcast boundary,
//   docs/compiler-requests/assertion-to-undeclared-row-member.md).
// - jointReactions: every case that settles a joint under a built-in solver (eleven, all through
//   registerBuiltInPhysics2DJointSolvers or the joint kinds' solvers) needs the joint solvers that joints.ts
//   refuses at a627849, so they cannot run.
#include "physics2d_test_support.hpp"
#include "upstream_test_harness.hpp"

#include <flight/physics2d/joint_reactions.hpp>
#include <flight/physics2d/joint_registry.hpp>
#include <flight/physics2d/step.hpp>
#include <flight/physics2d/world_queries.hpp>

#include <cmath>
#include <functional>
#include <limits>
#include <string>
#include <vector>

namespace {

namespace p = flight::physics2d;
namespace t = flight::physics2d_test;
namespace types = flight::types;
using flight::upstream_test::expect;
using flight::upstream_test::AssertionFailure;

constexpr double INF = std::numeric_limits<double>::infinity();

auto stone() { return t::material(1, 0.3, 0); }

// createPhysics2DWorld() with no arguments: gravity (0, -9.81). The shared support header's world() defaults to
// zero gravity, so the no-argument upstream form goes straight to the generated constructor.
t::World default_world() { return p::create_physics_2d_world(); }

t::Body add(const t::World& world, const t::Body& body) { return p::add_physics_2d_body(world, body); }

void push(const t::Body& body, const t::Collider& collider) { body->colliders.push(collider); }

flight::Ref<types::SpatialAabb2D> region(double min_x, double min_y, double max_x, double max_y) {
  return flight::make_ref<types::SpatialAabb2D>(types::SpatialAabb2D{.min_x = min_x, .min_y = min_y, .max_x = max_x, .max_y = max_y});
}

template <typename Function>
void expect_throws(Function&& function) {
  bool threw = false;
  try {
    function();
  } catch (const AssertionFailure&) {
    throw;
  } catch (...) {
    threw = true;
  }
  if (!threw) throw AssertionFailure("expected the call to throw");
}

template <typename Function>
void expect_no_throw(Function&& function) {
  function();
}

std::vector<t::Body> hit_bodies(const flight::Ref<types::Physics2DQueryResult>& out) {
  std::vector<t::Body> bodies;
  for (double i = 0; i < out->hit_count; ++i) bodies.push_back(out->hits.element(i)->body);
  return bodies;
}

std::vector<t::Body> ray_bodies(const flight::Ref<types::Physics2DRayResult>& out) {
  std::vector<t::Body> bodies;
  for (double i = 0; i < out->hit_count; ++i) bodies.push_back(out->hits.element(i)->body);
  return bodies;
}

std::vector<std::pair<double, double>> hit_pairs(const flight::Ref<types::Physics2DQueryResult>& out) {
  std::vector<std::pair<double, double>> pairs;
  for (double i = 0; i < out->hit_count; ++i) {
    pairs.emplace_back(out->hits.element(i)->body->index, out->hits.element(i)->collider_index);
  }
  return pairs;
}

void expect_true(bool condition, const char* what) {
  if (!condition) throw AssertionFailure(what);
}

// The upstream jointRegistry/jointReactions `joint()` / `base()` helper: every common field written, through
// allocateEntity and finishEntity.
flight::Ref<types::Physics2DJoint> joint(const char* kind, double body_a, double body_b, double anchor_ax = 0,
                                         double anchor_bx = 0) {
  auto row = flight::entity::finish_entity<flight::Ref<types::Physics2DJoint>>(
      flight::entity::allocate_entity<flight::Ref<types::Physics2DJoint>>());
  row->kind = flight::String(kind);
  row->body_a = body_a;
  row->body_b = body_b;
  row->local_anchor_ax = anchor_ax;
  row->local_anchor_ay = 0;
  row->local_anchor_bx = anchor_bx;
  row->local_anchor_by = 0;
  row->collide_connected = false;
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

flight::Ref<types::Physics2DJointSolver> solver(std::function<void()> on_prepare = [] {},
                                                std::function<void()> on_solve = [] {}) {
  auto out = flight::make_ref<types::Physics2DJointSolver>();
  out->prepare = [on_prepare](flight::Ref<types::Physics2DWorld>, flight::Ref<types::Physics2DJoint>, double) { on_prepare(); };
  out->solve = [on_solve](flight::Ref<types::Physics2DWorld>, flight::Ref<types::Physics2DJoint>) { on_solve(); };
  return out;
}

// jointRegistry.test.ts's `body()`: a dynamic unit box.
t::Body registry_body(const t::World& world, double x, double y) {
  auto made = t::body("dynamic", x, y);
  push(made, t::collider(t::aabb(-0.5, -0.5, 0.5, 0.5), stone()));
  return add(world, made);
}

std::optional<double> suppression(const t::World& world, double a, double b) {
  auto inner = world->joint_collision_suppressions.get(a);
  if (!inner.has_value()) return std::nullopt;
  return inner->get(b);
}

void world_queries(flight::upstream_test::Runner& runner) {
  runner.describe("createPhysics2DQueryFilter", [&] {
    runner.it("includes every body kind, sensor, category, and mask by default", [] {
      auto filter = p::create_physics_2d_query_filter();
      expect(filter->category_bits).to_be(4294967295.0);
      expect(filter->mask_bits).to_be(4294967295.0);
      expect(filter->include_sensors).to_be(true);
      expect(filter->include_dynamic).to_be(true);
      expect(filter->include_kinematic).to_be(true);
      expect(filter->include_static).to_be(true);
    });
  });
  runner.describe("createPhysics2DQueryResult", [&] {
    runner.it("starts with no live hits", [] {
      auto result = p::create_physics_2d_query_result();
      expect(static_cast<double>(result->hits.size())).to_be(0.0);
      expect(result->hit_count).to_be(0.0);
    });
  });
  runner.describe("createPhysics2DRayResult", [&] {
    runner.it("starts with no live hits", [] {
      auto result = p::create_physics_2d_ray_result();
      expect(static_cast<double>(result->hits.size())).to_be(0.0);
      expect(result->hit_count).to_be(0.0);
    });
  });
  runner.describe("createPhysics2DShapeCastResult", [&] {
    runner.it("starts as a miss with nothing referenced", [] {
      auto result = p::create_physics_2d_shape_cast_result();
      expect(result->body.has_value()).to_be(false);
      expect(result->collider.has_value()).to_be(false);
      expect(result->collider_index).to_be(-1.0);
      expect(result->hit).to_be(false);
      expect(result->fraction).to_be(0.0);
      expect(result->x).to_be(0.0);
      expect(result->y).to_be(0.0);
      expect(result->normal_x).to_be(0.0);
      expect(result->normal_y).to_be(0.0);
    });
  });

  runner.describe("queryPhysics2DPoint", [&] {
    runner.it("returns exact collider hits rather than broadphase-only body hits", [] {
      auto world = t::world(0, 0);
      auto body = t::body("dynamic", 0, 0, M_PI / 4);
      push(body, t::collider(t::aabb(-2, -0.25, 2, 0.25), stone()));
      add(world, body);
      auto out = p::create_physics_2d_query_result();

      p::query_physics_2d_point(world, 1, 0, out);
      expect(out->hit_count).to_be(0.0);
      p::query_physics_2d_point(world, 1, 1, out);

      expect(out->hit_count).to_be(1.0);
      expect_true(out->hits.element(0)->body == body, "hit body");
      expect_true(out->hits.element(0)->collider == body->colliders.element(0), "hit collider");
      expect(out->hits.element(0)->collider_index).to_be(0.0);
    });

    runner.it("observes current poses and reuses its high-water hit object", [] {
      auto world = t::world(0, 0);
      auto body = t::body("dynamic", 0, 0);
      push(body, t::collider(t::circle(0, 0, 1), stone()));
      add(world, body);
      auto out = p::create_physics_2d_query_result();

      p::query_physics_2d_point(world, 0, 0, out);
      auto retained_hit = out->hits.element(0);
      body->x = 5;
      p::query_physics_2d_point(world, 0, 0, out);
      expect(out->hit_count).to_be(0.0);
      p::query_physics_2d_point(world, 5, 0, out);

      expect(out->hit_count).to_be(1.0);
      expect_true(out->hits.element(0) == retained_hit, "the retained hit object is reused");
    });

    runner.it("reports every containing collider in deterministic order", [] {
      auto world = t::world(0, 0);
      auto first = t::body("dynamic", 0, 0);
      push(first, t::collider(t::circle(0, 0, 2), stone()));
      push(first, t::collider(t::circle(0, 0, 1), stone()));
      auto second = t::body("dynamic", 0, 0);
      push(second, t::collider(t::circle(0, 0, 1), stone()));
      add(world, first);
      add(world, second);
      auto out = p::create_physics_2d_query_result();

      p::query_physics_2d_point(world, 0, 0, out);

      expect_true(hit_pairs(out) == std::vector<std::pair<double, double>>{{first->index, 0}, {first->index, 1}, {second->index, 0}},
                  "deterministic body/collider order");
    });

    runner.it("queries transformed point and segment colliders at their current pose", [] {
      auto world = t::world(0, 0);
      auto body = t::body("dynamic", 3, 4, M_PI / 2);
      push(body, t::collider(t::point(2, 0), stone()));
      push(body, t::collider(t::segment(0, 0, 2, 0), stone()));
      add(world, body);
      auto out = p::create_physics_2d_query_result();

      p::query_physics_2d_point(world, 3, 6, out);

      std::vector<double> indices;
      for (double i = 0; i < out->hit_count; ++i) indices.push_back(out->hits.element(i)->collider_index);
      expect_true(indices == std::vector<double>{0, 1}, "both transformed colliders contain the point");
    });

    runner.it("filters body participation, sensors, collider categories, and collider masks", [] {
      auto world = t::world(0, 0);
      auto dynamic = t::body("dynamic", 0, 0);
      push(dynamic, t::collider(t::circle(0, 0, 1), stone(), true, t::filter(0x2, 0x4, 0)));
      auto fixed = t::body("static", 0, 0);
      push(fixed, t::collider(t::circle(0, 0, 1), stone()));
      add(world, dynamic);
      add(world, fixed);
      auto filter = p::create_physics_2d_query_filter();
      filter->category_bits = 0x2;
      filter->mask_bits = 0x4;
      filter->include_sensors = false;
      filter->include_static = false;
      auto out = p::create_physics_2d_query_result();

      p::query_physics_2d_point(world, 0, 0, out, t::view(filter));
      expect(out->hit_count).to_be(0.0);
      filter->include_sensors = true;
      p::query_physics_2d_point(world, 0, 0, out, t::view(filter));
      expect_true(hit_bodies(out) == std::vector<t::Body>{dynamic}, "only the dynamic sensor passes");
    });
  });

  runner.describe("queryPhysics2DRay", [&] {
    runner.it("returns exact intersections nearest-first rather than body insertion order", [] {
      auto world = t::world(0, 0);
      auto far = t::body("dynamic", 5, 0);
      push(far, t::collider(t::circle(0, 0, 1), stone()));
      auto near = t::body("dynamic", 2, 0);
      push(near, t::collider(t::aabb(-0.5, -1, 0.5, 1), stone()));
      add(world, far);
      add(world, near);
      auto out = p::create_physics_2d_ray_result();

      p::query_physics_2d_ray(world, 0, 0, 1, 0, out);

      expect(out->hit_count).to_be(2.0);
      expect_true(ray_bodies(out) == std::vector<t::Body>{near, far}, "nearest first");
      auto first = out->hits.element(0);
      expect(first->collider_index).to_be(0.0);
      expect(first->fraction).to_be(1.5);
      expect(first->normal_x).to_be(-1.0);
      expect(first->normal_y).to_be(0.0);
      expect(first->x).to_be(1.5);
      expect(first->y).to_be(0.0);
      expect(out->hits.element(1)->fraction).to_be_close_to(4);
    });

    runner.it("refines broadphase candidates against rotated shape geometry", [] {
      auto world = t::world(0, 0);
      auto body = t::body("dynamic", 0, 0, M_PI / 4);
      push(body, t::collider(t::aabb(-2, -0.25, 2, 0.25), stone()));
      add(world, body);
      auto out = p::create_physics_2d_ray_result();

      p::query_physics_2d_ray(world, -1.5, 1.4, 0.2, 0, out, 1.0);

      expect(out->hit_count).to_be(0.0);
    });

    runner.it("honours maxFraction and reuses retained hit objects", [] {
      auto world = t::world(0, 0);
      auto body = t::body("dynamic", 3, 0);
      push(body, t::collider(t::circle(0, 0, 1), stone()));
      add(world, body);
      auto out = p::create_physics_2d_ray_result();
      p::query_physics_2d_ray(world, 0, 0, 1, 0, out);
      auto retained = out->hits.element(0);

      p::query_physics_2d_ray(world, 0, 0, 1, 0, out, 1.0);
      expect(out->hit_count).to_be(0.0);
      p::query_physics_2d_ray(world, 0, 0, 1, 0, out, 2.0);

      expect(out->hit_count).to_be(1.0);
      expect_true(out->hits.element(0) == retained, "the retained hit object is reused");
    });

    runner.it("degenerates a zero direction to an exact point query", [] {
      auto world = t::world(0, 0);
      auto body = t::body("dynamic", 3, 4);
      push(body, t::collider(t::point(0, 0), stone()));
      add(world, body);
      auto out = p::create_physics_2d_ray_result();

      p::query_physics_2d_ray(world, 3, 4, 0, 0, out);

      expect(out->hit_count).to_be(1.0);
      auto hit = out->hits.element(0);
      expect_true(hit->body == body, "hit body");
      expect(hit->fraction).to_be(0.0);
      expect(hit->normal_x).to_be(0.0);
      expect(hit->normal_y).to_be(0.0);
      expect(hit->x).to_be(3.0);
      expect(hit->y).to_be(4.0);
    });
  });

  runner.describe("queryPhysics2DRayClosest", [&] {
    runner.it("writes only the deterministic nearest filtered hit and reuses its retained record", [] {
      auto world = t::world(0, 0);
      auto far = t::body("dynamic", 5, 0);
      push(far, t::collider(t::circle(0, 0, 1), stone(), false, t::filter(0x2, 0x2, 0)));
      auto near = t::body("dynamic", 2, 0);
      push(near, t::collider(t::circle(0, 0, 0.5), stone()));
      add(world, far);
      add(world, near);
      auto out = p::create_physics_2d_ray_result();

      p::query_physics_2d_ray_closest(world, 0, 0, 1, 0, out);
      expect(out->hit_count).to_be(1.0);
      expect_true(out->hits.element(0)->body == near, "nearest body");
      auto retained = out->hits.element(0);

      auto filter = p::create_physics_2d_query_filter();
      filter->category_bits = 0x2;
      filter->mask_bits = 0x2;
      p::query_physics_2d_ray_closest(world, 0, 0, 1, 0, out, INF, t::view(filter));
      expect(out->hit_count).to_be(1.0);
      expect_true(out->hits.element(0) == retained, "retained record");
      expect_true(out->hits.element(0)->body == far, "filtered nearest body");
    });
  });

  runner.describe("queryPhysics2DRegion", [&] {
    runner.it("refines aggregate body candidates to overlapping collider bounds", [] {
      auto world = t::world(0, 0);
      auto body = t::body("dynamic", 0, 0);
      push(body, t::collider(t::circle(-5, 0, 1), stone()));
      push(body, t::collider(t::circle(5, 0, 1), stone()));
      add(world, body);
      auto out = p::create_physics_2d_query_result();

      p::query_physics_2d_region(world, t::view(region(-0.5, -0.5, 0.5, 0.5)), out);
      expect(out->hit_count).to_be(0.0);

      p::query_physics_2d_region(world, t::view(region(4.5, -0.5, 5.5, 0.5)), out);
      expect(out->hit_count).to_be(1.0);
      expect_true(out->hits.element(0)->body == body, "hit body");
      expect_true(out->hits.element(0)->collider == body->colliders.element(1), "hit collider");
      expect(out->hits.element(0)->collider_index).to_be(1.0);
    });

    runner.it("uses current poses and preserves deterministic body/collider order", [] {
      auto world = t::world(0, 0);
      auto first = t::body("dynamic", -10, 0);
      push(first, t::collider(t::circle(0, 0, 1), stone()));
      push(first, t::collider(t::circle(0, 0, 0.5), stone()));
      auto second = t::body("dynamic", 0, 0);
      push(second, t::collider(t::circle(0, 0, 1), stone()));
      add(world, first);
      add(world, second);
      first->x = 0;
      auto out = p::create_physics_2d_query_result();

      p::query_physics_2d_region(world, t::view(region(-0.25, -0.25, 0.25, 0.25)), out);

      expect_true(hit_pairs(out) == std::vector<std::pair<double, double>>{{first->index, 0}, {first->index, 1}, {second->index, 0}},
                  "deterministic body/collider order at current poses");
    });

    runner.it("clears live hits for an empty region while retaining high-water storage", [] {
      auto world = t::world(0, 0);
      auto body = t::body("dynamic", 0, 0);
      push(body, t::collider(t::circle(0, 0, 1), stone()));
      add(world, body);
      auto out = p::create_physics_2d_query_result();
      p::query_physics_2d_region(world, t::view(region(-1, -1, 1, 1)), out);
      auto retained = out->hits.element(0);

      p::query_physics_2d_region(world, t::view(region(10, 10, 11, 11)), out);

      expect(out->hit_count).to_be(0.0);
      expect_true(out->hits.element(0) == retained, "high-water storage retained");
    });

    runner.it("applies the same reusable filter and rejects invalid regions", [] {
      auto world = t::world(0, 0);
      auto body = t::body("kinematic", 0, 0);
      push(body, t::collider(t::circle(0, 0, 1), stone()));
      add(world, body);
      auto filter = p::create_physics_2d_query_filter();
      filter->include_kinematic = false;
      auto out = p::create_physics_2d_query_result();

      p::query_physics_2d_region(world, t::view(region(-1, -1, 1, 1)), out, t::view(filter));
      expect(out->hit_count).to_be(0.0);
      filter->include_kinematic = true;
      p::query_physics_2d_region(world, t::view(region(1, 1, -1, -1)), out, t::view(filter));
      expect(out->hit_count).to_be(0.0);
      p::query_physics_2d_region(world, t::view(region(-1, -1, 1, 1)), out, t::view(filter));
      expect_true(hit_bodies(out) == std::vector<t::Body>{body}, "the kinematic body once included");
    });
  });

  runner.describe("queryPhysics2DShapeCast", [&] {
    auto walled_world = [] {
      auto world = t::world(0, 0);
      auto wall = t::body("static", 0, 0);
      push(wall, t::collider(t::aabb(5, -10, 6, 10), stone()));
      add(world, wall);
      return world;
    };
    auto unit_box = [] { return t::aabb(-0.5, -0.5, 0.5, 0.5); };

    runner.it("stops the sweep exactly where the shape first touches", [=] {
      auto world = walled_world();
      auto out = p::create_physics_2d_shape_cast_result();

      p::query_physics_2d_shape_cast(world, unit_box(), 10, 0, out);

      expect(out->hit).to_be(true);
      expect(out->fraction).to_be_close_to(0.45, 9);
      expect_true(out->body.has_value() && out->body.value() == world->bodies.element(0), "the wall");
      expect(out->collider_index).to_be(0.0);
      expect(out->normal_x).to_be_close_to(-1, 9);
      expect(out->normal_y).to_be_close_to(0, 9);
    });

    runner.it("reports a clean miss when the sweep is too short to reach anything", [=] {
      auto world = walled_world();
      auto out = p::create_physics_2d_shape_cast_result();

      p::query_physics_2d_shape_cast(world, unit_box(), 3, 0, out);

      expect(out->body.has_value()).to_be(false);
      expect(out->collider.has_value()).to_be(false);
      expect(out->collider_index).to_be(-1.0);
      expect(out->hit).to_be(false);
      expect(out->fraction).to_be(0.0);
      expect(out->x).to_be(0.0);
      expect(out->y).to_be(0.0);
      expect(out->normal_x).to_be(0.0);
      expect(out->normal_y).to_be(0.0);
    });

    runner.it("reports a hit at fraction zero for a shape that already overlaps where it starts", [=] {
      auto world = walled_world();
      auto out = p::create_physics_2d_shape_cast_result();

      p::query_physics_2d_shape_cast(world, t::aabb(5.2, -0.5, 5.8, 0.5), 10, 0, out);

      expect(out->hit).to_be(true);
      expect(out->fraction).to_be(0.0);
    });

    runner.it("agrees with a brute-force march of the discrete overlap test", [] {
      auto world = t::world(0, 0);
      for (auto [x, y] : std::vector<std::pair<double, double>>{{4, 0.2}, {7, -3}, {9, 0.1}}) {
        auto blocker = t::body("static", 0, 0);
        push(blocker, t::collider(t::circle(x, y, 0.6), stone()));
        add(world, blocker);
      }
      auto out = p::create_physics_2d_shape_cast_result();
      p::query_physics_2d_shape_cast(world, t::circle(0, 0, 0.3), 12, 0, out);

      constexpr int samples = 20000;
      double marched_fraction = -1;
      for (int sample = 0; sample <= samples && marched_fraction < 0; sample += 1) {
        const double fraction = static_cast<double>(sample) / samples;
        const double probe_x = 12 * fraction;
        auto found = p::create_physics_2d_query_result();
        p::query_physics_2d_region(world, t::view(region(probe_x - 0.3, -0.3, probe_x + 0.3, 0.3)), found);
        for (double at = 0; at < found->hit_count; at += 1) {
          const auto& shape = found->hits.element(at)->collider->world;
          const auto* circle = std::get_if<flight::Ref<types::x_y_radius_kind_90fbb3b4c4569518>>(&shape);
          if (circle == nullptr) continue;
          const double dx = (*circle)->x - probe_x;
          const double dy = (*circle)->y - 0;
          if (dx * dx + dy * dy <= ((*circle)->radius + 0.3) * ((*circle)->radius + 0.3)) marched_fraction = fraction;
        }
      }

      expect(marched_fraction).to_be_greater_than(0.0);
      expect(out->hit).to_be(true);
      expect(std::abs(out->fraction - marched_fraction)).to_be_less_than(2.0 / samples);
    });

    runner.it("breaks a tie between two equally distant colliders by body order, every time", [] {
      auto world = t::world(0, 0);
      for (double y : {-2.0, 2.0}) {
        auto wall = t::body("static", 0, 0);
        push(wall, t::collider(t::aabb(5, y - 1, 6, y + 1), stone()));
        add(world, wall);
      }
      auto tall = [] { return t::aabb(-0.5, -3, 0.5, 3); };
      auto out = p::create_physics_2d_shape_cast_result();

      p::query_physics_2d_shape_cast(world, tall(), 10, 0, out);
      auto first = out->body;
      for (int repeat = 0; repeat < 5; repeat += 1) {
        p::query_physics_2d_shape_cast(world, tall(), 10, 0, out);
        expect_true(out->body == first, "the same body every time");
      }
      expect_true(first.has_value() && first.value() == world->bodies.element(0), "the lower body index");
    });

    runner.it("honours maxFraction without changing what a fraction means", [=] {
      auto world = walled_world();
      auto out = p::create_physics_2d_shape_cast_result();

      p::query_physics_2d_shape_cast(world, unit_box(), 10, 0, out, 0.4);
      expect(out->hit).to_be(false);

      p::query_physics_2d_shape_cast(world, unit_box(), 10, 0, out, 0.5);
      expect(out->hit).to_be(true);
      expect(out->fraction).to_be_close_to(0.45, 9);
    });

    runner.it("applies the query filter to bodies and colliders alike", [=] {
      auto world = walled_world();
      auto out = p::create_physics_2d_shape_cast_result();
      auto filter = p::create_physics_2d_query_filter();

      filter->include_static = false;
      p::query_physics_2d_shape_cast(world, unit_box(), 10, 0, out, 1.0, t::view(filter));
      expect(out->hit).to_be(false);

      filter->include_static = true;
      filter->mask_bits = 0;
      p::query_physics_2d_shape_cast(world, unit_box(), 10, 0, out, 1.0, t::view(filter));
      expect(out->hit).to_be(false);
    });

    runner.it("clears a reused result rather than leaving the previous cast in it", [=] {
      auto world = walled_world();
      auto out = p::create_physics_2d_shape_cast_result();
      p::query_physics_2d_shape_cast(world, unit_box(), 10, 0, out);
      expect(out->hit).to_be(true);

      p::query_physics_2d_shape_cast(world, unit_box(), -10, 0, out);
      expect(out->hit).to_be(false);
      expect(out->body.has_value()).to_be(false);
      expect(out->collider.has_value()).to_be(false);
      expect(out->collider_index).to_be(-1.0);
    });

    runner.it("declines a non-finite displacement or a negative maxFraction instead of searching", [=] {
      auto world = walled_world();
      auto out = p::create_physics_2d_shape_cast_result();

      for (auto [dx, dy] : std::vector<std::pair<double, double>>{{std::nan(""), 0}, {0, INF}}) {
        p::query_physics_2d_shape_cast(world, unit_box(), dx, dy, out);
        expect(out->hit).to_be(false);
      }
      p::query_physics_2d_shape_cast(world, unit_box(), 10, 0, out, -1.0);
      expect(out->hit).to_be(false);
    });

    runner.it("sweeps backwards as readily as forwards, so the bounds union is not one-sided", [=] {
      auto world = t::world(0, 0);
      auto wall = t::body("static", 0, 0);
      push(wall, t::collider(t::aabb(-6, -10, -5, 10), stone()));
      add(world, wall);
      auto out = p::create_physics_2d_shape_cast_result();

      p::query_physics_2d_shape_cast(world, unit_box(), -10, 0, out);

      expect(out->hit).to_be(true);
      expect(out->fraction).to_be_close_to(0.45, 9);
      expect(out->normal_x).to_be_close_to(1, 9);
    });
  });
}

void joint_registry(flight::upstream_test::Runner& runner) {
  runner.describe("addPhysics2DJoint", [&] {
    runner.it("rejects duplicate and cross-world insertion without solving a shared accumulator twice", [] {
      auto first_world = default_world();
      auto second_world = default_world();
      auto first = registry_body(first_world, 0, 0);
      auto second = registry_body(first_world, 2, 0);
      auto added = joint("Unknown", first->index, second->index);

      p::add_physics_2d_joint(first_world, added);

      expect_throws([&] { p::add_physics_2d_joint(first_world, added); });
      expect_throws([&] { p::add_physics_2d_joint(second_world, added); });
      expect(static_cast<double>(first_world->joints.size())).to_be(1.0);
      expect_true(first_world->joints.element(0) == added, "the one joint");
      expect(static_cast<double>(second_world->joints.size())).to_be(0.0);
    });

    runner.it("wakes both bodies when an active constraint is added", [] {
      auto world = default_world();
      p::register_physics_2d_joint_solver(world, flight::String("Test"), solver());
      auto first = registry_body(world, 0, 0);
      auto second = registry_body(world, 2, 0);
      first->sleeping = true;
      first->sleep_timer = 5;
      second->sleeping = true;
      second->sleep_timer = 5;

      p::add_physics_2d_joint(world, joint("Test", first->index, second->index));

      expect(first->sleeping).to_be(false);
      expect(first->sleep_timer).to_be(0.0);
      expect(second->sleeping).to_be(false);
      expect(second->sleep_timer).to_be(0.0);
    });

    runner.it("orders the joint by body index, carrying its anchors with the swap", [] {
      auto world = default_world();
      p::register_physics_2d_joint_solver(world, flight::String("Test"), solver());
      auto first = registry_body(world, 0, 0);
      auto second = registry_body(world, 2, 0);
      auto added = p::add_physics_2d_joint(world, joint("Test", second->index, first->index, 7, 9));

      expect(added->body_a).to_be(first->index);
      expect(added->body_b).to_be(second->index);
      expect(added->local_anchor_ax).to_be(9.0);
      expect(added->local_anchor_bx).to_be(7.0);
    });

    runner.it("leaves a joint whose kind has no solver in its authored order", [] {
      auto world = default_world();
      auto first = registry_body(world, 0, 0);
      auto second = registry_body(world, 2, 0);

      auto added = p::add_physics_2d_joint(world, joint("Unknown", second->index, first->index, 7, 9));

      expect(added->body_a).to_be(second->index);
      expect(added->local_anchor_ax).to_be(7.0);
    });

    runner.it("leaves an unknown joint inert until registration, then wakes both ends", [] {
      auto world = default_world();
      auto first = registry_body(world, 0, 0);
      auto second = registry_body(world, 2, 0);
      first->sleeping = true;
      first->sleep_timer = 5;
      second->sleeping = true;
      second->sleep_timer = 5;

      p::add_physics_2d_joint(world, joint("Deferred", first->index, second->index));
      expect(first->sleeping).to_be(true);
      expect(second->sleeping).to_be(true);

      p::register_physics_2d_joint_solver(world, flight::String("Deferred"), solver());
      expect(first->sleeping).to_be(false);
      expect(first->sleep_timer).to_be(0.0);
      expect(second->sleeping).to_be(false);
      expect(second->sleep_timer).to_be(0.0);
    });

    runner.it("honours a solver that vetoes the swap when it registers later", [] {
      auto world = default_world();
      auto first = registry_body(world, 0, 0);
      auto second = registry_body(world, 2, 0);
      auto added = p::add_physics_2d_joint(world, joint("Vetoing", second->index, first->index, 7, 9));

      auto vetoing = solver();
      vetoing->swap_ends = [](flight::Ref<types::Physics2DJoint>) { return false; };
      p::register_physics_2d_joint_solver(world, flight::String("Vetoing"), vetoing);

      expect(added->body_a).to_be(second->index);
      expect(added->local_anchor_ax).to_be(7.0);
    });

    runner.it("leaves an already-ordered joint untouched", [] {
      auto world = default_world();
      auto first = registry_body(world, 0, 0);
      auto second = registry_body(world, 2, 0);
      auto added = p::add_physics_2d_joint(world, joint("Test", first->index, second->index, 7, 9));
      expect(added->body_a).to_be(first->index);
      expect(added->local_anchor_ax).to_be(7.0);
    });
  });

  runner.describe("getPhysics2DJointSolver", [&] {
    runner.it("returns null for an unregistered kind rather than throwing", [] {
      auto world = default_world();
      expect(p::get_physics_2d_joint_solver(t::view(world), flight::String("acme.Conveyor")).has_value()).to_be(false);
    });

    runner.it("steps a world holding a joint whose kind has no solver", [] {
      auto world = default_world();
      auto first = registry_body(world, 0, 0);
      auto second = registry_body(world, 2, 0);
      p::add_physics_2d_joint(world, joint("acme.Unknown", first->index, second->index));
      expect_no_throw([&] { p::step_physics_2d(world, 1.0 / 60); });
    });
  });

  runner.describe("invalidatePhysics2DJoint", [&] {
    runner.it("clears common and kind-owned impulses and wakes both participating bodies", [] {
      auto world = default_world();
      int cleared = 0;
      auto clearing = solver();
      clearing->clear_accumulated_impulses = [&cleared](flight::Ref<types::Physics2DJoint> value) {
        cleared++;
        value->impulse2 = 0;
      };
      p::register_physics_2d_joint_solver(world, flight::String("Test"), clearing);
      auto first = registry_body(world, 0, 0);
      auto second = registry_body(world, 2, 0);
      auto added = p::add_physics_2d_joint(world, joint("Test", first->index, second->index));
      added->impulse0 = 10;
      added->impulse1 = 20;
      added->impulse2 = 30;
      first->sleeping = true;
      first->sleep_timer = 5;
      second->sleeping = true;
      second->sleep_timer = 5;

      expect(p::invalidate_physics_2d_joint(world, added)).to_be(true);

      expect(cleared).to_be(1);
      expect(added->impulse0).to_be(0.0);
      expect(added->impulse1).to_be(0.0);
      expect(added->impulse2).to_be(0.0);
      expect(first->sleeping).to_be(false);
      expect(first->sleep_timer).to_be(0.0);
      expect(second->sleeping).to_be(false);
      expect(second->sleep_timer).to_be(0.0);
    });

    runner.it("reports false without mutating a joint owned by another world", [] {
      auto owner = default_world();
      auto other = default_world();
      auto first = registry_body(owner, 0, 0);
      auto second = registry_body(owner, 2, 0);
      auto added = p::add_physics_2d_joint(owner, joint("Unknown", first->index, second->index));
      added->impulse0 = 10;

      expect(p::invalidate_physics_2d_joint(other, added)).to_be(false);
      expect(added->impulse0).to_be(10.0);
    });

    runner.it("reindexes collision suppression after an authored flag changes", [] {
      auto world = default_world();
      p::register_physics_2d_joint_solver(world, flight::String("Test"), solver());
      auto first = registry_body(world, 0, 0);
      auto second = registry_body(world, 2, 0);
      auto added = p::add_physics_2d_joint(world, joint("Test", first->index, second->index));
      expect_true(suppression(world, first->index, second->index) == std::optional<double>(1.0), "suppressed once");

      added->collide_connected = true;
      expect(p::invalidate_physics_2d_joint(world, added)).to_be(true);

      expect(suppression(world, first->index, second->index).has_value()).to_be(false);
    });
  });

  runner.describe("registerPhysics2DJointSolver", [&] {
    runner.it("makes a custom kind solvable without this package knowing it", [] {
      auto world = t::world(0, 0);
      int prepared = 0;
      int solved = 0;
      p::register_physics_2d_joint_solver(world, flight::String("acme.Conveyor"),
                                         solver([&prepared] { prepared++; }, [&solved] { solved++; }));
      auto first = registry_body(world, 0, 0);
      auto second = registry_body(world, 2, 0);
      p::add_physics_2d_joint(world, joint("acme.Conveyor", first->index, second->index));
      p::step_physics_2d(world, 1.0 / 60);

      expect(prepared).to_be(1);
      expect(static_cast<double>(solved)).to_be(world->config->velocity_iterations);
    });

    runner.it("is scoped to its world, so two worlds can register different kinds", [] {
      auto first = default_world();
      auto second = default_world();
      p::register_physics_2d_joint_solver(first, flight::String("acme.One"), solver());
      expect(p::get_physics_2d_joint_solver(t::view(first), flight::String("acme.One")).has_value()).to_be(true);
      expect(p::get_physics_2d_joint_solver(t::view(second), flight::String("acme.One")).has_value()).to_be(false);
    });

    runner.it("lets a later registration replace an earlier one", [] {
      auto world = default_world();
      auto replacement = solver();
      p::register_physics_2d_joint_solver(world, flight::String("acme.One"), solver());
      p::register_physics_2d_joint_solver(world, flight::String("acme.One"), replacement);
      auto found = p::get_physics_2d_joint_solver(t::view(world), flight::String("acme.One"));
      expect_true(found.has_value() && found.value() == replacement, "the replacement");
    });
  });

  runner.describe("removePhysics2DJoint", [&] {
    runner.it("releases ownership so a detached joint may be inserted again", [] {
      auto first_world = default_world();
      auto second_world = default_world();
      auto first = registry_body(first_world, 0, 0);
      auto second = registry_body(first_world, 2, 0);
      auto added = p::add_physics_2d_joint(first_world, joint("Unknown", first->index, second->index));

      p::remove_physics_2d_joint(first_world, t::view(added));

      expect_true(p::add_physics_2d_joint(second_world, added) == added, "re-inserted");
    });

    runner.it("removes the joint and reports false for one the world does not hold", [] {
      auto world = default_world();
      auto first = registry_body(world, 0, 0);
      auto second = registry_body(world, 2, 0);
      auto added = p::add_physics_2d_joint(world, joint("Test", first->index, second->index));
      expect(p::remove_physics_2d_joint(world, t::view(added))).to_be(true);
      expect(static_cast<double>(world->joints.size())).to_be(0.0);
      expect(p::remove_physics_2d_joint(world, t::view(added))).to_be(false);
    });

    runner.it("drops joints naming a removed body, so a later body cannot inherit the constraint", [] {
      auto world = default_world();
      auto first = registry_body(world, 0, 0);
      auto second = registry_body(world, 2, 0);
      p::add_physics_2d_joint(world, joint("Test", first->index, second->index));
      p::remove_physics_2d_body(world, t::view(second));
      expect(static_cast<double>(world->joints.size())).to_be(0.0);
    });

    runner.it("wakes both ends when an active constraint is removed", [] {
      auto world = default_world();
      p::register_physics_2d_joint_solver(world, flight::String("Test"), solver());
      auto first = registry_body(world, 0, 0);
      auto second = registry_body(world, 2, 0);
      auto added = p::add_physics_2d_joint(world, joint("Test", first->index, second->index));
      first->sleeping = true;
      first->sleep_timer = 5;
      second->sleeping = true;
      second->sleep_timer = 5;

      p::remove_physics_2d_joint(world, t::view(added));

      expect(first->sleeping).to_be(false);
      expect(first->sleep_timer).to_be(0.0);
      expect(second->sleeping).to_be(false);
      expect(second->sleep_timer).to_be(0.0);
    });

    runner.it("keeps a pair suppressed until its final suppressing joint is removed", [] {
      auto world = default_world();
      p::register_physics_2d_joint_solver(world, flight::String("Test"), solver());
      auto first = registry_body(world, 0, 0);
      auto second = registry_body(world, 2, 0);
      auto one = p::add_physics_2d_joint(world, joint("Test", first->index, second->index));
      auto two = p::add_physics_2d_joint(world, joint("Test", first->index, second->index));
      expect_true(suppression(world, first->index, second->index) == std::optional<double>(2.0), "suppressed twice");

      p::remove_physics_2d_joint(world, t::view(one));
      expect_true(suppression(world, first->index, second->index) == std::optional<double>(1.0), "suppressed once");
      p::remove_physics_2d_joint(world, t::view(two));
      expect(suppression(world, first->index, second->index).has_value()).to_be(false);
    });

    runner.it("wakes the surviving end when its joint neighbour is removed from the world", [] {
      auto world = default_world();
      p::register_physics_2d_joint_solver(world, flight::String("Test"), solver());
      auto removed = registry_body(world, 0, 0);
      auto survivor = registry_body(world, 2, 0);
      p::add_physics_2d_joint(world, joint("Test", removed->index, survivor->index));
      survivor->sleeping = true;
      survivor->sleep_timer = 5;

      p::remove_physics_2d_body(world, t::view(removed));

      expect(survivor->sleeping).to_be(false);
      expect(survivor->sleep_timer).to_be(0.0);
    });
  });
}

void joint_reactions(flight::upstream_test::Runner& runner) {
  runner.describe("createPhysics2DJointReaction", [&] {
    runner.it("starts at rest", [] {
      auto reaction = p::create_physics_2d_joint_reaction();
      expect(reaction->force_x).to_be(0.0);
      expect(reaction->force_y).to_be(0.0);
      expect(reaction->torque).to_be(0.0);
    });

    runner.it("allocates a fresh object each call, so two readings do not alias", [] {
      auto first = p::create_physics_2d_joint_reaction();
      auto second = p::create_physics_2d_joint_reaction();
      first->force_y = 5;
      expect(second->force_y).to_be(0.0);
    });
  });

  runner.describe("writePhysics2DJointReaction", [&] {
    runner.it("declines for a kind whose solver was never registered", [] {
      constexpr double dt = 1.0 / 240;
      auto world = t::world(0, -10);
      auto ball = [&](const char* type, double x, double y) {
        auto made = t::body(type, x, y);
        push(made, t::collider(t::circle(0, 0, 0.4), t::material(1, 0, 0)));
        return add(world, made);
      };
      auto anchor = ball("static", 0, 0);
      auto bob = ball("dynamic", 0, -2);
      auto unregistered = joint("acme.Unregistered", anchor->index, bob->index);
      p::add_physics_2d_joint(world, unregistered);
      p::step_physics_2d(world, dt);

      auto out = p::create_physics_2d_joint_reaction();
      expect(p::write_physics_2d_joint_reaction(t::view(world), t::view(unregistered), dt, out)).to_be(false);
    });
  });
}

}  // namespace

int main() {
  flight::upstream_test::Runner runner;
  world_queries(runner);
  joint_registry(runner);
  joint_reactions(runner);
  return runner.run();
}
