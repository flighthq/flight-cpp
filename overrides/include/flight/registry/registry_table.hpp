// Derived from @flighthq/registry/packages/registry/src/registryTable.ts.
#pragma once

#include <concepts>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

#include <flight/entity/entity.hpp>
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
using flight::types::KeyedTable;
using flight::types::OrdinalTable;
using flight::types::RegistryId;
using flight::types::RegistryMissPolicy;
using flight::types::RegistryTable;
using flight::types::RegistryTableEntry;
using flight::types::SlotTable;
using flight::types::registry_entry_state;

template <typename T>
using ReadonlyKeyedTable = flight::StructuralRef<
    flight::RowReadonly<flight::RowOf<flight::Ref<KeyedTable<T>>>>>;

template <typename T>
using ReadonlyOrdinalTable = flight::StructuralRef<
    flight::RowReadonly<flight::RowOf<flight::Ref<OrdinalTable<T>>>>>;

template <typename T>
using ReadonlySlotTable = flight::StructuralRef<
    flight::RowReadonly<flight::RowOf<flight::Ref<SlotTable<T>>>>>;

template <typename T>
using ComposableRegistryTable =
    std::variant<flight::Ref<KeyedTable<T>>, flight::Ref<SlotTable<T>>>;

template <typename T>
inline void initialize_keyed_table(
    flight::types::EntityConstruction<flight::Ref<KeyedTable<T>>> out,
    const RegistryId& registry,
    const RegistryMissPolicy& on_miss) {
  out->entries = flight::Map<flight::String, RegistryTableEntry<T>>{};
  out->on_miss = on_miss;
  out->registry = registry;
  out->shape = flight::String("keyed");
}

template <typename T>
inline flight::Ref<KeyedTable<T>> create_keyed_table(
    const RegistryId& registry,
    const RegistryMissPolicy& on_miss) {
  auto out = flight::entity::allocate_entity<flight::Ref<KeyedTable<T>>>();
  initialize_keyed_table<T>(out, registry, on_miss);
  return flight::entity::finish_entity(out);
}

template <typename T>
inline void initialize_ordinal_table(
    flight::types::EntityConstruction<flight::Ref<OrdinalTable<T>>> out,
    const RegistryId& registry,
    const RegistryMissPolicy& on_miss,
    flight::Array<Kind> vocabulary) {
  flight::Array<std::optional<T>> entries;
  entries.resize(vocabulary.size());
  out->entries = std::move(entries);
  out->on_miss = on_miss;
  out->registry = registry;
  out->shape = flight::String("ordinal");
  out->vocabulary = std::move(vocabulary);
}

template <typename T>
inline flight::Ref<OrdinalTable<T>> create_ordinal_table(
    const RegistryId& registry,
    const RegistryMissPolicy& on_miss,
    flight::Array<Kind> vocabulary) {
  auto out = flight::entity::allocate_entity<flight::Ref<OrdinalTable<T>>>();
  initialize_ordinal_table<T>(out, registry, on_miss, std::move(vocabulary));
  return flight::entity::finish_entity(out);
}

template <typename T>
inline void initialize_slot_table(
    flight::types::EntityConstruction<flight::Ref<SlotTable<T>>> out,
    const RegistryId& registry,
    const RegistryMissPolicy& on_miss) {
  out->entry = std::nullopt;
  out->on_miss = on_miss;
  out->registry = registry;
  out->shape = flight::String("slot");
}

template <typename T>
inline flight::Ref<SlotTable<T>> create_slot_table(
    const RegistryId& registry,
    const RegistryMissPolicy& on_miss) {
  auto out = flight::entity::allocate_entity<flight::Ref<SlotTable<T>>>();
  initialize_slot_table<T>(out, registry, on_miss);
  return flight::entity::finish_entity(out);
}

