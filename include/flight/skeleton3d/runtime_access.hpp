#pragma once

#include <flight/node/transform3d_access.hpp>
#include <flight/runtime.hpp>
#include <flight/types/skeleton3_d.hpp>

namespace flight::skeleton3d {

inline double get_skeleton3_djoint_index_by_name(
    flight::Ref<flight::types::Skeleton3D> skeleton,
    flight::String name) {
  const auto* names = std::get_if<flight::Array<flight::String>>(&skeleton->names);
  return names == nullptr ? -1.0 : static_cast<double>(names->index_of(name));
}

} // namespace flight::skeleton3d
