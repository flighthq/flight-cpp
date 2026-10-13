// Derived from @flighthq/snapshot/packages/snapshot/src/enableSnapshotGuards.ts at
// a62784923f3be814463286c9fb28edfe6ab15789. The generated module is refused at
// ArrayBuffer.isView and emits an uncompilable Any-to-ErasedRef Set membership.
#pragma once

#include <algorithm>
#include <string>
#include <unordered_set>
#include <vector>

#include <flight/log/contract.hpp>
#include <flight/record.hpp>
#include <flight/runtime.hpp>
#include <flight/snapshot/capture_snapshot.hpp>
#include <flight/types/log.hpp>

static_assert(flight::runtime_contract.compiler_contract == "flight-runtime-contract/2",
              "Flight compiler/runtime contract mismatch");
static_assert(flight::runtime_contract.cpp_abi == 1, "Flight C++ runtime ABI mismatch");

namespace flight::snapshot {

inline void disable_snapshot_guards() {
  set_snapshot_capture_guard(std::nullopt);
}

namespace detail {

inline void emit_snapshot_guard_warning(const flight::String& key, const flight::String& message) {
  flight::log::log_once(
      key,
      flight::types::LogLevel::Warn,
      flight::Record<flight::String, flight::Any>{
          {flight::String("message"), flight::Any(message)},
      },
      std::optional<std::optional<flight::String>>{
          std::optional<flight::String>{flight::String("snapshot")}});
}

inline void visit_snapshot_source(
    const SnapshotValue& value,
    std::unordered_set<const SnapshotValue::Node*>& seen,
    std::vector<flight::String>& non_plain,
    bool& cyclic) {
  if (value.identity() == nullptr) return;
  if (!seen.insert(value.node().get()).second) {
    cyclic = true;
    return;
  }
  if (value.is_non_plain()) {
    if (std::find(non_plain.begin(), non_plain.end(), value.non_plain_kind()) == non_plain.end()) {
      non_plain.push_back(value.non_plain_kind());
    }
    return;
  }
  if (value.is_array()) {
    for (std::size_t index = 0; index < value.size(); ++index) {
      visit_snapshot_source(value.array_at(index), seen, non_plain, cyclic);
    }
  } else if (value.is_object()) {
    for (const auto& entry : value.object_entries()) {
      visit_snapshot_source(entry.second, seen, non_plain, cyclic);
    }
  }
}

}  // namespace detail

inline void warn_on_unsupported_snapshot_source(const SnapshotValue& source) {
  std::unordered_set<const SnapshotValue::Node*> seen;
  std::vector<flight::String> non_plain;
  bool cyclic = false;
  detail::visit_snapshot_source(source, seen, non_plain, cyclic);

  if (!non_plain.empty()) {
    std::sort(non_plain.begin(), non_plain.end());
    flight::String kinds;
    for (std::size_t index = 0; index < non_plain.size(); ++index) {
      if (index > 0) kinds += flight::String(", ");
      kinds += non_plain[index];
    }
    detail::emit_snapshot_guard_warning(
        flight::String("snapshot:non-plain-source"),
        flight::String("captureSnapshot: source contains non-plain value(s) (") + kinds +
            flight::String("). They clone, but a frozen Map/Set stays mutable and equalsSnapshot ") +
            flight::String("compares them by reference, so the snapshot is neither immutable nor ") +
            flight::String("comparable. Convert to plain arrays/objects first."));
  }
  if (cyclic) {
    detail::emit_snapshot_guard_warning(
        flight::String("snapshot:cyclic-source"),
        flight::String("captureSnapshot: source contains a cycle. Capture handles it, but ") +
            flight::String("equalsSnapshot, interpolateSnapshots, and restoreSnapshot recurse ") +
            flight::String("without cycle tracking and will overflow the stack — restoreSnapshot ") +
            flight::String("only on the second restore into the same target."));
  }
}

inline void enable_snapshot_guards() {
  set_snapshot_capture_guard(SnapshotCaptureGuard{warn_on_unsupported_snapshot_source});
}

}  // namespace flight::snapshot
