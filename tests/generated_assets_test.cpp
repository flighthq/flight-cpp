#include <flight/assets/contract.hpp>

#include <algorithm>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

// Native port of packages/assets/src/{assetLibrary,enableAssetGuards,explainAssetLoad}.test.ts (a627849).
// acquireAsset comes from the assets-reject-task-template-parameter override; everything else exercised
// here is the generated emission (and the complete assets-enable-asset-guards-callable-identity override).
//
// The upstream createMockAdapter() is ported as MockAdapter: each load is a pending Task resolved by
// flush(), so in-flight state (dedup, orphaned release) is observable, and every load resolves a fresh
// Loaded object keyed by id, so identity assertions tell a shared value from a re-load. Values are compared
// by the identity of the Ref the Any holds, which is what `toBe` compares.
//
// registerAssetLoader is refused by the emitter (docs/compiler-requests/readonly-adapter-stored-as-writable.md)
// and absent from the override. Where it is only setup, use_loader() performs its source body,
// `library.runtime.adapters.set(type, adapter)`.
//
// Excluded, with their blockers:
// - loadAssetGroup's three cases and releaseAssetGroup's 'releases and disposes every group member' (which
//   loads through loadAssetGroup): loadAssetGroup is refused, because Promise.allSettled has no C++ binding
//   (docs/compiler-requests/promise-all-settled-binding.md).
// - registerAssetLoader 'is last-write-wins for a type': its subject is the refused function itself.
// - enableAssetGuards' two warning cases and disableAssetGuards 'restores terse rejection without a
//   warning': each captures the log through createMemoryLogSink, which reaches the log override's
//   initialize_log_entity NotImplementedInThisProfile stub (see that override's `remaining`).
// - acquireAsset<T> for T other than flight::Any: the override supplies only T = Any
//   (docs/compiler-requests/generic-promise-assertion-to-type-parameter.md). Every case below uses Any.

