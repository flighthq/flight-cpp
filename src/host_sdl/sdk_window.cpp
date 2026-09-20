#include <flight/host_sdl/sdk_window.hpp>

#include <flight/host_sdl/window.hpp>
#include <flight/types/host_fullscreen.hpp>
#include <flight/types/host_input.hpp>
#include <flight/types/host_input_target.hpp>
#include <flight/weak_map.hpp>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_video.h>

namespace flight::host_sdl {

struct SdkWindowBackend::State final {
  struct FocusSubscription final {
    std::uint64_t id{};
    SDL_WindowID window_id{};
    std::function<void()> on_focus;
    std::function<void()> on_blur;
    bool active{true};
  };

  struct DropFileSubscription final {
    std::uint64_t id{};
    SDL_WindowID window_id{};
    std::function<void(flight::String)> listener;
    bool active{true};
  };

  explicit State(SDL_WindowID id) : window_id(id) {}

  void release_focus(std::uint64_t id) {
    for (const auto& subscription : focus_subscriptions) {
      if (subscription->id == id) subscription->active = false;
    }
    std::erase_if(focus_subscriptions, [](const auto& subscription) {
      return !subscription->active;
    });
  }

  void release_drop_file(std::uint64_t id) {
    for (const auto& subscription : drop_file_subscriptions) {
      if (subscription->id == id) subscription->active = false;
    }
    std::erase_if(drop_file_subscriptions, [](const auto& subscription) {
      return !subscription->active;
    });
  }

