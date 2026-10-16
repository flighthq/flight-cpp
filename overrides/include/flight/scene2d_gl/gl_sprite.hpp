// Source-derived textured Sprite renderer from glSprite.ts; actual GL batch writer.
#pragma once
#include <flight/scene2d/sprite.hpp>
#include <flight/render_gl/gl_texture_resolver.hpp>
#include <flight/scene2d_gl/gl_quad_batch_writer.hpp>
#include <flight/render_gl/gl_quad_material_registry.hpp>
#include <flight/geometry/matrix3.hpp>
#include <flight/geometry/vector2.hpp>
#include <flight/render/scene2d_working_color_space.hpp>
namespace flight::scene2d_gl {
inline auto sprite_uv_matrix = flight::geometry::create_matrix3();
inline auto sprite_view_size = flight::geometry::create_vector2();
inline void draw_gl_sprite(flight::Ref<flight::types::GlRenderState> state, flight::Ref<flight::types::RenderProxy2D> proxy) {
  auto runtime = flight::render_gl::get_gl_render_state_runtime(state);
  auto source = flight::scene2d::native_sprite(proxy->source);
  if (!source->data->texture) return;
  auto entry = std::get_if<flight::Ref<flight::types::Texture2D>>(&*source->data->texture);
  if (!entry || !flight::texture::has_texture_source(*entry)) return;
  auto texture = *entry;
  flight::texture::get_texture_view_size(sprite_view_size, texture);
  const double width = sprite_view_size->x, height = sprite_view_size->y;
  if (width <= 0 || height <= 0) return;
  using Material = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::Material>>>>;
  std::optional<Material> material;
  if (proxy->material) material = Material(flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<flight::types::Material2D>>>>(*proxy->material));
  auto renderer = flight::render_gl::resolve_gl_quad_material_renderer(state, material);
  if (!renderer) return;
  auto gl_texture = flight::render_gl::resolve_gl_texture(state, flight::types::GlTextureValue{flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::Texture2D>>>>(texture)}, true, flight::render::scene_2d_working_color_space);
  if (!gl_texture) return;
  const bool straight = runtime->context->current_texture_realization.value()->straight_alpha;
  ensure_gl_quad_batch_shader(state);
  flight::texture::get_texture_uv_matrix(sprite_uv_matrix, texture);
  auto uv = sprite_uv_matrix->m;
  double oy = uv[7], uy = uv[1], vy = uv[4];
  if (flight::texture::get_texture_source_kind(texture) == flight::types::render_target_texture_source_kind) { oy = 1 - oy; uy = -uy; vy = -vy; }
  const double index = prepare_gl_quad_batch_write(state, *gl_texture, straight, texture->sampler, proxy->blend_mode, material, *renderer, 1);
  write_gl_quad_batch_affine_instance(runtime->quad_batch_writer_instance_data, index * quad_batch_instance_floats, proxy->transform_2d, width, height, uv[6], oy, uv[0], uy, uv[3], vy, proxy->alpha);
  pack_gl_quad_batch_material_instance(state, proxy->material_data, index);
  if (std::holds_alternative<flight::Array<double>>(proxy->color_matrix)) record_gl_quad_batch_color_scale_bias(state, std::get<flight::Array<double>>(proxy->color_matrix), index);
  else if (proxy->color_scale_bias) record_gl_quad_batch_color_scale_bias(state, *proxy->color_scale_bias, index);
  else record_gl_quad_batch_color_scale_bias(state, flight::null, index);
  ++runtime->quad_batch_writer_count;
}
inline flight::Ref<flight::types::NodeRenderer> gl_sprite_renderer = [] {
  auto descriptor = flight::make_ref<flight::types::NodeRenderer>(); descriptor->format = flight::types::BatchFormat::Quad;
  auto binding = std::make_shared<flight::types::NativeGlScene2DRendererBinding>();
  binding->create_data = flight::scene2d::create_sprite_renderer_data; binding->is_dirty = flight::scene2d::is_sprite_renderer_dirty; binding->submit = draw_gl_sprite;
  descriptor->native_gl_scene_2d = binding;
  return descriptor;
}();
} // namespace flight::scene2d_gl
