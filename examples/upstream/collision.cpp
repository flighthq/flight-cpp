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
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace {

using flight::examples::Canvas;
using flight::examples::Frame;
using flight::examples::Point;

constexpr std::uint32_t color_idle = 0x4488ccffU;
constexpr std::uint32_t color_colliding = 0xcc4444ffU;
constexpr std::uint32_t color_mtv = 0x44cc44ffU;

struct CircleCollider final {
  flight::Ref<flight::types::CollisionCircle2D> collider;
};

struct AabbCollider final {
  flight::Ref<flight::types::CollisionAabb2D> collider;
};

struct PolygonCollider final {
  flight::Ref<flight::types::CollisionPolygon2D> collider;
  double center_x{};
  double center_y{};
};

using Collider = std::variant<CircleCollider, AabbCollider, PolygonCollider>;

[[nodiscard]] CircleCollider circle(double x, double y, double radius) {
  return CircleCollider{.collider = flight::make_ref<flight::types::CollisionCircle2D>(
                            flight::types::CollisionCircle2D{
                                .x = x,
                                .y = y,
                                .radius = radius,
                            })};
}

[[nodiscard]] AabbCollider rectangle(
    double center_x,
    double center_y,
    double half_width,
    double half_height) {
  return AabbCollider{.collider = flight::make_ref<flight::types::CollisionAabb2D>(
                          flight::types::CollisionAabb2D{
                              .min_x = center_x - half_width,
                              .min_y = center_y - half_height,
                              .max_x = center_x + half_width,
                              .max_y = center_y + half_height,
                          })};
}

[[nodiscard]] PolygonCollider regular_polygon(
    double center_x,
    double center_y,
    double radius,
    int sides) {
  flight::Array<double> points;
  for (int index = 0; index < sides; ++index) {
    const double angle = -std::numbers::pi / 2.0 +
                         static_cast<double>(index) * 2.0 * std::numbers::pi /
                             static_cast<double>(sides);
    points.push(
        center_x + std::cos(angle) * radius,
        center_y + std::sin(angle) * radius);
  }
  return PolygonCollider{
      .collider = flight::make_ref<flight::types::CollisionPolygon2D>(
          flight::types::CollisionPolygon2D{.points = std::move(points)}),
      .center_x = center_x,
      .center_y = center_y,
  };
}

[[nodiscard]] Point collider_center(const Collider& collider) {
  return std::visit(
      [](const auto& value) -> Point {
        using Value = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<Value, CircleCollider>) {
          return {value.collider->x, value.collider->y};
        } else if constexpr (std::is_same_v<Value, AabbCollider>) {
          return {
              (value.collider->min_x + value.collider->max_x) * 0.5,
              (value.collider->min_y + value.collider->max_y) * 0.5,
          };
        } else {
          return {value.center_x, value.center_y};
        }
      },
      collider);
}

