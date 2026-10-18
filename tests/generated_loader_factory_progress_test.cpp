// The generated @flighthq/loader module at a627849, consumed through the installed public header:
// - create_resource_loader finishes its construction at the exact owner and returns the same loader,
//   so a factory call does not abort in exact-owner recovery;
// - the progress body reads the loader's recorded cells through the structural member view: a
//   public-only loader has `started` stored false, so progress is 0 for the global and grouped
//   questions alike;
// - dispose_resource_loader reads the nullable itemSignals member through the two-layer presence
//   path (its cell is a stored null here) and completes without touching a signal owner.
#include <flight/loader/resource_loader.hpp>
#include <flight/types/resource_loader_options.hpp>

#include <iostream>

namespace {

int failures = 0;

void check(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}

} // namespace

int main() {
  namespace loader = flight::loader;

  auto first = loader::create_resource_loader();
  check(first != nullptr, "create_resource_loader returns a loader");
  check(loader::get_resource_load_progress(first) == 0.0, "a not-started loader reports 0 progress");
  check(
      loader::get_resource_load_progress(first, flight::String("group")) == 0.0,
      "a not-started loader reports 0 group progress");

  auto options = flight::make_ref<flight::types::ResourceLoaderOptions>();
  options->dedupe = false;
  options->error_policy = flight::String("fail-fast");
  options->max_concurrent = 2.0;
  using OptionsView =
      flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::ResourceLoaderOptions>>>>;
  auto configured = loader::create_resource_loader(OptionsView(options));
  check(configured != nullptr, "create_resource_loader accepts an options row");
  check(configured.get() != first.get(), "each factory call returns its own loader");
  check(
      loader::get_resource_load_progress(configured) == 0.0,
      "a configured, not-started loader reports 0 progress");
  check(
      loader::get_resource_load_progress(configured, flight::String("group")) == 0.0,
      "a configured, not-started loader reports 0 group progress");

  // The returned reference is the same owner across copies, and it stays usable while held.
  auto held = configured;
  check(held.get() == configured.get(), "a held reference is the same loader owner");
  check(loader::get_resource_load_progress(held) == 0.0, "the held loader answers progress");

  loader::dispose_resource_loader(configured);
  check(true, "dispose_resource_loader completes with a null item-signal group");

  if (failures != 0) {
    std::cerr << failures << " loader check(s) failed\n";
    return 1;
  }
  std::cout << "loader factory finishes its construction and the progress body reads the public loader\n";
  return 0;
}
