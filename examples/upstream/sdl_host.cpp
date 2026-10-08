#include "sdl_host.hpp"

#include <SDL3/SDL_error.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <numbers>
#include <stdexcept>
#include <thread>

namespace flight::examples {
namespace {

[[nodiscard]] std::runtime_error sdl_error(std::string_view action) {
  return std::runtime_error(std::string(action) + ": " + SDL_GetError());
}

[[nodiscard]] std::uint8_t channel(double value) {
  return static_cast<std::uint8_t>(std::clamp(std::lround(value), 0L, 255L));
}

} // namespace

bool Input::key_down(SDL_Keycode key) const { return held_keys.contains(key); }

bool Input::key_pressed(SDL_Keycode key) const {
  return std::ranges::find(keys_pressed, key) != keys_pressed.end();
}

Canvas::Canvas(std::string_view title, bool hidden)
    : window_(flight::host_sdl::WindowOptions{
          .title = std::string(title),
          .width = canvas_width,
          .height = canvas_height,
          .graphics_api = flight::host_sdl::GraphicsApi::none,
          .open_gl = {},
          .extra_flags = 0,
          .resizable = true,
          .high_pixel_density = true,
          .hidden = hidden,
      }) {
  renderer_ = SDL_CreateRenderer(window_.native_handle(), nullptr);
  if (renderer_ == nullptr) throw sdl_error("SDL could not create the example renderer");
  if (!SDL_SetRenderLogicalPresentation(
          renderer_,
          canvas_width,
          canvas_height,
          SDL_LOGICAL_PRESENTATION_LETTERBOX)) {
    throw sdl_error("SDL could not set the example logical presentation");
  }
  if (!SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND)) {
    throw sdl_error("SDL could not enable alpha blending");
  }
}

Canvas::~Canvas() noexcept {
  if (renderer_ != nullptr) SDL_DestroyRenderer(renderer_);
}

void Canvas::set_color(std::uint32_t color, double alpha) {
  const auto rgba = unpack_color(color, alpha);
  if (!SDL_SetRenderDrawColor(
          renderer_,
          channel(static_cast<double>(rgba[0]) * 255.0),
          channel(static_cast<double>(rgba[1]) * 255.0),
          channel(static_cast<double>(rgba[2]) * 255.0),
          channel(static_cast<double>(rgba[3]) * 255.0))) {
    throw sdl_error("SDL could not set a draw color");
  }
}

void Canvas::begin(std::uint32_t background) {
  set_color(background, 1.0);
  if (!SDL_RenderClear(renderer_)) throw sdl_error("SDL could not clear an example frame");
}

void Canvas::present() {
  if (!SDL_RenderPresent(renderer_)) throw sdl_error("SDL could not present an example frame");
}

void Canvas::fill_rectangle(
    double x,
    double y,
    double width,
    double height,
    std::uint32_t color,
    double alpha) {
  set_color(color, alpha);
  const SDL_FRect rectangle{
      static_cast<float>(x),
      static_cast<float>(y),
      static_cast<float>(width),
      static_cast<float>(height),
  };
  if (!SDL_RenderFillRect(renderer_, &rectangle)) {
    throw sdl_error("SDL could not fill an example rectangle");
  }
}

void Canvas::stroke_rectangle(
    double x,
    double y,
    double width,
    double height,
    std::uint32_t color,
    double thickness,
    double alpha) {
  const double resolved = std::max(1.0, thickness);
  fill_rectangle(x, y, width, resolved, color, alpha);
  fill_rectangle(x, y + height - resolved, width, resolved, color, alpha);
  fill_rectangle(x, y, resolved, height, color, alpha);
  fill_rectangle(x + width - resolved, y, resolved, height, color, alpha);
}

void Canvas::fill_circle(
    double center_x,
    double center_y,
    double radius,
    std::uint32_t color,
    double alpha) {
  set_color(color, alpha);
  const int extent = static_cast<int>(std::ceil(std::max(0.0, radius)));
  for (int offset = -extent; offset <= extent; ++offset) {
    const double remaining = std::max(0.0, radius * radius - static_cast<double>(offset * offset));
    const double half_width = std::sqrt(remaining);
    if (!SDL_RenderLine(
            renderer_,
            static_cast<float>(center_x - half_width),
            static_cast<float>(center_y + static_cast<double>(offset)),
            static_cast<float>(center_x + half_width),
            static_cast<float>(center_y + static_cast<double>(offset)))) {
      throw sdl_error("SDL could not fill an example circle");
    }
  }
}

