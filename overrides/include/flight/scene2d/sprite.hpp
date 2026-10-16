// Default native Sprite owner lane from sprite.ts. Custom factory/runtime lanes remain unsupported.
#pragma once
#include <flight/node/native_node2d_access.hpp>
#include <flight/scene2d/display_object.hpp>
#include <flight/texture/texture.hpp>
#include <flight/types/sprite_identity_renderer_data.hpp>
#include <flight/render/gl_scene2d_bindings.hpp>
namespace flight::scene2d {
inline flight::Ref<flight::types::Sprite> create_sprite() {
  auto result = flight::make_ref<flight::types::Sprite>();
  result->data = flight::make_ref<flight::types::SpriteData>();
  result->enabled = true; result->kind = flight::types::sprite_kind;
  result->entity_runtime_key = create_node_2d_runtime(); result->alpha = 1; result->visible = true;
  result->pivot_x = result->pivot_y = result->rotation = result->skew_x = result->skew_y = result->x = result->y = 0;
  result->scale_x = result->scale_y = 1;
  return result;
}
inline auto native_sprite(flight::types::Node2D node) {
  auto source = node.shared_exact_object<flight::types::Sprite>();
  if (!source) throw std::logic_error("Sprite source owner mismatch");
  return source;
}
inline std::optional<flight::ErasedRef> create_sprite_renderer_data(flight::Ref<flight::types::GlRenderState>, flight::types::Node2D node) {
  auto source = native_sprite(node);
  auto data = flight::make_ref<flight::types::SpriteIdentityRendererData>();
  data->texture_identity = source->data->texture;
  data->texture_version = source->data->texture ? std::visit([](auto value) { return value->version; }, *source->data->texture) : -1;
  return flight::ErasedRef(data);
}
inline bool is_sprite_renderer_dirty(flight::Ref<flight::types::GlRenderState>, flight::types::Node2D node, std::optional<flight::ErasedRef> entry) {
  if (!entry) return false;
  auto data = entry->as<flight::types::SpriteIdentityRendererData>();
  if (!data) throw std::logic_error("Sprite renderer data owner mismatch");
  auto texture = native_sprite(node)->data->texture;
  auto version = texture ? std::visit([](auto value) { return value->version; }, *texture) : -1;
  bool dirty = data->texture_identity != texture || data->texture_version != version;
  data->texture_identity = texture; data->texture_version = version;
  return dirty;
}
} // namespace flight::scene2d
