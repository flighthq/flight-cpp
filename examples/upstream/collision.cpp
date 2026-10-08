// Native SDL port of @flighthq/example-collision.
// Source: .dependencies/flight/examples/packages/collision/src/app.ts

#include "sdl_host.hpp"

#include <flight/collision/manifold2_d.hpp>
#include <flight/collision/shape_collision2_d.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <numbers>
#include <vector>

namespace {

using flight::examples::Canvas;
using flight::examples::Frame;
using flight::examples::Point;

constexpr std::uint32_t color_idle = 0x4488ccffU;
constexpr std::uint32_t color_colliding = 0xcc4444ffU;
constexpr std::uint32_t color_mtv = 0x44cc44ffU;

enum class Kind { circle, aabb, polygon };

struct Collider final {
  Kind kind{};
  Point center{};
  double radius{};
  std::vector<Point> points;
};

[[nodiscard]] std::vector<Point> regular_polygon(Point center, double radius, int sides) {
  std::vector<Point> points;
  points.reserve(static_cast<std::size_t>(sides));
  for (int index = 0; index < sides; ++index) {
    const double angle = -std::numbers::pi / 2.0 +
                         static_cast<double>(index) * 2.0 * std::numbers::pi /
                             static_cast<double>(sides);
    points.push_back({center.x + std::cos(angle) * radius, center.y + std::sin(angle) * radius});
  }
  return points;
}

[[nodiscard]] Collider rectangle(Point center, double half_width, double half_height) {
  return {
      .kind = Kind::aabb,
      .center = center,
      .radius = 0.0,
      .points = {
          {center.x - half_width, center.y - half_height},
          {center.x + half_width, center.y - half_height},
          {center.x + half_width, center.y + half_height},
          {center.x - half_width, center.y + half_height},
      },
  };
}

[[nodiscard]] flight::Ref<flight::types::CollisionCircle2D> as_circle(const Collider& collider) {
  return flight::make_ref<flight::types::CollisionCircle2D>(flight::types::CollisionCircle2D{
      .x = collider.center.x,
      .y = collider.center.y,
      .radius = collider.radius,
  });
}

[[nodiscard]] flight::Ref<flight::types::CollisionAabb2D> as_aabb(const Collider& collider) {
  return flight::make_ref<flight::types::CollisionAabb2D>(flight::types::CollisionAabb2D{
      .min_x = collider.points[0].x,
      .min_y = collider.points[0].y,
      .max_x = collider.points[2].x,
      .max_y = collider.points[2].y,
  });
}

[[nodiscard]] flight::Ref<flight::types::CollisionPolygon2D> as_polygon(const Collider& collider) {
  flight::Array<double> points;
  for (const auto& point : collider.points) points.push(point.x, point.y);
  return flight::make_ref<flight::types::CollisionPolygon2D>(
      flight::types::CollisionPolygon2D{.points = std::move(points)});
}

[[nodiscard]] bool test_pair_collision(
    const Collider& a,
    const Collider& b,
    const flight::Ref<flight::types::CollisionManifold2D>& out) {
  using namespace flight::collision;
  if (a.kind == Kind::circle && b.kind == Kind::circle) {
    return test_circle_circle_collision2_d(as_circle(a), as_circle(b), out);
  }
  if (a.kind == Kind::circle && b.kind == Kind::aabb) {
    return test_circle_aabb_collision2_d(as_circle(a), as_aabb(b), out);
  }
  if (a.kind == Kind::aabb && b.kind == Kind::circle) {
    const bool result = test_circle_aabb_collision2_d(as_circle(b), as_aabb(a), out);
    if (result) {
      out->normal_x = -out->normal_x;
      out->normal_y = -out->normal_y;
    }
    return result;
  }
  if (a.kind == Kind::aabb && b.kind == Kind::aabb) {
    return test_aabb_aabb_collision2_d(as_aabb(a), as_aabb(b), out);
  }
  if (a.kind == Kind::circle && b.kind == Kind::polygon) {
    return test_circle_polygon_collision2_d(as_circle(a), as_polygon(b), out);
  }
  if (a.kind == Kind::polygon && b.kind == Kind::circle) {
    const bool result = test_circle_polygon_collision2_d(as_circle(b), as_polygon(a), out);
    if (result) {
      out->normal_x = -out->normal_x;
      out->normal_y = -out->normal_y;
    }
    return result;
  }
  if (a.kind == Kind::polygon && b.kind == Kind::polygon) {
    return test_polygon_polygon_collision2_d(as_polygon(a), as_polygon(b), out);
  }
  if (a.kind == Kind::aabb && b.kind == Kind::polygon) {
    return test_aabb_polygon_collision2_d(as_aabb(a), as_polygon(b), out);
  }
  if (a.kind == Kind::polygon && b.kind == Kind::aabb) {
    const bool result = test_aabb_polygon_collision2_d(as_aabb(b), as_polygon(a), out);
    if (result) {
      out->normal_x = -out->normal_x;
      out->normal_y = -out->normal_y;
    }
    return result;
  }
  return false;
}

[[nodiscard]] bool contains(const Collider& collider, Point point) {
  if (collider.kind == Kind::circle) {
    return std::hypot(point.x - collider.center.x, point.y - collider.center.y) <= collider.radius;
  }
  if (collider.kind == Kind::aabb) {
    return point.x >= collider.points[0].x && point.x <= collider.points[2].x &&
           point.y >= collider.points[0].y && point.y <= collider.points[2].y;
  }
  bool inside = false;
  for (std::size_t index = 0, previous = collider.points.size() - 1;
       index < collider.points.size();
       previous = index++) {
    const Point a = collider.points[index];
    const Point b = collider.points[previous];
    if (((a.y > point.y) != (b.y > point.y)) &&
        point.x < (b.x - a.x) * (point.y - a.y) / (b.y - a.y) + a.x) {
      inside = !inside;
    }
  }
  return inside;
}

void move(Collider& collider, Point delta) {
  collider.center.x += delta.x;
  collider.center.y += delta.y;
  for (auto& point : collider.points) {
    point.x += delta.x;
    point.y += delta.y;
  }
}

class CollisionExample final {
 public:
  CollisionExample()
      : colliders_{
            Collider{.kind = Kind::circle, .center = {210.0, 190.0}, .radius = 58.0, .points = {}},
            Collider{.kind = Kind::circle, .center = {285.0, 190.0}, .radius = 46.0, .points = {}},
            rectangle({500.0, 310.0}, 70.0, 55.0),
            Collider{.kind = Kind::polygon, .center = {545.0, 325.0}, .radius = 0.0, .points = regular_polygon({545.0, 325.0}, 60.0, 5)},
        },
        manifold_(flight::collision::create_collision_manifold2_d()) {}

