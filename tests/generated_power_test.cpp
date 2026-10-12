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
// Two things were broken in this module's failure paths and only one is fixed at this pin:
//
//   * the record and the rethrow, carried by
//     source-patches/compiler-nullish-assignment-statement-in-catch.patch and
//     source-patches/compiler-catch-recorded-slot-carrier.patch. The slot is an `std::exception_ptr`,
//     the first caught error is recorded once, and the rethrow hands the caller the ORIGINAL exception.
//     This file asserts that, including a direct first-failure-retention case with two failing
//     providers: asserting only the attempt-all halves was how the previous version recorded the defect
//     without certifying it.
//   * CATCH BREADTH, which is NOT fixed. `catch (error)` is lowered to
//     `catch (const std::exception&)`, so a thrown value that is not a class escapes a handler whose
//     body continues -- with a provider whose destroy throws `42`, the sibling provider is never
//     attempted. Filed as docs/compiler-requests/catch-handler-breadth-attempt-all.md, and this file
//     therefore throws only class values and asserts nothing about a non-class one.
//
// Three of my own expectations were wrong first and are fixed here rather than worked around: a
// change-provider fake that counted releases without removing the released listener (so a correct
// detach looked like a leak), an idle-read count that included the query calls the same block made
// itself, and an absolute release count where the block had already detached once. The first two are
// now behaviourally faithful fakes and the third is a delta; each block says so where it matters.
//
// Both live in destroyPowerKeepAwake and detachPower. Their attempt-all halves are asserted too -- a
// throwing provider's sibling is still attempted, a failed obligation is retried on the next call, and a
// succeeded one is not destroyed twice -- so what is left unasserted here is exactly the catch breadth.
#include <flight/power/power.hpp>

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
    int charged = 0;
    int discharged = 0;
    std::vector<String> thermal_states;
    connect(power_entity->on_change.value(), [&](StatusView status_view) {
      payloads.push_back(status_view.identity());
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

  if (failures == 0) std::cout << "generated power: all checks passed\n";
  return failures == 0 ? 0 : 1;
}