namespace {

using namespace flight;
namespace assets = flight::assets;
using Library = Ref<types::AssetLibrary>;

int failures = 0;

void check(bool condition, const std::string& message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

struct Loaded : ReferenceEnabled {
  String id;
};

Ref<Loaded> loaded_of(const Any& value) { return value.object_if<Loaded>(); }

bool is_loaded(const std::optional<Any>& value, const char* id) {
  if (!value.has_value()) return false;
  const auto object = loaded_of(*value);
  return object != nullptr && object->id == String(id);
}

bool same_value(const std::optional<Any>& a, const Any& b) {
  return a.has_value() && loaded_of(*a) != nullptr && loaded_of(*a) == loaded_of(b);
}

// The text of a settled rejection's Error, or "" when the task fulfilled.
std::string rejection_message(const Task<Any>& task) {
  const auto settlement = task.settle();
  if (settlement.status != TaskStatus::rejected || !settlement.rejection->is<Error>()) return "";
  return settlement.rejection->as<Error>().message().to_utf8();
}

struct MockAdapter {
  int load_calls = 0;
  int in_flight = 0;
  int peak = 0;
  std::vector<Ref<Loaded>> disposed;
  std::vector<std::function<void()>> pending;
  Ref<types::AssetLoaderAdapter<>> adapter = make_ref<types::AssetLoaderAdapter<>>();

  void flush() {
    auto wave = std::move(pending);
    pending.clear();
    for (auto& settle : wave) settle();
  }
};

std::shared_ptr<MockAdapter> mock_adapter() {
  auto mock = std::make_shared<MockAdapter>();
  std::weak_ptr<MockAdapter> weak = mock;
  mock->adapter->load = [weak](auto descriptor) -> Task<Any> {
    auto mock = weak.lock();
    ++mock->load_calls;
    ++mock->in_flight;
    mock->peak = std::max(mock->peak, mock->in_flight);
    const String id = row_get<RowKey<"id">>(descriptor);
    return Task<Any>::create([weak, id](Task<Any>::Resolver resolve) {
      weak.lock()->pending.push_back([weak, id, resolve] {
        --weak.lock()->in_flight;
        resolve(Any::object(make_ref<Loaded>(Loaded{.id = id})));
      });
    });
  };
  mock->adapter->dispose = [weak](Any value) { weak.lock()->disposed.push_back(loaded_of(value)); };
  return mock;
}

void use_loader(const Library& library, const char* type, const Ref<types::AssetLoaderAdapter<>>& adapter) {
  library->runtime->adapters.set(String(type), adapter);
}

Ref<types::AssetDescriptor> descriptor(const char* id, const char* type, const char* url,
                                       std::optional<std::vector<const char*>> groups = std::nullopt) {
  std::optional<Array<String>> group_array;
  if (groups.has_value()) {
    Array<String> values;
    for (const char* group : *groups) values.push(String(group));
    group_array = values;
  }
  return make_ref<types::AssetDescriptor>(
      types::AssetDescriptor{.id = String(id), .url = String(url), .type = String(type), .groups = group_array});
}

types::AssetManifest manifest(std::initializer_list<Ref<types::AssetDescriptor>> entries) {
  types::AssetManifest out;
  for (const auto& entry : entries) out.push(entry);
  return out;
}

// Upstream's libraryWith(id, type): a library with the mock registered for `type` and a one-descriptor
// manifest recorded for `id`.
std::pair<Library, std::shared_ptr<MockAdapter>> library_with(const char* id, const char* type = "image") {
  auto library = assets::create_asset_library();
  auto mock = mock_adapter();
  use_loader(library, type, mock->adapter);
  assets::register_asset_manifest(library, manifest({descriptor(id, type, (std::string(id) + ".bin").c_str())}));
  return {library, mock};
}

std::vector<std::string> strings(const Array<String>& values) {
  std::vector<std::string> out;
  for (const auto& value : values) out.push_back(value.to_utf8());
  return out;
}

using Strings = std::vector<std::string>;

void test_acquire_asset() {
  {
    auto library = assets::create_asset_library();
    int attempts = 0;
    auto adapter = make_ref<types::AssetLoaderAdapter<>>();
    adapter->dispose = [](Any) {};
    adapter->load = [&](auto) -> Task<Any> {
      ++attempts;
      if (attempts == 1) return Task<Any>::reject(Error(String("temporary")));
      return resolve_task(Any::object(make_ref<Loaded>(Loaded{.id = String("hero")})));
    };
    use_loader(library, "image", adapter);
    assets::register_asset_descriptor(library, descriptor("hero", "image", "hero.bin"));
    const auto first = assets::acquire_asset(library, String("hero"));
    const bool rejected = rejection_message(first) == "temporary";
    const auto second = assets::acquire_asset(library, String("hero"));
    check(rejected && is_loaded(second.get(), "hero") && attempts == 2 && assets::get_asset_ref_count(library, String("hero")) == 1,
          "acquireAsset drops a rejected entry so a later acquire retries");
  }
  {
    auto [library, mock] = library_with("hero");
    const auto promise = assets::acquire_asset(library, String("hero"));
    const bool one_call = mock->load_calls == 1;
    mock->flush();
    const auto value = promise.get();
    check(one_call && is_loaded(value, "hero") && same_value(assets::get_asset<Any>(library, String("hero")), value),
          "acquireAsset resolves the adapter loaded value and calls load once");
  }
  {
    auto [library, mock] = library_with("hero");
    const auto first = assets::acquire_asset(library, String("hero"));
    const auto second = assets::acquire_asset(library, String("hero"));
    const bool deduped = mock->load_calls == 1 && assets::get_asset_ref_count(library, String("hero")) == 2;
    mock->flush();
    const auto a = first.get();
    const auto b = second.get();
    check(deduped && loaded_of(a) == loaded_of(b) && loaded_of(a) != nullptr && same_value(assets::get_asset<Any>(library, String("hero")), a),
          "acquireAsset shares one in-flight load across concurrent acquires (dedup)");
  }
  {
    auto library = assets::create_asset_library();
    check(rejection_message(assets::acquire_asset(library, String("missing"))).find("no descriptor") != std::string::npos,
          "acquireAsset rejects when no descriptor is recorded for the id");
  }
  {
    auto library = assets::create_asset_library();
    assets::register_asset_manifest(library, manifest({descriptor("hero", "image", "hero.bin")}));
    check(rejection_message(assets::acquire_asset(library, String("hero"))).find("no loader") != std::string::npos,
          "acquireAsset rejects when no adapter is registered for the descriptor type");
  }
}

void test_create_and_dispose() {
  {
    auto library = assets::create_asset_library();
    check(!assets::get_asset<Any>(library, String("hero")).has_value() && assets::get_asset_ref_count(library, String("hero")) == 0,
          "createAssetLibrary creates an empty library with no resident assets");
  }
  {
    auto [library, mock] = library_with("hero");
    const auto first = assets::acquire_asset(library, String("hero"));
    mock->flush();
    static_cast<void>(first.get());
    static_cast<void>(assets::acquire_asset(library, String("hero")).get());
    const bool two = assets::get_asset_ref_count(library, String("hero")) == 2;
    assets::dispose_asset_library(library);
    check(two && mock->disposed.size() == 1 && mock->disposed[0] != nullptr && mock->disposed[0]->id == String("hero") &&
              !assets::get_asset<Any>(library, String("hero")).has_value() && assets::get_asset_ref_count(library, String("hero")) == 0,
          "disposeAssetLibrary disposes every resident asset and empties the library");
  }
  static_assert(std::is_invocable_v<decltype(&assets::initialize_asset_library), types::EntityConstruction<Library>>,
                "initializeAssetLibrary is the construction initializer of createAssetLibrary");
}

void test_queries() {
  {
    auto [library, mock] = library_with("hero");
    const bool before = !assets::get_asset<Any>(library, String("hero")).has_value();
    const auto promise = assets::acquire_asset(library, String("hero"));
    const bool loading = !assets::get_asset<Any>(library, String("hero")).has_value();
    mock->flush();
    const auto value = promise.get();
    check(before && loading && same_value(assets::get_asset<Any>(library, String("hero")), value),
          "getAsset returns null before load and the value once resident");
  }
  {
    auto library = assets::create_asset_library();
    assets::register_asset_manifest(library, manifest({descriptor("a", "image", "a.bin", std::vector<const char*>{"boot"}),
                                                       descriptor("b", "image", "b.bin", std::vector<const char*>{"boot", "shared"})}));
    auto ids = assets::get_asset_group_ids(library, String("boot"));
    const bool listed = strings(ids) == Strings{"a", "b"};
    ids.resize(0.0);
    check(listed && strings(assets::get_asset_group_ids(library, String("boot"))) == Strings{"a", "b"} &&
              assets::get_asset_group_ids(library, String("unknown")).size() == 0,
          "getAssetGroupIds returns a detached snapshot of catalog group membership");
  }
  {
    auto [library, mock] = library_with("hero");
    const auto pending = assets::acquire_asset(library, String("hero"));
    const bool while_loading = strings(assets::get_asset_ids(library)) == Strings{"hero"};
    mock->flush();
    static_cast<void>(pending.get());
    const bool resident = strings(assets::get_asset_ids(library)) == Strings{"hero"};
    assets::release_asset(library, String("hero"));
    check(while_loading && resident && assets::get_asset_ids(library).size() == 0,
          "getAssetIds enumerates held loading and resident entries, then drops freed ids");
  }
  {
    auto [library, mock] = library_with("hero");
    const auto promise = assets::acquire_asset(library, String("hero"));
    mock->flush();
    static_cast<void>(promise.get());
    static_cast<void>(assets::acquire_asset(library, String("hero")).get());
    const bool two = assets::get_asset_ref_count(library, String("hero")) == 2;
    assets::release_asset(library, String("hero"));
    const bool one = assets::get_asset_ref_count(library, String("hero")) == 1;
    assets::release_asset(library, String("hero"));
    check(two && one && assets::get_asset_ref_count(library, String("hero")) == 0, "getAssetRefCount counts acquires and drops to zero when freed");
  }
}

void test_register() {
  {
    auto library = assets::create_asset_library();
    Array<String> groups;
    groups.push(String("boot"));
    auto entry = make_ref<types::AssetDescriptor>(
        types::AssetDescriptor{.id = String("hero"), .url = String("hero.bin"), .type = String("image"), .groups = groups});
    assets::register_asset_descriptor(library, entry);
    groups.push(String("mutated"));
    const auto recorded = library->runtime->descriptors.get(String("hero"));
    check(recorded.has_value() && recorded.value()->groups.has_value() && strings(*recorded.value()->groups) == Strings{"boot"} &&
              !library->runtime->groups.has(String("mutated")),
          "registerAssetDescriptor copies caller-owned group data into the catalog");
  }
  {
    // Not an upstream case: upstream never registers duplicate groups. The expected copy is what upstream's own
    // copyAssetDescriptor (assetLibrary.ts at a627849, run under node) returns: ["boot","boot","shared","boot"]
    // becomes ["boot","shared"] in first-seen order, and a descriptor without groups keeps none.
    auto library = assets::create_asset_library();
    assets::register_asset_descriptor(library, descriptor("hero", "image", "hero.bin", std::vector<const char*>{"boot", "boot", "shared", "boot"}));
    assets::register_asset_descriptor(library, descriptor("x", "image", "x.bin"));
    const auto hero = library->runtime->descriptors.get(String("hero"));
    const auto x = library->runtime->descriptors.get(String("x"));
    const auto boot = library->runtime->groups.get(String("boot"));
    check(hero.has_value() && hero.value()->groups.has_value() && strings(*hero.value()->groups) == Strings{"boot", "shared"} &&
              x.has_value() && !x.value()->groups.has_value() && boot.has_value() && strings(*boot) == Strings{"hero"},
          "registerAssetDescriptor stores each group once, in first-seen order, and no groups when none were given");
  }
  {
    auto library = assets::create_asset_library();
    assets::register_asset_descriptor(library, descriptor("hero", "image", "hero-a.bin", std::vector<const char*>{"boot", "shared"}));
    assets::register_asset_descriptor(library, descriptor("hero", "image", "hero-b.bin", std::vector<const char*>{"level", "shared"}));
    const auto level = library->runtime->groups.get(String("level"));
    const auto shared = library->runtime->groups.get(String("shared"));
    check(!library->runtime->groups.get(String("boot")).has_value() && level.has_value() && strings(*level) == Strings{"hero"} &&
              shared.has_value() && strings(*shared) == Strings{"hero"},
          "registerAssetDescriptor moves replacement group membership instead of retaining stale groups");
  }
  {
    auto library = assets::create_asset_library();
    auto mock = mock_adapter();
    use_loader(library, "image", mock->adapter);
    assets::register_asset_descriptor(library, descriptor("hero", "image", "hero.bin", std::vector<const char*>{"boot"}));
    const auto pending = assets::acquire_asset(library, String("hero"));
    bool equivalent_ok = true;
    try {
      assets::register_asset_descriptor(library, descriptor("hero", "image", "hero.bin", std::vector<const char*>{"boot"}));
    } catch (...) {
      equivalent_ok = false;
    }
    std::string replaced;
    try {
      assets::register_asset_descriptor(library, descriptor("hero", "image", "replacement.bin", std::vector<const char*>{"boot"}));
    } catch (const Error& error) {
      replaced = error.message().to_utf8();
    }
    mock->flush();
    static_cast<void>(pending.get());
    check(equivalent_ok && replaced.find("cannot replace acquired descriptor") != std::string::npos,
          "registerAssetDescriptor rejects changing an acquired descriptor but permits equivalent registration");
  }
  {
    auto library = assets::create_asset_library();
    auto mock = mock_adapter();
    use_loader(library, "image", mock->adapter);
    assets::register_asset_manifest(library, manifest({descriptor("a", "image", "a.bin", std::vector<const char*>{"boot"}),
                                                       descriptor("b", "image", "b.bin", std::vector<const char*>{"boot"})}));
    const bool nothing_loaded = mock->load_calls == 0 && !assets::get_asset<Any>(library, String("a")).has_value();
    const auto promise = assets::acquire_asset(library, String("a"));
    mock->flush();
    static_cast<void>(promise.get());
    check(nothing_loaded && is_loaded(assets::get_asset<Any>(library, String("a")), "a"),
          "registerAssetManifest records descriptors and group membership without loading");
  }
}

void test_release() {
  {
    auto [library, mock] = library_with("hero");
    const auto promise = assets::acquire_asset(library, String("hero"));
    assets::release_asset(library, String("hero"));
    const bool dropped = assets::get_asset_ref_count(library, String("hero")) == 0 && !assets::get_asset<Any>(library, String("hero")).has_value();
    mock->flush();
    static_cast<void>(promise.get());
    check(dropped && mock->disposed.size() == 1 && mock->disposed[0] != nullptr && mock->disposed[0]->id == String("hero") &&
              !assets::get_asset<Any>(library, String("hero")).has_value(),
          "releaseAsset disposes the orphaned value when released while the load is still in flight");
  }
  {
    auto [library, mock] = library_with("hero");
    const auto promise = assets::acquire_asset(library, String("hero"));
    mock->flush();
    const auto value = promise.get();
    static_cast<void>(assets::acquire_asset(library, String("hero")).get());
    assets::release_asset(library, String("hero"));
    const bool held = mock->disposed.empty() && same_value(assets::get_asset<Any>(library, String("hero")), value);
    assets::release_asset(library, String("hero"));
    check(held && mock->disposed.size() == 1 && mock->disposed[0] == loaded_of(value) && !assets::get_asset<Any>(library, String("hero")).has_value(),
          "releaseAsset disposes and drops the asset at reference count zero");
  }
  {
    auto [library, mock] = library_with("hero");
    assets::release_asset(library, String("hero"));
    assets::release_asset(library, String("hero"));
    check(mock->disposed.empty() && assets::get_asset_ref_count(library, String("hero")) == 0, "releaseAsset is a no-op when releasing below zero");
  }
  {
    auto [library, mock] = library_with("hero");
    const auto first = assets::acquire_asset(library, String("hero"));
    const auto second = assets::acquire_asset(library, String("hero"));
    const bool one_load = mock->load_calls == 1;
    mock->flush();
    const auto a = first.get();
    const auto b = second.get();
    const bool shared = loaded_of(a) == loaded_of(b) && assets::get_asset_ref_count(library, String("hero")) == 2;
    assets::release_asset(library, String("hero"));
    check(one_load && shared && assets::get_asset_ref_count(library, String("hero")) == 1 && mock->disposed.empty() &&
              same_value(assets::get_asset<Any>(library, String("hero")), a),
          "releaseAsset keeps the asset loaded when one of two holders releases");
  }
  {
    auto library = assets::create_asset_library();
    bool threw = false;
    try {
      assets::release_asset_group(library, String("nope"));
    } catch (...) {
      threw = true;
    }
    check(!threw, "releaseAssetGroup is a no-op for an unknown group");
  }
}

void test_acquire_guard() {
  auto library = assets::create_asset_library();
  Strings statuses;
  assets::set_asset_acquire_guard(
      library, std::function<void(StructuralRef<RowReadonly<RowOf<Library>>>, StructuralRef<RowReadonly<RowOf<Ref<types::AssetLoadExplanation>>>>)>(
                   [&](auto, auto explanation) { statuses.push_back(row_get<RowKey<"status">>(explanation).to_utf8()); }));
  const bool first = rejection_message(assets::acquire_asset(library, String("missing"))).find("no descriptor") != std::string::npos;
  assets::set_asset_acquire_guard(library, std::nullopt);
  const bool second = rejection_message(assets::acquire_asset(library, String("still-missing"))).find("no descriptor") != std::string::npos;
  check(first && second && statuses == Strings{"missing-descriptor"}, "setAssetAcquireGuard installs and removes the per-library acquire failure hook");
}

void test_guards() {
  auto guarded = assets::create_asset_library();
  auto unguarded = assets::create_asset_library();
  assets::enable_asset_guards(guarded);
  check(assets::are_asset_guards_enabled(guarded) && !assets::are_asset_guards_enabled(unguarded), "areAssetGuardsEnabled reports guard state per library");
}

bool explains(const Library& library, const char* status, double ref_count) {
  const auto explanation = assets::explain_asset_load(library, String("hero"));
  return explanation->status == String(status) && explanation->ref_count == ref_count;
}

void test_explain() {
  {
    auto library = assets::create_asset_library();
    const auto missing = assets::explain_asset_load(library, String("hero"));
    const bool missing_ok = missing->id == String("hero") && missing->ref_count == 0 && missing->status == String("missing-descriptor") &&
                            !missing->type.has_value();
    assets::register_asset_descriptor(library, descriptor("hero", "image", "hero.bin"));
    const auto no_loader = assets::explain_asset_load(library, String("hero"));
    const bool no_loader_ok = no_loader->id == String("hero") && no_loader->ref_count == 0 && no_loader->status == String("missing-loader") &&
                              no_loader->type.has_value() && *no_loader->type == String("image");
    std::optional<Task<Any>::Resolver> settle;
    auto adapter = make_ref<types::AssetLoaderAdapter<>>();
    adapter->dispose = [](Any) {};
    adapter->load = [&](auto) -> Task<Any> { return Task<Any>::create([&](Task<Any>::Resolver resolve) { settle = resolve; }); };
    use_loader(library, "image", adapter);
    const bool never = explains(library, "never-acquired", 0);
    const auto pending = assets::acquire_asset(library, String("hero"));
    const bool loading = explains(library, "loading", 1);
    (*settle)(Any::object(make_ref<Loaded>(Loaded{.id = String("hero")})));
    static_cast<void>(pending.get());
    const bool resident = explains(library, "resident", 1);
    assets::release_asset(library, String("hero"));
    check(missing_ok && no_loader_ok && never && loading && resident && explains(library, "freed", 0),
          "explainAssetLoad distinguishes catalog, adapter, loading, resident, and freed states");
  }
  {
    auto library = assets::create_asset_library();
    auto adapter = make_ref<types::AssetLoaderAdapter<>>();
    adapter->dispose = [](Any) {};
    adapter->load = [](auto) -> Task<Any> { return resolve_task(Any::object(make_ref<Loaded>(Loaded{.id = String("hero")}))); };
    use_loader(library, "image", adapter);
    assets::register_asset_descriptor(library, descriptor("hero", "image", "hero-a.bin"));
    static_cast<void>(assets::acquire_asset(library, String("hero")).get());
    assets::release_asset(library, String("hero"));
    assets::register_asset_descriptor(library, descriptor("hero", "image", "hero-b.bin"));
    check(assets::explain_asset_load(library, String("hero"))->status == String("never-acquired"),
          "explainAssetLoad treats a replacement descriptor as a new never-acquired catalog entry");
  }
}

}  // namespace

int main() {
  test_acquire_asset();
  test_create_and_dispose();
  test_queries();
  test_register();
  test_release();
  test_acquire_guard();
  test_guards();
  test_explain();
  return failures == 0 ? 0 : 1;
}
