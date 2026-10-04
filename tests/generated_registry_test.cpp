#include <flight/registry/registry_table.hpp>

#include <iostream>

namespace {

bool check(bool condition, const char* message) {
  if (!condition) std::cerr << message << '\n';
  return condition;
}

} // namespace

int main() {
  using flight::String;
  using flight::types::KeyedTable;
  using flight::types::OrdinalTable;
  using flight::types::RegistryTable;
  using flight::types::SlotTable;

  const String alpha("alpha");
  const String missing("missing");
  const auto keyed_bound = flight::make_ref<flight::types::state_value_49c40b4f01b4314b<double>>(
      flight::types::state_value_49c40b4f01b4314b<double>{
          .state = flight::types::registry_entry_state->bound,
          .value = 7.0,
      });
  const auto keyed_tombstone = flight::make_ref<flight::types::state_fb2721e0bb344354<double>>(
      flight::types::state_fb2721e0bb344354<double>{
          .state = flight::types::registry_entry_state->tombstoned,
      });
  const auto keyed = flight::make_ref<KeyedTable<double>>(KeyedTable<double>{
      .registry = String("numbers"),
      .entries = {{alpha, keyed_bound}, {String("removed"), keyed_tombstone}},
      .shape = String("keyed"),
  });
  const RegistryTable<double> keyed_table = keyed;
  const auto keyed_value = flight::registry::get_registry_table_entry(keyed_table, alpha);
  if (!check(keyed_value == 7.0, "keyed binding did not resolve") ||
      !check(!flight::registry::get_registry_table_entry(keyed_table, String("removed")).has_value(),
             "keyed tombstone escaped as a binding") ||
      !check(!flight::registry::get_registry_table_entry(keyed_table, missing).has_value(),
             "missing keyed entry resolved")) {
    return 1;
  }

  const auto slot_bound = flight::make_ref<flight::types::state_value_835a3ead37753719<double>>(
      flight::types::state_value_835a3ead37753719<double>{
          .state = flight::types::registry_entry_state->bound,
          .value = 11.0,
      });
  const auto slot = flight::make_ref<SlotTable<double>>(SlotTable<double>{
      .registry = String("singleton"),
      .entry = slot_bound,
      .shape = String("slot"),
  });
  const RegistryTable<double> slot_table = slot;
  if (!check(flight::registry::get_registry_table_entry(slot_table, String("singleton")) == 11.0,
             "matching slot did not resolve") ||
      !check(!flight::registry::get_registry_table_entry(slot_table, alpha).has_value(),
             "slot resolved for a different registry key")) {
    return 1;
  }
  slot->entry = flight::make_ref<flight::types::state_3c8aa9fbfec5b0c7<double>>(
      flight::types::state_3c8aa9fbfec5b0c7<double>{
          .state = flight::types::registry_entry_state->tombstoned,
      });
  if (!check(!flight::registry::get_registry_table_entry(slot_table, String("singleton")).has_value(),
             "slot tombstone escaped as a binding")) {
    return 1;
  }

  const auto ordinal = flight::make_ref<OrdinalTable<double>>(OrdinalTable<double>{
      .registry = String("ordinals"),
      .entries = {3.0, std::nullopt, 5.0},
      .shape = String("ordinal"),
      .vocabulary = {String("first"), String("empty"), String("last")},
  });
  const RegistryTable<double> ordinal_table = ordinal;
  if (!check(flight::registry::get_registry_table_entry(ordinal_table, String("last")) == 5.0,
             "ordinal vocabulary lookup did not resolve") ||
      !check(!flight::registry::get_registry_table_entry(ordinal_table, String("empty")).has_value(),
             "empty ordinal resolved") ||
      !check(!flight::registry::get_registry_table_entry(ordinal_table, missing).has_value(),
             "unknown ordinal key resolved")) {
    return 1;
  }

  const auto any_bound = flight::make_ref<flight::types::state_value_49c40b4f01b4314b<flight::Any>>(
      flight::types::state_value_49c40b4f01b4314b<flight::Any>{
          .state = flight::types::registry_entry_state->bound,
          .value = flight::Any(17.0),
      });
  const auto any_tombstone = flight::make_ref<flight::types::state_fb2721e0bb344354<flight::Any>>(
      flight::types::state_fb2721e0bb344354<flight::Any>{
          .state = flight::types::registry_entry_state->tombstoned,
      });
  const RegistryTable<flight::Any> any_table = flight::make_ref<KeyedTable<flight::Any>>(
      KeyedTable<flight::Any>{
          .registry = String("anything"),
          .entries = {{alpha, any_bound}, {String("removed"), any_tombstone}},
          .shape = String("keyed"),
      });
  if (!check(flight::registry::has_registry_table_entry(any_table, alpha),
             "has rejected a bound entry") ||
      !check(!flight::registry::has_registry_table_entry(any_table, String("removed")),
             "has accepted a tombstone") ||
      !check(!flight::registry::has_registry_table_entry(any_table, missing),
             "has accepted a missing entry")) {
    return 1;
  }

  return 0;
}
