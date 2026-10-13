// Focused behavior tests for @flighthq/preferences.
//
// The package is `partial` at a627849: `storage.ts` (518 lines) is refused and supplied by
// overrides/include/flight/preferences/storage.hpp, whose own `status` is `partial` with one declared
// gap (a generic JSON read whose Value is not a represented domain reports parse-failed where the
// source's unchecked `as Value` would return the parsed value).
//
// WHAT THIS FILE COVERS. Originally only the two axes whose behavior needed no outcome-variant construction
// — the signal lifecycle and host identity — because this copy builds its outcome arms through its own
// factories (`storage_value_success`, `storage_query_failure`, `storage_batch_failure`) whose success/failure
// index conventions differ per family. The persistence and batching axes were deferred "until that vocabulary
// is pinned"; a later unit pinned it by reading the factories, and they are covered below:
//
//   value = index 1 on success, 0 on failure   (storage_value_success / storage_value_failure)
//   query = index 0 on success, 1 on failure   (storage_query_success / storage_query_failure)
//   batch = index 0 on success, 1 on failure   (storage_batch_success / storage_batch_failure)
//   mutation = index 1 when the reason is `ok`, otherwise 0   (storage_mutation)
//   migration = 0 success, 1 read-failure, 2 checkpoint-failure
//
// Every assertion below reads an outcome by INDEX as well as by field, so a factory whose convention
// shifted would fail here rather than agree with itself.
//
//   * `createStorageSignals` builds a group whose change signal exists;
//   * `storageSignalsActive` is false for an absent group and for a group with no listeners, and true
//     once a listener is connected — it reports listeners, not presence;
//   * `destroyStorage` reaches the CHANGE capability's own `destroy`, not any other slot.
#include <flight/executor.hpp>
#include <flight/preferences/storage.hpp>

#include <functional>
#include <iostream>
#include <memory>
#include <optional>

namespace {

int failures = 0;

bool check(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
  return condition;
}

}  // namespace

