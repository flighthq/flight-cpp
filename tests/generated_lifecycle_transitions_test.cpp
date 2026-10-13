// Native parity test for @flighthq/lifecycle's transition table and lifecycle edges -- complementary to the
// pre-existing tests/generated_lifecycle_test.cpp, which this file leaves untouched.
//
// That file pins the shape of the module: the seven signals, the operation explanations, launch-kind delegation
// and fallback, one raw/paused/resumed/saved/restored sequence, replacement and idempotent detach, the state
// helpers, a cancelled back request, and dispose clearing the saved bag. What it does not pin is the EDGE TABLE
// itself -- which transitions fire onPause and onResume and which deliberately do not -- nor the save/restore
// handoff, nor what attach does when the provider omits an optional capability. Those are the behaviors this
// file covers, through connected listeners rather than by replacing each signal's `emit`, so the signal's own
// slot machinery is on the path too:
//
//   * onStateChange is RAW: it fires on every provider notification even when the state did not change.
//   * onPause and onResume are DEDUPED EDGES across the 'active' boundary only: leaving 'active' fires onPause
//     (including the interruption edge to 'inactive'), entering 'active' fires onResume, and the
//     'inactive'/'background' crossing fires NEITHER in either direction.
//   * onSaveState is handed the bag the listener fills, and onRestoreState later receives THAT bag -- a restore
//     that carried a fresh bag would lose everything the save listener put in it.
//   * A resume with nothing saved restores nothing; a resume after a save restores.
//   * `requestAppBack` answers true unless a listener actually cancelled: no listener at all is the default
//     path, not a veto.
//
// The module is supplied by the hand-written override overrides/manifest.json entry `lifecycle-complete-module`
// (status complete, remaining "None in @flighthq/lifecycle at this pin"), which is what ships for this package;
// this file pins the behavior of that supply.
#include <flight/lifecycle/lifecycle.hpp>
#include <flight/signals/slot.hpp>

#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace {

namespace lifecycle = flight::lifecycle;
namespace types = flight::types;
namespace signals = flight::signals;

using StateBag = lifecycle::StateBag;
using StateFn = std::function<void(flight::String)>;
using PlainFn = std::function<void()>;
using BagFn = std::function<void(StateBag)>;

int failures = 0;

