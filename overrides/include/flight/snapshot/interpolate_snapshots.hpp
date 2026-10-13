// Derived from @flighthq/snapshot/packages/snapshot/src/interpolateSnapshots.ts at
// a62784923f3be814463286c9fb28edfe6ab15789. The generated module is refused at its erased empty
// array and imported Snapshot<T> boundaries, and its retained helpers contain invalid object casts.
#pragma once

#include <algorithm>
#include <cstddef>
#include <optional>

#include <flight/runtime.hpp>
#include <flight/snapshot/capture_snapshot.hpp>

static_assert(flight::runtime_contract.compiler_contract == "flight-runtime-contract/2",
              "Flight compiler/runtime contract mismatch");
static_assert(flight::runtime_contract.cpp_abi == 1, "Flight C++ runtime ABI mismatch");

namespace flight::snapshot {

inline flight::String snapshot_path(const flight::String& prefix, const flight::String& key) {
  return prefix.empty() ? key : prefix + flight::String(".") + key;
}

inline SnapshotValue clone_snapshot_value(const SnapshotValue& value) {
  return value.is_container() || value.is_non_plain() ? clone_snapshot_graph(value, false) : value;
}

namespace detail {

inline bool schema_has(const flight::Array<flight::String>& paths, const flight::String& path) {
  return std::find(paths.begin(), paths.end(), path) != paths.end();
}

inline bool should_interpolate(
    const std::optional<flight::Array<flight::String>>& paths,
    const flight::String& prefix,
    const flight::String& key) {
  return !paths.has_value() || schema_has(*paths, snapshot_path(prefix, key));
}

inline void interpolate_snapshots_into(
    SnapshotValue& out,
    const SnapshotValue& a,
    const SnapshotValue& b,
    double t,
    const std::optional<flight::Array<flight::String>>& paths,
    const flight::String& prefix);

inline void interpolate_child(
    SnapshotValue& output,
    const SnapshotValue* a_value,
    const SnapshotValue& b_value,
    double t,
    const std::optional<flight::Array<flight::String>>& paths,
    const flight::String& path_prefix,
    const flight::String& key) {
  if (a_value != nullptr && a_value->kind() == SnapshotValue::Kind::number &&
      b_value.kind() == SnapshotValue::Kind::number) {
    output = should_interpolate(paths, path_prefix, key)
                 ? SnapshotValue(a_value->as_number() +
                                 (b_value.as_number() - a_value->as_number()) * t)
                 : b_value;
    return;
  }

  if (a_value != nullptr && a_value->is_container() && b_value.is_container() &&
      a_value->kind() == b_value.kind()) {
    if (!output.is_container() || output.kind() != b_value.kind()) {
      output = b_value.is_array() ? SnapshotValue::array() : SnapshotValue::object();
    }
    interpolate_snapshots_into(
        output, *a_value, b_value, t, paths,
        paths.has_value() ? snapshot_path(path_prefix, key) : flight::String());
    return;
  }

  output = clone_snapshot_value(b_value);
}

inline void interpolate_snapshots_into(
    SnapshotValue& out,
    const SnapshotValue& a,
    const SnapshotValue& b,
    double t,
    const std::optional<flight::Array<flight::String>>& paths,
    const flight::String& prefix) {
  if (b.is_array()) {
    if (!out.is_array() || !a.is_array()) return;
    out.array_resize(b.size());
    for (std::size_t index = 0; index < b.size(); ++index) {
      const auto key = flight::String::from_number(static_cast<double>(index));
      const SnapshotValue* a_value = index < a.size() ? &a.array_at(index) : nullptr;
      auto& output = out.array_at(index);
      interpolate_child(output, a_value, b.array_at(index), t, paths, prefix, key);
    }
    return;
  }

  if (!b.is_object() || !out.is_object() || !a.is_object()) return;
  for (const auto& [key, b_value] : b.object_entries()) {
    const SnapshotValue* a_value = static_cast<const SnapshotValue&>(a).object_get(key);
    auto* output = out.object_get(key);
    if (output == nullptr) {
      out.object_set(key, SnapshotValue());
      output = out.object_get(key);
    }
    interpolate_child(*output, a_value, b_value, t, paths, prefix, key);
  }
}

}  // namespace detail

inline void interpolate_snapshots(
    const SnapshotValue& a,
    const SnapshotValue& b,
    double t,
    SnapshotValue& out,
    std::optional<flight::Array<flight::String>> schema = std::nullopt) {
  if (!a.is_container() || !b.is_container() || !out.is_container()) return;
  detail::interpolate_snapshots_into(
      out, a, b, std::clamp(t, 0.0, 1.0), schema, flight::String());
}

}  // namespace flight::snapshot
