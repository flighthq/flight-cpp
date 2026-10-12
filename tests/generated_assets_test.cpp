#include <flight/assets/contract.hpp>

#include <iostream>
#include <string>

// acquireAsset is hand-written by the assets-reject-task-template-parameter override; the rest of
// asset_library.hpp is the generated header. This exercises the two together.

namespace {

using namespace flight;

int failures = 0;

void check(bool condition, const char* message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

bool rejects(Task<Any> task) {
  try {
    task.get();
    return false;
  } catch (...) {
    return true;
  }
}

}  // namespace
int main() {
  auto library = assets::create_asset_library();
  check(rejects(assets::acquire_asset(library, String("missing"))), "an unknown id rejects");
  auto descriptor = make_ref<types::AssetDescriptor>(types::AssetDescriptor{.id = String("a"), .url = String("u"), .type = String("t"), .groups = std::nullopt});
  assets::register_asset_descriptor(library, descriptor);
  check(rejects(assets::acquire_asset(library, String("a"))), "an id with no loader for its type rejects");
  int loads = 0;
  bool fail_next = false;
  auto adapter = make_ref<types::AssetLoaderAdapter<>>();
  adapter->load = [&](auto) -> Task<Any> {
    ++loads;
    if (fail_next) return Task<Any>::reject(std::string("load failed"));
    return resolve_task(Any(42.0));
  };
  adapter->dispose = [](Any) {};
  library->runtime->adapters.set(String("t"), adapter);
  auto first = assets::acquire_asset(library, String("a"));
  auto second = assets::acquire_asset(library, String("a"));
  static_cast<void>(first.get());
  static_cast<void>(second.get());
  check(loads == 1, "a concurrent acquire shares the in-flight load");
  check(assets::get_asset_ref_count(library, String("a")) == 2.0, "each acquire holds one reference");
  static_cast<void>(assets::acquire_asset(library, String("a")).get());
  check(loads == 1 && assets::get_asset_ref_count(library, String("a")) == 3.0, "a resident asset resolves without reloading");
  auto other = make_ref<types::AssetDescriptor>(types::AssetDescriptor{.id = String("b"), .url = String("v"), .type = String("t"), .groups = std::nullopt});
  assets::register_asset_descriptor(library, other);
  fail_next = true;
  auto failed = assets::acquire_asset(library, String("b"));
  const auto settlement = [&] { try { failed.get(); } catch (...) {} return failed.settle(); }();
  check(settlement.status == TaskStatus::rejected && settlement.rejection->is<std::string>() &&
            settlement.rejection->as<std::string>() == "load failed",
        "a failed load rejects every holder with the adapter's exact reason");
  check(!library->runtime->entries.has(String("b")), "a failed load does not leave a poisoned entry behind");
  fail_next = false;
  static_cast<void>(assets::acquire_asset(library, String("b")).get());
  check(loads == 3, "a later acquire after a failure starts a fresh load");
  return failures == 0 ? 0 : 1;
}