template <typename T>
inline ComposableRegistryTable<T> concat_registry_table(
    ComposableRegistryTable<T> base,
    ComposableRegistryTable<T> overlay) {
  const auto shape = [](const auto& table) -> const flight::String& {
    return std::visit([](const auto& selected) -> const flight::String& {
      return selected->shape;
    }, table);
  };
  const auto registry = [](const auto& table) -> const RegistryId& {
    return std::visit([](const auto& selected) -> const RegistryId& {
      return selected->registry;
    }, table);
  };
  const auto on_miss = [](const auto& table) -> const RegistryMissPolicy& {
    return std::visit([](const auto& selected) -> const RegistryMissPolicy& {
      return selected->on_miss;
    }, table);
  };

  if (shape(base) != shape(overlay)) {
    throw flight::Error(
        flight::String("concatRegistryTable: cannot compose a '") + shape(base) +
        flight::String("' table with a '") + shape(overlay) + flight::String("' table"));
  }
  if (registry(base) != registry(overlay)) {
    throw flight::Error(
        flight::String("concatRegistryTable: cannot compose registry '") + registry(base) +
        flight::String("' with registry '") + registry(overlay) + flight::String("'"));
  }
  if (on_miss(base) != on_miss(overlay)) {
    throw flight::Error(
        flight::String("concatRegistryTable: cannot compose miss policy '") + on_miss(base) +
        flight::String("' with miss policy '") + on_miss(overlay) + flight::String("'"));
  }

  if (const auto* base_slot = std::get_if<flight::Ref<SlotTable<T>>>(&base)) {
    const auto& overlay_slot = std::get<flight::Ref<SlotTable<T>>>(overlay);
    auto out = flight::entity::allocate_entity<flight::Ref<SlotTable<T>>>();
    out->entry = overlay_slot->entry.has_value() ? overlay_slot->entry : (*base_slot)->entry;
    out->on_miss = (*base_slot)->on_miss;
    out->registry = (*base_slot)->registry;
    out->shape = flight::String("slot");
    return ComposableRegistryTable<T>{
        std::in_place_type<flight::Ref<SlotTable<T>>>,
        flight::entity::finish_entity(out)};
  }

  const auto& base_keyed = std::get<flight::Ref<KeyedTable<T>>>(base);
  const auto& overlay_keyed = std::get<flight::Ref<KeyedTable<T>>>(overlay);
  auto entries = base_keyed->entries.clone();
  for (const auto& [key, entry] : overlay_keyed->entries) entries.set(key, entry);
  auto out = flight::entity::allocate_entity<flight::Ref<KeyedTable<T>>>();
  out->entries = std::move(entries);
  out->on_miss = base_keyed->on_miss;
  out->registry = base_keyed->registry;
  out->shape = flight::String("keyed");
  return ComposableRegistryTable<T>{
      std::in_place_type<flight::Ref<KeyedTable<T>>>,
      flight::entity::finish_entity(out)};
}

template <typename T>
inline flight::Ref<KeyedTable<T>> concat_registry_table(
    std::shared_ptr<KeyedTable<T>> base,
    std::shared_ptr<KeyedTable<T>> overlay) {
  return std::get<flight::Ref<KeyedTable<T>>>(concat_registry_table<T>(
      ComposableRegistryTable<T>{std::in_place_type<flight::Ref<KeyedTable<T>>>,
                                 std::move(base)},
      ComposableRegistryTable<T>{std::in_place_type<flight::Ref<KeyedTable<T>>>,
                                 std::move(overlay)}));
}

template <typename T>
inline flight::Ref<SlotTable<T>> concat_registry_table(
    std::shared_ptr<SlotTable<T>> base,
    std::shared_ptr<SlotTable<T>> overlay) {
  return std::get<flight::Ref<SlotTable<T>>>(concat_registry_table<T>(
      ComposableRegistryTable<T>{std::in_place_type<flight::Ref<SlotTable<T>>>,
                                 std::move(base)},
      ComposableRegistryTable<T>{std::in_place_type<flight::Ref<SlotTable<T>>>,
                                 std::move(overlay)}));
}

