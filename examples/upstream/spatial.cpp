// Native SDL port of @flighthq/example-spatial.
// Source: .dependencies/flight/examples/packages/spatial/src/app.ts

#include "sdl_host.hpp"

#include <flight/spatial/spatial_index.hpp>
#include <flight/spatial/uniform_grid.hpp>
#include <flight/types/spatial.hpp>

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>
#include <vector>

namespace {

using flight::examples::Canvas;
using flight::examples::Frame;

constexpr double width = 800.0;
constexpr double height = 500.0;
constexpr std::uint32_t color_idle = 0x4488ccffU;
constexpr std::uint32_t color_overlap = 0xcc4444ffU;
constexpr std::uint32_t color_point = 0x44cc44ffU;
constexpr std::uint32_t color_ray = 0xcccc44ffU;
constexpr std::uint32_t color_region = 0x44ccccffU;

struct Object final {
  double id{};
  flight::Ref<flight::types::SpatialAabb2D> bounds;
  double velocity_x{};
  double velocity_y{};
};

class Random final {
 public:
  [[nodiscard]] double range(double minimum, double maximum) {
    state_ = state_ * 1664525U + 1013904223U;
    const double amount = static_cast<double>(state_) / 4294967296.0;
    return minimum + amount * (maximum - minimum);
  }

 private:
  std::uint32_t state_{0x05a171a1U};
};

enum class Mode { pairs, point, ray, region };

[[nodiscard]] std::string_view mode_name(Mode mode) {
  switch (mode) {
    case Mode::pairs: return "Pairs";
    case Mode::point: return "Point";
    case Mode::ray: return "Ray";
    case Mode::region: return "Region";
  }
  return "Pairs";
}

class SpatialExample final {
 public:
  SpatialExample()
      : index_(flight::spatial::create_spatial_index2_d(
            flight::spatial::create_uniform_grid_spatial_backend2_d(100.0))) {
    Random random;
    for (int index = 0; index < 20; ++index) {
      const double object_width = random.range(30.0, 80.0);
      const double object_height = random.range(20.0, 60.0);
      const double x = random.range(10.0, width - object_width - 10.0);
      const double y = random.range(40.0, height - object_height - 50.0);
      const bool moving = index < 5;
      auto bounds = flight::make_ref<flight::types::SpatialAabb2D>(
          flight::types::SpatialAabb2D{
              .min_x = x,
              .min_y = y,
              .max_x = x + object_width,
              .max_y = y + object_height,
          });
      objects_.push_back(Object{
          .id = static_cast<double>(index),
          .bounds = bounds,
          .velocity_x = moving ? random.range(-30.0, 30.0) : 0.0,
          .velocity_y = moving ? random.range(-30.0, 30.0) : 0.0,
      });
      flight::spatial::insert_spatial_object2_d(index_, static_cast<double>(index), bounds);
    }
  }

  void frame(const Frame& frame) {
    update(frame.delta_time);
    const double mouse_x = frame.input.pointer_x;
    const double mouse_y = frame.input.pointer_y;
    const Mode mode = frame.smoke ? Mode::region
                      : frame.input.pointer_down ? Mode::point
                      : frame.input.key_down(SDLK_R) ? Mode::ray
                      : frame.input.key_down(SDLK_Q) ? Mode::region
                                                    : Mode::pairs;
    std::vector<bool> highlighted(objects_.size());
    int results{};
    if (mode == Mode::pairs) {
      flight::spatial::query_spatial_pairs2_d(index_, pairs_out_);
      results = static_cast<int>(pairs_out_.size());
      for (const auto& pair : pairs_out_) {
        highlight(pair->a, highlighted);
        highlight(pair->b, highlighted);
      }
    } else {
      if (mode == Mode::point) {
        flight::spatial::query_spatial_point2_d(index_, mouse_x, mouse_y, ids_out_);
      } else if (mode == Mode::ray) {
        flight::spatial::query_spatial_ray2_d(
            index_, 0.0, height / 2.0, mouse_x, mouse_y - height / 2.0, ids_out_);
      } else {
        const auto region = flight::make_ref<flight::types::SpatialAabb2D>(
            flight::types::SpatialAabb2D{
                .min_x = mouse_x - 50.0,
                .min_y = mouse_y - 50.0,
                .max_x = mouse_x + 50.0,
                .max_y = mouse_y + 50.0,
            });
        flight::spatial::query_spatial_region2_d(index_, region, ids_out_);
      }
      results = static_cast<int>(ids_out_.size());
      for (const double id : ids_out_) {
        highlight(id, highlighted);
      }
    }
    draw(frame.canvas, mode, highlighted, results, mouse_x, mouse_y);
  }

