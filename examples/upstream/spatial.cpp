// Native SDL port of @flighthq/example-spatial.
// Source: .dependencies/flight/examples/packages/spatial/src/app.ts

#include "sdl_host.hpp"

#include <flight/spatial/spatial_index.hpp>
#include <flight/spatial/uniform_grid.hpp>
#include <flight/types/spatial.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <random>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
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
  explicit Random(bool deterministic) : deterministic_(deterministic) {
    if (!deterministic_) engine_.seed(std::random_device{}());
  }

  [[nodiscard]] double range(double minimum, double maximum) {
    double amount{};
    if (deterministic_) {
      state_ = state_ * 1664525U + 1013904223U;
      amount = static_cast<double>(state_) / 4294967296.0;
    } else {
      amount = std::generate_canonical<double, 53>(engine_);
    }
    return minimum + amount * (maximum - minimum);
  }

 private:
  bool deterministic_{};
  std::uint32_t state_{0x05a171a1U};
  std::mt19937_64 engine_{};
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
  explicit SpatialExample(bool capture_mode)
      : capture_mode_(capture_mode),
        active_mode_(capture_mode ? Mode::region : Mode::pairs),
        index_(flight::spatial::create_spatial_index2_d(
            flight::spatial::create_uniform_grid_spatial_backend2_d(100.0))) {
    Random random(capture_mode);
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
      if (flight::spatial::insert_spatial_object2_d(index_, static_cast<double>(index), bounds)) {
        ++inserted_count_;
      }
    }
  }

  void frame(const Frame& frame) {
    handle_input(frame);
    const std::size_t updated_count = update(std::min(frame.delta_time, 0.05));
    std::vector<bool> highlighted(objects_.size());
    int results{};
    if (active_mode_ == Mode::pairs) {
      flight::spatial::query_spatial_pairs2_d(index_, pairs_out_);
      results = static_cast<int>(pairs_out_.size());
      for (const auto& pair : pairs_out_) {
        highlight(pair->a, highlighted);
        highlight(pair->b, highlighted);
      }
    } else {
      if (active_mode_ == Mode::point) {
        flight::spatial::query_spatial_point2_d(index_, mouse_x_, mouse_y_, ids_out_);
      } else if (active_mode_ == Mode::ray) {
        flight::spatial::query_spatial_ray2_d(
            index_, 0.0, height / 2.0, mouse_x_, mouse_y_ - height / 2.0, ids_out_);
      } else {
        const auto region = flight::make_ref<flight::types::SpatialAabb2D>(
            flight::types::SpatialAabb2D{
                .min_x = mouse_x_ - 50.0,
                .min_y = mouse_y_ - 50.0,
                .max_x = mouse_x_ + 50.0,
                .max_y = mouse_y_ + 50.0,
            });
        flight::spatial::query_spatial_region2_d(index_, region, ids_out_);
      }
      results = static_cast<int>(ids_out_.size());
      for (const double id : ids_out_) {
        highlight(id, highlighted);
      }
    }
    if (frame.smoke) verify_smoke(updated_count);
    draw(frame.canvas, active_mode_, highlighted, results, mouse_x_, mouse_y_);
  }

 private:
  void handle_input(const Frame& frame) {
    if (capture_mode_) return;
    if (frame.input.pointer_inside) {
      mouse_x_ = frame.input.pointer_x;
      mouse_y_ = frame.input.pointer_y;
    }
    if (frame.input.pointer_pressed) active_mode_ = Mode::point;
    for (const SDL_Keycode key : frame.input.keys_pressed) {
      if (key == SDLK_R) active_mode_ = Mode::ray;
      if (key == SDLK_Q) active_mode_ = Mode::region;
    }
    for (const SDL_Keycode key : frame.input.keys_released) {
      if (key == SDLK_R || key == SDLK_Q) active_mode_ = Mode::pairs;
    }
    if (frame.input.pointer_released) active_mode_ = Mode::pairs;
  }

  [[nodiscard]] std::size_t update(double delta_time) {
    std::size_t updated_count{};
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
      if (flight::spatial::update_spatial_object2_d(index_, object.id, object.bounds)) {
        ++updated_count;
      }
    }
    return updated_count;
  }

  [[nodiscard]] static bool overlaps(
      const flight::types::SpatialAabb2D& a,
      const flight::types::SpatialAabb2D& b) {
    return a.min_x < b.max_x && a.max_x > b.min_x &&
           a.min_y < b.max_y && a.max_y > b.min_y;
  }

  [[nodiscard]] static bool shares_grid_cell(
      const flight::types::SpatialAabb2D& a,
      const flight::types::SpatialAabb2D& b) {
    constexpr double cell_size = 100.0;
    const double shared_min_x = std::max(
        std::floor(a.min_x / cell_size),
        std::floor(b.min_x / cell_size));
    const double shared_max_x = std::min(
        std::floor(a.max_x / cell_size),
        std::floor(b.max_x / cell_size));
    const double shared_min_y = std::max(
        std::floor(a.min_y / cell_size),
        std::floor(b.min_y / cell_size));
    const double shared_max_y = std::min(
        std::floor(a.max_y / cell_size),
        std::floor(b.max_y / cell_size));
    return shared_min_x <= shared_max_x && shared_min_y <= shared_max_y;
  }

  void verify_smoke(std::size_t updated_count) const {
    if (inserted_count_ != objects_.size() || updated_count != 5 ||
        active_mode_ != Mode::region || mouse_x_ != width * 0.5 ||
        mouse_y_ != height * 0.5) {
      throw std::runtime_error("spatial smoke did not preserve capture state or index updates");
    }

    const flight::types::SpatialAabb2D region{
        .min_x = mouse_x_ - 50.0,
        .min_y = mouse_y_ - 50.0,
        .max_x = mouse_x_ + 50.0,
        .max_y = mouse_y_ + 50.0,
    };
    std::size_t expected_region_count{};
    for (const auto& object : objects_) {
      if (overlaps(*object.bounds, region)) {
        ++expected_region_count;
        if (!ids_out_.includes(object.id)) {
          throw std::runtime_error("spatial region query omitted an overlapping indexed object");
        }
      }
    }
    if (ids_out_.size() != expected_region_count) {
      throw std::runtime_error("spatial region query returned non-overlapping indexed objects");
    }

    flight::Array<double> probe_ids;
    const auto& first = *objects_.front().bounds;
    const double first_x = (first.min_x + first.max_x) * 0.5;
    const double first_y = (first.min_y + first.max_y) * 0.5;
    flight::spatial::query_spatial_point2_d(index_, first_x, first_y, probe_ids);
    if (!probe_ids.includes(objects_.front().id)) {
      throw std::runtime_error("spatial point query did not observe the generated index");
    }
    flight::spatial::query_spatial_ray2_d(index_, 0.0, 0.0, first_x, first_y, probe_ids);
    if (!probe_ids.includes(objects_.front().id)) {
      throw std::runtime_error("spatial ray query did not observe the generated index");
    }

    flight::Array<flight::Ref<flight::types::SpatialPair>> probe_pairs;
    flight::spatial::query_spatial_pairs2_d(index_, probe_pairs);
    std::set<std::pair<std::size_t, std::size_t>> candidate_pairs;
    for (const auto& pair : probe_pairs) {
      const auto first_index = static_cast<std::size_t>(pair->a);
      const auto second_index = static_cast<std::size_t>(pair->b);
      if (pair->a != static_cast<double>(first_index) ||
          pair->b != static_cast<double>(second_index) ||
          first_index >= objects_.size() || second_index >= objects_.size() ||
          first_index >= second_index ||
          !shares_grid_cell(*objects_[first_index].bounds, *objects_[second_index].bounds)) {
        throw std::runtime_error("spatial pair query returned an invalid broadphase candidate");
      }
      candidate_pairs.emplace(first_index, second_index);
    }
    if (candidate_pairs.size() != probe_pairs.size()) {
      throw std::runtime_error("spatial pair query returned a duplicate broadphase candidate");
    }
    for (std::size_t first_index = 0; first_index < objects_.size(); ++first_index) {
      for (std::size_t second_index = first_index + 1;
           second_index < objects_.size();
           ++second_index) {
        const bool expected = shares_grid_cell(
            *objects_[first_index].bounds,
            *objects_[second_index].bounds);
        const bool present = candidate_pairs.contains({first_index, second_index});
        if (expected != present) {
          throw std::runtime_error("spatial pair query did not match the indexed grid cells");
        }
      }
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
    canvas.text("Click: point query | R: ray query | Q: region query", 10.0, 6.0, 0x888888ffU, 13.0);
    canvas.text(
        "Mode: " + std::string(mode_name(mode)) + "  Hits: " + std::to_string(results),
        10.0,
        height - 30.0,
        0xccccccffU,
        14.0);
  }

  std::vector<Object> objects_;
  bool capture_mode_{};
  Mode active_mode_{Mode::pairs};
  double mouse_x_{width * 0.5};
  double mouse_y_{height * 0.5};
  std::size_t inserted_count_{};
  flight::Ref<flight::types::SpatialIndex2D> index_;
  flight::Array<flight::Ref<flight::types::SpatialPair>> pairs_out_;
  flight::Array<double> ids_out_;
};

} // namespace

int main(int argc, char** argv) {
  try {
    const bool capture_mode = argc == 2 && std::string_view(argv[1]) == "--smoke";
    SpatialExample example(capture_mode);
    return flight::examples::SdlExample::run(
        argc,
        argv,
        "Flight spatial index - SDL",
        [&](const Frame& frame) { example.frame(frame); },
        static_cast<int>(width),
        static_cast<int>(height));
  } catch (const std::exception& error) {
    std::cerr << "spatial example failed: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
