// Native SDL port of @flighthq/example-camera2d.
// Source: .dependencies/flight/examples/packages/camera2d/src/app.ts

#include "sdl_host.hpp"

#include <flight/camera/camera2d.hpp>
#include <flight/camera/parallax.hpp>
#include <flight/camera/view_matrix.hpp>
#include <flight/camera/visible_bounds.hpp>
#include <flight/camera/zoom.hpp>
#include <flight/camera_controls/follow.hpp>
#include <flight/geometry/matrix.hpp>
#include <flight/geometry/rectangle.hpp>
#include <flight/geometry/vector2.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
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
  return (channel(red) << 16U) | (channel(green) << 8U) | channel(blue);
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

template <typename Value>
[[nodiscard]] auto readonly_row(const std::shared_ptr<Value>& value) {
  return flight::structural_ref_cast<
      flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<Value>>>>>(
      flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<Value>>>>(value));
}

[[nodiscard]] flight::Ref<flight::types::Camera2D> create_example_camera() {
  auto options = flight::make_ref<flight::types::Camera2DOptions>(
      flight::types::Camera2DOptions{
          .rotation = std::nullopt,
          .x = world_width * 0.5,
          .y = world_height * 0.5,
          .zoom = 1.0,
      });
  return flight::camera::create_camera2_d(readonly_row(options));
}

class CameraExample final {
 public:
  CameraExample()
      : camera_(create_example_camera()),
        world_bounds_(flight::geometry::create_rectangle(0.0, 0.0, world_width, world_height)),
        follow_options_(flight::make_ref<flight::types::Camera2DFollowOptions>(
            flight::types::Camera2DFollowOptions{
                .deadzone_half_height = 40.0,
                .deadzone_half_width = 60.0,
                .smooth_time = 0.15,
                .world_bounds = world_bounds_,
            })),
        view_matrix_(flight::geometry::create_matrix()),
        visible_bounds_(flight::geometry::create_rectangle()),
        stars_offset_(flight::geometry::create_vector2()),
        mountains_offset_(flight::geometry::create_vector2()),
        clouds_offset_(flight::geometry::create_vector2()) {
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
    if (frame.smoke) verify_smoke(frame.index);
    draw_parallax(frame.canvas);
    draw_world(frame.canvas);
    draw_hud(frame.canvas);
  }

