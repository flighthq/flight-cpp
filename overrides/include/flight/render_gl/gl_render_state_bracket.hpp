// Native completion of render-gl/src/glRenderStateBracket.ts at the pinned Flight revision.
#pragma once

#include <exception>
#include <functional>
#include <optional>
#include <type_traits>
#include <variant>

#include <flight/render_gl/gl_render_state.hpp>

namespace flight::render_gl {

using flight::types::GlBlendSignature;
using flight::types::GlBoundShader;
using flight::types::GlContext;
using flight::types::GlCubeRenderTarget;
using flight::types::GlRenderState;
using flight::types::GlRenderTarget;
using flight::types::GlScissorRect;
using flight::types::GlTextureRealization;
using flight::types::GlViewportRect;

using GlBooleanQuad = flight::Array<bool>;
using GlBox = flight::Array<double>;

struct SavedGlRenderState : public flight::ReferenceEnabled {
  std::uint32_t active_texture;
  bool blend;
  std::uint32_t blend_dst_alpha;
  std::uint32_t blend_dst_rgb;
  std::uint32_t blend_equation_alpha;
  std::uint32_t blend_equation_rgb;
  std::uint32_t blend_src_alpha;
  std::uint32_t blend_src_rgb;
  GlBox clear_color;
  GlBooleanQuad color_mask;
  bool cull_face;
  std::uint32_t cull_face_mode;
  std::uint32_t front_face;
  std::uint32_t stencil_back_fail;
  std::uint32_t stencil_back_func;
  std::uint32_t stencil_back_pass_depth_fail;
  std::uint32_t stencil_back_pass_depth_pass;
  int stencil_back_ref;
  std::uint32_t stencil_back_value_mask;
  std::uint32_t stencil_back_write_mask;
  std::uint32_t stencil_fail;
  std::uint32_t stencil_func;
  std::uint32_t stencil_pass_depth_fail;
  std::uint32_t stencil_pass_depth_pass;
  int stencil_ref;
  bool stencil_test;
  std::uint32_t stencil_value_mask;
  std::uint32_t stencil_write_mask;
  std::optional<flight::host_sdl::WebGlFramebuffer> current_framebuffer;
  std::optional<flight::Ref<GlBlendSignature>> current_blend_signature;
  std::variant<flight::Ref<GlCubeRenderTarget>, flight::Ref<GlRenderTarget>,
               flight::Null, flight::Undefined> current_render_target;
  std::optional<flight::Ref<GlBoundShader>> current_shader;
  std::variant<flight::Ref<GlScissorRect>, flight::Null, flight::Undefined>
      current_scissor_rect;
  std::optional<flight::Ref<GlTextureRealization>> current_texture_realization;
  std::uint32_t depth_func;
  bool depth_mask;
  bool depth_test;
  std::optional<flight::host_sdl::WebGlFramebuffer> framebuffer;
  std::optional<flight::host_sdl::WebGlProgram> program;
  std::optional<flight::Ref<GlViewportRect>> render_target_viewport;
  GlBox scissor_box;
  bool scissor_test;
  flight::Array<std::optional<flight::host_sdl::WebGlTexture>> texture_2d_by_unit;
  bool unpack_premultiply_alpha;
  std::optional<flight::host_sdl::WebGlVertexArrayObject> vertex_array;
  GlBox viewport;
};

inline flight::WeakMap<flight::Ref<GlRenderState>,
                       flight::Array<flight::Ref<SavedGlRenderState>>>
    render_state_stack;

inline GlBooleanQuad read_gl_boolean_quad(GlContext gl, std::uint32_t parameter) {
  try {
    return static_cast<GlBooleanQuad>(gl.get_parameter(parameter));
  } catch (...) {
    return GlBooleanQuad{true, true, true, true};
  }
}

inline GlBox read_gl_box(GlContext gl, std::uint32_t parameter) {
  try {
    return static_cast<GlBox>(gl.get_parameter(parameter));
  } catch (...) {
    return GlBox{0.0, 0.0, 0.0, 0.0};
  }
}

inline std::uint32_t read_gl_enum(GlContext gl, std::uint32_t parameter) {
  return static_cast<std::uint32_t>(
      static_cast<double>(gl.get_parameter(parameter)));
}

inline int read_gl_int(GlContext gl, std::uint32_t parameter) {
  return static_cast<int>(static_cast<double>(gl.get_parameter(parameter)));
}

inline void restore_gl_capability(
    GlContext gl, std::uint32_t capability, bool enabled) {
  if (enabled) gl.enable(capability);
  else gl.disable(capability);
}

inline constexpr std::size_t gl_render_state_texture_unit_count = 14;

inline void push_gl_render_state(flight::Ref<GlRenderState> state) {
  const auto runtime = get_gl_render_state_runtime(state);
  if (auto* flush = std::get_if<std::function<void(flight::Ref<GlRenderState>)>>(
          &runtime->flush_pending_draws)) {
    (*flush)(state);
  }
  const auto gl = state->gl;
  const auto active_texture = read_gl_enum(gl, gl.active_texture_constant);
  flight::Array<std::optional<flight::host_sdl::WebGlTexture>> textures;
  for (std::size_t index = 0; index < gl_render_state_texture_unit_count;
       ++index) {
    gl.active_texture(gl.texture0 + static_cast<std::uint32_t>(index));
    textures.push(static_cast<std::optional<flight::host_sdl::WebGlTexture>>(
        gl.get_parameter(gl.texture_binding_2_d)));
  }
  gl.active_texture(active_texture);

  auto stack = render_state_stack.get(state);
  if (!stack.has_value()) {
    stack = flight::Array<flight::Ref<SavedGlRenderState>>{};
    render_state_stack.set(state, stack.value());
  }
  stack.value().push(flight::make_ref<SavedGlRenderState>(SavedGlRenderState{
      .active_texture = active_texture,
      .blend = gl.is_enabled(gl.blend),
      .blend_dst_alpha = read_gl_enum(gl, gl.blend_dst_alpha),
      .blend_dst_rgb = read_gl_enum(gl, gl.blend_dst_rgb),
      .blend_equation_alpha = read_gl_enum(gl, gl.blend_equation_alpha),
      .blend_equation_rgb = read_gl_enum(gl, gl.blend_equation_rgb),
      .blend_src_alpha = read_gl_enum(gl, gl.blend_src_alpha),
      .blend_src_rgb = read_gl_enum(gl, gl.blend_src_rgb),
      .clear_color = read_gl_box(gl, gl.color_clear_value),
      .color_mask = read_gl_boolean_quad(gl, gl.color_writemask),
      .cull_face = gl.is_enabled(gl.cull_face_constant),
      .cull_face_mode = read_gl_enum(gl, gl.cull_face_mode),
      .front_face = read_gl_enum(gl, gl.front_face_constant),
      .stencil_back_fail = read_gl_enum(gl, gl.stencil_back_fail),
      .stencil_back_func = read_gl_enum(gl, gl.stencil_back_func),
      .stencil_back_pass_depth_fail = read_gl_enum(gl, gl.stencil_back_pass_depth_fail),
      .stencil_back_pass_depth_pass = read_gl_enum(gl, gl.stencil_back_pass_depth_pass),
      .stencil_back_ref = read_gl_int(gl, gl.stencil_back_ref),
      .stencil_back_value_mask = read_gl_enum(gl, gl.stencil_back_value_mask),
      .stencil_back_write_mask = read_gl_enum(gl, gl.stencil_back_writemask),
      .stencil_fail = read_gl_enum(gl, gl.stencil_fail),
      .stencil_func = read_gl_enum(gl, gl.stencil_func_constant),
      .stencil_pass_depth_fail = read_gl_enum(gl, gl.stencil_pass_depth_fail),
      .stencil_pass_depth_pass = read_gl_enum(gl, gl.stencil_pass_depth_pass),
      .stencil_ref = read_gl_int(gl, gl.stencil_ref),
      .stencil_test = gl.is_enabled(gl.stencil_test),
      .stencil_value_mask = read_gl_enum(gl, gl.stencil_value_mask),
      .stencil_write_mask = read_gl_enum(gl, gl.stencil_writemask),
      .current_framebuffer = runtime->current_framebuffer,
      .current_blend_signature = runtime->context->current_blend_signature,
      .current_render_target = runtime->current_render_target,
      .current_shader = runtime->context->current_shader,
      .current_scissor_rect = runtime->current_scissor_rect,
      .current_texture_realization = runtime->context->current_texture_realization,
      .depth_func = read_gl_enum(gl, gl.depth_func_constant),
      .depth_mask = static_cast<bool>(gl.get_parameter(gl.depth_writemask)),
      .depth_test = gl.is_enabled(gl.depth_test),
      .framebuffer = static_cast<std::optional<flight::host_sdl::WebGlFramebuffer>>(
          gl.get_parameter(gl.framebuffer_binding)),
      .program = static_cast<std::optional<flight::host_sdl::WebGlProgram>>(
          gl.get_parameter(gl.current_program)),
      .render_target_viewport = runtime->render_target_viewport,
      .scissor_box = read_gl_box(gl, gl.scissor_box),
      .scissor_test = gl.is_enabled(gl.scissor_test),
      .texture_2d_by_unit = textures,
      .unpack_premultiply_alpha = static_cast<bool>(
          gl.get_parameter(gl.unpack_premultiply_alpha_webgl)),
      .vertex_array = static_cast<std::optional<flight::host_sdl::WebGlVertexArrayObject>>(
          gl.get_parameter(gl.vertex_array_binding)),
      .viewport = read_gl_box(gl, gl.viewport_constant)}));
}

inline void pop_gl_render_state(flight::Ref<GlRenderState> state) {
  auto stack = render_state_stack.get(state);
  if (!stack.has_value() || stack.value().empty()) return;
  auto saved = stack.value().pop().value();
  if (stack.value().empty()) render_state_stack.erase(state);
  const auto gl = state->gl;
  restore_gl_capability(gl, gl.depth_test, saved->depth_test);
  gl.depth_mask(saved->depth_mask);
  gl.depth_func(saved->depth_func);
  restore_gl_capability(gl, gl.cull_face_constant, saved->cull_face);
  gl.cull_face(saved->cull_face_mode);
  gl.front_face(saved->front_face);
  restore_gl_capability(gl, gl.stencil_test, saved->stencil_test);
  gl.stencil_mask_separate(gl.front, saved->stencil_write_mask);
  gl.stencil_mask_separate(gl.back, saved->stencil_back_write_mask);
  gl.stencil_func_separate(gl.front, saved->stencil_func,
                           saved->stencil_ref, saved->stencil_value_mask);
  gl.stencil_func_separate(gl.back, saved->stencil_back_func,
                           saved->stencil_back_ref,
                           saved->stencil_back_value_mask);
  gl.stencil_op_separate(gl.front, saved->stencil_fail,
                         saved->stencil_pass_depth_fail,
                         saved->stencil_pass_depth_pass);
  gl.stencil_op_separate(gl.back, saved->stencil_back_fail,
                         saved->stencil_back_pass_depth_fail,
                         saved->stencil_back_pass_depth_pass);
  restore_gl_capability(gl, gl.blend, saved->blend);
  gl.blend_func_separate(saved->blend_src_rgb, saved->blend_dst_rgb,
                         saved->blend_src_alpha, saved->blend_dst_alpha);
  gl.blend_equation_separate(saved->blend_equation_rgb,
                             saved->blend_equation_alpha);
  gl.bind_vertex_array(saved->vertex_array);
  gl.use_program(saved->program);
  gl.bind_framebuffer(gl.framebuffer, saved->framebuffer);
  for (std::size_t index = 0; index < saved->texture_2d_by_unit.size(); ++index) {
    gl.active_texture(gl.texture0 + static_cast<std::uint32_t>(index));
    gl.bind_texture(gl.texture_2_d,
                    saved->texture_2d_by_unit.element(index));
  }
  gl.active_texture(saved->active_texture);
  gl.color_mask(saved->color_mask.element(0), saved->color_mask.element(1),
                saved->color_mask.element(2), saved->color_mask.element(3));
  gl.clear_color(saved->clear_color.element(0), saved->clear_color.element(1),
                 saved->clear_color.element(2), saved->clear_color.element(3));
  gl.pixel_storei(gl.unpack_premultiply_alpha_webgl,
                  saved->unpack_premultiply_alpha);
  restore_gl_capability(gl, gl.scissor_test, saved->scissor_test);
  gl.scissor(static_cast<int>(saved->scissor_box.element(0)),
             static_cast<int>(saved->scissor_box.element(1)),
             static_cast<int>(saved->scissor_box.element(2)),
             static_cast<int>(saved->scissor_box.element(3)));
  gl.viewport(static_cast<int>(saved->viewport.element(0)),
              static_cast<int>(saved->viewport.element(1)),
              static_cast<int>(saved->viewport.element(2)),
              static_cast<int>(saved->viewport.element(3)));
  const auto runtime = get_gl_render_state_runtime(state);
  runtime->current_framebuffer = saved->current_framebuffer;
  runtime->context->current_shader = saved->current_shader;
  runtime->current_render_target = saved->current_render_target;
  runtime->context->current_texture_realization =
      saved->current_texture_realization;
  runtime->context->current_blend_signature = saved->current_blend_signature;
  runtime->current_scissor_rect = saved->current_scissor_rect;
  runtime->render_target_viewport = saved->render_target_viewport;
}

template <typename T>
inline T with_gl_render_state(
    flight::Ref<GlRenderState> state, std::function<T()> callback) {
  push_gl_render_state(state);
  if constexpr (std::is_void_v<T>) {
    try {
      callback();
    } catch (...) {
      pop_gl_render_state(state);
      throw;
    }
    pop_gl_render_state(state);
  } else {
    try {
      auto result = callback();
      pop_gl_render_state(state);
      return result;
    } catch (...) {
      pop_gl_render_state(state);
      throw;
    }
  }
}

}  // namespace flight::render_gl
