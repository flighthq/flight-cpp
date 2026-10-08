// Naive C++ port of @flighthq/example-adjustments
// Ported from .dependencies/flight/examples/packages/adjustments/src/app.ts

#include <flight/adjustments/color_matrix_math.hpp>
#include <flight/node/hierarchy.hpp>
#include <flight/node/node_transform2d.hpp>
#include <flight/scene2d/display_object.hpp>
#include <flight/shape/shape.hpp>
#include <flight/shape/shape_commands.hpp>
#include <flight/text/text_label.hpp>

#include <array>
#include <cmath>
#include <iostream>

namespace {

constexpr double SWATCH_SIZE = 40.0;
constexpr double SWATCH_GAP = 8.0;
constexpr double SWATCHES_X = 40.0;
constexpr double SWATCHES_BEFORE_Y = 340.0;
constexpr double SWATCHES_AFTER_Y = 420.0;

constexpr std::array<std::uint32_t, 8> SAMPLE_COLORS = {
  0xff0000ff, 0x00ff00ff, 0x0000ffff, 0xffff00ff,
  0xff00ffff, 0x00ffffff, 0xffffffff, 0x808080ff,
};

} // namespace

int main() {
  using namespace flight::node;
  using namespace flight::shape;
  using namespace flight::scene2d;
  using namespace flight::adjustments;

  // NOT AVAILABLE: generated Scene2D construction/hierarchy is partial at this SDK pin.
  auto root = create_display_object(std::nullopt);

  double brightness = 0.1;
  double contrast = 1.2;
  double hueRotation = 30.0;
  double saturation = 1.0;

  auto brightnessMatrix = create_brightness_color_matrix(brightness);
  auto contrastMatrix = create_contrast_color_matrix(contrast);
  auto hueRotateMatrix = create_hue_rotate_color_matrix(hueRotation);
  auto saturationMatrix = create_saturation_color_matrix(saturation);

  auto fused = fuse_color_matrices(brightnessMatrix, contrastMatrix);
  fused = fuse_color_matrices(fused, hueRotateMatrix);
  fused = fuse_color_matrices(fused, saturationMatrix);

  auto heading = flight::text::create_text_label();
  heading->data.text = flight::String("COLOR ADJUSTMENTS");
  heading->x = 40.0;
  heading->y = 20.0;
  invalidate_node_local_transform(heading);
  add_node_child(root, heading);

  auto beforeLabel = flight::text::create_text_label();
  beforeLabel->data.text = flight::String("Before matrix:");
  beforeLabel->x = SWATCHES_X;
  beforeLabel->y = SWATCHES_BEFORE_Y - 20.0;
  invalidate_node_local_transform(beforeLabel);
  add_node_child(root, beforeLabel);

  for (std::size_t i = 0; i < SAMPLE_COLORS.size(); ++i) {
    double x = SWATCHES_X + static_cast<double>(i) * (SWATCH_SIZE + SWATCH_GAP);

    auto before = create_shape(std::nullopt);
    append_shape_begin_fill(before, SAMPLE_COLORS[i]);
    append_shape_rectangle(before, x, SWATCHES_BEFORE_Y, SWATCH_SIZE, SWATCH_SIZE);
    append_shape_end_fill(before);
    add_node_child(root, before);

    std::uint32_t adjusted = apply_color_matrix_to_color(fused, SAMPLE_COLORS[i]);
    auto after = create_shape(std::nullopt);
    append_shape_begin_fill(after, adjusted);
    append_shape_rectangle(after, x, SWATCHES_AFTER_Y, SWATCH_SIZE, SWATCH_SIZE);
    append_shape_end_fill(after);
    add_node_child(root, after);
  }

  auto afterLabel = flight::text::create_text_label();
  afterLabel->data.text = flight::String("After matrix:");
  afterLabel->x = SWATCHES_X;
  afterLabel->y = SWATCHES_AFTER_Y - 20.0;
  invalidate_node_local_transform(afterLabel);
  add_node_child(root, afterLabel);

  auto tintDemo = create_shape(std::nullopt);
  append_shape_begin_fill(tintDemo, 0xffffffff);
  append_shape_rectangle(tintDemo, SWATCHES_X, 510.0, 200.0, 40.0);
  append_shape_end_fill(tintDemo);
  set_node_color_adjustments_tint(tintDemo, 0xff6600ff);
  add_node_child(root, tintDemo);

  std::cout << "Flight adjustments example (naive C++ port): "
            << SAMPLE_COLORS.size() << " color swatches processed.\n";
  return 0;
}
