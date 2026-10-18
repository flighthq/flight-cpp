// A derived record declared in ANOTHER package's namespace and header than its base, as the SDK's
// cross-package interface heritage does.
#pragma once
#include "allocation_descriptor_base.hpp"
namespace peer_package {
struct Screen : base_package::Target {
  using flight_native_base = base_package::Target;
  double presentation{0.0};
};
} // namespace peer_package
