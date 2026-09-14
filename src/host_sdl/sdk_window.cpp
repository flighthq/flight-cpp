#include <flight/host_sdl/sdk_window.hpp>

#include <flight/host_sdl/window.hpp>
#include <flight/types/application_visibility_backend.hpp>
#include <flight/types/fullscreen_backend.hpp>
#include <flight/weak_map.hpp>

#include <optional>
#include <stdexcept>
#include <utility>

#include <SDL3/SDL_video.h>

namespace flight::host_sdl {

struct SdkWindowBackend::State final {
  explicit State(SDL_WindowID id) : window_id(id) {}

  SDL_WindowID window_id{};
  std::optional<SDL_WindowID> fullscreen_window_id;
  flight::WeakMap<flight::Ref<flight::types::FullscreenTargetHandle>, SDL_WindowID>
      fullscreen_targets;
};

SdkWindowBackend::SdkWindowBackend(const Window& window)
    : state_(std::make_shared<State>(window.id())) {}

SdkWindowBackend::~SdkWindowBackend() noexcept = default;

SdkWindowBackend::SdkWindowBackend(const SdkWindowBackend&) noexcept = default;

SdkWindowBackend& SdkWindowBackend::operator=(const SdkWindowBackend&) noexcept = default;

SdkWindowBackend::SdkWindowBackend(SdkWindowBackend&&) noexcept = default;

SdkWindowBackend& SdkWindowBackend::operator=(SdkWindowBackend&&) noexcept = default;

flight::types::ApplicationVisibilityBackend SdkWindowBackend::visibility_backend() const {
  if (state_ == nullptr) throw std::logic_error("moved-from SDL SDK window backend");
  flight::types::ApplicationVisibilityBackend result;
  const auto state = state_;
  result.is_visible = [state] {
    SDL_Window* window = SDL_GetWindowFromID(state->window_id);
    if (window == nullptr) return false;
    constexpr SDL_WindowFlags invisible = SDL_WINDOW_HIDDEN | SDL_WINDOW_MINIMIZED;
    return (SDL_GetWindowFlags(window) & invisible) == 0;
  };
  return result;
}

flight::types::FullscreenBackend SdkWindowBackend::fullscreen_backend() const {
  if (state_ == nullptr) throw std::logic_error("moved-from SDL SDK window backend");
  flight::types::FullscreenBackend result;
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

} // namespace flight::host_sdl
