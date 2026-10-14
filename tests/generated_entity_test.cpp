// Focused native parity for @flighthq/entity at Flight a627849.
//
// This suite owns the emitted package's allocation, clone/strip, binding, UID, guard, diagnostic,
// and runtime lifecycle behavior. The absent getEntityRuntime path is intentionally not called:
// docs/compiler-requests/non-null-asserted-optional-property-return.md records why the current
// non-optional C++ boundary throws where the JavaScript source returns undefined. The adjacent
// structural-construction-finish-identity.md and proxy-return-loses-nominal-traps.md requests record
// the two other residuals rather than letting usable-state or entity-slot checks overclaim them.
//
// MUTATION SENSITIVITY, measured 2026-10-13: ten one-at-a-time shadow-header mutations were
// compiled and run, then discarded. The suite killed construction with a pre-bound runtime (8
// failures), forced binding runtime replacement (2), retained clone/strip runtime slots (3), a
// guard mode that never disables (5), swapped diagnostic helper order (1), retained detached
// binding ownership (1), a UID generator that did not advance (1), a guard proxy listening to the
// wrong property (3), disabled runtime guarding that minted a new owner (1), and an ordinary
// property incorrectly observed by the entity guard (3).
#include <flight/entity/contract.hpp>

#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

namespace entity = flight::entity;
namespace types = flight::types;

int failures = 0;

