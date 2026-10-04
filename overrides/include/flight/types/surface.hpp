// Derived from @flighthq/types/packages/types/src/Surface.ts.
#pragma once

#include <optional>

#include <flight/any.hpp>
#include <flight/erased_ref.hpp>
#include <flight/runtime.hpp>
#include <flight/types/entity.hpp>

static_assert(flight::runtime_contract.compiler_contract == "flight-runtime-contract/2",
              "Flight compiler/runtime contract mismatch");
static_assert(flight::runtime_contract.cpp_abi == 1,
              "Flight C++ runtime ABI mismatch");

namespace flight::types {

using NativeSurfaceHandle = flight::Any;

struct Surface : public flight::ReferenceEnabled {
  std::optional<flight::Ref<EntityRuntime>> entity_runtime_key;
};

// SurfaceRuntime extends EntityRuntime in the source. Retaining that heritage is what lets one
// owner live simultaneously in the generic EntityRuntime slot and in the surface-specific view;
// duplicating the base cells into an unrelated aggregate makes the source assertion impossible.
struct SurfaceRuntime : public EntityRuntime {
  NativeSurfaceHandle handle;
};

}  // namespace flight::types
