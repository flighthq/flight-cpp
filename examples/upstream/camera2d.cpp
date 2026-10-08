// Native SDL port of @flighthq/example-camera2d.
// Source: .dependencies/flight/examples/packages/camera2d/src/app.ts

#include "sdl_host.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

using flight::examples::Canvas;
using flight::examples::Frame;
using flight::examples::Point;

constexpr double world_width = 2400.0;
constexpr double world_height = 1800.0;
constexpr double player_size = 24.0;
constexpr double player_speed = 300.0;

class Random final {
 public:
  [[nodiscard]] double next() {
    state_ = (state_ * 16807) % 2147483647;
    return static_cast<double>(state_) / 2147483647.0;
  }

 private:
  std::int64_t state_{42};
};

[[nodiscard]] std::uint32_t hsl(double hue) {
  constexpr double saturation = 0.6;
  constexpr double lightness = 0.5;
  const double chroma = (1.0 - std::abs(2.0 * lightness - 1.0)) * saturation;
  const double intermediate = chroma * (1.0 - std::abs(std::fmod(hue / 60.0, 2.0) - 1.0));
  const double match = lightness - chroma / 2.0;
  double red{};
  double green{};
  double blue{};
  if (hue < 60.0) {
    red = chroma;
    green = intermediate;
  } else if (hue < 120.0) {
    red = intermediate;
    green = chroma;
  } else if (hue < 180.0) {
    green = chroma;
    blue = intermediate;
  } else if (hue < 240.0) {
    green = intermediate;
    blue = chroma;
  } else if (hue < 300.0) {
    red = intermediate;
    blue = chroma;
  } else {
    red = chroma;
    blue = intermediate;
  }
  const auto channel = [match](double value) {
    return static_cast<std::uint32_t>(std::lround((value + match) * 255.0));
  };
  return (channel(red) << 24U) | (channel(green) << 16U) | (channel(blue) << 8U) | 0xffU;
}

[[nodiscard]] std::string fixed(double value, int precision) {
  std::ostringstream stream;
  stream << std::fixed << std::setprecision(precision) << value;
  return stream.str();
}

struct Landmark final {
  Point position;
  double width{};
  double height{};
  std::uint32_t color{};
  bool circle{};
};

struct Star final {
  Point position;
  double radius{};
  double brightness{};
};

struct Cloud final {
  Point position;
  double width{};
  double height{};
};

struct Mountain final {
  double x{};
  double peak_height{};
  double width{};
};

class CameraExample final {
 public:
  CameraExample() {
    Random random;
    for (int index = 0; index < 60; ++index) {
      const bool circle = random.next() <= 0.5;
      const double size = 20.0 + random.next() * 80.0;
      landmarks_.push_back({
          .position = {100.0 + random.next() * (world_width - 200.0), 100.0 + random.next() * (world_height - 200.0)},
          .width = size,
          .height = circle ? size : 20.0 + random.next() * 80.0,
          .color = hsl(std::floor(random.next() * 360.0)),
          .circle = circle,
      });
    }
    for (int index = 0; index < 80; ++index) {
      stars_.push_back({
          .position = {random.next() * 800.0, random.next() * 600.0},
          .radius = 1.0 + random.next() * 2.0,
          .brightness = 0.3 + random.next() * 0.7,
      });
    }
    for (int index = 0; index < 12; ++index) {
      clouds_.push_back({
          .position = {random.next() * 800.0, 50.0 + random.next() * 200.0},
          .width = 80.0 + random.next() * 120.0,
          .height = 30.0 + random.next() * 40.0,
      });
    }
    for (int index = 0; index < 8; ++index) {
      mountains_.push_back({
          .x = random.next() * 800.0,
          .peak_height = 100.0 + random.next() * 200.0,
          .width = 120.0 + random.next() * 180.0,
      });
    }
  }

  void frame(const Frame& frame) {
    update_player(frame);
    update_zoom(frame);
    update_camera(frame.delta_time);
    draw_parallax(frame.canvas);
    draw_world(frame.canvas);
    draw_hud(frame.canvas);
  }

