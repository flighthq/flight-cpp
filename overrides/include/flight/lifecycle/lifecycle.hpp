#pragma once

#include <functional>
#include <optional>
#include <utility>

#include <flight/any.hpp>
#include <flight/entity/entity.hpp>
#include <flight/record.hpp>
#include <flight/runtime.hpp>
#include <flight/signals/emitter.hpp>
#include <flight/signals/signal.hpp>
#include <flight/types/backend_operation_explanation.hpp>
#include <flight/types/lifecycle.hpp>
#include <flight/weak_map.hpp>

namespace flight::lifecycle {

using StateSignal = std::function<void(flight::types::AppLifecycleState)>;
using VoidSignal = std::function<void()>;
using MemorySignal = std::function<void(flight::types::AppMemoryPressure)>;
using StateBag = flight::Record<flight::String, flight::Any>;
using StateBagSignal = std::function<void(StateBag)>;

inline flight::WeakMap<flight::Ref<flight::types::AppLifecycle>, StateBag>
    saved_state;
inline flight::WeakMap<flight::Ref<flight::types::AppLifecycle>,
                       std::function<void()>>
    subscriptions;

[[nodiscard]] inline flight::Ref<flight::types::BackendOperationExplanation>
explain_lifecycle_operation(
    flight::Ref<flight::types::HostLifecycleCapability> host_lifecycle,
    flight::types::LifecycleOperation operation) {
  bool implemented = false;
  if (operation == flight::String("getState") ||
      operation == flight::String("subscribe")) {
    implemented = true;
  } else if (operation == flight::String("getLaunchKind")) {
    implemented = host_lifecycle->get_launch_kind.has_value();
  } else if (operation == flight::String("subscribeMemoryWarning")) {
    implemented = host_lifecycle->subscribe_memory_warning.has_value();
  }
  return flight::make_ref<flight::types::BackendOperationExplanation>(
      flight::types::BackendOperationExplanation{
          .implemented = implemented,
          .layer = flight::String(implemented ? "host" : "sentinel"),
          .operation = std::move(operation),
      });
}

[[nodiscard]] inline flight::types::AppLaunchKind get_app_launch_kind(
    flight::Ref<flight::types::HostLifecycleCapability> host_lifecycle) {
  if (!host_lifecycle->get_launch_kind.has_value()) {
    return flight::String("warm");
  }
  return host_lifecycle->get_launch_kind.value()();
}

[[nodiscard]] inline flight::types::AppLifecycleState get_app_lifecycle_state(
    flight::Ref<flight::types::HostLifecycleCapability> host_lifecycle) {
  return host_lifecycle->get_state();
}

[[nodiscard]] inline bool has_lifecycle_operation(
    flight::Ref<flight::types::HostLifecycleCapability> host_lifecycle,
    flight::types::LifecycleOperation operation) {
  return explain_lifecycle_operation(std::move(host_lifecycle),
                                     std::move(operation))
      ->implemented;
}

inline void initialize_app_lifecycle(
    flight::types::EntityConstruction<flight::Ref<flight::types::AppLifecycle>> out) {
  out->on_state_change = flight::signals::create_signal<StateSignal>();
  out->on_resume = flight::signals::create_signal<VoidSignal>();
  out->on_pause = flight::signals::create_signal<VoidSignal>();
  out->on_back_button = flight::signals::create_signal<VoidSignal>();
  out->on_memory_warning = flight::signals::create_signal<MemorySignal>();
  out->on_save_state = flight::signals::create_signal<StateBagSignal>();
  out->on_restore_state = flight::signals::create_signal<StateBagSignal>();
}

[[nodiscard]] inline flight::Ref<flight::types::AppLifecycle>
create_app_lifecycle() {
  auto out =
      flight::entity::allocate_entity<flight::Ref<flight::types::AppLifecycle>>();
  initialize_app_lifecycle(out);
  return flight::entity::finish_entity(out);
}

[[nodiscard]] inline bool is_app_active(
    flight::Ref<flight::types::HostLifecycleCapability> host_lifecycle) {
  return host_lifecycle->get_state() == flight::String("active");
}

[[nodiscard]] inline bool is_app_background(
    flight::Ref<flight::types::HostLifecycleCapability> host_lifecycle) {
  return host_lifecycle->get_state() == flight::String("background");
}

[[nodiscard]] inline bool is_app_inactive(
    flight::Ref<flight::types::HostLifecycleCapability> host_lifecycle) {
  return host_lifecycle->get_state() == flight::String("inactive");
}

[[nodiscard]] inline bool request_app_back(
    flight::Ref<flight::types::AppLifecycle> app) {
  flight::signals::emit_signal(app->on_back_button);
  return !app->on_back_button->data.has_value() ||
         !app->on_back_button->data.value()->cancelled;
}

inline void detach_app_lifecycle(
    flight::Ref<flight::types::AppLifecycle> app) {
  auto unsubscribe = subscriptions.get(app);
  if (!unsubscribe.has_value()) return;
  unsubscribe.value()();
  static_cast<void>(subscriptions.erase(app));
}

inline void attach_app_lifecycle(
    flight::Ref<flight::types::HostLifecycleCapability> host_lifecycle,
    flight::Ref<flight::types::AppLifecycle> app) {
  detach_app_lifecycle(app);
  const auto backend = host_lifecycle;
  auto previous = backend->get_state();
  auto unsubscribe_state = backend->subscribe([backend, app, previous]() mutable {
    const auto state = backend->get_state();
    flight::signals::emit_signal(app->on_state_change, state);
    if (state == flight::String("active") &&
        previous != flight::String("active")) {
      flight::signals::emit_signal(app->on_resume);
      auto saved = saved_state.get(app);
      if (saved.has_value()) {
        flight::signals::emit_signal(app->on_restore_state, saved.value());
      }
    } else if (state != flight::String("active") &&
               previous == flight::String("active")) {
      flight::signals::emit_signal(app->on_pause);
      StateBag state_bag;
      flight::signals::emit_signal(app->on_save_state, state_bag);
      saved_state.set(app, state_bag);
    }
    previous = state;
  });

  std::optional<std::function<void()>> unsubscribe_memory;
  if (backend->subscribe_memory_warning.has_value()) {
    unsubscribe_memory = backend->subscribe_memory_warning.value()(
        [app](flight::types::AppMemoryPressure level) {
          flight::signals::emit_signal(app->on_memory_warning,
                                       std::move(level));
        });
  }
  subscriptions.set(
      app, [unsubscribe_state = std::move(unsubscribe_state),
            unsubscribe_memory = std::move(unsubscribe_memory)]() mutable {
        unsubscribe_state();
        if (unsubscribe_memory.has_value()) unsubscribe_memory.value()();
      });
}

inline void dispose_app_lifecycle(
    flight::Ref<flight::types::AppLifecycle> app) {
  detach_app_lifecycle(app);
  static_cast<void>(saved_state.erase(app));
}

}  // namespace flight::lifecycle
