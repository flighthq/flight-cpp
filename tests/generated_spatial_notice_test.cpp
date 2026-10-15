// Native parity for @flighthq/spatial's formatSpatialIndexingNotice at a627849, which the emitter refuses
// and overrides/include/flight/spatial/format_spatial_indexing_notice.hpp supplies.
//
// Every source branch is pinned to its exact text, in source order: invalid-cell-size, inverted-bounds,
// missing-id on remove and on update, declined, overflow, and the no-advice fallback. Reason is tested
// before mode, as the source does, so a reasoned notice in a declined or overflow mode takes the reason
// text; a null reason falls through to the mode tests. Interpolated numbers render as JavaScript
// Number-to-String would (integers without a fraction, NaN, Infinity, -0 as "0", exponent form).
// `scripts/spatialNoticeOracle.mjs` checks the same function against the pinned TypeScript source itself.
//
// The last case goes through the real seam: a guard installed with setSpatialIndexingGuard formats the
// notice a uniform grid reports for inverted bounds, which is the source's documented one-line
// composition and proves the formatter accepts the guard's own notice carrier.
#include <flight/spatial/format_spatial_indexing_notice.hpp>
#include <flight/spatial/spatial_index.hpp>
#include <flight/spatial/spatial_indexing_guard.hpp>
#include <flight/spatial/uniform_grid.hpp>

#include <iostream>
#include <limits>
#include <optional>
#include <string>

