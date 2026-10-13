// Native parity test for @flighthq/app's core, ported from app.test.ts and appHost.test.ts.
//
// The core is a bag of one-line delegations plus ONE piece of real machinery: the application event entity, which
// subscribes six host providers to six signals and has to release and replace them correctly.
//
//   * `createAppEvents` allocates the entity that carries the six signals; nothing else in the module holds state.
//   * Each `attachApp*` family routes one host provider to one signal, and `attachAppQuitRequest` is the only one
//     that carries a value in both directions: the host hands it a cancel callback, and a listener that cancels the
//     signal must call that callback -- that is how a JS-side veto reaches the native quit.
//   * `attachAppEvents` attaches all six at once and REPLACES a prior attachment, so every provider's release runs
//     exactly once.
//   * `disposeAppEvents` is teardown in the full sense: it releases the providers AND clears the listeners, so a
//     provider that fires afterwards reaches nobody.
//   * Everything else is a delegation, and what is worth pinning about a delegation is that it reaches the provider
//     it was handed and NO other -- which is the whole of appHost.test.ts's subject, and is asserted here for the
//     commands, the queries, the badge task, the single-instance lock and the recent-documents pair.
//
// One appHost case asserts an ABSENCE -- that the module exports no ambient backend API (`setAppBackend` and five
// more). C++ has no export list to inspect, so that case is covered by its behavioural content instead: the only
// provider a call can reach is the one passed in, asserted by handing two providers of the same kind and showing
// that only the selected one is touched. That is a translation of the case, and it is labelled as such.
#include <flight/app/app.hpp>
#include <flight/signals/slot.hpp>

#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace {

namespace app = flight::app;
namespace types = flight::types;
namespace signals = flight::signals;

using Events = types::AppEvents;
using PlainFn = std::function<void()>;
using StringFn = std::function<void(flight::String)>;
using ArgsFn = std::function<void(flight::Array<flight::String>)>;

int failures = 0;

