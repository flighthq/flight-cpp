// Re-derived from the generated command module at a627849. Every function the emitter produced is carried
// verbatim except two spellings, and the one declaration it refused is supplied in its place. See
// overrides/manifest.json (command-undeclared-node-helpers) for what changed, why, and what remains.
//
// OVERRIDE: initialize_remove_node_child_command and initialize_reorder_node_child_command spell the
// node package's Traits argument as flight::Any. NodeAny is Node<any>, and the emitter left its generic
// parameter name `Traits` undeclared in these non-generic functions
// (docs/compiler-requests/node-any-generic-traits-leak.md).
#pragma once
#include <flight/any.hpp>
#include <flight/sequence_view.hpp>
#include <flight/structural_ref.hpp>
#include <functional>
#include <optional>
#include <flight/runtime.hpp>

static_assert(flight::runtime_contract.compiler_contract == "flight-runtime-contract/2", "Flight compiler/runtime contract mismatch");
static_assert(flight::runtime_contract.cpp_abi == 1, "Flight C++ runtime ABI mismatch");

namespace flight::types { struct AddNodeChildCommand; }
namespace flight::types { struct Command; }
namespace flight::types { struct CommandPropertyEntry; }
namespace flight::types { struct CompositeCommand; }
namespace flight::types { struct Entity; }
namespace flight::types { struct RemoveNodeChildCommand; }
namespace flight::types { struct ReorderNodeChildCommand; }
namespace flight::types { struct SetNodePropertyCommand; }
namespace flight::types { template <typename Traits> struct Node; }

#include <flight/entity/entity.hpp>
#include <flight/node/hierarchy.hpp>
#include <flight/types/command.hpp>
#include <flight/types/entity.hpp>
#include <flight/types/node.hpp>