namespace {

namespace spatial = flight::spatial;
namespace types = flight::types;

int failures = 0;

void expect(const flight::String& actual, const std::string& expected, const char* name) {
  if (actual.to_utf8() == expected) return;
  std::cerr << "FAIL: " << name << "\n  expected: " << expected << "\n  actual:   " << actual.to_utf8() << '\n';
  ++failures;
}

flight::Ref<types::SpatialIndexingNotice> notice(
    const char* operation, const char* mode, std::optional<const char*> reason, double id = 7.0,
    double cell_size = 128.0, double would_occupy = 0.0) {
  auto value = flight::make_ref<types::SpatialIndexingNotice>();
  value->cell_size = cell_size;
  value->id = id;
  value->mode = flight::String(mode);
  value->operation = flight::String(operation);
  value->would_occupy_bucket_count = would_occupy;
  value->reason = reason ? std::optional<flight::String>(flight::String(*reason)) : std::nullopt;
  return value;
}

flight::String format(const flight::Ref<types::SpatialIndexingNotice>& value) {
  return spatial::format_spatial_indexing_notice(value);
}

void check_branches() {
  expect(format(notice("insert", "overflow", "invalid-cell-size", 7.0, -2.5)),
         "the uniform grid's cellSize (-2.5) must be a positive finite number. insertSpatialObject(7) used the bounded "
         "overflow path instead, so results remain correct but queries scan this object.",
         "invalid-cell-size");
  expect(format(notice("update", "declined", "inverted-bounds", 12.0)),
         "updateSpatialObject(12): no minimum may exceed its matching maximum, so the object was not indexed and no "
         "query will return it. The operation returns false for this — normalize or correct the bounds upstream.",
         "inverted-bounds takes the reason text over the declined mode");
  expect(format(notice("remove", "absent", "missing-id", 3.0)),
         "removeSpatialObject(3): the id was not indexed, so removal was a no-op. Check the object's indexing "
         "lifecycle if this was unexpected.",
         "missing-id on remove");
  expect(format(notice("update", "cells", "missing-id", 4.0)),
         "updateSpatialObject(4): the id was not indexed, so update used its documented insert behavior and left the "
         "object in 'cells' mode. Use insertSpatialObject for a new id, or check the object's indexing lifecycle.",
         "missing-id on update names the resulting mode");
  expect(format(notice("insert", "declined", std::nullopt, 5.0)),
         "insertSpatialObject(5): the bounds are not finite, so the object was not indexed and no query will return "
         "it. The operation returns false for this — check the sentinel, and check what produced NaN/Infinity bounds "
         "upstream.",
         "declined with a null reason");
  expect(format(notice("update", "overflow", std::nullopt, 9.0, 1.0, 2048.0)),
         "updateSpatialObject(9): the bounds span 2048 cells, over the 1024 per-object budget, so the object is held "
         "in the flat overflow list instead of the grid. Results are unaffected. If this is not a one-off outlier, the "
         "grid's cellSize is too small for the objects being indexed — size it to a typical object.",
         "overflow interpolates the span and the per-object budget");
  expect(format(notice("insert", "cells", std::nullopt, 1.0)),
         "insertSpatialObject(1): indexed as 'cells', which carries no caller-facing advice.",
         "an unadvised mode falls back to the neutral text");
  expect(format(notice("remove", "overflow", "unrecognized-reason", 2.0, 1.0, 5000.0)),
         "removeSpatialObject(2): the bounds span 5000 cells, over the 1024 per-object budget, so the object is held "
         "in the flat overflow list instead of the grid. Results are unaffected. If this is not a one-off outlier, the "
         "grid's cellSize is too small for the objects being indexed — size it to a typical object.",
         "a reason the formatter does not name falls through to the mode tests");
}

void check_number_rendering() {
  const auto cell = [](double cell_size) {
    return format(notice("insert", "overflow", "invalid-cell-size", 7.0, cell_size));
  };
  const std::string prefix = "the uniform grid's cellSize (";
  const std::string suffix =
      ") must be a positive finite number. insertSpatialObject(7) used the bounded overflow path instead, so results "
      "remain correct but queries scan this object.";
  expect(cell(std::numeric_limits<double>::quiet_NaN()), prefix + "NaN" + suffix, "NaN renders as NaN");
  expect(cell(std::numeric_limits<double>::infinity()), prefix + "Infinity" + suffix, "Infinity renders as Infinity");
  expect(cell(-0.0), prefix + "0" + suffix, "negative zero renders as 0");
  expect(cell(0.1 + 0.2), prefix + "0.30000000000000004" + suffix, "shortest round-trip digits");
  expect(cell(1e21), prefix + "1e+21" + suffix, "exponent form at 1e21");
  expect(format(notice("insert", "cells", std::nullopt, 1.5)),
         "insertSpatialObject(1.5): indexed as 'cells', which carries no caller-facing advice.",
         "a fractional id renders with its fraction");
}

void check_guard_composition() {
  std::optional<flight::String> seen;
  spatial::set_spatial_indexing_guard(
      [&seen](flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<types::SpatialIndexingNotice>>>> value) {
        seen = spatial::format_spatial_indexing_notice(value);
      });
  auto index = spatial::create_spatial_index2_d(spatial::create_uniform_grid_spatial_backend2_d(10.0));
  const auto inverted = flight::make_ref<types::SpatialAabb2D>(
      types::SpatialAabb2D{.min_x = 4.0, .min_y = 0.0, .max_x = 0.0, .max_y = 4.0});
  const bool inserted = spatial::insert_spatial_object2_d(index, 21.0, inverted);
  spatial::set_spatial_indexing_guard(std::nullopt);
  if (inserted) {
    std::cerr << "FAIL: inverted bounds were indexed\n";
    ++failures;
  }
  if (!seen) {
    std::cerr << "FAIL: the guard saw no notice for inverted bounds\n";
    ++failures;
    return;
  }
  expect(*seen,
         "insertSpatialObject(21): no minimum may exceed its matching maximum, so the object was not indexed and no "
         "query will return it. The operation returns false for this — normalize or correct the bounds upstream.",
         "guard composition formats the grid's own inverted-bounds notice");
}

}  // namespace

int main() {
  check_branches();
  check_number_rendering();
  check_guard_composition();
  if (failures != 0) {
    std::cerr << failures << " spatial notice check(s) failed\n";
    return 1;
  }
  std::cout << "spatial notice checks passed\n";
  return 0;
}
