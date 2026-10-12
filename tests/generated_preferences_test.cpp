// Focused behavior tests for @flighthq/preferences.
//
// The package is `partial` at a627849: `storage.ts` (518 lines) is refused and supplied by
// overrides/include/flight/preferences/storage.hpp, whose own `status` is `partial` with one declared
// gap (a generic JSON read whose Value is not a represented domain reports parse-failed where the
// source's unchecked `as Value` would return the parsed value).
//
// WHAT THIS FILE COVERS, and why only this much: the two axes whose behavior needs no outcome-variant
// construction — the signal lifecycle and host identity — because this copy builds its outcome arms
// through its own factories (`storage_value_success`, `storage_query_failure`, `storage_batch_failure`)
// whose success/failure index conventions differ per family. A test that guessed one would assert the
// wrong thing, so the persistence and batching axes (round trips, the namespace prefix filter, the
// completed/failed-key aggregation) are left to the module's own 666-line upstream suite, which is their
// specification, until that vocabulary is pinned.
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

  if (failures == 0) std::cout << "generated preferences: all checks passed\n";
  return failures == 0 ? 0 : 1;
}
