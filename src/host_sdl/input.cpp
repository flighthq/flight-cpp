#include <flight/host_sdl/input.hpp>

#include <algorithm>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>

#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_mouse.h>

namespace flight::host_sdl {

namespace {

double milliseconds(std::uint64_t nanoseconds) noexcept {
  return static_cast<double>(nanoseconds) / 1'000'000.0;
}

std::uint32_t dom_mouse_buttons(SDL_MouseButtonFlags buttons) noexcept {
  std::uint32_t result = 0;
  if ((buttons & SDL_BUTTON_LMASK) != 0) result |= 1U;
  if ((buttons & SDL_BUTTON_RMASK) != 0) result |= 2U;
  if ((buttons & SDL_BUTTON_MMASK) != 0) result |= 4U;
  if ((buttons & SDL_BUTTON_X1MASK) != 0) result |= 8U;
  if ((buttons & SDL_BUTTON_X2MASK) != 0) result |= 16U;
  return result;
}

double dom_mouse_button(std::uint8_t button) noexcept {
  return button == 0 ? -1.0 : static_cast<double>(button - 1U);
}

double key_location(SDL_Scancode scancode) {
  switch (scancode) {
    case SDL_SCANCODE_LALT:
    case SDL_SCANCODE_LCTRL:
    case SDL_SCANCODE_LGUI:
    case SDL_SCANCODE_LSHIFT: return 1.0;
    case SDL_SCANCODE_RALT:
    case SDL_SCANCODE_RCTRL:
    case SDL_SCANCODE_RGUI:
    case SDL_SCANCODE_RSHIFT: return 2.0;
    default: break;
  }
  const char* name = SDL_GetScancodeName(scancode);
  return name != nullptr && std::string_view(name).starts_with("Keypad") ? 3.0 : 0.0;
}

String sdl_string(const char* value) { return String(value == nullptr ? "" : value); }

double normalized_gamepad_axis(std::int16_t value) noexcept {
  if (value < 0) {
    return static_cast<double>(value) /
           static_cast<double>(-std::numeric_limits<std::int16_t>::min());
  }
  return static_cast<double>(value) /
         static_cast<double>(std::numeric_limits<std::int16_t>::max());
}

std::optional<std::uint8_t> standard_gamepad_button(std::uint8_t button) noexcept {
  switch (static_cast<SDL_GamepadButton>(button)) {
    case SDL_GAMEPAD_BUTTON_SOUTH: return 0;
    case SDL_GAMEPAD_BUTTON_EAST: return 1;
    case SDL_GAMEPAD_BUTTON_WEST: return 2;
    case SDL_GAMEPAD_BUTTON_NORTH: return 3;
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: return 4;
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return 5;
    case SDL_GAMEPAD_BUTTON_BACK: return 8;
    case SDL_GAMEPAD_BUTTON_START: return 9;
    case SDL_GAMEPAD_BUTTON_LEFT_STICK: return 10;
    case SDL_GAMEPAD_BUTTON_RIGHT_STICK: return 11;
    case SDL_GAMEPAD_BUTTON_DPAD_UP: return 12;
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN: return 13;
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT: return 14;
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: return 15;
    case SDL_GAMEPAD_BUTTON_GUIDE: return 16;
    case SDL_GAMEPAD_BUTTON_TOUCHPAD: return 17;
    default: return std::nullopt;
  }
}

} // namespace

InputDispatcher::InputDispatcher(std::uint32_t window_id, InputSink sink)
    : window_id_(window_id), sink_(std::move(sink)) {
  if (window_id_ == 0) throw std::invalid_argument("SDL input dispatcher requires a window id");
}

bool InputDispatcher::dispatch(const SDL_Event& event) {
  if (!enabled()) return false;
  switch (event.type) {
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP: {
      if (!accepts_window(event.key.windowID)) return false;
      const auto modifier = static_cast<std::uint32_t>(event.key.mod);
      InputKeyboardData data{
          .alt_key = (event.key.mod & SDL_KMOD_ALT) != 0,
          .caps_lock = (event.key.mod & SDL_KMOD_CAPS) != 0,
          .code = sdl_string(SDL_GetScancodeName(event.key.scancode)),
          .ctrl_key = (event.key.mod & SDL_KMOD_CTRL) != 0,
          .key = sdl_string(SDL_GetKeyName(event.key.key)),
          .key_code = static_cast<double>(event.key.key),
          .location = key_location(event.key.scancode),
          .meta_key = (event.key.mod & SDL_KMOD_GUI) != 0,
          .modifier = static_cast<double>(modifier),
          .num_lock = (event.key.mod & SDL_KMOD_NUM) != 0,
          .repeat = event.key.repeat,
          .shift_key = (event.key.mod & SDL_KMOD_SHIFT) != 0,
          .time_stamp = milliseconds(event.key.timestamp),
      };
      const auto& callback = event.type == SDL_EVENT_KEY_DOWN ? sink_.key_down : sink_.key_up;
      if (callback) callback(data);
      return static_cast<bool>(callback);
    }
    case SDL_EVENT_TEXT_EDITING: {
      if (!accepts_window(event.edit.windowID) || !sink_.text_edit) return false;
      const InputTextData data{.is_composing = true, .text = sdl_string(event.edit.text)};
      sink_.text_edit(data);
      return true;
    }
    case SDL_EVENT_TEXT_INPUT: {
      if (!accepts_window(event.text.windowID) || !sink_.text_input) return false;
      const InputTextData data{.is_composing = false, .text = sdl_string(event.text.text)};
      sink_.text_input(data);
      return true;
    }
    case SDL_EVENT_MOUSE_MOTION: {
      if (!accepts_window(event.motion.windowID)) return false;
      mouse_buttons_ = event.motion.state;
      last_pointer_ = pointer_data(
          event.motion.timestamp,
          event.motion.which,
          event.motion.x,
          event.motion.y,
          -1.0,
          dom_mouse_buttons(mouse_buttons_),
          event.motion.xrel,
          event.motion.yrel,
          String("unknown"));
      bool delivered = false;
      if (sink_.pointer_move) {
        sink_.pointer_move(last_pointer_);
        delivered = true;
      }
      if (sink_.pointer_move_relative) {
        sink_.pointer_move_relative(last_pointer_);
        delivered = true;
      }
      return delivered;
    }
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP: {
      if (!accepts_window(event.button.windowID)) return false;
      const auto mask = SDL_BUTTON_MASK(event.button.button);
      if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) mouse_buttons_ |= mask;
      else mouse_buttons_ &= ~mask;
      last_pointer_ = pointer_data(
          event.button.timestamp,
          event.button.which,
          event.button.x,
          event.button.y,
          dom_mouse_button(event.button.button),
          dom_mouse_buttons(mouse_buttons_),
          0.0,
          0.0,
          String("unknown"));
      const auto& callback =
          event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? sink_.pointer_down : sink_.pointer_up;
      if (callback) callback(last_pointer_);
      return static_cast<bool>(callback);
    }
    case SDL_EVENT_MOUSE_WHEEL: {
      if (!accepts_window(event.wheel.windowID) || !sink_.wheel) return false;
      const auto direction = event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -1.0 : 1.0;
      last_pointer_ = pointer_data(
          event.wheel.timestamp,
          event.wheel.which,
          event.wheel.mouse_x,
          event.wheel.mouse_y,
          -1.0,
          dom_mouse_buttons(mouse_buttons_),
          direction * static_cast<double>(event.wheel.x),
          -direction * static_cast<double>(event.wheel.y),
          String("lines"));
      sink_.wheel(last_pointer_);
      return true;
    }
    case SDL_EVENT_WINDOW_MOUSE_LEAVE:
    case SDL_EVENT_WINDOW_FOCUS_LOST: {
      if (!accepts_window(event.window.windowID) || !sink_.pointer_cancel) return false;
      last_pointer_.buttons = 0.0;
      last_pointer_.pressure = 0.0;
      last_pointer_.time_stamp = milliseconds(event.window.timestamp);
      mouse_buttons_ = 0;
      sink_.pointer_cancel(last_pointer_);
      return true;
    }
    case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
      const auto axis = static_cast<SDL_GamepadAxis>(event.gaxis.axis);
      if (axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER || axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) {
        const std::uint8_t button = axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER ? 6 : 7;
        const auto value = std::clamp(
            static_cast<double>(event.gaxis.value) /
                static_cast<double>(std::numeric_limits<std::int16_t>::max()),
            0.0,
            1.0);
        const bool pressed = value > 0.5;
        const auto state_key =
            (static_cast<std::uint64_t>(event.gaxis.which) << 8U) | button;
        const auto previous = gamepad_trigger_state_.find(state_key);
        if (previous == gamepad_trigger_state_.end() && !pressed) {
          gamepad_trigger_state_.emplace(state_key, false);
          return false;
        }
        if (previous != gamepad_trigger_state_.end() && previous->second == pressed) return false;
        gamepad_trigger_state_.insert_or_assign(state_key, pressed);
        const auto& callback = pressed ? sink_.gamepad_button_down : sink_.gamepad_button_up;
        if (!callback) return false;
        const InputGamepadButtonData data{
            .button = static_cast<double>(button),
            .gamepad = static_cast<double>(event.gaxis.which),
            .time_stamp = milliseconds(event.gaxis.timestamp),
            .value = value,
        };
        callback(data);
        return true;
      }
      if (!sink_.gamepad_axis_move || event.gaxis.axis > SDL_GAMEPAD_AXIS_RIGHTY) return false;
      const InputGamepadAxisData data{
          .axis = static_cast<double>(event.gaxis.axis),
          .gamepad = static_cast<double>(event.gaxis.which),
          .time_stamp = milliseconds(event.gaxis.timestamp),
          .value = normalized_gamepad_axis(event.gaxis.value),
      };
      sink_.gamepad_axis_move(data);
      return true;
    }
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP: {
      const auto button = standard_gamepad_button(event.gbutton.button);
      if (!button) return false;
      const auto& callback = event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN
                                 ? sink_.gamepad_button_down
                                 : sink_.gamepad_button_up;
      if (!callback) return false;
      const InputGamepadButtonData data{
          .button = static_cast<double>(*button),
          .gamepad = static_cast<double>(event.gbutton.which),
          .time_stamp = milliseconds(event.gbutton.timestamp),
          .value = event.gbutton.down ? 1.0 : 0.0,
      };
      callback(data);
      return true;
    }
    case SDL_EVENT_GAMEPAD_ADDED:
    case SDL_EVENT_GAMEPAD_REMOVED: {
      const auto id = static_cast<std::uint32_t>(event.gdevice.which);
      String name;
      if (event.type == SDL_EVENT_GAMEPAD_ADDED) {
        name = sdl_string(SDL_GetGamepadNameForID(event.gdevice.which));
        gamepad_names_.insert_or_assign(id, name);
      } else {
        const auto existing = gamepad_names_.find(id);
        if (existing != gamepad_names_.end()) {
          name = existing->second;
          gamepad_names_.erase(existing);
        }
        const auto first_trigger = static_cast<std::uint64_t>(id) << 8U;
        gamepad_trigger_state_.erase(first_trigger | 6U);
        gamepad_trigger_state_.erase(first_trigger | 7U);
      }
      const InputGamepadConnectData data{
          .gamepad = static_cast<double>(event.gdevice.which),
          .id = std::move(name),
          .mapping = String("standard"),
      };
      const auto& callback = event.type == SDL_EVENT_GAMEPAD_ADDED
                                 ? sink_.gamepad_connect
                                 : sink_.gamepad_disconnect;
      if (callback) callback(data);
      return static_cast<bool>(callback);
    }
    default: return false;
  }
}

