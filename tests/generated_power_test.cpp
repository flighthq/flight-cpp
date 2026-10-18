// Bounded native evidence for @flighthq/power.
//
// The package has no override and no native test at this pin: three modules, three emitted, none
// refused, and the emitted slice compiles 3/3 on its own. Everything asserted below is therefore the
// emitter's own lowering of packages/power/src/power.ts, and every expectation is derived from that
// file and its upstream suite (packages/power/src/power.test.ts) rather than from the emission -- the
// bars are the source's, the values are read out of the compiled module.
//
// THE RECORD-AND-RETHROW PATH IS CARRIED, AND IS ASSERTED
//
// Both failure paths of this module are carried, by three patches that land together:
//
//   * the record and the rethrow -- compiler-nullish-assignment-statement-in-catch.patch and
//     compiler-catch-recorded-slot-carrier.patch. The slot is an `std::exception_ptr`, the first caught
//     error is recorded once, and the rethrow hands the caller the ORIGINAL exception.
//   * CATCH BREADTH, for this module's handlers -- compiler-catch-handler-breadth.patch. Both handlers
//     only REIFY the caught value (every use of the binding is the record into the exception slot), so
//     they are emitted as `catch (...)`: a value that is not a class reaches them, is reified by
//     `std::current_exception()`, and `std::rethrow_exception` hands the caller the same value. The
//     boundary that remains is a handler which READS the caught value -- C++ cannot name a non-class
//     caught value at all -- and no handler in this module does that, so this file asserts the whole
//     path, including a provider that throws a plain `int`.
//
// Three of my own expectations were wrong first and are fixed here rather than worked around: a
// change-provider fake that counted releases without removing the released listener (so a correct
// detach looked like a leak), an idle-read count that included the query calls the same block made
// itself, and an absolute release count where the block had already detached once. The first two are
// now behaviourally faithful fakes and the third is a delta; each block says so where it matters.
//
// Both live in destroyPowerKeepAwake and detachPower, and their attempt-all halves are asserted too -- a
// throwing provider's sibling is still attempted, a failed obligation is retried on the next call, and a
// succeeded one is not destroyed twice. Nothing about either function is left unasserted here.
#include <flight/power/power.hpp>

#include <chrono>
#include <iostream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

int failures = 0;

bool check(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
  return condition;
}

using flight::Ref;
using flight::String;
using flight::types::HostPowerBatteryHealthCapability;
using flight::types::HostPowerChangeCapability;
using flight::types::HostPowerIdleCapability;
using flight::types::HostPowerKeepAwakeCapability;
using flight::types::HostPowerSessionLockCapability;
using flight::types::HostPowerStatusCapability;
using flight::types::HostPowerSuspensionCapability;
using flight::types::HostPowerThermalCapability;
using flight::types::PowerBatteryHealth;
using flight::types::PowerKeepAwakeAcquireResult;
using flight::types::PowerKeepAwakeReleaseResult;
using flight::types::PowerStatus;

// The emitted capability parameters are readonly row views; a Ref converts to one, so a test can build
// an optional view the same way the emitter's own callers do.
template <typename Capability>
using View = flight::StructuralRef<flight::RowReadonly<flight::RowOf<Ref<Capability>>>>;

template <typename Capability>
std::optional<View<Capability>> view(const std::shared_ptr<Capability>& capability) {
  return std::optional<View<Capability>>{View<Capability>(capability)};
}

// connect_signal's signature is `(shared_ptr<Signal<T>>, T, options)`, so T cannot be deduced from a
// lambda; this helper names it from the signal and constructs the std::function the slot carrier is.
using StatusView = flight::StructuralRef<flight::RowReadonly<flight::RowOf<Ref<PowerStatus>>>>;

template <typename Slot, typename Callable>
void connect(const std::shared_ptr<flight::types::Signal<Slot>>& signal, Callable&& callable) {
  flight::signals::connect_signal<Slot>(signal, Slot(std::forward<Callable>(callable)));
}

Ref<PowerKeepAwakeAcquireResult> acquire_result(const char* reason) {
  auto result = flight::make_ref<PowerKeepAwakeAcquireResult>();
  result->reason = String(reason);
  return result;
}

Ref<PowerKeepAwakeReleaseResult> release_result(const char* reason) {
  auto result = flight::make_ref<PowerKeepAwakeReleaseResult>();
  result->reason = String(reason);
  return result;
}

// A keep-awake provider that records its calls, so a test can compare what the module asked for with
// what the source says it should have asked for.
struct KeepAwake {
  Ref<HostPowerKeepAwakeCapability> capability = flight::make_ref<HostPowerKeepAwakeCapability>();
  std::vector<String> modes;
  int destroys = 0;
  bool active = false;
  bool acquire_rejects = false;
};

Ref<HostPowerKeepAwakeCapability> keep_awake(const std::shared_ptr<KeepAwake>& state,
                                             std::function<void()> destroy = {}) {
  state->capability->acquire = [state](flight::types::PowerKeepAwakeMode mode) {
    state->modes.push_back(mode);
    if (state->acquire_rejects) {
      return flight::Task<Ref<PowerKeepAwakeAcquireResult>>::reject(
          flight::Error(String("the host refused the lock")));
    }
    return flight::Task<Ref<PowerKeepAwakeAcquireResult>>::resolve(acquire_result("ok"));
  };
  if (destroy) {
    state->capability->destroy = std::move(destroy);
  }
  state->capability->is_active = [state]() { return state->active; };
  state->capability->release = []() {
    return flight::Task<Ref<PowerKeepAwakeReleaseResult>>::resolve(release_result("released"));
  };
  return state->capability;
}