void Canvas::stroke_circle(
    double center_x,
    double center_y,
    double radius,
    std::uint32_t color,
    double thickness,
    double alpha) {
  constexpr int segments = 96;
  std::array<Point, segments + 1> points{};
  for (int index = 0; index <= segments; ++index) {
    const double angle = static_cast<double>(index) * 2.0 * std::numbers::pi /
                         static_cast<double>(segments);
    points[static_cast<std::size_t>(index)] = {
        center_x + std::cos(angle) * radius,
        center_y + std::sin(angle) * radius,
    };
  }
  polyline(points, color, thickness, false, alpha);
}

void Canvas::fill_ellipse(
    double center_x,
    double center_y,
    double radius_x,
    double radius_y,
    std::uint32_t color,
    double alpha) {
  set_color(color, alpha);
  const int extent = static_cast<int>(std::ceil(std::max(0.0, radius_y)));
  for (int offset = -extent; offset <= extent; ++offset) {
    const double normalized = radius_y == 0.0 ? 0.0 : static_cast<double>(offset) / radius_y;
    const double half_width = radius_x * std::sqrt(std::max(0.0, 1.0 - normalized * normalized));
    if (!SDL_RenderLine(
            renderer_,
            static_cast<float>(center_x - half_width),
            static_cast<float>(center_y + static_cast<double>(offset)),
            static_cast<float>(center_x + half_width),
            static_cast<float>(center_y + static_cast<double>(offset)))) {
      throw sdl_error("SDL could not fill an example ellipse");
    }
  }
}

void Canvas::line(
    double x0,
    double y0,
    double x1,
    double y1,
    std::uint32_t color,
    double thickness,
    double alpha) {
  set_color(color, alpha);
  const double dx = x1 - x0;
  const double dy = y1 - y0;
  const double length = std::hypot(dx, dy);
  const int passes = std::max(1, static_cast<int>(std::ceil(thickness)));
  const double normal_x = length == 0.0 ? 0.0 : -dy / length;
  const double normal_y = length == 0.0 ? 1.0 : dx / length;
  for (int pass = 0; pass < passes; ++pass) {
    const double offset = static_cast<double>(pass) - (static_cast<double>(passes) - 1.0) / 2.0;
    if (!SDL_RenderLine(
            renderer_,
            static_cast<float>(x0 + normal_x * offset),
            static_cast<float>(y0 + normal_y * offset),
            static_cast<float>(x1 + normal_x * offset),
            static_cast<float>(y1 + normal_y * offset))) {
      throw sdl_error("SDL could not draw an example line");
    }
  }
}

void Canvas::polyline(
    std::span<const Point> points,
    std::uint32_t color,
    double thickness,
    bool closed,
    double alpha) {
  if (points.size() < 2) return;
  for (std::size_t index = 1; index < points.size(); ++index) {
    line(
        points[index - 1].x,
        points[index - 1].y,
        points[index].x,
        points[index].y,
        color,
        thickness,
        alpha);
  }
  if (closed) {
    line(
        points.back().x,
        points.back().y,
        points.front().x,
        points.front().y,
        color,
        thickness,
        alpha);
  }
}

void Canvas::fill_polygon(
    std::span<const Point> points,
    std::uint32_t color,
    double alpha) {
  if (points.size() < 3) return;
  const auto [minimum, maximum] = std::ranges::minmax_element(points, {}, &Point::y);
  const int top = static_cast<int>(std::floor(minimum->y));
  const int bottom = static_cast<int>(std::ceil(maximum->y));
  set_color(color, alpha);
  std::vector<double> intersections;
  intersections.reserve(points.size());
  for (int y = top; y <= bottom; ++y) {
    intersections.clear();
    const double scan_y = static_cast<double>(y) + 0.5;
    for (std::size_t index = 0; index < points.size(); ++index) {
      const Point& from = points[index];
      const Point& to = points[(index + 1) % points.size()];
      if ((from.y <= scan_y && to.y > scan_y) || (to.y <= scan_y && from.y > scan_y)) {
        const double amount = (scan_y - from.y) / (to.y - from.y);
        intersections.push_back(from.x + (to.x - from.x) * amount);
      }
    }
    std::ranges::sort(intersections);
    for (std::size_t index = 1; index < intersections.size(); index += 2) {
      if (!SDL_RenderLine(
              renderer_,
              static_cast<float>(intersections[index - 1]),
              static_cast<float>(scan_y),
              static_cast<float>(intersections[index]),
              static_cast<float>(scan_y))) {
        throw sdl_error("SDL could not fill an example polygon");
      }
    }
  }
}

