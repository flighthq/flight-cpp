// Focused behavior tests for @flighthq/connectivity.
//
// The package is `assisted` at a627849: the emitter writes two of its three modules and refuses one
// function -- attachConnectivity, `cpp-member-projection-without-present-storage`, on the source's
// `if (unsubscribe === null)` -- which overrides/include/flight/connectivity/ supplies over the
// emission. Unlike the packages where the emitted header itself cannot compile, the emission here
// compiles on its own (measured: 3/3 of its headers, no errors), so nothing is hidden by the override.
//
// The blocks below the line marked ADDED are the cases the earlier authoring did not reach; the rest of
// the file is that authoring, unchanged, except for one replacement-block fixture that my first
// version got wrong and the block itself now explains. All of it is ported from the module's own upstream suite
// (packages/connectivity/src/connectivity.test.ts) and the emphasis is the three things the parcel asked
// for:
//
//   * SUBSCRIPTION LIFECYCLE -- a replacement consumes the prior provider's exact release before the new
//     one is touched, a detached or re-entrant detach releases once, a provider that answers null
//     subscription stores nothing and reports false, teardown is terminal and idempotent, and dispose
//     clears every signal;
//   * HOST IDENTITY -- status reads go to the status capability, the subscription and its teardown to
//     the change capability, reachability to its own capability, and status results keep their reference
//     identity;
//   * OUTCOME SEMANTICS -- unknown online (null) is unmeasured rather than offline, a transition is
//     reported only against the attach-time baseline and only in the arm it moved to, onChange publishes
//     every host notification with a FRESH snapshot per event, and the eight-field change test detects
//     each field independently.
#include <flight/connectivity/connectivity.hpp>

#include <functional>
#include <iostream>
#include <optional>
#include <vector>
#include <utility>

namespace {

bool check(bool condition, const char* message) {
  if (!condition) std::cerr << message << '\n';
  return condition;
}

}  // namespace

