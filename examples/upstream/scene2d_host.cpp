#include "scene2d_host.hpp"

#include <flight/host_sdl/gl_app.hpp>
#include <flight/node/native_node2d_access.hpp>
#include <flight/path/tessellate_stroke_path.hpp>
#include <flight/shape/shape_stroke_outline.hpp>
#include <flight/render/render_proxy.hpp>
#include <flight/render/renderer.hpp>
#include <flight/render_gl/gl_render_pass.hpp>
#include <flight/render_gl/gl_render_state.hpp>
#include <flight/render_gl/gl_render_target.hpp>
#include <flight/scene2d_gl/gl_mesh_shape_renderer.hpp>
#include <flight/scene2d_gl/gl_node_2d.hpp>
#include <flight/scene2d_gl/gl_sprite.hpp>
#include <flight/scene2d_gl/gl_standard_material.hpp>

#include <SDL3/SDL_events.h>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

namespace flight::examples {
namespace {
template <typename T> auto readonly(const std::shared_ptr<T>& value) {
  return flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<T>>>>(value);
}
void capture(const flight::Uint8Array& pixels, int width, int height, const std::string& path) {
  std::ofstream output(path, std::ios::binary);
  output << "P6\n" << width << ' ' << height << "\n255\n";
  for (int y = height - 1; y >= 0; --y) for (int x = 0; x < width; ++x) {
    const auto offset = static_cast<std::size_t>((y * width + x) * 4);
    const char rgb[]{static_cast<char>(pixels[offset]), static_cast<char>(pixels[offset + 1]),
                     static_cast<char>(pixels[offset + 2])};
    output.write(rgb, 3);
  }
  if (!output) throw std::runtime_error("could not write GL capture: " + path);
}
}

bool SceneInput::key_down(SDL_Keycode key) const { return held_keys.contains(key); }
bool SceneInput::key_pressed(SDL_Keycode key) const {
  return std::ranges::find(keys_pressed, key) != keys_pressed.end();
}

int Scene2DHost::run(
    int argc, char** argv, flight::types::Node2D root,
    FrameCallback frame, CheckCallback check,
    const Scene2DHostOptions& options, SetupCallback setup) {
  bool smoke = false;
  std::optional<std::string> capture_path;
  for (int i = 1; i < argc; ++i) {
    const std::string_view argument(argv[i]);
    if (argument == "--smoke") smoke = true;
    else if (argument == "--capture" && i + 1 < argc) capture_path = argv[++i];
    else throw std::runtime_error("usage: " + std::string(options.name) + " [--smoke] [--capture frame.ppm]");
  }
  if (smoke && !check) throw std::invalid_argument("Scene2D smoke requires its subject verifier");
  flight::host_sdl::GlApplication app({.title = std::string(options.title),
      .width = options.width, .height = options.height, .hidden = smoke});
  auto gl = app.context();
  auto render_options = flight::make_ref<flight::types::GlRenderStateOptions>();
  render_options->pixel_ratio = 1;
  render_options->scene_graph_sync_policy = flight::String("requiresInvalidation");
  render_options->blend_realizations = flight::render_gl::standard_gl_blend_realizations;
  render_options->texture_resolvers = flight::render_gl::standard_gl_texture_resolvers;
  // Use the SDK's default compact outline for open strokes (including touching
  // arrow shaft/head subpaths), and its ring tessellator for closed borders.
  // The strict ring strategy intentionally rejects intersecting open subpaths.
  render_options->stroke_tessellator = flight::types::StrokeTessellator(
      [](auto path, auto style, std::optional<double> tolerance) -> std::optional<flight::Ref<flight::types::PathMesh>> {
        if (flight::shape::is_centerline_closed(path))
          return flight::path::tessellate_stroke_path(path, style, tolerance);
        return flight::path::tessellate_path(flight::shape::compact_stroke_path(path, style, tolerance));
      });
  auto state = flight::render_gl::create_gl_render_state(gl, readonly(render_options));
  flight::render::register_node_renderer(state, flight::types::display_object_kind, flight::scene2d_gl::gl_scene_2d_renderer);
  flight::render::register_node_renderer(state, flight::types::shape_kind, flight::scene2d_gl::gl_mesh_shape_renderer);
  flight::render::register_node_renderer(state, flight::types::sprite_kind, flight::scene2d_gl::gl_sprite_renderer);
  flight::scene2d_gl::register_gl_standard_material(state);
  if (setup) setup(state);
  auto screen = flight::render_gl::create_gl_screen_render_target(gl);
  auto clear = flight::make_ref<flight::types::RenderTargetClear>();
  const auto background = options.background;
  clear->color = flight::Array<double>{static_cast<double>((background >> 24U) & 255U) / 255,
      static_cast<double>((background >> 16U) & 255U) / 255,
      static_cast<double>((background >> 8U) & 255U) / 255, static_cast<double>(background & 255U) / 255};
  SceneInput input;
  input.pointer_x = options.width / 2.0;
  input.pointer_y = options.height / 2.0;
  std::size_t verified = 0, maximum_coverage = 0;
  flight::host_sdl::GlLoopOptions loop;
  loop.maximum_delta_time = options.maximum_delta_time;
  if (smoke) { loop.fixed_delta_time = 1.0 / 60.0; loop.maximum_frames = 3; }
  app.run([&](const flight::host_sdl::GlFrame& timing) {
    if (timing.resized) {
      flight::row_set<flight::RowKey<"scaleX">>(root, static_cast<double>(timing.pixel_size.width) / options.width);
      flight::row_set<flight::RowKey<"scaleY">>(root, static_cast<double>(timing.pixel_size.height) / options.height);
      flight::node::invalidate_node_local_transform(root);
    }
    frame(Scene2DFrame{input, timing.elapsed_time, timing.delta_time, timing.index, smoke});
    flight::render::prepare_scene_2d_render(state, root);
    auto pass = flight::render_gl::begin_gl_render_pass(state, screen, readonly(clear));
    flight::scene2d_gl::render_gl_scene_2d(pass, root);
    flight::render_gl::end_gl_render_pass(pass);
    if (gl.get_error() != gl.no_error) throw std::runtime_error("Scene2D subject GL draw failed");
    if (smoke || (capture_path && timing.index == 0)) {
      gl.finish();
      const auto size = timing.pixel_size;
      flight::Uint8Array pixels(static_cast<std::size_t>(size.width) * static_cast<std::size_t>(size.height) * 4U);
      gl.read_pixels(0, 0, size.width, size.height, gl.rgba, gl.unsigned_byte, pixels);
      if (gl.get_error() != gl.no_error) throw std::runtime_error("Scene2D GL readback failed");
      std::size_t coverage = 0;
      for (std::size_t i = 0; i < pixels.size(); i += 4) {
        if (pixels[i] != ((background >> 24U) & 255U) || pixels[i + 1] != ((background >> 16U) & 255U) ||
            pixels[i + 2] != ((background >> 8U) & 255U)) ++coverage;
      }
      if (smoke) {
        if (coverage < static_cast<std::size_t>(size.width) * static_cast<std::size_t>(size.height) / 1000U)
          throw std::runtime_error("Scene2D subject framebuffer is effectively empty");
        check(Scene2DRenderFrame{state, pixels, size.width, size.height, timing.index});
        ++verified;
        maximum_coverage = std::max(maximum_coverage, coverage);
      }
      if (capture_path && timing.index == 0) capture(pixels, size.width, size.height, *capture_path);
    }
    input.keys_pressed.clear();
    input.wheel_y = 0;
    input.pointer_pressed = input.pointer_released = false;
  }, loop, [&](const SDL_Event& event) {
    const auto pointer = [&](double x, double y) {
      const auto size = app.canvas().logical_size();
      input.pointer_x = x * options.width / size.width;
      input.pointer_y = y * options.height / size.height;
    };
    if (event.type == SDL_EVENT_KEY_DOWN) {
      input.held_keys.insert(event.key.key);
      input.keys_pressed.push_back(event.key.key);
    } else if (event.type == SDL_EVENT_KEY_UP) input.held_keys.erase(event.key.key);
    else if (event.type == SDL_EVENT_MOUSE_MOTION) pointer(event.motion.x, event.motion.y);
    else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
      pointer(event.button.x, event.button.y);
      if (event.button.button == SDL_BUTTON_LEFT) {
        input.pointer_down = event.type == SDL_EVENT_MOUSE_BUTTON_DOWN;
        input.pointer_pressed = input.pointer_down;
        input.pointer_released = !input.pointer_down;
      }
    } else if (event.type == SDL_EVENT_MOUSE_WHEEL) input.wheel_y += event.wheel.y;
  });
  if (smoke) {
    if (verified != 3) throw std::runtime_error("Scene2D smoke did not verify every authored frame");
    std::cout << "scene2d-gl " << options.name << " smoke: rendered " << verified << " verified frames, "
              << flight::render_gl::get_gl_render_state_runtime(state)->render_proxy_sources.size()
              << " nodes, maximum non-background coverage " << maximum_coverage << '\n';
  }
  flight::render::dispose_scene_2d_render(state, root);
  return EXIT_SUCCESS;
}

} // namespace flight::examples
