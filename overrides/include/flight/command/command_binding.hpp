// Hand-written override for the nominal CommandBindingTable boundary. TypeScript structurally extends
// KeyedTable here, but the generated C++ declarations are separate owners; this module constructs and
// queries the exact owner stored by CommandHistory.
#pragma once

#include <optional>
#include <variant>

#include <flight/entity/entity.hpp>
#include <flight/runtime.hpp>
#include <flight/types/command.hpp>
#include <flight/types/entity.hpp>
#include <flight/types/node.hpp>
#include <flight/types/registry_table.hpp>

namespace flight::command {

using flight::types::CommandBinding;
using flight::types::CommandBindingTable;
using flight::types::CommandHistory;
using flight::types::CommandPropertyEntry;
using flight::types::EntityConstruction;
using flight::types::Kind;
using flight::types::NodeAny;
using flight::types::SetNodePropertyCommand;

inline flight::Ref<CommandBindingTable> create_command_binding_table() {
  auto out = flight::entity::allocate_entity<flight::Ref<CommandBindingTable>>();
  out->entries = {};
  out->on_miss = flight::types::command_binding_miss_policy;
  out->registry = flight::types::command_binding_registry_id;
  out->shape = flight::String("keyed");
  return flight::entity::finish_entity<flight::Ref<CommandBindingTable>>(out);
}

inline std::optional<flight::Ref<CommandBinding>> get_command_binding(
    flight::Ref<CommandHistory> history, flight::Ref<Kind> kind) {
  const auto entry = history->bindings->entries.get(kind);
  if (!entry.has_value()) return std::nullopt;
  return std::visit(
      [](const auto& selected) -> std::optional<flight::Ref<CommandBinding>> {
        if (selected->state != flight::types::registry_entry_state->bound) return std::nullopt;
        if constexpr (requires { selected->value; }) return selected->value;
        return std::nullopt;
      },
      entry.value());
}

inline bool has_command_binding(flight::Ref<CommandHistory> history, flight::Ref<Kind> kind) {
  return get_command_binding(history, kind).has_value();
}

inline void initialize_set_node_property_command(
    flight::Ref<EntityConstruction<flight::Ref<SetNodePropertyCommand>>> out,
    flight::Array<flight::Ref<CommandPropertyEntry>> entries, flight::Ref<Kind> kind,
    flight::String label, double merge_window, double time) {
  out->entries = std::move(entries);
  out->kind = std::move(kind);
  out->label = std::move(label);
  out->merge_window = merge_window;
  out->time = time;
}

inline void register_command_binding(flight::Ref<CommandHistory> history, flight::Ref<Kind> kind,
                                     flight::Ref<CommandBinding> binding) {
  using Value = flight::Ref<CommandBinding>;
  using BoundEntry = flight::types::state_value_49c40b4f01b4314b<Value>;
  auto entries = history->bindings->entries.clone();
  entries.set(
      kind,
      flight::types::RegistryTableEntry<Value>{
          std::in_place_type<flight::Ref<BoundEntry>>,
          flight::make_ref<BoundEntry>(BoundEntry{
              .state = flight::types::registry_entry_state->bound,
              .value = std::move(binding),
          })});

  auto out = flight::entity::allocate_entity<flight::Ref<CommandBindingTable>>();
  out->entries = std::move(entries);
  out->on_miss = history->bindings->on_miss;
  out->registry = history->bindings->registry;
  out->shape = history->bindings->shape;
  history->bindings = flight::entity::finish_entity<flight::Ref<CommandBindingTable>>(out);
}

inline double resolve_insert_index(flight::Ref<NodeAny> parent, double index) {
  double count = 0.0;
  if (parent->entity_runtime_key.has_value()) {
    const auto& children = parent->entity_runtime_key.value()->children;
    if (children.has_value()) count = static_cast<double>(children->size());
  }
  return index < 0.0 || index > count ? count : index;
}

// These source bindings require checked recovery of derived command owners (and dynamic named-property
// writes for the property binding), which the current target runtime deliberately does not provide. Keep
// their declarations visible so this partial module and its aggregators compile without pretending that
// an unsafe native cast is an implementation.
extern flight::Ref<CommandBinding> add_node_child_command_binding;
extern flight::Ref<CommandBinding> remove_node_child_command_binding;
extern flight::Ref<CommandBinding> reorder_node_child_command_binding;
extern flight::Ref<CommandBinding> set_node_property_command_binding;
flight::Ref<CommandBinding> composite_command_binding(flight::Ref<CommandHistory> history);

inline void register_default_command_bindings(flight::Ref<CommandHistory> history) {
  register_command_binding(history, flight::types::add_node_child_command_kind,
                           add_node_child_command_binding);
  register_command_binding(history, flight::types::composite_command_kind,
                           composite_command_binding(history));
  register_command_binding(history, flight::types::remove_node_child_command_kind,
                           remove_node_child_command_binding);
  register_command_binding(history, flight::types::reorder_node_child_command_kind,
                           reorder_node_child_command_binding);
  register_command_binding(history, flight::types::set_node_property_command_kind,
                           set_node_property_command_binding);
}

}  // namespace flight::command
