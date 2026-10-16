// Native owner-preserving lowering of glNode2D.ts at the pinned Flight revision.
#pragma once
#include <flight/render/render_proxy.hpp>
#include <flight/scene2d_gl/gl_quad_batch_writer.hpp>

namespace flight::scene2d_gl {
inline void draw_gl_scene_2d(flight::Ref<flight::types::GlRenderState>, flight::Ref<flight::types::RenderProxy2D>) {}

inline void render_gl_scene_2d(flight::Ref<flight::types::GlRenderPass> pass, flight::types::Node2D source,
    std::optional<flight::Ref<flight::types::Matrix>> render_transform = std::nullopt) {
  auto state = pass->state;
  auto gl = state->gl;
  gl.disable(gl.cull_face_constant); gl.disable(gl.depth_test); gl.enable(gl.blend);
  auto runtime = flight::render_gl::get_gl_render_state_runtime(state);
  if (state->display_object_clip_hooks) throw std::logic_error("Native GL clip-hook owner binding is unavailable");
  auto stack = runtime->temp_stack;
  auto put = [&](std::size_t index, flight::types::Node2D node) { if (index >= stack.size()) stack.resize(index + 1); stack[index] = node; };
  auto scratch = render_transform ? flight::geometry::create_matrix() : nullptr;
  std::size_t length = 1; put(0, source);
  while (length) {
    auto current = stack[--length];
    if (!flight::row_get<flight::RowKey<"enabled">>(current)) continue;
    auto entry = flight::render::get_render_proxy_2d(state, current);
    if (!entry) continue;
    auto data = *entry;
    auto saved = data->transform_2d;
    if (render_transform) {
      flight::geometry::multiply_matrix(scratch, *render_transform, saved);
      data->transform_2d = scratch;
    }
    if (!flight::render::is_render_proxy_visible(data)) {
      if (render_transform) data->transform_2d = saved;
      continue;
    }
    if (auto binding = flight::render::gl_renderer_binding(data->renderer); binding && binding->submit) binding->submit(state, data);
    if (data->traverse_children) {
      auto children = flight::node::native_node_2d_runtime(current)->children;
      if (children) for (std::size_t i = children->size(); i > 0; --i) put(length++, (*children)[i - 1]);
    }
    if (render_transform) data->transform_2d = saved;
  }
  flush_gl_quad_batch_writer(state);
}

inline flight::Ref<flight::types::NodeRenderer> gl_scene_2d_renderer = [] {
  auto descriptor = flight::make_ref<flight::types::NodeRenderer>();
  descriptor->native_gl_scene_2d = std::make_shared<flight::types::NativeGlScene2DRendererBinding>();
  descriptor->native_gl_scene_2d->submit = draw_gl_scene_2d;
  return descriptor;
}();
} // namespace flight::scene2d_gl
