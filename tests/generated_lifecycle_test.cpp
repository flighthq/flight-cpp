#include <flight/lifecycle/lifecycle.hpp>

#include <functional>
#include <iostream>
#include <optional>
#include <utility>

#include <flight/signals/emitter.hpp>

namespace {

bool check(bool condition, const char* message) {
  if (!condition) std::cerr << message << '\n';
  return condition;
}

}  // namespace

int main() {
  using flight::String;
  using namespace flight::types;

  const auto app = flight::lifecycle::create_app_lifecycle();
  if (!check(app && app->on_state_change && app->on_resume && app->on_pause &&
                 app->on_back_button && app->on_memory_warning &&
                 app->on_save_state && app->on_restore_state,
             "lifecycle did not create all seven signals")) {
    return 1;
  }

  String state("active");
  std::function<void()> state_listener;
  std::function<void(String)> memory_listener;
  int state_unsubscribes = 0;
  int memory_unsubscribes = 0;
  const auto provider = flight::make_ref<HostLifecycleCapability>(
      HostLifecycleCapability{
          .get_state = [&]() { return state; },
          .subscribe = [&](std::function<void()> listener) {
            state_listener = std::move(listener);
            return [&]() {
              ++state_unsubscribes;
              state_listener = {};
            };
          },
          .get_launch_kind = std::function<String()>([]() {
            return String("cold");
          }),
          .subscribe_memory_warning =
              std::function<std::function<void()>(std::function<void(String)>)>(
                  [&](std::function<void(String)> listener) {
                    memory_listener = std::move(listener);
                    return [&]() {
                      ++memory_unsubscribes;
                      memory_listener = {};
                    };
                  }),
      });
  const auto bare_provider = flight::make_ref<HostLifecycleCapability>(
      HostLifecycleCapability{
          .get_state = []() { return String("background"); },
          .subscribe = [](std::function<void()>) {
            return std::function<void()>([]() {});
          },
          .get_launch_kind = std::nullopt,
          .subscribe_memory_warning = std::nullopt,
      });

  const auto required = flight::lifecycle::explain_lifecycle_operation(
      bare_provider, String("getState"));
  const auto missing = flight::lifecycle::explain_lifecycle_operation(
      bare_provider, String("getLaunchKind"));
  const auto present = flight::lifecycle::explain_lifecycle_operation(
      provider, String("subscribeMemoryWarning"));
  const auto unknown = flight::lifecycle::explain_lifecycle_operation(
      provider, String("futureOperation"));
  if (!check(required->implemented && required->layer == String("host") &&
                 required->operation == String("getState"),
             "required operation explanation changed") ||
      !check(!missing->implemented && missing->layer == String("sentinel") &&
                 present->implemented && !unknown->implemented,
             "optional operation presence was not answered per provider") ||
      !check(flight::lifecycle::has_lifecycle_operation(
                 provider, String("subscribeMemoryWarning")) &&
                 !flight::lifecycle::has_lifecycle_operation(
                     bare_provider, String("subscribeMemoryWarning")),
             "hasLifecycleOperation disagreed with its explanation") ||
      !check(flight::lifecycle::get_app_launch_kind(provider) == String("cold") &&
                 flight::lifecycle::get_app_launch_kind(bare_provider) ==
                     String("warm"),
             "launch-kind delegation or fallback changed")) {
    return 1;
  }

  int changes = 0;
  int pauses = 0;
  int resumes = 0;
  int saves = 0;
  int restores = 0;
  int memory_events = 0;
  String last_state;
  String last_pressure;
  double restored_value = 0.0;
  app->on_state_change->emit = [&](String value) {
    ++changes;
    last_state = std::move(value);
  };
  app->on_pause->emit = [&]() { ++pauses; };
  app->on_resume->emit = [&]() { ++resumes; };
  app->on_save_state->emit = [&](flight::lifecycle::StateBag bag) {
    ++saves;
    bag.set(String("answer"), flight::Any(42.0));
  };
  app->on_restore_state->emit = [&](flight::lifecycle::StateBag bag) {
    ++restores;
    const auto value = bag.get(String("answer"));
    restored_value = value.has_value() ? value->as_number() : -1.0;
  };
  app->on_memory_warning->emit = [&](String level) {
    ++memory_events;
    last_pressure = std::move(level);
  };

  flight::lifecycle::attach_app_lifecycle(provider, app);
  state = String("inactive");
  state_listener();
  state = String("background");
  state_listener();
  state = String("active");
  state_listener();
  memory_listener(String("critical"));
  if (!check(changes == 3 && last_state == String("active"),
             "state delivery was not raw per provider notification") ||
      !check(pauses == 1 && resumes == 1,
             "pause/resume did not follow active-boundary edges") ||
      !check(saves == 1 && restores == 1 && restored_value == 42.0,
             "saved state did not retain listener mutation for restore") ||
      !check(memory_events == 1 && last_pressure == String("critical"),
             "memory-pressure delivery changed")) {
    return 1;
  }

  flight::lifecycle::attach_app_lifecycle(provider, app);
  flight::lifecycle::detach_app_lifecycle(app);
  flight::lifecycle::detach_app_lifecycle(app);
  if (!check(state_unsubscribes == 2 && memory_unsubscribes == 2,
             "replacement or idempotent detach changed release ownership")) {
    return 1;
  }

  if (!check(flight::lifecycle::get_app_lifecycle_state(provider) ==
                 String("active") &&
                 flight::lifecycle::is_app_active(provider) &&
                 !flight::lifecycle::is_app_background(provider) &&
                 !flight::lifecycle::is_app_inactive(provider),
             "state query helpers changed")) {
    return 1;
  }

  int back_events = 0;
  const auto back_data =
      flight::make_ref<SignalData<std::function<void()>>>(
          SignalData<std::function<void()>>{
              .slots = {},
              .priorities = {},
              .repeat = {},
              .cancelled = false,
              .depth = 0.0,
          });
  app->on_back_button->data = back_data;
  app->on_back_button->emit = [&]() {
    ++back_events;
    back_data->cancelled = true;
  };
  if (!check(!flight::lifecycle::request_app_back(app) && back_events == 1,
             "back-button cancellation was not reported synchronously")) {
    return 1;
  }

  state = String("active");
  flight::lifecycle::attach_app_lifecycle(provider, app);
  state = String("background");
  state_listener();
  flight::lifecycle::dispose_app_lifecycle(app);
  const int restores_before_dispose_probe = restores;
  flight::lifecycle::attach_app_lifecycle(provider, app);
  state = String("active");
  state_listener();
  if (!check(restores == restores_before_dispose_probe,
             "dispose retained the saved state bag")) {
    return 1;
  }
  flight::lifecycle::dispose_app_lifecycle(app);

  return 0;
}
