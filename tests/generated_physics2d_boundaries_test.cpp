// Boundaries of the generated @flighthq/physics2d that the ported upstream cases leave unexercised, found by
// fixed-seed operator mutation of the emitted and overridden headers. Each case states the behaviour of the
// TypeScript source it checks. These are additions, not upstream ports; the ported cases live in
// generated_physics2d_{bodies,world,step,queries}_test.cpp.
#include <flight/physics2d/collider_transform.hpp>
#include <flight/physics2d/islands.hpp>
#include <flight/physics2d/mass_properties.hpp>
#include <flight/physics2d/step.hpp>
#include <flight/physics2d/step_validation.hpp>
#include <flight/physics2d/world_queries.hpp>

#include "physics2d_test_support.hpp"
#include "upstream_test_harness.hpp"

#include <cmath>
#include <limits>

namespace {

namespace t = flight::physics2d_test;
namespace p = flight::physics2d;
namespace types = flight::types;
using flight::upstream_test::expect;

void expect_true(bool condition, const char* what) {
  if (!condition) throw flight::upstream_test::AssertionFailure(what);
}

flight::String kind_of(const auto& variant) {
  return std::visit([](const auto& arm) { return arm->kind; }, variant);
}

void register_cases(flight::upstream_test::Runner& runner) {
  runner.describe("writePolygonMassData", [&] {
    runner.it("treats a triangle, the smallest polygon, as having area", [] {
      // count = points.length >> 1, and only count < 3 is degenerate.
      auto out = flight::make_ref<types::Physics2DMassData>();
      p::write_polygon_mass_data(flight::Array<double>{0.0, 0.0, 1.0, 0.0, 0.0, 1.0}, 2.0, out);
      expect(out->mass).to_be_close_to(1.0, 12);
      expect(out->center_x).to_be_close_to(1.0 / 3.0, 12);
      expect(out->center_y).to_be_close_to(1.0 / 3.0, 12);
      p::write_polygon_mass_data(flight::Array<double>{0.0, 0.0, 1.0, 0.0}, 2.0, out);
      expect(out->mass).to_be(0.0);
    });
  });

  runner.describe("createPhysics2DColliderWorldShape", [&] {
    runner.it("allocates a point world shape at the local point, and a segment at its endpoints", [] {
      auto point = p::create_physics2_dcollider_world_shape(t::point(2, 3));
      expect_true(kind_of(point) == flight::String("point"), "a point local shape stays a point");
      std::visit(
          [](const auto& arm) {
            if constexpr (requires { arm->x; arm->y; } && !requires { arm->radius; } && !requires { arm->half_w; }) {
              expect(arm->x).to_be(2.0);
              expect(arm->y).to_be(3.0);
            }
          },
          point);
      auto segment = p::create_physics2_dcollider_world_shape(t::segment(1, 2, 3, 4));
      expect_true(kind_of(segment) == flight::String("segment"), "a segment local shape stays a segment");
    });
  });

  runner.describe("writePhysics2DColliderBounds", [&] {
    runner.it("bounds an area-less point and segment by their own coordinates", [] {
      auto bounds = flight::make_ref<types::SpatialAabb2D>();
      p::write_physics2_dcollider_bounds(t::view(t::collider(t::point(2, 3))), bounds);
      expect(bounds->min_x).to_be(2.0);
      expect(bounds->max_x).to_be(2.0);
      expect(bounds->min_y).to_be(3.0);
      expect(bounds->max_y).to_be(3.0);
      p::write_physics2_dcollider_bounds(t::view(t::collider(t::segment(1, 2, 3, -1))), bounds);
      expect(bounds->min_x).to_be(1.0);
      expect(bounds->min_y).to_be(-1.0);
      expect(bounds->max_x).to_be(3.0);
      expect(bounds->max_y).to_be(2.0);
    });
    runner.it("zeroes the bounds of a world shape whose kind it does not know", [] {
      // The source's `default:` arm, reached by a shape kind outside the seven built-ins.
      auto shape = t::point(4, 5);
      std::get<flight::Ref<types::x_y_kind_35ad667397176b0f>>(shape)->kind = flight::String("vendor.unknown");
      auto unknown = t::collider(t::point(4, 5));
      unknown->world = shape;
      auto bounds = flight::make_ref<types::SpatialAabb2D>(types::SpatialAabb2D{.min_x = 7, .min_y = 7, .max_x = 7, .max_y = 7});
      p::write_physics2_dcollider_bounds(t::view(unknown), bounds);
      expect(bounds->min_x).to_be(0.0);
      expect(bounds->min_y).to_be(0.0);
      expect(bounds->max_x).to_be(0.0);
      expect(bounds->max_y).to_be(0.0);
    });
  });

  runner.describe("isCollisionShapeStateValid", [&] {
    runner.it("checks every oriented-box field for finiteness", [] {
      expect(p::is_collision_shape_state_valid(t::obb(0, 0, 1, 1, 0.5))).to_be(true);
      expect(p::is_collision_shape_state_valid(t::obb(std::nan(""), 0, 1, 1, 0))).to_be(false);
      expect(p::is_collision_shape_state_valid(t::obb(0, 0, 1, 1, std::numeric_limits<double>::infinity()))).to_be(false);
    });
  });

  runner.describe("physics2DSolveIslandForBody", [&] {
    runner.it("answers -1 for a missing body and for an awake dynamic body no island has been built for", [] {
      auto world = t::world();
      expect(p::physics2_dsolve_island_for_body(world, 99)).to_be(-1.0);
      auto body = p::add_physics2_dbody(world, t::body("dynamic", 0, 0));
      expect(p::physics2_dsolve_island_for_body(world, body->index)).to_be(-1.0);
    });
  });

  runner.describe("testPhysics2DSegmentOverlap", [&] {
    runner.it("reports a sensor oriented box crossing a solid segment", [] {
      auto world = t::world(0, 0);
      auto line = t::body("static", 0, 0);
      line->colliders.push(t::collider(t::segment(-2, 0, 2, 0)));
      p::add_physics2_dbody(world, line);
      auto box = t::body("static", 0, 0);
      box->colliders.push(t::collider(t::obb(0, 0, 0.5, 0.5, 0.3), t::material(), true));
      p::add_physics2_dbody(world, box);
      p::step_physics2_d(world, 1.0 / 60);
      expect(world->events->began.size()).to_be(static_cast<std::size_t>(1));
      expect(world->contacts.element(0)->sensor).to_be(true);
    });
  });

  runner.describe("queryPhysics2DRayClosest", [&] {
    runner.it("breaks a tie at one fraction by the lower body index, whatever the candidate order", [] {
      auto world = t::world(0, 0);
      auto first = t::body("static", 5, 0);
      first->colliders.push(t::collider(t::circle(0, 0, 1)));
      auto second = t::body("static", 5, 0);
      second->colliders.push(t::collider(t::circle(0, 0, 1)));
      p::add_physics2_dbody(world, first);
      p::add_physics2_dbody(world, second);
      auto out = p::create_physics2_dray_result();
      p::query_physics2_dray_closest(world, 0, 0, 1, 0, out);
      expect(out->hit_count).to_be(1.0);
      expect_true(out->hits.element(0)->body == first, "the lower body index wins the tie");
    });
  });
}

}  // namespace

int main() {
  flight::upstream_test::Runner runner;
  register_cases(runner);
  return runner.run();
}