void Canvas::quadratic_curve(
    Point from,
    Point control,
    Point to,
    std::uint32_t color,
    double thickness,
    double alpha) {
  std::array<Point, 49> points{};
  for (std::size_t index = 0; index < points.size(); ++index) {
    const double amount = static_cast<double>(index) / static_cast<double>(points.size() - 1);
    const double inverse = 1.0 - amount;
    points[index] = {
        inverse * inverse * from.x + 2.0 * inverse * amount * control.x + amount * amount * to.x,
        inverse * inverse * from.y + 2.0 * inverse * amount * control.y + amount * amount * to.y,
    };
  }
  polyline(points, color, thickness, false, alpha);
}

void Canvas::cubic_curve(
    Point from,
    Point control0,
    Point control1,
    Point to,
    std::uint32_t color,
    double thickness,
    double alpha) {
  std::array<Point, 65> points{};
  for (std::size_t index = 0; index < points.size(); ++index) {
    const double amount = static_cast<double>(index) / static_cast<double>(points.size() - 1);
    const double inverse = 1.0 - amount;
    points[index] = {
        inverse * inverse * inverse * from.x +
            3.0 * inverse * inverse * amount * control0.x +
            3.0 * inverse * amount * amount * control1.x + amount * amount * amount * to.x,
        inverse * inverse * inverse * from.y +
            3.0 * inverse * inverse * amount * control0.y +
            3.0 * inverse * amount * amount * control1.y + amount * amount * amount * to.y,
    };
  }
  polyline(points, color, thickness, false, alpha);
}

void Canvas::fill_rounded_rectangle(
    double x,
    double y,
    double width,
    double height,
    double radius,
    std::uint32_t color,
    double alpha) {
  const double resolved = std::clamp(radius, 0.0, std::min(width, height) / 2.0);
  fill_rectangle(x + resolved, y, width - 2.0 * resolved, height, color, alpha);
  fill_rectangle(x, y + resolved, width, height - 2.0 * resolved, color, alpha);
  fill_circle(x + resolved, y + resolved, resolved, color, alpha);
  fill_circle(x + width - resolved, y + resolved, resolved, color, alpha);
  fill_circle(x + resolved, y + height - resolved, resolved, color, alpha);
  fill_circle(x + width - resolved, y + height - resolved, resolved, color, alpha);
}

void Canvas::horizontal_gradient(
    double x,
    double y,
    double width,
    double height,
    std::span<const std::uint32_t> colors) {
  if (colors.empty()) return;
  const int columns = std::max(1, static_cast<int>(std::ceil(width)));
  for (int column = 0; column < columns; ++column) {
    const double normalized = columns == 1 ? 0.0 : static_cast<double>(column) /
                                                  static_cast<double>(columns - 1);
    const double scaled = normalized * static_cast<double>(colors.size() - 1);
    const auto left = static_cast<std::size_t>(std::floor(scaled));
    const auto right = std::min(left + 1, colors.size() - 1);
    fill_rectangle(
        x + static_cast<double>(column),
        y,
        1.0,
        height,
        lerp_color(colors[left], colors[right], scaled - static_cast<double>(left)));
  }
}

void Canvas::text(
    std::string_view value,
    double x,
    double y,
    std::uint32_t color,
    double size,
    double alpha) {
  set_color(color, alpha);
  const float scale = static_cast<float>(std::max(0.5, size / 8.0));
  if (!SDL_SetRenderScale(renderer_, scale, scale)) {
    throw sdl_error("SDL could not scale example text");
  }
  const std::string terminated(value);
  const bool rendered = SDL_RenderDebugText(
      renderer_,
      static_cast<float>(x) / scale,
      static_cast<float>(y) / scale,
      terminated.c_str());
  static_cast<void>(SDL_SetRenderScale(renderer_, 1.0F, 1.0F));
  if (!rendered) throw sdl_error("SDL could not draw example text");
}

void Canvas::start_text_input() { window_.start_text_input(); }

void Canvas::stop_text_input() { window_.stop_text_input(); }

SDL_Window* Canvas::native_window() const noexcept { return window_.native_handle(); }

SDL_Renderer* Canvas::native_renderer() const noexcept { return renderer_; }

