#pragma once

#include <functional>
#include <optional>
#include <utility>

#include <flight/entity/entity.hpp>
#include <flight/runtime.hpp>
#include <flight/signals/emitter.hpp>
#include <flight/signals/signal.hpp>
#include <flight/signals/slot.hpp>
#include <flight/types/connectivity.hpp>
#include <flight/weak_map.hpp>

namespace flight::connectivity {

using ConnectivityStatusSignal =
    std::function<void(flight::StructuralRef<flight::RowReadonly<
        flight::RowOf<flight::Ref<flight::types::ConnectivityStatus>>>>)>;
using ConnectivityTypeSignal =
    std::function<void(flight::types::ConnectivityConnectionType)>;
using ConnectivityBooleanSignal = std::function<void(bool)>;
using ConnectivityVoidSignal = std::function<void()>;

inline flight::WeakMap<flight::Ref<flight::types::Connectivity>, std::function<void()>>
    subscriptions;

inline void initialize_connectivity(
    flight::types::EntityConstruction<flight::Ref<flight::types::Connectivity>> out) {
  out->on_change = flight::signals::create_signal<ConnectivityStatusSignal>();
  out->on_connection_type_change =
      flight::signals::create_signal<ConnectivityTypeSignal>();
  out->on_metered_change = flight::signals::create_signal<ConnectivityBooleanSignal>();
  out->on_offline = flight::signals::create_signal<ConnectivityVoidSignal>();
  out->on_online = flight::signals::create_signal<ConnectivityVoidSignal>();
}

[[nodiscard]] inline flight::Ref<flight::types::Connectivity> create_connectivity() {
  auto out =
      flight::entity::allocate_entity<flight::Ref<flight::types::Connectivity>>();
  initialize_connectivity(out);
  return flight::entity::finish_entity(out);
}

inline void destroy_connectivity(
    flight::Ref<flight::types::HostConnectivityChangeCapability>
        host_connectivity_change) {
  host_connectivity_change->destroy();
}

[[nodiscard]] inline flight::Task<flight::Ref<flight::types::ConnectivityReachability>>
detect_connectivity_reachability(
    flight::Ref<flight::types::HostConnectivityReachabilityCapability>
        host_connectivity_reachability,
    flight::Ref<flight::types::ConnectivityReachabilityOptions> options,
    flight::Ref<flight::types::ConnectivityReachability> out) {
  return host_connectivity_reachability->detect_reachability(options, out);
}

[[nodiscard]] inline flight::Ref<flight::types::ConnectivityStatus>
get_connectivity_status(
    flight::Ref<flight::types::HostConnectivityStatusCapability>
        host_connectivity_status,
    flight::Ref<flight::types::ConnectivityStatus> out) {
  return host_connectivity_status->get_status(out);
}

[[nodiscard]] inline bool has_connectivity_status_changed(
    flight::Ref<flight::types::ConnectivityStatus> a,
    flight::Ref<flight::types::ConnectivityStatus> b) {
  return a->online != b->online || a->type != b->type ||
         a->downlink != b->downlink || a->downlink_max != b->downlink_max ||
         a->effective_type != b->effective_type || a->rtt != b->rtt ||
         a->save_data != b->save_data || a->metered != b->metered;
}

[[nodiscard]] inline flight::Ref<flight::types::ConnectivityStatus>
connectivity_status_out() {
  auto out = flight::make_ref<flight::types::ConnectivityStatus>();
  out->online = std::nullopt;
  out->type = flight::String("unknown");
  out->downlink = -1.0;
  out->downlink_max = -1.0;
  out->effective_type = flight::String("");
  out->rtt = -1.0;
  out->save_data = false;
  out->metered = false;
  return out;
}

[[nodiscard]] inline std::optional<bool> get_connectivity_online(
    flight::Ref<flight::types::HostConnectivityStatusCapability>
        host_connectivity_status) {
  return host_connectivity_status->get_status(connectivity_status_out())->online;
}

[[nodiscard]] inline bool is_connectivity_metered(
    flight::Ref<flight::types::HostConnectivityStatusCapability>
        host_connectivity_status) {
  return host_connectivity_status->get_status(connectivity_status_out())->metered;
}

[[nodiscard]] inline bool is_connectivity_save_data_enabled(
    flight::Ref<flight::types::HostConnectivityStatusCapability>
        host_connectivity_status) {
  return host_connectivity_status->get_status(connectivity_status_out())->save_data;
}

inline void detach_connectivity(
    flight::Ref<flight::types::Connectivity> connectivity) {
  auto unsubscribe = subscriptions.get(connectivity);
  if (!unsubscribe.has_value()) return;
  static_cast<void>(subscriptions.erase(connectivity));
  unsubscribe.value()();
}

[[nodiscard]] inline bool attach_connectivity(
    flight::Ref<flight::types::HostConnectivityStatusCapability>
        host_connectivity_status,
    flight::Ref<flight::types::HostConnectivityChangeCapability>
        host_connectivity_change,
    flight::Ref<flight::types::Connectivity> connectivity) {
  detach_connectivity(connectivity);
  const auto initial =
      host_connectivity_status->get_status(connectivity_status_out());
  auto was_online = initial->online;
  auto was_type = initial->type;
  auto was_metered = initial->metered;
  auto unsubscribe = host_connectivity_change->subscribe(
      [connectivity, host_connectivity_status, was_online, was_type,
       was_metered]() mutable {
        const auto status =
            host_connectivity_status->get_status(connectivity_status_out());
        flight::signals::emit_signal(connectivity->on_change, status);
        if (status->online != was_online) {
          was_online = status->online;
          if (status->online == std::optional<bool>{true}) {
            flight::signals::emit_signal(connectivity->on_online);
          } else if (status->online == std::optional<bool>{false}) {
            flight::signals::emit_signal(connectivity->on_offline);
          }
        }
        if (status->type != was_type) {
          was_type = status->type;
          flight::signals::emit_signal(connectivity->on_connection_type_change,
                                       status->type);
        }
        if (status->metered != was_metered) {
          was_metered = status->metered;
          flight::signals::emit_signal(connectivity->on_metered_change,
                                       status->metered);
        }
      });
  if (!unsubscribe.has_value()) return false;
  subscriptions.set(connectivity, std::move(unsubscribe.value()));
  return true;
}

inline void dispose_connectivity(
    flight::Ref<flight::types::Connectivity> connectivity) {
  detach_connectivity(connectivity);
  flight::signals::clear_signal(connectivity->on_change);
  flight::signals::clear_signal(connectivity->on_connection_type_change);
  flight::signals::clear_signal(connectivity->on_metered_change);
  flight::signals::clear_signal(connectivity->on_offline);
  flight::signals::clear_signal(connectivity->on_online);
}

}  // namespace flight::connectivity
