#include <flight/host_sdl/gl_app.hpp>

#include <SDL3/SDL_opengles2.h>

#include <array>
#include <bit>
#include <cmath>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>

namespace {
void require(bool value, const char* message) {
  if (!value) throw std::runtime_error(message);
}

void check_array_uniform_ranges(const flight::host_sdl::WebGl2Context& gl) {
  const auto vertex = gl.create_shader(gl.vertex_shader);
  const auto fragment = gl.create_shader(gl.fragment_shader);
  require(vertex.has_value() && fragment.has_value(), "could not create uniform-range shaders");
  gl.shader_source(vertex, flight::String(
      "#version 300 es\nvoid main() { gl_Position = vec4(0.0, 0.0, 0.0, 1.0); }\n"));
  gl.shader_source(fragment, flight::String(
      "#version 300 es\nprecision highp float;\n"
      "uniform float u_scalar; uniform vec2 u_pair; uniform vec3 u_triple; uniform vec4 u_quad;\n"
      "out vec4 color;\nvoid main() {\n"
      "color = vec4(u_scalar) + vec4(u_pair, 0.0, 0.0) + vec4(u_triple, 0.0) + u_quad;\n}\n"));
  gl.compile_shader(vertex);
  gl.compile_shader(fragment);
  require(gl.get_shader_parameter(vertex, gl.compile_status) == 1 &&
              gl.get_shader_parameter(fragment, gl.compile_status) == 1,
          "uniform-range shaders did not compile");
  const auto program = gl.create_program();
  gl.attach_shader(program, vertex);
  gl.attach_shader(program, fragment);
  gl.link_program(program);
  require(gl.get_program_parameter(program, gl.link_status) == 1, "uniform-range program did not link");
  gl.use_program(program);
  // Read the real driver values through the existing host GL entry-point loader.
  const auto read_uniform = std::bit_cast<PFNGLGETUNIFORMFVPROC>(gl.function_address("glGetUniformfv"));
  const std::array<double, 4> numbers{0.1, -0.2, 1.0 / 3.0, 1.23456789};
  const auto verify = [&](const char* name, std::size_t width, auto upload) {
    const auto location = gl.get_uniform_location(program, flight::String(name));
    require(location.has_value(), "uniform-range fixture was optimized out");
    flight::Array<double> exact;
    flight::Array<double> prefixed{99.0, 98.0};
    for (std::size_t i = 0; i < width; ++i) {
      exact.push(numbers[i]);
      prefixed.push(numbers[i]);
    }
    flight::Float32Array sentinel(width);
    for (std::size_t i = 0; i < width; ++i) sentinel[i] = -99.0F;
    const auto reset_values = [&] {
      // Use the unchanged two-argument typed overloads, independently of the range path under test.
      switch (width) {
        case 1: gl.uniform1fv(location, sentinel); break;
        case 2: gl.uniform2fv(location, sentinel); break;
        case 3: gl.uniform3fv(location, sentinel); break;
        case 4: gl.uniform4fv(location, sentinel); break;
      }
    };
    const auto check_values = [&] {
      std::array<float, 4> actual{};
      read_uniform(program.native_name(), static_cast<GLint>(location->native_name()), actual.data());
      for (std::size_t i = 0; i < width; ++i) {
        require(actual[i] == static_cast<float>(numbers[i]), "numeric-array uniform selected the wrong values");
      }
      require(gl.get_error() == gl.no_error, "uniform-range upload/readback raised a GL error");
    };
    reset_values();
    upload(location, exact, std::nullopt, std::nullopt);
    check_values();
    reset_values();
    upload(location, exact, 0.0, std::nullopt);
    check_values();
    reset_values();
    upload(location, prefixed, 2.0, std::nullopt);
    check_values();
    reset_values();
    upload(location, prefixed, 2.0, 0.0);
    check_values();
    auto suffixed = prefixed.slice();
    suffixed.push(97.0);
    reset_values();
    upload(location, suffixed, 2.0, static_cast<double>(width));
    check_values();
    reset_values();
    upload(location, exact, 0.0, static_cast<double>(width));
    check_values();

    flight::Float32Array typed(width + 2);
    typed[0] = 99.0F;
    typed[1] = 98.0F;
    for (std::size_t i = 0; i < width; ++i) typed[i + 2] = static_cast<float>(numbers[i]);
    reset_values();
    upload(location, typed, 2.0, static_cast<double>(width));
    check_values();

    const auto require_range_error = [&](auto action) {
      bool rejected = false;
      try { action(); } catch (const std::range_error&) { rejected = true; }
      require(rejected, "invalid numeric-array uniform range did not throw");
    };
    require_range_error([&] { upload(location, exact, static_cast<double>(width + 1), std::nullopt); });
    require_range_error([&] { upload(location, exact, 0.0, static_cast<double>(width + 1)); });
    require_range_error([&] { upload(location, exact, -1.0, std::nullopt); });
    require_range_error([&] { upload(location, exact, 0.5, std::nullopt); });
    check_values();
  };
  verify("u_scalar", 1, [&](auto location, const auto& values, auto offset, auto length) {
    gl.uniform1fv(location, values, offset, length);
  });
  verify("u_pair", 2, [&](auto location, const auto& values, auto offset, auto length) {
    gl.uniform2fv(location, values, offset, length);
  });
  verify("u_triple", 3, [&](auto location, const auto& values, auto offset, auto length) {
    gl.uniform3fv(location, values, offset, length);
  });
  verify("u_quad", 4, [&](auto location, const auto& values, auto offset, auto length) {
    gl.uniform4fv(location, values, offset, length);
  });
  gl.use_program(std::nullopt);
  gl.delete_program(program);
  gl.delete_shader(vertex);
  gl.delete_shader(fragment);
  require(gl.get_error() == gl.no_error, "uniform-range teardown raised a GL error");
  std::cout << "Numeric-array uniform ranges passed all four widths with actual float readback\n";
}

void check_attribute_locations(const flight::host_sdl::WebGl2Context& gl) {
  using namespace flight::host_sdl;
  static_assert(std::is_same_v<decltype(gl.get_attrib_location(
      std::declval<const WebGlProgram&>(), std::declval<const flight::String&>())), double>);

  const auto vertex = gl.create_shader(gl.vertex_shader);
  const auto fragment = gl.create_shader(gl.fragment_shader);
  require(vertex.has_value() && fragment.has_value(), "could not create attribute-location shaders");
  gl.shader_source(vertex, flight::String(
      "#version 300 es\nlayout(location = 5) in vec2 a_position;\n"
      "void main() { gl_Position = vec4(a_position, 0.0, 1.0); }\n"));
  gl.shader_source(fragment, flight::String(
      "#version 300 es\nprecision mediump float;\nout vec4 color;\n"
      "void main() { color = vec4(1.0); }\n"));
  gl.compile_shader(vertex);
  gl.compile_shader(fragment);
  require(gl.get_shader_parameter(vertex, gl.compile_status) == 1 &&
              gl.get_shader_parameter(fragment, gl.compile_status) == 1,
          "attribute-location shaders did not compile");
  const auto program = gl.create_program();
  gl.attach_shader(program, vertex);
  gl.attach_shader(program, fragment);
  gl.link_program(program);
  require(gl.get_program_parameter(program, gl.link_status) == 1,
          "attribute-location program did not link");

  // Generated shader records use number (double) fields and aggregate initialization.
  struct Locations { double active; double missing; };
  const Locations locations{
      gl.get_attrib_location(program, flight::String("a_position")),
      gl.get_attrib_location(program, flight::String("a_missing"))};
  require(locations.active == 5.0, "attribute location lost its numeric value");
  require(locations.missing == -1.0, "missing attribute location lost its negative sentinel");
  gl.delete_program(program);
  gl.delete_shader(vertex);
  gl.delete_shader(fragment);
  require(gl.get_error() == gl.no_error, "attribute-location query or teardown raised a GL error");
}
}

