// Complete native lowering of canvasTextureResolver.ts at the pinned Flight revision.
#pragma once

#include <functional>
#include <optional>
#include <variant>

#include <flight/entity/entity.hpp>
#include <flight/render/render_state.hpp>
#include <flight/scene2d_canvas/canvas_render_surface.hpp>
#include <flight/types/canvas_texture_resolver.hpp>
#include <flight/types/render_registry_signals.hpp>
#include <flight/types/texture.hpp>
#include <flight/weak_map.hpp>

namespace flight::scene2d_canvas {

using flight::types::CanvasRenderSurface;
using flight::types::CanvasRenderSurfaceCreator;
using flight::types::CanvasRenderSurfaceOptions;
using flight::types::CanvasTextureResolver;
using flight::types::CanvasTextureResolvers;
using flight::types::RenderRegistryTable;
using flight::types::Texture;
using flight::types::Texture2D;

using ResolverRegistry = flight::Map<flight::String, CanvasTextureResolver>;
using RegistryMiss = std::function<void(RenderRegistryTable, flight::String)>;

inline flight::WeakMap<flight::Ref<CanvasTextureResolvers>,
                       flight::Set<flight::Ref<CanvasRenderSurface>>>
    owned_surfaces;

inline void connect_canvas_texture_resolver_misses(
    flight::Ref<CanvasTextureResolvers> resolvers,
    flight::Ref<flight::types::RenderState> state) {
  const auto runtime = flight::render::get_render_state_runtime(state);
  resolvers->registry_miss = decltype(CanvasTextureResolvers::registry_miss){
      std::in_place_type<RegistryMiss>,
      [runtime](RenderRegistryTable registry, flight::String kind) {
        const auto& slot = runtime->registry_miss;
        if (!slot.has_value() || !slot.value()) return;
        (*slot.value())(registry, kind);
      }};
}

inline void register_canvas_texture_resolver(
    flight::Ref<CanvasTextureResolvers> resolvers,
    flight::String source_kind,
    std::optional<CanvasTextureResolver> resolver) {
  auto* registry = std::get_if<ResolverRegistry>(&resolvers->registry);
  if (!registry) {
    resolvers->registry = ResolverRegistry{};
    registry = std::get_if<ResolverRegistry>(&resolvers->registry);
  }
  if (!resolver.has_value()) registry->erase(source_kind);
  else registry->set(source_kind, resolver.value());
}

inline std::optional<flight::host_sdl::ImageSource> resolve_canvas_texture(
    flight::Ref<CanvasTextureResolvers> resolvers,
    Texture texture) {
  const auto* texture_2d = std::get_if<flight::Ref<Texture2D>>(&texture);
  if (!texture_2d || !*texture_2d || !(*texture_2d)->source.has_value()) return std::nullopt;

  const flight::String source_kind = (*texture_2d)->source.value()->kind;
  const auto* registry = std::get_if<ResolverRegistry>(&resolvers->registry);
  const auto resolver = registry ? registry->get(source_kind) : std::nullopt;
  if (!resolver.has_value()) {
    if (const auto* miss = std::get_if<RegistryMiss>(&resolvers->registry_miss)) {
      (*miss)(RenderRegistryTable::TextureResolver, source_kind);
    }
    return std::nullopt;
  }
  return resolver.value()(resolvers, std::move(texture));
}

inline std::optional<flight::Ref<CanvasRenderSurface>> acquire_canvas_texture_resolver_surface(
    flight::Ref<CanvasTextureResolvers> resolvers,
    flight::Ref<CanvasRenderSurfaceOptions> options) {
  auto surface = acquire_canvas_render_surface(resolvers->surface_creator, options);
  if (surface.has_value()) {
    if (auto surfaces = owned_surfaces.get(resolvers)) surfaces->add(surface.value());
  }
  return surface;
}

inline void destroy_canvas_texture_resolvers(flight::Ref<CanvasTextureResolvers> resolvers) {
  const auto surfaces = owned_surfaces.get(resolvers);
  if (!surfaces.has_value()) return;
  (void)owned_surfaces.erase(resolvers);
  for (const auto& surface : surfaces.value()) destroy_canvas_render_surface(surface);
  surfaces->clear();
  if (auto* registry = std::get_if<ResolverRegistry>(&resolvers->registry)) registry->clear();
  resolvers->registry = decltype(CanvasTextureResolvers::registry){
      std::in_place_type<flight::Null>, flight::null};
  resolvers->registry_miss = decltype(CanvasTextureResolvers::registry_miss){
      std::in_place_type<flight::Null>, flight::null};
}

inline void initialize_canvas_texture_resolvers(
    flight::types::EntityConstruction<flight::Ref<CanvasTextureResolvers>> resolvers,
    flight::Ref<CanvasRenderSurfaceCreator> surface_creator) {
  using CreatorView = flight::StructuralRef<flight::RowReadonly<flight::RowOf<
      flight::Ref<CanvasRenderSurfaceCreator>>>>;
  resolvers.shared_owner()->set_named_value(
      "registry", decltype(CanvasTextureResolvers::registry){
                      std::in_place_type<flight::Null>, flight::null});
  resolvers.shared_owner()->set_named_value(
      "registryMiss", decltype(CanvasTextureResolvers::registry_miss){
                          std::in_place_type<flight::Null>, flight::null});
  resolvers.shared_owner()->set_named_value(
      "surfaceCreator", flight::structural_ref_cast<CreatorView>(surface_creator));
  flight::row_set(
      resolvers,
      flight::types::entity_runtime_key,
      flight::make_ref<flight::types::EntityRuntime>(
          flight::types::EntityRuntime{.binding = std::nullopt, .uid = std::nullopt}));
}

inline flight::Ref<CanvasTextureResolvers> create_canvas_texture_resolvers(
    flight::Ref<CanvasRenderSurfaceCreator> surface_creator) {
  auto construction =
      flight::entity::allocate_entity<flight::Ref<CanvasTextureResolvers>>();
  initialize_canvas_texture_resolvers(construction, surface_creator);
  auto resolvers =
      flight::entity::finish_entity<flight::Ref<CanvasTextureResolvers>>(construction);
  owned_surfaces.set(resolvers, flight::Set<flight::Ref<CanvasRenderSurface>>{});
  return resolvers;
}

}  // namespace flight::scene2d_canvas