// A status provider whose reading the test drives.
struct Status {
  Ref<HostPowerStatusCapability> capability = flight::make_ref<HostPowerStatusCapability>();
  bool is_charging = false;
  int reads = 0;
};

Ref<HostPowerStatusCapability> status_of(const std::shared_ptr<Status>& state) {
  state->capability->get_status = [state](Ref<PowerStatus> out) {
    ++state->reads;
    out->is_charging = state->is_charging;
    return out;
  };
  return state->capability;
}

// A change provider that keeps its listeners and counts the releases the module performs.
// A change provider that keeps its listeners and counts the releases the module performs. A release
// REMOVES the listener from the set, which is what makes "the module stopped delivering" observable: a
// first version of this fake only counted releases, so a released listener kept firing and the detach
// assertions failed against a correct module.
struct Change {
  Ref<HostPowerChangeCapability> capability = flight::make_ref<HostPowerChangeCapability>();
  std::vector<std::function<void()>> listeners;
  std::vector<bool> registered;
  int releases = 0;
};

Ref<HostPowerChangeCapability> change_of(const std::shared_ptr<Change>& state) {
  state->capability->subscribe = [state](std::function<void()> listener) {
    state->listeners.push_back(std::move(listener));
    state->registered.push_back(true);
    const auto index = state->registered.size() - 1;
    return std::function<void()>{[state, index]() {
      ++state->releases;
      if (index < state->registered.size()) state->registered[index] = false;
    }};
  };
  return state->capability;
}

void fire(const std::shared_ptr<Change>& state) {
  for (std::size_t index = 0; index < state->listeners.size(); ++index) {
    if (state->registered[index]) state->listeners[index]();
  }
}

}  // namespace

