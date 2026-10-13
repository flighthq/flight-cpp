// Derived from @flighthq/snapshot/packages/snapshot/src/restoreSnapshot.ts at
// a62784923f3be814463286c9fb28edfe6ab15789. The generated public entry point is refused at the
// imported Snapshot<T> alias and its retained helpers contain invalid erased container casts.
#pragma once

#include <cstddef>

#include <flight/runtime.hpp>
#include <flight/snapshot/capture_snapshot.hpp>

static_assert(flight::runtime_contract.compiler_contract == "flight-runtime-contract/2",
              "Flight compiler/runtime contract mismatch");
static_assert(flight::runtime_contract.cpp_abi == 1, "Flight C++ runtime ABI mismatch");

namespace flight::snapshot {

inline void restore_snapshot_into(SnapshotValue& target, const SnapshotValue& source);

inline void restore_snapshot_value(SnapshotValue& target, const SnapshotValue& source) {
  if (!source.is_container()) {
    target = source.is_non_plain() ? clone_snapshot_graph(source, false) : source;
    return;
  }
  if (target.is_container() && target.kind() == source.kind()) {
    restore_snapshot_into(target, source);
    return;
  }
  target = clone_snapshot_graph(source, false);
}

inline void restore_snapshot_into(SnapshotValue& target, const SnapshotValue& source) {
  if (source.is_array()) {
    if (!target.is_array()) return;
    target.array_resize(source.size());
    for (std::size_t index = 0; index < source.size(); ++index) {
      restore_snapshot_value(target.array_at_mutable(index), source.array_at(index));
    }
    return;
  }

  if (!source.is_object() || !target.is_object()) return;
  for (const auto& [key, source_value] : source.object_entries()) {
    auto* target_value = target.object_get_mutable(key);
    if (target_value == nullptr) {
      target.object_set(key, clone_snapshot_graph(source_value, false));
      continue;
    }
    restore_snapshot_value(*target_value, source_value);
  }
}

inline void restore_snapshot(const SnapshotValue& snapshot, SnapshotValue& target) {
  if (!snapshot.is_container() || !target.is_container()) return;
  if (snapshot.kind() != target.kind()) return;
  restore_snapshot_into(target, snapshot);
}

}  // namespace flight::snapshot
