#include <flight/host_sdl/sdk_haptics.hpp>

#include <flight/types/haptics.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>

#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_stdinc.h>

namespace flight::host_sdl {
namespace {

[[nodiscard]] std::uint16_t rumble_strength(double intensity) {
  const double normalized = std::clamp(std::isfinite(intensity) ? intensity : 0.0, 0.0, 1.0);
  return static_cast<std::uint16_t>(
      std::round(normalized * static_cast<double>(std::numeric_limits<std::uint16_t>::max())));
}

[[nodiscard]] std::uint32_t rumble_duration(double duration_ms) {
  if (!std::isfinite(duration_ms) || duration_ms <= 0.0) return 0;
  const double maximum = static_cast<double>(std::numeric_limits<std::uint32_t>::max());
  return static_cast<std::uint32_t>(std::clamp(std::round(duration_ms), 1.0, maximum));
}

} // namespace

struct SdkHapticsBackend::State final {
  ~State() noexcept {
    std::lock_guard lock(mutex);
    if (gamepad != nullptr) {
      SDL_RumbleGamepad(gamepad, 0, 0, 0);
      SDL_CloseGamepad(gamepad);
    }
  }

  [[nodiscard]] SDL_Gamepad* find_gamepad() {
    if (gamepad != nullptr && SDL_GamepadConnected(gamepad)) return gamepad;
    if (gamepad != nullptr) {
      SDL_CloseGamepad(gamepad);
      gamepad = nullptr;
    }

    int count = 0;
    SDL_JoystickID* gamepads = SDL_GetGamepads(&count);
    for (int index = 0; gamepads != nullptr && index < count; ++index) {
      SDL_Gamepad* candidate = SDL_OpenGamepad(gamepads[index]);
      if (candidate == nullptr) continue;
      const SDL_PropertiesID properties = SDL_GetGamepadProperties(candidate);
      if (properties != 0 &&
          SDL_GetBooleanProperty(properties, SDL_PROP_GAMEPAD_CAP_RUMBLE_BOOLEAN, false)) {
        gamepad = candidate;
        break;
      }
      SDL_CloseGamepad(candidate);
    }
    SDL_free(gamepads);
    return gamepad;
  }

  [[nodiscard]] bool supported() {
    std::lock_guard lock(mutex);
    return find_gamepad() != nullptr;
  }

  [[nodiscard]] bool rumble(double intensity, double duration_ms) {
    const std::uint32_t duration = rumble_duration(duration_ms);
    if (duration == 0) return false;
    std::lock_guard lock(mutex);
    SDL_Gamepad* target = find_gamepad();
    if (target == nullptr) return false;
    const std::uint16_t strength = rumble_strength(intensity);
    return SDL_RumbleGamepad(target, strength, strength, duration);
  }

  [[nodiscard]] bool cancel() {
    std::lock_guard lock(mutex);
    SDL_Gamepad* target = find_gamepad();
    return target != nullptr && SDL_RumbleGamepad(target, 0, 0, 0);
  }

  std::mutex mutex;
  SDL_Gamepad* gamepad{nullptr};
};

SdkHapticsBackend::SdkHapticsBackend() : state_(std::make_shared<State>()) {}
SdkHapticsBackend::~SdkHapticsBackend() noexcept = default;
SdkHapticsBackend::SdkHapticsBackend(const SdkHapticsBackend&) noexcept = default;
SdkHapticsBackend& SdkHapticsBackend::operator=(const SdkHapticsBackend&) noexcept = default;
SdkHapticsBackend::SdkHapticsBackend(SdkHapticsBackend&&) noexcept = default;
SdkHapticsBackend& SdkHapticsBackend::operator=(SdkHapticsBackend&&) noexcept = default;

flight::types::HostHapticsProvider SdkHapticsBackend::backend() const {
  flight::types::HostHapticsProvider result;
  result.entity_runtime_key = std::nullopt;
  result.cancel = [state = state_] { return state->cancel(); };
  result.capabilities = [state = state_](flight::Ref<flight::types::HapticsCapabilities> output) {
    if (output == nullptr) return output;
    const bool supported = state->supported();
    output->amplitude_control = supported;
    output->custom_events = false;
    output->intensity = supported;
    output->patterns = false;
    output->supported = supported;
    return output;
  };
  result.impact = [state = state_](flight::String style, std::optional<double> intensity) {
    double duration = 10.0;
    if (style == flight::String("medium")) duration = 20.0;
    else if (style == flight::String("heavy") || style == flight::String("rigid")) duration = 30.0;
    else if (style == flight::String("soft")) duration = 25.0;
    return state->rumble(intensity.value_or(1.0), duration);
  };
  result.is_supported = [state = state_] { return state->supported(); };
  result.notification = [state = state_](flight::types::HapticNotificationType type) {
    const double duration = type == flight::String("error")
                                ? 40.0
                                : (type == flight::String("warning") ? 30.0 : 20.0);
    return state->rumble(1.0, duration);
  };
  result.prepare = [state = state_] { static_cast<void>(state->supported()); };
  result.selection = [state = state_] { return state->rumble(0.35, 5.0); };
  result.vibrate = [state = state_](double duration_ms) {
    return state->rumble(1.0, duration_ms);
  };
  result.vibrate_pattern = [](flight::Array<double>) { return false; };
  result.vibrate_waveform = std::nullopt;
  return result;
}

} // namespace flight::host_sdl
