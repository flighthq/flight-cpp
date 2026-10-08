#pragma once

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_render.h>

#include <flight/host_sdl/host.hpp>
#include <flight/host_sdl/window.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace flight::examples {

inline constexpr int canvas_width = 800;
inline constexpr int canvas_height = 600;

struct Point final {
  double x{};
  double y{};
};

struct Input final {
  double pointer_x{};
  double pointer_y{};
  double wheel_x{};
  double wheel_y{};
  bool pointer_down{};
  bool pointer_pressed{};
  bool pointer_released{};
  bool shift{};
  bool control{};
  bool alt{};
  std::vector<SDL_Keycode> keys_pressed;
  std::vector<SDL_Keycode> keys_released;
  std::string text;

  [[nodiscard]] bool key_down(SDL_Keycode key) const;
  [[nodiscard]] bool key_pressed(SDL_Keycode key) const;

 private:
  friend class SdlExample;
  std::unordered_set<SDL_Keycode> held_keys;
};

class Canvas final {
 public:
  explicit Canvas(std::string_view title, bool hidden);
  ~Canvas() noexcept;

  Canvas(const Canvas&) = delete;
  Canvas& operator=(const Canvas&) = delete;

  void begin(std::uint32_t background = 0x1a1a2eff);
  void present();

  void fill_rectangle(
      double x,
      double y,
      double width,
      double height,
      std::uint32_t color,
      double alpha = 1.0);
  void stroke_rectangle(
      double x,
      double y,
      double width,
      double height,
      std::uint32_t color,
      double thickness = 1.0,
      double alpha = 1.0);
  void fill_circle(
      double center_x,
      double center_y,
      double radius,
      std::uint32_t color,
      double alpha = 1.0);
  void stroke_circle(
      double center_x,
      double center_y,
      double radius,
      std::uint32_t color,
      double thickness = 1.0,
      double alpha = 1.0);
  void fill_ellipse(
      double center_x,
      double center_y,
      double radius_x,
      double radius_y,
      std::uint32_t color,
      double alpha = 1.0);
  void line(
      double x0,
      double y0,
      double x1,
      double y1,
      std::uint32_t color,
      double thickness = 1.0,
      double alpha = 1.0);
  void polyline(
      std::span<const Point> points,
      std::uint32_t color,
      double thickness = 1.0,
      bool closed = false,
      double alpha = 1.0);
  void fill_polygon(
      std::span<const Point> points,
      std::uint32_t color,
      double alpha = 1.0);
  void quadratic_curve(
      Point from,
      Point control,
      Point to,
      std::uint32_t color,
      double thickness = 1.0,
      double alpha = 1.0);
  void cubic_curve(
      Point from,
      Point control0,
      Point control1,
      Point to,
      std::uint32_t color,
      double thickness = 1.0,
      double alpha = 1.0);
  void fill_rounded_rectangle(
      double x,
      double y,
      double width,
      double height,
      double radius,
      std::uint32_t color,
      double alpha = 1.0);
  void horizontal_gradient(
      double x,
      double y,
      double width,
      double height,
      std::span<const std::uint32_t> colors);
  void text(
      std::string_view value,
      double x,
      double y,
      std::uint32_t color = 0xffffffff,
      double size = 12.0,
      double alpha = 1.0);

  void start_text_input();
  void stop_text_input();

  [[nodiscard]] SDL_Window* native_window() const noexcept;
  [[nodiscard]] SDL_Renderer* native_renderer() const noexcept;

 private:
  void set_color(std::uint32_t color, double alpha);

  flight::host_sdl::Window window_;
  SDL_Renderer* renderer_{};
};

struct Frame final {
  Canvas& canvas;
  const Input& input;
  double time;
  double delta_time;
  std::size_t index;
  bool smoke;
};

class SdlExample final {
 public:
  using FrameCallback = std::function<void(const Frame&)>;
  using ExitCallback = std::function<void()>;

  static int run(
      int argc,
      char** argv,
      std::string_view title,
      FrameCallback callback,
      std::uint32_t background = 0x1a1a2eff,
      ExitCallback on_exit = {});
};

[[nodiscard]] std::array<float, 4> unpack_color(
    std::uint32_t color,
    double alpha = 1.0);
[[nodiscard]] std::uint32_t lerp_color(
    std::uint32_t from,
    std::uint32_t to,
    double amount);
[[nodiscard]] bool contains(
    double x,
    double y,
    double width,
    double height,
    double point_x,
    double point_y);

} // namespace flight::examples