  SDL_WindowID window_id{};
  std::optional<SDL_WindowID> fullscreen_window_id;
  std::optional<SDL_WindowID> relative_pointer_window_id;
  // These maps resolve only opaque target handles minted by this State. They are provider-owned
  // handle registries, not storage for either capability record or its operational state.
  flight::WeakMap<flight::Ref<flight::types::FullscreenTargetHandle>, SDL_WindowID>
      fullscreen_targets;
  flight::WeakMap<flight::Ref<flight::types::InputTargetHandle>, SDL_WindowID> input_targets;
  std::vector<std::shared_ptr<FocusSubscription>> focus_subscriptions;
  std::vector<std::shared_ptr<DropFileSubscription>> drop_file_subscriptions;
  std::uint64_t next_subscription_id{1};
};

namespace {

template <typename Outcome>
[[nodiscard]] Outcome pointer_lock_outcome(const char* reason) {
  auto result = flight::make_ref<typename Outcome::element_type>();
  result->reason = flight::String(reason);
  return result;
}

} // namespace

SdkWindowBackend::SdkWindowBackend(const Window& window)
    : state_(std::make_shared<State>(window.id())) {}

SdkWindowBackend::~SdkWindowBackend() noexcept = default;

SdkWindowBackend::SdkWindowBackend(const SdkWindowBackend&) noexcept = default;

SdkWindowBackend& SdkWindowBackend::operator=(const SdkWindowBackend&) noexcept = default;

SdkWindowBackend::SdkWindowBackend(SdkWindowBackend&&) noexcept = default;

SdkWindowBackend& SdkWindowBackend::operator=(SdkWindowBackend&&) noexcept = default;

flight::types::HostElementFullscreenCapability SdkWindowBackend::fullscreen_backend() const {
  if (state_ == nullptr) throw std::logic_error("moved-from SDL SDK window backend");
  flight::types::HostElementFullscreenCapability result;
  const auto state = state_;
  result.exit = [state] {
    const SDL_WindowID id = state->fullscreen_window_id.value_or(state->window_id);
    SDL_Window* window = SDL_GetWindowFromID(id);
    if (window == nullptr) return flight::Task<bool>::resolve(false);
    const bool succeeded = SDL_SetWindowFullscreen(window, false);
    if (succeeded) state->fullscreen_window_id.reset();
    return flight::Task<bool>::resolve(succeeded);
  };
  result.request = [state](flight::Ref<flight::types::FullscreenTargetHandle> target) {
    if (target == nullptr) return flight::Task<bool>::resolve(false);
    const auto id = state->fullscreen_targets.get(target);
    if (!id.has_value()) return flight::Task<bool>::resolve(false);
    SDL_Window* window = SDL_GetWindowFromID(*id);
    if (window == nullptr) return flight::Task<bool>::resolve(false);
    const bool succeeded = SDL_SetWindowFullscreen(window, true);
    if (succeeded) state->fullscreen_window_id = *id;
    return flight::Task<bool>::resolve(succeeded);
  };
  return result;
}

flight::Ref<flight::types::FullscreenTargetHandle> SdkWindowBackend::fullscreen_target() const {
  if (state_ == nullptr) throw std::logic_error("moved-from SDL SDK window backend");
  auto target = flight::make_ref<flight::types::FullscreenTargetHandle>();
  target->brand = flight::String("FullscreenTargetHandle");
  static_cast<void>(state_->fullscreen_targets.set(target, state_->window_id));
  return target;
}

flight::types::HostInputDropFileCapability SdkWindowBackend::input_drop_file_backend() const {
  if (state_ == nullptr) throw std::logic_error("moved-from SDL SDK window backend");
  flight::types::HostInputDropFileCapability result;
  const auto state = state_;
  result.subscribe = [state](
                         flight::Ref<flight::types::InputTargetHandle> target,
                         std::function<void(flight::String)> listener) -> std::function<void()> {
    if (target == nullptr || !listener) return [] {};
    const auto window_id = state->input_targets.get(target);
    if (!window_id.has_value()) return [] {};
    const auto subscription = std::make_shared<State::DropFileSubscription>();
    subscription->id = state->next_subscription_id++;
    subscription->window_id = *window_id;
    subscription->listener = std::move(listener);
    state->drop_file_subscriptions.push_back(subscription);
    const std::weak_ptr<State> weak_state = state;
    const std::uint64_t id = subscription->id;
    return [weak_state, id] {
      if (const auto locked = weak_state.lock()) locked->release_drop_file(id);
    };
  };
  return result;
}

flight::types::HostInputFocusCapability SdkWindowBackend::input_focus_backend() const {
  if (state_ == nullptr) throw std::logic_error("moved-from SDL SDK window backend");
  flight::types::HostInputFocusCapability result;
  const auto state = state_;
  result.subscribe = [state](
                         flight::Ref<flight::types::InputTargetHandle> target,
                         std::function<void()> on_focus,
                         std::function<void()> on_blur) -> std::function<void()> {
    if (target == nullptr) return [] {};
    const auto window_id = state->input_targets.get(target);
    if (!window_id.has_value()) return [] {};
    const auto subscription = std::make_shared<State::FocusSubscription>();
    subscription->id = state->next_subscription_id++;
    subscription->window_id = *window_id;
    subscription->on_focus = std::move(on_focus);
    subscription->on_blur = std::move(on_blur);
    state->focus_subscriptions.push_back(subscription);
    const std::weak_ptr<State> weak_state = state;
    const std::uint64_t id = subscription->id;
    return [weak_state, id] {
      if (const auto locked = weak_state.lock()) locked->release_focus(id);
    };
  };
  return result;
}

flight::types::HostInputPointerLockCapability SdkWindowBackend::input_pointer_lock_backend() const {
  if (state_ == nullptr) throw std::logic_error("moved-from SDL SDK window backend");
  flight::types::HostInputPointerLockCapability result;
  const auto state = state_;
  result.exit = [state] {
    if (!state->relative_pointer_window_id.has_value()) {
      return flight::Task<flight::types::InputPointerLockExitOutcome>::resolve(
          pointer_lock_outcome<flight::types::InputPointerLockExitOutcome>("ok"));
    }
    SDL_Window* window = SDL_GetWindowFromID(*state->relative_pointer_window_id);
    if (window == nullptr) {
      state->relative_pointer_window_id.reset();
      return flight::Task<flight::types::InputPointerLockExitOutcome>::resolve(
          pointer_lock_outcome<flight::types::InputPointerLockExitOutcome>("operation-failed"));
    }
    const bool succeeded = SDL_SetWindowRelativeMouseMode(window, false);
    if (succeeded) state->relative_pointer_window_id.reset();
    return flight::Task<flight::types::InputPointerLockExitOutcome>::resolve(
        pointer_lock_outcome<flight::types::InputPointerLockExitOutcome>(
            succeeded ? "ok" : "operation-failed"));
  };
  result.request = [state](flight::Ref<flight::types::InputTargetHandle> target) {
    if (target == nullptr) {
      return flight::Task<flight::types::InputPointerLockRequestOutcome>::resolve(
          pointer_lock_outcome<flight::types::InputPointerLockRequestOutcome>("target-not-found"));
    }
    const auto window_id = state->input_targets.get(target);
    if (!window_id.has_value()) {
      return flight::Task<flight::types::InputPointerLockRequestOutcome>::resolve(
          pointer_lock_outcome<flight::types::InputPointerLockRequestOutcome>("target-not-found"));
    }
    SDL_Window* window = SDL_GetWindowFromID(*window_id);
    if (window == nullptr) {
      return flight::Task<flight::types::InputPointerLockRequestOutcome>::resolve(
          pointer_lock_outcome<flight::types::InputPointerLockRequestOutcome>("operation-failed"));
    }
    const bool succeeded = SDL_SetWindowRelativeMouseMode(window, true);
    if (succeeded) state->relative_pointer_window_id = *window_id;
    return flight::Task<flight::types::InputPointerLockRequestOutcome>::resolve(
        pointer_lock_outcome<flight::types::InputPointerLockRequestOutcome>(
            succeeded ? "ok" : "operation-failed"));
  };
  return result;
}

flight::types::HostInputTargetCapability SdkWindowBackend::input_target_backend() const {
  if (state_ == nullptr) throw std::logic_error("moved-from SDL SDK window backend");
  flight::types::HostInputTargetCapability result;
  const auto state = state_;
  result.prepare = [state](flight::Ref<flight::types::InputTargetHandle> target) {
    if (target != nullptr) static_cast<void>(state->input_targets.get(target));
  };
  return result;
}

flight::Ref<flight::types::InputTargetHandle> SdkWindowBackend::input_target() const {
  if (state_ == nullptr) throw std::logic_error("moved-from SDL SDK window backend");
  auto target = flight::make_ref<flight::types::InputTargetHandle>();
  target->brand = flight::String("InputTargetHandle");
  static_cast<void>(state_->input_targets.set(target, state_->window_id));
  return target;
}

bool SdkWindowBackend::dispatch(const SDL_Event& event) const {
  if (state_ == nullptr) return false;
  if (event.type == SDL_EVENT_WINDOW_FOCUS_GAINED || event.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
    const auto subscriptions = state_->focus_subscriptions;
    for (const auto& subscription : subscriptions) {
      if (!subscription->active || subscription->window_id != event.window.windowID) continue;
      const auto& callback = event.type == SDL_EVENT_WINDOW_FOCUS_GAINED
                                 ? subscription->on_focus
                                 : subscription->on_blur;
      if (callback) callback();
    }
    return event.window.windowID == state_->window_id;
  }
  if (event.type == SDL_EVENT_DROP_FILE) {
    const auto subscriptions = state_->drop_file_subscriptions;
    for (const auto& subscription : subscriptions) {
      if (!subscription->active || subscription->window_id != event.drop.windowID ||
          event.drop.data == nullptr) {
        continue;
      }
      subscription->listener(flight::String(event.drop.data));
    }
    return event.drop.windowID == state_->window_id;
  }
  return false;
}

} // namespace flight::host_sdl
