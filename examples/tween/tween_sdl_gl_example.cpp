#include "generated/tween.hpp"

#include <flight/host_sdl/host.hpp>
#include <flight/host_sdl/webgl.hpp>

#include <SDL3/SDL_keycode.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <thread>

namespace {

struct Color {
  float red;
  float green;
  float blue;
};

void fill_rectangle(
    const flight::host_sdl::WebGl2Context& gl,
    flight::host_sdl::WindowSize viewport,
    int x,
    int y,
    int width,
    int height,
    Color color) {
  const int left = std::clamp(x, 0, viewport.width);
  const int bottom = std::clamp(y, 0, viewport.height);
  const int right = std::clamp(x + width, 0, viewport.width);
  const int top = std::clamp(y + height, 0, viewport.height);
  if (right <= left || top <= bottom) return;
  gl.scissor(left, bottom, right - left, top - bottom);
  gl.clear_color(color.red, color.green, color.blue, 1.0F);
  gl.clear(flight::host_sdl::WebGl2Context::color_buffer_bit);
}

void fill_circle(
    const flight::host_sdl::WebGl2Context& gl,
    flight::host_sdl::WindowSize viewport,
    int center_x,
    int center_y,
    int radius,
    Color color) {
  const int radius_squared = radius * radius;
  for (int offset_y = -radius; offset_y <= radius; ++offset_y) {
    const auto extent = static_cast<int>(
        std::sqrt(static_cast<double>(radius_squared - (offset_y * offset_y))));
    fill_rectangle(
        gl,
        viewport,
        center_x - extent,
        center_y + offset_y,
        (extent * 2) + 1,
        1,
        color);
  }
}

void draw_frame(
    const flight::host_sdl::WebGl2Context& gl,
    flight::host_sdl::WindowSize size,
    double progress) {
  constexpr Color background{0.035F, 0.047F, 0.075F};
  constexpr Color track{0.16F, 0.19F, 0.25F};
  constexpr std::array<Color, 15> colors{{
      {0.96F, 0.38F, 0.38F},
      {0.96F, 0.55F, 0.30F},
      {0.96F, 0.72F, 0.28F},
      {0.78F, 0.80F, 0.28F},
      {0.53F, 0.82F, 0.34F},
      {0.30F, 0.82F, 0.53F},
      {0.25F, 0.80F, 0.72F},
      {0.23F, 0.72F, 0.88F},
      {0.30F, 0.58F, 0.96F},
      {0.45F, 0.48F, 0.96F},
      {0.62F, 0.42F, 0.94F},
      {0.77F, 0.38F, 0.88F},
      {0.90F, 0.36F, 0.72F},
      {0.96F, 0.36F, 0.56F},
      {0.96F, 0.42F, 0.44F},
  }};

  gl.viewport(0, 0, size.width, size.height);
  gl.disable(flight::host_sdl::WebGl2Context::scissor_test);
  gl.clear_color(background.red, background.green, background.blue, 1.0F);
  gl.clear(flight::host_sdl::WebGl2Context::color_buffer_bit);
  gl.enable(flight::host_sdl::WebGl2Context::scissor_test);

  const auto values = flighthq_examples_tween::sample_tween_curves(progress);
  if (values.size() != colors.size()) throw std::runtime_error("unexpected tween curve count");

  const int horizontal_margin = std::max(28, size.width / 16);
  const int vertical_margin = std::max(20, size.height / 24);
  const int available_height = std::max(1, size.height - (vertical_margin * 2));
  const int row_height = std::max(1, available_height / static_cast<int>(colors.size()));
  const int radius = std::clamp(row_height / 4, 4, 12);
  const int track_left = horizontal_margin + radius;
  const int track_right = std::max(track_left, size.width - horizontal_margin - radius);
  const int track_width = track_right - track_left;

  for (std::size_t index = 0; index < colors.size(); ++index) {
    const int center_y =
        size.height - vertical_margin -
        static_cast<int>((static_cast<double>(index) + 0.5) * static_cast<double>(row_height));
    fill_rectangle(gl, size, track_left, center_y - 1, track_width, 3, track);
    const double value = std::clamp(values[index], 0.0, 1.0);
    const int center_x = track_left + static_cast<int>(std::lround(value * static_cast<double>(track_width)));
    fill_circle(gl, size, center_x, center_y, radius, colors[index]);
  }
  gl.disable(flight::host_sdl::WebGl2Context::scissor_test);
  if (gl.get_error() != flight::host_sdl::WebGl2Context::no_error) {
    throw std::runtime_error("OpenGL rejected the tween frame");
  }
}

int run(bool smoke) {
  using namespace std::chrono_literals;

  flight::host_sdl::Host host;
  auto canvas = flight::host_sdl::GlCanvas::create(
      960, 720, 1.0, flight::String("Flight tween - SDL + OpenGL ES"), smoke);
  auto context = canvas.get_context();
  context.make_current();

  const std::uint64_t started = flight::host_sdl::Host::ticks_nanoseconds();
  std::size_t frames = 0;
  bool running = true;
  while (running) {
    SDL_Event event{};
    while (host.poll_event(event)) {
      if (event.type == SDL_EVENT_QUIT ||
          (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE)) {
        running = false;
      }
    }

    const std::uint64_t elapsed = flight::host_sdl::Host::ticks_nanoseconds() - started;
    const double seconds = static_cast<double>(elapsed) / 1'000'000'000.0;
    const double cycle = std::fmod(seconds / 2.0, 2.0);
    const double progress = cycle <= 1.0 ? cycle : 2.0 - cycle;
    draw_frame(context, canvas.pixel_size(), progress);
    context.present();

    ++frames;
    if (smoke && frames >= 3) running = false;
    std::this_thread::sleep_for(8ms);
  }
  return 0;
}

} // namespace

int main(int argc, char** argv) {
  if (argc > 2 || (argc == 2 && std::string_view(argv[1]) != "--smoke")) {
    std::cerr << "usage: flight_cpp_tween_sdl_gl_example [--smoke]\n";
    return 2;
  }
  try {
    return run(argc == 2);
  } catch (const std::exception& error) {
    std::cerr << "Flight SDL tween failed: " << error.what() << '\n';
    return 1;
  }
}
