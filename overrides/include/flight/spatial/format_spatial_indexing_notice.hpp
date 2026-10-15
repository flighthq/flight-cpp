// Derived from @flighthq/spatial/packages/spatial/src/formatSpatialIndexingNotice.ts.
#pragma once

#include <flight/runtime.hpp>
#include <flight/structural_ref.hpp>

static_assert(flight::runtime_contract.compiler_contract == "flight-runtime-contract/2", "Flight compiler/runtime contract mismatch");
static_assert(flight::runtime_contract.cpp_abi == 1, "Flight C++ runtime ABI mismatch");

#include <flight/types/spatial_indexing.hpp>
#include <flight/spatial/uniform_grid.hpp>

namespace flight::spatial {

using flight::types::SpatialIndexingNotice;

// The parameter carrier is the one SpatialIndexingGuard hands its callback, so the source's one-line
// composition -- setSpatialIndexingGuard((notice) => ...formatSpatialIndexingNotice(notice)...) --
// passes the guard's notice straight through. Each interpolation is the source's `${...}`: a number
// renders through flight::to_string (JavaScript Number-to-String), a string verbatim.
inline flight::String format_spatial_indexing_notice(
    flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::SpatialIndexingNotice>>>> notice) {
  const auto reason = flight::row_get<flight::RowKey<"reason">>(notice);
  const flight::String mode = flight::row_get<flight::RowKey<"mode">>(notice);
  const flight::String operation = flight::row_get<flight::RowKey<"operation">>(notice);
  const flight::String id = flight::to_string(flight::row_get<flight::RowKey<"id">>(notice));
  if (reason == flight::String("invalid-cell-size")) {
    return flight::String("the uniform grid's cellSize (") +
           flight::to_string(flight::row_get<flight::RowKey<"cellSize">>(notice)) +
           flight::String(") must be a positive finite number. ") + operation + flight::String("SpatialObject(") + id +
           flight::String(") used the bounded overflow path instead, so results remain correct but queries scan this object.");
  }
  if (reason == flight::String("inverted-bounds")) {
    return operation + flight::String("SpatialObject(") + id +
           flight::String("): no minimum may exceed its matching maximum, so the object was not indexed and no query will "
                          "return it. The operation returns false for this — normalize or correct the bounds upstream.");
  }
  if (reason == flight::String("missing-id")) {
    if (operation == flight::String("remove")) {
      return flight::String("removeSpatialObject(") + id +
             flight::String("): the id was not indexed, so removal was a no-op. Check the object's indexing lifecycle if "
                            "this was unexpected.");
    }
    return flight::String("updateSpatialObject(") + id +
           flight::String("): the id was not indexed, so update used its documented insert behavior and left the object in '") +
           mode + flight::String("' mode. Use insertSpatialObject for a new id, or check the object's indexing lifecycle.");
  }
  if (mode == flight::String("declined")) {
    return operation + flight::String("SpatialObject(") + id +
           flight::String("): the bounds are not finite, so the object was not indexed and no query will return it. The "
                          "operation returns false for this — check the sentinel, and check what produced NaN/Infinity "
                          "bounds upstream.");
  }
  if (mode == flight::String("overflow")) {
    return operation + flight::String("SpatialObject(") + id + flight::String("): the bounds span ") +
           flight::to_string(flight::row_get<flight::RowKey<"wouldOccupyBucketCount">>(notice)) +
           flight::String(" cells, over the ") + flight::to_string(max_indexed_cells_per_object) +
           flight::String(" per-object budget, so the object is held in the flat overflow list instead of the grid. "
                          "Results are unaffected. If this is not a one-off outlier, the grid's cellSize is too small for "
                          "the objects being indexed — size it to a typical object.");
  }
  return operation + flight::String("SpatialObject(") + id + flight::String("): indexed as '") + mode +
         flight::String("', which carries no caller-facing advice.");
}

} // namespace flight::spatial
