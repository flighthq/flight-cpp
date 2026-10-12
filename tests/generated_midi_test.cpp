// Focused behavior tests for @flighthq/midi.
//
// The package is partial at a627849: two of its seven modules carry omissions, and the four headers that
// transclude them cannot compile. This exercises the two modules that emit and compile whole --
// `midiPermission` and `midiResource` -- against the cases their own upstream suites state:
//
//   * `getMidiPermission` answers a capability that throws with the `operation-failed` outcome rather
//     than rejecting, and otherwise returns the capability's own outcome object;
//   * a resource state is absent until it is retained, is stored under its own access or port, and
//     carries the operations it was retained with and empty subscription sets;
//   * an input port's state and an output port's state are distinct arms of the port-state union, with
//     the kind each was retained as, and a second retain replaces the first.
//
// The refused modules -- `midiPort` (captured referent mutation) and `midiSubscription`
// (optional-property call, contextual-union value) -- are filed as compiler requests; nothing here
// supplies them, and no test asserts their behavior.
#include <flight/executor.hpp>
#include <flight/midi/midi_permission.hpp>
#include <flight/midi/midi_resource.hpp>

#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <variant>

namespace {

int failures = 0;

bool check(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
  return condition;
}

using flight::String;
using flight::types::PermissionQueryOutcome;

// The permission union's success arm and its failure arm, as the emitter names them.
using QueryOk = flight::types::reason_state_7172771d69bee031;
using QueryFailure = flight::types::reason_5a15b37101877c7d;
using HostPermission = flight::types::HostMidiPermissionCapability;

double count_of(const flight::Set<flight::Ref<flight::types::MidiPortStateSubscription>>& values) {
  return static_cast<double>(values.size());
}

double count_of(const flight::Set<flight::Ref<flight::types::MidiInputMessageSubscription>>& values) {
  return static_cast<double>(values.size());
}

// Awaits the generated coroutine and records what it settled with, so the test can read the outcome
// after the queue drains. `rejected` distinguishes a failed task from a fulfilled failure outcome --
// the source's `catch` turns the latter into an outcome, not a rejection.
flight::Task<void> await_permission(flight::Ref<HostPermission> host,
                                    PermissionQueryOutcome& outcome,
                                    bool& rejected) {
  try {
    outcome = co_await flight::midi::get_midi_permission(host);
  } catch (...) {
    rejected = true;
  }
  co_return;
}

}  // namespace

