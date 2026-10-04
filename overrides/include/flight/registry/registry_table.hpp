// Derived from @flighthq/registry/packages/registry/src/registryTable.ts.
#pragma once

#include <concepts>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

#include <flight/runtime.hpp>
#include <flight/types/registry_table.hpp>

static_assert(
    flight::runtime_contract.compiler_contract == "flight-runtime-contract/2",
    "Flight compiler/runtime contract mismatch");
static_assert(
    flight::runtime_contract.cpp_abi == 1,
    "Flight C++ runtime ABI mismatch");

namespace flight::registry {

using flight::types::Kind;
using flight::types::OrdinalTable;
using flight::types::RegistryTable;
using flight::types::registry_entry_state;

template <typename T>
inline std::optional<T> get_ordinal_table_entry(
    std::shared_ptr<OrdinalTable<T>> table,
    double ordinal) {
  if (!flight::is_integer(ordinal) || ordinal < 0.0 ||
      ordinal >= static_cast<double>(table->entries.size())) {
    return std::nullopt;
  }
  return table->entries.element(ordinal);
}

template <typename T>
struct RegistryTableEntryStateView {
  bool bound;
  std::optional<T> value;
};

template <typename T, typename Entry>
inline RegistryTableEntryStateView<T> project_registry_table_entry(const Entry& entry) {
  return std::visit(
      [](const auto& selected) -> RegistryTableEntryStateView<T> {
        if constexpr (requires { selected->value; }) {
          return {
              .bound = selected->state == registry_entry_state->bound,
              .value = selected->value,
          };
        } else {
          return {.bound = false, .value = std::nullopt};
        }
      },
      entry);
}

// This private source helper is the one declaration supplied by the override. The source entry object
// never escapes this module: its two callers observe only bound versus unbound and, for a bound entry,
// the carried T. Keeping those three states in an internal view also accommodates the distinct anonymous
// entry structs the emitter generated for keyed and slot storage without rebuilding either owner.
template <typename T>
inline std::optional<RegistryTableEntryStateView<T>> get_registry_table_entry_state(
    RegistryTable<T> table,
    const Kind& key) {
  using Keyed = std::shared_ptr<flight::types::KeyedTable<T>>;
  using Ordinal = std::shared_ptr<flight::types::OrdinalTable<T>>;
  using Slot = std::shared_ptr<flight::types::SlotTable<T>>;

  return std::visit(
      [&](const auto& selected) -> std::optional<RegistryTableEntryStateView<T>> {
        using Selected = std::remove_cvref_t<decltype(selected)>;
        if constexpr (std::same_as<Selected, Keyed>) {
          const auto entry = selected->entries.get(key);
          if (!entry.has_value()) return std::nullopt;
          return project_registry_table_entry<T>(*entry);
        } else if constexpr (std::same_as<Selected, Slot>) {
          if (key != selected->registry || !selected->entry.has_value()) return std::nullopt;
          return project_registry_table_entry<T>(*selected->entry);
        } else {
          static_assert(std::same_as<Selected, Ordinal>);
          const double ordinal = selected->vocabulary.index_of(key);
          if (ordinal == -1.0) return std::nullopt;
          const auto& value = selected->entries.element(ordinal);
          if (!value.has_value()) return std::nullopt;
          return RegistryTableEntryStateView<T>{.bound = true, .value = value.value()};
        }
      },
      table);
}

template <typename T>
inline std::optional<T> get_registry_table_entry(RegistryTable<T> table, const Kind& key) {
  const auto entry = get_registry_table_entry_state<T>(std::move(table), key);
  if (!entry.has_value() || !entry->bound) return std::nullopt;
  return entry->value;
}

inline bool has_registry_table_entry(RegistryTable<flight::Any> table, const Kind& key) {
  const auto entry = get_registry_table_entry_state<flight::Any>(std::move(table), key);
  return entry.has_value() && entry->bound;
}

} // namespace flight::registry