[[nodiscard]] bool test_pair_collision(
    const Collider& a,
    const Collider& b,
    const flight::Ref<flight::types::CollisionManifold2D>& out) {
  using namespace flight::collision;
  if (const auto* circle_a = std::get_if<CircleCollider>(&a)) {
    if (const auto* circle_b = std::get_if<CircleCollider>(&b)) {
      return test_circle_circle_collision2_d(circle_a->collider, circle_b->collider, out);
    }
    if (const auto* aabb_b = std::get_if<AabbCollider>(&b)) {
      return test_circle_aabb_collision2_d(circle_a->collider, aabb_b->collider, out);
    }
    return test_circle_polygon_collision2_d(
        circle_a->collider, std::get<PolygonCollider>(b).collider, out);
  }
  if (const auto* aabb_a = std::get_if<AabbCollider>(&a)) {
    if (const auto* circle_b = std::get_if<CircleCollider>(&b)) {
      const bool result = test_circle_aabb_collision2_d(circle_b->collider, aabb_a->collider, out);
      if (result) {
        out->normal_x = -out->normal_x;
        out->normal_y = -out->normal_y;
      }
      return result;
    }
    if (const auto* aabb_b = std::get_if<AabbCollider>(&b)) {
      return test_aabb_aabb_collision2_d(aabb_a->collider, aabb_b->collider, out);
    }
    return test_aabb_polygon_collision2_d(
        aabb_a->collider, std::get<PolygonCollider>(b).collider, out);
  }

  const auto& polygon_a = std::get<PolygonCollider>(a);
  if (const auto* circle_b = std::get_if<CircleCollider>(&b)) {
    const bool result = test_circle_polygon_collision2_d(
        circle_b->collider, polygon_a.collider, out);
    if (result) {
      out->normal_x = -out->normal_x;
      out->normal_y = -out->normal_y;
    }
    return result;
  }
  if (const auto* aabb_b = std::get_if<AabbCollider>(&b)) {
    const bool result = test_aabb_polygon_collision2_d(
        aabb_b->collider, polygon_a.collider, out);
    if (result) {
      out->normal_x = -out->normal_x;
      out->normal_y = -out->normal_y;
    }
    return result;
  }
  return test_polygon_polygon_collision2_d(
      polygon_a.collider, std::get<PolygonCollider>(b).collider, out);
}

[[nodiscard]] bool contains(const Collider& collider, Point point) {
  if (const auto* circle_value = std::get_if<CircleCollider>(&collider)) {
    const double dx = point.x - circle_value->collider->x;
    const double dy = point.y - circle_value->collider->y;
    return dx * dx + dy * dy <=
           circle_value->collider->radius * circle_value->collider->radius;
  }
  if (const auto* aabb = std::get_if<AabbCollider>(&collider)) {
    return point.x >= aabb->collider->min_x && point.x <= aabb->collider->max_x &&
           point.y >= aabb->collider->min_y && point.y <= aabb->collider->max_y;
  }
  const auto& points = std::get<PolygonCollider>(collider).collider->points;
  const std::size_t count = points.size() / 2;
  bool inside = true;
  for (std::size_t index = 0; index < count; ++index) {
    const std::size_t next = (index + 1) % count;
    const double edge_x = points[next * 2] - points[index * 2];
    const double edge_y = points[next * 2 + 1] - points[index * 2 + 1];
    const double target_x = point.x - points[index * 2];
    const double target_y = point.y - points[index * 2 + 1];
    if (edge_x * target_y - edge_y * target_x < 0.0) {
      inside = false;
      break;
    }
  }
  return inside;
}

void move(Collider& collider, Point delta) {
  std::visit(
      [&](auto& value) {
        using Value = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<Value, CircleCollider>) {
          value.collider->x += delta.x;
          value.collider->y += delta.y;
        } else if constexpr (std::is_same_v<Value, AabbCollider>) {
          value.collider->min_x += delta.x;
          value.collider->min_y += delta.y;
          value.collider->max_x += delta.x;
          value.collider->max_y += delta.y;
        } else {
          value.center_x += delta.x;
          value.center_y += delta.y;
          for (std::size_t index = 0; index < value.collider->points.size(); index += 2) {
            value.collider->points[index] += delta.x;
            value.collider->points[index + 1] += delta.y;
          }
        }
      },
      collider);
}

class CollisionExample final {
 public:
  CollisionExample()
      : colliders_{
            circle(210.0, 190.0, 58.0),
            circle(285.0, 190.0, 46.0),
            rectangle(500.0, 310.0, 70.0, 55.0),
            regular_polygon(545.0, 325.0, 60.0, 5),
        },
        manifold_(flight::collision::create_collision_manifold2_d()) {}

