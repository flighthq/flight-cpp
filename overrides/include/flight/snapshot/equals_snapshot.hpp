// Derived from @flighthq/snapshot/packages/snapshot/src/equalsSnapshot.ts at
// a62784923f3be814463286c9fb28edfe6ab15789. The generated module is refused at the erased-array
// view and imported Snapshot<T> alias boundaries.
#pragma once

#include <cmath>

#include <flight/runtime.hpp>
#include <flight/snapshot/capture_snapshot.hpp>

static_assert(flight::runtime_contract.compiler_contract == "flight-runtime-contract/2",
              "Flight compiler/runtime contract mismatch");
static_assert(flight::runtime_contract.cpp_abi == 1, "Flight C++ runtime ABI mismatch");

namespace flight::snapshot {

inline bool snapshot_values_equal(const SnapshotValue& a, const SnapshotValue& b) {
  if (a.same_identity(b)) return true;
  if (a.kind() != b.kind()) return false;

  switch (a.kind()) {
    case SnapshotValue::Kind::undefined:
    case SnapshotValue::Kind::null: return true;
    case SnapshotValue::Kind::boolean: return a.as_boolean() == b.as_boolean();
    // C++ == gives the source's strict-equality leaf rules here: NaN differs from itself and
    // negative zero equals positive zero.
    case SnapshotValue::Kind::number: return a.as_number() == b.as_number();
    case SnapshotValue::Kind::string: return a.as_string() == b.as_string();
    case SnapshotValue::Kind::non_plain:
      // Non-plain values are leaves under the package contract. Distinct clones are distinct
      // references even if they name the same host kind.
      return false;
    case SnapshotValue::Kind::array:
      if (a.size() != b.size()) return false;
      for (std::size_t index = 0; index < a.size(); ++index) {
        if (!snapshot_values_equal(a.array_at(index), b.array_at(index))) return false;
      }
      return true;
    case SnapshotValue::Kind::object:
      if (a.size() != b.size()) return false;
      for (const auto& [key, value] : a.object_entries()) {
        const auto* other = static_cast<const SnapshotValue&>(b).object_get(key);
        if (other == nullptr || !snapshot_values_equal(value, *other)) return false;
      }
      return true;
  }
  return false;
}

inline bool equals_snapshot(const SnapshotValue& a, const SnapshotValue& b) {
  return snapshot_values_equal(a, b);
}

}  // namespace flight::snapshot