int main() {
  namespace power = flight::power;

  // The value domains: the source's complete "unknown" encoding, and a fresh value per call rather than
  // one shared scratch (the two make* functions construct a literal each time).
  {
    const auto first = power::make_power_status();
    const auto second = power::make_power_status();
    check(first->battery_level == -1.0 && first->charging_time == -1.0 &&
              first->discharging_time == -1.0,
          "PowerStatus keeps the source's -1 timings");
    check(!first->is_battery_low && !first->is_charging && !first->is_low_power &&
              !first->is_on_battery,
          "and its false booleans");
    check(first->thermal_state == String("Unknown"), "and the Unknown thermal state");
    check(first != second && first->thermal_state == second->thermal_state,
          "two makePowerStatus calls are distinct values, not one shared scratch");

    const auto health = power::make_power_battery_health();
    check(health->capacity_wear_level == -1.0 && health->cycle_count == -1.0 &&
              health->temperature_celsius == -1.0 && health->voltage == -1.0 &&
              health->health_state == String("Unknown"),
          "PowerBatteryHealth keeps the source's complete unknown encoding");
    check(health != power::make_power_battery_health(),
          "and each call builds its own value");
  }

  // The polling interval is a module setting with a documented clamp: a non-positive value becomes 1,
  // never 0, which is what keeps a host from scheduling a busy loop.
  {
    check(power::get_power_idle_polling_interval_ms() == 5000.0,
          "the default polling interval is 5000ms");
    power::set_power_idle_polling_interval_ms(1234.0);
    check(power::get_power_idle_polling_interval_ms() == 1234.0, "a positive interval is kept");
    power::set_power_idle_polling_interval_ms(0.0);
    check(power::get_power_idle_polling_interval_ms() == 1.0, "zero clamps to 1");
    power::set_power_idle_polling_interval_ms(-10.0);
    check(power::get_power_idle_polling_interval_ms() == 1.0, "and so does a negative interval");
    power::set_power_idle_polling_interval_ms(5000.0);
  }

  // The keep-awake operations forward exactly what the source forwards, including the default mode and
  // a provider rejection, which the source does not catch and therefore must propagate.
  {
    const auto state = std::make_shared<KeepAwake>();
    const auto host = keep_awake(state);

    const auto explicit_result =
        power::acquire_power_keep_awake(host, String("PreventAppSuspension")).get();
    check(explicit_result->reason == String("ok"), "acquire resolves the provider's outcome");
    check(state->modes.size() == 1 && state->modes.front() == String("PreventAppSuspension"),
          "and forwards the mode it was given");

    static_cast<void>(power::acquire_power_keep_awake(host).get());
    check(state->modes.size() == 2 && state->modes.back() == String("PreventDisplaySleep"),
          "an omitted mode is the source's PreventDisplaySleep default");

    const auto released = power::release_power_keep_awake(host).get();
    check(released->reason == String("released"), "release resolves the provider's outcome");

    state->active = true;
    check(power::is_power_keep_awake_active(host), "isActive reports the provider's state");
    state->active = false;
    check(!power::is_power_keep_awake_active(host), "in both directions");

    state->acquire_rejects = true;
    bool rejected = false;
    try {
      static_cast<void>(power::acquire_power_keep_awake(host).get());
    } catch (const std::exception&) {
      rejected = true;
    }
    check(rejected, "a rejecting provider is not swallowed: the source has no catch here");
  }

  // The entity: created inert, allocated on demand, and idempotent.
  {
    const auto power_entity = power::create_power();
    check(!power_entity->on_change.has_value() && !power_entity->on_charging.has_value() &&
              !power_entity->on_discharging.has_value() && !power_entity->on_idle_state_change.has_value() &&
              !power_entity->on_lock_screen.has_value() && !power_entity->on_resume.has_value() &&
              !power_entity->on_suspend.has_value() && !power_entity->on_thermal_state_change.has_value() &&
              !power_entity->on_unlock_screen.has_value(),
          "createPower leaves all nine signals unallocated");
    power::enable_power_signals(power_entity);
    check(power_entity->on_change.has_value() && power_entity->on_suspend.has_value() &&
              power_entity->on_unlock_screen.has_value(),
          "enablePowerSignals allocates them");
    const auto kept = power_entity->on_change.value();
    power::enable_power_signals(power_entity);
    check(power_entity->on_change.value() == kept,
          "and calling it again keeps the same signal objects rather than replacing them");
  }

  // attachPower: one fresh payload per host event, the two charging edges derived from status, the
  // thermal state carried as the payload, and the idle slot consulted only when the host offers it.
  {
    const auto status = std::make_shared<Status>();
    const auto change = std::make_shared<Change>();
    const auto power_entity = power::create_power();
    power::enable_power_signals(power_entity);

    std::vector<const void*> payloads;
    // Every payload is RETAINED while identities are compared: an address is an identity only while its
    // object is alive, and a payload dropped after delivery may have its storage reused by the next one.
    std::vector<StatusView> held_payloads;
    int charged = 0;
    int discharged = 0;
    std::vector<String> thermal_states;
    connect(power_entity->on_change.value(), [&](StatusView status_view) {
      payloads.push_back(status_view.identity());
      held_payloads.push_back(status_view);
    });
    connect(power_entity->on_charging.value(), [&]() { ++charged; });
    connect(power_entity->on_discharging.value(), [&]() { ++discharged; });
    connect(power_entity->on_thermal_state_change.value(),
            [&](String state) { thermal_states.push_back(std::move(state)); });

    power::attach_power(view(status_of(status)), view(change_of(change)), std::nullopt, std::nullopt,
                        std::nullopt, std::nullopt, power_entity);
    check(status->reads == 1, "attach reads the status once to start its charging baseline");

    fire(change);
    fire(change);
    check(payloads.size() == 2, "every host event publishes on onChange");
    check(payloads[0] != payloads[1],
          "and each event carries a FRESH payload, never one buffer reused underneath its holder");

    status->is_charging = true;
    fire(change);
    {
      const auto earliest = held_payloads.front().shared_exact_object<PowerStatus>();
      const auto latest = held_payloads.back().shared_exact_object<PowerStatus>();
      check(earliest && latest && earliest != latest && !earliest->is_charging && latest->is_charging,
            "an earlier payload keeps its own snapshot after a later event publishes a different one");
    }
    fire(change);
    check(charged == 1, "a repeat of the same charging state is not an edge");
    status->is_charging = false;
    fire(change);
    check(discharged == 1, "and the reverse edge fires once");
    check(charged == 1 && discharged == 1, "in total one edge each way");

    power::detach_power(power_entity);
    const auto before = payloads.size();
    fire(change);
    check(payloads.size() == before, "detach stops delivery");

    // The thermal capability is separate from the change capability, and the payload is the state.
    const auto thermal = flight::make_ref<HostPowerThermalCapability>();
    std::vector<std::function<void(String)>> thermal_listeners;
    thermal->get_thermal_state = []() { return String("Fair"); };
    thermal->subscribe_thermal_state_change = [&thermal_listeners](std::function<void(String)> listener) {
      thermal_listeners.push_back(std::move(listener));
      return std::function<void()>{[]() {}};
    };
    power::attach_power(std::nullopt, std::nullopt, std::nullopt, std::nullopt, view(thermal),
                        std::nullopt, power_entity);
    for (auto& listener : thermal_listeners) listener(String("Critical"));
    check(thermal_states.size() == 1 && thermal_states.front() == String("Critical"),
          "the thermal event carries the state it was given");
    check(power::get_power_thermal_state(thermal) == String("Fair"),
          "and the thermal query reads the same capability");
    power::detach_power(power_entity);
  }

  // The idle slot: consulted once at attach and never for a host that does not offer it, and re-attach
  // replaces the entity's own subscription instead of stacking one.
  {
    const auto status = std::make_shared<Status>();
    const auto change = std::make_shared<Change>();
    const auto power_entity = power::create_power();
    power::enable_power_signals(power_entity);
    const auto idle = flight::make_ref<HostPowerIdleCapability>();
    int idle_reads = 0;
    idle->get_idle_state = [&idle_reads](double) {
      ++idle_reads;
      return String("Active");
    };
    idle->get_idle_time_seconds = []() { return 42.0; };
    check(power::get_power_system_idle_time(idle) == 42.0, "the idle-time query forwards");
    check(power::get_power_system_idle_state(idle, 90.0) == String("Active"),
          "the idle-state query forwards the threshold and returns the provider's state");

    // A host with no idle slot polls nothing: the source's interval exists only when the slot does.
    // The counter already includes the query calls above, so the assertion is on the DELTA.
    const int reads_before_attach = idle_reads;
    power::attach_power(view(status_of(status)), view(change_of(change)), std::nullopt, std::nullopt,
                        std::nullopt, std::nullopt, power_entity);
    check(idle_reads == reads_before_attach, "a host with no idle slot consults no idle provider");
    power::detach_power(power_entity);

    // With the slot, attach takes one reading for its baseline; the interval is raised so no host timer
    // can fire during the test, and dispose cancels it.
    power::set_power_idle_polling_interval_ms(600000.0);
    power::attach_power(view(status_of(status)), view(change_of(change)), std::nullopt, std::nullopt,
                        std::nullopt, view(idle), power_entity);
    check(idle_reads == reads_before_attach + 1,
          "a host with the slot is read once more, for the baseline");

    int deliveries = 0;
    connect(power_entity->on_change.value(), [&](StatusView) { ++deliveries; });
    fire(change);
    check(deliveries == 1, "one delivery after one attach");
    const int releases_before_reattach = change->releases;
    power::attach_power(view(status_of(status)), view(change_of(change)), std::nullopt, std::nullopt,
                        std::nullopt, std::nullopt, power_entity);
    // The count is a DELTA: this block has already detached once above, so an absolute number would be
    // asserting the block's history rather than what the re-attach did.
    check(change->releases == releases_before_reattach + 1,
          "re-attaching releases the subscription it replaces");
    fire(change);
    check(deliveries == 2, "and the entity holds exactly one live subscription afterwards");
    power::set_power_idle_polling_interval_ms(5000.0);
    power::dispose_power(power_entity);
    fire(change);
    check(deliveries == 2, "dispose detaches, so the entity stops delivering");

    // Detaching one entity never ends another's subscription.
    const auto kept = power::create_power();
    const auto dropped = power::create_power();
    power::enable_power_signals(kept);
    const auto kept_change = std::make_shared<Change>();
    int kept_deliveries = 0;
    connect(kept->on_change.value(), [&](StatusView) { ++kept_deliveries; });
    power::attach_power(view(status_of(status)), view(change_of(kept_change)), std::nullopt, std::nullopt,
                        std::nullopt, std::nullopt, kept);
    power::attach_power(view(status_of(status)), view(change_of(change)), std::nullopt, std::nullopt,
                        std::nullopt, std::nullopt, dropped);
    power::detach_power(dropped);
    fire(kept_change);
    check(kept_deliveries == 1, "detaching one entity leaves another's subscription alone");

    // Detach is safe on an entity that was never attached.
    power::detach_power(power::create_power());

    // Dispose clears the signals themselves: after it, a delivery attempt reaches nobody.
    int after_dispose = 0;
    connect(kept->on_change.value(), [&](StatusView) { ++after_dispose; });
    power::dispose_power(kept);
    fire(kept_change);
    check(after_dispose == 0 && kept_deliveries == 1,
          "dispose clears the entity's signals, so a later connection receives nothing");

    // The SIGNALS THEMSELVES are cleared, observed independently of the detach -- because the check above
    // cannot see it. `fire` reaches the entity only through the host registration that dispose removed, so
    // it reaches nobody whether or not the signals were cleared: removing ALL NINE clearSignal calls from
    // disposePower leaves that assertion passing. Connecting a listener is what makes a signal carry data,
    // and clearing is what takes it away, so the pair below is the clearing and nothing else.
    // The emission below goes STRAIGHT at the signal, not through the host, so no detach can mask it: the
    // listener is connected before dispose and the same emission is made after it.
    int direct = 0;
    const auto cleared = power::create_power();
    power::enable_power_signals(cleared);
    connect(cleared->on_change.value(), [&direct](StatusView) { ++direct; });
    flight::signals::emit_signal<std::function<void(StatusView)>>(
        cleared->on_change.value(), StatusView(flight::make_ref<PowerStatus>()));
    if (!check(direct == 1, "a direct emission reaches the listener connected before dispose")) {
      return 1;
    }
    power::dispose_power(cleared);
    flight::signals::emit_signal<std::function<void(StatusView)>>(
        cleared->on_change.value(), StatusView(flight::make_ref<PowerStatus>()));
    if (!check(direct == 1,
               "and the SAME direct emission reaches nobody after dispose, so the signal itself was cleared")) {
      return 1;
    }
  }

  // destroyPowerKeepAwake: alias-safe final release. The two hosts below genuinely share one provider,
  // which is the property the source argues for -- destroying it twice would double-release an OS lock.
  // ASSERTED: each distinct provider is destroyed once. NOT ASSERTED: the rethrow of a failure, which
  // the emission drops (see the header).
  {
    const auto shared = std::make_shared<KeepAwake>();
    const auto provider = keep_awake(shared, [shared]() { ++shared->destroys; });
    flight::Array<View<HostPowerKeepAwakeCapability>> hosts = flight::Array<View<HostPowerKeepAwakeCapability>>{};
    hosts.push(View<HostPowerKeepAwakeCapability>(provider));
    hosts.push(View<HostPowerKeepAwakeCapability>(provider));
    power::destroy_power_keep_awake(hosts);
    check(shared->destroys == 1, "two hosts aliasing one provider release it exactly once");

    const auto other = std::make_shared<KeepAwake>();
    const auto second = keep_awake(other, [other]() { ++other->destroys; });
    flight::Array<View<HostPowerKeepAwakeCapability>> distinct =
        flight::Array<View<HostPowerKeepAwakeCapability>>{};
    distinct.push(View<HostPowerKeepAwakeCapability>(provider));
    distinct.push(View<HostPowerKeepAwakeCapability>(second));
    power::destroy_power_keep_awake(distinct);
    check(shared->destroys == 1 && other->destroys == 1,
          "a provider already released is not destroyed again, and a new one is");

    // The two correct halves of the attempt-all contract: a throwing provider does not strand its
    // sibling, and it is RETAINED so the next call retries only the failures.
    const auto flaky = std::make_shared<KeepAwake>();
    int attempts = 0;
    // Throws a class on its first attempt only, so the retry is observable, the recorded error is
    // rethrown to the caller, and the handler's narrowed form still catches it (the defect this file
    // excludes is the NON-class throw, which it does not use).
    const auto flaky_provider = keep_awake(flaky, [&attempts]() {
      ++attempts;
      if (attempts == 1) throw flight::Error(String("transient"));
    });
    const auto healthy = std::make_shared<KeepAwake>();
    const auto healthy_provider = keep_awake(healthy, [healthy]() { ++healthy->destroys; });
    flight::Array<View<HostPowerKeepAwakeCapability>> mixed =
        flight::Array<View<HostPowerKeepAwakeCapability>>{};
    mixed.push(View<HostPowerKeepAwakeCapability>(flaky_provider));
    mixed.push(View<HostPowerKeepAwakeCapability>(healthy_provider));

    String rethrown;
    try {
      power::destroy_power_keep_awake(mixed);
    } catch (const std::exception& error) {
      rethrown = String(error.what());
    }
    check(rethrown == String("transient"),
          "the failure the loop recorded is rethrown to the caller, as the original exception");
    check(attempts == 1 && healthy->destroys == 1,
          "and it does not strand its sibling, which is destroyed once");

    bool retried_without_failing = true;
    try {
      power::destroy_power_keep_awake(mixed);
    } catch (const std::exception&) {
      retried_without_failing = false;
    }
    check(retried_without_failing, "the retry that succeeds rethrows nothing");
    check(attempts == 2 && healthy->destroys == 1,
          "the failed obligation is retried while the succeeded one is never released twice");

    // FIRST-FAILURE RETENTION, directly: two providers fail, and the error the caller sees is the first
    // one caught, not the last. That is `??=`'s rule -- assign only while nothing is recorded.
    const auto first = std::make_shared<KeepAwake>();
    const auto second_failing = std::make_shared<KeepAwake>();
    const auto first_provider = keep_awake(first, []() { throw flight::Error(String("the first failure")); });
    const auto second_provider =
        keep_awake(second_failing, []() { throw flight::Error(String("the second failure")); });
    flight::Array<View<HostPowerKeepAwakeCapability>> both_failing =
        flight::Array<View<HostPowerKeepAwakeCapability>>{};
    both_failing.push(View<HostPowerKeepAwakeCapability>(first_provider));
    both_failing.push(View<HostPowerKeepAwakeCapability>(second_provider));
    String retained;
    try {
      power::destroy_power_keep_awake(both_failing);
    } catch (const std::exception& error) {
      retained = String(error.what());
    }
    check(retained == String("the first failure"),
          "with two failing providers the FIRST error is the one rethrown");

    // A throw that is NOT a class. C++ has a value channel for it, and the handler catches everything
    // because its binding is only reified, so the sibling is attempted and the caller catches the same
    // `int` the provider threw. Before the catch-handler-breadth patch the value escaped the handler,
    // the sibling was skipped, and this call returned without throwing at all.
    const auto throws_number = std::make_shared<KeepAwake>();
    const auto sibling = std::make_shared<KeepAwake>();
    const auto number_provider = keep_awake(throws_number, []() { throw 42; });
    const auto sibling_provider = keep_awake(sibling, [sibling]() { ++sibling->destroys; });
    flight::Array<View<HostPowerKeepAwakeCapability>> primitive_mixed =
        flight::Array<View<HostPowerKeepAwakeCapability>>{};
    primitive_mixed.push(View<HostPowerKeepAwakeCapability>(number_provider));
    primitive_mixed.push(View<HostPowerKeepAwakeCapability>(sibling_provider));
    bool caught_number = false;
    int number_value = 0;
    try {
      power::destroy_power_keep_awake(primitive_mixed);
    } catch (int value) {
      caught_number = true;
      number_value = value;
    } catch (...) {
    }
    check(caught_number && number_value == 42,
          "a provider that throws a value which is not a class is caught, reified and rethrown as it was");
    check(sibling->destroys == 1, "and its sibling is still attempted");

    // Retention is type-agnostic: the first failure is kept whether it is a class or not. Here the FIRST
    // throw is the int and the second is an exception, and the caller must see the int.
    const auto first_number = std::make_shared<KeepAwake>();
    const auto second_class = std::make_shared<KeepAwake>();
    const auto first_number_provider = keep_awake(first_number, []() { throw 7; });
    const auto second_class_provider = keep_awake(second_class, []() { throw flight::Error(String("later")); });
    flight::Array<View<HostPowerKeepAwakeCapability>> mixed_kinds =
        flight::Array<View<HostPowerKeepAwakeCapability>>{};
    mixed_kinds.push(View<HostPowerKeepAwakeCapability>(first_number_provider));
    mixed_kinds.push(View<HostPowerKeepAwakeCapability>(second_class_provider));
    bool kept_first_kind = false;
    int kept_number = 0;
    try {
      power::destroy_power_keep_awake(mixed_kinds);
    } catch (int value) {
      kept_first_kind = true;
      kept_number = value;
    } catch (...) {
    }
    check(kept_first_kind && kept_number == 7,
          "the retained failure is the FIRST one whatever kind it is, not the class that followed it");
  }

  // detachPower: the attempt-all half of teardown, and the endpoints its failures must not lose. The
  // rethrow is deliberately not asserted (see the header).
  {
    const auto session_lock = flight::make_ref<HostPowerSessionLockCapability>();
    bool unlock_torn_down = false;
    // The source hands each subscribe* a listener and keeps the teardown it answers with; this case
    // only needs the teardowns, so the listener parameter is accepted and not stored.
    session_lock->subscribe_lock = [](std::function<void()>) {
      return std::function<void()>{[]() { throw flight::Error(String("the lock teardown failed")); }};
    };
    session_lock->subscribe_unlock = [&unlock_torn_down](std::function<void()>) {
      return std::function<void()>{[&unlock_torn_down]() { unlock_torn_down = true; }};
    };
    const auto power_entity = power::create_power();
    power::attach_power(std::nullopt, std::nullopt, view(session_lock), std::nullopt, std::nullopt,
                        std::nullopt, power_entity);
    String teardown_failure;
    try {
      power::detach_power(power_entity);
    } catch (const std::exception& error) {
      teardown_failure = String(error.what());
    }
    check(teardown_failure == String("the lock teardown failed"),
          "detach rethrows the first teardown failure to its caller");
    check(unlock_torn_down,
          "and it does not strand the teardowns after it in the same entity");
  }

  // --- Cases added by the mutation battery, which found three holes --------------------------------
  //
  // A 17-mutation battery run against this file killed 11 and SURVIVED three, and each survivor is a
  // genuine hole rather than an equivalence: dropping `subscriptions.erase(power)` from detachPower,
  // emptying the emission inside emitSignalWhenPresent, and dropping one signal slot from
  // initializePower all went unnoticed. Each block below closes one, and says which.
  {
    const auto by_constructor = power::create_power();
    check(!by_constructor->on_change.has_value() && !by_constructor->on_charging.has_value() &&
              !by_constructor->on_discharging.has_value() && !by_constructor->on_idle_state_change.has_value() &&
              !by_constructor->on_lock_screen.has_value() && !by_constructor->on_resume.has_value() &&
              !by_constructor->on_suspend.has_value() && !by_constructor->on_thermal_state_change.has_value() &&
              !by_constructor->on_unlock_screen.has_value(),
          "initializePower clears all NINE signal slots, so a fresh entity has no live listener on any of them");
  }

  {
    // getPowerBatteryHealth and getPowerStatus: caller-owned out, filled and returned by identity.
    const auto battery_provider = flight::make_ref<HostPowerBatteryHealthCapability>();
    battery_provider->get_battery_health = [](Ref<PowerBatteryHealth> out) {
      out->health_state = String("Good");
      out->cycle_count = 412;
      out->voltage = 3.87;
      return out;
    };
    const auto battery_out = power::make_power_battery_health();
    const auto battery_returned = power::get_power_battery_health(view(battery_provider).value(), battery_out);
    check(battery_returned.get() == battery_out.get(),
          "getPowerBatteryHealth returns the caller's own object, not a copy");
    check(battery_out->health_state == String("Good") && battery_out->cycle_count == 412 &&
              battery_out->voltage == 3.87,
          "and the provider's values land on it, replacing the -1 sentinels it was asked to fill");

    const auto status_provider = flight::make_ref<HostPowerStatusCapability>();
    status_provider->get_status = [](Ref<PowerStatus> out) {
      out->battery_level = 0.42;
      out->is_charging = true;
      out->is_on_battery = false;
      out->thermal_state = String("Nominal");
      return out;
    };
    const auto status_out = power::make_power_status();
    const auto status_returned = power::get_power_status(view(status_provider).value(), status_out);
    check(status_returned.get() == status_out.get(), "getPowerStatus returns the caller's own object too");
    check(status_out->battery_level == 0.42 && status_out->is_charging && !status_out->is_on_battery &&
              status_out->thermal_state == String("Nominal"),
          "with the provider's values, so no field keeps the sentinel it started at");
  }

  {
    // The lock stream: the emission really reaches a listener, and detaching FORGETS the registration.
    const auto session_lock = flight::make_ref<HostPowerSessionLockCapability>();
    int lock_teardowns = 0;
    std::function<void()> lock_listener;
    session_lock->subscribe_lock = [&](std::function<void()> listener) {
      lock_listener = std::move(listener);
      return std::function<void()>{[&lock_teardowns]() { ++lock_teardowns; }};
    };
    // attachPower subscribes BOTH session-lock streams when the provider is present, so the unlock one
    // has to answer too -- an unwired std::function here is a bad_function_call, not a no-op.
    session_lock->subscribe_unlock = [](std::function<void()>) { return std::function<void()>{[]() {}}; };
    const auto entity = power::create_power();
    power::enable_power_signals(entity);
    int locked = 0;
    connect(entity->on_lock_screen.value(), [&locked]() { ++locked; });

    power::attach_power(std::nullopt, std::nullopt, view(session_lock), std::nullopt, std::nullopt, std::nullopt,
                        entity);
    check(static_cast<bool>(lock_listener), "attaching hands the lock provider a listener to call");
    if (lock_listener) lock_listener();
    check(locked == 1, "and calling it reaches the onLockScreen signal, which is the emission the store carries");

    power::detach_power(entity);
    check(lock_teardowns == 1, "detaching runs the teardown the provider answered with");
    power::detach_power(entity);
    check(lock_teardowns == 1,
          "and a SECOND detach runs nothing, because the registration was forgotten rather than replayed");
  }

  {
    // The FIRST teardown failure is the one the caller sees, and that needs TWO failing teardowns to
    // show: with a single thrower the first and the last failure are the same object, so the existing
    // detach case cannot tell "kept the first" from "kept the last". Both lock streams fail here, with
    // distinguishable messages, and the caller must see the one that ran FIRST.
    const auto session_lock = flight::make_ref<HostPowerSessionLockCapability>();
    session_lock->subscribe_lock = [](std::function<void()>) {
      return std::function<void()>{[]() { throw flight::Error(String("the lock failed first")); }};
    };
    session_lock->subscribe_unlock = [](std::function<void()>) {
      return std::function<void()>{[]() { throw flight::Error(String("the unlock failed second")); }};
    };
    const auto entity = power::create_power();
    power::attach_power(std::nullopt, std::nullopt, view(session_lock), std::nullopt, std::nullopt, std::nullopt,
                        entity);
    String kept;
    try {
      power::detach_power(entity);
    } catch (const std::exception& error) {
      kept = String(error.what());
    }
    check(kept == String("the lock failed first"),
          "detach rethrows the FIRST teardown failure when a later one also throws, not the last");
  }

  // --- Additive parity re-audit: exact mutation survivors -------------------------------------------
  //
  // The authoritative 701-line suite above was frozen before a second exact-diff battery. Seventeen of
  // 34 valid mutations survived; every check below answers one or more of those survivors, and nothing
  // here changes or narrows an existing assertion.
  {
    // Omitting or replacing onIdleStateChange/onResume survived because the old allocation check named
    // only three signals and kept only onChange's identity. Pin the complete nine-signal contract.
    const auto entity = power::create_power();
    power::enable_power_signals(entity);
    check(entity->on_change.has_value() && entity->on_charging.has_value() &&
              entity->on_discharging.has_value() && entity->on_idle_state_change.has_value() &&
              entity->on_lock_screen.has_value() && entity->on_resume.has_value() &&
              entity->on_suspend.has_value() && entity->on_thermal_state_change.has_value() &&
              entity->on_unlock_screen.has_value(),
          "enablePowerSignals allocates all nine signals, including idle and resume");
    const auto on_change = entity->on_change.value();
    const auto on_charging = entity->on_charging.value();
    const auto on_discharging = entity->on_discharging.value();
    const auto on_idle_state_change = entity->on_idle_state_change.value();
    const auto on_lock_screen = entity->on_lock_screen.value();
    const auto on_resume = entity->on_resume.value();
    const auto on_suspend = entity->on_suspend.value();
    const auto on_thermal_state_change = entity->on_thermal_state_change.value();
    const auto on_unlock_screen = entity->on_unlock_screen.value();
    power::enable_power_signals(entity);
    check(entity->on_change.value() == on_change && entity->on_charging.value() == on_charging &&
              entity->on_discharging.value() == on_discharging &&
              entity->on_idle_state_change.value() == on_idle_state_change &&
              entity->on_lock_screen.value() == on_lock_screen && entity->on_resume.value() == on_resume &&
              entity->on_suspend.value() == on_suspend &&
              entity->on_thermal_state_change.value() == on_thermal_state_change &&
              entity->on_unlock_screen.value() == on_unlock_screen,
          "enablePowerSignals is idempotent for every signal, not just onChange");
  }

  {
    // Both threshold lanes and the timer callback survived independently. Drive the query once, then
    // attach with a non-default threshold and pump the native timer until its changed-state event runs.
    const auto idle = flight::make_ref<HostPowerIdleCapability>();
    std::vector<double> thresholds;
    String state("Active");
    idle->get_idle_state = [&thresholds, &state](double threshold) {
      thresholds.push_back(threshold);
      return state;
    };
    idle->get_idle_time_seconds = []() { return 0.0; };
    check(power::get_power_system_idle_state(idle, 91.0) == String("Active") &&
              thresholds.size() == 1 && thresholds.front() == 91.0,
          "getPowerSystemIdleState forwards its caller's exact threshold");

    thresholds.clear();
    const auto entity = power::create_power();
    power::enable_power_signals(entity);
    int idle_events = 0;
    connect(entity->on_idle_state_change.value(), [&idle_events]() { ++idle_events; });
    power::set_power_idle_polling_interval_ms(1.0);
    power::attach_power(std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, view(idle),
                        entity, 37.0);
    check(thresholds.size() == 1 && thresholds.front() == 37.0,
          "attachPower forwards its custom idle threshold to the baseline read");
    state = String("Idle");
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(100);
    while (idle_events == 0 && std::chrono::steady_clock::now() < deadline) {
      static_cast<void>(flight::host::pump_timers());
    }
    check(idle_events == 1, "a changed polled idle state reaches onIdleStateChange");
    bool polled_with_custom_threshold = thresholds.size() >= 2;
    for (const double threshold : thresholds) polled_with_custom_threshold &= threshold == 37.0;
    check(polled_with_custom_threshold,
          "the timer callback keeps the custom idle threshold instead of substituting the default");
    power::detach_power(entity);
    power::set_power_idle_polling_interval_ms(5000.0);
  }

  {
    // The previous lock-stream case observed onLockScreen and both lock teardowns, but no assertion
    // invoked onUnlockScreen, onSuspend, or onResume. Each subscription and each emission survived.
    const auto session_lock = flight::make_ref<HostPowerSessionLockCapability>();
    std::function<void()> unlock_listener;
    session_lock->subscribe_lock = [](std::function<void()>) { return std::function<void()>{[]() {}}; };
    session_lock->subscribe_unlock = [&unlock_listener](std::function<void()> listener) {
      unlock_listener = std::move(listener);
      return std::function<void()>{[]() {}};
    };
    const auto suspension = flight::make_ref<HostPowerSuspensionCapability>();
    std::function<void()> suspend_listener;
    std::function<void()> resume_listener;
    suspension->subscribe_suspend = [&suspend_listener](std::function<void()> listener) {
      suspend_listener = std::move(listener);
      return std::function<void()>{[]() {}};
    };
    suspension->subscribe_resume = [&resume_listener](std::function<void()> listener) {
      resume_listener = std::move(listener);
      return std::function<void()>{[]() {}};
    };
    const auto entity = power::create_power();
    power::enable_power_signals(entity);
    int unlocked = 0;
    int suspended = 0;
    int resumed = 0;
    connect(entity->on_unlock_screen.value(), [&unlocked]() { ++unlocked; });
    connect(entity->on_suspend.value(), [&suspended]() { ++suspended; });
    connect(entity->on_resume.value(), [&resumed]() { ++resumed; });
    power::attach_power(std::nullopt, std::nullopt, view(session_lock), view(suspension), std::nullopt,
                        std::nullopt, entity);
    check(static_cast<bool>(unlock_listener) && static_cast<bool>(suspend_listener) &&
              static_cast<bool>(resume_listener),
          "attachPower registers the unlock, suspend, and resume streams");
    if (unlock_listener) unlock_listener();
    if (suspend_listener) suspend_listener();
    if (resume_listener) resume_listener();
    check(unlocked == 1 && suspended == 1 && resumed == 1,
          "unlock, suspend, and resume provider events reach their matching signals");
    power::detach_power(entity);
  }

  {
    // Clearing onChange masked a missing detach, and it said nothing about the three other clear calls
    // whose removal survived. Observe the provider release separately, then emit straight at those
    // three signals on both sides of disposePower.
    const auto change = std::make_shared<Change>();
    const auto entity = power::create_power();
    power::enable_power_signals(entity);
    int resumed = 0;
    int unlocked = 0;
    int thermal = 0;
    connect(entity->on_resume.value(), [&resumed]() { ++resumed; });
    connect(entity->on_unlock_screen.value(), [&unlocked]() { ++unlocked; });
    connect(entity->on_thermal_state_change.value(), [&thermal](String) { ++thermal; });
    power::attach_power(std::nullopt, view(change_of(change)), std::nullopt, std::nullopt, std::nullopt,
                        std::nullopt, entity);
    flight::signals::emit_signal<std::function<void()>>(entity->on_resume.value());
    flight::signals::emit_signal<std::function<void()>>(entity->on_unlock_screen.value());
    flight::signals::emit_signal<std::function<void(String)>>(entity->on_thermal_state_change.value(),
                                                              String("Nominal"));
    check(resumed == 1 && unlocked == 1 && thermal == 1,
          "the resume, unlock, and thermal listeners are live before disposePower");
    power::dispose_power(entity);
    check(change->releases == 1, "disposePower releases its host subscription rather than only clearing signals");
    flight::signals::emit_signal<std::function<void()>>(entity->on_resume.value());
    flight::signals::emit_signal<std::function<void()>>(entity->on_unlock_screen.value());
    flight::signals::emit_signal<std::function<void(String)>>(entity->on_thermal_state_change.value(),
                                                              String("Critical"));
    check(resumed == 1 && unlocked == 1 && thermal == 1,
          "disposePower clears the resume, unlock, and thermal signals themselves");
  }

  if (failures == 0) std::cout << "generated power: all checks passed\n";
  return failures == 0 ? 0 : 1;
}