int main() {
  try {
    using namespace flight::host_sdl;
    GlApplication app({.title = "Flight installed GL app test", .width = 64, .height = 48, .hidden = true});
    const auto& gl = app.context();
    check_array_uniform_ranges(gl);
    check_attribute_locations(gl);

    // `getParameter(VIEWPORT) as ArrayLike<number> | null` through the nullable carrier: an engaged
    // view of the four integers, and a non-sequence parameter refused rather than read as null.
    gl.viewport(1, 2, 30, 40);
    const auto box = static_cast<std::optional<flight::SequenceView<double>>>(gl.get_parameter(gl.viewport_constant));
    require(box.has_value() && box->length == 4 && (*box)[0] == 1 && (*box)[1] == 2 && (*box)[2] == 30 &&
                (*box)[3] == 40,
            "VIEWPORT did not answer its four integers through the nullable sequence carrier");
    bool refused_non_sequence = false;
    try { (void)static_cast<std::optional<flight::SequenceView<double>>>(gl.get_parameter(gl.renderbuffer_binding)); }
    catch (const std::logic_error&) { refused_non_sequence = true; }
    require(refused_non_sequence, "a non-sequence parameter was read through the nullable sequence carrier");
    require(gl.get_error() == gl.no_error, "the viewport query raised a GL error");

    const auto id = SDL_GetWindowID(app.canvas().native_window());
    SDL_Event foreign_close{};
    foreign_close.type = SDL_EVENT_WINDOW_CLOSE_REQUESTED;
    foreign_close.window.windowID = id + 1;
    require(SDL_PushEvent(&foreign_close), "could not queue foreign-window event");
    bool forwarded = false, rejected_recursive = false;
    GlLoopOptions loop;
    loop.fixed_delta_time = 0.025;
    loop.maximum_frames = 3;
    loop.frame_delay = std::chrono::milliseconds(0);
    const auto frames = app.run([&](const GlFrame& frame) {
      require(frame.delta_time == 0.025 && std::abs(frame.elapsed_time - static_cast<double>(frame.index) * 0.025) < 1e-12,
              "fixed frame timing or index changed");
      if (frame.index == 0) {
        require(forwarded && frame.resized && frame.logical_size == WindowSize{64, 48},
                "first-frame sizes or event-before-frame ordering changed");
        app.resize(96, 64);
        try { app.run([](const GlFrame&) {}, loop); }
        catch (const std::logic_error&) { rejected_recursive = true; }
      } else {
        require(frame.resized && frame.logical_size == WindowSize{96, 64} &&
                    frame.pixel_size == app.canvas().pixel_size() &&
                    app.canvas().width == 96 && app.canvas().height == 64,
                "resized window did not reach the next frame and canvas");
        app.request_shutdown();
      }
      gl.viewport(0, 0, frame.pixel_size.width, frame.pixel_size.height);
      gl.clear_color(1, 0, 1, 1);
      gl.clear(gl.color_buffer_bit);
      flight::Uint8Array pixel(4U);
      gl.read_pixels(0, 0, 1, 1, gl.rgba, gl.unsigned_byte, pixel);
      require(gl.get_error() == gl.no_error && pixel[0] == 255 && pixel[1] == 0 && pixel[2] == 255,
              "frame callback did not render into the current real GL surface");
    }, loop, [&](const SDL_Event& event) {
      if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID != id) forwarded = true;
    });
    require(frames == 2 && rejected_recursive, "shutdown or recursive-loop guard changed");

    bool propagated = false;
    try { app.run([](const GlFrame&) { throw std::runtime_error("frame failure"); }, loop); }
    catch (const std::runtime_error& error) { propagated = std::string_view(error.what()) == "frame failure"; }
    require(propagated, "frame exception was swallowed");
    loop.maximum_frames = 1;
    require(app.run([](const GlFrame&) {}, loop) == 1, "exception left the loop running");
    SDL_Event close{};
    close.type = SDL_EVENT_WINDOW_CLOSE_REQUESTED;
    close.window.windowID = id;
    require(SDL_PushEvent(&close), "could not queue close event");
    require(app.run([](const GlFrame&) { throw std::runtime_error("rendered after window close"); }, loop) == 0,
            "own-window close did not stop before another frame");
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
