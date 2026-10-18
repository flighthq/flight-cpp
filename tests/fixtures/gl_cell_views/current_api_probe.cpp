// Counterexample against current runtime APIs, not a GL implementation/public-consumer substitute.
// These declarations mirror the two currently flattened source relations. The by-owner projections
// deliberately expose the missing payload bridge; no native downcast or replacement owner is used.
#include <flight/runtime.hpp>
#include <flight/out_of_profile.hpp>
#include <iostream>
#include <memory>
#include <optional>
#include <type_traits>

namespace sample {
struct RenderRegistries : flight::ReferenceEnabled { double marker{}; };
struct GlRegistries : flight::ReferenceEnabled { double marker{}; double gl_tag{}; };
struct RenderRuntime : flight::ReferenceEnabled {
  flight::Ref<RenderRegistries> registries;
  double frame{};
};
struct GlRuntime : flight::ReferenceEnabled {
  flight::Ref<GlRegistries> registries;
  double frame{};
  double gl_frame{};
};
struct RenderState : flight::ReferenceEnabled {
  std::optional<flight::Ref<RenderRuntime>> entity_runtime_key;
};
struct GlState : flight::ReferenceEnabled {
  std::optional<flight::Ref<GlRuntime>> entity_runtime_key;
};
}

int main() {
  using namespace sample;
  using BaseStateView = flight::StructuralRef<flight::RowOf<flight::Ref<RenderState>>>;
  using DerivedStateView = flight::StructuralRef<flight::RowOf<flight::Ref<GlState>>>;
  using BaseRuntimeView = flight::StructuralRef<flight::RowOf<flight::Ref<RenderRuntime>>>;
  using DerivedRuntimeView = flight::StructuralRef<flight::RowOf<flight::Ref<GlRuntime>>>;
  static_assert(std::is_base_of_v<std::runtime_error, flight::NativeScalarEscapeError>);
  const auto key = flight::Symbol::for_key(flight::String("EntityRuntime"));
  auto registries = flight::make_ref<GlRegistries>();
  auto runtime = flight::make_ref<GlRuntime>(); runtime->registries = registries;
  auto state = flight::make_ref<GlState>(); state->entity_runtime_key = runtime;
  auto derived = DerivedStateView(state);
  auto base = BaseStateView::from_owner(derived.shared_owner());
  const bool exact_state_cell = derived.shared_owner()->symbol_cell(key) == base.shared_owner()->symbol_cell(key);
  const auto base_read = flight::row_get<std::optional<flight::Ref<RenderRuntime>>>(base, key);
  auto runtime_derived = DerivedRuntimeView(runtime);
  auto runtime_base = BaseRuntimeView::from_owner(runtime_derived.shared_owner());
  bool base_registry_raw_read_lazy = true;
  try { (void)flight::row_get<flight::RowKey<"registries">>(runtime_base); }
  catch (const std::bad_cast&) { base_registry_raw_read_lazy = false; }
  auto base_only = flight::make_ref<RenderRuntime>();
  flight::row_set(base, key, std::optional<flight::Ref<RenderRuntime>>{base_only});
  const bool physical_replaced = state->entity_runtime_key && state->entity_runtime_key->get() == static_cast<void*>(base_only.get());
  const bool used_attachment = base.shared_owner()->has_dynamic_value(key);
  flight::row_set(base, key, std::nullopt);
  const bool physical_cleared = !state->entity_runtime_key.has_value();
  const bool base_read_cleared = !flight::row_get<std::optional<flight::Ref<RenderRuntime>>>(base, key).has_value();

  auto absent = flight::make_ref<GlState>();
  auto absent_view = DerivedStateView(absent);
  const bool absent_is_missing = !flight::row_has_own_property(absent_view, key);
  flight::row_set(absent_view, key, std::nullopt);
  const bool written_undefined_present = flight::row_has_own_property(absent_view, key);
  const bool unique_symbol_distinct = !flight::row_has_own_property(absent_view, flight::Symbol(flight::String("EntityRuntime")));
  std::weak_ptr<GlState> weak = state;
  state.reset(); derived = {};
  const bool base_keeps_owner = !weak.expired();
  base = {};
  const bool released = weak.expired();
  auto print = [](const char* name, bool value, bool last = false) {
    std::cout << '"' << name << "\":" << (value ? "true" : "false") << (last ? "}\n" : ",");
  };
  std::cout << '{';
  print("onePhysicalSymbolCell", exact_state_cell);
  print("baseReadsPresentRuntime", base_read.has_value());
  print("baseRegistryRawReadIsLazy", base_registry_raw_read_lazy);
  print("baseOnlyRuntimeWriteChangesPhysicalCell", physical_replaced);
  print("legacyWriteUsesAttachment", used_attachment);
  print("baseClearVisibleToDerived", physical_cleared);
  print("baseReadAfterClearIsUndefined", base_read_cleared);
  print("absentIsNotPresent", absent_is_missing);
  print("writtenUndefinedIsPresent", written_undefined_present);
  print("sameDescriptionSymbolDistinct", unique_symbol_distinct);
  print("baseViewRetainsOwner", base_keeps_owner);
  print("lastViewReleasesOwner", released, true);
}
