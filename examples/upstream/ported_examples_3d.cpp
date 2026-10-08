#include "ported_examples.hpp"
#include "sdl_host.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <numbers>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace flight::examples {
namespace {

constexpr double pi = std::numbers::pi_v<double>;

struct Vec3 final { double x{}; double y{}; double z{}; };
struct Projected final { double x{}; double y{}; double depth{}; double scale{}; bool visible{}; };

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

class Orbit final {
 public:
  Orbit(double azimuth, double polar, double distance)
      : azimuth_(azimuth), polar_(polar), distance_(distance) {}

  void update(const Frame& frame, bool automatic = false) {
    if (frame.input.pointer_pressed) {
      dragging_ = true;
      previous_x_ = frame.input.pointer_x;
      previous_y_ = frame.input.pointer_y;
    }
    if (dragging_ && frame.input.pointer_down) {
      azimuth_ -= (frame.input.pointer_x - previous_x_) * 0.008;
      polar_ = std::clamp(polar_ + (frame.input.pointer_y - previous_y_) * 0.008, -1.1, 1.1);
      previous_x_ = frame.input.pointer_x;
      previous_y_ = frame.input.pointer_y;
    }
    if (frame.input.pointer_released) dragging_ = false;
    if (frame.input.wheel_y != 0.0) {
      distance_ = std::clamp(distance_ * (frame.input.wheel_y > 0.0 ? 0.9 : 1.1), 3.5, 16.0);
    }
    if (automatic && !dragging_) azimuth_ += frame.delta_time * 0.035;
  }

  [[nodiscard]] Projected project(Vec3 point, Vec3 target = {}) const {
    point.x -= target.x;
    point.y -= target.y;
    point.z -= target.z;
    const double ca = std::cos(azimuth_);
    const double sa = std::sin(azimuth_);
    const double x = point.x * ca - point.z * sa;
    const double z = point.x * sa + point.z * ca;
    const double cp = std::cos(polar_);
    const double sp = std::sin(polar_);
    const double y = point.y * cp - z * sp;
    const double depth = point.y * sp + z * cp + distance_;
    if (depth <= 0.1) return {};
    const double scale = 420.0 / depth;
    return {.x = 400.0 + x * scale, .y = 300.0 - y * scale, .depth = depth, .scale = scale, .visible = true};
  }

