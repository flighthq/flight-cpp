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

  const auto created = flight::registry::create_keyed_table<String>(
      String("created"), String("fallback"));
  if (!check(created->entries.empty() && created->registry == String("created") &&
                 created->on_miss == String("fallback") &&
                 created->shape == String("keyed"),
             "keyed table construction did not initialize its public fields") ||
      !check(flight::attached_properties(created).has(flight::types::entity_runtime_key),
             "keyed table construction did not retain entity identity storage")) {
    return 1;
  }

  const auto with_alpha = flight::registry::with_registry_table_entry<String>(
      created, alpha, String("value"));
  const auto tombstoned = flight::registry::with_registry_table_tombstone<String>(
      with_alpha, String("removed"));
  const auto without_alpha = flight::registry::without_registry_table_entry<String>(
      with_alpha, alpha);
  const flight::registry::ReadonlyKeyedTable<String> readonly_with_alpha(with_alpha);
  const auto from_readonly = flight::registry::with_registry_table_entry<String>(
      readonly_with_alpha, String("readonly"), String("projection"));
  if (!check(created->entries.empty(),
             "persistent registry update mutated the source table") ||
      !check(flight::registry::get_registry_table_entry(with_alpha, alpha) ==
                 String("value"),
             "persistent registry update did not bind its value") ||
      !check(tombstoned->entries.has(String("removed")) &&
                 !flight::registry::get_registry_table_entry(
                      tombstoned, String("removed")).has_value(),
             "registry tombstone was removed or escaped as a value") ||
      !check(!without_alpha->entries.has(alpha) && with_alpha->entries.has(alpha),
             "without-entry did not distinguish absence from persistent source storage") ||
      !check(flight::registry::get_registry_table_entry(
                 from_readonly, String("readonly")) == String("projection"),
             "readonly keyed-table projection did not retain its concrete owner")) {
    return 1;
  }

  const auto overlay = flight::registry::with_registry_table_entry<String>(
      flight::registry::create_keyed_table<String>(String("created"), String("fallback")),
      alpha,
      String("overlay"));
  const auto composed = flight::registry::concat_registry_table<String>(with_alpha, overlay);
  const auto no_opinion = flight::registry::without_registry_table_entry<String>(overlay, alpha);
  const auto inherited = flight::registry::concat_registry_table<String>(with_alpha, no_opinion);
  const auto omitted = flight::registry::concat_registry_table<String>(
      with_alpha,
      flight::registry::with_registry_table_tombstone<String>(no_opinion, alpha));
  if (!check(flight::registry::get_registry_table_entry(composed, alpha) == String("overlay"),
             "keyed overlay did not win during composition") ||
      !check(flight::registry::get_registry_table_entry(inherited, alpha) == String("value"),
             "absent overlay opinion did not inherit the base binding") ||
      !check(!flight::registry::get_registry_table_entry(omitted, alpha).has_value() &&
                 omitted->entries.has(alpha),
             "overlay tombstone did not suppress the base binding")) {
    return 1;
  }

  flight::Array<String> keys{String("stale")};
  const auto enumerable = flight::registry::with_registry_table_entry<String>(
      flight::registry::with_registry_table_entry<String>(tombstoned, String("beta"), String("b")),
      String("aardvark"),
      String("a"));
  flight::registry::get_registry_table_keys(keys, enumerable);
  if (!check(keys.size() == 3 && keys[0] == String("aardvark") &&
                 keys[1] == alpha && keys[2] == String("beta"),
             "registry key enumeration did not clear, filter tombstones, and sort")) {
    return 1;
  }

  const auto created_ordinal = flight::registry::create_ordinal_table<String>(
      String("ordinals"),
      String("none"),
      flight::Array<String>{String("zero"), String("one")});
  if (!check(created_ordinal->entries.size() == 2 &&
                 !created_ordinal->entries[0].has_value() &&
                 !created_ordinal->entries[1].has_value(),
             "ordinal construction did not create one empty entry per vocabulary key")) {
    return 1;
  }
  created_ordinal->entries[1] = String("bound");
  if (!check(!flight::registry::get_ordinal_table_entry(created_ordinal, -1.0).has_value() &&
                 !flight::registry::get_ordinal_table_entry(created_ordinal, 0.5).has_value() &&
                 flight::registry::get_ordinal_table_entry(created_ordinal, 1.0) ==
                     String("bound"),
             "ordinal lookup did not preserve direct-index bounds semantics")) {
    return 1;
  }

  const auto created_slot = flight::registry::create_slot_table<String>(
      String("slot"), String("none"));
  const auto base_slot = flight::registry::create_slot_table<String>(
      String("slot"), String("none"));
  base_slot->entry =
      flight::make_ref<flight::types::state_value_835a3ead37753719<String>>(
          flight::types::state_value_835a3ead37753719<String>{
              .state = flight::types::registry_entry_state->bound,
              .value = String("base"),
          });
  const auto inherited_slot =
      flight::registry::concat_registry_table<String>(base_slot, created_slot);
  const auto omitted_slot_overlay = flight::registry::create_slot_table<String>(
      String("slot"), String("none"));
  omitted_slot_overlay->entry =
      flight::make_ref<flight::types::state_3c8aa9fbfec5b0c7<String>>(
          flight::types::state_3c8aa9fbfec5b0c7<String>{
              .state = flight::types::registry_entry_state->tombstoned,
          });
  const auto omitted_slot =
      flight::registry::concat_registry_table<String>(base_slot, omitted_slot_overlay);
  if (!check(flight::registry::get_registry_table_entry(inherited_slot, String("slot")) ==
                 String("base"),
             "empty slot overlay did not inherit the base opinion") ||
      !check(!flight::registry::get_registry_table_entry(
                  omitted_slot, String("slot")).has_value(),
             "slot tombstone did not override the base opinion")) {
    return 1;
  }

  bool shape_mismatch = false;
  bool registry_mismatch = false;
  bool policy_mismatch = false;
  try {
    static_cast<void>(flight::registry::concat_registry_table<String>(created, created_slot));
  } catch (const flight::Error& error) {
    shape_mismatch =
        error.message() ==
        String("concatRegistryTable: cannot compose a 'keyed' table with a 'slot' table");
  }
  try {
    static_cast<void>(flight::registry::concat_registry_table<String>(
        created,
        flight::registry::create_keyed_table<String>(String("other"), String("fallback"))));
  } catch (const flight::Error& error) {
    registry_mismatch =
        error.message() ==
        String("concatRegistryTable: cannot compose registry 'created' with registry 'other'");
  }
  try {
    static_cast<void>(flight::registry::concat_registry_table<String>(
        created,
        flight::registry::create_keyed_table<String>(String("created"), String("none"))));
  } catch (const flight::Error& error) {
    policy_mismatch =
        error.message() ==
        String("concatRegistryTable: cannot compose miss policy 'fallback' with miss policy 'none'");
  }
  if (!check(shape_mismatch && registry_mismatch && policy_mismatch,
             "registry composition did not reject every incompatible table contract")) {
    return 1;
  }

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
