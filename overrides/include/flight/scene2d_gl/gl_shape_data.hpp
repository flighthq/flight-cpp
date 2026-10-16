// Derived mesh-data owner lane of glShapeData.ts. Raster replay remains unavailable.
#pragma once
#include <flight/render/gl_scene2d_bindings.hpp>
#include <flight/render_gl/gl_render_state.hpp>
#include <flight/types/gl_shape_renderer_data.hpp>
#include <stdexcept>
namespace flight::scene2d_gl {
inline flight::Ref<flight::types::GlShapeRendererData> get_gl_shape_data(flight::ErasedRef data) {
  auto result = data.as<flight::types::GlShapeRendererData>();
  if (!result) throw std::logic_error("GL shape renderer data owner mismatch");
  return result;
}
inline std::optional<flight::ErasedRef> create_gl_shape_data(flight::Ref<flight::types::GlRenderState>, flight::types::Node2D) {
  auto data = flight::make_ref<flight::types::GlShapeRendererData>();
  data->last_content_id = -1; data->last_h = 0; data->last_pixel_ratio = 0; data->last_w = 0; data->mesh_version = -1;
  return flight::ErasedRef(data);
}
inline void destroy_gl_shape_data(flight::Ref<flight::types::GlRenderState>, flight::ErasedRef data) {
  auto shape = get_gl_shape_data(data);
  if (shape->image || shape->surface) throw std::logic_error("Native GL raster-shape resource teardown is unavailable");
  // Mesh buffers belong to the context, not to each shape.
}
} // namespace flight::scene2d_gl
