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

namespace flight::render {
using AdaptHook = std::function<void(flight::Ref<flight::types::RenderState>, flight::types::NodeAny, flight::Ref<flight::types::RenderProxy2D>)>;
void update_render_proxy_2d(flight::Ref<flight::types::RenderState>, flight::types::NodeAny, flight::Ref<flight::types::RenderProxy2D>, std::optional<flight::Ref<flight::types::RenderProxy2D>>);
bool walk_node(flight::Ref<flight::types::RenderState>, flight::types::Node2D, flight::types::RenderProxyVisitor);
void walk_render_subtree(flight::Ref<flight::types::RenderState>, flight::types::NodeAny, std::function<void(flight::Ref<flight::types::RenderState>, flight::types::NodeAny)>);
flight::Ref<flight::types::RenderProxy2D> get_or_create_render_proxy_2d(flight::Ref<flight::types::RenderState>, flight::types::Node2D);
inline void dispose_render_proxy(flight::Ref<flight::types::RenderState> state, flight::types::NodeAny source) {
  auto runtime = flight::render::get_render_state_runtime(state);
  flight::WeakMap<flight::types::NodeAny, flight::Ref<flight::types::RenderProxy>> render_proxy_map = runtime->render_proxy_map;
  auto node = render_proxy_map.get(source);
  if (!node.has_value()) {
    return;
  }
  if (node.value()->renderer_data.has_value()) {
    ([&]() { auto optional_chain_receiver = ([&]() -> std::optional<std::function<void(flight::Ref<flight::types::RenderState>, flight::Ref<flight::types::RendererData>)>> { auto optional_chain_receiver = node.value()->renderer; if (!optional_chain_receiver.has_value()) return std::nullopt; return optional_chain_receiver.value()->destroy_data; }()); if (!optional_chain_receiver.has_value()) return; optional_chain_receiver.value()(state, node.value()->renderer_data.value()); }());
  }
  render_proxy_map.erase(source);
  runtime->render_proxy_sources.erase(source);
}

inline void install_render_adapt_hook(flight::Ref<flight::types::RenderState> state, AdaptHook fn) {
  (flight::render::get_render_state_runtime(state)->render_adapt_hook = std::optional<std::function<void(flight::Ref<flight::types::RenderState>, flight::types::NodeAny, flight::Ref<flight::types::RenderProxy2D>)>>{fn});
}

inline std::optional<flight::Ref<flight::types::NodeRenderer>> resolve_render_proxy_renderer(flight::Ref<flight::types::RenderState> state, flight::String kind) {
  auto runtime = flight::render::get_render_state_runtime(state);
  auto entry = runtime->registries->node_renderers.get(kind);
  if (!entry.has_value()) {
    ([&]() { auto optional_chain_receiver = runtime->registry_miss; if (!optional_chain_receiver.has_value()) return; (*optional_chain_receiver.value())(flight::types::RenderRegistryTable::NodeRenderer, kind); }());
    return std::nullopt;
  }
  return std::optional<flight::Ref<flight::types::NodeRenderer>>{entry.value()};
}

inline void initialize_render_proxy(flight::types::EntityConstruction<flight::Ref<flight::types::RenderProxy>> out, flight::Ref<flight::types::RenderState> state, flight::types::NodeAny source) {
  auto runtime = flight::render::get_render_state_runtime(state);
  auto renderer = resolve_render_proxy_renderer(state, source->kind);
  flight::row_set<flight::RowKey<"source">>(out, source);
  flight::row_set<flight::RowKey<"kind">>(out, source->kind);
  flight::row_set<flight::RowKey<"next">>(out, std::nullopt);
  flight::row_set<flight::RowKey<"alpha">>(out, 1.0);
  flight::row_set<flight::RowKey<"appearanceFrameId">>(out, -1.0);
  flight::row_set<flight::RowKey<"blendMode">>(out, std::optional<flight::String>{flight::types::blend_mode->normal});
  flight::row_set<flight::RowKey<"colorScaleBias">>(out, std::nullopt);
  flight::row_set<flight::RowKey<"colorMatrix">>(out, std::variant<flight::Array<double>, flight::Null, flight::Undefined>{std::in_place_type<flight::Null>, flight::null});
  flight::row_set<flight::RowKey<"material">>(out, std::nullopt);
  flight::row_set<flight::RowKey<"materialData">>(out, std::nullopt);
  flight::row_set<flight::RowKey<"lastAppearanceId">>(out, -1.0);
  flight::row_set<flight::RowKey<"lastChildrenId">>(out, -1.0);
  flight::row_set<flight::RowKey<"lastLocalContentId">>(out, -1.0);
  flight::row_set<flight::RowKey<"lastLocalTransformId">>(out, -1.0);
  flight::row_set<flight::RowKey<"lastParentReferenceId">>(out, -1.0);
  flight::row_set<flight::RowKey<"name">>(out, std::nullopt);
  flight::row_set<flight::RowKey<"renderer">>(out, renderer);
  flight::row_set<flight::RowKey<"rendererData">>(out, ([&]() -> std::optional<flight::Ref<flight::types::RendererData>> { auto nullish_coalesce_left = ([&]() -> std::optional<flight::Ref<flight::types::RendererData>> { auto optional_chain_receiver = renderer; if (!optional_chain_receiver.has_value()) return std::nullopt; return optional_chain_receiver.value()->create_data(state, source); }()); if (nullish_coalesce_left.has_value()) return nullish_coalesce_left; return std::nullopt; }()));
  flight::row_set<flight::RowKey<"rendererDataSource">>(out, std::optional<std::shared_ptr<flight::types::Node<flight::Any>>>{source});
  flight::row_set<flight::RowKey<"rendererMapId">>(out, runtime->renderer_map_id);
  flight::row_set<flight::RowKey<"transformFrameId">>(out, -1.0);
  flight::row_set<flight::RowKey<"visible">>(out, true);
}

inline flight::Ref<flight::types::RenderProxy> create_render_proxy(flight::Ref<flight::types::RenderState> state, flight::types::NodeAny source) {
  flight::types::EntityConstruction<flight::Ref<flight::types::RenderProxy>> out = flight::entity::allocate_entity<flight::Ref<flight::types::RenderProxy>>();
  initialize_render_proxy(out, state, source);
  return flight::entity::finish_entity<flight::Ref<flight::types::RenderProxy>>(out);
}

inline void update_render_proxy_renderer(flight::Ref<flight::types::RenderState> state, flight::Ref<flight::types::RenderProxy> node) {
  auto runtime = flight::render::get_render_state_runtime(state);
  auto renderer = resolve_render_proxy_renderer(state, node->kind);
  if (((node->renderer != renderer) || (node->renderer_data_source != node->source))) {
    if (node->renderer_data.has_value()) {
      ([&]() { auto optional_chain_receiver = ([&]() -> std::optional<std::function<void(flight::Ref<flight::types::RenderState>, flight::Ref<flight::types::RendererData>)>> { auto optional_chain_receiver = node->renderer; if (!optional_chain_receiver.has_value()) return std::nullopt; return optional_chain_receiver.value()->destroy_data; }()); if (!optional_chain_receiver.has_value()) return; optional_chain_receiver.value()(state, node->renderer_data.value()); }());
    }
    (node->renderer = renderer);
    (node->renderer_data = ([&]() -> std::optional<flight::Ref<flight::types::RendererData>> { auto nullish_coalesce_left_2 = ([&]() -> std::optional<flight::Ref<flight::types::RendererData>> { auto optional_chain_receiver = renderer; if (!optional_chain_receiver.has_value()) return std::nullopt; return optional_chain_receiver.value()->create_data(state, node->source); }()); if (nullish_coalesce_left_2.has_value()) return nullish_coalesce_left_2; return std::nullopt; }()));
    (node->renderer_data_source = std::optional<std::shared_ptr<flight::types::Node<flight::Any>>>{node->source});
  }
  (node->renderer_map_id = runtime->renderer_map_id);
}

inline bool prepare_scene_2d_render(flight::Ref<flight::types::RenderState> state, flight::types::Node2D source) {
  return walk_node(state, source, update_render_proxy_2d);
}

inline void dispose_scene_2d_render(flight::Ref<flight::types::RenderState> state, flight::types::NodeAny root) {
  walk_render_subtree(state, root, dispose_render_proxy);
}
} // namespace flight::render

namespace flight::render {
inline void dispose_render_proxy(NativeGlState state, NativeNode2D source) {
  auto runtime = flight::render_gl::get_gl_render_state_runtime(state);
  auto data = runtime->render_proxy_map.get(source);
  if (!data) return;
  if ((*data)->renderer_data) if (auto binding = gl_renderer_binding((*data)->renderer); binding && binding->destroy_data) binding->destroy_data(state, *(*data)->renderer_data);
  static_cast<void>(runtime->render_proxy_map.erase(source));
  static_cast<void>(runtime->render_proxy_sources.erase(source));
}
inline void dispose_scene_2d_render(NativeGlState state, NativeNode2D root) {
  auto runtime = flight::render_gl::get_gl_render_state_runtime(state);
  auto stack = runtime->temp_stack;
  auto put = [&](std::size_t index, NativeNode2D node) { if (index >= stack.size()) stack.resize(index + 1); stack[index] = node; };
  std::size_t length = 1; put(0, root);
  while (length) {
    auto node = stack[--length];
    dispose_render_proxy(state, node);
    auto children = flight::node::native_node_2d_runtime(node)->children;
    if (children) for (std::size_t i=children->size(); i>0; --i) put(length++, (*children)[i-1]);
  }
}
} // namespace flight::render