  void frame(const Frame& frame) {
    handle_drag(frame);
    std::vector<bool> colliding(colliders_.size());
    struct Arrow final {
      Point origin;
      Point normal;
      double depth;
    };
    std::vector<Arrow> arrows;
    for (std::size_t first = 0; first < colliders_.size(); ++first) {
      for (std::size_t second = first + 1; second < colliders_.size(); ++second) {
        if (test_pair_collision(colliders_[first], colliders_[second], manifold_)) {
          colliding[first] = true;
          colliding[second] = true;
          arrows.push_back({
              colliders_[first].center,
              {manifold_->normal_x, manifold_->normal_y},
              manifold_->depth * 2.0,
          });
        }
      }
    }
    for (std::size_t index = 0; index < colliders_.size(); ++index) {
      draw_collider(frame.canvas, colliders_[index], colliding[index] ? color_colliding : color_idle);
    }
    for (const auto& arrow : arrows) draw_arrow(frame.canvas, arrow.origin, arrow.normal, arrow.depth);
  }

 private:
  void handle_drag(const Frame& frame) {
    const Point pointer{frame.input.pointer_x, frame.input.pointer_y};
    if (frame.input.pointer_pressed) {
      dragged_ = colliders_.size();
      for (std::size_t offset = 0; offset < colliders_.size(); ++offset) {
        const std::size_t index = colliders_.size() - 1 - offset;
        if (contains(colliders_[index], pointer)) {
          dragged_ = index;
          drag_offset_ = {
              pointer.x - colliders_[index].center.x,
              pointer.y - colliders_[index].center.y,
          };
          break;
        }
      }
    }
    if (frame.input.pointer_released) dragged_ = colliders_.size();
    if (dragged_ < colliders_.size() && frame.input.pointer_down) {
      const Point target{pointer.x - drag_offset_.x, pointer.y - drag_offset_.y};
      move(colliders_[dragged_], {
          target.x - colliders_[dragged_].center.x,
          target.y - colliders_[dragged_].center.y,
      });
    }
  }

  static void draw_collider(Canvas& canvas, const Collider& collider, std::uint32_t color) {
    if (collider.kind == Kind::circle) {
      canvas.fill_circle(collider.center.x, collider.center.y, collider.radius, color, 0.6);
      canvas.stroke_circle(collider.center.x, collider.center.y, collider.radius, color, 2.0);
    } else {
      canvas.fill_polygon(collider.points, color, 0.6);
      canvas.polyline(collider.points, color, 2.0, true);
    }
  }

  static void draw_arrow(Canvas& canvas, Point origin, Point normal, double depth) {
    const Point end{origin.x + normal.x * depth, origin.y + normal.y * depth};
    canvas.line(origin.x, origin.y, end.x, end.y, color_mtv, 3.0);
    const Point perpendicular{-normal.y, normal.x};
    canvas.line(
        end.x - normal.x * 8.0 + perpendicular.x * 4.0,
        end.y - normal.y * 8.0 + perpendicular.y * 4.0,
        end.x,
        end.y,
        color_mtv,
        3.0);
    canvas.line(
        end.x,
        end.y,
        end.x - normal.x * 8.0 - perpendicular.x * 4.0,
        end.y - normal.y * 8.0 - perpendicular.y * 4.0,
        color_mtv,
        3.0);
  }

  std::vector<Collider> colliders_;
  flight::Ref<flight::types::CollisionManifold2D> manifold_;
  std::size_t dragged_{colliders_.size()};
  Point drag_offset_{};
};

} // namespace

int main(int argc, char** argv) {
  try {
    CollisionExample example;
    return flight::examples::SdlExample::run(
        argc,
        argv,
        "Flight collision - SDL",
        [&](const Frame& frame) { example.frame(frame); });
  } catch (const std::exception& error) {
    std::cerr << "collision example failed: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
