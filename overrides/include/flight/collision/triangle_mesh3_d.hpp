// Derived from @flighthq/collision/packages/collision/src/triangleMesh3D.ts.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

#include <flight/collision/collide_contact_manifold3_d.hpp>
#include <flight/collision/collision_support3_d.hpp>
#include <flight/collision/contact_manifold3_d.hpp>
#include <flight/collision/sweep_collision_shape3_d.hpp>
#include <flight/entity/entity.hpp>
#include <flight/number.hpp>
#include <flight/runtime.hpp>
#include <flight/types/collision.hpp>
#include <flight/types/entity.hpp>
#include <flight/weak_map.hpp>

static_assert(flight::runtime_contract.compiler_contract == "flight-runtime-contract/2", "Flight compiler/runtime contract mismatch");
static_assert(flight::runtime_contract.cpp_abi == 1, "Flight C++ runtime ABI mismatch");

namespace flight::collision {

using TriangleAabb3D = flight::types::min_x_min_y_min_z_max_x_max_y_max_z_kind_748c675e8e0cf88a;
using TriangleConvex3D = flight::types::points_kind_35a8f5f2a3a8bd66;

struct CollisionTriangleMeshNode3D {
  double min_x;
  double min_y;
  double min_z;
  double max_x;
  double max_y;
  double max_z;
  std::size_t start;
  std::size_t count;
  int left;
  int right;
};

struct CollisionTriangleMeshAcceleration3D {
  flight::Array<double> points;
  flight::Array<double> indices;
  double version;
  std::size_t triangle_count;
  std::vector<CollisionTriangleMeshNode3D> nodes;
  std::vector<std::size_t> order;
};

struct CollisionHeightfieldMeshCache3D {
  flight::Array<double> heights;
  double columns;
  double rows;
  double cell_size_x;
  double cell_size_z;
  double version;
  flight::Ref<flight::types::CollisionTriangleMesh3D> mesh;
};

struct CollisionHeightfieldValidationCache3D {
  flight::Array<double> heights;
  double columns;
  double rows;
  double version;
  std::optional<flight::types::CollisionTestStatus> status;
};

struct CollisionTriangleMeshValidationCache3D {
  flight::Array<double> points;
  flight::Array<double> indices;
  double version;
  std::optional<flight::types::CollisionTestStatus> status;
};

struct CollisionBounds3D {
  double min_x;
  double min_y;
  double min_z;
  double max_x;
  double max_y;
  double max_z;
};

struct CollisionTriangleContactCandidate3D {
  double x;
  double y;
  double z;
  double depth;
  double feature_id;
};

inline constexpr double contact_depth_epsilon = 1e-7;
inline constexpr double contact_normal_alignment = 0.98;
inline constexpr double contact_point_epsilon_squared = 1e-12;
inline constexpr double quaternion_length_tolerance = 1e-6;
inline constexpr double ray_epsilon = 1e-12;
inline constexpr std::size_t triangle_bvh_leaf_size = 8U;

inline flight::WeakMap<
    flight::Ref<flight::types::CollisionHeightfield3D>,
    std::shared_ptr<CollisionHeightfieldMeshCache3D>>
    collision_heightfield_meshes3_d;
inline flight::WeakMap<
    flight::Ref<flight::types::CollisionHeightfield3D>,
    std::shared_ptr<CollisionHeightfieldValidationCache3D>>
    collision_heightfield_validations3_d;
inline flight::WeakMap<
    flight::Ref<flight::types::CollisionTriangleMesh3D>,
    std::shared_ptr<CollisionTriangleMeshAcceleration3D>>
    collision_triangle_mesh_accelerations3_d;
inline flight::WeakMap<
    flight::Ref<flight::types::CollisionTriangleMesh3D>,
    std::shared_ptr<CollisionTriangleMeshValidationCache3D>>
    collision_triangle_mesh_validations3_d;

inline void initialize_collision_heightfield3_d(
    flight::types::EntityConstruction<flight::Ref<flight::types::CollisionHeightfield3D>> out,
    double columns,
    double rows,
    flight::Array<double> heights,
    std::optional<double> cell_size_x = std::nullopt,
    std::optional<double> cell_size_z = std::nullopt) {
  flight::row_set<flight::RowKey<"kind">>(out, flight::String("heightfield"));
  flight::row_set<flight::RowKey<"columns">>(out, columns);
  flight::row_set<flight::RowKey<"rows">>(out, rows);
  flight::row_set<flight::RowKey<"heights">>(out, heights.slice());
  flight::row_set<flight::RowKey<"cellSizeX">>(out, cell_size_x.value_or(1.0));
  flight::row_set<flight::RowKey<"cellSizeZ">>(out, cell_size_z.value_or(1.0));
  flight::row_set<flight::RowKey<"version">>(out, 0.0);
  flight::row_set<flight::RowKey<"x">>(out, 0.0);
  flight::row_set<flight::RowKey<"y">>(out, 0.0);
  flight::row_set<flight::RowKey<"z">>(out, 0.0);
  flight::row_set<flight::RowKey<"rotationX">>(out, 0.0);
  flight::row_set<flight::RowKey<"rotationY">>(out, 0.0);
  flight::row_set<flight::RowKey<"rotationZ">>(out, 0.0);
  flight::row_set<flight::RowKey<"rotationW">>(out, 1.0);
}

inline flight::Ref<flight::types::CollisionHeightfield3D> create_collision_heightfield3_d(
    double columns,
    double rows,
    flight::Array<double> heights,
    std::optional<double> cell_size_x = std::nullopt,
    std::optional<double> cell_size_z = std::nullopt) {
  auto out = flight::entity::allocate_entity<flight::Ref<flight::types::CollisionHeightfield3D>>();
  initialize_collision_heightfield3_d(
      out, columns, rows, std::move(heights), cell_size_x, cell_size_z);
  return flight::entity::finish_entity<flight::Ref<flight::types::CollisionHeightfield3D>>(out);
}

inline void initialize_collision_triangle_mesh3_d(
    flight::types::EntityConstruction<flight::Ref<flight::types::CollisionTriangleMesh3D>> out,
    flight::Array<double> points,
    flight::Array<double> indices) {
  flight::row_set<flight::RowKey<"kind">>(out, flight::String("triangle-mesh"));
  flight::row_set<flight::RowKey<"points">>(out, points.slice());
  flight::row_set<flight::RowKey<"indices">>(out, indices.slice());
  flight::row_set<flight::RowKey<"version">>(out, 0.0);
  flight::row_set<flight::RowKey<"x">>(out, 0.0);
  flight::row_set<flight::RowKey<"y">>(out, 0.0);
  flight::row_set<flight::RowKey<"z">>(out, 0.0);
  flight::row_set<flight::RowKey<"rotationX">>(out, 0.0);
  flight::row_set<flight::RowKey<"rotationY">>(out, 0.0);
  flight::row_set<flight::RowKey<"rotationZ">>(out, 0.0);
  flight::row_set<flight::RowKey<"rotationW">>(out, 1.0);
}

inline flight::Ref<flight::types::CollisionTriangleMesh3D> create_collision_triangle_mesh3_d(
    flight::Array<double> points,
    flight::Array<double> indices) {
  auto out = flight::entity::allocate_entity<flight::Ref<flight::types::CollisionTriangleMesh3D>>();
  initialize_collision_triangle_mesh3_d(out, std::move(points), std::move(indices));
  return flight::entity::finish_entity<flight::Ref<flight::types::CollisionTriangleMesh3D>>(out);
}

inline void invalidate_collision_heightfield3_d(
    flight::Ref<flight::types::CollisionHeightfield3D> heightfield) {
  heightfield->version += 1.0;
}

inline void invalidate_collision_triangle_mesh3_d(
    flight::Ref<flight::types::CollisionTriangleMesh3D> mesh) {
  mesh->version += 1.0;
}

inline bool same_array_identity(const flight::Array<double>& a, const flight::Array<double>& b) {
  return a.identity() == b.identity();
}

template <typename Shape>
inline bool is_collision_triangle_surface_pose_valid3_d(const std::shared_ptr<Shape>& shape) {
  const double length_squared =
      shape->rotation_x * shape->rotation_x + shape->rotation_y * shape->rotation_y +
      shape->rotation_z * shape->rotation_z + shape->rotation_w * shape->rotation_w;
  return std::isfinite(shape->x) && std::isfinite(shape->y) && std::isfinite(shape->z) &&
         std::isfinite(length_squared) &&
         std::abs(length_squared - 1.0) <= quaternion_length_tolerance;
}

inline bool is_collision_triangle_mesh_triangle_valid3_d(
    const flight::Ref<flight::types::CollisionTriangleMesh3D>& mesh,
    std::size_t triangle) {
  const std::size_t offset = triangle * 3U;
  const std::size_t a = static_cast<std::size_t>(mesh->indices[offset]) * 3U;
  const std::size_t b = static_cast<std::size_t>(mesh->indices[offset + 1U]) * 3U;
  const std::size_t c = static_cast<std::size_t>(mesh->indices[offset + 2U]) * 3U;
  const double ab_x = mesh->points[b] - mesh->points[a];
  const double ab_y = mesh->points[b + 1U] - mesh->points[a + 1U];
  const double ab_z = mesh->points[b + 2U] - mesh->points[a + 2U];
  const double ac_x = mesh->points[c] - mesh->points[a];
  const double ac_y = mesh->points[c + 1U] - mesh->points[a + 1U];
  const double ac_z = mesh->points[c + 2U] - mesh->points[a + 2U];
  const double cross_x = ab_y * ac_z - ab_z * ac_y;
  const double cross_y = ab_z * ac_x - ab_x * ac_z;
  const double cross_z = ab_x * ac_y - ab_y * ac_x;
  const double cross_length_squared =
      cross_x * cross_x + cross_y * cross_y + cross_z * cross_z;
  const double ab_length_squared = ab_x * ab_x + ab_y * ab_y + ab_z * ab_z;
  const double ac_length_squared = ac_x * ac_x + ac_y * ac_y + ac_z * ac_z;
  const double scale_squared = std::max(ab_length_squared, ac_length_squared);
  const double epsilon = std::numeric_limits<double>::epsilon();
  return std::isfinite(cross_length_squared) && std::isfinite(scale_squared) &&
         cross_length_squared > epsilon * epsilon * scale_squared * scale_squared;
}

inline std::optional<flight::types::CollisionTestStatus>
get_collision_heightfield_validation_status3_d(
    const flight::Ref<flight::types::CollisionHeightfield3D>& heightfield) {
  if (!flight::is_safe_integer(heightfield->columns) ||
      !flight::is_safe_integer(heightfield->rows) || heightfield->columns < 2.0 ||
      heightfield->rows < 2.0 ||
      static_cast<double>(heightfield->heights.size()) !=
          heightfield->columns * heightfield->rows ||
      !std::isfinite(heightfield->cell_size_x) || !std::isfinite(heightfield->cell_size_z) ||
      heightfield->cell_size_x <= 0.0 || heightfield->cell_size_z <= 0.0 ||
      !flight::is_safe_integer(heightfield->version) || heightfield->version < 0.0 ||
      !is_collision_triangle_surface_pose_valid3_d(heightfield)) {
    return flight::String("degenerate-shape");
  }
  if (const auto cached = collision_heightfield_validations3_d.get(heightfield);
      cached && same_array_identity((*cached)->heights, heightfield->heights) &&
      (*cached)->columns == heightfield->columns && (*cached)->rows == heightfield->rows &&
      (*cached)->version == heightfield->version) {
    return (*cached)->status;
  }
  std::optional<flight::types::CollisionTestStatus> status;
  for (const double height : heightfield->heights) {
    if (!std::isfinite(height)) {
      status = flight::String("degenerate-shape");
      break;
    }
  }
  (void)collision_heightfield_validations3_d.set(
      heightfield,
      std::make_shared<CollisionHeightfieldValidationCache3D>(
          CollisionHeightfieldValidationCache3D{
              .heights = heightfield->heights,
              .columns = heightfield->columns,
              .rows = heightfield->rows,
              .version = heightfield->version,
              .status = status,
          }));
  return status;
}

inline std::optional<flight::types::CollisionTestStatus>
get_collision_triangle_mesh_validation_status3_d(
    const flight::Ref<flight::types::CollisionTriangleMesh3D>& mesh) {
  if (mesh->points.size() < 9U || mesh->points.size() % 3U != 0U ||
      mesh->indices.size() < 3U || mesh->indices.size() % 3U != 0U ||
      !flight::is_safe_integer(mesh->version) || mesh->version < 0.0 ||
      !is_collision_triangle_surface_pose_valid3_d(mesh)) {
    return flight::String("degenerate-shape");
  }
  if (const auto cached = collision_triangle_mesh_validations3_d.get(mesh);
      cached && same_array_identity((*cached)->points, mesh->points) &&
      same_array_identity((*cached)->indices, mesh->indices) &&
      (*cached)->version == mesh->version) {
    return (*cached)->status;
  }
  std::optional<flight::types::CollisionTestStatus> status;
  for (const double point : mesh->points) {
    if (!std::isfinite(point)) {
      status = flight::String("degenerate-shape");
      break;
    }
  }
  const double vertex_count = static_cast<double>(mesh->points.size() / 3U);
  if (!status) {
    for (const double index : mesh->indices) {
      if (!flight::is_safe_integer(index) || index < 0.0 || index >= vertex_count) {
        status = flight::String("degenerate-shape");
        break;
      }
    }
  }
  if (!status) {
    for (std::size_t triangle = 0; triangle < mesh->indices.size() / 3U; ++triangle) {
      if (!is_collision_triangle_mesh_triangle_valid3_d(mesh, triangle)) {
        status = flight::String("degenerate-shape");
        break;
      }
    }
  }
  (void)collision_triangle_mesh_validations3_d.set(
      mesh,
      std::make_shared<CollisionTriangleMeshValidationCache3D>(
          CollisionTriangleMeshValidationCache3D{
              .points = mesh->points,
              .indices = mesh->indices,
              .version = mesh->version,
              .status = status,
          }));
  return status;
}

inline double get_collision_triangle_mesh_triangle_centroid_axis3_d(
    const flight::Ref<flight::types::CollisionTriangleMesh3D>& mesh,
    std::size_t triangle,
    std::size_t axis) {
  const std::size_t offset = triangle * 3U;
  const std::size_t a = static_cast<std::size_t>(mesh->indices[offset]) * 3U + axis;
  const std::size_t b = static_cast<std::size_t>(mesh->indices[offset + 1U]) * 3U + axis;
  const std::size_t c = static_cast<std::size_t>(mesh->indices[offset + 2U]) * 3U + axis;
  return (mesh->points[a] + mesh->points[b] + mesh->points[c]) / 3.0;
}

inline void clear_collision_bounds3_d(flight::Ref<TriangleAabb3D> out) {
  out->min_x = 0.0;
  out->min_y = 0.0;
  out->min_z = 0.0;
  out->max_x = 0.0;
  out->max_y = 0.0;
  out->max_z = 0.0;
}

template <typename Target, typename Source>
inline void include_collision_bounds3_d(Target& target, const Source& source) {
  target.min_x = std::min(target.min_x, source.min_x);
  target.min_y = std::min(target.min_y, source.min_y);
  target.min_z = std::min(target.min_z, source.min_z);
  target.max_x = std::max(target.max_x, source.max_x);
  target.max_y = std::max(target.max_y, source.max_y);
  target.max_z = std::max(target.max_z, source.max_z);
}

template <typename Left, typename Right>
inline bool is_collision_bounds_overlap3_d(const Left& a, const Right& b) {
  return !(a.max_x < b.min_x || a.min_x > b.max_x || a.max_y < b.min_y ||
           a.min_y > b.max_y || a.max_z < b.min_z || a.min_z > b.max_z);
}

inline void write_collision_triangle_mesh_triangle_bounds3_d(
    const flight::Ref<flight::types::CollisionTriangleMesh3D>& mesh,
    std::size_t triangle,
    CollisionBounds3D& out) {
  const std::size_t offset = triangle * 3U;
  const std::size_t a = static_cast<std::size_t>(mesh->indices[offset]) * 3U;
  const std::size_t b = static_cast<std::size_t>(mesh->indices[offset + 1U]) * 3U;
  const std::size_t c = static_cast<std::size_t>(mesh->indices[offset + 2U]) * 3U;
  out.min_x = std::min({mesh->points[a], mesh->points[b], mesh->points[c]});
  out.min_y = std::min({mesh->points[a + 1U], mesh->points[b + 1U], mesh->points[c + 1U]});
  out.min_z = std::min({mesh->points[a + 2U], mesh->points[b + 2U], mesh->points[c + 2U]});
  out.max_x = std::max({mesh->points[a], mesh->points[b], mesh->points[c]});
  out.max_y = std::max({mesh->points[a + 1U], mesh->points[b + 1U], mesh->points[c + 1U]});
  out.max_z = std::max({mesh->points[a + 2U], mesh->points[b + 2U], mesh->points[c + 2U]});
}

inline std::size_t build_collision_triangle_mesh_node3_d(
    const flight::Ref<flight::types::CollisionTriangleMesh3D>& mesh,
    CollisionTriangleMeshAcceleration3D& acceleration,
    std::size_t start,
    std::size_t count) {
  const std::size_t node_index = acceleration.nodes.size();
  acceleration.nodes.push_back(CollisionTriangleMeshNode3D{
      .min_x = std::numeric_limits<double>::infinity(),
      .min_y = std::numeric_limits<double>::infinity(),
      .min_z = std::numeric_limits<double>::infinity(),
      .max_x = -std::numeric_limits<double>::infinity(),
      .max_y = -std::numeric_limits<double>::infinity(),
      .max_z = -std::numeric_limits<double>::infinity(),
      .start = start,
      .count = count,
      .left = -1,
      .right = -1,
  });
  CollisionBounds3D triangle_bounds{};
  for (std::size_t index = start; index < start + count; ++index) {
    write_collision_triangle_mesh_triangle_bounds3_d(
        mesh, acceleration.order[index], triangle_bounds);
    include_collision_bounds3_d(acceleration.nodes[node_index], triangle_bounds);
  }
  if (count <= triangle_bvh_leaf_size) return node_index;

  const auto& node = acceleration.nodes[node_index];
  const double extent_x = node.max_x - node.min_x;
  const double extent_y = node.max_y - node.min_y;
  const double extent_z = node.max_z - node.min_z;
  const std::size_t axis =
      extent_x >= extent_y && extent_x >= extent_z ? 0U : (extent_y >= extent_z ? 1U : 2U);
  std::sort(
      acceleration.order.begin() + static_cast<std::ptrdiff_t>(start),
      acceleration.order.begin() + static_cast<std::ptrdiff_t>(start + count),
      [&](std::size_t a, std::size_t b) {
        const double a_axis = get_collision_triangle_mesh_triangle_centroid_axis3_d(mesh, a, axis);
        const double b_axis = get_collision_triangle_mesh_triangle_centroid_axis3_d(mesh, b, axis);
        return a_axis != b_axis ? a_axis < b_axis : a < b;
      });
  const std::size_t left_count = count / 2U;
  const std::size_t left =
      build_collision_triangle_mesh_node3_d(mesh, acceleration, start, left_count);
  const std::size_t right = build_collision_triangle_mesh_node3_d(
      mesh, acceleration, start + left_count, count - left_count);
  acceleration.nodes[node_index].left = static_cast<int>(left);
  acceleration.nodes[node_index].right = static_cast<int>(right);
  acceleration.nodes[node_index].count = 0U;
  return node_index;
}

inline std::shared_ptr<CollisionTriangleMeshAcceleration3D>
build_collision_triangle_mesh_acceleration3_d(
    const flight::Ref<flight::types::CollisionTriangleMesh3D>& mesh) {
  const std::size_t triangle_count = mesh->indices.size() / 3U;
  auto acceleration = std::make_shared<CollisionTriangleMeshAcceleration3D>(
      CollisionTriangleMeshAcceleration3D{
          .points = mesh->points,
          .indices = mesh->indices,
          .version = mesh->version,
          .triangle_count = triangle_count,
          .nodes = {},
          .order = std::vector<std::size_t>(triangle_count),
      });
  for (std::size_t triangle = 0; triangle < triangle_count; ++triangle) {
    acceleration->order[triangle] = triangle;
  }
  build_collision_triangle_mesh_node3_d(mesh, *acceleration, 0U, triangle_count);
  return acceleration;
}

inline std::shared_ptr<CollisionTriangleMeshAcceleration3D>
get_collision_triangle_mesh_acceleration3_d(
    const flight::Ref<flight::types::CollisionTriangleMesh3D>& mesh) {
  auto acceleration = collision_triangle_mesh_accelerations3_d.get(mesh);
  if (!acceleration || !same_array_identity((*acceleration)->points, mesh->points) ||
      !same_array_identity((*acceleration)->indices, mesh->indices) ||
      (*acceleration)->version != mesh->version) {
    auto replacement = build_collision_triangle_mesh_acceleration3_d(mesh);
    (void)collision_triangle_mesh_accelerations3_d.set(mesh, replacement);
    return replacement;
  }
  return *acceleration;
}

inline flight::Ref<flight::types::CollisionTriangleMesh3D>
get_collision_heightfield_triangle_mesh3_d(
    const flight::Ref<flight::types::CollisionHeightfield3D>& heightfield) {
  auto cached = collision_heightfield_meshes3_d.get(heightfield);
  if (!cached || !same_array_identity((*cached)->heights, heightfield->heights) ||
      (*cached)->columns != heightfield->columns || (*cached)->rows != heightfield->rows ||
      (*cached)->cell_size_x != heightfield->cell_size_x ||
      (*cached)->cell_size_z != heightfield->cell_size_z ||
      (*cached)->version != heightfield->version) {
    flight::Array<double> points;
    const std::size_t columns = static_cast<std::size_t>(heightfield->columns);
    const std::size_t rows = static_cast<std::size_t>(heightfield->rows);
    for (std::size_t row = 0; row < rows; ++row) {
      for (std::size_t column = 0; column < columns; ++column) {
        const std::size_t source = row * columns + column;
        points.push(static_cast<double>(column) * heightfield->cell_size_x);
        points.push(heightfield->heights[source]);
        points.push(static_cast<double>(row) * heightfield->cell_size_z);
      }
    }
    flight::Array<double> indices;
    for (std::size_t row = 0; row + 1U < rows; ++row) {
      for (std::size_t column = 0; column + 1U < columns; ++column) {
        const std::size_t lower_left = row * columns + column;
        const std::size_t lower_right = lower_left + 1U;
        const std::size_t upper_left = lower_left + columns;
        const std::size_t upper_right = upper_left + 1U;
        indices.push(static_cast<double>(lower_left));
        indices.push(static_cast<double>(upper_right));
        indices.push(static_cast<double>(lower_right));
        indices.push(static_cast<double>(lower_left));
        indices.push(static_cast<double>(upper_left));
        indices.push(static_cast<double>(upper_right));
      }
    }
    auto replacement = std::make_shared<CollisionHeightfieldMeshCache3D>(
        CollisionHeightfieldMeshCache3D{
            .heights = heightfield->heights,
            .columns = heightfield->columns,
            .rows = heightfield->rows,
            .cell_size_x = heightfield->cell_size_x,
            .cell_size_z = heightfield->cell_size_z,
            .version = heightfield->version,
            .mesh = create_collision_triangle_mesh3_d(std::move(points), std::move(indices)),
        });
    (void)collision_heightfield_meshes3_d.set(heightfield, replacement);
    cached = replacement;
  }
  const auto mesh = (*cached)->mesh;
  mesh->x = heightfield->x;
  mesh->y = heightfield->y;
  mesh->z = heightfield->z;
  mesh->rotation_x = heightfield->rotation_x;
  mesh->rotation_y = heightfield->rotation_y;
  mesh->rotation_z = heightfield->rotation_z;
  mesh->rotation_w = heightfield->rotation_w;
  return mesh;
}

inline void rotate_collision_vector_by_quaternion3_d(
    double q_x,
    double q_y,
    double q_z,
    double q_w,
    double x,
    double y,
    double z,
    flight::Array<double> out) {
  const double t_x = 2.0 * (q_y * z - q_z * y);
  const double t_y = 2.0 * (q_z * x - q_x * z);
  const double t_z = 2.0 * (q_x * y - q_y * x);
  out.element(0.0) = x + q_w * t_x + q_y * t_z - q_z * t_y;
  out.element(1.0) = y + q_w * t_y + q_z * t_x - q_x * t_z;
  out.element(2.0) = z + q_w * t_z + q_x * t_y - q_y * t_x;
}

inline void write_collision_triangle_mesh_local_direction3_d(
    const flight::Ref<flight::types::CollisionTriangleMesh3D>& mesh,
    double x,
    double y,
    double z,
    flight::Array<double> out) {
  rotate_collision_vector_by_quaternion3_d(
      -mesh->rotation_x,
      -mesh->rotation_y,
      -mesh->rotation_z,
      mesh->rotation_w,
      x,
      y,
      z,
      out);
}

inline void write_collision_triangle_mesh_local_point3_d(
    const flight::Ref<flight::types::CollisionTriangleMesh3D>& mesh,
    double x,
    double y,
    double z,
    flight::Array<double> out) {
  write_collision_triangle_mesh_local_direction3_d(
      mesh, x - mesh->x, y - mesh->y, z - mesh->z, out);
}

inline void write_collision_triangle_mesh_world_direction3_d(
    const flight::Ref<flight::types::CollisionTriangleMesh3D>& mesh,
    double x,
    double y,
    double z,
    flight::Array<double> out) {
  rotate_collision_vector_by_quaternion3_d(
      mesh->rotation_x, mesh->rotation_y, mesh->rotation_z, mesh->rotation_w, x, y, z, out);
}

inline void write_collision_triangle_mesh_world_point3_d(
    const flight::Ref<flight::types::CollisionTriangleMesh3D>& mesh,
    double x,
    double y,
    double z,
    flight::Array<double> out) {
  write_collision_triangle_mesh_world_direction3_d(mesh, x, y, z, out);
  out.element(0.0) += mesh->x;
  out.element(1.0) += mesh->y;
  out.element(2.0) += mesh->z;
}

inline CollisionBounds3D scratch_bounds{0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
inline std::vector<CollisionTriangleContactCandidate3D> scratch_contact_candidates;
inline CollisionBounds3D scratch_local_bounds{0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
inline flight::Array<double> scratch_local_direction{0.0, 0.0, 0.0};
inline flight::Array<double> scratch_local_origin{0.0, 0.0, 0.0};
inline flight::Array<double> scratch_local_point{0.0, 0.0, 0.0};
inline flight::Ref<flight::types::CollisionContactManifold3D> scratch_manifold =
    create_collision_contact_manifold3_d();
inline std::vector<std::size_t> scratch_node_stack;
inline flight::Array<double> scratch_normal{0.0, 0.0, 0.0};
inline flight::Array<double> scratch_ray{0.0};
inline flight::Array<double> scratch_ray_bounds{0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
inline flight::Array<double> scratch_ray_directions{0.0, 0.0, 0.0};
inline flight::Array<double> scratch_ray_origins{0.0, 0.0, 0.0};
inline std::vector<std::size_t> scratch_selected_candidates;
inline flight::Array<double> scratch_support{0.0, 0.0, 0.0};
inline flight::Ref<flight::types::CollisionTimeOfImpact3D> scratch_time_of_impact =
    create_collision_time_of_impact3_d();
inline flight::Ref<TriangleConvex3D> scratch_triangle = flight::make_ref<TriangleConvex3D>(
    TriangleConvex3D{
        .points = flight::Array<double>{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
        .kind = flight::String("convex"),
    });
inline CollisionBounds3D scratch_triangle_bounds{0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
inline std::vector<std::size_t> scratch_triangles;
inline flight::Array<double> scratch_world_normal{0.0, 0.0, 0.0};
inline flight::Array<double> scratch_world_point{0.0, 0.0, 0.0};

inline void write_collision_triangle_mesh_triangle_normal_local3_d(
    const flight::Ref<flight::types::CollisionTriangleMesh3D>& mesh,
    std::size_t triangle,
    flight::Array<double> out) {
  const std::size_t offset = triangle * 3U;
  const std::size_t a = static_cast<std::size_t>(mesh->indices[offset]) * 3U;
  const std::size_t b = static_cast<std::size_t>(mesh->indices[offset + 1U]) * 3U;
  const std::size_t c = static_cast<std::size_t>(mesh->indices[offset + 2U]) * 3U;
  const double ab_x = mesh->points[b] - mesh->points[a];
  const double ab_y = mesh->points[b + 1U] - mesh->points[a + 1U];
  const double ab_z = mesh->points[b + 2U] - mesh->points[a + 2U];
  const double ac_x = mesh->points[c] - mesh->points[a];
  const double ac_y = mesh->points[c + 1U] - mesh->points[a + 1U];
  const double ac_z = mesh->points[c + 2U] - mesh->points[a + 2U];
  const double x = ab_y * ac_z - ab_z * ac_y;
  const double y = ab_z * ac_x - ab_x * ac_z;
  const double z = ab_x * ac_y - ab_y * ac_x;
  const double inverse_length = 1.0 / std::hypot(x, y, z);
  out.element(0.0) = x * inverse_length;
  out.element(1.0) = y * inverse_length;
  out.element(2.0) = z * inverse_length;
}

inline void write_collision_triangle_mesh_triangle_world3_d(
    const flight::Ref<flight::types::CollisionTriangleMesh3D>& mesh,
    std::size_t triangle,
    flight::Array<double> out) {
  const std::size_t offset = triangle * 3U;
  for (std::size_t vertex = 0; vertex < 3U; ++vertex) {
    const std::size_t source = static_cast<std::size_t>(mesh->indices[offset + vertex]) * 3U;
    write_collision_triangle_mesh_world_point3_d(
        mesh,
        mesh->points[source],
        mesh->points[source + 1U],
        mesh->points[source + 2U],
        scratch_world_point);
    const double target = static_cast<double>(vertex * 3U);
    out.element(target) = scratch_world_point.element(0.0);
    out.element(target + 1.0) = scratch_world_point.element(1.0);
    out.element(target + 2.0) = scratch_world_point.element(2.0);
  }
}

inline void write_world_bounds_in_collision_triangle_mesh_local3_d(
    const flight::Ref<flight::types::CollisionTriangleMesh3D>& mesh,
    const CollisionBounds3D& bounds,
    CollisionBounds3D& out) {
  out = CollisionBounds3D{
      .min_x = std::numeric_limits<double>::infinity(),
      .min_y = std::numeric_limits<double>::infinity(),
      .min_z = std::numeric_limits<double>::infinity(),
      .max_x = -std::numeric_limits<double>::infinity(),
      .max_y = -std::numeric_limits<double>::infinity(),
      .max_z = -std::numeric_limits<double>::infinity(),
  };
  for (std::size_t corner = 0; corner < 8U; ++corner) {
    write_collision_triangle_mesh_local_point3_d(
        mesh,
        (corner & 1U) == 0U ? bounds.min_x : bounds.max_x,
        (corner & 2U) == 0U ? bounds.min_y : bounds.max_y,
        (corner & 4U) == 0U ? bounds.min_z : bounds.max_z,
        scratch_local_point);
    out.min_x = std::min(out.min_x, scratch_local_point.element(0.0));
    out.min_y = std::min(out.min_y, scratch_local_point.element(1.0));
    out.min_z = std::min(out.min_z, scratch_local_point.element(2.0));
    out.max_x = std::max(out.max_x, scratch_local_point.element(0.0));
    out.max_y = std::max(out.max_y, scratch_local_point.element(1.0));
    out.max_z = std::max(out.max_z, scratch_local_point.element(2.0));
  }
}

inline void query_collision_triangle_mesh_candidates3_d(
    const flight::Ref<flight::types::CollisionTriangleMesh3D>& mesh,
    const CollisionBounds3D& bounds,
    std::vector<std::size_t>& out) {
  out.clear();
  const auto acceleration = get_collision_triangle_mesh_acceleration3_d(mesh);
  scratch_node_stack.clear();
  if (!acceleration->nodes.empty()) scratch_node_stack.push_back(0U);
  while (!scratch_node_stack.empty()) {
    const std::size_t node_index = scratch_node_stack.back();
    scratch_node_stack.pop_back();
    const auto& node = acceleration->nodes[node_index];
    if (!is_collision_bounds_overlap3_d(bounds, node)) continue;
    if (node.count > 0U) {
      for (std::size_t index = node.start; index < node.start + node.count; ++index) {
        out.push_back(acceleration->order[index]);
      }
      continue;
    }
    if (node.right >= 0) scratch_node_stack.push_back(static_cast<std::size_t>(node.right));
    if (node.left >= 0) scratch_node_stack.push_back(static_cast<std::size_t>(node.left));
  }
  std::sort(out.begin(), out.end());
}

inline bool raycast_collision_triangle_local3_d(
    const flight::Ref<flight::types::CollisionTriangleMesh3D>& mesh,
    std::size_t triangle,
    double origin_x,
    double origin_y,
    double origin_z,
    double direction_x,
    double direction_y,
    double direction_z,
    flight::Array<double> out,
    double max_fraction) {
  const std::size_t offset = triangle * 3U;
  const std::size_t a = static_cast<std::size_t>(mesh->indices[offset]) * 3U;
  const std::size_t b = static_cast<std::size_t>(mesh->indices[offset + 1U]) * 3U;
  const std::size_t c = static_cast<std::size_t>(mesh->indices[offset + 2U]) * 3U;
  const double edge1_x = mesh->points[b] - mesh->points[a];
  const double edge1_y = mesh->points[b + 1U] - mesh->points[a + 1U];
  const double edge1_z = mesh->points[b + 2U] - mesh->points[a + 2U];
  const double edge2_x = mesh->points[c] - mesh->points[a];
  const double edge2_y = mesh->points[c + 1U] - mesh->points[a + 1U];
  const double edge2_z = mesh->points[c + 2U] - mesh->points[a + 2U];
  const double p_x = direction_y * edge2_z - direction_z * edge2_y;
  const double p_y = direction_z * edge2_x - direction_x * edge2_z;
  const double p_z = direction_x * edge2_y - direction_y * edge2_x;
  const double determinant = edge1_x * p_x + edge1_y * p_y + edge1_z * p_z;
  if (std::abs(determinant) <= ray_epsilon) return false;
  const double inverse = 1.0 / determinant;
  const double t_x = origin_x - mesh->points[a];
  const double t_y = origin_y - mesh->points[a + 1U];
  const double t_z = origin_z - mesh->points[a + 2U];
  const double u = (t_x * p_x + t_y * p_y + t_z * p_z) * inverse;
  if (u < 0.0 || u > 1.0) return false;
  const double q_x = t_y * edge1_z - t_z * edge1_y;
  const double q_y = t_z * edge1_x - t_x * edge1_z;
  const double q_z = t_x * edge1_y - t_y * edge1_x;
  const double v = (direction_x * q_x + direction_y * q_y + direction_z * q_z) * inverse;
  if (v < 0.0 || u + v > 1.0) return false;
  const double fraction = (edge2_x * q_x + edge2_y * q_y + edge2_z * q_z) * inverse;
  if (fraction < 0.0 || fraction > max_fraction) return false;
  out.element(0.0) = fraction;
  return true;
}

template <typename Bounds>
inline bool raycast_collision_bounds_local3_d(
    const Bounds& bounds,
    double origin_x,
    double origin_y,
    double origin_z,
    double direction_x,
    double direction_y,
    double direction_z,
    double max_fraction) {
  double minimum = 0.0;
  double maximum = max_fraction;
  scratch_ray_origins.element(0.0) = origin_x;
  scratch_ray_origins.element(1.0) = origin_y;
  scratch_ray_origins.element(2.0) = origin_z;
  scratch_ray_directions.element(0.0) = direction_x;
  scratch_ray_directions.element(1.0) = direction_y;
  scratch_ray_directions.element(2.0) = direction_z;
  scratch_ray_bounds.element(0.0) = bounds.min_x;
  scratch_ray_bounds.element(1.0) = bounds.min_y;
  scratch_ray_bounds.element(2.0) = bounds.min_z;
  scratch_ray_bounds.element(3.0) = bounds.max_x;
  scratch_ray_bounds.element(4.0) = bounds.max_y;
  scratch_ray_bounds.element(5.0) = bounds.max_z;
  for (std::size_t axis = 0; axis < 3U; ++axis) {
    const double index = static_cast<double>(axis);
    const double origin = scratch_ray_origins.element(index);
    const double direction = scratch_ray_directions.element(index);
    const double lower = scratch_ray_bounds.element(index);
    const double upper = scratch_ray_bounds.element(index + 3.0);
    if (std::abs(direction) <= ray_epsilon) {
      if (origin < lower || origin > upper) return false;
      continue;
    }
    double first = (lower - origin) / direction;
    double last = (upper - origin) / direction;
    if (first > last) std::swap(first, last);
    minimum = std::max(minimum, first);
    maximum = std::min(maximum, last);
    if (minimum > maximum) return false;
  }
  return maximum >= 0.0;
}

inline void write_collision_shape_bounds3_d(
    const flight::types::CollisionShape3D& shape,
    const flight::types::CollisionSupport3D& support,
    CollisionBounds3D& out) {
  support(shape, -1.0, 0.0, 0.0, scratch_support);
  out.min_x = scratch_support.element(0.0);
  support(shape, 1.0, 0.0, 0.0, scratch_support);
  out.max_x = scratch_support.element(0.0);
  support(shape, 0.0, -1.0, 0.0, scratch_support);
  out.min_y = scratch_support.element(1.0);
  support(shape, 0.0, 1.0, 0.0, scratch_support);
  out.max_y = scratch_support.element(1.0);
  support(shape, 0.0, 0.0, -1.0, scratch_support);
  out.min_z = scratch_support.element(2.0);
  support(shape, 0.0, 0.0, 1.0, scratch_support);
  out.max_z = scratch_support.element(2.0);
}

inline bool raycast_collision_triangle_mesh3_d(
    const flight::Ref<flight::types::CollisionTriangleMesh3D>& mesh,
    double origin_x,
    double origin_y,
    double origin_z,
    double direction_x,
    double direction_y,
    double direction_z,
    flight::Ref<flight::types::CollisionRaycastHit3D> out,
    std::optional<double> max_fraction = std::nullopt) {
  const double limit = max_fraction.value_or(std::numeric_limits<double>::infinity());
  if (get_collision_triangle_mesh_validation_status3_d(mesh) || limit < 0.0) return false;
  write_collision_triangle_mesh_local_point3_d(
      mesh, origin_x, origin_y, origin_z, scratch_local_origin);
  write_collision_triangle_mesh_local_direction3_d(
      mesh, direction_x, direction_y, direction_z, scratch_local_direction);
  const auto cache = get_collision_triangle_mesh_acceleration3_d(mesh);
  double fraction = limit;
  int triangle_hit = -1;
  scratch_node_stack.clear();
  if (!cache->nodes.empty()) scratch_node_stack.push_back(0U);
  while (!scratch_node_stack.empty()) {
    const std::size_t node_index = scratch_node_stack.back();
    scratch_node_stack.pop_back();
    const auto& node = cache->nodes[node_index];
    if (!raycast_collision_bounds_local3_d(
            node,
            scratch_local_origin.element(0.0),
            scratch_local_origin.element(1.0),
            scratch_local_origin.element(2.0),
            scratch_local_direction.element(0.0),
            scratch_local_direction.element(1.0),
            scratch_local_direction.element(2.0),
            fraction)) {
      continue;
    }
    if (node.count == 0U) {
      if (node.right >= 0) scratch_node_stack.push_back(static_cast<std::size_t>(node.right));
      if (node.left >= 0) scratch_node_stack.push_back(static_cast<std::size_t>(node.left));
      continue;
    }
    for (std::size_t index = node.start; index < node.start + node.count; ++index) {
      const std::size_t triangle = cache->order[index];
      if (!raycast_collision_triangle_local3_d(
              mesh,
              triangle,
              scratch_local_origin.element(0.0),
              scratch_local_origin.element(1.0),
              scratch_local_origin.element(2.0),
              scratch_local_direction.element(0.0),
              scratch_local_direction.element(1.0),
              scratch_local_direction.element(2.0),
              scratch_ray,
              fraction)) {
        continue;
      }
      fraction = scratch_ray.element(0.0);
      triangle_hit = static_cast<int>(triangle);
    }
  }
  if (triangle_hit < 0) return false;
  write_collision_triangle_mesh_triangle_normal_local3_d(
      mesh, static_cast<std::size_t>(triangle_hit), scratch_normal);
  if (scratch_normal.element(0.0) * scratch_local_direction.element(0.0) +
          scratch_normal.element(1.0) * scratch_local_direction.element(1.0) +
          scratch_normal.element(2.0) * scratch_local_direction.element(2.0) >
      0.0) {
    scratch_normal.element(0.0) = -scratch_normal.element(0.0);
    scratch_normal.element(1.0) = -scratch_normal.element(1.0);
    scratch_normal.element(2.0) = -scratch_normal.element(2.0);
  }
  write_collision_triangle_mesh_world_direction3_d(
      mesh,
      scratch_normal.element(0.0),
      scratch_normal.element(1.0),
      scratch_normal.element(2.0),
      scratch_world_normal);
  out->fraction = fraction;
  out->x = origin_x + direction_x * fraction;
  out->y = origin_y + direction_y * fraction;
  out->z = origin_z + direction_z * fraction;
  out->normal_x = scratch_world_normal.element(0.0);
  out->normal_y = scratch_world_normal.element(1.0);
  out->normal_z = scratch_world_normal.element(2.0);
  return true;
}

inline bool raycast_collision_heightfield3_d(
    const flight::Ref<flight::types::CollisionHeightfield3D>& heightfield,
    double origin_x,
    double origin_y,
    double origin_z,
    double direction_x,
    double direction_y,
    double direction_z,
    flight::Ref<flight::types::CollisionRaycastHit3D> out,
    std::optional<double> max_fraction = std::nullopt) {
  if (get_collision_heightfield_validation_status3_d(heightfield)) return false;
  return raycast_collision_triangle_mesh3_d(
      get_collision_heightfield_triangle_mesh3_d(heightfield),
      origin_x,
      origin_y,
      origin_z,
      direction_x,
      direction_y,
      direction_z,
      std::move(out),
      max_fraction);
}

inline void write_collision_triangle_mesh_bounds3_d(
    const flight::Ref<flight::types::CollisionTriangleMesh3D>& mesh,
    flight::Ref<TriangleAabb3D> out) {
  if (get_collision_triangle_mesh_validation_status3_d(mesh)) {
    clear_collision_bounds3_d(std::move(out));
    return;
  }
  const auto acceleration = get_collision_triangle_mesh_acceleration3_d(mesh);
  const auto& root = acceleration->nodes[0];
  out->min_x = std::numeric_limits<double>::infinity();
  out->min_y = std::numeric_limits<double>::infinity();
  out->min_z = std::numeric_limits<double>::infinity();
  out->max_x = -std::numeric_limits<double>::infinity();
  out->max_y = -std::numeric_limits<double>::infinity();
  out->max_z = -std::numeric_limits<double>::infinity();
  for (std::size_t corner = 0; corner < 8U; ++corner) {
    write_collision_triangle_mesh_world_point3_d(
        mesh,
        (corner & 1U) == 0U ? root.min_x : root.max_x,
        (corner & 2U) == 0U ? root.min_y : root.max_y,
        (corner & 4U) == 0U ? root.min_z : root.max_z,
        scratch_world_point);
    out->min_x = std::min(out->min_x, scratch_world_point.element(0.0));
    out->min_y = std::min(out->min_y, scratch_world_point.element(1.0));
    out->min_z = std::min(out->min_z, scratch_world_point.element(2.0));
    out->max_x = std::max(out->max_x, scratch_world_point.element(0.0));
    out->max_y = std::max(out->max_y, scratch_world_point.element(1.0));
    out->max_z = std::max(out->max_z, scratch_world_point.element(2.0));
  }
}

inline void write_collision_heightfield_bounds3_d(
    const flight::Ref<flight::types::CollisionHeightfield3D>& heightfield,
    flight::Ref<TriangleAabb3D> out) {
  if (get_collision_heightfield_validation_status3_d(heightfield)) {
    clear_collision_bounds3_d(std::move(out));
    return;
  }
  write_collision_triangle_mesh_bounds3_d(
      get_collision_heightfield_triangle_mesh3_d(heightfield), std::move(out));
}

inline void append_collision_triangle_contact_candidate3_d(
    double x,
    double y,
    double z,
    double depth,
    double feature_id) {
  for (auto& existing : scratch_contact_candidates) {
    const double dx = existing.x - x;
    const double dy = existing.y - y;
    const double dz = existing.z - z;
    if (dx * dx + dy * dy + dz * dz <= contact_point_epsilon_squared) {
      if (depth > existing.depth) existing.depth = depth;
      if (feature_id < existing.feature_id) existing.feature_id = feature_id;
      return;
    }
  }
  scratch_contact_candidates.push_back(CollisionTriangleContactCandidate3D{
      .x = x,
      .y = y,
      .z = z,
      .depth = depth,
      .feature_id = feature_id,
  });
}

inline double get_collision_contact_manifold_depth3_d(
    const flight::Ref<flight::types::CollisionContactManifold3D>& manifold) {
  double depth = 0.0;
  for (double index = 0.0; index < manifold->point_count; index += 1.0) {
    depth = std::max(depth, manifold->points.element(index)->depth);
  }
  return depth;
}

inline void write_reduced_collision_triangle_contact_candidates3_d(
    flight::Ref<flight::types::CollisionContactManifold3D> out) {
  if (scratch_contact_candidates.empty()) {
    out->point_count = 0.0;
    return;
  }
  scratch_selected_candidates.clear();
  std::size_t first = 0U;
  for (std::size_t index = 1U; index < scratch_contact_candidates.size(); ++index) {
    const auto& candidate = scratch_contact_candidates[index];
    const auto& current = scratch_contact_candidates[first];
    if (candidate.x < current.x ||
        (candidate.x == current.x && candidate.y < current.y) ||
        (candidate.x == current.x && candidate.y == current.y && candidate.z < current.z)) {
      first = index;
    }
  }
  scratch_selected_candidates.push_back(first);
  while (scratch_selected_candidates.size() <
         std::min<std::size_t>(4U, scratch_contact_candidates.size())) {
    std::optional<std::size_t> best;
    double best_distance = -1.0;
    for (std::size_t index = 0; index < scratch_contact_candidates.size(); ++index) {
      if (std::find(
              scratch_selected_candidates.begin(), scratch_selected_candidates.end(), index) !=
          scratch_selected_candidates.end()) {
        continue;
      }
      double minimum_distance = std::numeric_limits<double>::infinity();
      for (const std::size_t selected : scratch_selected_candidates) {
        const double dx =
            scratch_contact_candidates[index].x - scratch_contact_candidates[selected].x;
        const double dy =
            scratch_contact_candidates[index].y - scratch_contact_candidates[selected].y;
        const double dz =
            scratch_contact_candidates[index].z - scratch_contact_candidates[selected].z;
        minimum_distance = std::min(minimum_distance, dx * dx + dy * dy + dz * dz);
      }
      if (minimum_distance > best_distance) {
        best_distance = minimum_distance;
        best = index;
      }
    }
    if (!best) break;
    scratch_selected_candidates.push_back(*best);
  }
  out->point_count = static_cast<double>(scratch_selected_candidates.size());
  for (std::size_t index = 0; index < scratch_selected_candidates.size(); ++index) {
    const auto& source = scratch_contact_candidates[scratch_selected_candidates[index]];
    auto target = out->points.element(static_cast<double>(index));
    target->x = source.x;
    target->y = source.y;
    target->z = source.z;
    target->depth = source.depth;
    target->feature_id = source.feature_id;
  }
}

inline bool collide_collision_triangle_mesh3_d(
    flight::types::CollisionShape3D convex,
    const flight::Ref<flight::types::CollisionTriangleMesh3D>& mesh,
    flight::Ref<flight::types::CollisionContactManifold3D> out) {
  clear_collision_contact_manifold3_d(out);
  const auto kind = std::visit([](const auto& value) { return value->kind; }, convex);
  const auto support = get_collision_support3_d(kind);
  if (!support || get_collision_triangle_mesh_validation_status3_d(mesh)) return false;
  write_collision_shape_bounds3_d(convex, *support, scratch_bounds);
  write_world_bounds_in_collision_triangle_mesh_local3_d(mesh, scratch_bounds, scratch_local_bounds);
  query_collision_triangle_mesh_candidates3_d(mesh, scratch_local_bounds, scratch_triangles);

  scratch_contact_candidates.clear();
  double best_depth = -std::numeric_limits<double>::infinity();
  double best_normal_x = 0.0;
  double best_normal_y = 0.0;
  double best_normal_z = 0.0;
  for (const std::size_t triangle : scratch_triangles) {
    write_collision_triangle_mesh_triangle_world3_d(mesh, triangle, scratch_triangle->points);
    const flight::types::CollisionShape3D triangle_shape(scratch_triangle);
    if (!collide_contact_manifold3_d(convex, triangle_shape, scratch_manifold)) continue;
    const double depth = get_collision_contact_manifold_depth3_d(scratch_manifold);
    const double alignment = scratch_manifold->normal_x * best_normal_x +
                             scratch_manifold->normal_y * best_normal_y +
                             scratch_manifold->normal_z * best_normal_z;
    if (best_depth == -std::numeric_limits<double>::infinity() ||
        (depth > best_depth + contact_depth_epsilon &&
         alignment < contact_normal_alignment)) {
      best_depth = depth;
      best_normal_x = scratch_manifold->normal_x;
      best_normal_y = scratch_manifold->normal_y;
      best_normal_z = scratch_manifold->normal_z;
      scratch_contact_candidates.clear();
    }
    const double selected_alignment = scratch_manifold->normal_x * best_normal_x +
                                      scratch_manifold->normal_y * best_normal_y +
                                      scratch_manifold->normal_z * best_normal_z;
    if (selected_alignment < contact_normal_alignment) continue;
    if (depth > best_depth) best_depth = depth;
    for (double index = 0.0; index < scratch_manifold->point_count; index += 1.0) {
      const auto point = scratch_manifold->points.element(index);
      append_collision_triangle_contact_candidate3_d(
          point->x,
          point->y,
          point->z,
          point->depth,
          static_cast<double>(triangle) * 4.0 + index);
    }
  }
  if (best_depth == -std::numeric_limits<double>::infinity()) return false;
  out->overlapping = true;
  out->normal_x = best_normal_x;
  out->normal_y = best_normal_y;
  out->normal_z = best_normal_z;
  write_reduced_collision_triangle_contact_candidates3_d(out);
  return true;
}

inline bool collide_collision_heightfield3_d(
    flight::types::CollisionShape3D convex,
    const flight::Ref<flight::types::CollisionHeightfield3D>& heightfield,
    flight::Ref<flight::types::CollisionContactManifold3D> out) {
  if (get_collision_heightfield_validation_status3_d(heightfield)) {
    clear_collision_contact_manifold3_d(out);
    return false;
  }
  return collide_collision_triangle_mesh3_d(
      std::move(convex), get_collision_heightfield_triangle_mesh3_d(heightfield), std::move(out));
}

inline bool sweep_collision_triangle_mesh3_d(
    flight::types::CollisionShape3D convex,
    double delta_x,
    double delta_y,
    double delta_z,
    const flight::Ref<flight::types::CollisionTriangleMesh3D>& mesh,
    flight::Ref<flight::types::CollisionTimeOfImpact3D> out,
    std::optional<double> max_fraction = std::nullopt) {
  const double limit = max_fraction.value_or(1.0);
  const auto kind = std::visit([](const auto& value) { return value->kind; }, convex);
  const auto support = get_collision_support3_d(kind);
  if (!support || get_collision_triangle_mesh_validation_status3_d(mesh)) return false;
  write_collision_shape_bounds3_d(convex, *support, scratch_bounds);
  scratch_bounds.min_x = std::min(scratch_bounds.min_x, scratch_bounds.min_x + delta_x * limit);
  scratch_bounds.min_y = std::min(scratch_bounds.min_y, scratch_bounds.min_y + delta_y * limit);
  scratch_bounds.min_z = std::min(scratch_bounds.min_z, scratch_bounds.min_z + delta_z * limit);
  scratch_bounds.max_x = std::max(scratch_bounds.max_x, scratch_bounds.max_x + delta_x * limit);
  scratch_bounds.max_y = std::max(scratch_bounds.max_y, scratch_bounds.max_y + delta_y * limit);
  scratch_bounds.max_z = std::max(scratch_bounds.max_z, scratch_bounds.max_z + delta_z * limit);
  write_world_bounds_in_collision_triangle_mesh_local3_d(mesh, scratch_bounds, scratch_local_bounds);
  query_collision_triangle_mesh_candidates3_d(mesh, scratch_local_bounds, scratch_triangles);
  double fraction = limit;
  bool hit = false;
  for (const std::size_t triangle : scratch_triangles) {
    write_collision_triangle_mesh_triangle_world3_d(mesh, triangle, scratch_triangle->points);
    const flight::types::CollisionShape3D triangle_shape(scratch_triangle);
    if (!sweep_collision_shape3_d(
            convex,
            delta_x,
            delta_y,
            delta_z,
            triangle_shape,
            0.0,
            0.0,
            0.0,
            scratch_time_of_impact,
            fraction)) {
      continue;
    }
    fraction = scratch_time_of_impact->fraction;
    out->fraction = scratch_time_of_impact->fraction;
    out->x = scratch_time_of_impact->x;
    out->y = scratch_time_of_impact->y;
    out->z = scratch_time_of_impact->z;
    out->normal_x = scratch_time_of_impact->normal_x;
    out->normal_y = scratch_time_of_impact->normal_y;
    out->normal_z = scratch_time_of_impact->normal_z;
    hit = true;
  }
  return hit;
}

inline bool sweep_collision_heightfield3_d(
    flight::types::CollisionShape3D convex,
    double delta_x,
    double delta_y,
    double delta_z,
    const flight::Ref<flight::types::CollisionHeightfield3D>& heightfield,
    flight::Ref<flight::types::CollisionTimeOfImpact3D> out,
    std::optional<double> max_fraction = std::nullopt) {
  if (get_collision_heightfield_validation_status3_d(heightfield)) return false;
  return sweep_collision_triangle_mesh3_d(
      std::move(convex),
      delta_x,
      delta_y,
      delta_z,
      get_collision_heightfield_triangle_mesh3_d(heightfield),
      std::move(out),
      max_fraction);
}

} // namespace flight::collision
