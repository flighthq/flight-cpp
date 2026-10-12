#include <flight/command/command.hpp>
#include <flight/command/command_history_signals.hpp>

#include <iostream>
#include <memory>
#include <string>

// Native port of the callable cases of packages/command/src/command.test.ts and commandHistorySignals.test.ts
// at a627849, against the command.hpp override and the generated command_history_signals.hpp, the two
// command headers that compile independently at this base.
//
// Excluded, with their blockers:
// - command_binding.hpp and every header that includes it (history, transaction, contract, explain
//   dispatch, index) stop in the registry row, which stays at 7e2fc7d. Its registry_table.hpp override
//   names flight::types::KeyedTable, which the a627849 types row no longer declares, and
//   registerCommandBinding calls withKindMapEntry, which only the a627849 registry exports. So
//   commandBinding, commandHistory, commandTransaction and explainCommandDispatch tests have no callable
//   surface, and neither do the two history-signal cases that execute and undo through a history.
// - createRemoveNodeChildCommand and createReorderNodeChildCommand now call the node package's
//   getNodeChildIndex, but upstream needs a parent with attached children. The node row (7e2fc7d) cannot
//   attach at Traits = flight::Any:
//   - add_node_child_at calls get_node_child_count and is_node_ancestor_of without the template argument
//     and without converting to their readonly parameters, which the current compiler already emits as
//     count<Traits>(...);
//   - it also emits a NodeOf<Traits> into a std::function<void(NodeAny)> signal.
//   Writing the runtime children array by hand would fabricate graph state.
// - createSetNodePropertyCommand "reads the current value as before" and the batch's before/after case
//   both seed a dynamic `x` on the node. Node<Any> has no keyed storage, NamedProperties::set("x")
//   returns false, and the captured `before` is undefined.
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

// Upstream starts from createCommandHistory(), whose module does not compile here (see above). These
// cases observe only onChange, which createCommandHistory leaves null and a bare CommandHistory leaves
// absent, so the bare owner is the same starting state for them.
flight::Ref<types::CommandHistory> history() { return flight::make_ref<types::CommandHistory>(); }

void test_command_history_signals() {
  {
    const auto subject = history();
    check(!subject->on_change.has_value(), "createCommandHistory leaves onChange null");
    const auto signal = command::enable_command_history_signals(subject);
    check(signal != nullptr && command::enable_command_history_signals(subject) == signal,
          "enableCommandHistorySignals allocates once and returns the same signal on a second call");
  }
  {
    const auto subject = history();
    check(!command::get_command_history_signals(subject).has_value() && !subject->on_change.has_value(),
          "getCommandHistorySignals returns null until signals are enabled, and never allocates");
    const auto signal = command::enable_command_history_signals(subject);
    const auto read = command::get_command_history_signals(subject);
    check(read.has_value() && *read == signal, "getCommandHistorySignals returns the enabled signal");
  }
}

}  // namespace

int main() {
  test_add_node_child_command();
  test_composite_command();
  test_set_node_property_command();
  test_command_history_signals();
  return failures == 0 ? 0 : 1;
}
