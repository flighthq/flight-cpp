// Mechanism probe: actual generated exact Sprite slot, actual flat SpriteRuntime, existing row APIs.
// Include selection/provenance belongs in the accompanying design receipt, not a product override.
#include <flight/types/sprite.hpp>
#include <flight/types/node_2d.hpp>
#include <flight/types/has_bounds_rectangle.hpp>
#include <iostream>
#include <tuple>
#include <type_traits>

using Traits = flight::Ref<flight::types::Node2DTraits>;
using Generic = flight::types::NodeRuntime<Traits>;
using Exact = flight::types::SpriteRuntime;
using Sprite = flight::types::Sprite;
using ExactRef = flight::Ref<Exact>;
using ExactView = flight::StructuralRef<flight::RowWritable<flight::RowOf<ExactRef>>>;
using CommonView = flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<Generic>>>>;
using SpriteView = flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<Sprite>>>>;
template <class T> struct FunctionArgs;
template <class R, class... A> struct FunctionArgs<std::function<R(A...)>> { using tuple = std::tuple<A...>; };
using SpriteBoundsSource = std::tuple_element_t<1, FunctionArgs<decltype(Exact::compute_local_bounds_rectangle)>::tuple>;
using NodeBoundsSource = std::tuple_element_t<1, FunctionArgs<decltype(flight::types::Node2DRuntime::compute_local_bounds_rectangle)>::tuple>;

template <class Common, class From, class Check>
void common_view_checks(const From& exact, const ExactRef& runtime, Check check) {
  if constexpr (std::constructible_from<Common, From>) {
    Common common(exact);
    check("common view retains the exact native allocation", common.shared_native_object().get() == runtime.get());
    check("common view recovers original exact runtime", common.template shared_exact_object<Exact>() == runtime);
    check("common view shares the original row owner", common.shared_owner() == exact.shared_owner());
    check("flat common view has no generic native subobject", !common.shared_object());
    flight::row_set<flight::RowKey<"appearanceId">>(common, 17.0);
    check("common-view write updates exact physical field", runtime->appearance_id == 17);
    runtime->local_content_id = 23;
    check("exact-field mutation is live through common view", flight::row_get<flight::RowKey<"localContentId">>(common) == 23);
    check("common callback is the actual field cell", &flight::row_get<flight::RowKey<"canAddChild">>(common) == &runtime->can_add_child);
    check("common parent slot is the actual field cell", &flight::row_get<flight::RowKey<"parent">>(common) == &runtime->parent);
  }
}

int main() {
  int checks = 0, failures = 0;
  const auto check = [&](const char* name, bool result) {
    ++checks; if (!result) ++failures;
    std::cout << name << ": " << (result ? "PASS" : "FAIL") << '\n';
  };
  std::cout << "node2d_has_generic_base=" << std::is_base_of_v<Generic, flight::types::Node2DRuntime> << '\n';
  std::cout << "common_runtime_view_constructible=" << std::constructible_from<CommonView, ExactView> << '\n';
  std::cout << "sprite_bounds_receiver_from_sprite_view=" << std::constructible_from<SpriteBoundsSource, SpriteView> << '\n';
  std::cout << "node_bounds_receiver_from_sprite_view=" << std::constructible_from<NodeBoundsSource, SpriteView> << '\n';
  std::cout << "bounds_receiver_types_same=" << std::is_same_v<SpriteBoundsSource, NodeBoundsSource> << '\n';
  check("actual generated Sprite has exact runtime optional slot", std::is_same_v<decltype(Sprite::entity_runtime_key), std::optional<ExactRef>>);
  check("exact runtime still has no fabricated generic base", !std::is_base_of_v<Generic, Exact>);
  check("existing table proves writable common runtime view", std::constructible_from<CommonView, ExactView>);
  auto sprite = flight::make_ref<Sprite>();
  auto runtime = flight::make_ref<Exact>();
  SpriteView source(sprite);
  flight::row_set(source, flight::types::entity_runtime_key, runtime);
  check("exact computed write reaches physical slot", sprite->entity_runtime_key.value() == runtime);
  check("exact computed read returns physical value", flight::row_get<std::optional<ExactRef>>(source, flight::types::entity_runtime_key).value() == runtime);
  check("exact computed write creates no dynamic shadow", source.shared_owner()->dynamic_value(flight::types::entity_runtime_key) == nullptr);
  check("legacy generic-pointer read is not a flat base view", !flight::row_get<std::optional<flight::Ref<Generic>>>(source, flight::types::entity_runtime_key));
  ExactView exact(runtime);
  common_view_checks<CommonView>(exact, runtime, check);
  auto next = flight::make_ref<Exact>();
  flight::row_set(source, flight::types::entity_runtime_key, next);
  check("replacement updates the same exact physical slot", sprite->entity_runtime_key.value() == next);
  check("previously retained runtime view keeps original owner", exact.shared_native_object().get() == runtime.get());
  flight::row_set(source, flight::types::entity_runtime_key, std::nullopt);
  check("undefined write clears the physical optional", !sprite->entity_runtime_key);
  check("undefined assignment remains source-present", flight::row_has_own_property(source, flight::types::entity_runtime_key));
  check("undefined value remains disengaged independently of presence", !flight::row_has(source, flight::types::entity_runtime_key));
  check("clear and replacement never add a dynamic shadow", source.shared_owner()->dynamic_value(flight::types::entity_runtime_key) == nullptr);
  // Keep each declared callback type. Both take a view of the original Sprite; no function conversion.
  bool sprite_compute = false, node_compute = false, sprite_valid = false, node_valid = false;
  runtime->compute_local_bounds_rectangle = [&](flight::Ref<flight::types::Rectangle>, SpriteBoundsSource received) {
    sprite_compute = received.template shared_exact_object<Sprite>() == sprite && received.shared_owner() == source.shared_owner();
  };
  runtime->is_local_bounds_rectangle_valid = [&](SpriteBoundsSource received) {
    sprite_valid = received.template shared_exact_object<Sprite>() == sprite && received.shared_owner() == source.shared_owner();
    return sprite_valid;
  };
  auto node_runtime = flight::make_ref<flight::types::Node2DRuntime>();
  node_runtime->compute_local_bounds_rectangle = [&](flight::Ref<flight::types::Rectangle>, NodeBoundsSource received) {
    node_compute = received.template shared_exact_object<Sprite>() == sprite && received.shared_owner() == source.shared_owner();
  };
  node_runtime->is_local_bounds_rectangle_valid = [&](NodeBoundsSource received) {
    node_valid = received.template shared_exact_object<Sprite>() == sprite && received.shared_owner() == source.shared_owner();
    return node_valid;
  };
  runtime->compute_local_bounds_rectangle({}, SpriteBoundsSource(source));
  check("Sprite compute callback receives the original Sprite owner", sprite_compute);
  check("Sprite validity callback receives the original Sprite owner", runtime->is_local_bounds_rectangle_valid.value()(SpriteBoundsSource(source)) && sprite_valid);
  node_runtime->compute_local_bounds_rectangle({}, NodeBoundsSource(source));
  check("Node2D compute callback receives the original Sprite owner", node_compute);
  check("Node2D validity callback receives the original Sprite owner", node_runtime->is_local_bounds_rectangle_valid.value()(NodeBoundsSource(source)) && node_valid);
  std::cout << "Only fixture callbacks execute: original Sprite factory, bounds handlers and renderers are not qualified.\n";
  std::cout << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
