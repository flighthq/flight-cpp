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

void validate_gl_resource_path(const flight::host_sdl::WebGl2Context& gl) {
  auto texture = gl.create_texture();
  if (!texture) throw std::runtime_error("OpenGL could not allocate the tween smoke texture");
  const flight::Uint8Array pixels{
      255, 0, 0, 255,
      0, 255, 0, 255,
      0, 0, 255, 255,
      255, 255, 255, 255,
  };
  gl.bind_texture(flight::host_sdl::WebGl2Context::texture_2_d, texture);
  gl.tex_image2_d(
      flight::host_sdl::WebGl2Context::texture_2_d,
      0,
      flight::host_sdl::WebGl2Context::rgba8,
      2,
      2,
      0,
      flight::host_sdl::WebGl2Context::rgba,
      flight::host_sdl::WebGl2Context::unsigned_byte,
      flight::ArrayBufferView(pixels));
  gl.tex_sub_image2_d(
      flight::host_sdl::WebGl2Context::texture_2_d,
      0,
      0,
      0,
      1,
      1,
      flight::host_sdl::WebGl2Context::rgba,
      flight::host_sdl::WebGl2Context::unsigned_byte,
      flight::ArrayBufferView(pixels));
  gl.tex_parameteri(
      flight::host_sdl::WebGl2Context::texture_2_d,
      flight::host_sdl::WebGl2Context::texture_min_filter,
      flight::host_sdl::WebGl2Context::nearest);
  auto framebuffer = gl.create_framebuffer();
  gl.bind_framebuffer(flight::host_sdl::WebGl2Context::framebuffer, framebuffer);
  gl.framebuffer_texture2_d(
      flight::host_sdl::WebGl2Context::framebuffer,
      flight::host_sdl::WebGl2Context::color_attachment0,
      flight::host_sdl::WebGl2Context::texture_2_d,
      texture,
      0);
  if (gl.check_framebuffer_status(flight::host_sdl::WebGl2Context::framebuffer) !=
      flight::host_sdl::WebGl2Context::framebuffer_complete) {
    throw std::runtime_error("OpenGL could not complete the tween smoke framebuffer");
  }
  gl.clear_bufferfv(
      flight::host_sdl::WebGl2Context::color,
      0,
      flight::Float32Array{0.25F, 0.5F, 0.75F, 1.0F});
  flight::Uint8Array readback(16);
  gl.read_pixels(
      0,
      0,
      2,
      2,
      flight::host_sdl::WebGl2Context::rgba,
      flight::host_sdl::WebGl2Context::unsigned_byte,
      flight::ArrayBufferView(readback));
  gl.bind_framebuffer(flight::host_sdl::WebGl2Context::framebuffer, std::nullopt);
  gl.delete_framebuffer(framebuffer);
  gl.delete_texture(texture);
  if (framebuffer || texture) {
    throw std::runtime_error("deleting WebGL texture resources did not invalidate their aliases");
  }

  auto volume_texture = gl.create_texture();
  const flight::Uint8Array volume_pixels(32);
  gl.bind_texture(flight::host_sdl::WebGl2Context::texture_3_d, volume_texture);
  gl.tex_image3_d(
      flight::host_sdl::WebGl2Context::texture_3_d,
      0,
      flight::host_sdl::WebGl2Context::rgba8,
      2,
      2,
      2,
      0,
      flight::host_sdl::WebGl2Context::rgba,
      flight::host_sdl::WebGl2Context::unsigned_byte,
      flight::ArrayBufferView(volume_pixels));
  gl.delete_texture(volume_texture);

  auto array_texture = gl.create_texture();
  gl.bind_texture(flight::host_sdl::WebGl2Context::texture_2_d_array, array_texture);
  gl.tex_storage3_d(
      flight::host_sdl::WebGl2Context::texture_2_d_array,
      1,
      flight::host_sdl::WebGl2Context::rgba8,
      2,
      2,
      1);
  gl.delete_texture(array_texture);

  auto buffer = gl.create_buffer();
  if (!buffer) throw std::runtime_error("OpenGL could not allocate the tween smoke buffer");
  const flight::Float32Array vertices{-1.0F, -1.0F, 1.0F, -1.0F, 0.0F, 1.0F};
  gl.bind_buffer(flight::host_sdl::WebGl2Context::array_buffer, buffer);
  gl.buffer_data(
      flight::host_sdl::WebGl2Context::array_buffer,
      flight::ArrayBufferView(vertices),
      flight::host_sdl::WebGl2Context::static_draw);
  gl.buffer_sub_data(
      flight::host_sdl::WebGl2Context::array_buffer,
      0,
      flight::ArrayBufferView(vertices),
      2,
      2);
  gl.bind_buffer(flight::host_sdl::WebGl2Context::array_buffer, std::nullopt);
  gl.delete_buffer(buffer);
  if (buffer) throw std::runtime_error("deleting a WebGL buffer did not invalidate its aliases");

  const auto compile = [&](std::uint32_t type, const char* source) {
    auto shader = gl.create_shader(type);
    if (!shader) throw std::runtime_error("OpenGL could not allocate a tween smoke shader");
    gl.shader_source(*shader, flight::String(source));
    gl.compile_shader(*shader);
    if (gl.get_shader_parameter(*shader, flight::host_sdl::WebGl2Context::compile_status) == 0.0) {
      const auto log = gl.get_shader_info_log(*shader).value_or(flight::String()).to_utf8();
      gl.delete_shader(shader);
      throw std::runtime_error("OpenGL rejected a tween smoke shader: " + log);
    }
    return *shader;
  };

  auto vertex_shader = compile(
      flight::host_sdl::WebGl2Context::vertex_shader,
      "#version 300 es\n"
      "const vec2 p[3] = vec2[3](vec2(-0.5, -0.5), vec2(0.5, -0.5), vec2(0.0, 0.5));\n"
      "void main() { gl_Position = vec4(p[gl_VertexID], 0.0, 1.0); }\n");
  auto fragment_shader = compile(
      flight::host_sdl::WebGl2Context::fragment_shader,
      "#version 300 es\nprecision mediump float;\nuniform vec4 u_color;\nout vec4 color;\n"
      "void main() { color = u_color; }\n");
  auto program = gl.create_program();
  if (!program) throw std::runtime_error("OpenGL could not allocate the tween smoke program");
  gl.attach_shader(program, vertex_shader);
  gl.attach_shader(program, fragment_shader);
  gl.link_program(program);
  if (gl.get_program_parameter(program, flight::host_sdl::WebGl2Context::link_status) == 0.0) {
    const auto log = gl.get_program_info_log(program).value_or(flight::String()).to_utf8();
    gl.delete_program(program);
    gl.delete_shader(vertex_shader);
    gl.delete_shader(fragment_shader);
    throw std::runtime_error("OpenGL rejected the tween smoke program: " + log);
  }
  gl.use_program(program);
  const auto active_uniform = gl.get_active_uniform(program, 0);
  if (!active_uniform || active_uniform->name != flight::String("u_color") ||
      active_uniform->size != 1.0 ||
      active_uniform->type != flight::host_sdl::WebGl2Context::float_vec4) {
    throw std::runtime_error("OpenGL returned unexpected tween smoke uniform metadata");
  }
  const auto color_location = gl.get_uniform_location(program, flight::String("u_color"));
  if (!color_location) throw std::runtime_error("OpenGL removed the tween smoke uniform");
  gl.uniform4f(color_location, 1.0F, 1.0F, 1.0F, 1.0F);
  auto vertex_array = gl.create_vertex_array();
  if (!vertex_array) throw std::runtime_error("OpenGL could not allocate the tween smoke vertex array");
  gl.bind_vertex_array(vertex_array);
  gl.draw_arrays(flight::host_sdl::WebGl2Context::triangles, 0, 3);
  gl.bind_vertex_array(std::nullopt);
  gl.delete_vertex_array(vertex_array);
  gl.use_program(std::nullopt);
  gl.delete_program(program);
  gl.delete_shader(vertex_shader);
  gl.delete_shader(fragment_shader);
  if (program || vertex_shader || fragment_shader || vertex_array) {
    throw std::runtime_error("deleting WebGL resources did not invalidate their aliases");
  }
  if (gl.get_error() != flight::host_sdl::WebGl2Context::no_error) {
    throw std::runtime_error("OpenGL rejected the tween resource smoke path");
  }
}

int run(bool smoke) {
  using namespace std::chrono_literals;

  flight::host_sdl::Host host;
  auto canvas = flight::host_sdl::GlCanvas::create(
      960, 720, 1.0, flight::String("Flight tween - SDL + OpenGL ES"), smoke);
  auto context = canvas.get_context();
  context.make_current();
  validate_gl_resource_path(context);

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