template <typename T>
inline ComposableRegistryTable<T> concat_registry_table(
    std::shared_ptr<KeyedTable<T>> base,
    std::shared_ptr<SlotTable<T>> overlay) {
  return concat_registry_table<T>(
      ComposableRegistryTable<T>{std::in_place_type<flight::Ref<KeyedTable<T>>>,
                                 std::move(base)},
      ComposableRegistryTable<T>{std::in_place_type<flight::Ref<SlotTable<T>>>,
                                 std::move(overlay)});
}

template <typename T>
inline ComposableRegistryTable<T> concat_registry_table(
    std::shared_ptr<SlotTable<T>> base,
    std::shared_ptr<KeyedTable<T>> overlay) {
  return concat_registry_table<T>(
      ComposableRegistryTable<T>{std::in_place_type<flight::Ref<SlotTable<T>>>,
                                 std::move(base)},
      ComposableRegistryTable<T>{std::in_place_type<flight::Ref<KeyedTable<T>>>,
                                 std::move(overlay)});
}

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

// The source entry object never escapes this module: its two callers observe only bound versus unbound
// and, for a bound entry, the carried T. Keeping those three states in an internal view also accommodates
// the distinct anonymous entry structs the emitter generated for keyed and slot storage without rebuilding
// either owner.
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
inline std::optional<RegistryTableEntryStateView<T>> get_registry_table_entry_state(
    std::shared_ptr<KeyedTable<T>> table,
    const Kind& key) {
  return get_registry_table_entry_state<T>(
      RegistryTable<T>{std::in_place_type<flight::Ref<KeyedTable<T>>>, std::move(table)}, key);
}

template <typename T>
inline std::optional<RegistryTableEntryStateView<T>> get_registry_table_entry_state(
    std::shared_ptr<SlotTable<T>> table,
    const Kind& key) {
  return get_registry_table_entry_state<T>(
      RegistryTable<T>{std::in_place_type<flight::Ref<SlotTable<T>>>, std::move(table)}, key);
}

template <typename T>
inline std::optional<RegistryTableEntryStateView<T>> get_registry_table_entry_state(
    std::shared_ptr<OrdinalTable<T>> table,
    const Kind& key) {
  return get_registry_table_entry_state<T>(
      RegistryTable<T>{std::in_place_type<flight::Ref<OrdinalTable<T>>>, std::move(table)}, key);
}

template <typename T>
inline std::optional<RegistryTableEntryStateView<T>> get_registry_table_entry_state(
    ReadonlyKeyedTable<T> table,
    const Kind& key) {
  return get_registry_table_entry_state<T>(table.shared_object(), key);
}

template <typename T>
inline std::optional<RegistryTableEntryStateView<T>> get_registry_table_entry_state(
    ReadonlySlotTable<T> table,
    const Kind& key) {
  return get_registry_table_entry_state<T>(table.shared_object(), key);
}

template <typename T>
inline std::optional<RegistryTableEntryStateView<T>> get_registry_table_entry_state(
    ReadonlyOrdinalTable<T> table,
    const Kind& key) {
  return get_registry_table_entry_state<T>(table.shared_object(), key);
}

template <typename T>
inline std::optional<T> get_registry_table_entry(RegistryTable<T> table, const Kind& key) {
  const auto entry = get_registry_table_entry_state<T>(std::move(table), key);
  if (!entry.has_value() || !entry->bound) return std::nullopt;
  return entry->value;
}

template <typename T>
inline std::optional<T> get_registry_table_entry(
    std::shared_ptr<KeyedTable<T>> table,
    const Kind& key) {
  return get_registry_table_entry<T>(
      RegistryTable<T>{std::in_place_type<flight::Ref<KeyedTable<T>>>, std::move(table)}, key);
}