namespace flight::command {

using flight::types::AddNodeChildCommand;
using flight::types::Command;
using flight::types::CommandPropertyEntry;
using flight::types::CompositeCommand;
using flight::types::NodeAny;
using flight::types::RemoveNodeChildCommand;
using flight::types::ReorderNodeChildCommand;
using flight::types::SetNodePropertyCommand;

inline void initialize_add_node_child_command(flight::types::EntityConstruction<flight::Ref<flight::types::AddNodeChildCommand>> out, flight::String label, flight::types::NodeAny parent, flight::types::NodeAny child, double index) {
  flight::row_set<flight::RowKey<"child">>(out, child);
  flight::row_set<flight::RowKey<"index">>(out, index);
  flight::row_set<flight::RowKey<"kind">>(out, flight::types::add_node_child_command_kind);
  flight::row_set<flight::RowKey<"label">>(out, label);
  flight::row_set<flight::RowKey<"parent">>(out, parent);
}

inline flight::Ref<flight::types::AddNodeChildCommand> create_add_node_child_command(flight::String label, flight::types::NodeAny parent, flight::types::NodeAny child, std::optional<double> index = std::nullopt) {
  index = index.value_or(-1.0);
  flight::types::EntityConstruction<flight::Ref<flight::types::AddNodeChildCommand>> out = flight::entity::allocate_entity<flight::Ref<flight::types::AddNodeChildCommand>>();
  initialize_add_node_child_command(out, label, parent, child, index.value());
  return flight::entity::finish_entity<flight::Ref<flight::types::AddNodeChildCommand>>(out);
}

inline void initialize_composite_command(flight::types::EntityConstruction<flight::Ref<flight::types::CompositeCommand>> out, flight::String label, flight::Array<flight::Ref<flight::types::Command>> children) {
  flight::row_set<flight::RowKey<"children">>(out, ([&]() { flight::Array<flight::Ref<flight::types::Command>> array_spread_result; for (const auto& array_spread_item : children) { array_spread_result.push(array_spread_item); } return array_spread_result; }()));
  flight::row_set<flight::RowKey<"kind">>(out, flight::types::composite_command_kind);
  flight::row_set<flight::RowKey<"label">>(out, label);
}

inline flight::Ref<flight::types::CompositeCommand> create_composite_command(flight::String label, flight::Array<flight::Ref<flight::types::Command>> children) {
  flight::types::EntityConstruction<flight::Ref<flight::types::CompositeCommand>> out = flight::entity::allocate_entity<flight::Ref<flight::types::CompositeCommand>>();
  initialize_composite_command(out, label, children);
  return flight::entity::finish_entity<flight::Ref<flight::types::CompositeCommand>>(out);
}

inline void initialize_remove_node_child_command(flight::types::EntityConstruction<flight::Ref<flight::types::RemoveNodeChildCommand>> out, flight::String label, flight::types::NodeAny parent, flight::types::NodeAny child) {
  flight::row_set<flight::RowKey<"child">>(out, child);
  flight::row_set<flight::RowKey<"index">>(out, flight::node::get_node_child_index(flight::structural_ref_cast<flight::StructuralRef<flight::RowReadonly<flight::RowMerge<flight::RowOf<std::shared_ptr<flight::types::Node<flight::Any>>>, flight::RowOf<flight::Any>>>>>(flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::types::NodeAny>>>(parent)), flight::structural_ref_cast<flight::StructuralRef<flight::RowReadonly<flight::RowMerge<flight::RowOf<std::shared_ptr<flight::types::Node<flight::Any>>>, flight::RowOf<flight::Any>>>>>(flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::types::NodeAny>>>(child))));
  flight::row_set<flight::RowKey<"kind">>(out, flight::types::remove_node_child_command_kind);
  flight::row_set<flight::RowKey<"label">>(out, label);
  flight::row_set<flight::RowKey<"parent">>(out, parent);
}

inline flight::Ref<flight::types::RemoveNodeChildCommand> create_remove_node_child_command(flight::String label, flight::types::NodeAny parent, flight::types::NodeAny child) {
  flight::types::EntityConstruction<flight::Ref<flight::types::RemoveNodeChildCommand>> out = flight::entity::allocate_entity<flight::Ref<flight::types::RemoveNodeChildCommand>>();
  initialize_remove_node_child_command(out, label, parent, child);
  return flight::entity::finish_entity<flight::Ref<flight::types::RemoveNodeChildCommand>>(out);
}

inline void initialize_reorder_node_child_command(flight::types::EntityConstruction<flight::Ref<flight::types::ReorderNodeChildCommand>> out, flight::String label, flight::types::NodeAny parent, flight::types::NodeAny child, double to_index) {
  flight::row_set<flight::RowKey<"child">>(out, child);
  flight::row_set<flight::RowKey<"fromIndex">>(out, flight::node::get_node_child_index(flight::structural_ref_cast<flight::StructuralRef<flight::RowReadonly<flight::RowMerge<flight::RowOf<std::shared_ptr<flight::types::Node<flight::Any>>>, flight::RowOf<flight::Any>>>>>(flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::types::NodeAny>>>(parent)), flight::structural_ref_cast<flight::StructuralRef<flight::RowReadonly<flight::RowMerge<flight::RowOf<std::shared_ptr<flight::types::Node<flight::Any>>>, flight::RowOf<flight::Any>>>>>(flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::types::NodeAny>>>(child))));
  flight::row_set<flight::RowKey<"kind">>(out, flight::types::reorder_node_child_command_kind);
  flight::row_set<flight::RowKey<"label">>(out, label);
  flight::row_set<flight::RowKey<"parent">>(out, parent);
  flight::row_set<flight::RowKey<"toIndex">>(out, to_index);
}

inline flight::Ref<flight::types::ReorderNodeChildCommand> create_reorder_node_child_command(flight::String label, flight::types::NodeAny parent, flight::types::NodeAny child, double to_index) {
  flight::types::EntityConstruction<flight::Ref<flight::types::ReorderNodeChildCommand>> out = flight::entity::allocate_entity<flight::Ref<flight::types::ReorderNodeChildCommand>>();
  initialize_reorder_node_child_command(out, label, parent, child, to_index);
  return flight::entity::finish_entity<flight::Ref<flight::types::ReorderNodeChildCommand>>(out);
}


// OVERRIDE: readNodeProperty, refused at the erasure of Readonly<NodeAny> into unknown
// (docs/compiler-requests/structural-row-erasure-to-unknown.md).
//
// The source it stands for:
//   function readNodeProperty(target: Readonly<NodeAny>, property: string): unknown {
//     return (target as unknown as Readonly<Record<string, unknown>>)[property];
//   }
//
// The keyed read goes through the node's one row owner, the same owner its structural projection
// resolves to, so it reads the node's own cells with no copy and no second identity. A key with no cell
// reads as undefined, which is what the source's Record read returns for an absent property.
inline flight::Any read_node_property(flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::types::NodeAny>>> target, flight::String property) {
  return flight::named_properties(target).get(property);
}

#ifndef FLIGHT_COMPILER_ANONYMOUS__FLIGHTHQ_COMMAND_PROPERTY_TARGET_VALUE_1FB663985ABECA9C
#define FLIGHT_COMPILER_ANONYMOUS__FLIGHTHQ_COMMAND_PROPERTY_TARGET_VALUE_1FB663985ABECA9C
struct property_target_value_1fb663985abeca9c : public flight::ReferenceEnabled {
  flight::String property;
  flight::types::NodeAny target;
  flight::Any value;
};
#endif // FLIGHT_COMPILER_ANONYMOUS__FLIGHTHQ_COMMAND_PROPERTY_TARGET_VALUE_1FB663985ABECA9C

inline flight::Ref<flight::types::SetNodePropertyCommand> create_set_node_property_command_batch(flight::String label, flight::SequenceView<flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<property_target_value_1fb663985abeca9c>>>>> entries, std::optional<double> merge_window = std::nullopt, std::optional<double> time = std::nullopt) {
  merge_window = merge_window.value_or(0.0);
  time = time.value_or(0.0);
  flight::Array<flight::Ref<flight::types::CommandPropertyEntry>> captured = entries.map([=](flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<property_target_value_1fb663985abeca9c>>>> entry) {
  return flight::make_ref<flight::types::CommandPropertyEntry>(flight::types::CommandPropertyEntry{.after = flight::row_get<flight::RowKey<"value">>(entry), .before = read_node_property(flight::structural_ref_cast<flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::types::NodeAny>>>>(flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::types::NodeAny>>>(flight::row_get<flight::RowKey<"target">>(entry))), flight::row_get<flight::RowKey<"property">>(entry)), .property = flight::row_get<flight::RowKey<"property">>(entry), .target = flight::row_get<flight::RowKey<"target">>(entry)});
});
  flight::types::EntityConstruction<flight::Ref<flight::types::SetNodePropertyCommand>> out = flight::entity::allocate_entity<flight::Ref<flight::types::SetNodePropertyCommand>>();
  flight::row_set<flight::RowKey<"entries">>(out, captured);
  flight::row_set<flight::RowKey<"kind">>(out, flight::types::set_node_property_command_kind);
  flight::row_set<flight::RowKey<"label">>(out, label);
  flight::row_set<flight::RowKey<"mergeWindow">>(out, merge_window.value());
  flight::row_set<flight::RowKey<"time">>(out, time.value());
  return flight::entity::finish_entity<flight::Ref<flight::types::SetNodePropertyCommand>>(out);
}

inline void initialize_set_node_property_command_flight_value_function_initialize_u000053_et_u00004e_ode_u000050_roperty_u000043_ommand_flight_private_c5cd7774562c55d8(flight::types::EntityConstruction<flight::Ref<flight::types::SetNodePropertyCommand>> out, flight::String label, flight::types::NodeAny target, flight::String property, flight::Any value, double merge_window, double time) {
  flight::row_set<flight::RowKey<"entries">>(out, flight::Array<flight::Ref<flight::types::CommandPropertyEntry>>{flight::make_ref<flight::types::CommandPropertyEntry>(flight::types::CommandPropertyEntry{.after = value, .before = read_node_property(flight::structural_ref_cast<flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::types::NodeAny>>>>(flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::types::NodeAny>>>(target)), property), .property = property, .target = target})});
  flight::row_set<flight::RowKey<"kind">>(out, flight::types::set_node_property_command_kind);
  flight::row_set<flight::RowKey<"label">>(out, label);
  flight::row_set<flight::RowKey<"mergeWindow">>(out, merge_window);
  flight::row_set<flight::RowKey<"time">>(out, time);
}

inline flight::Ref<flight::types::SetNodePropertyCommand> create_set_node_property_command(flight::String label, flight::types::NodeAny target, flight::String property, flight::Any value, std::optional<double> merge_window = std::nullopt, std::optional<double> time = std::nullopt) {
  merge_window = merge_window.value_or(0.0);
  time = time.value_or(0.0);
  flight::types::EntityConstruction<flight::Ref<flight::types::SetNodePropertyCommand>> out = flight::entity::allocate_entity<flight::Ref<flight::types::SetNodePropertyCommand>>();
  initialize_set_node_property_command_flight_value_function_initialize_u000053_et_u00004e_ode_u000050_roperty_u000043_ommand_flight_private_c5cd7774562c55d8(out, label, target, property, value, merge_window.value(), time.value());
  return flight::entity::finish_entity<flight::Ref<flight::types::SetNodePropertyCommand>>(out);
}

} // namespace flight::command