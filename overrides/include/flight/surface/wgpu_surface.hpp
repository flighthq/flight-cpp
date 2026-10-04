// Derived from @flighthq/surface/packages/surface/src/wgpuSurface.ts.
#pragma once

#include <optional>

#include <flight/entity/entity.hpp>
#include <flight/runtime.hpp>
#include <flight/structural_ref.hpp>
#include <flight/surface/surface.hpp>
#include <flight/types/app_window.hpp>
#include <flight/types/wgpu_host.hpp>
#include <flight/types/wgpu_surface.hpp>

namespace flight::surface {

using ReadonlyWgpuCapability = flight::StructuralRef<flight::RowReadonly<
    flight::RowOf<flight::Ref<flight::types::HostWgpuCapability>>>>;
using ReadonlyWgpuAppWindow = flight::StructuralRef<flight::RowReadonly<
    flight::RowOf<flight::Ref<flight::types::AppWindow>>>>;
using ReadonlyWgpuOptions = flight::StructuralRef<flight::RowReadonly<
    flight::RowOf<flight::Ref<flight::types::WgpuHostAcquisitionOptions>>>>;
using ReadonlyWgpuSurface = flight::StructuralRef<flight::RowReadonly<
    flight::RowOf<flight::Ref<flight::types::WgpuSurface>>>>;
using ReadonlyWgpuAcquisition = flight::StructuralRef<flight::RowReadonly<
    flight::RowOf<flight::Ref<flight::types::WgpuHostAcquisition>>>>;

inline flight::Task<std::optional<flight::Ref<flight::types::WgpuSurface>>>
create_wgpu_surface_from_native_handle(
    ReadonlyWgpuCapability capability,
    flight::types::NativeSurfaceHandle handle,
    std::optional<ReadonlyWgpuOptions> options = std::nullopt) {
  auto surface = allocate_surface<flight::Ref<flight::types::WgpuSurface>>(
      std::move(handle));
  flight::row_set<flight::RowKey<"__brand">>(surface, flight::String("WgpuSurface"));
  flight::Ref<flight::types::WgpuHostAcquisition> acquisition;
  try {
    acquisition = co_await flight::row_get<flight::RowKey<"acquire">>(capability)(
        readonly_surface_view(surface),
        options.has_value()
            ? options.value()
            : flight::make_structural_ref<flight::RowReadonly<flight::RowOf<
                  flight::Ref<flight::types::WgpuHostAcquisitionOptions>>>>());
  } catch (...) {
    co_return std::nullopt;
  }
  flight::row_set<flight::RowKey<"acquisition">>(surface, acquisition);
  co_return flight::entity::finish_entity<flight::Ref<flight::types::WgpuSurface>>(
      surface);
}

inline flight::Task<std::optional<flight::Ref<flight::types::WgpuSurface>>>
create_wgpu_surface(
    ReadonlyWgpuCapability capability,
    ReadonlyWgpuAppWindow window,
    double width,
    double height,
    std::optional<ReadonlyWgpuOptions> options = std::nullopt) {
  auto handle = flight::row_get<flight::RowKey<"create">>(capability)(
      std::move(window), width, height);
  if (!handle.has_value()) co_return std::nullopt;
  co_return co_await create_wgpu_surface_from_native_handle(
      std::move(capability), handle.value(), options);
}

inline void destroy_wgpu_surface(
    ReadonlyWgpuCapability capability,
    ReadonlyWgpuSurface surface) {
  flight::row_get<flight::RowKey<"release">>(capability)(
      ReadonlyWgpuAcquisition(
          flight::row_get<flight::RowKey<"acquisition">>(surface)));
}

}  // namespace flight::surface