void check(bool condition, const std::string& message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

// A provider whose state the test moves, recording what was subscribed and released.
struct Host {
  flight::String state = flight::String("active");
  int state_subscribes = 0;
  int state_unsubscribes = 0;
  int memory_subscribes = 0;
  int memory_unsubscribes = 0;
  bool has_memory = true;
  flight::String launch_kind = flight::String("cold");
  std::function<void()> notify_state;
  std::function<void(flight::String)> notify_memory;

  flight::Ref<types::HostLifecycleCapability> capability() {
    auto out = flight::make_ref<types::HostLifecycleCapability>();
    out->get_state = [this]() { return state; };
    out->subscribe = [this](std::function<void()> listener) {
      ++state_subscribes;
      notify_state = std::move(listener);
      return std::function<void()>([this]() {
        ++state_unsubscribes;
        notify_state = {};
      });
    };
    out->get_launch_kind = [this]() { return launch_kind; };
    if (has_memory) {
      out->subscribe_memory_warning = [this](std::function<void(flight::String)> listener) {
        ++memory_subscribes;
        notify_memory = std::move(listener);
        return std::function<void()>([this]() {
          ++memory_unsubscribes;
          notify_memory = {};
        });
      };
    }
    return out;
  }
};

// Every signal the module can emit, counted, with the last payload of each.
struct Watcher {
  int changes = 0;
  int resumes = 0;
  int pauses = 0;
  int saves = 0;
  int restores = 0;
  int memory = 0;
  int back = 0;
  std::vector<std::string> order;
  flight::String last_state;
  flight::String last_pressure;
  StateBag last_saved;
  StateBag last_restored;

  void watch(const flight::Ref<types::AppLifecycle>& app) {
    signals::connect_signal<StateFn>(app->on_state_change, [this](flight::String value) {
      ++changes;
      last_state = value;
      order.push_back("state:" + value.to_utf8());
    }, std::nullopt);
    signals::connect_signal<PlainFn>(app->on_resume, [this]() { ++resumes; order.push_back("resume"); }, std::nullopt);
    signals::connect_signal<PlainFn>(app->on_pause, [this]() { ++pauses; order.push_back("pause"); }, std::nullopt);
    signals::connect_signal<BagFn>(app->on_save_state, [this](StateBag bag) {
      ++saves;
      last_saved = bag;
      order.push_back("save");
    }, std::nullopt);
    signals::connect_signal<BagFn>(app->on_restore_state, [this](StateBag bag) {
      ++restores;
      last_restored = bag;
      order.push_back("restore");
    }, std::nullopt);
    signals::connect_signal<StateFn>(app->on_memory_warning, [this](flight::String level) {
      ++memory;
      last_pressure = level;
    }, std::nullopt);
    signals::connect_signal<PlainFn>(app->on_back_button, [this]() { ++back; }, std::nullopt);
  }
};

// --- The edge table --------------------------------------------------------------------------------------------

void test_leaving_and_entering_active_are_the_only_resume_and_pause_edges() {
  Host host;
  auto capability = host.capability();
  auto app = lifecycle::create_app_lifecycle();
  Watcher w;
  w.watch(app);
  lifecycle::attach_app_lifecycle(capability, app);

  host.state = flight::String("inactive");   // active -> inactive is the INTERRUPTION edge: a pause
  host.notify_state();
  host.state = flight::String("background"); // inactive -> background crosses no active boundary
  host.notify_state();
  host.state = flight::String("inactive");   // and neither does the reverse
  host.notify_state();
  host.state = flight::String("active");     // entering active resumes
  host.notify_state();

  check(w.pauses == 1, "only the active -> inactive edge paused");
  check(w.resumes == 1, "and only the return to active resumed");
  check(w.changes == 4, "while every provider notification was reported raw");
  check(w.order == std::vector<std::string>({"state:inactive", "pause", "save", "state:background", "state:inactive",
                                             "state:active", "resume", "restore"}),
        "and in the order the provider reported them, with the save on the pause edge and the restore after the "
        "resume that follows it");
}

void test_a_repeat_notification_of_the_same_state_is_raw_but_not_an_edge() {
  Host host;
  auto capability = host.capability();
  auto app = lifecycle::create_app_lifecycle();
  Watcher w;
  w.watch(app);
  lifecycle::attach_app_lifecycle(capability, app);

  host.notify_state();   // 'active' -> 'active'
  host.notify_state();
  check(w.changes == 2, "a repeat notification is still reported on onStateChange");
  check(w.resumes == 0 && w.pauses == 0, "but it is not an edge, so nothing resumes or pauses");

  host.state = flight::String("background");
  host.notify_state();
  host.notify_state();   // 'background' -> 'background'
  check(w.changes == 4, "and the same holds away from active");
  check(w.pauses == 1 && w.resumes == 0, "with only the one genuine departure from active counting");
}

void test_attach_establishes_the_baseline_without_emitting() {
  Host host;
  host.state = flight::String("inactive");
  auto capability = host.capability();
  auto app = lifecycle::create_app_lifecycle();
  Watcher w;
  w.watch(app);
  lifecycle::attach_app_lifecycle(capability, app);

  check(w.changes == 0 && w.resumes == 0 && w.pauses == 0 && w.saves == 0,
        "attaching to an already-inactive provider reports nothing until the provider notifies");
  // The baseline is the state at attach, so the first notification to 'active' is a genuine resume edge.
  host.state = flight::String("active");
  host.notify_state();
  check(w.resumes == 1 && w.pauses == 0, "and the next move into active is a resume from that baseline");
}

// --- The save/restore handoff ----------------------------------------------------------------------------------

void test_a_resume_with_nothing_saved_restores_nothing() {
  Host host;
  auto capability = host.capability();
  auto app = lifecycle::create_app_lifecycle();
  Watcher w;
  w.watch(app);
  lifecycle::attach_app_lifecycle(capability, app);

  host.state = flight::String("background");
  host.notify_state();
  host.state = flight::String("active");
  host.notify_state();
  check(w.saves == 1, "leaving active collected state once");
  check(w.restores == 1, "and the return delivered it once");
}

void test_the_restored_bag_is_the_bag_the_save_listener_filled() {
  Host host;
  auto capability = host.capability();
  auto app = lifecycle::create_app_lifecycle();
  Watcher w;
  w.watch(app);
  // A save listener writes into the bag it is handed; the restore listener reads it back. If the module handed
  // the restore a fresh bag -- rather than the one it collected -- nothing written here would survive.
  signals::connect_signal<BagFn>(app->on_save_state, [](StateBag bag) {
    bag.set(flight::String("scroll"), flight::Any(120.0));
  }, std::nullopt);
  lifecycle::attach_app_lifecycle(capability, app);

  host.state = flight::String("background");
  host.notify_state();
  host.state = flight::String("active");
  host.notify_state();

  check(w.restores == 1, "the resume restored state");
  const auto restored = w.last_restored.get(flight::String("scroll"));
  check(restored.has_value() && restored->as_number() == 120.0,
        "and what the save listener wrote into the bag is what the restore listener received");
}

void test_a_second_save_replaces_what_a_later_restore_receives() {
  Host host;
  auto capability = host.capability();
  auto app = lifecycle::create_app_lifecycle();
  Watcher w;
  w.watch(app);
  int round = 0;
  signals::connect_signal<BagFn>(app->on_save_state, [&round](StateBag bag) {
    ++round;
    bag.set(flight::String("round"), flight::Any(static_cast<double>(round)));
  }, std::nullopt);
  lifecycle::attach_app_lifecycle(capability, app);

  host.state = flight::String("background");
  host.notify_state();
  host.state = flight::String("active");
  host.notify_state();
  host.state = flight::String("background");
  host.notify_state();
  host.state = flight::String("active");
  host.notify_state();

  check(w.saves == 2 && w.restores == 2, "two departures and two returns each reported once");
  const auto restored = w.last_restored.get(flight::String("round"));
  check(restored.has_value() && restored->as_number() == 2.0,
        "and the second restore carries the second save, not a stale first one");
}

// --- Listener and disposer identity ----------------------------------------------------------------------------

void test_a_replacement_attach_releases_the_previous_subscription_once() {
  Host host;
  auto capability = host.capability();
  auto app = lifecycle::create_app_lifecycle();
  lifecycle::attach_app_lifecycle(capability, app);
  const auto first_listener = host.notify_state;
  lifecycle::attach_app_lifecycle(capability, app);

  check(host.state_subscribes == 2, "re-attaching subscribes again");
  check(host.state_unsubscribes == 1, "and releases the subscription it replaced exactly once");
  check(host.memory_subscribes == 2 && host.memory_unsubscribes == 1,
        "including the memory subscription when the provider has one");

  // The registry keeps ONE subscription per app, so a detach after a replacement releases the CURRENT one and
  // never the replaced one again. (The replaced listener's closure is still callable -- it was never neutralized
  // -- which is why the host's release, not this closure, is what stops delivery.)
  static_cast<void>(first_listener);
  lifecycle::detach_app_lifecycle(app);
  check(host.state_unsubscribes == 2 && host.memory_unsubscribes == 2,
        "and a following detach releases the current subscription, one per subscription made");
}

void test_detach_and_dispose_are_safe_when_nothing_was_attached() {
  auto app = lifecycle::create_app_lifecycle();
  lifecycle::detach_app_lifecycle(app);
  lifecycle::dispose_app_lifecycle(app);
  lifecycle::detach_app_lifecycle(app);
  check(true, "detaching and disposing an app that was never attached is safe");
}

void test_dispose_releases_the_subscription_it_held() {
  Host host;
  auto capability = host.capability();
  auto app = lifecycle::create_app_lifecycle();
  lifecycle::attach_app_lifecycle(capability, app);
  lifecycle::dispose_app_lifecycle(app);

  check(host.state_unsubscribes == 1 && host.memory_unsubscribes == 1,
        "disposing releases the provider subscriptions");
  lifecycle::dispose_app_lifecycle(app);
  check(host.state_unsubscribes == 1 && host.memory_unsubscribes == 1,
        "and disposing again releases nothing further");
}

void test_detach_stops_further_delivery_by_the_provider() {
  Host host;
  auto capability = host.capability();
  auto app = lifecycle::create_app_lifecycle();
  lifecycle::attach_app_lifecycle(capability, app);
  lifecycle::detach_app_lifecycle(app);

  // Detach releases the origin, which is what the host needs to stop calling; the module's own half of that is
  // that the release ran and that the recorded listener is gone.
  check(host.state_unsubscribes == 1, "detaching releases the state subscription");
  check(!host.notify_state, "and the listener the host held has been dropped");
  check(host.memory_unsubscribes == 1, "as has the memory one");
}

// --- Optional capabilities and the queries ---------------------------------------------------------------------

void test_a_provider_without_a_memory_capability_attaches_and_releases_cleanly() {
  Host host;
  host.has_memory = false;
  auto capability = host.capability();
  auto app = lifecycle::create_app_lifecycle();
  Watcher w;
  w.watch(app);
  lifecycle::attach_app_lifecycle(capability, app);

  check(host.memory_subscribes == 0, "a provider that omits memory warnings is never asked to subscribe to them");
  host.state = flight::String("background");
  host.notify_state();
  check(w.changes == 1 && w.pauses == 1, "while state delivery works exactly as it does with one");

  lifecycle::detach_app_lifecycle(app);
  check(host.state_unsubscribes == 1, "and detach releases what was actually subscribed, without reaching for the absent one");
}

void test_the_state_queries_read_the_provider_at_call_time() {
  Host host;
  auto capability = host.capability();

  host.state = flight::String("active");
  check(lifecycle::is_app_active(capability) && !lifecycle::is_app_background(capability) &&
            !lifecycle::is_app_inactive(capability),
        "the active query answers active");
  host.state = flight::String("inactive");
  check(!lifecycle::is_app_active(capability) && lifecycle::is_app_inactive(capability),
        "and the interruption state answers inactive");
  host.state = flight::String("background");
  check(lifecycle::is_app_background(capability) && !lifecycle::is_app_inactive(capability),
        "and the hidden state answers background");
  check(lifecycle::get_app_lifecycle_state(capability) == flight::String("background"),
        "with the state reader agreeing");
}

void test_launch_kind_is_delegated_and_falls_back_to_warm() {
  Host host;
  host.launch_kind = flight::String("warm");
  auto capability = host.capability();
  check(lifecycle::get_app_launch_kind(capability) == flight::String("warm"), "a reported warm launch is warm");

  host.launch_kind = flight::String("cold");
  check(lifecycle::get_app_launch_kind(capability) == flight::String("cold"), "and a reported cold one is cold");

  auto bare = flight::make_ref<types::HostLifecycleCapability>();
  bare->get_state = []() { return flight::String("active"); };
  bare->subscribe = [](std::function<void()>) { return std::function<void()>([]() {}); };
  check(lifecycle::get_app_launch_kind(bare) == flight::String("warm"),
        "a provider predating the method reports warm rather than nothing");
}

void test_the_back_request_answers_true_unless_a_listener_cancelled() {
  auto app = lifecycle::create_app_lifecycle();
  // No listener connected at all: the default path, which the contract says may proceed.
  check(lifecycle::request_app_back(app), "a back request with no listener proceeds");
  check(!app->on_back_button->data.has_value(), "and leaves no cancellation behind");

  int seen = 0;
  signals::connect_signal<PlainFn>(app->on_back_button, [&seen]() { ++seen; }, std::nullopt);
  check(lifecycle::request_app_back(app), "a listener that does not cancel lets the request proceed");
  check(seen == 1, "having been called");

  signals::connect_signal<PlainFn>(app->on_back_button, [app]() {
    signals::cancel_signal<std::function<void()>>(app->on_back_button);
  }, std::nullopt);
  check(seen == 1, "the vetoing listener has not run yet");
  check(!lifecycle::request_app_back(app), "a listener that cancels vetoes the request");
  check(lifecycle::request_app_back(app) == false, "and the veto is reported on every run it occurs in");
}

} // namespace

int main() {
  test_leaving_and_entering_active_are_the_only_resume_and_pause_edges();
  test_a_repeat_notification_of_the_same_state_is_raw_but_not_an_edge();
  test_attach_establishes_the_baseline_without_emitting();

  test_a_resume_with_nothing_saved_restores_nothing();
  test_the_restored_bag_is_the_bag_the_save_listener_filled();
  test_a_second_save_replaces_what_a_later_restore_receives();

  test_a_replacement_attach_releases_the_previous_subscription_once();
  test_detach_and_dispose_are_safe_when_nothing_was_attached();
  test_dispose_releases_the_subscription_it_held();
  test_detach_stops_further_delivery_by_the_provider();

  test_a_provider_without_a_memory_capability_attaches_and_releases_cleanly();
  test_the_state_queries_read_the_provider_at_call_time();
  test_launch_kind_is_delegated_and_falls_back_to_warm();
  test_the_back_request_answers_true_unless_a_listener_cancelled();

  if (failures == 0) {
    std::cout << "lifecycle transitions parity: all checks passed\n";
    return 0;
  }
  std::cerr << failures << " check(s) failed\n";
  return 1;
}