 private:
  [[nodiscard]] Point screen(Point world) const {
    return {
        (world.x - camera_.x) * zoom_ + 400.0,
        (world.y - camera_.y) * zoom_ + 300.0,
    };
  }

  void update_player(const Frame& frame) {
    double horizontal{};
    double vertical{};
    if (frame.input.key_down(SDLK_LEFT) || frame.input.key_down(SDLK_A)) horizontal -= 1.0;
    if (frame.input.key_down(SDLK_RIGHT) || frame.input.key_down(SDLK_D)) horizontal += 1.0;
    if (frame.input.key_down(SDLK_UP) || frame.input.key_down(SDLK_W)) vertical -= 1.0;
    if (frame.input.key_down(SDLK_DOWN) || frame.input.key_down(SDLK_S)) vertical += 1.0;
    const double length = std::hypot(horizontal, vertical);
    if (length != 0.0) {
      horizontal /= length;
      vertical /= length;
    }
    player_.x = std::clamp(player_.x + horizontal * player_speed * frame.delta_time, player_size, world_width - player_size);
    player_.y = std::clamp(player_.y + vertical * player_speed * frame.delta_time, player_size, world_height - player_size);
  }

  void update_zoom(const Frame& frame) {
    if (frame.input.wheel_y == 0.0) return;
    const Point before{
        camera_.x + (frame.input.pointer_x - 400.0) / zoom_,
        camera_.y + (frame.input.pointer_y - 300.0) / zoom_,
    };
    const double direction = frame.input.wheel_y > 0.0 ? 1.0 : -1.0;
    const double next = std::clamp(zoom_ + direction * 0.1 * zoom_, 0.25, 4.0);
    camera_.x = before.x - (frame.input.pointer_x - 400.0) / next;
    camera_.y = before.y - (frame.input.pointer_y - 300.0) / next;
    zoom_ = next;
  }

  void update_camera(double delta_time) {
    Point target = camera_;
    if (player_.x < camera_.x - 60.0) target.x = player_.x + 60.0;
    if (player_.x > camera_.x + 60.0) target.x = player_.x - 60.0;
    if (player_.y < camera_.y - 40.0) target.y = player_.y + 40.0;
    if (player_.y > camera_.y + 40.0) target.y = player_.y - 40.0;
    const double smoothing = 1.0 - std::exp(-delta_time / 0.15);
    camera_.x += (target.x - camera_.x) * smoothing;
    camera_.y += (target.y - camera_.y) * smoothing;
    const double half_width = 400.0 / zoom_;
    const double half_height = 300.0 / zoom_;
    camera_.x = std::clamp(camera_.x, half_width, world_width - half_width);
    camera_.y = std::clamp(camera_.y, half_height, world_height - half_height);
  }

  static double wrapped(double value, double extent) {
    value = std::fmod(value, extent);
    return value < 0.0 ? value + extent : value;
  }

  void draw_parallax(Canvas& canvas) const {
    for (const auto& star : stars_) {
      canvas.fill_circle(
          wrapped(star.position.x - camera_.x * 0.1, 800.0),
          wrapped(star.position.y - camera_.y * 0.1, 600.0),
          star.radius,
          0xffffd0ffU,
          star.brightness);
    }
    for (const auto& mountain : mountains_) {
      const double x = wrapped(mountain.x - camera_.x * 0.4, 950.0) - 75.0;
      const std::array<Point, 3> points{{
          {x - mountain.width / 2.0, 600.0},
          {x, 600.0 - mountain.peak_height},
          {x + mountain.width / 2.0, 600.0},
      }};
      canvas.fill_polygon(points, 0x3c5064ffU, 0.6);
    }
    for (const auto& cloud : clouds_) {
      canvas.fill_ellipse(
          wrapped(cloud.position.x - camera_.x * 0.6, 950.0) - 75.0,
          cloud.position.y,
          cloud.width / 2.0,
          cloud.height / 2.0,
          0xc8d2e6ffU,
          0.4);
    }
  }