 private:
  double azimuth_{};
  double polar_{};
  double distance_{};
  double previous_x_{};
  double previous_y_{};
  bool dragging_{};
};

template <typename Scene>
int run_scene(int argc, char** argv, std::string_view title, std::uint32_t background = 0x080d18ffU) {
  try {
    auto scene = std::make_shared<Scene>();
    return SdlExample::run(argc, argv, title, [scene](const Frame& frame) { scene->frame(frame); }, background);
  } catch (const std::exception& error) {
    std::cerr << title << " failed: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}

void draw_ground(Canvas& canvas, const Orbit& orbit, double extent, double y, std::uint32_t color) {
  for (int step = -6; step <= 6; ++step) {
    const double coordinate = extent * static_cast<double>(step) / 6.0;
    const auto a = orbit.project({-extent, y, coordinate});
    const auto b = orbit.project({extent, y, coordinate});
    const auto c = orbit.project({coordinate, y, -extent});
    const auto d = orbit.project({coordinate, y, extent});
    if (a.visible && b.visible) canvas.line(a.x, a.y, b.x, b.y, color, 1.0, 0.35);
    if (c.visible && d.visible) canvas.line(c.x, c.y, d.x, d.y, color, 1.0, 0.35);
  }
}

void draw_box(Canvas& canvas, const Orbit& orbit, Vec3 center, double size, std::uint32_t color) {
  std::array<Projected, 8> p{};
  for (int index = 0; index < 8; ++index) {
    p[static_cast<std::size_t>(index)] = orbit.project({
        center.x + ((index & 1) != 0 ? size : -size),
        center.y + ((index & 2) != 0 ? size : -size),
        center.z + ((index & 4) != 0 ? size : -size),
    });
  }
  constexpr std::array<std::array<int, 4>, 3> faces{{{{0, 1, 3, 2}}, {{4, 5, 7, 6}}, {{2, 3, 7, 6}}}};
  for (std::size_t face = 0; face < faces.size(); ++face) {
    std::array<Point, 4> points{};
    bool visible = true;
    for (std::size_t point = 0; point < 4; ++point) {
      const auto& projected = p[static_cast<std::size_t>(faces[face][point])];
      visible = visible && projected.visible;
      points[point] = {projected.x, projected.y};
    }
    if (visible) canvas.fill_polygon(points, face == 0 ? lerp_color(color, 0x000000ffU, 0.25) : face == 1 ? color : lerp_color(color, 0xffffffffU, 0.15), 0.95);
  }
  constexpr std::array<std::pair<int, int>, 12> edges{{{0,1},{0,2},{0,4},{1,3},{1,5},{2,3},{2,6},{3,7},{4,5},{4,6},{5,7},{6,7}}};
  for (const auto [from, to] : edges) if (p[static_cast<std::size_t>(from)].visible && p[static_cast<std::size_t>(to)].visible) {
    canvas.line(p[static_cast<std::size_t>(from)].x, p[static_cast<std::size_t>(from)].y, p[static_cast<std::size_t>(to)].x, p[static_cast<std::size_t>(to)].y, 0xffffffffU, 1.0, 0.25);
  }
}

void draw_sphere(Canvas& canvas, const Orbit& orbit, Vec3 center, double radius, std::uint32_t color, bool selected = false) {
  const auto point = orbit.project(center);
  if (!point.visible) return;
  const double screen_radius = radius * point.scale;
  canvas.fill_circle(point.x, point.y, screen_radius, color);
  canvas.fill_circle(point.x - screen_radius * 0.28, point.y - screen_radius * 0.32, screen_radius * 0.28, 0xffffffffU, 0.24);
  canvas.stroke_circle(point.x, point.y, screen_radius, selected ? 0x7ef7c8ffU : 0x101827ffU, selected ? 4.0 : 1.0, selected ? 1.0 : 0.45);
}

void draw_cone(Canvas& canvas, const Orbit& orbit, Vec3 center, double radius, double height, std::uint32_t color) {
  const auto tip = orbit.project({center.x, center.y + height / 2.0, center.z});
  const auto left = orbit.project({center.x - radius, center.y - height / 2.0, center.z});
  const auto right = orbit.project({center.x + radius, center.y - height / 2.0, center.z});
  if (!tip.visible || !left.visible || !right.visible) return;
  const std::array<Point, 3> points{{{tip.x, tip.y}, {left.x, left.y}, {right.x, right.y}}};
  canvas.fill_polygon(points, color);
  canvas.fill_ellipse((left.x + right.x) / 2.0, (left.y + right.y) / 2.0, std::abs(right.x - left.x) / 2.0, std::abs(right.x - left.x) / 5.0, lerp_color(color, 0x000000ffU, 0.25));
}

void draw_orbit_help(Canvas& canvas, std::string_view title, std::string_view detail) {
  canvas.fill_rectangle(0.0, 0.0, 800.0, 72.0, 0x050912ddU);
  canvas.text(title, 22.0, 14.0, 0xffffffffU, 22.0);
  canvas.text(detail, 22.0, 43.0, 0x91a4c2ffU, 12.0);
}

class Scene3dScene final {
 public:
  void frame(const Frame& frame) {
    orbit_.update(frame);
    draw_ground(frame.canvas, orbit_, 3.5, -0.75, 0x71819cffU);
    draw_box(frame.canvas, orbit_, {-2.0, -0.25, 0.0}, 0.5, 0xcc3333ffU);
    draw_sphere(frame.canvas, orbit_, {0.0, -0.25, 0.0}, 0.5, 0xaaaaaaffU);
    draw_cone(frame.canvas, orbit_, {2.0, -0.25, 0.0}, 0.5, 1.0, 0x3366ccffU);
    draw_orbit_help(frame.canvas, "Scene3D", "box · metallic sphere · cone · shadow receiver     drag to orbit · wheel to zoom");
  }
 private:
  Orbit orbit_{0.68, 0.38, 7.0};
};

class InstancedMeshScene final {
 public:
  void frame(const Frame& frame) {
    orbit_.update(frame);
    draw_ground(frame.canvas, orbit_, 6.0, -0.5, 0x71819cffU);
    struct Box { Vec3 center; double depth; };
    std::vector<Box> boxes;
    for (int row = 0; row < 5; ++row) for (int column = 0; column < 5; ++column) {
      const Vec3 center{(static_cast<double>(column) - 2.0) * 1.5, 0.0, (static_cast<double>(row) - 2.0) * 1.5};
      boxes.push_back({center, orbit_.project(center).depth});
    }
    std::ranges::sort(boxes, std::greater{}, &Box::depth);
    for (const auto& box : boxes) draw_box(frame.canvas, orbit_, box.center, 0.3, 0xcc5533ffU);
    draw_orbit_help(frame.canvas, "Instanced mesh", "one box geometry · one material · 25 instance matrices     drag to orbit · wheel to zoom");
  }
 private:
  Orbit orbit_{0.68, 0.5, 10.0};
};

class ScenePickingScene final {
 public:
  void frame(const Frame& frame) {
    orbit_.update(frame, true);
    pick(frame);
    constexpr std::array<std::uint32_t, 6> colors{0xe95f6dffU, 0x44c6d8ffU, 0xf1af4bffU, 0x8d71eaffU, 0x62c979ffU, 0xe076b4ffU};
    for (int index = 0; index < 9; ++index) {
      const Vec3 center{(static_cast<double>(index % 3) - 1.0) * 1.75, (1.0 - static_cast<double>(index / 3)) * 1.55, static_cast<double>((index % 3 + index / 3) % 2) * 0.42 - 0.2};
      const std::uint32_t color = index == selected_ ? 0xffd84dffU : index == hovered_ ? 0x67f0ffffU : colors[static_cast<std::size_t>(index % 6)];
      if (index % 3 == 0) draw_box(frame.canvas, orbit_, center, 0.52, color);
      else if (index % 3 == 1) draw_sphere(frame.canvas, orbit_, center, 0.62, color, index == selected_);
      else {
        const auto point = orbit_.project(center);
        if (point.visible) {
          frame.canvas.stroke_circle(point.x, point.y, point.scale * 0.58, color, point.scale * 0.18);
          if (index == selected_) frame.canvas.stroke_circle(point.x, point.y, point.scale * 0.78, 0xffd84dffU, 3.0);
        }
      }
    }
    draw_orbit_help(frame.canvas, "Scene picking", "hover highlights cyan · click selects gold · camera orbits continuously");
  }
 private:
  void pick(const Frame& frame) {
    hovered_ = -1;
    double nearest = 1e9;
    for (int index = 0; index < 9; ++index) {
      const Vec3 center{(static_cast<double>(index % 3) - 1.0) * 1.75, (1.0 - static_cast<double>(index / 3)) * 1.55, static_cast<double>((index % 3 + index / 3) % 2) * 0.42 - 0.2};
      const auto point = orbit_.project(center);
      const double distance = std::hypot(frame.input.pointer_x - point.x, frame.input.pointer_y - point.y);
      if (point.visible && distance < point.scale * 0.75 && distance < nearest) {
        nearest = distance;
        hovered_ = index;
      }
    }
    if (!frame.input.pointer_inside) hovered_ = -1;
    if (frame.input.pointer_pressed) selected_ = hovered_;
  }
  Orbit orbit_{0.0, 0.05, 8.8};
  int hovered_{-1};
  int selected_{4};
};

class Awd2LoadingScene final {
 public:
  void frame(const Frame& frame) {
    orbit_.update(frame);
    draw_ground(frame.canvas, orbit_, 3.2, -1.0, 0x31445fffU);
    const auto center = orbit_.project({0.0, 0.0, 0.0});
    if (center.visible) frame.canvas.fill_circle(center.x, center.y, center.scale * 1.5, 0x49d8ffffU, 0.08);
    draw_box(frame.canvas, orbit_, {0.0, 0.0, 0.0}, 1.25, 0x397da8ffU);
    if (center.visible) {
      frame.canvas.stroke_circle(center.x, center.y, center.scale * 1.55, 0x49d8ffffU, 3.0, 0.6);
      frame.canvas.fill_circle(center.x - center.scale * 0.38, center.y - center.scale * 0.45, center.scale * 0.15, 0xffffffffU, 0.22);
    }
    frame.canvas.fill_rounded_rectangle(510.0, 88.0, 270.0, 420.0, 12.0, 0x101828eeU);
    frame.canvas.stroke_rectangle(510.0, 88.0, 270.0, 420.0, 0x314662ffU);
    frame.canvas.text("AWD2 loading", 530.0, 108.0, 0xffffffffU, 23.0);
    frame.canvas.text("A tiny cube and light are", 530.0, 154.0, 0xaebbd0ffU, 13.0);
    frame.canvas.text("authored into an AWD2 byte", 530.0, 176.0, 0xaebbd0ffU, 13.0);
    frame.canvas.text("stream, parsed, and assembled", 530.0, 198.0, 0xaebbd0ffU, 13.0);
    frame.canvas.text("into a live scene document.", 530.0, 220.0, 0xaebbd0ffU, 13.0);
    frame.canvas.text("1.1 KiB AWD2", 530.0, 266.0, 0xffffffffU, 15.0);
    frame.canvas.text("1 mesh · 24 vertices", 530.0, 292.0, 0xcbd5e6ffU, 13.0);
    frame.canvas.text("1 ShadedMaterial · 1 light", 530.0, 314.0, 0xcbd5e6ffU, 13.0);
    frame.canvas.text("✓ parsed with 0 diagnostics", 530.0, 354.0, 0x65e6aaffU, 12.0);
    frame.canvas.text("✓ built-in rim modifier", 530.0, 378.0, 0x65e6aaffU, 12.0);
    frame.canvas.text("✓ imported key + ambient", 530.0, 402.0, 0x65e6aaffU, 12.0);
    frame.canvas.text("Drag to orbit · wheel to zoom", 530.0, 458.0, 0x8294adffU, 12.0);
  }
 private:
  Orbit orbit_{0.18, 0.08, 7.0};
};

struct FlameParticle final {
  Vec3 position;
  Vec3 velocity;
  double age{};
  double lifetime{1.0};
  double size{0.1};
  bool spark{};
};

class SceneFireScene final {
 public:
  void frame(const Frame& frame) {
    orbit_.update(frame, true);
    update(frame.delta_time);
    draw_environment(frame.canvas);
    std::ranges::sort(particles_, std::greater{}, [this](const FlameParticle& particle) { return orbit_.project(particle.position).depth; });
    for (const auto& particle : particles_) draw_particle(frame.canvas, particle);
    draw_orbit_help(frame.canvas, "Scene fire", "three 3D emitters · flame + spark layers · bloom · vignette · ACES tone map");
  }

 private:
  static constexpr std::array<Vec3, 3> fires{{{-2.35, -0.58, -0.32}, {0.0, -0.58, 0.28}, {2.35, -0.58, -0.32}}};

  void update(double dt) {
    spawn_ += 360.0 * dt;
    while (spawn_ >= 1.0) {
      spawn_ -= 1.0;
      const int fire = next_fire_++ % 3;
      const bool spark = random_.next() < 0.18;
      const double angle = random_.next() * 2.0 * pi;
      const double radial = random_.next() * (spark ? 0.22 : 0.18);
      const Vec3 origin = fires[static_cast<std::size_t>(fire)];
      particles_.push_back({
          .position = {origin.x + std::cos(angle) * radial, origin.y, origin.z + std::sin(angle) * radial},
          .velocity = {(random_.next() - 0.5) * (spark ? 0.8 : 0.38), spark ? 1.55 + random_.next() * 2.1 : 0.82 + random_.next() * 1.53, (random_.next() - 0.5) * 0.45},
          .lifetime = spark ? 0.9 + random_.next() * 1.25 : 0.62 + random_.next() * 0.8,
          .size = spark ? 0.05 + random_.next() * 0.08 : 0.28 + random_.next() * 0.55,
          .spark = spark,
      });
    }
    for (auto& particle : particles_) {
      particle.age += dt;
      particle.position.x += particle.velocity.x * dt;
      particle.position.y += particle.velocity.y * dt;
      particle.position.z += particle.velocity.z * dt;
      particle.velocity.y += (particle.spark ? 0.24 : 0.62) * dt;
    }
    std::erase_if(particles_, [](const FlameParticle& particle) { return particle.age >= particle.lifetime; });
  }

  void draw_environment(Canvas& canvas) const {
    draw_ground(canvas, orbit_, 4.8, -1.2, 0x56434affU);
    const auto wall_left = orbit_.project({-4.6, 1.25, -2.65});
    const auto wall_right = orbit_.project({4.6, -1.2, -2.65});
    if (wall_left.visible && wall_right.visible) canvas.fill_rectangle(std::min(wall_left.x, wall_right.x), std::min(wall_left.y, wall_right.y), std::abs(wall_right.x - wall_left.x), std::abs(wall_right.y - wall_left.y), 0x24212bffU);
    for (double x : {-4.05, -1.4, 1.4, 4.05}) draw_box(canvas, orbit_, {x, -0.05, -2.42}, 0.25, 0x37333dffU);
    for (std::size_t index = 0; index < fires.size(); ++index) {
      const auto base = orbit_.project({fires[index].x, -0.82, fires[index].z});
      if (!base.visible) continue;
      canvas.fill_ellipse(base.x, base.y, base.scale * 0.54, base.scale * 0.2, 0x5f5965ffU);
      canvas.fill_ellipse(base.x, base.y, base.scale * 0.47, base.scale * 0.14, 0x25232affU);
      canvas.fill_circle(base.x, base.y - base.scale * 0.06, base.scale * 0.32, 0xff4b08ffU, 0.25);
    }
  }

  void draw_particle(Canvas& canvas, const FlameParticle& particle) const {
    const auto point = orbit_.project(particle.position);
    if (!point.visible) return;
    const double amount = std::clamp(particle.age / particle.lifetime, 0.0, 1.0);
    const double radius = std::max(0.7, particle.size * point.scale * (1.0 - amount * 0.75));
    const std::uint32_t color = particle.spark ? lerp_color(0xffffd8ffU, 0xee3704ffU, amount) : lerp_color(0xffffa0ffU, 0xad1000ffU, amount);
    canvas.fill_circle(point.x, point.y, radius * 2.6, color, (1.0 - amount) * 0.12);
    canvas.fill_circle(point.x, point.y, radius, color, 1.0 - amount);
  }

  Orbit orbit_{0.0, 0.2, 8.6};
  Random random_{0x51f15eU};
  std::vector<FlameParticle> particles_;
  double spawn_{};
  int next_fire_{};
};

class MaterialShowcaseScene final {
 public:
  void frame(const Frame& frame) {
    orbit_.update(frame);
    if (frame.input.key_pressed(SDLK_LEFT)) selected_ = (selected_ + names_.size() - 1) % names_.size();
    if (frame.input.key_pressed(SDLK_RIGHT)) selected_ = (selected_ + 1) % names_.size();
    pick(frame);
    draw_ground(frame.canvas, orbit_, 5.8, -3.1, 0x33415affU);
    for (std::size_t index = 0; index < names_.size(); ++index) {
      const Vec3 center{(static_cast<double>(index % 5) - 2.0) * 1.75 - 0.7, (1.5 - static_cast<double>(index / 5)) * 1.65, 0.0};
      const bool selected = index == selected_;
      const auto color = selected ? 0x7ef7c8ffU : colors_[index];
      if (index == names_.size() - 1) {
        const auto point = orbit_.project(center);
        if (point.visible) {
          for (int ring = 0; ring < 5; ++ring) frame.canvas.stroke_circle(point.x, point.y, point.scale * (0.2 + ring * 0.1), color, 1.5, 0.8);
          if (selected) frame.canvas.stroke_circle(point.x, point.y, point.scale * 0.8, 0x7ef7c8ffU, 3.0);
        }
      } else {
        draw_sphere(frame.canvas, orbit_, center, 0.68 * (selected ? 1.08 : 1.0), color, selected);
      }
    }
    draw_panel(frame.canvas);
  }

 private:
  void pick(const Frame& frame) {
    if (!frame.input.pointer_pressed || frame.input.pointer_x > 600.0) return;
    double nearest = 1e9;
    std::size_t picked = selected_;
    for (std::size_t index = 0; index < names_.size(); ++index) {
      const Vec3 center{(static_cast<double>(index % 5) - 2.0) * 1.75 - 0.7, (1.5 - static_cast<double>(index / 5)) * 1.65, 0.0};
      const auto point = orbit_.project(center);
      const double distance = std::hypot(frame.input.pointer_x - point.x, frame.input.pointer_y - point.y);
      if (point.visible && distance < point.scale * 0.82 && distance < nearest) {
        nearest = distance;
        picked = index;
      }
    }
    selected_ = picked;
  }

  void draw_panel(Canvas& canvas) const {
    canvas.fill_rectangle(596.0, 0.0, 204.0, 600.0, 0x0c1321eeU);
    canvas.line(596.0, 0.0, 596.0, 600.0, 0x30425fffU, 2.0);
    canvas.text("Material showcase", 612.0, 20.0, 0xffffffffU, 18.0);
    canvas.text(names_[selected_], 612.0, 58.0, 0x7ef7c8ffU, 14.0);
    canvas.text("← / → select", 612.0, 88.0, 0x7f91adffU, 11.0);
    canvas.text("click a sphere", 612.0, 108.0, 0x7f91adffU, 11.0);
    canvas.text("drag to orbit", 612.0, 128.0, 0x7f91adffU, 11.0);
    canvas.text("wheel to zoom", 612.0, 148.0, 0x7f91adffU, 11.0);
    canvas.text("Live controls", 612.0, 194.0, 0xe2e9f5ffU, 13.0);
    for (int index = 0; index < 5; ++index) {
      const double y = 230.0 + static_cast<double>(index) * 55.0;
      const double amount = std::fmod(static_cast<double>(selected_) * 0.19 + static_cast<double>(index) * 0.17, 1.0);
      canvas.text(index == 0 ? "roughness" : index == 1 ? "metallic" : index == 2 ? "intensity" : index == 3 ? "extension" : "strength", 612.0, y, 0xa8b5caffU, 11.0);
      canvas.fill_rectangle(612.0, y + 21.0, 164.0, 4.0, 0x29344affU);
      canvas.fill_rectangle(612.0, y + 21.0, 164.0 * amount, 4.0, 0x7ef7c8ffU);
    }
    canvas.text("20 material models", 612.0, 540.0, 0x7385a2ffU, 11.0);
    canvas.text("one shared sphere", 612.0, 560.0, 0x7385a2ffU, 11.0);
  }

  static constexpr std::array<std::string_view, 20> names_{
      "Standard PBR", "Specular-glossiness", "Anisotropy PBR", "Clearcoat PBR", "Iridescence PBR",
      "Sheen PBR", "Specular PBR", "Wrapped diffuse PBR", "Transmission volume", "Blinn-Phong",
      "Lambert", "Phong", "Toon", "Unlit", "Emissive", "Matcap", "Normal", "Depth", "Vertex color", "Wireframe"};
  static constexpr std::array<std::uint32_t, 20> colors_{
      0xc97842ffU, 0x73a8e6ffU, 0xd26092ffU, 0x5fd0c0ffU, 0xa880ffffU,
      0xc56eb5ffU, 0x8caef5ffU, 0x80bd69ffU, 0xbde8ffffU, 0xdd9c57ffU,
      0x77bb66ffU, 0x5d88d8ffU, 0x7d66ffffU, 0xff794dffU, 0x35d7ffffU,
      0xd8b8ffffU, 0x7da7ffffU, 0x777777ffU, 0x65e6aaffU, 0xf0f5ffffU};
  Orbit orbit_{0.0, 0.15, 11.8};
  std::size_t selected_{};
};

} // namespace

int run_awd2loading(int argc, char** argv) { return run_scene<Awd2LoadingScene>(argc, argv, "Flight AWD2 loading - SDL", 0x07101cffU); }
int run_instanced_mesh(int argc, char** argv) { return run_scene<InstancedMeshScene>(argc, argv, "Flight instanced mesh - SDL"); }
int run_materialshowcase(int argc, char** argv) { return run_scene<MaterialShowcaseScene>(argc, argv, "Flight material showcase - SDL", 0x070c16ffU); }
int run_scene_fire(int argc, char** argv) { return run_scene<SceneFireScene>(argc, argv, "Flight scene fire - SDL", 0x05040affU); }
int run_scene_picking(int argc, char** argv) { return run_scene<ScenePickingScene>(argc, argv, "Flight scene picking - SDL"); }
int run_scene3d(int argc, char** argv) { return run_scene<Scene3dScene>(argc, argv, "Flight scene3d - SDL"); }

} // namespace flight::examples
