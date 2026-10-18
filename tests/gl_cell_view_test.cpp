#include <flight/nominal_or_structural_view.hpp>

#include <cstdio>
#include <memory>
#include <optional>
#include <type_traits>

namespace {
struct Registries : flight::ReferenceEnabled { double marker{}; };
struct GlRegistries : Registries {
  using flight_native_base = Registries;
  double gl_tag{};
};
struct Runtime : flight::ReferenceEnabled { flight::Ref<Registries> registries; double frame{}; };
struct GlRuntime : Runtime {
  using flight_native_base = Runtime;
  double gl_frame{};
};
struct State : flight::ReferenceEnabled { std::optional<flight::Ref<Runtime>> entity_runtime_key; };
struct GlState : State { using flight_native_base = State; };
using RuntimeView = flight::NominalOrStructuralView<GlRuntime, Runtime>;
using RegistryView = flight::NominalOrStructuralView<GlRegistries, Registries>;
int failures = 0;
void check(bool condition, const char* message) {
  if (!condition) { std::printf("FAIL %s\n", message); ++failures; }
}
template <typename Left, typename Right>
bool same_owner(const Left& left, const Right& right) {
  return !left.owner_before(right) && !right.owner_before(left);
}
template <typename View>
bool refuses_native(const View& view) {
  try { (void)view.native(); }
  catch (const flight::NativeScalarEscapeError&) { return true; }
  catch (...) { return false; }
  return false;
}
}

