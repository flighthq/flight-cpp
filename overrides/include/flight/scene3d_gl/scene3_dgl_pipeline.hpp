// Native partial preset derived from the pinned Scene3D GL pipeline.
#pragma once
#include <flight/render_gl/gl_render_state.hpp>
#include <flight/scene3d_gl/gl_standard_pbr_mesh_material_renderer.hpp>
#include <flight/types/gl_render_state_options.hpp>

namespace flight::scene3d_gl {
// Preserve the existing native preset's supported StandardPbr path. Other source material
// families and the Scene2D preset remain explicitly unsupported, never remapped to StandardPbr.
inline flight::Ref<flight::types::GlRenderStateOptions> build_native_scene3_dgl_render_preset() {
  auto options = flight::make_ref<flight::types::GlRenderStateOptions>();
  options->material_renderers.emplace();
  options->material_renderers->set(flight::types::standard_pbr_material_kind,
      gl_standard_pbr_mesh_material_renderer);
  return options;
}
inline const flight::Ref<flight::types::GlRenderStateOptions> gl_scene3_drender_preset =
    build_native_scene3_dgl_render_preset();
} // namespace flight::scene3d_gl