int SdlExample::run(
    int argc,
    char** argv,
    std::string_view title,
    FrameCallback callback,
    std::uint32_t background) {
  if (argc > 2 || (argc == 2 && std::string_view(argv[1]) != "--smoke")) {
    throw std::invalid_argument("the only supported option is --smoke");
  }
  const bool smoke = argc == 2;
  flight::host_sdl::Host host;
  Canvas canvas(title, smoke);
  Input input;
  const auto started = std::chrono::steady_clock::now();
  auto previous = started;
  std::size_t index = 0;
  bool running = true;
  while (running) {
    input.pointer_pressed = false;
    input.pointer_released = false;
    input.wheel_x = 0.0;
    input.wheel_y = 0.0;
    input.keys_pressed.clear();
    input.keys_released.clear();
    input.text.clear();

    SDL_Event event{};
    while (host.poll_event(event)) {
      switch (event.type) {
        case SDL_EVENT_QUIT:
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
          running = false;
          break;
        case SDL_EVENT_KEY_DOWN:
          if (!event.key.repeat) input.keys_pressed.push_back(event.key.key);
          input.held_keys.insert(event.key.key);
          if (event.key.key == SDLK_ESCAPE) running = false;
          break;
        case SDL_EVENT_KEY_UP:
          input.keys_released.push_back(event.key.key);
          input.held_keys.erase(event.key.key);
          break;
        case SDL_EVENT_MOUSE_MOTION:
          input.pointer_x = event.motion.x;
          input.pointer_y = event.motion.y;
          break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
          input.pointer_x = event.button.x;
          input.pointer_y = event.button.y;
          if (event.button.button == SDL_BUTTON_LEFT) {
            input.pointer_down = true;
            input.pointer_pressed = true;
          }
          break;
        case SDL_EVENT_MOUSE_BUTTON_UP:
          input.pointer_x = event.button.x;
          input.pointer_y = event.button.y;
          if (event.button.button == SDL_BUTTON_LEFT) {
            input.pointer_down = false;
            input.pointer_released = true;
          }
          break;
        case SDL_EVENT_MOUSE_WHEEL:
          input.wheel_x += static_cast<double>(event.wheel.x);
          input.wheel_y += static_cast<double>(event.wheel.y);
          break;
        case SDL_EVENT_TEXT_INPUT:
          input.text += event.text.text;
          break;
        default:
          break;
      }
    }

    const auto now = std::chrono::steady_clock::now();
    const double time = std::chrono::duration<double>(now - started).count();
    double delta_time = std::chrono::duration<double>(now - previous).count();
    previous = now;
    if (smoke) delta_time = 1.0 / 60.0;
    canvas.begin(background);
    callback(Frame{
        .canvas = canvas,
        .input = input,
        .time = time,
        .delta_time = std::min(delta_time, 0.1),
        .index = index,
        .smoke = smoke,
    });
    canvas.present();

    ++index;
    if (smoke && index >= 3) running = false;
    if (!smoke) std::this_thread::sleep_for(std::chrono::milliseconds(8));
  }
  return EXIT_SUCCESS;
}

std::array<float, 4> unpack_color(std::uint32_t color, double alpha) {
  return {
      static_cast<float>((color >> 24U) & 0xffU) / 255.0F,
      static_cast<float>((color >> 16U) & 0xffU) / 255.0F,
      static_cast<float>((color >> 8U) & 0xffU) / 255.0F,
      static_cast<float>(color & 0xffU) / 255.0F * static_cast<float>(std::clamp(alpha, 0.0, 1.0)),
  };
}

std::uint32_t lerp_color(std::uint32_t from, std::uint32_t to, double amount) {
  const auto from_rgba = unpack_color(from);
  const auto to_rgba = unpack_color(to);
  const double resolved = std::clamp(amount, 0.0, 1.0);
  const auto interpolate = [&](std::size_t index) {
    return channel(
        (static_cast<double>(from_rgba[index]) +
         (static_cast<double>(to_rgba[index]) - static_cast<double>(from_rgba[index])) * resolved) *
        255.0);
  };
  return (static_cast<std::uint32_t>(interpolate(0)) << 24U) |
         (static_cast<std::uint32_t>(interpolate(1)) << 16U) |
         (static_cast<std::uint32_t>(interpolate(2)) << 8U) |
         static_cast<std::uint32_t>(interpolate(3));
}

bool contains(
    double x,
    double y,
    double width,
    double height,
    double point_x,
    double point_y) {
  return point_x >= x && point_x <= x + width && point_y >= y && point_y <= y + height;
}

} // namespace flight::examples
