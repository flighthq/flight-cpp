// Derived from @flighthq/surface/packages/surface/src/surface.ts.
#pragma once

#include <memory>
#include <optional>
#include <utility>

#include <flight/entity/entity.hpp>
#include <flight/runtime.hpp>
#include <flight/structural_ref.hpp>
#include <flight/types/entity.hpp>
#include <flight/types/surface.hpp>

static_assert(flight::runtime_contract.compiler_contract == "flight-runtime-contract/2",
              "Flight compiler/runtime contract mismatch");
static_assert(flight::runtime_contract.cpp_abi == 1,
              "Flight C++ runtime ABI mismatch");

namespace flight::surface {

using ReadonlySurface = flight::StructuralRef<flight::RowReadonly<
    flight::RowOf<flight::Ref<flight::types::Surface>>>>;

template <typename Schema>
inline ReadonlySurface readonly_surface_view(
    const flight::StructuralRef<Schema>& surface) {
  return ReadonlySurface::from_owner(surface.shared_owner());
}

template <typename Type>
inline flight::types::EntityConstruction<Type> allocate_surface(
    flight::types::NativeSurfaceHandle handle) {
  auto surface = flight::entity::allocate_entity<Type>();
  auto runtime = flight::make_ref<flight::types::SurfaceRuntime>();
  runtime->binding = std::nullopt;
  runtime->uid = std::nullopt;
  runtime->handle = std::move(handle);
  flight::row_set(
      surface,
      flight::types::entity_runtime_key,
      std::optional<flight::Ref<flight::types::EntityRuntime>>{
          std::static_pointer_cast<flight::types::EntityRuntime>(runtime)});
  return surface;
}

inline flight::Ref<flight::types::SurfaceRuntime> get_surface_runtime(
    ReadonlySurface surface) {
  const auto runtime = flight::row_get<
      std::optional<flight::Ref<flight::types::EntityRuntime>>>(
      surface, flight::types::entity_runtime_key);
  return std::static_pointer_cast<flight::types::SurfaceRuntime>(runtime.value());
}

inline flight::types::NativeSurfaceHandle get_surface_handle(
    ReadonlySurface surface) {
  return get_surface_runtime(std::move(surface))->handle;
}

}  // namespace flight::surface
