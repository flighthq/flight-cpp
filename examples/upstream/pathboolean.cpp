// Naive C++ port of @flighthq/example-pathboolean
// Ported from .dependencies/flight/examples/packages/pathboolean/src/app.ts

#include <flight/node/hierarchy.hpp>
#include <flight/node/node_transform2d.hpp>
#include <flight/path/path.hpp>
#include <flight/path_boolean/boolean_paths.hpp>
#include <flight/scene2d/display_object.hpp>
#include <flight/shape/shape.hpp>
#include <flight/shape/shape_commands.hpp>

#include <array>
#include <iostream>
#include <string_view>

namespace {

constexpr double CELL_W = 400.0;
constexpr double CELL_H = 300.0;

constexpr std::array<std::uint32_t, 4> FILL_COLORS = {
  0x2980b9ff, 0x27ae60ff, 0xc0392bff, 0x8e44adff,
};

constexpr std::array<std::string_view, 4> LABELS = {
  "Union", "Intersect", "Difference (A - B)", "XOR",
};

} // namespace

int main() {
  using namespace flight::node;
  using namespace flight::shape;
  using namespace flight::scene2d;
  using namespace flight::path;
  using namespace flight::path_boolean;

  // NOT AVAILABLE: generated Scene2D construction/hierarchy is partial at this SDK pin.
  auto root = create_display_object(std::nullopt);

  auto pathA = create_path();
  append_path_rounded_rectangle(pathA, -80.0, -60.0, 120.0, 120.0, 16.0);

  auto pathB = create_path();
  append_path_circle(pathB, 0.0, 0.0, 70.0);

  double shapeAOffsetX = -40.0;
  double shapeBOffsetX = 40.0;

  for (int i = 0; i < 4; ++i) {
    int col = i % 2;
    int row = i / 2;
    double cellX = col * CELL_W;
    double cellY = row * CELL_H;
    double cx = cellX + CELL_W * 0.5;
    double cy = cellY + CELL_H * 0.5;

    auto resultShape = create_shape(std::nullopt);

    auto resultPath = [&]() {
      switch (i) {
        case 0: return union_paths(pathA, pathB, martinez_path_boolean_kernel);
        case 1: return intersect_paths(pathA, pathB, martinez_path_boolean_kernel);
        case 2: return difference_paths(pathA, pathB, martinez_path_boolean_kernel);
        case 3: return xor_paths(pathA, pathB, martinez_path_boolean_kernel);
        default: return create_path();
      }
    }();

    append_shape_begin_fill(resultShape, FILL_COLORS[i]);
    append_shape_path(resultShape, resultPath);
    append_shape_end_fill(resultShape);

    resultShape->x = cx;
    resultShape->y = cy;
    invalidate_node_local_transform(resultShape);
    add_node_child(root, resultShape);

    auto outlineA = create_shape(std::nullopt);
    append_shape_line_style(outlineA, 2.0, 0x34495eff);
    append_shape_path(outlineA, pathA);
    outlineA->x = cx + shapeAOffsetX;
    outlineA->y = cy;
    invalidate_node_local_transform(outlineA);
    add_node_child(root, outlineA);

    auto outlineB = create_shape(std::nullopt);
    append_shape_line_style(outlineB, 2.0, 0xe67e22ff);
    append_shape_path(outlineB, pathB);
    outlineB->x = cx + shapeBOffsetX;
    outlineB->y = cy;
    invalidate_node_local_transform(outlineB);
    add_node_child(root, outlineB);
  }

  std::cout << "Flight pathboolean example (naive C++ port): "
            << "4 boolean operations (union, intersect, difference, xor).\n";
  return 0;
}