template <typename T>
inline std::optional<T> get_registry_table_entry(
    std::shared_ptr<SlotTable<T>> table,
    const Kind& key) {
  return get_registry_table_entry<T>(
      RegistryTable<T>{std::in_place_type<flight::Ref<SlotTable<T>>>, std::move(table)}, key);
}

template <typename T>
inline std::optional<T> get_registry_table_entry(
    std::shared_ptr<OrdinalTable<T>> table,
    const Kind& key) {
  return get_registry_table_entry<T>(
      RegistryTable<T>{std::in_place_type<flight::Ref<OrdinalTable<T>>>, std::move(table)}, key);
}

template <typename T>
inline std::optional<T> get_registry_table_entry(ReadonlyKeyedTable<T> table, const Kind& key) {
  return get_registry_table_entry<T>(table.shared_object(), key);
}

template <typename T>
inline std::optional<T> get_registry_table_entry(ReadonlySlotTable<T> table, const Kind& key) {
  return get_registry_table_entry<T>(table.shared_object(), key);
}

template <typename T>
inline std::optional<T> get_registry_table_entry(ReadonlyOrdinalTable<T> table, const Kind& key) {
  return get_registry_table_entry<T>(table.shared_object(), key);
}

template <typename T>
inline void get_registry_table_keys(flight::Array<Kind> out, RegistryTable<T> table) {
  out.clear();
  std::visit(
      [&](const auto& selected) {
        using Selected = std::remove_cvref_t<decltype(selected)>;
        if constexpr (std::same_as<Selected, flight::Ref<KeyedTable<T>>>) {
          for (const auto& [key, entry] : selected->entries) {
            const bool bound = std::visit(
                [](const auto& selected_entry) {
                  return selected_entry->state == registry_entry_state->bound;
                },
                entry);
            if (bound) out.push(key);
          }
        } else if constexpr (std::same_as<Selected, flight::Ref<SlotTable<T>>>) {
          if (selected->entry.has_value()) {
            const bool bound = std::visit(
                [](const auto& selected_entry) {
                  return selected_entry->state == registry_entry_state->bound;
                },
                *selected->entry);
            if (bound) out.push(selected->registry);
          }
        } else {
          static_assert(std::same_as<Selected, flight::Ref<OrdinalTable<T>>>);
          for (std::size_t ordinal = 0; ordinal < selected->entries.size(); ++ordinal) {
            if (selected->entries[ordinal].has_value()) out.push(selected->vocabulary[ordinal]);
          }
        }
      },
      table);
  out.sort();
}

template <typename T>
inline void get_registry_table_keys(
    flight::Array<Kind> out,
    std::shared_ptr<KeyedTable<T>> table) {
  get_registry_table_keys<T>(
      std::move(out),
      RegistryTable<T>{std::in_place_type<flight::Ref<KeyedTable<T>>>, std::move(table)});
}

template <typename T>
inline void get_registry_table_keys(
    flight::Array<Kind> out,
    std::shared_ptr<SlotTable<T>> table) {
  get_registry_table_keys<T>(
      std::move(out),
      RegistryTable<T>{std::in_place_type<flight::Ref<SlotTable<T>>>, std::move(table)});
}

template <typename T>
inline void get_registry_table_keys(
    flight::Array<Kind> out,
    std::shared_ptr<OrdinalTable<T>> table) {
  get_registry_table_keys<T>(
      std::move(out),
      RegistryTable<T>{std::in_place_type<flight::Ref<OrdinalTable<T>>>, std::move(table)});
}

template <typename T>
inline bool has_registry_table_entry(RegistryTable<T> table, const Kind& key) {
  const auto entry = get_registry_table_entry_state<T>(std::move(table), key);
  return entry.has_value() && entry->bound;
}

