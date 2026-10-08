// Native SDL port of @flighthq/example-collision.
// Source: .dependencies/flight/examples/packages/collision/src/app.ts

#include "sdl_host.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <limits>
#include <numbers>
#include <utility>
#include <vector>

namespace {

using flight::examples::Canvas;
using flight::examples::Frame;
using flight::examples::Point;

constexpr std::uint32_t color_idle = 0x4488ccffU;
constexpr std::uint32_t color_colliding = 0xcc4444ffU;
constexpr std::uint32_t color_mtv = 0x44cc44ffU;

enum class Kind { circle, polygon };

struct Collider final {
  Kind kind{};
  Point center{};
  double radius{};
  std::vector<Point> points;
};

struct Manifold final {
  Point normal{};
  double depth{};
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
      .kind = Kind::polygon,
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

[[nodiscard]] Point normalized(Point point) {
  const double length = std::hypot(point.x, point.y);
  return length == 0.0 ? Point{1.0, 0.0} : Point{point.x / length, point.y / length};
}

[[nodiscard]] std::pair<double, double> project(const Collider& collider, Point axis) {
  if (collider.kind == Kind::circle) {
    const double center = collider.center.x * axis.x + collider.center.y * axis.y;
    return {center - collider.radius, center + collider.radius};
  }
  double minimum = std::numeric_limits<double>::infinity();
  double maximum = -std::numeric_limits<double>::infinity();
  for (const auto& point : collider.points) {
    const double value = point.x * axis.x + point.y * axis.y;
    minimum = std::min(minimum, value);
    maximum = std::max(maximum, value);
  }
  return {minimum, maximum};
}

void append_axes(const Collider& collider, std::vector<Point>& axes) {
  if (collider.kind == Kind::circle) return;
  for (std::size_t index = 0; index < collider.points.size(); ++index) {
    const Point from = collider.points[index];
    const Point to = collider.points[(index + 1) % collider.points.size()];
    axes.push_back(normalized({-(to.y - from.y), to.x - from.x}));
  }
}

[[nodiscard]] bool collide(const Collider& a, const Collider& b, Manifold& manifold) {
  if (a.kind == Kind::circle && b.kind == Kind::circle) {
    const Point delta{b.center.x - a.center.x, b.center.y - a.center.y};
    const double distance = std::hypot(delta.x, delta.y);
    const double depth = a.radius + b.radius - distance;
    if (depth <= 0.0) return false;
    manifold = {.normal = normalized(delta), .depth = depth};
    return true;
  }

  std::vector<Point> axes;
  append_axes(a, axes);
  append_axes(b, axes);
  const Collider* circle = a.kind == Kind::circle ? &a : b.kind == Kind::circle ? &b : nullptr;
  const Collider* polygon = a.kind == Kind::polygon ? &a : &b;
  if (circle != nullptr) {
    const auto closest = std::ranges::min_element(
        polygon->points,
        {},
        [&](Point point) {
          return std::hypot(point.x - circle->center.x, point.y - circle->center.y);
        });
    axes.push_back(normalized({closest->x - circle->center.x, closest->y - circle->center.y}));
  }

  double minimum_overlap = std::numeric_limits<double>::infinity();
  Point minimum_axis{};
  for (Point axis : axes) {
    const auto [a_minimum, a_maximum] = project(a, axis);
    const auto [b_minimum, b_maximum] = project(b, axis);
    const double overlap = std::min(a_maximum, b_maximum) - std::max(a_minimum, b_minimum);
    if (overlap <= 0.0) return false;
    if (overlap < minimum_overlap) {
      minimum_overlap = overlap;
      minimum_axis = axis;
    }
  }
  const Point direction{b.center.x - a.center.x, b.center.y - a.center.y};
  if (direction.x * minimum_axis.x + direction.y * minimum_axis.y < 0.0) {
    minimum_axis.x = -minimum_axis.x;
    minimum_axis.y = -minimum_axis.y;
  }
  manifold = {.normal = minimum_axis, .depth = minimum_overlap};
  return true;
}

[[nodiscard]] bool contains(const Collider& collider, Point point) {
  if (collider.kind == Kind::circle) {
    return std::hypot(point.x - collider.center.x, point.y - collider.center.y) <= collider.radius;
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
        } {}

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
        Manifold manifold;
        if (collide(colliders_[first], colliders_[second], manifold)) {
          colliding[first] = true;
          colliding[second] = true;
          arrows.push_back({colliders_[first].center, manifold.normal, manifold.depth * 2.0});
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