bool InputDispatcher::enabled() const { return !sink_.is_enabled || sink_.is_enabled(); }

bool InputDispatcher::accepts_window(std::uint32_t event_window_id) const noexcept {
  return event_window_id == window_id_;
}

InputPointerData InputDispatcher::pointer_data(
    std::uint64_t timestamp,
    std::uint32_t pointer_id,
    double x,
    double y,
    double button,
    std::uint32_t buttons,
    double delta_x,
    double delta_y,
    String wheel_mode) const {
  const auto modifiers = SDL_GetModState();
  return InputPointerData{
      .alt_key = (modifiers & SDL_KMOD_ALT) != 0,
      .button = button,
      .buttons = static_cast<double>(buttons),
      .ctrl_key = (modifiers & SDL_KMOD_CTRL) != 0,
      .delta_x = delta_x,
      .delta_y = delta_y,
      .height = 1.0,
      .is_primary = true,
      .meta_key = (modifiers & SDL_KMOD_GUI) != 0,
      .pointer_id = static_cast<double>(pointer_id),
      .pointer_type = pointer_id == SDL_TOUCH_MOUSEID ? String("touch") : String("mouse"),
      .pressure = buttons == 0 ? 0.0 : 0.5,
      .shift_key = (modifiers & SDL_KMOD_SHIFT) != 0,
      .tilt_x = 0.0,
      .tilt_y = 0.0,
      .time_stamp = milliseconds(timestamp),
      .twist = 0.0,
      .wheel_mode = std::move(wheel_mode),
      .width = 1.0,
      .x = x,
      .y = y,
  };
}

} // namespace flight::host_sdl
