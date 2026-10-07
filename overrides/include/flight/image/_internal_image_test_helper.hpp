// Derived from @flighthq/image/packages/image/src/imageTestHelper.ts.
#pragma once

#include <flight/runtime.hpp>
#include <flight/types/host_image_dimensions.hpp>
#include <flight/types/host_image_source.hpp>
#include <flight/image/image_source_dimensions.hpp>

static_assert(
    flight::runtime_contract.compiler_contract == "flight-runtime-contract/2",
    "Flight compiler/runtime contract mismatch");
static_assert(
    flight::runtime_contract.cpp_abi == 1,
    "Flight C++ runtime ABI mismatch");

namespace flight::image {

inline flight::types::HostImageDimensionResolver test_image_dimension_resolver =
    [](flight::types::HostImageSource source,
       flight::Ref<flight::types::HostImageDimensions> out) -> bool {
  if (!source) return false;
  out->height = static_cast<double>(source.height());
  out->width = static_cast<double>(source.width());
  return true;
};

inline void register_test_image_dimension_resolver() {
  register_host_image_dimension_resolver(test_image_dimension_resolver);
}

inline void unregister_test_image_dimension_resolver() {
  unregister_host_image_dimension_resolver();
}

} // namespace flight::image
