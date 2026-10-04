#include <flight/connectivity/connectivity.hpp>

#include <functional>
#include <iostream>
#include <optional>
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

  return 0;
}
