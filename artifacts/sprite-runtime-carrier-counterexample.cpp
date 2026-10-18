// Design probe against actual installed declarations. No Sprite implementation or replacement type.
#include <flight/node/native_node2d_access.hpp>
#include <flight/erased_ref.hpp>
#include <flight/types/entity.hpp>
#include <iostream>
#include <type_traits>

using Traits = flight::Ref<flight::types::Node2DTraits>;
using Generic = flight::types::NodeRuntime<Traits>;
using Exact = flight::types::SpriteRuntime;
using Sprite = flight::types::Sprite;
using GenericRef = flight::Ref<Generic>;
using ExactRef = flight::Ref<Exact>;

int main() {
  int checks = 0, failures = 0;
  const auto check = [&](const char* name, bool result) {
    ++checks;
    if (!result) ++failures;
    std::cout << name << ": " << (result ? "PASS" : "FAIL") << '\n';
  };
  std::cout << "node2d_has_generic_base=" << std::is_base_of_v<Generic, flight::types::Node2DRuntime> << '\n';
  std::cout << "sprite_runtime_has_generic_base=" << std::is_base_of_v<Generic, Exact> << '\n';
  std::cout << "sprite_runtime_has_node2d_base=" << std::is_base_of_v<flight::types::Node2DRuntime, Exact> << '\n';
  std::cout << "bounds_callbacks_same=" << std::is_same_v<decltype(Exact::compute_local_bounds_rectangle), decltype(flight::types::Node2DRuntime::compute_local_bounds_rectangle)> << '\n';
  std::cout << "traits_cells_same=" << std::is_same_v<decltype(Exact::traits), decltype(Generic::traits)> << '\n';
  std::cout << "child_callbacks_same=" << std::is_same_v<decltype(Exact::can_add_child), decltype(Generic::can_add_child)> << '\n';
  check("installed slot cannot assign exact flat SpriteRuntime", !std::is_assignable_v<decltype(Sprite::entity_runtime_key)&, ExactRef>);
  check("installed SpriteRuntime has no generic runtime base", !std::is_base_of_v<Generic, Exact>);
  check("installed recursive bounds callback carriers differ", !std::is_same_v<decltype(Exact::compute_local_bounds_rectangle), decltype(flight::types::Node2DRuntime::compute_local_bounds_rectangle)>);

  auto sprite = flight::make_ref<Sprite>();
  auto old_runtime = flight::make_ref<Generic>();
  auto exact_runtime = flight::make_ref<Exact>();
  sprite->entity_runtime_key = old_runtime;
  sprite->enabled = true;
  sprite->x = 0;
  sprite->alpha = 1;
  flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<Sprite>>>> row(sprite);
  const auto slot = &sprite->entity_runtime_key;
  flight::row_set(row, flight::types::entity_runtime_key, exact_runtime);
  check("incompatible computed write leaves physical generic slot unchanged", slot == &sprite->entity_runtime_key && sprite->entity_runtime_key.value() == old_runtime);
  check("exact computed read sees separate attached value", flight::row_get<ExactRef>(row, flight::types::entity_runtime_key) == exact_runtime);
  check("generic computed read still sees physical slot", flight::row_get<std::optional<GenericRef>>(row, flight::types::entity_runtime_key).value() == old_runtime);
  check("optional exact computed read does not recover bare attached value", !flight::row_get<std::optional<ExactRef>>(row, flight::types::entity_runtime_key));
  check("legacy runtime helper still returns physical generic owner", flight::node::native_node_2d_runtime(flight::node::native_node_2d(sprite)) == old_runtime);
  check("generic allocation cannot recover unrelated SpriteRuntime", !flight::recover_owner<Exact>(old_runtime));

  flight::ErasedRef erased(exact_runtime);
  check("existing erased reference retains exact runtime", erased.as<Exact>() == exact_runtime);
  check("exact erased reference does not fabricate a generic base", !erased.as<Generic>());
  check("exact allocation provenance recovers original runtime", flight::recover_owner<Exact>(exact_runtime) == exact_runtime);

  // A REAL existing inheritance edge shows that C++ convertibility alone does not adapt symbol cells.
  using Entity = flight::types::Entity;
  using EntityRuntime = flight::types::EntityRuntime;
  check("real NodeRuntime-to-EntityRuntime pointer upcast exists", std::is_convertible_v<GenericRef, flight::Ref<EntityRuntime>>);
  check("real derived pointer can assign native base optional slot", std::is_assignable_v<decltype(Entity::entity_runtime_key)&, GenericRef>);
  auto entity = flight::make_ref<Entity>();
  auto next_runtime = flight::make_ref<Generic>();
  entity->entity_runtime_key = old_runtime;
  flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<Entity>>>> entity_row(entity);
  flight::row_set(entity_row, flight::types::entity_runtime_key, next_runtime);
  check("computed derived write also leaves real base slot unchanged", entity->entity_runtime_key.value() == old_runtime);
  check("computed derived write creates another typed answer", flight::row_get<GenericRef>(entity_row, flight::types::entity_runtime_key) == next_runtime);
  check("checked recovery through real base preserves exact allocation", flight::recover_owner<Generic>(entity->entity_runtime_key.value()) == old_runtime);
  auto declared = flight::ErasedRef::with_declared_base<EntityRuntime>(next_runtime);
  check("existing declared-base erasure retains exact type", declared.as<Generic>() == next_runtime);
  check("existing declared-base erasure returns same real base owner", declared.declared_base_as<EntityRuntime>() == next_runtime);
  check("direct typed assignment updates the one physical base slot", (entity->entity_runtime_key = next_runtime).value() == next_runtime);
  std::cout << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