void check(bool condition, const std::string& message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

std::string utf8(const flight::String& value) { return value.to_utf8(); }

// The six event providers a fixture needs, each remembering its listener and counting its release.
struct EventsHost {
  std::vector<std::string> calls;
  std::function<void()> activate;
  std::function<void()> all_windows_closed;
  std::function<void(flight::String)> open_file;
  std::function<void(std::function<void()>)> quit_request;
  std::function<void()> ready;
  std::function<void(flight::Array<flight::String>)> second_instance;
  int releases = 0;

  flight::Ref<types::HostAppActivateCapability> activate_capability;
  flight::Ref<types::HostAppAllWindowsClosedCapability> closed_capability;
  flight::Ref<types::HostAppOpenFileCapability> open_file_capability;
  flight::Ref<types::HostAppQuitRequestCapability> quit_request_capability;
  flight::Ref<types::HostAppReadyCapability> ready_capability;
  flight::Ref<types::HostAppSecondInstanceCapability> second_instance_capability;

  EventsHost() { build(); }

  void build() {
    activate_capability = flight::make_ref<types::HostAppActivateCapability>();
    activate_capability->subscribe = [this](std::function<void()> listener) {
      activate = std::move(listener);
      return std::function<void()>([this]() { ++releases; activate = {}; });
    };
    closed_capability = flight::make_ref<types::HostAppAllWindowsClosedCapability>();
    closed_capability->subscribe = [this](std::function<void()> listener) {
      all_windows_closed = std::move(listener);
      return std::function<void()>([this]() { ++releases; all_windows_closed = {}; });
    };
    open_file_capability = flight::make_ref<types::HostAppOpenFileCapability>();
    open_file_capability->subscribe = [this](std::function<void(flight::String)> listener) {
      open_file = std::move(listener);
      return std::function<void()>([this]() { ++releases; open_file = {}; });
    };
    quit_request_capability = flight::make_ref<types::HostAppQuitRequestCapability>();
    quit_request_capability->subscribe = [this](std::function<void(std::function<void()>)> listener) {
      quit_request = std::move(listener);
      return std::function<void()>([this]() { ++releases; quit_request = {}; });
    };
    ready_capability = flight::make_ref<types::HostAppReadyCapability>();
    ready_capability->subscribe = [this](std::function<void()> listener) {
      ready = std::move(listener);
      return std::function<void()>([this]() { ++releases; ready = {}; });
    };
    second_instance_capability = flight::make_ref<types::HostAppSecondInstanceCapability>();
    second_instance_capability->subscribe = [this](std::function<void(flight::Array<flight::String>)> listener) {
      second_instance = std::move(listener);
      return std::function<void()>([this]() { ++releases; second_instance = {}; });
    };
  }
};

// An inert provider: subscribes, records the subscription, and returns a release that counts. Used for the five
// providers a case does not exercise.
struct InertHost {
  int subscribes = 0;
  int releases = 0;

  flight::Ref<types::HostAppActivateCapability> activate = flight::make_ref<types::HostAppActivateCapability>();
  flight::Ref<types::HostAppAllWindowsClosedCapability> closed = flight::make_ref<types::HostAppAllWindowsClosedCapability>();
  flight::Ref<types::HostAppOpenFileCapability> open_file = flight::make_ref<types::HostAppOpenFileCapability>();
  flight::Ref<types::HostAppQuitRequestCapability> quit_request = flight::make_ref<types::HostAppQuitRequestCapability>();
  flight::Ref<types::HostAppSecondInstanceCapability> second_instance = flight::make_ref<types::HostAppSecondInstanceCapability>();

  InertHost() {
    activate->subscribe = [this](std::function<void()>) { ++subscribes; return std::function<void()>([this]() { ++releases; }); };
    closed->subscribe = [this](std::function<void()>) { ++subscribes; return std::function<void()>([this]() { ++releases; }); };
    open_file->subscribe = [this](std::function<void(flight::String)>) { ++subscribes; return std::function<void()>([this]() { ++releases; }); };
    quit_request->subscribe = [this](std::function<void(std::function<void()>)>) { ++subscribes; return std::function<void()>([this]() { ++releases; }); };
    second_instance->subscribe = [this](std::function<void(flight::Array<flight::String>)>) { ++subscribes; return std::function<void()>([this]() { ++releases; }); };
  }
};

void attach_all(EventsHost* host, const flight::Ref<Events>& events) {
  app::attach_app_events(host->activate_capability, host->closed_capability, host->open_file_capability,
                         host->quit_request_capability, host->ready_capability, host->second_instance_capability, events);
}

// --- The events entity ----------------------------------------------------------------------------------------

void test_a_fresh_events_entity_carries_all_six_signals() {
  const auto events = app::create_app_events();
  check(events->on_activate && events->on_all_windows_closed && events->on_open_file && events->on_quit_request &&
            events->on_ready && events->on_second_instance,
        "a fresh events entity carries all six application signals");
}

void test_two_events_entities_do_not_share_signals() {
  const auto first = app::create_app_events();
  const auto second = app::create_app_events();
  check(first->on_ready.get() != second->on_ready.get() &&
            first->on_activate.get() != second->on_activate.get(),
        "two events entities are independent, with their own signals");
}

// --- The six event families -----------------------------------------------------------------------------------

void test_each_parameterless_family_forwards_its_event() {
  EventsHost host;
  const auto events = app::create_app_events();
  int activated = 0;
  int closed = 0;
  int readied = 0;
  signals::connect_signal<PlainFn>(events->on_activate, [&activated]() { ++activated; }, std::nullopt);
  signals::connect_signal<PlainFn>(events->on_all_windows_closed, [&closed]() { ++closed; }, std::nullopt);
  signals::connect_signal<PlainFn>(events->on_ready, [&readied]() { ++readied; }, std::nullopt);

  app::attach_app_activate(host.activate_capability, events);
  app::attach_app_all_windows_closed(host.closed_capability, events);
  app::attach_app_ready(host.ready_capability, events);

  host.activate();
  host.all_windows_closed();
  host.ready();
  check(activated == 1 && closed == 1 && readied == 1,
        "each provider's event reached its own signal exactly once");
}

void test_the_open_file_family_carries_the_path() {
  EventsHost host;
  const auto events = app::create_app_events();
  std::string received;
  signals::connect_signal<StringFn>(events->on_open_file,
                                    [&received](flight::String path) { received = utf8(path); }, std::nullopt);

  app::attach_app_open_file(host.open_file_capability, events);
  host.open_file(flight::String("/tmp/a.flight"));
  check(received == "/tmp/a.flight", "the path the host reported reached onOpenFile");
}

void test_the_second_instance_family_carries_the_arguments() {
  EventsHost host;
  const auto events = app::create_app_events();
  std::size_t received = 0;
  std::string first_argument;
  signals::connect_signal<ArgsFn>(events->on_second_instance,
                                  [&received, &first_argument](flight::Array<flight::String> argv) {
                                    received = static_cast<std::size_t>(argv.size());
                                    if (argv.size() > 0) first_argument = utf8(argv.element(0.0));
                                  },
                                  std::nullopt);

  app::attach_app_second_instance(host.second_instance_capability, events);
  flight::Array<flight::String> argv;
  argv.push(flight::String("--open"));
  argv.push(flight::String("/tmp/b"));
  host.second_instance(argv);
  check(received == 2 && first_argument == "--open",
        "the second invocation's arguments reached onSecondInstance intact");
}

void test_a_quit_request_veto_reaches_the_native_cancel_callback() {
  EventsHost host;
  const auto events = app::create_app_events();
  int cancelled = 0;
  signals::connect_signal<PlainFn>(events->on_quit_request,
                                   [&events]() { signals::cancel_signal<std::function<void()>>(events->on_quit_request); },
                                   std::nullopt);

  app::attach_app_quit_request(host.quit_request_capability, events);
  host.quit_request([&cancelled]() { ++cancelled; });
  check(cancelled == 1, "a listener that cancelled the signal called the host's native cancel callback");
}

void test_a_quit_request_that_is_not_cancelled_leaves_the_native_side_alone() {
  EventsHost host;
  const auto events = app::create_app_events();
  int requests = 0;
  int cancelled = 0;
  signals::connect_signal<PlainFn>(events->on_quit_request, [&requests]() { ++requests; }, std::nullopt);

  app::attach_app_quit_request(host.quit_request_capability, events);
  host.quit_request([&cancelled]() { ++cancelled; });
  check(requests == 1 && cancelled == 0,
        "a listener that did not cancel reported the request without vetoing the native quit");
}

// --- Attaching all six, replacing them, and tearing them down --------------------------------------------------

void test_attaching_all_six_and_then_replacing_them_releases_every_origin() {
  EventsHost first;
  EventsHost second;
  const auto events = app::create_app_events();

  attach_all(&first, events);
  check(first.releases == 0, "the first attachment is live");

  attach_all(&second, events);
  check(first.releases == 6, "re-attaching releases all six of the origins it replaced");

  const int releases_after_replacement = second.releases;
  app::detach_app_events(events);
  check(second.releases == releases_after_replacement + 6, "and detaching releases the six it held");
}

void test_one_real_provider_among_five_inert_ones_still_works() {
  InertHost inert;
  EventsHost host;
  const auto events = app::create_app_events();
  int readied = 0;
  signals::connect_signal<PlainFn>(events->on_ready, [&readied]() { ++readied; }, std::nullopt);

  app::attach_app_events(inert.activate, inert.closed, inert.open_file, inert.quit_request, host.ready_capability,
                         inert.second_instance, events);
  host.ready();
  check(host.releases == 0, "the real provider is subscribed and still live");
  check(inert.subscribes == 5, "each of the five inert providers was subscribed once");
  check(readied == 1, "and the event it produced reached the signal");
}

void test_dispose_detaches_the_providers_and_clears_the_listeners() {
  EventsHost host;
  const auto events = app::create_app_events();
  int readied = 0;
  signals::connect_signal<PlainFn>(events->on_ready, [&readied]() { ++readied; }, std::nullopt);
  attach_all(&host, events);
  const auto listener = host.ready;

  app::dispose_app_events(events);
  check(host.releases == 6, "disposing releases every provider it had subscribed");
  listener();   // the provider's own callback, still in hand, fires after teardown
  check(readied == 0, "and clears the listeners, so a late event reaches nobody");
}

// --- The single-instance lock and the recent documents ---------------------------------------------------------

void test_the_single_instance_lock_routes_all_three_verbs() {
  int requests = 0;
  int releases = 0;
  const auto capability = flight::make_ref<types::HostAppSingleInstanceCapability>();
  capability->request_single_instance_lock = [&requests]() {
    ++requests;
    return true;
  };
  capability->has_single_instance_lock = []() { return false; };
  capability->release_single_instance_lock = [&releases]() { ++releases; };

  check(app::request_app_single_instance_lock(capability), "requesting reports the provider's answer");
  check(!app::has_app_single_instance_lock(capability), "and the query reports its own answer rather than the request's");
  app::release_app_single_instance_lock(capability);
  check(requests == 1 && releases == 1, "each verb reached its own provider entry once");
}

void test_the_recent_document_pair_delegates() {
  std::vector<std::string> calls;
  const auto capability = flight::make_ref<types::HostAppRecentDocumentsCapability>();
  capability->add_recent_document = [&calls](flight::String path) { calls.push_back("recent:" + utf8(path)); };
  capability->clear_recent_documents = [&calls]() { calls.push_back("clearRecent"); };

  app::add_app_recent_document(capability, flight::String("/tmp/a"));
  app::clear_app_recent_documents(capability);
  check(calls == std::vector<std::string>({"recent:/tmp/a", "clearRecent"}),
        "the path reached the provider and the clear reached its own entry");
}

// --- The ownership property (appHost.test.ts) ------------------------------------------------------------------
//
// The upstream suite's third case asserts that the module exports no ambient backend API. There is no export list
// to inspect here, so the behaviour behind that absence is what is asserted: handing two providers of the same kind
// shows that only the selected one is ever touched, which no cached or last-installed backend would produce.

void test_commands_and_queries_reach_only_the_selected_provider() {
  int first_name_calls = 0;
  int second_name_calls = 0;
  const auto first_name = flight::make_ref<types::HostAppNameCapability>();
  first_name->get_name = [&first_name_calls]() {
    ++first_name_calls;
    return flight::String("First App");
  };
  const auto second_name = flight::make_ref<types::HostAppNameCapability>();
  second_name->get_name = [&second_name_calls]() {
    ++second_name_calls;
    return flight::String("Second App");
  };

  int quits = 0;
  const auto quit = flight::make_ref<types::HostAppQuitCapability>();
  quit->quit = [&quits]() { ++quits; };

  int focuses = 0;
  const auto focus = flight::make_ref<types::HostAppFocusCapability>();
  focus->focus = [&focuses]() { ++focuses; };

  check(utf8(app::get_app_name(first_name)) == "First App", "the name comes from the provider that was handed over");
  check(utf8(app::get_app_name(second_name)) == "Second App",
        "and a different provider answers for itself, so the query is not memoized across calls");
  check(first_name_calls == 1 && second_name_calls == 1, "each having been asked exactly once");

  app::quit_app(quit);
  app::focus_app(focus);
  check(quits == 1 && focuses == 1, "each command reached its own provider exactly once");
}

void test_the_badge_command_returns_the_providers_task_result() {
  int counts = 0;
  double last = 0.0;
  const auto badge = flight::make_ref<types::HostAppBadgeCapability>();
  badge->set_badge_count = [&counts, &last](double count) {
    ++counts;
    last = count;
    return flight::Task<bool>::resolve(true);
  };

  check(app::set_app_badge_count(badge, 3.0).get(), "the badge command reports the provider's own result");
  check(counts == 1 && last == 3.0, "having passed the count through once");
}

void test_the_window_visibility_commands_delegate_to_their_provider() {
  std::vector<std::string> calls;
  const auto show = flight::make_ref<types::HostAppShowCapability>();
  show->show_app = [&calls]() { calls.push_back("showApp"); };
  const auto hide = flight::make_ref<types::HostAppHideCapability>();
  hide->hide_app = [&calls]() { calls.push_back("hideApp"); };

  app::show_app(show);
  app::hide_app(hide);
  check(calls == std::vector<std::string>({"showApp", "hideApp"}),
        "show and hide are separate capabilities, each reached at its own entry");
}

void test_the_hidden_query_derives_from_the_lifecycle_state() {
  // isAppHidden reads the LIFECYCLE provider rather than a window provider of its own: the app is hidden exactly
  // when the lifecycle reports it backgrounded.
  auto lifecycle = flight::make_ref<types::HostLifecycleCapability>();
  flight::String state = flight::String("background");
  lifecycle->get_state = [&state]() { return state; };
  lifecycle->subscribe = [](std::function<void()>) { return std::function<void()>([]() {}); };

  check(app::is_app_hidden(lifecycle), "a backgrounded app reports itself hidden");
  state = flight::String("active");
  check(!app::is_app_hidden(lifecycle), "and an active one does not");
}

} // namespace

int main() {
  test_a_fresh_events_entity_carries_all_six_signals();
  test_two_events_entities_do_not_share_signals();

  test_each_parameterless_family_forwards_its_event();
  test_the_open_file_family_carries_the_path();
  test_the_second_instance_family_carries_the_arguments();
  test_a_quit_request_veto_reaches_the_native_cancel_callback();
  test_a_quit_request_that_is_not_cancelled_leaves_the_native_side_alone();

  test_attaching_all_six_and_then_replacing_them_releases_every_origin();
  test_one_real_provider_among_five_inert_ones_still_works();
  test_dispose_detaches_the_providers_and_clears_the_listeners();

  test_the_single_instance_lock_routes_all_three_verbs();
  test_the_recent_document_pair_delegates();

  test_commands_and_queries_reach_only_the_selected_provider();
  test_the_badge_command_returns_the_providers_task_result();
  test_the_window_visibility_commands_delegate_to_their_provider();
  test_the_hidden_query_derives_from_the_lifecycle_state();

  if (failures == 0) {
    std::cout << "app core parity: all checks passed\n";
    return 0;
  }
  std::cerr << failures << " check(s) failed\n";
  return 1;
}
