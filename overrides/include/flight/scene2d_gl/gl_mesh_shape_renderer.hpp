// Derived from glMeshShapeRenderer.ts at the pinned Flight revision.
#pragma once
#include <flight/node/native_node2d_access.hpp>
#include <flight/shape/shape_fill.hpp>
#include <flight/shape/shape_stroke.hpp>
#include <flight/shape/shape_stroke_outline.hpp>
#include <flight/path/tessellate_path.hpp>
#include <flight/scene2d_gl/gl_shape_data.hpp>
#include <flight/scene2d_gl/gl_shape_mesh.hpp>
namespace flight::scene2d_gl {
using NativeShapeRegion = std::variant<flight::Ref<flight::types::ShapeFillRegion>, flight::Ref<flight::types::ShapeStrokeRegion>>;
inline std::optional<flight::Array<NativeShapeRegion>> resolve_gl_shape_mesh_regions(flight::Array<flight::types::ShapeCommandToken> commands, bool strokes) {
  auto fill = flight::shape::get_shape_fill_regions(commands);
  if (!fill) return std::nullopt;
  flight::Array<NativeShapeRegion> regions;
  for (auto item : *fill) regions.push(NativeShapeRegion{item});
  if (strokes) {
    auto stroke = flight::shape::get_shape_stroke_regions(commands);
    if (!stroke) return std::nullopt;
    for (auto item : *stroke) regions.push(NativeShapeRegion{item});
  } else {
    auto outline = flight::shape::get_shape_stroke_outline_regions(commands);
    if (!outline) return std::nullopt;
    for (auto item : *outline) regions.push(NativeShapeRegion{item});
  }
  return regions.empty() ? std::nullopt : std::optional{regions};
}
inline bool draw_gl_mesh_shape(flight::Ref<flight::types::GlRenderState> state, flight::Ref<flight::types::RenderProxy2D> proxy) {
  auto source = proxy->source.shared_exact_object<flight::types::Shape>();
  if (!source) throw std::logic_error("GL shape source owner mismatch");
  if (source->data->commands.empty() || !proxy->renderer_data) return false;
  auto runtime = flight::render_gl::get_gl_render_state_runtime(state);
  auto stroke = runtime->registries->stroke_tessellator;
  auto regions = resolve_gl_shape_mesh_regions(source->data->commands, stroke.has_value());
  if (!regions || regions->empty()) return false;
  auto version = flight::node::native_node_2d_runtime(proxy->source)->local_content_id;
  auto data = get_gl_shape_data(*proxy->renderer_data);
  if (data->mesh_version != version) {
    flight::Array<flight::Ref<flight::types::GlShapeMesh>> meshes;
    bool supported = true;
    for (auto region : *regions) {
      auto mesh = std::visit([&](auto value) -> std::optional<flight::Ref<flight::types::PathMesh>> {
        using Region = typename decltype(value)::element_type;
        if constexpr (std::is_same_v<Region, flight::types::ShapeStrokeRegion>) {
          if (stroke) return (*stroke)(value->path, value->style, std::nullopt);
        }
        return flight::path::tessellate_path(value->path);
      }, region);
      if (!mesh) { supported = false; break; }
      std::visit([&](auto value) {
        meshes.push(flight::make_ref<flight::types::GlShapeMesh>(flight::types::GlShapeMesh{
          .vertices = flight::Float32Array((*mesh)->vertices), .indices = flight::Uint16Array((*mesh)->indices), .color = value->color, .alpha = value->alpha}));
      }, region);
    }
    data->meshes = supported ? std::optional{meshes} : std::nullopt;
    data->mesh_version = version;
  }
  if (!data->meshes) return false;
  draw_gl_shape_meshes(state, proxy, *data->meshes);
  return true;
}
inline flight::Ref<flight::types::NodeRenderer> gl_mesh_shape_renderer = [] {
  auto descriptor = flight::make_ref<flight::types::NodeRenderer>();
  descriptor->format = flight::types::BatchFormat::Quad;
  auto binding = std::make_shared<flight::types::NativeGlScene2DRendererBinding>();
  binding->create_data = create_gl_shape_data; binding->destroy_data = destroy_gl_shape_data;
  binding->submit = [](auto state, auto proxy) {
    if (draw_gl_mesh_shape(state, proxy)) return;
    auto runtime = flight::render_gl::get_gl_render_state_runtime(state);
    if (runtime->registry_miss && *runtime->registry_miss) runtime->registry_miss.value()->callable(flight::types::RenderRegistryTable::ShapeRasterizer, flight::types::shape_kind);
  };
  descriptor->native_gl_scene_2d = binding;
  return descriptor;
}();
} // namespace flight::scene2d_gl