int main() {
  static_assert(!std::is_convertible_v<RuntimeView, flight::Ref<GlRuntime>>);
  static_assert(!std::is_convertible_v<RegistryView, flight::Ref<GlRegistries>>);
  auto first = flight::make_ref<GlRuntime>();
  auto second = flight::make_ref<GlRuntime>();
  first->gl_frame = 5;
  auto state = flight::make_ref<GlState>();
  flight::Ref<State> base = state;
  check(same_owner(base, state), "state views share one allocation/control block");
  check(&base->entity_runtime_key == &state->entity_runtime_key, "one physical runtime cell");
  base->entity_runtime_key = first;
  auto saved = RuntimeView::read_cell(state->entity_runtime_key);
  check(saved && saved->object() == first && saved->native() == first, "base write read as same derived owner");
  check(same_owner(saved->object(), first), "runtime view preserves control block");
  base->entity_runtime_key = second;
  check(RuntimeView::read_cell(state->entity_runtime_key)->native() == second, "base replacement is live");
  check(saved->object() == first, "a previously read source value keeps its original identity");
  RuntimeView::write_cell(state->entity_runtime_key, RuntimeView(first));
  check(base->entity_runtime_key == first, "derived replacement reaches base cell");
  RuntimeView::write_cell(state->entity_runtime_key, std::nullopt);
  check(!base->entity_runtime_key, "derived clear reaches base cell");
  base->entity_runtime_key = second;
  base->entity_runtime_key = std::nullopt;
  check(!RuntimeView::read_cell(state->entity_runtime_key), "base clear reaches derived view");

  auto a = flight::make_ref<GlRegistries>();
  auto b = flight::make_ref<GlRegistries>();
  a->gl_tag = 7;
  first->registries = a;
  flight::Ref<Runtime> runtime_base = first;
  check(&first->registries == &runtime_base->registries, "one physical registries cell");
  auto registries = RegistryView::read_cell(first->registries);
  check(registries.object() == a && registries.native() == a, "registries read preserves owner");
  check(same_owner(registries.object(), a), "registries preserve control block");
  runtime_base->registries = b;
  check(RegistryView::read_cell(first->registries).native() == b, "base registries replacement is live");
  RegistryView::write_cell(first->registries, RegistryView(a));
  check(runtime_base->registries == a, "derived registries replacement reaches base");
  runtime_base->registries->marker = 23;
  check(RegistryView::read_cell(first->registries).native()->marker == 23, "registry mutation reaches original object");

  auto base_registries = flight::make_ref<Registries>();
  runtime_base->registries = base_registries;
  auto wrong_registries = RegistryView::read_cell(first->registries);
  check(wrong_registries.object() == base_registries, "wrong registries value is carried lazily");
  check(!wrong_registries.read<flight::RowKey<"glTag">, double, &GlRegistries::gl_tag>().has_value(),
        "base-only registries derived member reads undefined");
  check(refuses_native(wrong_registries), "wrong registries native escape throws shared error");
  auto base_runtime = flight::make_ref<Runtime>();
  base->entity_runtime_key = base_runtime;
  auto wrong_runtime = RuntimeView::read_cell(state->entity_runtime_key);
  check(wrong_runtime && wrong_runtime->object() == base_runtime, "wrong runtime value is carried lazily");
  check(!wrong_runtime->read<flight::RowKey<"glFrame">, double, &GlRuntime::gl_frame>().has_value(),
        "base-only runtime derived member reads undefined");
  check(refuses_native(*wrong_runtime), "wrong runtime native escape throws shared error");
  base_runtime->registries = a;
  auto nested = wrong_runtime->read_cell_member<flight::RowKey<"registries">, RegistryView, &GlRuntime::registries>();
  check(nested.native() == a, "a base-only runtime reads its guaranteed physical registries cell without a derived cast");
  wrong_runtime->write_cell_member<flight::RowKey<"registries">, RegistryView, &GlRuntime::registries>(RegistryView(b));
  check(base_runtime->registries == b, "nested derived view writes through the guaranteed base cell");


  // Provenance stays bounded: even a genuine GlRuntime cannot be recovered through an unrecorded base.
  flight::Ref<Runtime> unrecorded = std::make_shared<GlRuntime>();
  const auto unproven = RuntimeView::of(unrecorded);
  check(unproven.object() == unrecorded && refuses_native(unproven), "missing provenance remains an explicit partial escape");
  check(RuntimeView(flight::Ref<GlRuntime>(std::make_shared<GlRuntime>())).native() != nullptr,
        "a native caller already holding the derived owner needs no recovery");

  const auto key = flight::Symbol::for_key(flight::String("EntityRuntime"));
  auto empty = flight::make_ref<GlState>();
  flight::StructuralRef<flight::RowOf<flight::Ref<State>>> row{flight::Ref<State>(empty)};
  check(!flight::row_has_own_property(row, key), "unwritten runtime is missing");
  RuntimeView::write_symbol_cell<&GlState::entity_runtime_key>(empty, key, std::optional<RuntimeView>{});
  check(flight::row_has_own_property(row, key) && !RuntimeView::read_cell(empty->entity_runtime_key),
        "written undefined is present while both value views read undefined");
  check(!flight::row_has_own_property(row, flight::Symbol(flight::String("EntityRuntime"))), "same-description symbol is distinct");
  check(row.shared_owner()->symbol_cell(key)->storage_address() == &empty->entity_runtime_key,
        "symbol view reaches physical cell");
  check(!row.shared_owner()->has_dynamic_value(key), "no attachment substitutes for physical cell");
  RuntimeView::write_row_cell(row, key, RuntimeView(first));
  check(empty->entity_runtime_key == first &&
        RuntimeView::read_row_cell<State, &State::entity_runtime_key>(row, key)->native() == first,
        "construction and readonly row projection uses the base cell type");
  RuntimeView::write_row_cell(row, key, std::nullopt);
  check(!empty->entity_runtime_key && !row.shared_owner()->has_dynamic_value(key),
        "row clear stays physical without a derived-reference attachment");


  {
    auto owner = flight::make_ref<GlRuntime>();
    std::weak_ptr<GlRuntime> weak = owner;
    std::optional<flight::Ref<Runtime>> cell = owner;
    auto view = RuntimeView::read_cell(cell);
    owner.reset(); cell.reset();
    check(!weak.expired(), "read view retains original runtime allocation");
    view.reset();
    check(weak.expired(), "last runtime view releases allocation");
  }
  {
    auto owner = flight::make_ref<GlRegistries>();
    std::weak_ptr<GlRegistries> weak = owner;
    flight::Ref<Registries> cell = owner;
    auto view = RegistryView::read_cell(cell);
    owner.reset(); cell.reset();
    check(!weak.expired(), "read view retains original registries allocation");
    view = {};
    check(weak.expired(), "last registry view releases allocation");
  }
  std::printf("GL cell views: %d failures\n", failures);
  return failures ? 1 : 0;
}