  void frame(const Frame& frame) {
    handle_drag(frame);
    std::vector<bool> colliding(colliders_.size());
    std::vector<ManifoldRecord> manifolds;
    std::size_t pair_tests{};
    for (std::size_t first = 0; first < colliders_.size(); ++first) {
      for (std::size_t second = first + 1; second < colliders_.size(); ++second) {
        ++pair_tests;
        if (test_pair_collision(colliders_[first], colliders_[second], manifold_)) {
          colliding[first] = true;
          colliding[second] = true;
          manifolds.push_back({
              collider_center(colliders_[first]),
              {manifold_->normal_x, manifold_->normal_y},
              manifold_->depth,
          });
        }
      }
    }
    if (frame.smoke) verify_smoke(pair_tests, colliding, manifolds);
    for (std::size_t index = 0; index < colliders_.size(); ++index) {
      draw_collider(frame.canvas, colliders_[index], colliding[index] ? color_colliding : color_idle);
    }
    for (const auto& record : manifolds) {
      draw_arrow(frame.canvas, record.origin, record.normal, record.depth * 2.0);
    }
  }

 private:
  struct ManifoldRecord final {
    Point origin;
    Point normal;
    double depth;
  };

  static void verify_smoke(
      std::size_t pair_tests,
      const std::vector<bool>& colliding,
      const std::vector<ManifoldRecord>& manifolds) {
    const std::size_t colliding_count = static_cast<std::size_t>(
        std::count(colliding.begin(), colliding.end(), true));
    if (pair_tests != 6 || colliding_count != 4 || manifolds.size() != 2 ||
        std::abs(manifolds.front().depth - 29.0) > 1e-9 ||
        std::abs(manifolds.front().normal.x + 1.0) > 1e-9 ||
        std::abs(manifolds.front().normal.y) > 1e-9) {
      throw std::runtime_error(
          "collision smoke did not observe the generated pair tests and manifold output");
    }
  }

  void handle_drag(const Frame& frame) {
    const Point pointer{frame.input.pointer_x, frame.input.pointer_y};
    if (frame.input.pointer_pressed) {
      dragged_ = colliders_.size();
      for (std::size_t offset = 0; offset < colliders_.size(); ++offset) {
        const std::size_t index = colliders_.size() - 1 - offset;
        if (contains(colliders_[index], pointer)) {
          dragged_ = index;
          const Point center = collider_center(colliders_[index]);
          drag_offset_ = {pointer.x - center.x, pointer.y - center.y};
          break;
        }
      }
    }
    if (frame.input.pointer_released) dragged_ = colliders_.size();
    if (dragged_ < colliders_.size() && frame.input.pointer_down) {
      const Point center = collider_center(colliders_[dragged_]);
      const Point target{pointer.x - drag_offset_.x, pointer.y - drag_offset_.y};
      move(colliders_[dragged_], {target.x - center.x, target.y - center.y});
    }
  }

  static void draw_collider(Canvas& canvas, const Collider& collider, std::uint32_t color) {
    if (const auto* circle_value = std::get_if<CircleCollider>(&collider)) {
      canvas.fill_circle(
          circle_value->collider->x,
          circle_value->collider->y,
          circle_value->collider->radius,
          color,
          0.6);
      canvas.stroke_circle(
          circle_value->collider->x,
          circle_value->collider->y,
          circle_value->collider->radius,
          color,
          2.0);
      return;
    }
    std::vector<Point> points;
    if (const auto* aabb = std::get_if<AabbCollider>(&collider)) {
      const double width = aabb->collider->max_x - aabb->collider->min_x;
      const double height = aabb->collider->max_y - aabb->collider->min_y;
      canvas.fill_rectangle(
          aabb->collider->min_x,
          aabb->collider->min_y,
          width,
          height,
          color,
          0.6);
      canvas.stroke_rectangle(
          aabb->collider->min_x,
          aabb->collider->min_y,
          width,
          height,
          color,
          2.0);
      return;
    } else {
      const auto& polygon = std::get<PolygonCollider>(collider).collider->points;
      points.reserve(polygon.size() / 2);
      for (std::size_t index = 0; index < polygon.size(); index += 2) {
        points.push_back({polygon[index], polygon[index + 1]});
      }
    }
    canvas.fill_polygon(points, color, 0.6);
    canvas.polyline(points, color, 2.0, true);
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
