#include <flight/host_sdl/sdk_keyboard.hpp>

#include <flight/host_sdl/window.hpp>
#include <flight/types/keyboard.hpp>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_video.h>

namespace flight::host_sdl {

struct SdkSoftKeyboardBackend::State final {
  struct Subscription final {
    std::function<void()> callback;
    bool active{true};
  };

  std::uint32_t window_id{};
  std::vector<std::shared_ptr<Subscription>> subscriptions;

  [[nodiscard]] SDL_Window* window() const {
    return SDL_GetWindowFromID(static_cast<SDL_WindowID>(window_id));
  }

  void prune() {
    std::erase_if(subscriptions, [](const auto& subscription) {
      return !subscription->active;
    });
  }
};

SdkSoftKeyboardBackend::SdkSoftKeyboardBackend(const Window& window)
    : state_(std::make_shared<State>()) {
  state_->window_id = static_cast<std::uint32_t>(window.id());
}

SdkSoftKeyboardBackend::~SdkSoftKeyboardBackend() noexcept = default;
SdkSoftKeyboardBackend::SdkSoftKeyboardBackend(const SdkSoftKeyboardBackend&) noexcept = default;
SdkSoftKeyboardBackend& SdkSoftKeyboardBackend::operator=(
    const SdkSoftKeyboardBackend&) noexcept = default;
SdkSoftKeyboardBackend::SdkSoftKeyboardBackend(SdkSoftKeyboardBackend&&) noexcept = default;
SdkSoftKeyboardBackend& SdkSoftKeyboardBackend::operator=(SdkSoftKeyboardBackend&&) noexcept = default;

flight::types::HostSoftKeyboardChangeCapability SdkSoftKeyboardBackend::change_backend() const {
  flight::types::HostSoftKeyboardChangeCapability result;
  result.subscribe = [state = state_](std::function<void()> callback) {
    auto output = flight::make_ref<flight::types::SoftKeyboardChangeSubscription>();
    output->result = flight::types::soft_keyboard_attach_acquisition_failed_kind;
    output->unsubscribe = std::nullopt;
    if (state->window() == nullptr || !SDL_HasScreenKeyboardSupport() || !callback) {
      return flight::Task<flight::Ref<flight::types::SoftKeyboardChangeSubscription>>::resolve(
          output);
    }

    auto subscription = std::make_shared<State::Subscription>();
    subscription->callback = std::move(callback);
    state->subscriptions.push_back(subscription);
    output->result = flight::types::soft_keyboard_attach_ok_kind;
    output->unsubscribe = [state, subscription] {
      if (!subscription->active) return;
      subscription->active = false;
      state->prune();
    };
    return flight::Task<flight::Ref<flight::types::SoftKeyboardChangeSubscription>>::resolve(
        output);
  };
  return result;
}

flight::types::HostSoftKeyboardInfoCapability SdkSoftKeyboardBackend::info_backend() const {
  flight::types::HostSoftKeyboardInfoCapability result;
  result.get_info = [state = state_](flight::Ref<flight::types::SoftKeyboardInfo> output) {
    if (output == nullptr) return output;
    SDL_Window* window = state->window();
    output->visible =
        window != nullptr && SDL_HasScreenKeyboardSupport() && SDL_ScreenKeyboardShown(window);
    output->height = 0.0;
    output->x = 0.0;
    output->y = 0.0;
    output->width = 0.0;
    return output;
  };
  return result;
}

flight::types::HostSoftKeyboardVisibilityCapability SdkSoftKeyboardBackend::visibility_backend() const {
  flight::types::HostSoftKeyboardVisibilityCapability result;
  result.show = [state = state_] {
    SDL_Window* window = state->window();
    const bool succeeded =
        window != nullptr && SDL_HasScreenKeyboardSupport() && SDL_StartTextInput(window);
    return flight::Task<flight::types::SoftKeyboardVisibilityResult>::resolve(
        succeeded ? flight::types::soft_keyboard_visibility_ok_kind
                  : flight::types::soft_keyboard_visibility_operation_failed_kind);
  };
  result.hide = [state = state_] {
    SDL_Window* window = state->window();
    const bool succeeded =
        window != nullptr && SDL_HasScreenKeyboardSupport() && SDL_StopTextInput(window);
    return flight::Task<flight::types::SoftKeyboardVisibilityResult>::resolve(
        succeeded ? flight::types::soft_keyboard_visibility_ok_kind
                  : flight::types::soft_keyboard_visibility_operation_failed_kind);
  };
  return result;
}

bool SdkSoftKeyboardBackend::dispatch(const SDL_Event& event) const {
  if (event.type != SDL_EVENT_SCREEN_KEYBOARD_SHOWN &&
      event.type != SDL_EVENT_SCREEN_KEYBOARD_HIDDEN) {
    return false;
  }
  if (state_->window() == nullptr) return false;
  const auto subscriptions = state_->subscriptions;
  for (const auto& subscription : subscriptions) {
    if (subscription->active && subscription->callback) subscription->callback();
  }
  state_->prune();
  return true;
}

} // namespace flight::host_sdl
