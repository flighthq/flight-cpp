// Declared EntityRuntime heritage for the a627849 selection runtime.
#pragma once

#include <flight/runtime.hpp>
#include <flight/types/entity.hpp>
#include <flight/types/path.hpp>

namespace flight::types {

struct LassoSelectionRuntime : public EntityRuntime {
  bool active;
  flight::Ref<Path> path;
};

} // namespace flight::types