template <typename T>
inline bool has_registry_table_entry(
    std::shared_ptr<KeyedTable<T>> table,
    const Kind& key) {
  return has_registry_table_entry<T>(
      RegistryTable<T>{std::in_place_type<flight::Ref<KeyedTable<T>>>, std::move(table)}, key);
}

template <typename T>
inline bool has_registry_table_entry(
    std::shared_ptr<SlotTable<T>> table,
    const Kind& key) {
  return has_registry_table_entry<T>(
      RegistryTable<T>{std::in_place_type<flight::Ref<SlotTable<T>>>, std::move(table)}, key);
}

template <typename T>
inline bool has_registry_table_entry(
    std::shared_ptr<OrdinalTable<T>> table,
    const Kind& key) {
  return has_registry_table_entry<T>(
      RegistryTable<T>{std::in_place_type<flight::Ref<OrdinalTable<T>>>, std::move(table)}, key);
}

template <typename T>
inline flight::Ref<KeyedTable<T>> without_registry_table_entry(
    std::shared_ptr<KeyedTable<T>> table,
    const Kind& key) {
  auto entries = table->entries.clone();
  entries.erase(key);
  auto out = flight::entity::allocate_entity<flight::Ref<KeyedTable<T>>>();
  out->entries = std::move(entries);
  out->on_miss = table->on_miss;
  out->registry = table->registry;
  out->shape = flight::String("keyed");
  return flight::entity::finish_entity(out);
}

template <typename T>
inline flight::Ref<KeyedTable<T>> without_registry_table_entry(
    ReadonlyKeyedTable<T> table,
    const Kind& key) {
  return without_registry_table_entry<T>(table.shared_object(), key);
}

template <typename T>
inline flight::Ref<KeyedTable<T>> with_registry_table_entry(
    std::shared_ptr<KeyedTable<T>> table,
    const Kind& key,
    T value) {
  auto entries = table->entries.clone();
  using BoundEntry = flight::types::state_value_49c40b4f01b4314b<T>;
  entries.set(
      key,
      RegistryTableEntry<T>{
          std::in_place_type<flight::Ref<BoundEntry>>,
          flight::make_ref<BoundEntry>(BoundEntry{
              .state = registry_entry_state->bound,
              .value = std::move(value),
          })});
  auto out = flight::entity::allocate_entity<flight::Ref<KeyedTable<T>>>();
  out->entries = std::move(entries);
  out->on_miss = table->on_miss;
  out->registry = table->registry;
  out->shape = flight::String("keyed");
  return flight::entity::finish_entity(out);
}

template <typename T>
inline flight::Ref<KeyedTable<T>> with_registry_table_entry(
    ReadonlyKeyedTable<T> table,
    const Kind& key,
    T value) {
  return with_registry_table_entry<T>(table.shared_object(), key, std::move(value));
}

template <typename T>
inline flight::Ref<KeyedTable<T>> with_registry_table_tombstone(
    std::shared_ptr<KeyedTable<T>> table,
    const Kind& key) {
  auto entries = table->entries.clone();
  using TombstoneEntry = flight::types::state_fb2721e0bb344354<T>;
  entries.set(
      key,
      RegistryTableEntry<T>{
          std::in_place_type<flight::Ref<TombstoneEntry>>,
          flight::make_ref<TombstoneEntry>(TombstoneEntry{
              .state = registry_entry_state->tombstoned,
          })});
  auto out = flight::entity::allocate_entity<flight::Ref<KeyedTable<T>>>();
  out->entries = std::move(entries);
  out->on_miss = table->on_miss;
  out->registry = table->registry;
  out->shape = flight::String("keyed");
  return flight::entity::finish_entity(out);
}

template <typename T>
inline flight::Ref<KeyedTable<T>> with_registry_table_tombstone(
    ReadonlyKeyedTable<T> table,
    const Kind& key) {
  return with_registry_table_tombstone<T>(table.shared_object(), key);
}

} // namespace flight::registry