 private:
  void update(double delta_time) {
    for (std::size_t index = 0; index < 5; ++index) {
      auto& object = objects_[index];
      auto& bounds = *object.bounds;
      const double object_width = bounds.max_x - bounds.min_x;
      const double object_height = bounds.max_y - bounds.min_y;
      bounds.min_x += object.velocity_x * delta_time;
      bounds.min_y += object.velocity_y * delta_time;
      bounds.max_x = bounds.min_x + object_width;
      bounds.max_y = bounds.min_y + object_height;
      if (bounds.min_x < 0.0) {
        bounds.min_x = 0.0;
        bounds.max_x = object_width;
        object.velocity_x = std::abs(object.velocity_x);
      }
      if (bounds.max_x > width) {
        bounds.max_x = width;
        bounds.min_x = width - object_width;
        object.velocity_x = -std::abs(object.velocity_x);
      }
      if (bounds.min_y < 0.0) {
        bounds.min_y = 0.0;
        bounds.max_y = object_height;
        object.velocity_y = std::abs(object.velocity_y);
      }
      if (bounds.max_y > height) {
        bounds.max_y = height;
        bounds.min_y = height - object_height;
        object.velocity_y = -std::abs(object.velocity_y);
      }
      flight::spatial::update_spatial_object2_d(index_, object.id, object.bounds);
    }
  }

  void highlight(double id, std::vector<bool>& highlighted) const {
    const auto index = static_cast<std::size_t>(id);
    if (index < highlighted.size()) highlighted[index] = true;
  }

  void draw(
      Canvas& canvas,
      Mode mode,
      const std::vector<bool>& highlighted,
      int results,
      double mouse_x,
      double mouse_y) const {
    for (double x = 100.0; x < width; x += 100.0) canvas.line(x, 0.0, x, height, 0x263954ffU, 1.0, 0.65);
    for (double y = 100.0; y < height; y += 100.0) canvas.line(0.0, y, width, y, 0x263954ffU, 1.0, 0.65);
    const std::uint32_t hit_color = mode == Mode::point ? color_point
                                    : mode == Mode::ray ? color_ray
                                    : mode == Mode::region ? color_region
                                                           : color_overlap;
    for (std::size_t index = 0; index < objects_.size(); ++index) {
      const auto& object = objects_[index];
      const auto& bounds = *object.bounds;
      const std::uint32_t color = highlighted[index] ? hit_color : color_idle;
      canvas.fill_rectangle(
          bounds.min_x,
          bounds.min_y,
          bounds.max_x - bounds.min_x,
          bounds.max_y - bounds.min_y,
          color,
          0.5);
      canvas.stroke_rectangle(
          bounds.min_x,
          bounds.min_y,
          bounds.max_x - bounds.min_x,
          bounds.max_y - bounds.min_y,
          0x335577ffU);
    }
    if (mode == Mode::point) {
      canvas.line(mouse_x - 8.0, mouse_y, mouse_x + 8.0, mouse_y, color_point, 2.0);
      canvas.line(mouse_x, mouse_y - 8.0, mouse_x, mouse_y + 8.0, color_point, 2.0);
    } else if (mode == Mode::ray) {
      const double length = std::hypot(mouse_x, mouse_y - height / 2.0);
      if (length > 0.0) {
        canvas.line(
            0.0,
            height / 2.0,
            mouse_x / length * width * 1.5,
            height / 2.0 + (mouse_y - height / 2.0) / length * width * 1.5,
            color_ray);
      }
    } else if (mode == Mode::region) {
      canvas.fill_rectangle(mouse_x - 50.0, mouse_y - 50.0, 100.0, 100.0, color_region, 0.15);
      canvas.stroke_rectangle(mouse_x - 50.0, mouse_y - 50.0, 100.0, 100.0, color_region);
    }
    canvas.text("Click: point query | R: ray query | Q: region query", 10.0, 6.0, 0x888888ffU, 11.0);
    canvas.text(
        "Mode: " + std::string(mode_name(mode)) + "  Hits: " + std::to_string(results),
        10.0,
        height - 30.0,
        0xccccccffU,
        14.0);
  }

  std::vector<Object> objects_;
  flight::Ref<flight::types::SpatialIndex2D> index_;
  flight::Array<flight::Ref<flight::types::SpatialPair>> pairs_out_;
  flight::Array<double> ids_out_;
};

} // namespace

int main(int argc, char** argv) {
  try {
    SpatialExample example;
    return flight::examples::SdlExample::run(
        argc,
        argv,
        "Flight spatial index - SDL",
        [&](const Frame& frame) { example.frame(frame); });
  } catch (const std::exception& error) {
    std::cerr << "spatial example failed: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
