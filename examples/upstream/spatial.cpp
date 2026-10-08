// Native SDL port of @flighthq/example-spatial.
// Source: .dependencies/flight/examples/packages/spatial/src/app.ts

#include "sdl_host.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <limits>
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
  double x{};
  double y{};
  double width{};
  double height{};
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

[[nodiscard]] bool overlaps(const Object& a, const Object& b) {
  return a.x <= b.x + b.width && a.x + a.width >= b.x &&
         a.y <= b.y + b.height && a.y + a.height >= b.y;
}

[[nodiscard]] bool contains(const Object& object, double x, double y) {
  return x >= object.x && x <= object.x + object.width &&
         y >= object.y && y <= object.y + object.height;
}

[[nodiscard]] bool ray_hits(const Object& object, double direction_x, double direction_y) {
  constexpr double origin_x = 0.0;
  constexpr double origin_y = height / 2.0;
  double minimum = 0.0;
  double maximum = std::numeric_limits<double>::infinity();
  const auto axis = [&](double origin, double direction, double low, double high) {
    if (std::abs(direction) < 1e-9) return origin >= low && origin <= high;
    double first = (low - origin) / direction;
    double second = (high - origin) / direction;
    if (first > second) std::swap(first, second);
    minimum = std::max(minimum, first);
    maximum = std::min(maximum, second);
    return minimum <= maximum;
  };
  return axis(origin_x, direction_x, object.x, object.x + object.width) &&
         axis(origin_y, direction_y, object.y, object.y + object.height) && maximum >= 0.0;
}

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
  SpatialExample() {
    Random random;
    for (int index = 0; index < 20; ++index) {
      const double object_width = random.range(30.0, 80.0);
      const double object_height = random.range(20.0, 60.0);
      const double x = random.range(10.0, width - object_width - 10.0);
      const double y = random.range(40.0, height - object_height - 50.0);
      const bool moving = index < 5;
      objects_.push_back({
          x,
          y,
          object_width,
          object_height,
          moving ? random.range(-30.0, 30.0) : 0.0,
          moving ? random.range(-30.0, 30.0) : 0.0,
      });
    }
  }

  void frame(const Frame& frame) {
    update(frame.delta_time);
    const double mouse_x = frame.input.pointer_x;
    const double mouse_y = frame.input.pointer_y;
    const Mode mode = frame.input.pointer_down ? Mode::point
                      : frame.input.key_down(SDLK_R) ? Mode::ray
                      : frame.input.key_down(SDLK_Q) ? Mode::region
                                                    : Mode::pairs;
    std::vector<bool> highlighted(objects_.size());
    int results{};
    if (mode == Mode::pairs) {
      for (std::size_t first = 0; first < objects_.size(); ++first) {
        for (std::size_t second = first + 1; second < objects_.size(); ++second) {
          if (overlaps(objects_[first], objects_[second])) {
            highlighted[first] = true;
            highlighted[second] = true;
            ++results;
          }
        }
      }
    } else {
      const Object region{mouse_x - 50.0, mouse_y - 50.0, 100.0, 100.0, 0.0, 0.0};
      for (std::size_t index = 0; index < objects_.size(); ++index) {
        const bool hit = mode == Mode::point ? contains(objects_[index], mouse_x, mouse_y)
                       : mode == Mode::region ? overlaps(objects_[index], region)
                       : ray_hits(objects_[index], mouse_x, mouse_y - height / 2.0);
        if (hit) {
          highlighted[index] = true;
          ++results;
        }
      }
    }
    draw(frame.canvas, mode, highlighted, results, mouse_x, mouse_y);
  }

 private:
  void update(double delta_time) {
    for (std::size_t index = 0; index < 5; ++index) {
      auto& object = objects_[index];
      object.x += object.velocity_x * delta_time;
      object.y += object.velocity_y * delta_time;
      if (object.x < 0.0) {
        object.x = 0.0;
        object.velocity_x = std::abs(object.velocity_x);
      }
      if (object.x + object.width > width) {
        object.x = width - object.width;
        object.velocity_x = -std::abs(object.velocity_x);
      }
      if (object.y < 0.0) {
        object.y = 0.0;
        object.velocity_y = std::abs(object.velocity_y);
      }
      if (object.y + object.height > height) {
        object.y = height - object.height;
        object.velocity_y = -std::abs(object.velocity_y);
      }
    }
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
      const std::uint32_t color = highlighted[index] ? hit_color : color_idle;
      canvas.fill_rectangle(object.x, object.y, object.width, object.height, color, 0.5);
      canvas.stroke_rectangle(object.x, object.y, object.width, object.height, 0x335577ffU);
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