void check(bool condition, const std::string& message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

template <typename Type>
auto readonly(const std::shared_ptr<Type>& value) {
  using View = flight::StructuralRef<
      flight::RowReadonly<flight::RowOf<flight::Ref<Type>>>>;
  return flight::structural_ref_cast<View>(value);
}

struct TestBinding final : flight::ReferenceEnabled {
  flight::String name;
};

struct OtherBinding final : flight::ReferenceEnabled {
  double value{};
};

struct Payload final : flight::ReferenceEnabled {
  double value{};
};

struct RichEntity final : flight::ReferenceEnabled {
  std::optional<flight::Ref<types::EntityRuntime>> entity_runtime_key;
  flight::String label;
  flight::Ref<Payload> payload;
  double x{};
};

flight::Ref<types::Entity> fresh_entity() {
  auto out = entity::allocate_entity<flight::Ref<types::Entity>>();
  check(static_cast<bool>(out), "allocation returns a live construction row");
  check(!flight::row_get<std::optional<flight::Ref<types::EntityRuntime>>>(
             out, types::entity_runtime_key)
              .has_value(),
        "allocation starts with an absent runtime slot");
  return entity::finish_entity<flight::Ref<types::Entity>>(out);
}

void allocation_binding_and_disposal() {
  const auto value = fresh_entity();
  check(value && !entity::has_entity_runtime(readonly(value)),
        "finishing allocation produces an unbound entity");
  check(!entity::get_entity_binding(readonly(value)).has_value() &&
            !entity::has_entity_binding(readonly(value)),
        "binding lookup reports absence without allocating runtime state");

  entity::detach_entity_binding(value);
  check(!value->entity_runtime_key.has_value(),
        "detaching an absent binding does not allocate a runtime");

  auto first = flight::make_ref<TestBinding>(
      TestBinding{.name = flight::String("first")});
  entity::attach_entity_binding(value, flight::ErasedRef(first));
  const auto runtime = value->entity_runtime_key;
  check(runtime.has_value() && entity::has_entity_runtime(readonly(value)),
        "attaching allocates runtime state exactly when needed");
  {
    const auto erased = entity::get_entity_binding(readonly(value));
    const auto typed = entity::get_entity_binding_as<flight::Ref<TestBinding>>(
        readonly(value));
    const auto mismatched =
        entity::get_entity_binding_as<flight::Ref<OtherBinding>>(readonly(value));
    check(erased.has_value() && erased->identity() == first.get() &&
              typed.has_value() && typed.value() == first && !mismatched.has_value(),
          "binding lookup preserves identity and rejects a caller-owned wrong type");
  }

  std::weak_ptr<TestBinding> released = first;
  auto replacement = flight::make_ref<TestBinding>(
      TestBinding{.name = flight::String("replacement")});
  first.reset();
  entity::attach_entity_binding(value, flight::ErasedRef(replacement));
  check(value->entity_runtime_key == runtime && released.expired(),
        "binding replacement preserves runtime ownership and releases the old binding");

  std::weak_ptr<TestBinding> detached = replacement;
  replacement.reset();
  entity::detach_entity_binding(value);
  check(value->entity_runtime_key == runtime && detached.expired() &&
            !entity::has_entity_binding(readonly(value)),
        "detach clears binding ownership while preserving runtime identity");
}

void clone_strip_and_public_ownership() {
  const auto payload = flight::make_ref<Payload>(Payload{.value = 7.0});
  const auto source = flight::make_ref<RichEntity>(RichEntity{
      .entity_runtime_key = entity::create_entity_runtime(),
      .label = flight::String("source"),
      .payload = payload,
      .x = 12.0,
  });
  source->entity_runtime_key.value()->binding =
      flight::ErasedRef(flight::make_ref<TestBinding>());

  const auto clone = entity::clone_entity<flight::Ref<RichEntity>>(readonly(source));
  check(clone && clone != source && clone->label == flight::String("source") &&
            clone->x == 12.0,
        "clone returns a fresh entity with the same public scalar data");
  check(clone->payload == payload,
        "clone keeps JavaScript spread's shallow ownership for public references");
  check(!clone->entity_runtime_key.has_value() &&
            source->entity_runtime_key.has_value(),
        "clone resets runtime state without mutating its source");

  const auto stripped =
      entity::strip_entity_runtime<flight::Ref<RichEntity>>(readonly(source));
  check(stripped && stripped != source &&
            stripped->label == flight::String("source") && stripped->x == 12.0 &&
            stripped->payload == payload,
        "runtime stripping copies public data with shallow reference ownership");
  check(!stripped->entity_runtime_key.has_value() &&
            source->entity_runtime_key.has_value(),
        "runtime stripping removes binding state from the output only");

  const auto rebound = flight::make_ref<types::Entity>(
      types::Entity{.entity_runtime_key = stripped->entity_runtime_key});
  check(!entity::has_entity_runtime(readonly(rebound)),
        "stripped runtime state round-trips as a valid unbound entity");
}

void runtime_and_uid_lifecycle() {
  const auto runtime = entity::create_entity_runtime();
  check(runtime && !runtime->binding.has_value() && !runtime->uid.has_value(),
        "a new runtime has empty binding and UID slots");

  const auto value = fresh_entity();
  value->entity_runtime_key = runtime;
  const auto runtime_view = entity::get_entity_runtime(readonly(value));
  check(flight::structural_ref_cast<flight::Ref<types::EntityRuntime>>(
            runtime_view) == runtime && entity::has_entity_runtime(readonly(value)),
        "present runtime lookup preserves exact runtime identity");

  const double saved_counter = entity::next_entity_uid_counter;
  entity::next_entity_uid_counter = 1.0;
  const auto first = fresh_entity();
  const auto second = fresh_entity();
  const flight::String first_uid = entity::get_entity_uid(first);
  check(first_uid == flight::String("entity-1") &&
            entity::get_entity_uid(first) == first_uid &&
            entity::get_entity_uid(second) == flight::String("entity-2"),
        "UIDs are lazy, stable, distinct, and advance in allocation order");
  check(first->entity_runtime_key.has_value() &&
            second->entity_runtime_key.has_value(),
        "UID access allocates runtime state for an untouched entity");

  const auto assigned = fresh_entity();
  entity::set_entity_uid(assigned, flight::String("stable"));
  check(entity::get_entity_uid(assigned) == flight::String("stable"),
        "setting a UID before lookup creates stable runtime state");
  entity::set_entity_uid(first, flight::String("replacement"));
  check(entity::get_entity_uid(first) == flight::String("replacement") &&
            entity::get_entity_uid(first) != first_uid,
        "an assigned UID replaces a generated one");
  entity::next_entity_uid_counter = saved_counter;
}

void guard_lifecycle_and_diagnostics() {
  entity::disable_entity_runtime_guards();
  check(!entity::are_entity_runtime_guards_enabled(),
        "runtime guards are disabled after explicit reset");

  using Entity = flight::Ref<types::Entity>;
  using GuardedView = flight::StructuralRef<flight::RowMerge<
      flight::RowOf<Entity>, flight::RowOf<flight::Ref<types::Entity>>>>;
  const auto target = fresh_entity();
  const GuardedView view = flight::structural_ref_cast<GuardedView>(target);
  const auto unguarded = entity::create_guarded_entity(view);
  check(unguarded == view,
        "guard creation preserves the original row while guard mode is disabled");
  const auto unguarded_runtime = entity::create_entity_runtime();
  check(entity::create_guarded_entity_runtime(unguarded_runtime) ==
            unguarded_runtime,
        "disabled runtime guard creation preserves the original runtime identity");

  std::vector<flight::String> slots;
  entity::enable_entity_runtime_guards(
      [&](types::EntityRuntimeWriteSlot slot) { slots.push_back(std::move(slot)); });
  check(entity::are_entity_runtime_guards_enabled(),
        "enabling guards installs guard mode and the caller reporter together");
  const auto guarded = entity::create_guarded_entity(view);
  const auto assigned_runtime = entity::create_entity_runtime();
  flight::row_set(guarded, types::entity_runtime_key, assigned_runtime);
  check(slots == std::vector<flight::String>{flight::String("runtime-slot")} &&
            target->entity_runtime_key.has_value() &&
            target->entity_runtime_key.value() == assigned_runtime,
        "a guarded runtime-slot write reports once and still reaches the entity");

  using RichGuardedView = flight::StructuralRef<flight::RowMerge<
      flight::RowOf<flight::Ref<RichEntity>>,
      flight::RowOf<flight::Ref<types::Entity>>>>;
  const auto ordinary_target = flight::make_ref<RichEntity>();
  const RichGuardedView ordinary_view =
      flight::structural_ref_cast<RichGuardedView>(ordinary_target);
  const auto ordinary_guarded =
      entity::create_guarded_entity<flight::Ref<RichEntity>>(ordinary_view);
  flight::row_set<flight::RowKey<"x">>(ordinary_guarded, 4.0);
  check(slots.size() == 1 && ordinary_target->x == 4.0,
        "guarded ordinary-property writes stay silent and reach the entity");

  entity::set_entity_runtime_guard_mode(false);
  flight::row_set(guarded, types::entity_runtime_key, std::nullopt);
  check(slots.size() == 1 && !target->entity_runtime_key.has_value(),
        "an existing proxy becomes silent after guard mode is disabled");

  entity::set_entity_runtime_guard_mode(true);
  entity::set_entity_runtime_write_guard(std::nullopt);
  const auto reporterless = entity::create_guarded_entity(view);
  flight::row_set(reporterless, types::entity_runtime_key, assigned_runtime);
  check(slots.size() == 1 && target->entity_runtime_key == assigned_runtime,
        "uninstalling the reporter leaves guarded writes allowed and silent");
  entity::disable_entity_runtime_guards();
  check(!entity::are_entity_runtime_guards_enabled(),
        "disabling guards uninstalls mode and reporter as one lifecycle reset");

  const auto binding =
      entity::explain_entity_runtime_write(flight::String("binding-slot"));
  const auto runtime_slot =
      entity::explain_entity_runtime_write(flight::String("runtime-slot"));
  check(binding->slot == flight::String("binding-slot") &&
            binding->use_instead.size() == 2.0 &&
            binding->use_instead.element(0.0) ==
                flight::String("attachEntityBinding") &&
            binding->use_instead.element(1.0) ==
                flight::String("detachEntityBinding"),
        "binding diagnostics identify both tracked mutation helpers");
  check(runtime_slot->slot == flight::String("runtime-slot") &&
            runtime_slot->use_instead.size() == 1.0 &&
            runtime_slot->use_instead.element(0.0) ==
                flight::String("attachEntityBinding") &&
            runtime_slot->message != binding->message,
        "runtime diagnostics identify allocation and use a distinct message");
}

}  // namespace

int main() {
  allocation_binding_and_disposal();
  clone_strip_and_public_ownership();
  runtime_and_uid_lifecycle();
  guard_lifecycle_and_diagnostics();

  if (failures == 0) {
    std::cout << "generated entity tests passed\n";
    return 0;
  }
  std::cerr << failures << " generated entity test(s) failed\n";
  return 1;
}