int main() {
  const auto executor = std::make_shared<flight::QueueExecutor>();
  const flight::ExecutorScope scope(executor);

  // getMidiPermission: the capability's own outcome, then the outcome a throwing capability produces.
  {
    auto ok = flight::make_ref<QueryOk>();
    ok->reason = String("ok");
    ok->state = String("granted");

    auto host = flight::make_ref<HostPermission>();
    host->get_permission = [ok]() {
      return flight::Task<PermissionQueryOutcome>::resolve(
          PermissionQueryOutcome{std::in_place_type<flight::Ref<QueryOk>>, ok});
    };

    PermissionQueryOutcome observed;
    bool rejected = false;
    const auto driven = await_permission(host, observed, rejected);
    executor->run_until_idle();
    check(!rejected, "a query that resolves does not reject the task");
    const auto* got = std::get_if<flight::Ref<QueryOk>>(&observed);
    check(got != nullptr, "the awaited outcome keeps the arm the capability answered with");
    check(got != nullptr && *got == ok, "and it is the capability's own outcome object, not a copy");
    check(got != nullptr && (*got)->state.has_value() && (*got)->state.value() == String("granted"),
          "with its state intact");
  }

  {
    auto host = flight::make_ref<HostPermission>();
    host->get_permission = []() -> flight::Task<PermissionQueryOutcome> {
      throw std::runtime_error("capability failed");
    };

    PermissionQueryOutcome observed;
    bool rejected = false;
    const auto driven = await_permission(host, observed, rejected);
    executor->run_until_idle();
    check(!rejected, "a throwing capability is answered as an outcome, not propagated as a rejection");
    const auto* failed = std::get_if<flight::Ref<QueryFailure>>(&observed);
    check(failed != nullptr, "the outcome is the failure arm");
    check(failed != nullptr && (*failed)->reason == String("operation-failed"),
          "and it carries the source's own failure reason");
  }

  // The access resource state: absent, then retained, then replaced.
  {
    auto access = flight::make_ref<flight::types::MidiAccess>();
    check(!flight::midi::get_midi_access_resource_state(access).has_value(),
          "an access has no resource state before it is retained");

    auto operations = flight::make_ref<flight::types::MidiAccessResourceOperations>();
    flight::midi::retain_midi_access_resource_state(access, operations);
    const auto state = flight::midi::get_midi_access_resource_state(access);
    check(state.has_value(), "retain stores the access's resource state");
    if (state.has_value()) {
      check(!state.value()->disposed && !state.value()->dispose_completed,
            "a fresh state is neither disposed nor dispose-completed");
      check(!state.value()->dispose_pending.has_value(), "and has no pending disposal");
      check(state.value()->known_ports.size() == 0.0, "with no known ports");
      check(state.value()->subscriptions.size() == 0.0, "and no subscriptions");
      check(state.value()->operations == operations, "and the operations it was retained with");
    }

    // A second retain replaces the state, as the source's map write does.
    auto replacement_operations = flight::make_ref<flight::types::MidiAccessResourceOperations>();
    const auto previous = state.has_value() ? state.value() : nullptr;
    flight::midi::retain_midi_access_resource_state(access, replacement_operations);
    const auto replaced = flight::midi::get_midi_access_resource_state(access);
    check(replaced.has_value(), "retaining again stores a state");
    check(replaced.has_value() && replaced.value() != previous, "and it is a different state object");
    check(replaced.has_value() && replaced.value()->operations == replacement_operations,
          "carrying the operations of the second retain");
  }

  // Port resource states: one arm per port kind, keyed by the port value.
  {
    auto input_port = flight::make_ref<flight::types::MidiInputPort>();
    auto output_port = flight::make_ref<flight::types::MidiOutputPort>();
    const auto input_key = flight::types::MidiPort{
        std::in_place_type<flight::Ref<flight::types::MidiInputPort>>, input_port};
    const auto output_key = flight::types::MidiPort{
        std::in_place_type<flight::Ref<flight::types::MidiOutputPort>>, output_port};

    check(!flight::midi::get_midi_port_resource_state(input_key).has_value(),
          "a port has no resource state before it is retained");

    auto input_operations = flight::make_ref<flight::types::MidiInputPortResourceOperations>();
    flight::midi::retain_midi_input_port_resource_state(input_port, input_operations);
    auto output_operations = flight::make_ref<flight::types::MidiOutputPortResourceOperations>();
    flight::midi::retain_midi_output_port_resource_state(output_port, output_operations);

    const auto input_state = flight::midi::get_midi_port_resource_state(input_key);
    const auto output_state = flight::midi::get_midi_port_resource_state(output_key);
    check(input_state.has_value() && output_state.has_value(),
          "each port's state is stored under its own key");

    const auto* input_arm =
        input_state.has_value()
            ? std::get_if<flight::Ref<flight::midi::MidiInputPortResourceState>>(&input_state.value())
            : nullptr;
    check(input_arm != nullptr, "the input port stores the input arm of the port-state union");
    if (input_arm != nullptr) {
      check((*input_arm)->kind == String("input"), "and its kind says input");
      check(count_of((*input_arm)->state_subscriptions) == 0.0 &&
                count_of((*input_arm)->message_subscriptions) == 0.0,
            "with no subscriptions of either kind");
      check((*input_arm)->operations == input_operations, "and its own operations");
      check(!(*input_arm)->flight_opened, "and it is not yet flight-opened");
    }

    const auto* output_arm =
        output_state.has_value()
            ? std::get_if<flight::Ref<flight::midi::MidiOutputPortResourceState>>(&output_state.value())
            : nullptr;
    check(output_arm != nullptr, "the output port stores the output arm");
    if (output_arm != nullptr) {
      check((*output_arm)->kind == String("output"), "and its kind says output");
      check((*output_arm)->operations == output_operations, "with its own operations");
    }
    check(output_state.has_value() &&
              std::get_if<flight::Ref<flight::midi::MidiInputPortResourceState>>(&output_state.value()) ==
                  nullptr,
          "an output port is never stored as an input state");

    // The two ports are distinct keys, so neither state answers for the other.
    check(input_key != output_key, "the input and output ports are distinct port keys");

    // Retaining the input port again replaces its state.
    const auto previous = *input_arm;
    auto second_operations = flight::make_ref<flight::types::MidiInputPortResourceOperations>();
    flight::midi::retain_midi_input_port_resource_state(input_port, second_operations);
    const auto replaced = flight::midi::get_midi_port_resource_state(input_key);
    const auto* replaced_arm =
        replaced.has_value()
            ? std::get_if<flight::Ref<flight::midi::MidiInputPortResourceState>>(&replaced.value())
            : nullptr;
    check(replaced_arm != nullptr, "retaining the input port again keeps the input arm");
    check(replaced_arm != nullptr && *replaced_arm != previous, "and replaces the stored state");
    check(replaced_arm != nullptr && (*replaced_arm)->operations == second_operations,
          "with the second retain's operations");
  }

  if (failures == 0) std::cout << "generated midi: all checks passed\n";
  return failures == 0 ? 0 : 1;
}
