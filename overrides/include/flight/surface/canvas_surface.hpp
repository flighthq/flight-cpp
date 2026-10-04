// Derived from @flighthq/surface/packages/surface/src/canvasSurface.ts.
#pragma once

#include <optional>

#include <flight/canvas_2d.hpp>
#include <flight/entity/entity.hpp>
#include <flight/runtime.hpp>
#include <flight/structural_ref.hpp>
#include <flight/surface/surface.hpp>
#include <flight/types/app_window.hpp>
#include <flight/types/canvas_surface.hpp>
#include <flight/types/host_canvas.hpp>

namespace flight::surface {

using ReadonlyAppWindow = flight::StructuralRef<flight::RowReadonly<
    flight::RowOf<flight::Ref<flight::types::AppWindow>>>>;
using ReadonlyCanvasCapability = flight::StructuralRef<flight::RowReadonly<
    flight::RowOf<flight::Ref<flight::types::HostCanvasCapability>>>>;
using ReadonlyCanvasSurface = flight::StructuralRef<flight::RowReadonly<
    flight::RowOf<flight::Ref<flight::types::CanvasSurface>>>>;

inline std::optional<flight::Ref<flight::types::CanvasSurface>>
create_canvas_surface_from_native_handle(
    ReadonlyCanvasCapability capability,
    flight::types::NativeSurfaceHandle handle,
    std::optional<flight::CanvasRenderingContext2DSettings> options = std::nullopt) {
  auto surface = allocate_surface<flight::Ref<flight::types::CanvasSurface>>(
      std::move(handle));
  flight::row_set<flight::RowKey<"__brand">>(surface, flight::String("CanvasSurface"));
  auto context = flight::row_get<flight::RowKey<"acquire">>(capability)(
      readonly_surface_view(surface), options);
  if (!context.has_value()) return std::nullopt;
  flight::row_set<flight::RowKey<"context">>(surface, context.value());
  return flight::entity::finish_entity<flight::Ref<flight::types::CanvasSurface>>(surface);
}

inline std::optional<flight::Ref<flight::types::CanvasSurface>>
create_canvas_surface(
    ReadonlyCanvasCapability capability,
    ReadonlyAppWindow window,
    double width,
    double height,
    std::optional<flight::CanvasRenderingContext2DSettings> options = std::nullopt) {
  auto handle = flight::row_get<flight::RowKey<"create">>(capability)(
      std::move(window), width, height, options);
  if (!handle.has_value()) return std::nullopt;
  return create_canvas_surface_from_native_handle(
      std::move(capability), handle.value(), options);
}

inline void destroy_canvas_surface(
    ReadonlyCanvasCapability capability,
    ReadonlyCanvasSurface surface) {
  flight::row_get<flight::RowKey<"release">>(capability)(
      readonly_surface_view(surface));
}

}  // namespace flight::surface