 private:
  [[nodiscard]] Point screen(Point world) const {
    return {
        view_matrix_->a * world.x + view_matrix_->c * world.y + view_matrix_->tx,
        view_matrix_->b * world.x + view_matrix_->d * world.y + view_matrix_->ty,
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
    const double direction = frame.input.wheel_y > 0.0 ? 1.0 : -1.0;
    const double next = std::clamp(
        camera_->zoom + direction * 0.1 * camera_->zoom,
        0.25,
        4.0);
    flight::camera::zoom_camera2_dat_screen_point(
        camera_, 800.0, 600.0, frame.input.pointer_x, frame.input.pointer_y, next);
  }

  void update_camera(double delta_time) {
    flight::camera_controls::update_camera2_dfollow(
        camera_, 800.0, 600.0, player_.x, player_.y, delta_time, follow_options_);
    ++follow_updates_;
    flight::camera::get_camera2_dview_matrix(camera_, 800.0, 600.0, view_matrix_);
    flight::camera::get_camera2_dvisible_bounds(camera_, 800.0, 600.0, visible_bounds_);
    flight::camera::get_camera2_dparallax_point(camera_, 800.0, 600.0, 0.1, stars_offset_);
    flight::camera::get_camera2_dparallax_point(camera_, 800.0, 600.0, 0.4, mountains_offset_);
    flight::camera::get_camera2_dparallax_point(camera_, 800.0, 600.0, 0.6, clouds_offset_);
  }

  [[nodiscard]] static bool near(double actual, double expected) {
    return std::abs(actual - expected) <= 1e-9;
  }

  void verify_smoke(std::size_t frame_index) {
    if (!near(camera_->x, world_width * 0.5) ||
        !near(camera_->y, world_height * 0.5) || !near(camera_->zoom, 1.0) ||
        follow_updates_ != frame_index + 1 ||
        !near(view_matrix_->a, 1.0) || !near(view_matrix_->b, 0.0) ||
        !near(view_matrix_->c, 0.0) || !near(view_matrix_->d, 1.0) ||
        !near(view_matrix_->tx, -800.0) || !near(view_matrix_->ty, -600.0) ||
        !near(visible_bounds_->x, 800.0) || !near(visible_bounds_->y, 600.0) ||
        !near(visible_bounds_->width, 800.0) || !near(visible_bounds_->height, 600.0) ||
        !near(stars_offset_->x, -120.0) || !near(stars_offset_->y, -90.0) ||
        !near(mountains_offset_->x, -480.0) || !near(mountains_offset_->y, -360.0) ||
        !near(clouds_offset_->x, -720.0) || !near(clouds_offset_->y, -540.0)) {
      throw std::runtime_error(
          "camera2d smoke did not observe generated follow, view, bounds, and parallax output");
    }
    if (smoke_probe_complete_) return;

    auto zoom_probe = create_example_camera();
    flight::camera::zoom_camera2_dat_screen_point(zoom_probe, 800.0, 600.0, 200.0, 150.0, 2.0);
    if (!near(zoom_probe->x, 1100.0) || !near(zoom_probe->y, 825.0) ||
        !near(zoom_probe->zoom, 2.0)) {
      throw std::runtime_error("camera2d smoke did not observe generated anchored zoom output");
    }

    auto follow_probe = create_example_camera();
    flight::camera_controls::update_camera2_dfollow(
        follow_probe, 800.0, 600.0, 1500.0, 1100.0, 1.0 / 60.0, follow_options_);
    if (!(follow_probe->x > world_width * 0.5 && follow_probe->x < 1500.0 &&
          follow_probe->y > world_height * 0.5 && follow_probe->y < 1100.0)) {
      throw std::runtime_error("camera2d smoke did not observe generated follow output");
    }
    smoke_probe_complete_ = true;
  }

  void draw_parallax(Canvas& canvas) const {
    for (const auto& star : stars_) {
      canvas.fill_circle(
          star.position.x + stars_offset_->x,
          star.position.y + stars_offset_->y,
          star.radius,
          0xffffd0ffU,
          star.brightness);
    }
    for (const auto& mountain : mountains_) {
      const double x = mountain.x + mountains_offset_->x;
      const std::array<Point, 3> points{{
          {x - mountain.width / 2.0, 600.0 + mountains_offset_->y},
          {x, 600.0 - mountain.peak_height + mountains_offset_->y},
          {x + mountain.width / 2.0, 600.0 + mountains_offset_->y},
      }};
      canvas.fill_polygon(points, 0x3c5064ffU, 0.6);
    }
    for (const auto& cloud : clouds_) {
      canvas.fill_ellipse(
          cloud.position.x + clouds_offset_->x,
          cloud.position.y + clouds_offset_->y,
          cloud.width / 2.0,
          cloud.height / 2.0,
          0xc8d2e6ffU,
          0.4);
    }
  }

  void draw_world(Canvas& canvas) const {
    constexpr double grid = 100.0;
    for (double x = 0.0; x <= world_width; x += grid) {
      const Point from = screen({x, 0.0});
      const Point to = screen({x, world_height});
      canvas.line(from.x, from.y, to.x, to.y, 0x64788cffU, 1.0, 0.15);
    }
    for (double y = 0.0; y <= world_height; y += grid) {
      const Point from = screen({0.0, y});
      const Point to = screen({world_width, y});
      canvas.line(from.x, from.y, to.x, to.y, 0x64788cffU, 1.0, 0.15);
    }
    const Point origin = screen({0.0, 0.0});
    canvas.stroke_rectangle(
        origin.x,
        origin.y,
        world_width * view_matrix_->a,
        world_height * view_matrix_->d,
        0xc85050ffU,
        3.0,
        0.5);

    for (const auto& landmark : landmarks_) {
      const Point position = screen(landmark.position);
      const double width = landmark.width * view_matrix_->a;
      const double height = landmark.height * view_matrix_->d;
      if (position.x + width < 0.0 || position.x - width > 800.0 || position.y + height < 0.0 || position.y - height > 600.0) continue;
      if (landmark.circle) {
        canvas.fill_circle(position.x, position.y, width / 2.0, landmark.color, 0.7);
      } else {
        canvas.fill_rectangle(position.x - width / 2.0, position.y - height / 2.0, width, height, landmark.color, 0.7);
      }
    }

    const Point player = screen(player_);
    const double size = player_size * view_matrix_->a;
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
        "Camera2D: (" + fixed(camera_->x, 0) + ", " + fixed(camera_->y, 0) + ")  Zoom: " + fixed(camera_->zoom, 2),
        16.0,
        16.0,
        0xffffffffU,
        13.0);
    canvas.text("Player: (" + fixed(player_.x, 0) + ", " + fixed(player_.y, 0) + ")", 16.0, 34.0, 0xffffffffU, 13.0);
    canvas.text("WASD/Arrows: move  Scroll: zoom", 16.0, 52.0, 0xffffffffU, 13.0);
    canvas.text("Green = visible bounds  Red = world border", 16.0, 70.0, 0xffffffffU, 13.0);
  }

  std::vector<Landmark> landmarks_;
  std::vector<Star> stars_;
  std::vector<Cloud> clouds_;
  std::vector<Mountain> mountains_;
  Point player_{world_width * 0.5, world_height * 0.5};
  flight::Ref<flight::types::Camera2D> camera_;
  flight::Ref<flight::types::Rectangle> world_bounds_;
  flight::Ref<flight::types::Camera2DFollowOptions> follow_options_;
  flight::Ref<flight::types::Matrix> view_matrix_;
  flight::Ref<flight::types::Rectangle> visible_bounds_;
  flight::Ref<flight::types::Vector2> stars_offset_;
  flight::Ref<flight::types::Vector2> mountains_offset_;
  flight::Ref<flight::types::Vector2> clouds_offset_;
  std::size_t follow_updates_{};
  bool smoke_probe_complete_{};
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
