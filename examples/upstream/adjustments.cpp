// Native SDL port of @flighthq/example-adjustments.
// Source: .dependencies/flight/examples/packages/adjustments/src/app.ts

#include "sdl_host.hpp"

#include <flight/adjustments/color_matrix_math.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace {

using flight::examples::Canvas;
using flight::examples::Frame;

constexpr std::array<std::uint32_t, 8> sample_colors{
    0xff0000ffU,
    0x00ff00ffU,
    0x0000ffffU,
    0xffff00ffU,
    0xff00ffffU,
    0x00ffffffU,
    0xffffffffU,
    0x808080ffU,
};
constexpr double swatch_size = 40.0;
constexpr double swatch_gap = 8.0;
constexpr double swatches_x = 40.0;

[[nodiscard]] std::string fixed(double value, int precision) {
  if (std::abs(value) < 0.0005) value = 0.0;
  std::ostringstream stream;
  stream << std::fixed << std::setprecision(precision) << value;
  return stream.str();
}

[[nodiscard]] std::string hex_color(std::uint32_t color) {
  std::ostringstream stream;
  stream << '#' << std::hex << std::nouppercase << std::setfill('0') << std::setw(6)
         << ((color >> 8U) & 0xffffffU);
  return stream.str();
}

class AdjustmentsExample final {
 public:
  void frame(const Frame& frame) {
    update_slider(frame);
    const auto matrix = fused_matrix();
    draw_controls(frame.canvas);
    draw_matrix(frame.canvas, matrix);
    draw_swatches(frame.canvas, matrix);
  }

 private:
  struct Slider final {
    std::string_view label;
    double minimum;
    double maximum;
    double* value;
  };

  [[nodiscard]] std::array<Slider, 4> sliders() {
    return {{
        {"Brightness", -1.0, 1.0, &brightness_},
        {"Contrast", 0.0, 3.0, &contrast_},
        {"Hue Rotate", -180.0, 180.0, &hue_rotation_},
        {"Saturation", 0.0, 3.0, &saturation_},
    }};
  }

  void update_slider(const Frame& frame) {
    if (frame.input.pointer_pressed) {
      active_slider_ = -1;
      for (int index = 0; index < 4; ++index) {
        if (flight::examples::contains(
                132.0,
                44.0 + static_cast<double>(index) * 31.0,
                236.0,
                18.0,
                frame.input.pointer_x,
                frame.input.pointer_y)) {
          active_slider_ = index;
        }
      }
    }
    if (frame.input.pointer_released) active_slider_ = -1;
    if (!frame.input.pointer_down || active_slider_ < 0) return;
    auto controls = sliders();
    auto& slider = controls[static_cast<std::size_t>(active_slider_)];
    const double amount = std::clamp((frame.input.pointer_x - 132.0) / 236.0, 0.0, 1.0);
    *slider.value = slider.minimum + (slider.maximum - slider.minimum) * amount;
    if (active_slider_ == 2) *slider.value = std::round(*slider.value);
  }

  [[nodiscard]] flight::Array<double> fused_matrix() const {
    using namespace flight::adjustments;
    flight::Array<flight::Array<double>> matrices;
    if (brightness_ != 0.0) matrices.push(create_brightness_color_matrix(brightness_));
    if (contrast_ != 1.0) matrices.push(create_contrast_color_matrix(contrast_));
    if (hue_rotation_ != 0.0) matrices.push(create_hue_rotate_color_matrix(hue_rotation_));
    if (saturation_ != 1.0) matrices.push(create_saturation_color_matrix(saturation_));
    return matrices.empty() ? create_identity_color_matrix() : fuse_color_matrices(matrices);
  }

