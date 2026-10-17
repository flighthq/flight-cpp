#include <flight/registry/registry_table.hpp>

#include <iostream>

// Native port of @flighthq/registry's registryTable.test.ts at flight a627849.
//
// createOrdinalTable is refused by the emitter at this pin (`NOT GENERATED` in registry_table.hpp), so
// its two upstream cases ("starts with every ordinal null", "carries the vocabulary through") are not
// ported, and nothing here supplies it. The getOrdinalTableEntry cases build the OrdinalTable value
// the refused function would return -- one null entry per vocabulary Kind -- directly.

namespace {

int failures = 0;

void check(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL " << message << '\n';
    ++failures;
  }
}

using Table = flight::Map<flight::String, flight::String>;

flight::StructuralRef<flight::RowReadonly<flight::RowOf<std::shared_ptr<flight::types::OrdinalTable<flight::String>>>>>
ordinal_table(flight::Array<std::optional<flight::String>> entries, flight::Array<flight::String> vocabulary) {
  return flight::make_ref<flight::types::OrdinalTable<flight::String>>(
      flight::types::OrdinalTable<flight::String>{.entries = entries, .vocabulary = vocabulary});
}

void test_concat_kind_map() {
  using flight::String;
  const Table base = flight::registry::with_kind_map_entry(Table{}, String("a"), String("base"));
  const Table overlay = flight::registry::with_kind_map_entry(Table{}, String("a"), String("overlay"));
  check(flight::registry::concat_kind_map(base, overlay).get(String("a")) == String("overlay"),
        "concatKindMap lets an overlay entry win over the base");

  const Table other = flight::registry::with_kind_map_entry(Table{}, String("b"), String("overlay"));
  const Table composed = flight::registry::concat_kind_map(base, other);
  check(composed.get(String("a")) == String("base") && composed.get(String("b")) == String("overlay"),
        "concatKindMap preserves base entries that the overlay does not mention");
  // `new Map(base)` copies: the composed map is a new object, and neither input gains the other's keys.
  check(!base.has(String("b")) && base.size() == 1 && other.size() == 1,
        "concatKindMap leaves both inputs unchanged");
}

void test_get_kind_map_keys() {
  using flight::String;
  const Table table = flight::registry::with_kind_map_entry(
      flight::registry::with_kind_map_entry(Table{}, String("b"), String("1")), String("a"), String("2"));
  flight::Array<String> out;
  flight::registry::get_kind_map_keys(out, table);
  check(out.size() == 2 && out[0] == String("a") && out[1] == String("b"), "getKindMapKeys lists all keys sorted");

  flight::Array<String> stale{String("stale")};
  flight::registry::get_kind_map_keys(stale, flight::Map<String, flight::Any>{});
  check(stale.empty(), "getKindMapKeys clears `out` first, so a reused array does not accumulate");
}

void test_get_ordinal_table_entry() {
  using flight::String;
  const auto one = ordinal_table({std::nullopt}, {String("Tag0")});
  check(!flight::registry::get_ordinal_table_entry(one, 1.0).has_value(), "getOrdinalTableEntry: 1 is out of range");
  check(!flight::registry::get_ordinal_table_entry(one, -1.0).has_value(), "getOrdinalTableEntry: -1 is out of range");
  check(!flight::registry::get_ordinal_table_entry(one, 1.5).has_value(), "getOrdinalTableEntry: 1.5 is not an ordinal");

  const auto bound = ordinal_table({String("zero"), std::nullopt}, {String("Tag0"), String("Tag1")});
  check(flight::registry::get_ordinal_table_entry(bound, 0.0) == String("zero"),
        "getOrdinalTableEntry indexes directly when the ordinal is in range");
  check(!flight::registry::get_ordinal_table_entry(bound, 1.0).has_value(),
        "getOrdinalTableEntry returns null for an in-range unbound ordinal");
  // A fractional ordinal inside the range is still not an ordinal (Number.isInteger guards before the read).
  check(!flight::registry::get_ordinal_table_entry(bound, 0.5).has_value(),
        "getOrdinalTableEntry refuses a fractional in-range ordinal");
}

void test_with_kind_map_entry() {
  using flight::String;
  const Table before;
  const Table after = flight::registry::with_kind_map_entry(before, String("a"), String("v"));
  check(before.size() == 0, "withKindMapEntry mutates nothing");
  check(after.get(String("a")) == String("v"), "withKindMapEntry returns the replacement with the entry");
  // `expect(after).not.toBe(before)`: a different Map object. Writing through one must not show in the other.
  after.set(String("z"), String("late"));
  check(!before.has(String("z")), "withKindMapEntry returns a different Map object, not the one it was given");

  const Table table = flight::registry::with_kind_map_entry(
      flight::registry::with_kind_map_entry(Table{}, String("a"), String("1")), String("a"), String("2"));
  check(table.get(String("a")) == String("2") && table.size() == 1, "withKindMapEntry is last-write-wins for a key");
}

void test_without_kind_map_entry() {
  using flight::String;
  const Table table = flight::registry::without_kind_map_entry(
      flight::registry::with_kind_map_entry(Table{}, String("a"), String("v")), String("a"));
  check(!table.has(String("a")), "withoutKindMapEntry removes the key from the result");

  const Table before = flight::registry::with_kind_map_entry(Table{}, String("a"), String("v"));
  (void)flight::registry::without_kind_map_entry(before, String("a"));
  check(before.get(String("a")) == String("v"), "withoutKindMapEntry does not mutate the table it was given");
}

} // namespace

int main() {
  test_concat_kind_map();
  test_get_kind_map_keys();
  test_get_ordinal_table_entry();
  test_with_kind_map_entry();
  test_without_kind_map_entry();
  return failures == 0 ? 0 : 1;
}