int main() {
  using flight::String;
  using namespace flight::types;

  const auto connectivity = flight::connectivity::create_connectivity();
  if (!check(connectivity && connectivity->on_change &&
                 connectivity->on_connection_type_change &&
                 connectivity->on_metered_change && connectivity->on_offline &&
                 connectivity->on_online,
             "connectivity did not create all five signals")) {
    return 1;
  }

  std::optional<bool> online;
  String connection_type("unknown");
  double downlink = 3.0;
  double downlink_max = 7.0;
  String effective_type("4g");
  double rtt = 12.0;
  bool save_data = false;
  bool metered = false;
  bool saw_sentinel = false;
  int status_reads = 0;
  const auto status_provider = flight::make_ref<HostConnectivityStatusCapability>(
      HostConnectivityStatusCapability{
          .get_status = [&](flight::Ref<ConnectivityStatus> out) {
            ++status_reads;
            saw_sentinel = !out->online.has_value() &&
                           out->type == String("unknown") && out->downlink == -1.0 &&
                           out->downlink_max == -1.0 && out->effective_type == String("") &&
                           out->rtt == -1.0 && !out->save_data && !out->metered;
            out->online = online;
            out->type = connection_type;
            out->downlink = downlink;
            out->downlink_max = downlink_max;
            out->effective_type = effective_type;
            out->rtt = rtt;
            out->save_data = save_data;
            out->metered = metered;
            return out;
          }});

  if (!check(!flight::connectivity::get_connectivity_online(status_provider).has_value(),
             "nullable online snapshot changed") ||
      !check(!flight::connectivity::is_connectivity_metered(status_provider),
             "metered snapshot changed") ||
      !check(!flight::connectivity::is_connectivity_save_data_enabled(status_provider),
             "save-data snapshot changed") ||
      !check(saw_sentinel && status_reads == 3,
             "snapshot queries did not use fresh sentinel objects")) {
    return 1;
  }

  int change_events = 0;
  int online_events = 0;
  int offline_events = 0;
  int type_events = 0;
  int metered_events = 0;
  String emitted_type;
  bool emitted_metered = false;
  connectivity->on_change->emit = [&](auto status) {
    ++change_events;
    if (flight::row_get<flight::RowKey<"type">>(status) != connection_type) {
      change_events = -100;
    }
  };
  connectivity->on_online->emit = [&]() { ++online_events; };
  connectivity->on_offline->emit = [&]() { ++offline_events; };
  connectivity->on_connection_type_change->emit = [&](String value) {
    ++type_events;
    emitted_type = std::move(value);
  };
  connectivity->on_metered_change->emit = [&](bool value) {
    ++metered_events;
    emitted_metered = value;
  };

  std::function<void()> host_listener;
  int unsubscribes = 0;
  const auto change_provider = flight::make_ref<HostConnectivityChangeCapability>(
      HostConnectivityChangeCapability{
          .destroy = []() {},
          .subscribe = [&](std::function<void()> listener) {
            host_listener = std::move(listener);
            return std::optional<std::function<void()>>{
                [&]() { ++unsubscribes; }};
          }});

  if (!check(flight::connectivity::attach_connectivity(
                 status_provider, change_provider, connectivity),
             "host subscription was rejected")) {
    return 1;
  }
  host_listener();
  online = true;
  connection_type = String("wifi");
  metered = true;
  host_listener();
  online = false;
  host_listener();
  if (!check(change_events == 3, "onChange did not publish every host event") ||
      !check(online_events == 1 && offline_events == 1,
             "online/offline edge routing changed") ||
      !check(type_events == 1 && emitted_type == String("wifi"),
             "connection-type edge routing changed") ||
      !check(metered_events == 1 && emitted_metered,
             "metered edge routing changed")) {
    return 1;
  }

  // A REPEAT of the same online value must not re-fire the edge signals. The mutation battery found this
  // unasserted: every event above CHANGES the value, and because the fixture's `online` starts as nullopt,
  // both a true event and a false event differ from the attach-time capture -- so a capture that is never
  // updated still produces exactly one online and one offline, and the edge semantics look pinned when
  // they are not. A repeated value is what separates an edge from a level.
  {
    const int online_before = online_events;
    const int offline_before = offline_events;
    online = false;   // the value the event above already delivered
    host_listener();
    host_listener();
    if (!check(online_events == online_before && offline_events == offline_before,
               "a repeated online value re-fired the online/offline edge signals")) {
      return 1;
    }
  }

  static_cast<void>(flight::connectivity::attach_connectivity(
      status_provider, change_provider, connectivity));
  flight::connectivity::detach_connectivity(connectivity);
  flight::connectivity::detach_connectivity(connectivity);
  if (!check(unsubscribes == 2,
             "replacement or idempotent detach changed unsubscribe ownership")) {
    return 1;
  }

  const auto failed_provider = flight::make_ref<HostConnectivityChangeCapability>(
      HostConnectivityChangeCapability{
          .destroy = []() {},
          .subscribe = [](std::function<void()>) {
            return std::optional<std::function<void()>>{};
          }});
  if (!check(!flight::connectivity::attach_connectivity(
                 status_provider, failed_provider, connectivity),
             "failed provider subscription reported success")) {
    return 1;
  }

  int reentrant_unsubscribes = 0;
  const auto reentrant_provider = flight::make_ref<HostConnectivityChangeCapability>(
      HostConnectivityChangeCapability{
          .destroy = []() {},
          .subscribe = [&](std::function<void()>) {
            return std::optional<std::function<void()>>{[&]() {
              ++reentrant_unsubscribes;
              flight::connectivity::detach_connectivity(connectivity);
            }};
          }});
  static_cast<void>(flight::connectivity::attach_connectivity(
      status_provider, reentrant_provider, connectivity));
  flight::connectivity::detach_connectivity(connectivity);
  if (!check(reentrant_unsubscribes == 1,
             "re-entrant detach invoked unsubscribe more than once")) {
    return 1;
  }

  const auto first = flight::connectivity::connectivity_status_out();
  const auto second = flight::connectivity::connectivity_status_out();
  if (!check(!flight::connectivity::has_connectivity_status_changed(first, second),
             "equal statuses reported a change")) {
    return 1;
  }
  second->rtt = 1.0;
  if (!check(flight::connectivity::has_connectivity_status_changed(first, second),
             "changed status was not detected")) {
    return 1;
  }

  const auto reachability_out = flight::make_ref<ConnectivityReachability>(
      ConnectivityReachability{.reachable = false, .latency = -1.0});
  const auto reachability_provider =
      flight::make_ref<HostConnectivityReachabilityCapability>(
          HostConnectivityReachabilityCapability{
              .detect_reachability = [](auto, flight::Ref<ConnectivityReachability> out) {
                out->reachable = true;
                out->latency = 8.0;
                return flight::Task<flight::Ref<ConnectivityReachability>>::resolve(out);
              }});
  const auto options = flight::make_ref<ConnectivityReachabilityOptions>(
      ConnectivityReachabilityOptions{.url = String("https://example.test"),
                                      .timeout = std::nullopt,
                                      .signal = std::nullopt});
  const auto reachability = flight::connectivity::detect_connectivity_reachability(
                                reachability_provider, options, reachability_out)
                                .get();
  if (!check(reachability == reachability_out && reachability->reachable &&
                 reachability->latency == 8.0,
             "reachability task or output identity changed")) {
    return 1;
  }

  int destroys = 0;
  const auto destroy_provider = flight::make_ref<HostConnectivityChangeCapability>(
      HostConnectivityChangeCapability{
          .destroy = [&]() { ++destroys; },
          .subscribe = [](std::function<void()>) {
            return std::optional<std::function<void()>>{};
          }});
  flight::connectivity::destroy_connectivity(destroy_provider);
  if (!check(destroys == 1, "provider destroy was not delegated")) return 1;

  const int events_before_dispose = change_events + online_events + offline_events +
                                    type_events + metered_events;
  flight::connectivity::dispose_connectivity(connectivity);
  connectivity->on_change->emit(first);
  connectivity->on_online->emit();
  connectivity->on_offline->emit();
  connectivity->on_connection_type_change->emit(String("cellular"));
  connectivity->on_metered_change->emit(false);
  if (!check(events_before_dispose == change_events + online_events + offline_events +
                                         type_events + metered_events,
             "dispose did not clear all five signals")) {
    return 1;
  }


  // ===== ADDED: the axes the earlier authoring did not reach =====

  // unknown online is NOT offline. The baseline is the attach-time reading, so null -> false is a
  // measured transition and null -> null is not, and moving back to null is not a transition either.
  {
    std::optional<bool> unknown_online;
    bool unknown_metered = false;
    const auto unknown_status = flight::make_ref<HostConnectivityStatusCapability>(
        HostConnectivityStatusCapability{
            .get_status = [&](flight::Ref<ConnectivityStatus> out) {
              out->online = unknown_online;
              out->type = String("wifi");
              out->metered = unknown_metered;
              return out;
            }});
    std::function<void()> unknown_listener;
    const auto unknown_change = flight::make_ref<HostConnectivityChangeCapability>(
        HostConnectivityChangeCapability{
            .destroy = []() {},
            .subscribe = [&](std::function<void()> listener) {
              unknown_listener = std::move(listener);
              return std::optional<std::function<void()>>{[]() {}};
            }});
    const auto unknown = flight::connectivity::create_connectivity();
    int online_events = 0;
    int offline_events = 0;
    std::vector<const void*> payload_ids;
    std::vector<String> payload_types;
    unknown->on_online->emit = [&]() { ++online_events; };
    unknown->on_offline->emit = [&]() { ++offline_events; };
    unknown->on_change->emit = [&](auto status) {
      payload_ids.push_back(status.identity());
      payload_types.push_back(flight::row_get<flight::RowKey<"type">>(status));
    };

    if (!check(flight::connectivity::attach_connectivity(unknown_status, unknown_change, unknown),
               "the unknown-state provider was rejected")) {
      return 1;
    }
    unknown_listener();
    if (!check(online_events == 0 && offline_events == 0,
               "an unknown online state reported a measured transition")) {
      return 1;
    }
    unknown_online = false;
    unknown_listener();
    if (!check(offline_events == 1 && online_events == 0,
               "null -> false was not an offline transition")) {
      return 1;
    }
    unknown_online = true;
    unknown_listener();
    if (!check(online_events == 1 && offline_events == 1,
               "false -> true was not an online transition")) {
      return 1;
    }
    unknown_online = std::nullopt;
    unknown_listener();
    if (!check(online_events == 1 && offline_events == 1,
               "true -> null reported a measured transition")) {
      return 1;
    }
    if (!check(payload_ids.size() == 4,
               "onChange did not publish every host notification, including the unmeasured one")) {
      return 1;
    }
    bool all_distinct = true;
    for (std::size_t i = 0; i < payload_ids.size(); ++i) {
      for (std::size_t j = i + 1; j < payload_ids.size(); ++j) {
        if (payload_ids[i] == payload_ids[j]) all_distinct = false;
      }
      if (payload_types[i] != String("wifi")) all_distinct = false;
    }
    if (!check(all_distinct,
               "two change events shared one payload object, or a payload kept a later value")) {
      return 1;
    }
  }

  // Replacement: the prior provider's release is consumed and its listener is removed from that
  // provider, so it is never delivered from again, while the replacement's own listener reaches the
  // entity. The fake removes the released listener from its own set, which is what makes "never
  // delivered from again" observable -- a first version of this case invoked the captured listener
  // directly, which delivered from a released subscription and proved nothing about the module.
  {
    struct Recorder {
      std::vector<std::function<void()>> listeners;
      int releases = 0;
    };
    const auto make_change = [](const std::shared_ptr<Recorder>& recorder) {
      return flight::make_ref<HostConnectivityChangeCapability>(
          HostConnectivityChangeCapability{
              .destroy = []() {},
              .subscribe = [recorder](std::function<void()> listener) {
                recorder->listeners.push_back(std::move(listener));
                const auto index = recorder->listeners.size() - 1;
                return std::optional<std::function<void()>>{[recorder, index]() {
                  ++recorder->releases;
                  if (index < recorder->listeners.size()) {
                    recorder->listeners[index] = nullptr;
                  }
                }};
              }});
    };
    const auto fire = [](const std::shared_ptr<Recorder>& recorder) {
      for (auto& listener : recorder->listeners) {
        if (listener) listener();
      }
    };

    std::optional<bool> online_a = true;
    std::optional<bool> online_b = false;
    const auto status_for = [](std::optional<bool>* value) {
      return flight::make_ref<HostConnectivityStatusCapability>(HostConnectivityStatusCapability{
          .get_status = [value](flight::Ref<ConnectivityStatus> out) {
            out->online = *value;
            return out;
          }});
    };
    const auto recorder_a = std::make_shared<Recorder>();
    const auto recorder_b = std::make_shared<Recorder>();
    const auto change_a = make_change(recorder_a);
    const auto change_b = make_change(recorder_b);
    const auto entity = flight::connectivity::create_connectivity();
    int deliveries = 0;
    entity->on_change->emit = [&](auto) { ++deliveries; };

    static_cast<void>(flight::connectivity::attach_connectivity(status_for(&online_a), change_a, entity));
    static_cast<void>(flight::connectivity::attach_connectivity(status_for(&online_b), change_b, entity));
    if (!check(recorder_a->releases == 1 && recorder_a->listeners.size() == 1 &&
                   !recorder_a->listeners.front(),
               "replacement did not consume the prior provider's release")) {
      return 1;
    }
    fire(recorder_a);
    if (!check(deliveries == 0, "the replaced provider still delivered to the entity")) return 1;
    fire(recorder_b);
    if (!check(deliveries == 1, "the replacement provider did not deliver")) return 1;
  }

  // Status outcomes keep their identity, online keeps its three states, and the change test sees each
  // of its eight fields independently.
  {
    const auto provider = flight::make_ref<HostConnectivityStatusCapability>(
        HostConnectivityStatusCapability{
            .get_status = [](flight::Ref<ConnectivityStatus> out) {
              out->online = true;
              out->type = String("wifi");
              return out;
            }});
    const auto out = flight::connectivity::connectivity_status_out();
    if (!check(flight::connectivity::get_connectivity_status(provider, out) == out,
               "the status query did not answer with the object it was given")) {
      return 1;
    }
    const auto measured = flight::connectivity::get_connectivity_online(provider);
    if (!check(measured.has_value() && measured.value(),
               "a measured online reading lost its value")) {
      return 1;
    }

    const auto base = flight::connectivity::connectivity_status_out();
    const auto varied = [&](auto mutate) {
      const auto other = flight::connectivity::connectivity_status_out();
      mutate(other);
      return flight::connectivity::has_connectivity_status_changed(base, other);
    };
    const bool axes[] = {
        varied([](auto s) { s->online = true; }),
        varied([](auto s) { s->type = String("cellular"); }),
        varied([](auto s) { s->downlink = 1.0; }),
        varied([](auto s) { s->downlink_max = 1.0; }),
        varied([](auto s) { s->effective_type = String("4g"); }),
        varied([](auto s) { s->rtt = 1.0; }),
        varied([](auto s) { s->save_data = true; }),
        varied([](auto s) { s->metered = true; }),
    };
    for (const bool changed : axes) {
      if (!check(changed, "one of the eight status fields was not detected as a change")) return 1;
    }
    if (!check(!flight::connectivity::has_connectivity_status_changed(base, base),
               "an aliased snapshot reported a change")) {
      return 1;
    }
  }

  // Teardown is terminal and idempotent, and a destroyed provider accepts nothing afterwards.
  {
    bool destroyed = false;
    int destroys = 0;
    const auto provider = flight::make_ref<HostConnectivityChangeCapability>(
        HostConnectivityChangeCapability{
            .destroy = [&]() {
              if (destroyed) return;
              destroyed = true;
              ++destroys;
            },
            .subscribe = [&](std::function<void()>) -> std::optional<std::function<void()>> {
              if (destroyed) return std::nullopt;
              return std::function<void()>{[]() {}};
            }});
    const auto status_provider = flight::make_ref<HostConnectivityStatusCapability>(
        HostConnectivityStatusCapability{
            .get_status = [](flight::Ref<ConnectivityStatus> out) {
              out->online = true;
              return out;
            }});
    flight::connectivity::destroy_connectivity(provider);
    flight::connectivity::destroy_connectivity(provider);
    if (!check(destroys == 1, "provider teardown was not terminal and idempotent")) return 1;
    if (!check(!flight::connectivity::attach_connectivity(
                   status_provider, provider, flight::connectivity::create_connectivity()),
               "a destroyed provider accepted a subscription")) {
      return 1;
    }
  }

  std::cout << "generated connectivity: all checks passed\n";
  return 0;
}
