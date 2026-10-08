// Naive C++ port of @flighthq/example-collision
// Ported from .dependencies/flight/examples/packages/collision/src/app.ts

#include <flight/collision/collide_contact_manifold2_d.hpp>
#include <flight/node/hierarchy.hpp>
#include <flight/node/node_transform2d.hpp>
#include <flight/scene2d/display_object.hpp>
#include <flight/shape/shape.hpp>
#include <flight/shape/shape_commands.hpp>

#include <array>
#include <cmath>
#include <iostream>
#include <variant>
#include <vector>

namespace {

constexpr std::uint32_t COLOR_IDLE = 0x4488ccff;
constexpr std::uint32_t COLOR_COLLIDING = 0xcc4444ff;
constexpr std::uint32_t COLOR_MTV = 0x44cc44ff;

struct CollisionCircle2D {
  double x, y, radius;
};

struct CollisionAabb2D {
  double minX, minY, maxX, maxY;
};

struct CollisionPolygon2D {
  std::vector<double> points;
};

struct CircleCollider {
  CollisionCircle2D collider;
};

struct AabbCollider {
  CollisionAabb2D collider;
};

struct PolygonCollider {
  CollisionPolygon2D collider;
  double centerX, centerY;
};

using Collider = std::variant<CircleCollider, AabbCollider, PolygonCollider>;

std::vector<double> make_regular_polygon_points(double cx, double cy, double radius, int sides) {
  std::vector<double> points;
  double angleStep = (M_PI * 2.0) / sides;
  double startAngle = -M_PI / 2.0;
  for (int i = 0; i < sides; ++i) {
    double angle = startAngle + angleStep * i;
    points.push_back(cx + std::cos(angle) * radius);
    points.push_back(cy + std::sin(angle) * radius);
  }
  return points;
}

} // namespace

int main() {
  using namespace flight::node;
  using namespace flight::shape;
  using namespace flight::scene2d;

  auto root = create_display_object(std::nullopt);

  std::vector<Collider> colliders;
  colliders.push_back(CircleCollider{{210.0, 190.0, 58.0}});
  colliders.push_back(CircleCollider{{285.0, 190.0, 46.0}});
  colliders.push_back(AabbCollider{{{500.0 - 70.0, 310.0 - 55.0, 500.0 + 70.0, 310.0 + 55.0}}});
  colliders.push_back(PolygonCollider{
    {make_regular_polygon_points(545.0, 325.0, 60.0, 5)},
    545.0, 325.0
  });

  for (auto& c : colliders) {
    auto shape = create_shape(std::nullopt);

    std::visit([&](auto& col) {
      using T = std::decay_t<decltype(col)>;
      if constexpr (std::is_same_v<T, CircleCollider>) {
        append_shape_begin_fill(shape, COLOR_IDLE);
        append_shape_circle(shape, col.collider.x, col.collider.y, col.collider.radius);
        append_shape_end_fill(shape);
      } else if constexpr (std::is_same_v<T, AabbCollider>) {
        append_shape_begin_fill(shape, COLOR_IDLE);
        append_shape_rectangle(shape,
          col.collider.minX, col.collider.minY,
          col.collider.maxX - col.collider.minX,
          col.collider.maxY - col.collider.minY);
        append_shape_end_fill(shape);
      } else if constexpr (std::is_same_v<T, PolygonCollider>) {
        append_shape_begin_fill(shape, COLOR_IDLE);
        flight::Array<double> pts(col.collider.points.size());
        for (std::size_t i = 0; i < col.collider.points.size(); ++i) {
          pts.set(i, col.collider.points[i]);
        }
        append_shape_polygon(shape, pts);
        append_shape_end_fill(shape);
      }
    }, c);

    add_node_child(root, shape);
  }

  auto manifold = flight::collision::create_collision_manifold_2_d();

  flight::collision::test_circle_circle_collision_2_d(
    manifold,
    std::get<CircleCollider>(colliders[0]).collider.x,
    std::get<CircleCollider>(colliders[0]).collider.y,
    std::get<CircleCollider>(colliders[0]).collider.radius,
    std::get<CircleCollider>(colliders[1]).collider.x,
    std::get<CircleCollider>(colliders[1]).collider.y,
    std::get<CircleCollider>(colliders[1]).collider.radius
  );

  bool colliding = manifold->hasCollision;
  std::cout << "Flight collision example (naive C++ port): "
            << colliders.size() << " colliders, "
            << "circle-circle collision = " << (colliding ? "true" : "false") << "\n";
  return 0;
}
