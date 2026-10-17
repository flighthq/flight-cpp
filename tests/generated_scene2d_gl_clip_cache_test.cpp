// The per-context clip program cache of @flighthq/scene2d-gl's glClipContours.ts at a627849, which the
// emitter refused and overrides/include/flight/scene2d_gl/gl_clip_contours.hpp supplies, together with the
// three host-binding spellings that override corrects in the same bodies.
//
// Everything runs against one actual offscreen GlApplication context and a real GL render state:
// - the first ensure_clip_program is a miss that compiles and links the clip program, creates its buffer
//   and looks up a_position / u_worldMatrix / u_projection, and caches that owner under the context;
// - a second render state over a copy of the context is a hit: no second entry, the identical owner;
// - upload_clip_uniforms writes both matrices through the real uniform locations, and the projection reads
//   the live drawing-buffer size through the host accessors when no render target viewport is set;
// - one push/pop of a real contour disables CULL_FACE, enables and then releases the stencil test, and
//   tracks the mask depth, with no GL error.
// This proves the supplied cache and the corrected bindings run. It does not claim the clip renderer, or
// gl_clip.hpp, which still fails on an unrelated cluster, is complete.
#include <flight/host_sdl/gl_app.hpp>
#include <flight/render_gl/gl_render_state.hpp>
#include <flight/scene2d_gl/gl_clip_contours.hpp>

#include <cmath>
#include <exception>
#include <iostream>
#include <optional>

namespace {

namespace scene2d_gl = flight::scene2d_gl;
namespace types = flight::types;
using flight::host_sdl::WebGl2Context;

int failures = 0;

void check(bool condition, const char* message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<types::Matrix>>>> identity_matrix() {
  return flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<types::Matrix>>>>(
      flight::make_ref<types::Matrix>(types::Matrix{.a = 1.0, .b = 0.0, .c = 0.0, .d = 1.0, .tx = 0.0, .ty = 0.0}));
}

void check_live_clip_cache() {
  flight::host_sdl::GlApplication app({.title = "Flight scene2d-gl clip cache", .width = 32, .height = 24, .hidden = true});
  const WebGl2Context gl = app.context();
  const auto state = flight::render_gl::create_gl_render_state(gl);
  check(!scene2d_gl::clip_programs.has(gl), "the live clip cache was not initially empty");

  // Miss: compile, link, buffer, locations, cached under the context.
  scene2d_gl::ensure_clip_program(state);
  const auto cached = scene2d_gl::clip_programs.get(gl);
  check(cached.has_value(), "the live miss did not cache its clip program");
  if (!cached.has_value()) return;
  const auto program = cached.value();
  check(static_cast<bool>(program->program) && gl.get_program_parameter(program->program, gl.link_status) == 1,
        "the live miss did not produce a linked clip program");
  check(static_cast<bool>(program->buffer), "the live miss did not create the contour buffer");
  check(program->position_location >= 0 &&
            program->position_location == gl.get_attrib_location(program->program, flight::String("a_position")),
        "the cached a_position location is not the program's own");
  const auto world = gl.get_uniform_location(program->program, flight::String("u_worldMatrix"));
  const auto projection = gl.get_uniform_location(program->program, flight::String("u_projection"));
  check(world && projection && program->world_matrix_location && program->projection_location &&
            program->world_matrix_location->native_name() == world->native_name() &&
            program->projection_location->native_name() == projection->native_name(),
        "the cached uniform locations are not the program's own");
  check(scene2d_gl::clip_program_for(state) == program, "clip_program_for did not return the cached owner");

  // Hit: a second render state over a copy of the same context reuses the identical owner.
  const WebGl2Context copy = gl;
  const auto second = flight::render_gl::create_gl_render_state(copy);
  scene2d_gl::ensure_clip_program(second);
  check(scene2d_gl::clip_program_for(second) == program && scene2d_gl::clip_programs.get(gl) == std::optional{program},
        "a context copy did not hit the identical cached clip program");

  // Uniforms and the drawing-buffer projection.
  gl.use_program(program->program);
  scene2d_gl::upload_clip_uniforms(state, program, identity_matrix());
  check(gl.get_error() == gl.no_error, "uploading the clip uniforms through the real locations raised a GL error");
  const auto mat = scene2d_gl::get_projection_mat3(state);
  const double width = gl.drawing_buffer_width();
  const double height = gl.drawing_buffer_height();
  check(width > 0 && height > 0 && mat.length == 9 && std::abs(mat[0] - 2.0 / width) < 1e-6 &&
            std::abs(mat[4] + 2.0 / height) < 1e-6 && mat[6] == -1.0F && mat[7] == 1.0F,
        "the projection did not use the live drawing-buffer size");

  // One push/pop of a real triangle contour.
  gl.enable(gl.cull_face_constant);
  flight::Array<flight::Array<double>> contours{flight::Array<double>{0.0, 0.0, 10.0, 0.0, 0.0, 10.0}};
  scene2d_gl::push_gl_clip_contours(state, contours, flight::String("nonZero"), identity_matrix());
  const auto runtime = flight::render_gl::get_gl_render_state_runtime(state);
  check(!gl.is_enabled(gl.cull_face_constant), "push did not disable CULL_FACE through cull_face_constant");
  check(gl.is_enabled(gl.stencil_test) && runtime->current_mask_depth == std::optional<double>{1.0},
        "push did not enable the stencil test and record mask depth 1");
  scene2d_gl::pop_gl_clip_contours(state);
  check(!gl.is_enabled(gl.stencil_test) && runtime->current_mask_depth == std::optional<double>{0.0},
        "pop did not release the stencil test and return to mask depth 0");
  check(gl.get_error() == gl.no_error, "the live push/pop raised a GL error");

  gl.use_program(std::nullopt);
  check(scene2d_gl::clip_programs.erase(gl), "could not release the live clip cache entry");
  gl.delete_program(program->program);
  check(gl.get_error() == gl.no_error, "live clip program teardown failed");
}

}  // namespace

int main() {
  try {
    check_live_clip_cache();
    if (failures != 0) {
      std::cerr << failures << " scene2d-gl clip cache check(s) failed\n";
      return 1;
    }
    std::cout << "scene2d-gl clip cache checks passed; live miss/link/locations/hit, uniforms, drawing-buffer "
                 "projection and push/pop verified\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "scene2d-gl live clip cache check failed: " << error.what() << '\n';
    return 1;
  }
}
