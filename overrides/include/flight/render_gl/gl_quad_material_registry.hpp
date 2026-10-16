// Source-derived native material registry; descriptors remain the registered owners.
#pragma once
#include <flight/render_gl/gl_render_state.hpp>
#include <flight/registry/registry_table.hpp>
namespace flight::render_gl {
inline void register_gl_quad_material_renderer(flight::Ref<flight::types::GlRenderState> state, flight::String kind, flight::Ref<flight::types::GlQuadMaterialRenderer> renderer) {
  auto runtime = get_gl_render_state_runtime(state);
  runtime->registries->material_renderers = flight::registry::with_kind_map_entry(runtime->registries->material_renderers, kind, std::variant<flight::Ref<flight::types::GlMeshMaterialRenderer>, flight::Ref<flight::types::GlQuadMaterialRenderer>>{renderer});
}
template <typename Material>
inline std::optional<flight::Ref<flight::types::GlQuadMaterialRenderer>> resolve_gl_quad_material_renderer(flight::Ref<flight::types::GlRenderState> state, std::optional<Material> material) {
  auto runtime = get_gl_render_state_runtime(state);
  auto kind = flight::types::standard_material_kind;
  if (material) {
    if constexpr (requires { typename Material::schema_type; })
      kind = flight::row_get<flight::RowKey<"kind">>(*material);
    else
      kind = (*material)->kind;
  }
  auto entry = runtime->registries->material_renderers.get(kind);
  if (entry) return std::get<flight::Ref<flight::types::GlQuadMaterialRenderer>>(*entry);
  if (runtime->registry_miss && *runtime->registry_miss) runtime->registry_miss.value()->callable(flight::types::RenderRegistryTable::MaterialRenderer, kind);
  return std::nullopt;
}
} // namespace flight::render_gl