  void draw_controls(Canvas& canvas) {
    canvas.fill_rounded_rectangle(18.0, 12.0, 382.0, 150.0, 9.0, 0x111426eeU);
    canvas.stroke_rectangle(18.0, 12.0, 382.0, 150.0, 0x39405effU);
    canvas.text("Color matrix controls", 30.0, 22.0, 0xd8def2ffU, 12.0);
    auto controls = sliders();
    for (std::size_t index = 0; index < controls.size(); ++index) {
      const double y = 51.0 + static_cast<double>(index) * 31.0;
      const auto& slider = controls[index];
      const double amount = (*slider.value - slider.minimum) / (slider.maximum - slider.minimum);
      canvas.text(slider.label, 30.0, y - 7.0, 0xd8def2ffU, 10.0);
      canvas.fill_rectangle(132.0, y, 236.0, 4.0, 0x39405effU);
      canvas.fill_rectangle(132.0, y, 236.0 * amount, 4.0, 0xeedd44ffU);
      canvas.fill_circle(132.0 + 236.0 * amount, y + 2.0, 7.0, 0xeedd44ffU);
      canvas.text(fixed(*slider.value, 2), 410.0, y - 8.0, 0xeedd44ffU, 10.0);
    }
  }

  static void draw_matrix(Canvas& canvas, const flight::Array<double>& matrix) {
    constexpr double matrix_x = 40.0;
    constexpr double matrix_y = 200.0;
    constexpr double cell_width = 90.0;
    constexpr double cell_height = 24.0;
    constexpr std::array<std::string_view, 4> rows{"R'", "G'", "B'", "A'"};
    constexpr std::array<std::string_view, 5> columns{"R", "G", "B", "A", "Offset"};
    canvas.text("4x5 Color Matrix (fused)", matrix_x, matrix_y - 30.0, 0xccccccffU, 16.0);
    for (std::size_t column = 0; column < columns.size(); ++column) {
      canvas.text(columns[column], matrix_x + 40.0 + static_cast<double>(column) * cell_width, matrix_y - 10.0, 0x888888ffU, 12.0);
    }
    for (std::size_t row = 0; row < rows.size(); ++row) {
      const double y = matrix_y + 14.0 + static_cast<double>(row) * cell_height;
      canvas.text(rows[row], matrix_x, y, 0x888888ffU, 12.0);
      for (std::size_t column = 0; column < columns.size(); ++column) {
        canvas.text(
            fixed(matrix.element(static_cast<double>(row * 5 + column)), 3),
            matrix_x + 40.0 + static_cast<double>(column) * cell_width,
            y,
            0xeedd44ffU,
            13.0);
      }
    }
  }

  static void draw_swatches(Canvas& canvas, const flight::Array<double>& matrix) {
    constexpr double before_y = 340.0;
    constexpr double after_y = 420.0;
    constexpr double tint_y = 510.0;
    canvas.text("Original colors", swatches_x, before_y - 22.0, 0xccccccffU, 14.0);
    canvas.text("After matrix", swatches_x, after_y - 22.0, 0xccccccffU, 14.0);
    canvas.text("Per-instance node tint", swatches_x, tint_y - 22.0, 0xccccccffU, 14.0);
    for (std::size_t index = 0; index < sample_colors.size(); ++index) {
      const double x = swatches_x + static_cast<double>(index) * (swatch_size + swatch_gap);
      const std::uint32_t transformed =
          flight::adjustments::apply_color_matrix_to_color(matrix, sample_colors[index]);
      canvas.fill_rectangle(x, before_y, swatch_size, swatch_size, sample_colors[index]);
      canvas.fill_rectangle(x, after_y, swatch_size, swatch_size, transformed);
      canvas.text(hex_color(transformed), x, after_y + swatch_size + 4.0, 0x888888ffU, 7.0);
      canvas.fill_rectangle(x, tint_y, swatch_size, swatch_size, sample_colors[index]);
    }
    canvas.text(
        "Adjustments compose a 4x5 color matrix as pure data. Use sliders to build a fused matrix.",
        swatches_x,
        578.0,
        0x666666ffU,
        9.0);
  }

  double brightness_{};
  double contrast_{1.0};
  double hue_rotation_{};
  double saturation_{1.0};
  int active_slider_{-1};
};

} // namespace

int main(int argc, char** argv) {
  try {
    AdjustmentsExample example;
    return flight::examples::SdlExample::run(
        argc,
        argv,
        "Flight adjustments - SDL",
        [&](const Frame& frame) { example.frame(frame); });
  } catch (const std::exception& error) {
    std::cerr << "adjustments example failed: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
