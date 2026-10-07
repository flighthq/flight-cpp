// Hand-written override that keeps the surface module off the generated canvas render-state
// implementation cycle. Most construction functions in the source module remain compiler-refused;
// the retained registration and owned-surface teardown paths need only the generated data types.
#pragma once

#include <optional>

#include <flight/runtime.hpp>
#include <flight/types/canvas_render_state.hpp>
#include <flight/types/canvas_render_surface.hpp>
#include <flight/weak_map.hpp>

namespace flight::scene2d_canvas {

inline flight::Ref<flight::types::CanvasRenderStateRuntime> get_canvas_render_state_runtime(
    flight::Ref<flight::types::CanvasRenderState> state);

inline flight::WeakMap<flight::Ref<flight::types::CanvasRenderSurface>,
                       flight::Ref<flight::types::CanvasRenderSurfaceCreator>>
    owned_surface_creators;

inline void register_canvas_surface_creator(
    flight::Ref<flight::types::CanvasRenderState> state,
    flight::Ref<flight::types::CanvasRenderSurfaceCreator> creator) {
  using CreatorView = flight::StructuralRef<flight::RowReadonly<flight::RowOf<
      flight::Ref<flight::types::CanvasRenderSurfaceCreator>>>>;
  get_canvas_render_state_runtime(state)->canvas_surface_creator =
      flight::structural_ref_cast<CreatorView>(creator);
}

inline flight::StructuralRef<flight::RowReadonly<flight::RowOf<
    flight::Ref<flight::types::CanvasRenderSurfaceCreator>>>> get_canvas_surface_creator(
    flight::Ref<flight::types::CanvasRenderState> state) {
  const auto creator = get_canvas_render_state_runtime(state)->canvas_surface_creator;
  if (!creator.has_value()) {
    throw flight::Error(flight::String(
        "This CanvasRenderState has no surface creator — call registerCanvasSurfaceCreator first"));
  }
  return creator.value();
}

inline std::optional<flight::Ref<flight::types::CanvasRenderSurface>> acquire_canvas_render_surface(
    flight::StructuralRef<flight::RowReadonly<flight::RowOf<
        flight::Ref<flight::types::CanvasRenderSurfaceCreator>>>>,
    flight::StructuralRef<flight::RowReadonly<flight::RowOf<
        flight::Ref<flight::types::CanvasRenderSurfaceOptions>>>>) {
  // The SDL binding maps HTMLCanvasElement to its GL-only canvas. It has no Canvas2D rasterizer
  // from which a CanvasRenderSurface can be constructed, so acquisition fails through the source
  // API's nullable result without creating a host surface that would immediately need teardown.
  return std::nullopt;
}

inline void destroy_canvas_render_surface(flight::Ref<flight::types::CanvasRenderSurface> surface) {
  const auto creator = owned_surface_creators.get(surface);
  if (!creator.has_value()) return;
  owned_surface_creators.erase(surface);
  creator.value()->destroy_render_surface(surface->canvas);
}

}  // namespace flight::scene2d_canvas