  void draw_world(Canvas& canvas) const {
    constexpr double grid = 100.0;
    const double left = camera_.x - 400.0 / zoom_;
    const double right = camera_.x + 400.0 / zoom_;
    const double top = camera_.y - 300.0 / zoom_;
    const double bottom = camera_.y + 300.0 / zoom_;
    for (double x = std::floor(left / grid) * grid; x <= right; x += grid) {
      const Point from = screen({x, std::max(0.0, top)});
      const Point to = screen({x, std::min(world_height, bottom)});
      canvas.line(from.x, from.y, to.x, to.y, 0x64788cffU, 1.0, 0.15);
    }
    for (double y = std::floor(top / grid) * grid; y <= bottom; y += grid) {
      const Point from = screen({std::max(0.0, left), y});
      const Point to = screen({std::min(world_width, right), y});
      canvas.line(from.x, from.y, to.x, to.y, 0x64788cffU, 1.0, 0.15);
    }
    const Point origin = screen({0.0, 0.0});
    canvas.stroke_rectangle(origin.x, origin.y, world_width * zoom_, world_height * zoom_, 0xc85050ffU, 3.0, 0.5);

    for (const auto& landmark : landmarks_) {
      const Point position = screen(landmark.position);
      const double width = landmark.width * zoom_;
      const double height = landmark.height * zoom_;
      if (position.x + width < 0.0 || position.x - width > 800.0 || position.y + height < 0.0 || position.y - height > 600.0) continue;
      if (landmark.circle) {
        canvas.fill_circle(position.x, position.y, width / 2.0, landmark.color, 0.7);
      } else {
        canvas.fill_rectangle(position.x - width / 2.0, position.y - height / 2.0, width, height, landmark.color, 0.7);
      }
    }

    const Point player = screen(player_);
    const double size = player_size * zoom_;
    const std::array<Point, 3> triangle{{
        {player.x, player.y - size},
        {player.x + size * 0.8, player.y + size * 0.6},
        {player.x - size * 0.8, player.y + size * 0.6},
    }};
    canvas.fill_polygon(triangle, 0xffcc33ffU);
    canvas.polyline(triangle, 0xcc9900ffU, 2.0, true);
    canvas.stroke_rectangle(0.0, 0.0, 800.0, 600.0, 0x00c864ffU, 2.0, 0.6);
  }

  void draw_hud(Canvas& canvas) const {
    canvas.fill_rectangle(8.0, 8.0, 260.0, 80.0, 0x000000ffU, 0.5);
    canvas.text(
        "Camera2D: (" + fixed(camera_.x, 0) + ", " + fixed(camera_.y, 0) + ")  Zoom: " + fixed(zoom_, 2),
        16.0,
        16.0,
        0xffffffffU,
        10.0);
    canvas.text("Player: (" + fixed(player_.x, 0) + ", " + fixed(player_.y, 0) + ")", 16.0, 34.0, 0xffffffffU, 10.0);
    canvas.text("WASD/Arrows: move  Scroll: zoom", 16.0, 52.0, 0xffffffffU, 9.0);
    canvas.text("Green = visible bounds  Red = world border", 16.0, 70.0, 0xffffffffU, 8.0);
  }

  std::vector<Landmark> landmarks_;
  std::vector<Star> stars_;
  std::vector<Cloud> clouds_;
  std::vector<Mountain> mountains_;
  Point player_{world_width * 0.5, world_height * 0.5};
  Point camera_{world_width * 0.5, world_height * 0.5};
  double zoom_{1.0};
};

} // namespace

int main(int argc, char** argv) {
  try {
    CameraExample example;
    return flight::examples::SdlExample::run(
        argc,
        argv,
        "Flight Camera2D - SDL",
        [&](const Frame& frame) { example.frame(frame); });
  } catch (const std::exception& error) {
    std::cerr << "camera2d example failed: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
