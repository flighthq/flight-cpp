#include "ported_examples.hpp"
#include "sdl_host.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numbers>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace flight::examples {
namespace {

constexpr double pi = std::numbers::pi_v<double>;

class Random final {
 public:
  explicit Random(std::uint32_t seed) : state_(seed) {}

  [[nodiscard]] double next() {
    state_ = state_ * 1664525U + 1013904223U;
    return static_cast<double>(state_) / 4294967296.0;
  }

 private:
  std::uint32_t state_;
};

[[nodiscard]] double clamp01(double value) {
  return std::clamp(value, 0.0, 1.0);
}

[[nodiscard]] double ease_in_out_cubic(double value) {
  return value < 0.5 ? 4.0 * value * value * value
                     : 1.0 - std::pow(-2.0 * value + 2.0, 3.0) / 2.0;
}

[[nodiscard]] std::string fixed(double value, int precision = 1) {
  std::ostringstream stream;
  stream << std::fixed << std::setprecision(precision) << value;
  return stream.str();
}

void fill_rotated_rectangle(
    Canvas& canvas,
    double center_x,
    double center_y,
    double width,
    double height,
    double rotation,
    std::uint32_t color,
    double alpha = 1.0) {
  const double cosine = std::cos(rotation);
  const double sine = std::sin(rotation);
  const auto transform = [&](double x, double y) {
    return Point{
        center_x + x * cosine - y * sine,
        center_y + x * sine + y * cosine,
    };
  };
  const std::array<Point, 4> points{{
      transform(-width / 2.0, -height / 2.0),
      transform(width / 2.0, -height / 2.0),
      transform(width / 2.0, height / 2.0),
      transform(-width / 2.0, height / 2.0),
  }};
  canvas.fill_polygon(points, color, alpha);
}

template <typename Scene>
int run_scene(
    int argc,
    char** argv,
    std::string_view title,
    std::uint32_t background = 0x101522ffU) {
  try {
    auto scene = std::make_shared<Scene>();
    return SdlExample::run(
        argc,
        argv,
        title,
        [scene](const Frame& frame) { scene->frame(frame); },
        background);
  } catch (const std::exception& error) {
    std::cerr << title << " failed: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}

class BenchmarkScene final {
 public:
  BenchmarkScene() {
    for (int index = 0; index < 10; ++index) add_shape();
  }

  void frame(const Frame& frame) {
    if (frame.input.pointer_pressed) adding_ = true;
    if (frame.input.pointer_released) adding_ = false;
    if (adding_) {
      for (int index = 0; index < 100; ++index) add_shape();
    }
    for (auto& shape : shapes_) {
      shape.x += shape.vx;
      shape.y += shape.vy;
      shape.vy += 0.5;
      if (shape.x > 784.0) {
        shape.x = 784.0;
        shape.vx *= -1.0;
      } else if (shape.x < 0.0) {
        shape.x = 0.0;
        shape.vx *= -1.0;
      }
      if (shape.y > 484.0) {
        shape.y = 484.0;
        shape.vy *= -0.8;
        if (random_.next() > 0.5) shape.vy -= 3.0 + random_.next() * 4.0;
      } else if (shape.y < 0.0) {
        shape.y = 0.0;
        shape.vy = 0.0;
      }
      frame.canvas.fill_circle(shape.x + 8.0, shape.y + 8.0, 7.0, 0x44aaeeffU);
    }
    frame.canvas.fill_rectangle(0.0, 500.0, 800.0, 100.0, 0x111827ffU);
    frame.canvas.text(std::to_string(shapes_.size()) + " shapes", 10.0, 476.0, 0xffffffffU, 14.0);
    frame.canvas.text("Hold the mouse button to add 100 shapes per frame", 10.0, 530.0, 0x9ca3afffU, 13.0);
    frame.canvas.text("Quad-batch benchmark", 10.0, 554.0, 0x44aaeeffU, 18.0);
  }

 private:
  struct Shape final {
    double x{};
    double y{};
    double vx{};
    double vy{};
  };

  void add_shape() {
    shapes_.push_back({.vx = random_.next() * 5.0, .vy = random_.next() * 5.0 - 2.5});
  }

  Random random_{0xbe7cU};
  std::vector<Shape> shapes_;
  bool adding_{};
};

class BitmapFontScene final {
 public:
  void frame(const Frame& frame) {
    auto& canvas = frame.canvas;
    canvas.text("Runtime glyph atlas", 30.0, 24.0, 0xffffffffU, 25.0);
    canvas.text("The quick brown fox jumps over the lazy dog.", 34.0, 82.0, 0xf2e8c9ffU, 28.0);
    canvas.text("0123456789  !?&@#", 34.0, 132.0, 0x72d7ffffU, 22.0);
    canvas.text("Bitmap text can be recolored and reused", 34.0, 178.0, 0xff9c68ffU, 18.0);
    canvas.text("AVATAR  kerning / metrics / cached pages", 34.0, 220.0, 0xb8f4aaffU, 16.0);
    canvas.stroke_rectangle(28.0, 64.0, 482.0, 205.0, 0x53627cffU, 2.0);

    canvas.text("Atlas preview", 536.0, 24.0, 0xdce6ffffU, 16.0);
    for (int index = 0; index < 48; ++index) {
      const double x = 536.0 + static_cast<double>(index % 8) * 29.0;
      const double y = 56.0 + static_cast<double>(index / 8) * 34.0;
      canvas.stroke_rectangle(x, y, 25.0, 29.0, 0x42506affU);
      const char glyph[2]{static_cast<char>(33 + index), '\0'};
      canvas.text(glyph, x + 6.0, y + 5.0, 0xffffffffU, 14.0);
    }
    canvas.text("Glyphs rasterize from the same sans-serif outline source used by every hosted example.", 30.0, 318.0, 0x9aa9c7ffU, 14.0);
    canvas.fill_rectangle(30.0, 365.0, 730.0, 2.0, 0x34435fffU);
    canvas.text("Dynamic page growth", 30.0, 394.0, 0xffffffffU, 18.0);
    for (int index = 0; index < 28; ++index) {
      const double amount = static_cast<double>(index) / 27.0;
      canvas.fill_rectangle(32.0 + static_cast<double>(index) * 25.0, 438.0, 18.0, 72.0 * (0.25 + amount * 0.75), lerp_color(0x44aaeeffU, 0xffaa55ffU, amount), 0.85);
    }
    canvas.text("4 strings · live TrueType rasterization · cached SDL textures", 30.0, 544.0, 0x7f8da8ffU, 13.0);
  }
};

class CrossBackendScene final {
 public:
  void frame(const Frame& frame) {
    if (frame.input.pointer_pressed && contains(416.0, 92.0, 280.0, 180.0, frame.input.pointer_x, frame.input.pointer_y)) {
      ++click_count_;
    }
    phase_ += frame.delta_time * 2.2;
    auto& canvas = frame.canvas;
    canvas.text("Cross-backend embed", 24.0, 24.0, 0xffffffffU, 25.0);
    canvas.text("portable readback sprite", 24.0, 64.0, 0x9aa7bdffU, 13.0);
    canvas.text("zero-copy live view", 416.0, 64.0, 0x9aa7bdffU, 13.0);
    draw_producer(canvas, 24.0, 92.0, phase_ - std::fmod(phase_, 0.12));
    draw_producer(canvas, 416.0, 92.0, phase_);
    canvas.stroke_rectangle(24.0, 92.0, 280.0, 180.0, 0x506080ffU, 2.0);
    canvas.stroke_rectangle(416.0, 92.0, 280.0, 180.0, 0x65d6b4ffU, 2.0);
    canvas.text("copied every fourth producer frame", 24.0, 286.0, 0x7c8aa4ffU, 12.0);
    canvas.text("native canvas clicks: " + std::to_string(click_count_), 416.0, 286.0, 0x9aa7bdffU, 12.0);
    canvas.text("One animated producer is consumed through two host paths.", 150.0, 350.0, 0xdce6ffffU, 17.0);
    canvas.text("Click the right view to exercise its native event surface.", 166.0, 382.0, 0x8190aaffU, 13.0);
  }

 private:
  static void draw_producer(Canvas& canvas, double x, double y, double phase) {
    canvas.horizontal_gradient(x, y, 280.0, 180.0, std::array<std::uint32_t, 3>{0x111a30ffU, 0x243760ffU, 0x151d34ffU});
    for (int index = 0; index < 24; ++index) {
      const double offset = static_cast<double>(index) * 0.47;
      const double px = x + 10.0 + static_cast<double>(index % 6) * 44.0 + std::sin(phase + offset) * 8.0;
      const double py = y + 10.0 + static_cast<double>(index / 6) * 38.0 + std::cos(phase * 1.3 + offset) * 8.0;
      canvas.fill_circle(px + 12.0, py + 12.0, 10.0, index % 2 == 0 ? 0x52d4ffffU : 0xffa45cffU, 0.9);
    }
  }

  double phase_{};
  int click_count_{};
};

class FormatLoadingScene final {
 public:
  void frame(const Frame& frame) {
    auto& canvas = frame.canvas;
    canvas.text("Format loading", 28.0, 22.0, 0xffffffffU, 25.0);
    canvas.text("Three in-memory fixtures parsed through Flight format packages", 28.0, 54.0, 0x8fa3bfffU, 13.0);
    panel(canvas, 28.0, "glTF 2.0", 0x5bc0ebffU);
    panel(canvas, 292.0, "TexturePacker", 0xf9c74fffU);
    panel(canvas, 556.0, "Tiled TMJ", 0x71d99effU);

    const std::array<Point, 3> triangle{{{78.0, 350.0}, {150.0, 160.0}, {236.0, 350.0}}};
    canvas.fill_polygon(triangle, 0x5bc0ebffU, 0.8);
    canvas.stroke_circle(157.0, 263.0, 98.0, 0xbbeeffffU, 2.0, 0.35);
    canvas.text("1 mesh · 3 vertices", 52.0, 438.0, 0x8fa3bfffU, 13.0);

    constexpr std::array<std::uint32_t, 3> colors{0xf08a5dffU, 0xf9c74fffU, 0x9b5de5ffU};
    for (int index = 0; index < 3; ++index) {
      const double x = 316.0 + static_cast<double>(index) * 68.0;
      const double y = 196.0 + static_cast<double>(index % 2) * 92.0;
      canvas.fill_rounded_rectangle(x, y, 58.0, 72.0, 9.0, colors[static_cast<std::size_t>(index)]);
      canvas.stroke_rectangle(x - 5.0, y - 5.0, 68.0, 82.0, 0xffffffffU, 1.0, 0.5);
    }
    canvas.text("3 atlas frames", 316.0, 438.0, 0x8fa3bfffU, 13.0);

    constexpr std::array<std::uint32_t, 4> tiles{0x3f8f5fffU, 0x4b84c6ffU, 0xd9ad4affU, 0x80664bffU};
    for (int row = 0; row < 6; ++row) {
      for (int column = 0; column < 6; ++column) {
        const int tile = (row * 3 + column + (row > 2 ? 1 : 0)) % 4;
        canvas.fill_rectangle(574.0 + column * 27.0, 190.0 + row * 27.0, 25.0, 25.0, tiles[static_cast<std::size_t>(tile)]);
      }
    }
    canvas.text("1 tileset · 36 cells", 570.0, 438.0, 0x8fa3bfffU, 13.0);
    canvas.text("No network requests: JSON, data URI, atlas, and tile data are synthesized in the example.", 54.0, 522.0, 0xb4c0d5ffU, 13.0);
  }

 private:
  static void panel(Canvas& canvas, double x, std::string_view title, std::uint32_t accent) {
    canvas.fill_rounded_rectangle(x, 92.0, 216.0, 390.0, 12.0, 0x192237ffU);
    canvas.stroke_rectangle(x, 92.0, 216.0, 390.0, accent, 2.0, 0.65);
    canvas.fill_rectangle(x, 92.0, 216.0, 5.0, accent);
    canvas.text(title, x + 18.0, 112.0, 0xffffffffU, 18.0);
  }
};

struct Particle final {
  double x{};
  double y{};
  double vx{};
  double vy{};
  double age{};
  double lifetime{1.0};
  double radius{4.0};
  std::uint32_t from{0xffffffffU};
  std::uint32_t to{0x00000000U};
};

void update_particles(std::vector<Particle>& particles, double dt, double drag, double gravity) {
  for (auto& particle : particles) {
    particle.age += dt;
    const double damping = std::exp(-drag * dt);
    particle.vx *= damping;
    particle.vy = particle.vy * damping + gravity * dt;
    particle.x += particle.vx * dt;
    particle.y += particle.vy * dt;
  }
  std::erase_if(particles, [](const Particle& particle) { return particle.age >= particle.lifetime; });
}

void draw_particles(Canvas& canvas, const std::vector<Particle>& particles, bool glow) {
  for (const auto& particle : particles) {
    const double amount = clamp01(particle.age / particle.lifetime);
    const double radius = particle.radius * (1.0 - amount * 0.7);
    const std::uint32_t color = lerp_color(particle.from, particle.to, amount);
    if (glow) canvas.fill_circle(particle.x, particle.y, radius * 2.2, color, (1.0 - amount) * 0.16);
    canvas.fill_circle(particle.x, particle.y, std::max(0.7, radius), color, 1.0 - amount * amount);
  }
}

class ParticlesScene final {
 public:
  void frame(const Frame& frame) {
    if (frame.input.pointer_inside) {
      mouse_x_ = frame.input.pointer_x;
      mouse_y_ = frame.input.pointer_y;
    }
    const double dt = frame.delta_time;
    fire_vx_ += ((mouse_x_ - fire_x_) * 300.0 - fire_vx_ * 22.0) * dt;
    fire_vy_ += ((mouse_y_ - fire_y_) * 300.0 - fire_vy_ * 22.0) * dt;
    fire_x_ += fire_vx_ * dt;
    fire_y_ += fire_vy_ * dt;
    fire_spawn_ += 300.0 * dt;
    while (fire_spawn_ >= 1.0) {
      fire_spawn_ -= 1.0;
      const double angle = random_.next() * pi * 2.0;
      const double speed = 40.0 + random_.next() * 90.0;
      fire_.push_back({
          .x = fire_x_,
          .y = fire_y_,
          .vx = std::cos(angle) * speed + fire_vx_ * 0.35,
          .vy = std::sin(angle) * speed + fire_vy_ * 0.35,
          .lifetime = 0.2 + random_.next() * 0.35,
          .radius = 3.0 + random_.next() * 8.0,
          .from = 0xffffd8ffU,
          .to = 0x8c0d00ffU,
      });
    }
    snow_spawn_ += 80.0 * dt;
    while (snow_spawn_ >= 1.0) {
      snow_spawn_ -= 1.0;
      snow_.push_back({
          .x = 400.0 + random_.next() * 400.0,
          .y = -4.0,
          .vx = 3.0 + random_.next() * 25.0,
          .vy = 15.0 + random_.next() * 25.0,
          .lifetime = 2.0 + random_.next() * 2.0,
          .radius = 2.0 + random_.next() * 4.0,
          .from = 0xffffffffU,
          .to = 0xb3d1ffffU,
      });
    }
    update_particles(fire_, dt, 0.9, 200.0);
    update_particles(snow_, dt, 0.3, 20.0);
    for (auto& particle : snow_) {
      particle.vx += std::sin(frame.time * 2.0 + particle.y * 0.02) * 8.0 * dt;
    }
    draw_particles(frame.canvas, fire_, true);
    draw_particles(frame.canvas, snow_, false);
    frame.canvas.text(std::to_string(fire_.size() + snow_.size()) + " particles", 8.0, 568.0, 0x999999ffU, 12.0);
    frame.canvas.text("Fire follows the pointer with a spring · snow drifts across the right half", 18.0, 20.0, 0xc4ccdcffU, 13.0);
  }

 private:
  Random random_{0xf12eU};
  std::vector<Particle> fire_;
  std::vector<Particle> snow_;
  double mouse_x_{200.0};
  double mouse_y_{250.0};
  double fire_x_{200.0};
  double fire_y_{250.0};
  double fire_vx_{};
  double fire_vy_{};
  double fire_spawn_{};
  double snow_spawn_{};
};

class ParticleEditorScene final {
 public:
  void frame(const Frame& frame) {
    handle_input(frame);
    const double dt = frame.delta_time;
    spring_vx_ += ((target_x_ - emitter_x_) * 490.0 - spring_vx_ * 28.0) * dt;
    spring_vy_ += ((target_y_ - emitter_y_) * 490.0 - spring_vy_ * 28.0) * dt;
    emitter_x_ += spring_vx_ * dt;
    emitter_y_ += spring_vy_ * dt;
    spawn_ += spawn_rate_ * dt;
    while (spawn_ >= 1.0 && particles_.size() < static_cast<std::size_t>(max_particles_)) {
      spawn_ -= 1.0;
      const double base = std::atan2(direction_y_, direction_x_);
      const double angle = base + (random_.next() - 0.5) * spread_;
      const double speed = speed_min_ + random_.next() * (speed_max_ - speed_min_);
      particles_.push_back({
          .x = emitter_x_,
          .y = emitter_y_,
          .vx = std::cos(angle) * speed,
          .vy = std::sin(angle) * speed,
          .lifetime = lifetime_min_ + random_.next() * (lifetime_max_ - lifetime_min_),
          .radius = 3.0 + random_.next() * 7.0,
          .from = 0xffd966ffU,
          .to = 0x991000ffU,
      });
    }
    update_particles(particles_, dt, drag_, gravity_y_);
    for (auto& particle : particles_) {
      particle.vx += std::sin(particle.y * 0.02 + frame.time * 4.0) * turbulence_ * dt;
    }
    draw_particles(frame.canvas, particles_, additive_);
    draw_panel(frame.canvas);
    frame.canvas.text(std::to_string(particles_.size()) + " particles", 8.0, 568.0, 0x999999ffU, 12.0);
  }

 private:
  struct Control final {
    std::string_view name;
    double* value;
    double minimum;
    double maximum;
  };

  [[nodiscard]] std::array<Control, 8> controls() {
    return {{
        {"Spawn rate", &spawn_rate_, 1.0, 500.0},
        {"Lifetime min", &lifetime_min_, 0.05, 5.0},
        {"Lifetime max", &lifetime_max_, 0.05, 5.0},
        {"Speed min", &speed_min_, 0.0, 400.0},
        {"Speed max", &speed_max_, 0.0, 400.0},
        {"Gravity Y", &gravity_y_, -500.0, 500.0},
        {"Drag", &drag_, 0.0, 3.0},
        {"Turbulence", &turbulence_, 0.0, 200.0},
    }};
  }

  void handle_input(const Frame& frame) {
    if (frame.input.pointer_inside && frame.input.pointer_x < 520.0) {
      target_x_ = frame.input.pointer_x;
      target_y_ = frame.input.pointer_y;
    }
    if (frame.input.pointer_down && frame.input.pointer_x >= 544.0 && frame.input.pointer_x <= 770.0) {
      auto list = controls();
      for (std::size_t index = 0; index < list.size(); ++index) {
        const double slider_y = 92.0 + static_cast<double>(index) * 52.0;
        if (std::abs(frame.input.pointer_y - slider_y) <= 12.0) {
          selected_ = index;
          const double amount = clamp01((frame.input.pointer_x - 544.0) / 226.0);
          *list[index].value = std::lerp(list[index].minimum, list[index].maximum, amount);
          break;
        }
      }
    }
    if (frame.input.pointer_pressed && contains(544.0, 482.0, 180.0, 34.0, frame.input.pointer_x, frame.input.pointer_y)) {
      additive_ = !additive_;
    }
    if (frame.input.key_pressed(SDLK_TAB)) selected_ = (selected_ + 1) % controls().size();
    if (frame.input.key_pressed(SDLK_B)) additive_ = !additive_;
    auto list = controls();
    const double span = list[selected_].maximum - list[selected_].minimum;
    if (frame.input.key_down(SDLK_LEFT)) *list[selected_].value -= span * frame.delta_time * 0.4;
    if (frame.input.key_down(SDLK_RIGHT)) *list[selected_].value += span * frame.delta_time * 0.4;
    *list[selected_].value = std::clamp(*list[selected_].value, list[selected_].minimum, list[selected_].maximum);
  }

  void draw_panel(Canvas& canvas) {
    canvas.fill_rectangle(530.0, 10.0, 258.0, 552.0, 0x1a1a2eeeU);
    canvas.stroke_rectangle(530.0, 10.0, 258.0, 552.0, 0x363653ffU);
    canvas.text("Particle editor controls", 544.0, 24.0, 0xf0f0ffffU, 16.0);
    auto list = controls();
    for (std::size_t index = 0; index < list.size(); ++index) {
      const double y = 70.0 + static_cast<double>(index) * 52.0;
      const auto& control = list[index];
      canvas.text(control.name, 544.0, y, index == selected_ ? 0xffff99ffU : 0xc5cad8ffU, 12.0);
      canvas.text(fixed(*control.value), 724.0, y, 0x8994aaffU, 11.0);
      canvas.fill_rectangle(544.0, y + 22.0, 226.0, 5.0, 0x34384affU);
      const double amount = (*control.value - control.minimum) / (control.maximum - control.minimum);
      canvas.fill_rectangle(544.0, y + 22.0, 226.0 * clamp01(amount), 5.0, 0xff9a3dffU);
    }
    canvas.text(std::string("Blend: ") + (additive_ ? "Add" : "Normal") + "  [B]", 544.0, 496.0, 0xffb36bffU, 12.0);
    canvas.text("Tab selects · arrows edit", 544.0, 524.0, 0x7f899fffU, 11.0);
  }

  Random random_{0x5045U};
  std::vector<Particle> particles_;
  double emitter_x_{350.0};
  double emitter_y_{300.0};
  double target_x_{350.0};
  double target_y_{300.0};
  double spring_vx_{};
  double spring_vy_{};
  double spawn_{};
  double spawn_rate_{120.0};
  double lifetime_min_{0.3};
  double lifetime_max_{0.8};
  double speed_min_{40.0};
  double speed_max_{120.0};
  double spread_{2.0 * pi};
  double direction_x_{};
  double direction_y_{-1.0};
  double gravity_y_{100.0};
  double drag_{0.5};
  double turbulence_{40.0};
  double max_particles_{2000.0};
  std::size_t selected_{};
  bool additive_{true};
};

class PathBooleanScene final {
 public:
  void frame(const Frame& frame) {
    update_drag(frame);
    auto& canvas = frame.canvas;
    canvas.fill_rectangle(399.0, 0.0, 2.0, 600.0, 0xd7dce4ffU);
    canvas.fill_rectangle(0.0, 299.0, 800.0, 2.0, 0xd7dce4ffU);
    constexpr std::array<std::string_view, 4> labels{"Union", "Intersect", "Difference (A - B)", "XOR"};
    constexpr std::array<std::uint32_t, 4> colors{0x2980b9ffU, 0x27ae60ffU, 0xc0392bffU, 0x8e44adffU};
    for (int index = 0; index < 4; ++index) {
      const double origin_x = static_cast<double>(index % 2) * 400.0;
      const double origin_y = static_cast<double>(index / 2) * 300.0;
      canvas.text(labels[static_cast<std::size_t>(index)], origin_x + 10.0, origin_y + 8.0, 0x333333ffU, 14.0);
      for (int y = 48; y < 288; y += 5) {
        for (int x = 5; x < 395; x += 5) {
          const bool in_a = inside_a(static_cast<double>(x), static_cast<double>(y));
          const bool in_b = inside_b(static_cast<double>(x), static_cast<double>(y));
          const bool inside = index == 0 ? in_a || in_b : index == 1 ? in_a && in_b : index == 2 ? in_a && !in_b : in_a != in_b;
          if (inside) canvas.fill_rectangle(origin_x + x, origin_y + y, 5.0, 5.0, colors[static_cast<std::size_t>(index)], 0.7);
        }
      }
      canvas.stroke_circle(origin_x + 200.0 + a_x_, origin_y + 150.0 + a_y_, 80.0, 0x34495effU, 2.0, 0.55);
      canvas.stroke_rectangle(origin_x + 200.0 + b_x_ - 70.0, origin_y + 150.0 + b_y_ - 60.0, 140.0, 120.0, 0xe67e22ffU, 2.0, 0.55);
    }
  }

 private:
  [[nodiscard]] bool inside_a(double x, double y) const {
    const double dx = x - (200.0 + a_x_);
    const double dy = y - (150.0 + a_y_);
    return dx * dx + dy * dy <= 6400.0;
  }

  [[nodiscard]] bool inside_b(double x, double y) const {
    const double left = 200.0 + b_x_ - 70.0;
    const double top = 150.0 + b_y_ - 60.0;
    return x >= left && x <= left + 140.0 && y >= top && y <= top + 120.0;
  }

  void update_drag(const Frame& frame) {
    const double local_x = std::fmod(frame.input.pointer_x, 400.0);
    const double local_y = std::fmod(frame.input.pointer_y, 300.0);
    if (frame.input.pointer_pressed) {
      const double da = std::hypot(local_x - (200.0 + a_x_), local_y - (150.0 + a_y_));
      const double db = std::hypot(local_x - (200.0 + b_x_), local_y - (150.0 + b_y_));
      dragging_ = da < 100.0 && da <= db ? 1 : db < 100.0 ? 2 : 0;
      drag_x_ = local_x;
      drag_y_ = local_y;
      start_x_ = dragging_ == 1 ? a_x_ : b_x_;
      start_y_ = dragging_ == 1 ? a_y_ : b_y_;
    }
    if (frame.input.pointer_down && dragging_ != 0) {
      const double x = start_x_ + local_x - drag_x_;
      const double y = start_y_ + local_y - drag_y_;
      if (dragging_ == 1) {
        a_x_ = x;
        a_y_ = y;
      } else {
        b_x_ = x;
        b_y_ = y;
      }
    }
    if (frame.input.pointer_released) dragging_ = 0;
  }

  double a_x_{-40.0};
  double a_y_{};
  double b_x_{40.0};
  double b_y_{};
  double drag_x_{};
  double drag_y_{};
  double start_x_{};
  double start_y_{};
  int dragging_{};
};

class PlatformerScene final {
 public:
  void frame(const Frame& frame) {
    if ((frame.input.pointer_pressed || frame.input.key_pressed(SDLK_RETURN)) && state_ != State::playing) reset();
    if (frame.smoke && state_ == State::title) reset();
    if (state_ == State::playing) update(frame);
    draw(frame.canvas);
  }

 private:
  struct Platform final {
    double x;
    double y;
    double width;
    double height;
    std::uint32_t color;
  };
  enum class State { title, playing, game_over };

  static constexpr std::array<Platform, 7> platforms_{{
      {-200.0, 400.0, 1600.0, 40.0, 0x4a7c59ffU},
      {100.0, 320.0, 150.0, 16.0, 0x8b6914ffU},
      {350.0, 260.0, 120.0, 16.0, 0x8b6914ffU},
      {550.0, 200.0, 180.0, 16.0, 0x8b6914ffU},
      {780.0, 300.0, 140.0, 16.0, 0x8b6914ffU},
      {980.0, 220.0, 160.0, 16.0, 0x8b6914ffU},
      {50.0, 160.0, 100.0, 16.0, 0x8b6914ffU},
  }};

  void reset() {
    state_ = State::playing;
    player_x_ = 200.0;
    player_y_ = 350.0;
    velocity_y_ = 0.0;
    camera_x_ = 400.0;
    camera_y_ = 250.0;
  }

  void update(const Frame& frame) {
    double velocity_x = 0.0;
    if (frame.input.key_down(SDLK_LEFT) || frame.input.key_down(SDLK_A)) velocity_x = -220.0;
    if (frame.input.key_down(SDLK_RIGHT) || frame.input.key_down(SDLK_D)) velocity_x = 220.0;
    if ((frame.input.key_pressed(SDLK_UP) || frame.input.key_pressed(SDLK_W) || frame.input.key_pressed(SDLK_SPACE)) && grounded_) {
      velocity_y_ = -420.0;
      grounded_ = false;
    }
    const double old_bottom = player_y_ + 32.0;
    velocity_y_ += 980.0 * frame.delta_time;
    player_x_ += velocity_x * frame.delta_time;
    player_y_ += velocity_y_ * frame.delta_time;
    grounded_ = false;
    for (const auto& platform : platforms_) {
      const bool horizontal = player_x_ + 24.0 > platform.x && player_x_ < platform.x + platform.width;
      const double bottom = player_y_ + 32.0;
      if (horizontal && velocity_y_ >= 0.0 && old_bottom <= platform.y && bottom >= platform.y) {
        player_y_ = platform.y - 32.0;
        velocity_y_ = 0.0;
        grounded_ = true;
      }
    }
    if (player_y_ > 600.0) state_ = State::game_over;
    const double target_x = player_x_ + 12.0;
    const double target_y = player_y_ + 16.0;
    const double smoothing = 1.0 - std::exp(-frame.delta_time / 0.15);
    camera_x_ += (target_x - camera_x_) * smoothing;
    camera_y_ += (target_y - camera_y_) * smoothing;
  }

  void draw(Canvas& canvas) const {
    canvas.horizontal_gradient(0.0, 0.0, 800.0, 600.0, std::array<std::uint32_t, 3>{0x17233affU, 0x253a58ffU, 0x101827ffU});
    if (state_ == State::title) {
      const std::string title = "PLATFORMER";
      canvas.text(title, 400.0 - canvas.text_width(title, 48.0) / 2.0, 150.0, 0xffffffffU, 48.0);
      const std::string subtitle = "Click to Play";
      canvas.text(subtitle, 400.0 - canvas.text_width(subtitle, 24.0) / 2.0, 260.0, 0xddddddffU, 24.0);
      return;
    }
    const double offset_x = 400.0 - camera_x_;
    const double offset_y = 250.0 - camera_y_;
    for (const auto& platform : platforms_) {
      canvas.fill_rectangle(platform.x + offset_x, platform.y + offset_y, platform.width, platform.height, platform.color);
      canvas.fill_rectangle(platform.x + offset_x, platform.y + offset_y, platform.width, 3.0, 0xc89b35ffU, 0.75);
    }
    canvas.fill_rectangle(player_x_ + offset_x, player_y_ + offset_y, 24.0, 32.0, 0xdd3333ffU);
    canvas.fill_rectangle(player_x_ + offset_x + 5.0, player_y_ + offset_y + 4.0, 14.0, 9.0, 0xffd7b5ffU);
    canvas.text("A/D or arrows move · W/Up/Space jumps", 18.0, 18.0, 0xffffffffU, 13.0);
    if (state_ == State::game_over) {
      canvas.fill_rectangle(0.0, 0.0, 800.0, 600.0, 0x000000ffU, 0.55);
      const std::string message = "Game Over - Click to Restart";
      canvas.text(message, 400.0 - canvas.text_width(message, 32.0) / 2.0, 210.0, 0xff5555ffU, 32.0);
    }
  }

  State state_{State::title};
  double player_x_{200.0};
  double player_y_{350.0};
  double velocity_y_{};
  double camera_x_{400.0};
  double camera_y_{250.0};
  bool grounded_{};
};

class SnapshotScene final {
 public:
  SnapshotScene() {
    state_.items = initial_items();
    save();
  }

  void frame(const Frame& frame) {
    handle_keys(frame);
    if (!interpolating_) update(frame.delta_time);
    else {
      interpolation_ = std::min(1.0, interpolation_ + frame.delta_time);
      if (slots_[0].valid) {
        display_ = blend(interpolation_source_, slots_[0], interpolation_);
      }
      if (interpolation_ >= 1.0) {
        state_ = slots_[0];
        interpolating_ = false;
        status_ = "Interpolation complete";
      }
    }
    if (!interpolating_) display_ = state_;
    status_age_ += frame.delta_time;
    draw(frame.canvas);
  }

 private:
  struct Item final { double x{}; double y{}; bool collected{}; };
  struct State final {
    double x{400.0};
    double y{230.0};
    double rotation{};
    double time{};
    int score{};
    std::array<Item, 6> items{};
    bool valid{};
  };

  static std::array<Item, 6> initial_items() {
    return {{{170.0, 120.0, false}, {280.0, 330.0, false}, {390.0, 105.0, false}, {505.0, 330.0, false}, {620.0, 130.0, false}, {400.0, 220.0, false}}};
  }

  void show(std::string message) {
    status_ = std::move(message);
    status_age_ = 0.0;
  }

  void save() {
    state_.valid = true;
    slots_[next_slot_] = state_;
    show("Saved to slot " + std::to_string(next_slot_ + 1));
    next_slot_ = (next_slot_ + 1) % slots_.size();
  }

  void restore(std::size_t slot) {
    if (!slots_[slot].valid) {
      show("Slot " + std::to_string(slot + 1) + " is empty");
      return;
    }
    state_ = slots_[slot];
    interpolating_ = false;
    show("Restored slot " + std::to_string(slot + 1));
  }

  void handle_keys(const Frame& frame) {
    if (frame.input.key_pressed(SDLK_S)) save();
    if (frame.input.key_pressed(SDLK_R)) restore((next_slot_ + slots_.size() - 1) % slots_.size());
    constexpr std::array<SDL_Keycode, 5> keys{SDLK_1, SDLK_2, SDLK_3, SDLK_4, SDLK_5};
    for (std::size_t index = 0; index < keys.size(); ++index) if (frame.input.key_pressed(keys[index])) restore(index);
    if (frame.input.key_pressed(SDLK_I)) {
      if (!slots_[0].valid) show("Slot 1 is empty -- save first");
      else {
        interpolation_source_ = state_;
        interpolation_ = 0.0;
        interpolating_ = true;
        show("Interpolating to slot 1...");
      }
    }
  }

  void update(double dt) {
    state_.time += dt;
    const double angle = state_.time * 0.3;
    state_.x = 400.0 + std::cos(angle) * 120.0;
    state_.y = 210.0 + std::sin(angle) * 120.0;
    state_.rotation = angle + pi / 2.0;
    for (auto& item : state_.items) {
      if (!item.collected && std::hypot(state_.x - item.x, state_.y - item.y) < 30.0) {
        item.collected = true;
        state_.score += 100;
      }
    }
  }

  static State blend(const State& from, const State& to, double amount) {
    State output = amount < 0.5 ? from : to;
    output.x = std::lerp(from.x, to.x, amount);
    output.y = std::lerp(from.y, to.y, amount);
    output.rotation = std::lerp(from.rotation, to.rotation, amount);
    output.score = static_cast<int>(std::lround(std::lerp(static_cast<double>(from.score), static_cast<double>(to.score), amount)));
    return output;
  }

  void draw(Canvas& canvas) const {
    for (const auto& item : display_.items) if (!item.collected) {
      canvas.fill_circle(item.x, item.y, 10.0, 0xffcc33ffU);
      canvas.stroke_circle(item.x, item.y, 14.0, 0xffe58affU, 1.0, 0.4);
    }
    const double cosine = std::cos(display_.rotation);
    const double sine = std::sin(display_.rotation);
    const std::array<Point, 3> player{{
        {display_.x + cosine * 18.0, display_.y + sine * 18.0},
        {display_.x + std::cos(display_.rotation + 2.4) * 18.0, display_.y + std::sin(display_.rotation + 2.4) * 18.0},
        {display_.x + std::cos(display_.rotation - 2.4) * 18.0, display_.y + std::sin(display_.rotation - 2.4) * 18.0},
    }};
    canvas.fill_polygon(player, 0x44aaffffU);
    canvas.fill_rectangle(0.0, 420.0, 800.0, 180.0, 0x10151fffU);
    canvas.text("Score: " + std::to_string(display_.score), 20.0, 440.0, 0xffffffffU, 16.0);
    canvas.text("[S] Save  [R] Restore  [1-5] Load slot  [I] Interpolate to slot 1", 20.0, 470.0, 0x999999ffU, 13.0);
    if (status_age_ < 2.0) canvas.text(status_, 400.0, 440.0, 0x44cc88ffU, 14.0);
    for (std::size_t index = 0; index < slots_.size(); ++index) {
      const double x = 650.0 + static_cast<double>(index) * 24.0;
      if (slots_[index].valid) canvas.fill_circle(x, 452.0, 8.0, 0x44cc88ffU);
      else canvas.stroke_circle(x, 452.0, 8.0, 0x555555ffU, 2.0);
    }
  }

  State state_;
  State display_;
  State interpolation_source_;
  std::array<State, 5> slots_{};
  std::size_t next_slot_{};
  double interpolation_{};
  double status_age_{};
  bool interpolating_{};
  std::string status_;
};

class SpringScene final {
 public:
  void frame(const Frame& frame) {
    if (frame.input.pointer_pressed) {
      if (frame.input.pointer_x < 600.0 && frame.input.pointer_y < 400.0) {
        target_x_ = frame.input.pointer_x;
        target_y_ = frame.input.pointer_y;
        tween_from_x_ = tween_x_;
        tween_from_y_ = tween_y_;
        tween_time_ = 0.0;
      } else if (frame.input.pointer_x >= 610.0 && frame.input.pointer_x <= 788.0) {
        const int preset = static_cast<int>((frame.input.pointer_y - 52.0) / 29.0);
        apply_preset(preset);
      }
    }
    constexpr std::array<SDL_Keycode, 5> keys{SDLK_1, SDLK_2, SDLK_3, SDLK_4, SDLK_5};
    for (std::size_t index = 0; index < keys.size(); ++index) if (frame.input.key_pressed(keys[index])) apply_preset(static_cast<int>(index));
    update_spring(frame.delta_time);
    tween_time_ += frame.delta_time;
    const double amount = ease_in_out_cubic(clamp01(tween_time_ / 0.9));
    tween_x_ = std::lerp(tween_from_x_, target_x_, amount);
    tween_y_ = std::lerp(tween_from_y_, target_y_ + 22.0, amount);
    draw(frame.canvas);
  }

 private:
  void apply_preset(int index) {
    constexpr std::array<std::pair<double, double>, 5> presets{{{3.0, 0.3}, {3.0, 1.0}, {3.0, 3.0}, {5.0, 0.15}, {8.0, 0.6}}};
    if (index < 0 || index >= static_cast<int>(presets.size())) return;
    frequency_ = presets[static_cast<std::size_t>(index)].first;
    damping_ = presets[static_cast<std::size_t>(index)].second;
  }

  void update_spring(double dt) {
    const double omega = 2.0 * pi * frequency_;
    const auto axis = [dt, omega, this](double target, double& value, double& velocity) {
      const double acceleration = omega * omega * (target - value) - 2.0 * damping_ * omega * velocity;
      velocity += acceleration * dt;
      value += velocity * dt;
    };
    axis(target_x_, spring_x_, spring_vx_);
    axis(target_y_ - 22.0, spring_y_, spring_vy_);
  }

  void draw(Canvas& canvas) const {
    canvas.fill_rectangle(0.0, 0.0, 600.0, 400.0, 0xf7f9fcffU);
    canvas.line(target_x_ - 10.0, target_y_, target_x_ + 10.0, target_y_, 0x999999ffU, 1.5);
    canvas.line(target_x_, target_y_ - 10.0, target_x_, target_y_ + 10.0, 0x999999ffU, 1.5);
    canvas.fill_circle(spring_x_, spring_y_, 18.0, 0x2196f3ffU);
    canvas.fill_circle(tween_x_, tween_y_, 18.0, 0xff9800ffU);
    canvas.fill_rectangle(12.0, 372.0, 12.0, 12.0, 0x2196f3ffU);
    canvas.text("Spring (2nd-order, overshoots)", 30.0, 369.0, 0x314156ffU, 12.0);
    canvas.fill_rectangle(270.0, 372.0, 12.0, 12.0, 0xff9800ffU);
    canvas.text("Tween (fixed duration)", 288.0, 369.0, 0x314156ffU, 12.0);
    canvas.fill_rectangle(610.0, 12.0, 178.0, 250.0, 0xfffffff2U);
    canvas.stroke_rectangle(610.0, 12.0, 178.0, 250.0, 0xc5ced9ffU);
    canvas.text("Spring presets", 624.0, 26.0, 0x314156ffU, 15.0);
    canvas.text("1  Underdamped", 624.0, 62.0, 0x52647affU, 12.0);
    canvas.text("2  Critical", 624.0, 91.0, 0x52647affU, 12.0);
    canvas.text("3  Overdamped", 624.0, 120.0, 0x52647affU, 12.0);
    canvas.text("4  Very bouncy", 624.0, 149.0, 0x52647affU, 12.0);
    canvas.text("5  Snappy", 624.0, 178.0, 0x52647affU, 12.0);
    canvas.text(fixed(frequency_) + " Hz", 624.0, 215.0, 0x68788dffU, 12.0);
    canvas.text("damping " + fixed(damping_, 2), 624.0, 235.0, 0x68788dffU, 12.0);
    canvas.text("Click the scene to move the target", 18.0, 430.0, 0x68788dffU, 13.0);
  }

  double frequency_{3.0};
  double damping_{0.3};
  double target_x_{470.0};
  double target_y_{200.0};
  double spring_x_{300.0};
  double spring_y_{178.0};
  double spring_vx_{};
  double spring_vy_{};
  double tween_x_{300.0};
  double tween_y_{222.0};
  double tween_from_x_{300.0};
  double tween_from_y_{222.0};
  double tween_time_{};
};

class SpritesheetScene final {
 public:
  void frame(const Frame& frame) {
    time_ += frame.delta_time;
    auto& canvas = frame.canvas;
    canvas.horizontal_gradient(0.0, 0.0, 800.0, 600.0, std::array<std::uint32_t, 4>{0x071126ffU, 0x102d4dffU, 0x203b58ffU, 0x081529ffU});
    canvas.fill_circle(400.0, 250.0, 220.0, 0x66ccffffU, 0.08);
    for (int index = 0; index < 70; ++index) {
      const double x = std::fmod(static_cast<double>(index * 97), 800.0);
      const double y = std::fmod(static_cast<double>(index * 53), 360.0);
      canvas.fill_circle(x, y, 1.2, 0xcbeeffffU, 0.3 + static_cast<double>(index % 5) * 0.1);
    }
    canvas.text("Spritesheet playback", 28.0, 24.0, 0xffffffffU, 26.0);
    canvas.text("same 12-frame strip · independent players", 29.0, 58.0, 0xa4b9d3ffU, 13.0);
    draw_creature(canvas, 164.0, 330.0, time_ * 12.5, 0x58d9ffffU);
    draw_creature(canvas, 400.0, 330.0, time_ * 20.6 + 3.0, 0xffba68ffU);
    const double ping = 5.5 + std::sin(time_ * 5.3) * 5.5;
    draw_creature(canvas, 636.0, 330.0, ping, 0xbe88ffffU);
    canvas.text("loop · 1.00x", 112.0, 475.0, 0xd5e6f9ffU, 14.0);
    canvas.text("loop · 1.65x", 348.0, 475.0, 0xd5e6f9ffU, 14.0);
    canvas.text("ping-pong · 0.85x", 566.0, 475.0, 0xd5e6f9ffU, 14.0);
    canvas.text("Frames are selected from one generated 3072 × 256 strip.", 192.0, 535.0, 0x7088a5ffU, 13.0);
  }

 private:
  static void draw_creature(Canvas& canvas, double x, double y, double frame, std::uint32_t tint) {
    const double phase = frame / 12.0 * 2.0 * pi;
    const double wing = 0.45 + std::sin(phase) * 0.42;
    canvas.fill_circle(x, y, 92.0, tint, 0.08);
    fill_rotated_rectangle(canvas, x - 43.0, y - 5.0, 78.0, 28.0, -wing, tint, 0.62);
    fill_rotated_rectangle(canvas, x + 43.0, y - 5.0, 78.0, 28.0, wing, tint, 0.62);
    canvas.fill_ellipse(x, y, 24.0, 58.0, 0x17243bffU);
    canvas.fill_circle(x, y - 51.0, 20.0, tint);
    canvas.fill_circle(x - 7.0, y - 55.0, 3.0, 0xffffffffU);
    canvas.fill_circle(x + 7.0, y - 55.0, 3.0, 0xffffffffU);
    canvas.line(x - 8.0, y - 66.0, x - 20.0, y - 86.0, tint, 2.0);
    canvas.line(x + 8.0, y - 66.0, x + 20.0, y - 86.0, tint, 2.0);
  }

  double time_{};
};

class TilemapScene final {
 public:
  void frame(const Frame& frame) {
    update_camera(frame);
    draw(frame.canvas);
  }

 private:
  static constexpr int columns = 72;
  static constexpr int rows = 54;
  static constexpr double tile = 32.0;
  static constexpr double map_width = static_cast<double>(columns) * tile;
  static constexpr double map_height = static_cast<double>(rows) * tile;

  [[nodiscard]] static int terrain(int column, int row) {
    if (row < 3 || row >= rows - 3) return 5;
    if (column < 2 || column >= columns - 2) return 3;
    if (row > 20 && row < 25 && column > 10 && column < 60) return 4;
    if (row > 30 && row < 40 && column > 20 && column < 50) return 1;
    if ((row == 30 || row == 40) && column > 20 && column < 50) return 2;
    if (row > 10 && row < 15 && column > 40 && column < 55) return 7;
    if (row > 45 && row < 48 && column > 5 && column < 15) return 6;
    return (column * 13 + row * 7) % 29 == 0 ? 7 : 0;
  }

  void constrain() {
    const double half_width = 400.0 / zoom_;
    const double half_height = 300.0 / zoom_;
    camera_x_ = half_width * 2.0 >= map_width ? map_width / 2.0 : std::clamp(camera_x_, half_width, map_width - half_width);
    camera_y_ = half_height * 2.0 >= map_height ? map_height / 2.0 : std::clamp(camera_y_, half_height, map_height - half_height);
  }

  void update_camera(const Frame& frame) {
    const double speed = 520.0 * frame.delta_time / zoom_;
    if (frame.input.key_down(SDLK_LEFT) || frame.input.key_down(SDLK_A)) camera_x_ -= speed;
    if (frame.input.key_down(SDLK_RIGHT) || frame.input.key_down(SDLK_D)) camera_x_ += speed;
    if (frame.input.key_down(SDLK_UP) || frame.input.key_down(SDLK_W)) camera_y_ -= speed;
    if (frame.input.key_down(SDLK_DOWN) || frame.input.key_down(SDLK_S)) camera_y_ += speed;
    if (frame.input.pointer_pressed) {
      dragging_ = true;
      previous_x_ = frame.input.pointer_x;
      previous_y_ = frame.input.pointer_y;
    }
    if (dragging_ && frame.input.pointer_down) {
      camera_x_ -= (frame.input.pointer_x - previous_x_) / zoom_;
      camera_y_ -= (frame.input.pointer_y - previous_y_) / zoom_;
      previous_x_ = frame.input.pointer_x;
      previous_y_ = frame.input.pointer_y;
    }
    if (frame.input.pointer_released) dragging_ = false;
    if (frame.input.wheel_y != 0.0) {
      const double before_x = camera_x_ + (frame.input.pointer_x - 400.0) / zoom_;
      const double before_y = camera_y_ + (frame.input.pointer_y - 300.0) / zoom_;
      const double factor = frame.input.wheel_y > 0.0 ? 1.12 : 1.0 / 1.12;
      zoom_ = std::clamp(zoom_ * factor, 0.55, 2.4);
      camera_x_ = before_x - (frame.input.pointer_x - 400.0) / zoom_;
      camera_y_ = before_y - (frame.input.pointer_y - 300.0) / zoom_;
    }
    if (frame.input.pointer_inside) {
      pointer_x_ = frame.input.pointer_x;
      pointer_y_ = frame.input.pointer_y;
    }
    constrain();
  }

  void draw(Canvas& canvas) const {
    constexpr std::array<std::uint32_t, 8> colors{0x4b9b59ffU, 0x397bb8ffU, 0xc9ae6affU, 0x626870ffU, 0xa77b48ffU, 0xddebf3ffU, 0xd94d36ffU, 0x23613bffU};
    constexpr std::array<std::string_view, 8> names{"meadow", "water", "shore", "rock", "trail", "snow", "lava", "forest"};
    const double left = camera_x_ - 400.0 / zoom_;
    const double top = camera_y_ - 300.0 / zoom_;
    const int first_column = std::max(0, static_cast<int>(std::floor(left / tile)));
    const int last_column = std::min(columns - 1, static_cast<int>(std::ceil((camera_x_ + 400.0 / zoom_) / tile)));
    const int first_row = std::max(0, static_cast<int>(std::floor(top / tile)));
    const int last_row = std::min(rows - 1, static_cast<int>(std::ceil((camera_y_ + 300.0 / zoom_) / tile)));
    for (int row = first_row; row <= last_row; ++row) {
      for (int column = first_column; column <= last_column; ++column) {
        const int value = terrain(column, row);
        const double x = (static_cast<double>(column) * tile - camera_x_) * zoom_ + 400.0;
        const double y = (static_cast<double>(row) * tile - camera_y_) * zoom_ + 300.0;
        const double size = tile * zoom_ + 0.5;
        canvas.fill_rectangle(x, y, size, size, colors[static_cast<std::size_t>(value)]);
        if (value == 1) canvas.line(x, y + size * 0.5, x + size, y + size * 0.5, 0x8bc7e8ffU, 1.0, 0.45);
        if (value == 7) canvas.fill_circle(x + size * 0.5, y + size * 0.5, size * 0.24, 0x163f29ffU);
      }
    }
    const double world_x = camera_x_ + (pointer_x_ - 400.0) / zoom_;
    const double world_y = camera_y_ + (pointer_y_ - 300.0) / zoom_;
    const int column = static_cast<int>(std::floor(world_x / tile));
    const int row = static_cast<int>(std::floor(world_y / tile));
    std::string selected = "no tile selected";
    if (column >= 0 && column < columns && row >= 0 && row < rows) {
      const double x = (static_cast<double>(column) * tile - camera_x_) * zoom_ + 400.0;
      const double y = (static_cast<double>(row) * tile - camera_y_) * zoom_ + 300.0;
      canvas.stroke_rectangle(x, y, tile * zoom_, tile * zoom_, 0xffff66ffU, 3.0);
      selected = std::string(names[static_cast<std::size_t>(terrain(column, row))]) + " (" + std::to_string(column) + ", " + std::to_string(row) + ")";
    }
    canvas.fill_rectangle(0.0, 0.0, 800.0, 32.0, 0x07101dddU);
    canvas.text("WASD / arrows pan · drag to explore · wheel zoom · " + selected, 12.0, 8.0, 0xffffffffU, 12.0);
  }

  double camera_x_{map_width * 0.5};
  double camera_y_{map_height * 0.48};
  double zoom_{1.0};
  double pointer_x_{400.0};
  double pointer_y_{300.0};
  double previous_x_{};
  double previous_y_{};
  bool dragging_{};
};

class TweenScene final {
 public:
  void frame(const Frame& frame) {
    elapsed_ += frame.delta_time * 1000.0;
    const double cycle = 3200.0 + 14.0 * 110.0 + 900.0;
    if (elapsed_ >= cycle) elapsed_ = std::fmod(elapsed_, cycle);
    constexpr std::array<std::string_view, 15> names{
        "easeInQuadratic", "easeOutQuadratic", "easeInOutQuadratic",
        "easeInCubic", "easeOutCubic", "easeInOutCubic",
        "easeInSine", "easeOutSine", "easeInOutSine",
        "easeInExponential", "easeOutExponential", "easeInOutExponential",
        "easeInElastic", "easeOutElastic", "easeOutBounce"};
    for (int index = 0; index < 15; ++index) {
      const double cell_x = static_cast<double>(index % 3) * (800.0 / 3.0);
      const double cell_y = static_cast<double>(index / 3) * 120.0;
      frame.canvas.text(names[static_cast<std::size_t>(index)], cell_x + 10.0, cell_y + 8.0, 0xccccccffU, 13.0);
      const double start = cell_x + 20.0;
      const double end = cell_x + 800.0 / 3.0 - 20.0;
      const double y = cell_y + 74.0;
      frame.canvas.line(start, y, end, y, 0x354157ffU, 2.0);
      const double local = clamp01((elapsed_ - static_cast<double>(index) * 110.0) / 3200.0);
      frame.canvas.fill_circle(std::lerp(start, end, ease(index, local)), y, 8.0, 0x44aaeeffU);
    }
  }

 private:
  [[nodiscard]] static double ease(int index, double x) {
    switch (index) {
      case 0: return x * x;
      case 1: return 1.0 - (1.0 - x) * (1.0 - x);
      case 2: return x < 0.5 ? 2.0 * x * x : 1.0 - std::pow(-2.0 * x + 2.0, 2.0) / 2.0;
      case 3: return x * x * x;
      case 4: return 1.0 - std::pow(1.0 - x, 3.0);
      case 5: return ease_in_out_cubic(x);
      case 6: return 1.0 - std::cos(x * pi / 2.0);
      case 7: return std::sin(x * pi / 2.0);
      case 8: return -(std::cos(pi * x) - 1.0) / 2.0;
      case 9: return x == 0.0 ? 0.0 : std::pow(2.0, 10.0 * x - 10.0);
      case 10: return x == 1.0 ? 1.0 : 1.0 - std::pow(2.0, -10.0 * x);
      case 11:
        if (x == 0.0 || x == 1.0) return x;
        return x < 0.5 ? std::pow(2.0, 20.0 * x - 10.0) * std::sin((20.0 * x - 11.125) * 2.0 * pi / 4.5) / -2.0
                       : std::pow(2.0, -20.0 * x + 10.0) * std::sin((20.0 * x - 11.125) * 2.0 * pi / 4.5) / 2.0 + 1.0;
      case 12:
        if (x == 0.0 || x == 1.0) return x;
        return -std::pow(2.0, 10.0 * x - 10.0) * std::sin((x * 10.0 - 10.75) * 2.0 * pi / 3.0);
      case 13:
        if (x == 0.0 || x == 1.0) return x;
        return std::pow(2.0, -10.0 * x) * std::sin((x * 10.0 - 0.75) * 2.0 * pi / 3.0) + 1.0;
      default: {
        constexpr double n1 = 7.5625;
        constexpr double d1 = 2.75;
        if (x < 1.0 / d1) return n1 * x * x;
        if (x < 2.0 / d1) { x -= 1.5 / d1; return n1 * x * x + 0.75; }
        if (x < 2.5 / d1) { x -= 2.25 / d1; return n1 * x * x + 0.9375; }
        x -= 2.625 / d1;
        return n1 * x * x + 0.984375;
      }
    }
  }

  double elapsed_{};
};

class VideoScene final {
 public:
  void frame(const Frame& frame) {
    time_ += frame.delta_time;
    auto& canvas = frame.canvas;
    canvas.text("Video textures", 28.0, 18.0, 0xffffffffU, 24.0);
    canvas.text("Three independently advancing views of a generated video source", 28.0, 50.0, 0x93a2baffU, 13.0);
    draw_video(canvas, 40.0, 90.0, 300.0, 170.0, time_, 0.0);
    draw_video(canvas, 400.0, 90.0, 330.0, 187.0, time_ * 0.75, 0.0);
    draw_video(canvas, 220.0, 330.0, 330.0, 190.0, time_ * 1.2, 0.17);
    canvas.text("1.00×", 42.0, 270.0, 0xbac6d9ffU, 13.0);
    canvas.text("0.75× · alpha 0.8", 402.0, 286.0, 0xbac6d9ffU, 13.0);
    canvas.text("1.20× · rotated 10°", 224.0, 535.0, 0xbac6d9ffU, 13.0);
  }

 private:
  static void draw_video(Canvas& canvas, double x, double y, double width, double height, double time, double rotation) {
    const double center_x = x + width / 2.0;
    const double center_y = y + height / 2.0;
    fill_rotated_rectangle(canvas, center_x, center_y, width, height, rotation, lerp_color(0x124477ffU, 0xb33b72ffU, (std::sin(time) + 1.0) / 2.0));
    for (int index = 0; index < 8; ++index) {
      const double amount = static_cast<double>(index) / 7.0;
      const double px = x + 30.0 + amount * (width - 60.0);
      const double py = center_y + std::sin(time * 2.0 + amount * pi * 3.0) * height * 0.28;
      canvas.fill_circle(px, py, 8.0 + static_cast<double>(index % 3) * 4.0, lerp_color(0xffdb66ffU, 0x63e6beffU, amount), 0.85);
    }
    canvas.stroke_rectangle(x, y, width, height, 0xdde8ffffU, 2.0, 0.45);
  }

  double time_{};
};

} // namespace

int run_benchmark(int argc, char** argv) { return run_scene<BenchmarkScene>(argc, argv, "Flight benchmark - SDL", 0x101522ffU); }
int run_bitmapfont_generate(int argc, char** argv) { return run_scene<BitmapFontScene>(argc, argv, "Flight bitmapfont generate - SDL", 0x101522ffU); }
int run_cross_backend_embed(int argc, char** argv) { return run_scene<CrossBackendScene>(argc, argv, "Flight cross-backend embed - SDL", 0x0d1322ffU); }
int run_formatloading(int argc, char** argv) { return run_scene<FormatLoadingScene>(argc, argv, "Flight format loading - SDL", 0x0e1525ffU); }
int run_particleeditor(int argc, char** argv) { return run_scene<ParticleEditorScene>(argc, argv, "Flight particle editor - SDL", 0x111122ffU); }
int run_particles(int argc, char** argv) { return run_scene<ParticlesScene>(argc, argv, "Flight particles - SDL", 0x101020ffU); }
int run_pathboolean(int argc, char** argv) { return run_scene<PathBooleanScene>(argc, argv, "Flight path boolean - SDL", 0xf7f8faffU); }
int run_platformer(int argc, char** argv) { return run_scene<PlatformerScene>(argc, argv, "Flight platformer - SDL", 0x17233affU); }
int run_snapshot(int argc, char** argv) { return run_scene<SnapshotScene>(argc, argv, "Flight snapshot - SDL", 0x111827ffU); }
int run_spring(int argc, char** argv) { return run_scene<SpringScene>(argc, argv, "Flight spring - SDL", 0xe9eef5ffU); }
int run_spritesheet(int argc, char** argv) { return run_scene<SpritesheetScene>(argc, argv, "Flight spritesheet - SDL", 0x071126ffU); }
int run_tilemap(int argc, char** argv) { return run_scene<TilemapScene>(argc, argv, "Flight tilemap - SDL", 0x172b1fffU); }
int run_tween(int argc, char** argv) { return run_scene<TweenScene>(argc, argv, "Flight tween - SDL", 0x111827ffU); }
int run_video(int argc, char** argv) { return run_scene<VideoScene>(argc, argv, "Flight video - SDL", 0x101522ffU); }

} // namespace flight::examples
