// Native C++ override for @flighthq/scene3d-gl's default registry pipeline.
#pragma once

#include <flight/registry/registry_table.hpp>
#include <flight/render_gl/gl_pipeline.hpp>
#include <flight/scene3d_gl/standard_pbr_gl_mesh_material_renderer.hpp>
#include <flight/structural_ref.hpp>
#include <flight/types/contract.hpp>

namespace flight::scene3d_gl {

// The browser registry spreads the complete Scene2D GL registry before adding every Scene3D
// material family. Native examples must not instantiate unavailable Scene2D DOM/canvas renderers.
// Build the usable native registry directly and register only material implementations that have
// a complete WebGL-shaped host path. Missing kinds continue to take Flight's observable registry-
// miss path instead of being substituted with a look-alike renderer.
inline flight::Ref<flight::types::GlRenderRegistries>
build_native_scene3_dgl_render_registries() {
  auto registries = flight::render_gl::allocate_empty_gl_render_registries();
  registries->mesh_material_renderers =
      flight::registry::with_registry_table_entry<
          flight::Ref<flight::types::GlMeshMaterialRenderer>>(
          flight::StructuralRef<flight::RowReadonly<flight::RowOf<std::shared_ptr<
              flight::types::KeyedTable<flight::Ref<flight::types::GlMeshMaterialRenderer>>>>>>(
              registries->mesh_material_renderers),
          flight::types::standard_pbr_material_kind,
          flight::scene3d_gl::standard_pbr_gl_mesh_material_renderer);
  return registries;
}

inline flight::StructuralRef<flight::RowReadonly<
    flight::RowOf<flight::Ref<flight::types::GlRenderRegistries>>>>
    default_scene3_dgl_render_registries =
        flight::StructuralRef<flight::RowWritable<
            flight::RowOf<flight::Ref<flight::types::GlRenderRegistries>>>>(
            build_native_scene3_dgl_render_registries());

} // namespace flight::scene3d_gl
