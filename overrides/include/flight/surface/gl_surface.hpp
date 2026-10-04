// Derived from @flighthq/surface/packages/surface/src/glSurface.ts.
#pragma once

#include <optional>

#include <flight/entity/entity.hpp>
#include <flight/host_sdl/webgl.hpp>
#include <flight/runtime.hpp>
#include <flight/structural_ref.hpp>
#include <flight/surface/surface.hpp>
#include <flight/types/app_window.hpp>
#include <flight/types/gl_context.hpp>
#include <flight/types/gl_surface.hpp>
#include <flight/types/host_gl.hpp>

namespace flight::surface {

using ReadonlyGlCapability = flight::StructuralRef<flight::RowReadonly<
    flight::RowOf<flight::Ref<flight::types::HostGlCapability>>>>;
using ReadonlyGlAppWindow = flight::StructuralRef<flight::RowReadonly<
    flight::RowOf<flight::Ref<flight::types::AppWindow>>>>;
using ReadonlyGlOptions = flight::StructuralRef<flight::RowReadonly<
    flight::RowOf<flight::Ref<flight::types::GlContextOptions>>>>;
using ReadonlyGlSurface = flight::StructuralRef<flight::RowReadonly<
    flight::RowOf<flight::Ref<flight::types::GlSurface>>>>;

inline std::optional<flight::Ref<flight::types::GlSurface>>
create_gl_surface_from_native_handle(
    ReadonlyGlCapability capability,
    flight::types::NativeSurfaceHandle handle,
    std::optional<ReadonlyGlOptions> options = std::nullopt) {
  auto surface = allocate_surface<flight::Ref<flight::types::GlSurface>>(
      std::move(handle));
  flight::row_set<flight::RowKey<"__brand">>(surface, flight::String("GlSurface"));
  auto context = flight::row_get<flight::RowKey<"acquire">>(capability)(
      readonly_surface_view(surface), options);
  if (!context.has_value()) return std::nullopt;
  flight::row_set<flight::RowKey<"context">>(surface, context.value());
  return flight::entity::finish_entity<flight::Ref<flight::types::GlSurface>>(surface);
}

inline std::optional<flight::Ref<flight::types::GlSurface>> create_gl_surface(
    ReadonlyGlCapability capability,
    ReadonlyGlAppWindow window,
    double width,
    double height,
    std::optional<ReadonlyGlOptions> options = std::nullopt) {
  auto handle = flight::row_get<flight::RowKey<"create">>(capability)(
      std::move(window), width, height, options);
  if (!handle.has_value()) return std::nullopt;
  return create_gl_surface_from_native_handle(
      std::move(capability), handle.value(), options);
}

inline void destroy_gl_surface(
    ReadonlyGlCapability capability,
    ReadonlyGlSurface surface) {
  flight::row_get<flight::RowKey<"release">>(capability)(
      readonly_surface_view(surface));
}

}  // namespace flight::surface
