// Native GL owner lane derived from renderProxy.ts at the pinned Flight revision.
// PARTIAL: generic RenderState disposal/adaptation remains the emitted unsupported lane.
#pragma once
#include <flight/render/gl_scene2d_bindings.hpp>
#include <flight/render_gl/gl_render_state.hpp>
#include <flight/geometry/matrix.hpp>
#include <flight/node/node_transform2d.hpp>
#include <flight/scene2d/display_object.hpp>
#include <flight/types/render_proxy_2d.hpp>
#include <stdexcept>

namespace flight::render {
using NativeGlState = flight::Ref<flight::types::GlRenderState>;
using NativeProxy2D = flight::Ref<flight::types::RenderProxy2D>;
using NativeNode2D = flight::types::Node2D;

inline bool is_render_proxy_visible(NativeProxy2D data) {
  return data->visible && data->alpha > 0 && !(data->transform_2d->a == 0 && data->transform_2d->d == 0);
}
inline std::optional<NativeProxy2D> get_render_proxy_2d(NativeGlState state, NativeNode2D source) {
  return flight::render_gl::get_gl_render_state_runtime(state)->render_proxy_map.get(source);
}
inline std::shared_ptr<flight::types::NativeGlScene2DRendererBinding> gl_renderer_binding(const std::optional<flight::Ref<flight::types::NodeRenderer>>& renderer) {
  if (!renderer) return {};
  auto binding = (*renderer)->native_gl_scene_2d;
  if (!binding) throw std::logic_error("Scene2D GL renderer has no native owner binding");
  return binding;
}
inline std::optional<flight::Ref<flight::types::NodeRenderer>> resolve_render_proxy_renderer(NativeGlState state, flight::String kind) {
  auto runtime = flight::render_gl::get_gl_render_state_runtime(state);
  auto entry = runtime->registries->node_renderers.get(kind);
  if (!entry && runtime->registry_miss && *runtime->registry_miss) {
    // Existing native registry sink uses the declared enum owner.
    runtime->registry_miss.value()->callable(flight::types::RenderRegistryTable::NodeRenderer, kind);
  }
  return entry;
}
inline NativeProxy2D create_render_proxy_2d(NativeGlState state, NativeNode2D source) {
  auto runtime = flight::render_gl::get_gl_render_state_runtime(state);
  auto out = flight::make_ref<flight::types::RenderProxy2D>();
  out->source = source; out->kind = flight::row_get<flight::RowKey<"kind">>(source);
  out->alpha = 1; out->appearance_frame_id = -1; out->blend_mode = flight::String("normal");
  out->last_appearance_id = out->last_children_id = out->last_local_content_id = -1;
  out->last_local_transform_id = out->last_parent_reference_id = -1;
  out->renderer = resolve_render_proxy_renderer(state, out->kind);
  if (auto binding = gl_renderer_binding(out->renderer); binding && binding->create_data) out->renderer_data = binding->create_data(state, source);
  out->renderer_data_source = source; out->renderer_map_id = runtime->renderer_map_id;
  out->transform_frame_id = -1; out->visible = true;
  out->transform_2d = flight::geometry::create_matrix(); out->traverse_children = true; out->clip_depth = 0;
  return out;
}
inline NativeProxy2D get_or_create_render_proxy_2d(NativeGlState state, NativeNode2D source) {
  auto runtime = flight::render_gl::get_gl_render_state_runtime(state);
  auto entry = runtime->render_proxy_map.get(source);
  auto data = entry ? *entry : create_render_proxy_2d(state, source);
  if (!entry) { runtime->render_proxy_map.set(source, data); runtime->render_proxy_sources.add(source); }
  if (data->renderer_map_id != runtime->renderer_map_id) {
    auto renderer = resolve_render_proxy_renderer(state, data->kind);
    if (data->renderer != renderer || data->renderer_data_source != std::optional<NativeNode2D>{data->source}) {
      if (data->renderer_data) if (auto old = gl_renderer_binding(data->renderer); old && old->destroy_data) old->destroy_data(state, *data->renderer_data);
      data->renderer = renderer;
      auto next = gl_renderer_binding(renderer);
      data->renderer_data = next && next->create_data ? next->create_data(state, data->source) : std::nullopt;
      data->renderer_data_source = data->source;
    }
    data->renderer_map_id = runtime->renderer_map_id;
  }
  return data;
}
inline bool prepare_scene_2d_render(NativeGlState state, NativeNode2D root) {
  auto runtime = flight::render_gl::get_gl_render_state_runtime(state);
  if (std::holds_alternative<std::function<void(flight::Ref<flight::types::RenderState>, flight::types::NodeAny)>>(runtime->registries->render_root_guard)) throw std::logic_error("Native Scene2D GL root guard binding is unavailable");
  ++runtime->current_frame_id;
  auto stack = runtime->temp_stack;
  auto put = [&](std::size_t index, NativeNode2D node) { if (index >= stack.size()) stack.resize(index + 1); stack[index] = node; };
  std::size_t length = 1; put(0, root);
  std::optional<NativeProxy2D> parent_data;
  std::optional<NativeNode2D> last_parent;
  bool dirty = false;
  while (length) {
    auto current = stack[--length];
    if (!flight::row_get<flight::RowKey<"enabled">>(current)) continue;
    auto nr = flight::node::native_node_2d_runtime(current);
    if (current != root) {
      if (!nr->parent) { parent_data.reset(); last_parent.reset(); }
      else if (nr->parent != last_parent) { parent_data = get_or_create_render_proxy_2d(state, *nr->parent); last_parent = nr->parent; }
    }
    auto data = get_or_create_render_proxy_2d(state, current);
    bool parent_dirty = parent_data && ((*parent_data)->transform_frame_id == runtime->current_frame_id || (*parent_data)->appearance_frame_id == runtime->current_frame_id);
    auto binding = gl_renderer_binding(data->renderer);
    bool renderer_dirty = binding && binding->is_dirty && binding->is_dirty(state, current, data->renderer_data);
    bool local_dirty = state->scene_graph_sync_policy == flight::String("refreshDerivedState") || data->last_local_transform_id != nr->local_transform_id || data->last_appearance_id != nr->appearance_id || data->last_local_content_id != nr->local_content_id;
    if (parent_dirty || renderer_dirty || local_dirty || data->last_children_id != nr->children_id || data->last_parent_reference_id != nr->parent_reference_id) {
      if (data->last_parent_reference_id != nr->parent_reference_id) { data->last_appearance_id = -1; data->last_local_transform_id = -1; }
      if ((parent_data && (*parent_data)->appearance_frame_id == runtime->current_frame_id) || data->last_appearance_id != nr->appearance_id) {
        data->visible = flight::row_get<flight::RowKey<"visible">>(current) && (!parent_data || (*parent_data)->visible);
        if (data->visible) {
          data->alpha = flight::row_get<flight::RowKey<"alpha">>(current) * (parent_data ? (*parent_data)->alpha : state->render_alpha);
          if (data->alpha > 0) { data->blend_mode = !parent_data && state->render_blend_mode ? state->render_blend_mode : flight::row_get<flight::RowKey<"blendMode">>(current); data->appearance_frame_id = runtime->current_frame_id; }
        }
        data->last_appearance_id = nr->appearance_id;
      }
      if ((parent_data && (*parent_data)->transform_frame_id == runtime->current_frame_id) || state->scene_graph_sync_policy == flight::String("refreshDerivedState") || data->last_local_transform_id != nr->local_transform_id) {
        auto local = flight::node::get_node_local_matrix(current);
        if (parent_data) flight::geometry::multiply_matrix(data->transform_2d, (*parent_data)->transform_2d, local);
        else flight::geometry::copy_matrix(data->transform_2d, local);
        data->transform_frame_id = runtime->current_frame_id; data->last_local_transform_id = nr->local_transform_id;
      }
      data->material = flight::row_get<flight::RowKey<"material">>(current);
      data->material_data = flight::row_get<flight::RowKey<"materialData">>(current);
      if (!std::holds_alternative<flight::Null>(runtime->registries->color_adjustments) && !std::holds_alternative<flight::Undefined>(runtime->registries->color_adjustments)) throw std::logic_error("Native Scene2D GL color adjustment callback binding is unavailable");
      data->clip_depth = (parent_data ? (*parent_data)->clip_depth : 0) + (flight::row_get<flight::RowKey<"clip">>(current) ? 1 : 0);
      data->last_children_id = nr->children_id; data->last_local_content_id = nr->local_content_id; data->last_parent_reference_id = nr->parent_reference_id;
      if (runtime->render_adapt_hook) throw std::logic_error("Native Scene2D GL adaptation hook binding is unavailable");
      dirty = true;
    }
    if (!is_render_proxy_visible(data)) continue;
    if (data->traverse_children && nr->children) for (std::size_t i = nr->children->size(); i > 0; --i) put(length++, (*nr->children)[i-1]);
  }
  return dirty;
}
} // namespace flight::render
