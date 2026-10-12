#include <flight/command/command.hpp>

#include <iostream>
#include <memory>
#include <string>

// Native port of the callable cases of packages/command/src/command.test.ts at the command row's source
// revision (7e2fc7d), against the command.hpp override -- the only command header that compiles
// independently at this base.
//
// Excluded, with their blockers:
// - The other seven command headers (binding, history, history signals, transaction, contract, explain
//   dispatch, index) fail to compile. The command row is generated at 7e2fc7d and forward-declares
//   `struct CommandBindingTable`, while flight/types/command.hpp is generated at a627849, where it is
//   `using CommandBindingTable = flight::Map<...>`. That is a typedef redefinition in history, history
//   signals and transaction. The command_binding.hpp override assigns `out->entries` and fails because
//   entries is a member function of flight::Map. Both clear only when the command row is regenerated
//   at a627849 and the drifted overrides are re-derived. So commandBinding, commandHistory,
//   commandHistorySignals, commandTransaction and explainCommandDispatch tests have no callable surface.
// - createRemoveNodeChildCommand and createReorderNodeChildCommand need a parent with attached children.
//   The node package cannot build or attach a command NodeAny (Node<flight::Any>):
//   - create_node and initialize_node do not instantiate at Traits = flight::Any (EntityConstruction of
//     the merged row does not convert to EntityConstruction<std::shared_ptr<Node<Any>>>);
//   - add_node_child at Any fails in get_node_child_count, is_node_ancestor_of and emit_signal;
//   - a merged row cannot be recovered as a native reference.
//   Writing the runtime children array by hand would fabricate graph state.
// - createSetNodePropertyCommand "reads the current value as before" and the batch's before/after case
//   both seed a dynamic `x` on the node. Node<Any> has no keyed storage, NamedProperties::set("x")
//   returns false, and the captured `before` is absent. See
//   docs/compiler-requests/structural-row-erasure-to-unknown.md for the refused readNodeProperty.
// - Object.hasOwn(command, EntityRuntimeKey) has no native oracle, because entity_runtime_key is a
//   declared member of every Command. The initialize* `typeof === 'function'` cases are established by
//   compilation alone.

namespace {

using flight::String;
using flight::types::NodeAny;
namespace command = flight::command;
namespace types = flight::types;

int failures = 0;

void check(bool condition, const std::string& message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

// Upstream builds its subject with `createNode('test.CommandTarget') as NodeAny`. The node package cannot
// construct that carrier here: create_node, initialize_node and add_node_child do not instantiate at
// Traits = flight::Any (see the blockers document), so these cases use a bare Node<Any> owner. Every case
// kept below observes only the identity of the node it was given, never its graph state.
NodeAny node() { return std::make_shared<types::Node<flight::Any>>(); }

void test_add_node_child_command() {
  const auto parent = node();
  const auto child = node();
  const auto command = command::create_add_node_child_command(String("Add sprite"), parent, child);
  check(command->child == child && command->index == -1 && command->kind == types::add_node_child_command_kind &&
            command->label == String("Add sprite") && command->parent == parent,
        "createAddNodeChildCommand carries its kind, and appends by default");
  check(command::create_add_node_child_command(String("Add"), node(), node(), 3.0)->index == 3, "createAddNodeChildCommand records an explicit insertion index");
}

void test_composite_command() {
  flight::Array<flight::Ref<types::Command>> children;
  children.push(command::create_add_node_child_command(String("Add"), node(), node()));
  const auto command = command::create_composite_command(String("Align"), children);
  children.push(command::create_add_node_child_command(String("Add again"), node(), node()));
  check(command->kind == types::composite_command_kind && command->children.size() == 1,
        "createCompositeCommand copies the child list, so a later push by the caller cannot rewrite a recorded entry");
}

void test_set_node_property_command() {
  const auto command = command::create_set_node_property_command(String("Move"), node(), String("x"), flight::Any(1.0));
  check(command->kind == types::set_node_property_command_kind && command->merge_window == 0 && command->time == 0,
        "createSetNodePropertyCommand defaults to no merge window, so every change is its own undo entry");
}

}  // namespace

int main() {
  test_add_node_child_command();
  test_composite_command();
  test_set_node_property_command();
  return failures == 0 ? 0 : 1;
}