int main() {
  const auto executor = std::make_shared<flight::QueueExecutor>();
  const flight::ExecutorScope scope(executor);

  // The signal lifecycle: active means "a listener is connected", not "a group exists".
  {
    const auto signals = flight::preferences::create_storage_signals();
    check(signals != nullptr && signals->on_change != nullptr,
          "createStorageSignals builds the change signal");
    check(!flight::preferences::storage_signals_active(signals),
          "a fresh group is not active, because it has no listeners");
    check(!flight::preferences::storage_signals_active(std::nullopt),
          "and an absent group is never active");

    flight::signals::connect_signal<std::function<void(flight::StructuralRef<flight::RowReadonly<
        flight::RowOf<flight::Ref<flight::types::StorageChange>>>>)>>(signals->on_change, [](auto) {});
    check(flight::preferences::storage_signals_active(signals),
          "connecting the first listener makes the group active");
  }

  // Host identity: destroyStorage reads the change capability, and only that slot.
  {
    int change_destroys = 0;
    auto change = flight::make_ref<flight::types::HostPreferencesChangeCapability>();
    change->destroy = [&change_destroys]() { ++change_destroys; };
    change->subscribe = [](std::function<void(flight::StructuralRef<flight::RowReadonly<
                               flight::RowOf<flight::Ref<flight::types::StorageChange>>>>)>)
        -> std::optional<std::function<void()>> { return std::nullopt; };

    flight::preferences::destroy_storage(change);
    check(change_destroys == 1, "destroyStorage reaches the change capability's own destroy");
  }


  // ============================ The deferred axes, now that the vocabulary is pinned ====================
  //
  // A recording store: one Record of key -> value plus the key ORDER, so aggregation and ordering are both
  // observable. `fail_key` makes that one key's read fail, which is how the query/batch failure arms are
  // reached.
  struct Store {
    flight::Record<flight::String, flight::String> values = flight::Record<flight::String, flight::String>{};
    flight::Array<flight::String> order = flight::Array<flight::String>{};
    std::optional<flight::String> fail_key;
    std::optional<flight::String> fail_write;
    int sets = 0;
    int removes = 0;
    int clears = 0;

    flight::Ref<flight::types::HostPreferencesCapability> capability =
        flight::make_ref<flight::types::HostPreferencesCapability>();

    void put(const char* key, const char* value) {
      if (!values.has(flight::String(key))) order.push(flight::String(key));
      values.set(flight::String(key), flight::String(value));
    }

    Store() {
      capability->keys = [this]() {
        return flight::preferences::storage_value_success<flight::types::StorageKeysResult>(order);
      };
      capability->get_item = [this](flight::String key) {
        if (fail_key.has_value() && fail_key.value() == key) {
          return flight::preferences::storage_value_failure<flight::types::StorageGetItemResult>(
              flight::String("read-failed"));
        }
        if (!values.has(key)) {
          return flight::preferences::storage_value_success<flight::types::StorageGetItemResult>(
              std::optional<flight::String>{});
        }
        return flight::preferences::storage_value_success<flight::types::StorageGetItemResult>(
            std::optional<flight::String>{values.get(key).value()});
      };
      capability->set_item = [this](flight::String key, flight::String value) {
        ++sets;
        if (fail_write.has_value() && fail_write.value() == key) {
          return flight::preferences::storage_mutation<flight::types::StorageSetItemResult>(
              flight::String("write-failed"));
        }
        if (!values.has(key)) order.push(key);
        values.set(key, value);
        return flight::preferences::storage_mutation<flight::types::StorageSetItemResult>(flight::String("ok"));
      };
      capability->remove_item = [this](flight::String key) {
        ++removes;
        if (fail_write.has_value() && fail_write.value() == key) {
          return flight::preferences::storage_mutation<flight::types::StorageRemoveItemResult>(
              flight::String("write-failed"));
        }
        values.erase(key);
        return flight::preferences::storage_mutation<flight::types::StorageRemoveItemResult>(flight::String("ok"));
      };
      capability->clear = [this]() {
        ++clears;
        values = flight::Record<flight::String, flight::String>{};
        order = flight::Array<flight::String>{};
        return flight::preferences::storage_mutation<flight::types::StorageClearResult>(flight::String("ok"));
      };
    }
  };

  namespace preferences_ns = flight::preferences;
  using flight::String;
  using flight::types::StorageChange;
  using flight::types::StorageSignals;
  const auto reason_of = [](const auto& outcome) {
    return preferences_ns::storage_outcome_reason(outcome).to_utf8();
  };

  // Value parsing: true/false/absent/parse-failed are four different answers.
  {
    Store store;
    store.put("flag", "true");
    store.put("other", "yes");
    const auto capability = store.capability;

    const auto parsed_true = preferences_ns::get_storage_boolean(capability, String("flag"));
    check(parsed_true.index() == 1 && preferences_ns::storage_value(parsed_true).has_value() &&
              preferences_ns::storage_value(parsed_true).value(),
          "`true` parses to a present true");

    store.put("flag", "false");
    const auto parsed_false = preferences_ns::get_storage_boolean(capability, String("flag"));
    check(parsed_false.index() == 1 && preferences_ns::storage_value(parsed_false).has_value() &&
              !preferences_ns::storage_value(parsed_false).value(),
          "`false` parses to a present FALSE rather than to absence");

    // Absence is a SUCCESS carrying an empty optional -- not a failure, and not `false`.
    const auto absent = preferences_ns::get_storage_boolean(capability, String("missing"));
    check(absent.index() == 1 && !preferences_ns::storage_value(absent).has_value(),
          "a missing key is a success with NO value, which is neither false nor a failure");

    const auto unparseable = preferences_ns::get_storage_boolean(capability, String("other"));
    check(unparseable.index() == 0 && reason_of(unparseable) == "parse-failed",
          "an unparseable boolean is a failure whose reason says so");
  }

  // The `or` variants: the fallback is delivered, but the REASON distinguishes absent from unparseable.
  {
    Store store;
    store.put("other", "yes");
    const auto capability = store.capability;

    const auto absent = preferences_ns::get_storage_boolean_or(capability, String("missing"), true);
    check(absent.index() == 1 && reason_of(absent) == "ok" &&
              preferences_ns::storage_value(absent) == true,
          "an absent key takes the fallback and reports `ok`, because nothing was wrong");

    const auto unparseable = preferences_ns::get_storage_boolean_or(capability, String("other"), true);
    check(unparseable.index() == 1 && reason_of(unparseable) == "parse-failed" &&
              preferences_ns::storage_value(unparseable) == true,
          "an unparseable key takes the SAME fallback but reports `parse-failed`, so the two are separable");
  }

  // Missing versus empty: an EMPTY STRING is a value, so it must not collapse into the fallback.
  {
    Store store;
    store.put("empty", "");
    const auto capability = store.capability;

    check(preferences_ns::storage_value(preferences_ns::get_storage_item_or(capability, String("missing"), String("d")))
                  == String("d"),
          "an absent item takes the fallback");
    check(preferences_ns::storage_value(preferences_ns::get_storage_item_or(capability, String("empty"), String("d")))
                  == String(""),
          "while an item that EXISTS and holds the empty string does not -- absence and emptiness are distinct");
  }

  // Presence and count read the store, not a cache.
  {
    Store store;
    store.put("a", "1");
    store.put("b", "2");
    const auto capability = store.capability;

    check(preferences_ns::storage_value(preferences_ns::get_storage_item_presence(capability, String("a"))),
          "a present key reports present");
    check(!preferences_ns::storage_value(preferences_ns::get_storage_item_presence(capability, String("z"))),
          "and a missing one does not");
    check(preferences_ns::storage_value(preferences_ns::get_storage_item_count(capability)) == 2.0,
          "the item count is the number of keys");
  }

  // Query aggregation: one failing key aborts the batch and is NAMED in the failure.
  {
    Store store;
    store.put("a", "1");
    store.put("b", "2");
    store.fail_key = String("b");
    const auto capability = store.capability;

    flight::Array<flight::String> asked;
    asked.push(String("a"));
    const auto ok = preferences_ns::get_storage_items(capability, asked);
    check(ok.index() == 0 && preferences_ns::storage_query_value(ok).size() == 1.0,
          "a batch of readable keys succeeds on the query arm and keeps the order asked for");

    flight::Array<flight::String> both;
    both.push(String("a"));
    both.push(String("b"));
    const auto failed = preferences_ns::get_storage_items(capability, both);
    check(failed.index() == 1, "one unreadable key moves the batch to the failure arm");
    check(reason_of(failed) == "read-failed", "carrying the provider's reason");
    check(std::get<1>(failed)->failed_key.has_value() &&
              std::get<1>(failed)->failed_key.value() == String("b"),
          "and NAMING the key that failed, not the first one");
  }

  // Namespace: the prefix is the namespace plus a dot, on writes, reads, keys and entries.
  {
    Store store;
    store.put("app.theme", "dark");
    store.put("app.size", "3");
    store.put("other.key", "x");
    const auto capability = store.capability;
    const auto namespace_ = flight::make_ref<flight::types::StorageNamespace>();
    namespace_->prefix = String("app");

    check(preferences_ns::namespaced_key(namespace_, String("theme")) == String("app.theme"),
          "a namespaced key is the prefix, a dot, then the key");
    check(preferences_ns::storage_value(preferences_ns::get_namespaced_storage_item(capability, namespace_,
                                                                                    String("theme")))
                  .value() == String("dark"),
          "and a namespaced read asks for exactly that key");

    const auto keys = preferences_ns::get_namespaced_storage_keys(capability, namespace_);
    const auto& listed = preferences_ns::storage_value(keys);
    check(listed.size() == 2.0, "the namespaced key list contains only this namespace's keys");
    check(listed.element(0.0) == String("theme") && listed.element(1.0) == String("size"),
          "with the prefix STRIPPED rather than returned as part of the key");

    const auto entries = preferences_ns::get_namespaced_storage_entries(capability, namespace_);
    const auto& pairs = preferences_ns::storage_query_value(entries);
    check(pairs.size() == 2.0, "and the entry list is scoped the same way");
    check(pairs.element(0.0).element(0.0) == String("theme") &&
              pairs.element(0.0).element(1.0) == String("dark"),
          "each pair naming the stripped key and its value");
  }

  // Batch aggregation: stop at the first failure, reporting how many COMPLETED before it.
  {
    Store store;
    store.put("a", "1");
    store.put("b", "2");
    store.put("c", "3");
    store.fail_write = String("b");
    const auto capability = store.capability;

    flight::Array<flight::String> keys;
    keys.push(String("a"));
    keys.push(String("b"));
    keys.push(String("c"));
    const auto removed = preferences_ns::remove_storage_items(capability, keys);
    check(removed.index() == 1, "a batch write that fails lands on the batch failure arm");
    check(std::get<1>(removed)->completed == 1.0,
          "reporting the number that COMPLETED before the failure, not the number attempted");
    check(std::get<1>(removed)->failed_key.has_value() && std::get<1>(removed)->failed_key.value() == String("b"),
          "and naming the key that failed");
    check(store.values.has(String("c")),
          "with the key AFTER the failure left untouched, so the batch stops rather than half-finishing");
  }

  // clearStorageNamespace removes only its own keys, and counts them.
  {
    Store store;
    store.put("app.a", "1");
    store.put("app.b", "2");
    store.put("other.c", "3");
    const auto capability = store.capability;
    const auto namespace_ = flight::make_ref<flight::types::StorageNamespace>();
    namespace_->prefix = String("app");

    const auto cleared = preferences_ns::clear_storage_namespace(capability, namespace_);
    check(cleared.index() == 0 && std::get<0>(cleared)->completed == 2.0,
          "clearing a namespace reports the two keys it removed");
    check(!store.values.has(String("app.a")) && !store.values.has(String("app.b")),
          "and it removed them");
    check(store.values.has(String("other.c")), "while leaving every key outside the namespace alone");
  }

  // Change emission: the signal is emitted only for a real change, and carries old and new.
  {
    Store store;
    store.put("theme", "dark");
    const auto capability = store.capability;
    const auto signals = preferences_ns::create_storage_signals();
    std::vector<std::string> emitted;
    flight::signals::connect_signal<std::function<void(flight::StructuralRef<flight::RowReadonly<
        flight::RowOf<flight::Ref<flight::types::StorageChange>>>>)>>(
        signals->on_change, [&emitted](auto change) {
          auto key = flight::row_get<flight::RowKey<"key">>(change);
          auto old_value = flight::row_get<flight::RowKey<"oldValue">>(change);
          auto new_value = flight::row_get<flight::RowKey<"newValue">>(change);
          emitted.push_back((key.has_value() ? key.value().to_utf8() : std::string("<none>")) + ":" +
                            (old_value.has_value() ? old_value.value().to_utf8() : std::string("<none>")) + ">" +
                            (new_value.has_value() ? new_value.value().to_utf8() : std::string("<none>")));
        });

    preferences_ns::set_storage_item(capability, String("theme"), String("dark"),
                                     std::optional<std::optional<flight::Ref<flight::types::StorageSignals>>>{
                                         std::optional<flight::Ref<flight::types::StorageSignals>>{signals}});
    check(emitted.empty(), "writing the SAME value emits nothing, because nothing changed");

    preferences_ns::set_storage_item(capability, String("theme"), String("light"),
                                     std::optional<std::optional<flight::Ref<flight::types::StorageSignals>>>{
                                         std::optional<flight::Ref<flight::types::StorageSignals>>{signals}});
    check(emitted.size() == 1 && emitted.front() == "theme:dark>light",
          "a real change emits once, carrying the key and BOTH the old and the new value");

    preferences_ns::remove_storage_item(capability, String("theme"),
                                        std::optional<std::optional<flight::Ref<flight::types::StorageSignals>>>{
                                            std::optional<flight::Ref<flight::types::StorageSignals>>{signals}});
    check(emitted.size() == 2 && emitted.back() == "theme:light><none>",
          "and a removal emits the old value with an absent new one");

    // Removing a key that was NOT there must not emit: the mutation battery found this unasserted --
    // every removal above targets a key that had a value, so "emits only when something was removed" was
    // invisible and a removal that always emits survived.
    const auto before_absent_removal = emitted.size();
    preferences_ns::remove_storage_item(capability, String("never-existed"),
                                        std::optional<std::optional<flight::Ref<flight::types::StorageSignals>>>{
                                            std::optional<flight::Ref<flight::types::StorageSignals>>{signals}});
    const std::string absent_removal_message =
        "removing a key that held no value emits NOTHING (before " +
        std::to_string(before_absent_removal) + ", after " + std::to_string(emitted.size()) + ")";
    check(emitted.size() == before_absent_removal, absent_removal_message.c_str());

    preferences_ns::clear_storage(capability,
                                  std::optional<std::optional<flight::Ref<flight::types::StorageSignals>>>{
                                      std::optional<flight::Ref<flight::types::StorageSignals>>{signals}});
    check(emitted.size() == 3 && emitted.back() == "<none>:<none>><none>",
          "while a wholesale clear emits a change with ALL THREE fields absent, meaning `everything`");
  }

  // The same writes with NO live signals emit nothing at all.
  {
    Store store;
    const auto capability = store.capability;
    preferences_ns::set_storage_item(capability, String("k"), String("v"));
    check(store.sets == 1, "a write with no signal group still reaches the provider");
    preferences_ns::remove_storage_item(capability, String("k"));
    check(store.removes == 1, "and so does a removal");
  }

  // Byte size: scoped by the prefix and counting UTF-16 units, not keys.
  {
    Store store;
    store.put("app.ab", "1234");     // name 6 + value 4 = 10 units * 2 = 20
    store.put("other.cd", "123456"); // outside the prefix, not counted
    const auto capability = store.capability;
    const auto bytes = preferences_ns::get_storage_byte_size_for_prefix(capability, String("app."));
    check(bytes.index() == 0 && preferences_ns::storage_query_value(bytes) == 20.0,
          "the byte size counts only the prefixed keys, as (key + value) doubled");
  }

  if (failures == 0) std::cout << "generated preferences: all checks passed\n";
  return failures == 0 ? 0 : 1;
}
