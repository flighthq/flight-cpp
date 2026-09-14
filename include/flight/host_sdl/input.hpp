#pragma once

#include <any>
#include <cstdint>
#include <functional>
#include <optional>
#include <unordered_map>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gamepad.h>

#include <flight/array.hpp>
#include <flight/host_sdl/export.hpp>
#include <flight/string.hpp>

namespace flight::host_sdl {

inline constexpr double keyboard_location_standard = 0.0;
inline constexpr double keyboard_location_left = 1.0;
inline constexpr double keyboard_location_right = 2.0;
inline constexpr double keyboard_location_numpad = 3.0;

inline constexpr double wheel_delta_pixel = 0.0;
inline constexpr double wheel_delta_line = 1.0;
inline constexpr double wheel_delta_page = 2.0;

struct GamepadButtonSnapshot final {
  bool pressed{false};
  bool touched{false};
  double value{0.0};
};

struct GamepadSnapshot final {
  Array<double> axes{4};
  Array<GamepadButtonSnapshot> buttons{18};
  String id;
  double index{0.0};
  String mapping{"standard"};
};

struct InputKeyboardData final {
  bool alt_key{false};
  bool caps_lock{false};
  String code;
  bool ctrl_key{false};
  String key;
  double key_code{0.0};
  double location{0.0};
  bool meta_key{false};
  double modifier{0.0};
  bool num_lock{false};
  bool repeat{false};
  bool shift_key{false};
  double time_stamp{0.0};
  bool default_prevented{false};

  void prevent_default() noexcept { default_prevented = true; }
  [[nodiscard]] bool get_modifier_state(const String& modifier) const {
    if (modifier == String("CapsLock")) return caps_lock;
    if (modifier == String("NumLock")) return num_lock;
    return false;
  }
};

struct InputPointerData final {
  bool alt_key{false};
  double button{0.0};
  double buttons{0.0};
  bool ctrl_key{false};
  double delta_x{0.0};
  double delta_y{0.0};
  double height{1.0};
  bool is_primary{true};
  bool meta_key{false};
  double pointer_id{0.0};
  String pointer_type{"mouse"};
  double pressure{0.0};
  bool shift_key{false};
  double tilt_x{0.0};
  double tilt_y{0.0};
  double time_stamp{0.0};
  double twist{0.0};
  String wheel_mode{"unknown"};
  double width{1.0};
  double x{0.0};
  double y{0.0};
  double client_x{0.0};
  double client_y{0.0};
  bool default_prevented{false};

  void prevent_default() noexcept { default_prevented = true; }
  [[nodiscard]] Array<InputPointerData> get_coalesced_events() const { return {}; }
};

struct InputTextData final {
  bool is_composing{false};
  String text;
};

struct InputGamepadAxisData final {
  double axis{0.0};
  double gamepad{0.0};
  double time_stamp{0.0};
  double value{0.0};
};

struct InputGamepadButtonData final {
  double button{0.0};
  double gamepad{0.0};
  double time_stamp{0.0};
  double value{0.0};
};

struct InputGamepadConnectData final {
  double gamepad{0.0};
  String id;
  String mapping;
};

template <typename Detail>
struct DomCustomEvent final {
  Detail detail;
  bool default_prevented{false};

  void prevent_default() noexcept { default_prevented = true; }
};

// Common browser-event carrier used where Flight accepts Event and narrows it to a concrete input
// event. SDL dispatch remains strongly typed; these conversions preserve the fields visible after
// the corresponding TypeScript cast.
struct DomEvent final {
  bool alt_key{false};
  double button{0.0};
  double buttons{0.0};
  bool caps_lock{false};
  String code;
  bool ctrl_key{false};
  std::optional<String> data;
  std::any detail;
  double delta_x{0.0};
  double delta_y{0.0};
  bool default_prevented{false};
  GamepadSnapshot gamepad;
  double height{1.0};
  bool is_composing{false};
  bool is_primary{true};
  String key;
  double key_code{0.0};
  double location{0.0};
  bool meta_key{false};
  double modifier{0.0};
  bool num_lock{false};
  double pointer_id{0.0};
  String pointer_type{"mouse"};
  double pressure{0.0};
  bool repeat{false};
  bool shift_key{false};
  String text;
  double tilt_x{0.0};
  double tilt_y{0.0};
  double time_stamp{0.0};
  double twist{0.0};
  String wheel_mode{"unknown"};
  double width{1.0};
  double x{0.0};
  double y{0.0};
  double client_x{0.0};
  double client_y{0.0};

  void prevent_default() noexcept { default_prevented = true; }
  [[nodiscard]] operator InputKeyboardData() const;
  [[nodiscard]] operator InputPointerData() const;

  template <typename Detail>
  [[nodiscard]] operator DomCustomEvent<Detail>() const {
    return DomCustomEvent<Detail>{.detail = std::any_cast<Detail>(detail)};
  }
};

class FLIGHT_HOST_SDL_API GamepadNavigator final {
 public:
  [[nodiscard]] Array<std::optional<GamepadSnapshot>> get_gamepads() const;
};

extern FLIGHT_HOST_SDL_API GamepadNavigator navigator;

// Synchronous borrowed delivery matching Flight's InputIngressSink shape. Callbacks may be left
// empty when an application has not attached that input category.
struct InputSink final {
  std::function<bool()> is_enabled;
  std::function<void(const InputKeyboardData&)> key_down;
  std::function<void(const InputKeyboardData&)> key_up;
  std::function<void(const InputPointerData&)> pointer_cancel;
  std::function<void(const InputPointerData&)> pointer_down;
  std::function<void(const InputPointerData&)> pointer_move;
  std::function<void(const InputPointerData&)> pointer_move_relative;
  std::function<void(const InputPointerData&)> pointer_up;
  std::function<void(const InputPointerData&)> wheel;
  std::function<void(const InputTextData&)> text_edit;
  std::function<void(const InputTextData&)> text_input;
  std::function<void(const InputGamepadAxisData&)> gamepad_axis_move;
  std::function<void(const InputGamepadButtonData&)> gamepad_button_down;
  std::function<void(const InputGamepadButtonData&)> gamepad_button_up;
  std::function<void(const InputGamepadConnectData&)> gamepad_connect;
  std::function<void(const InputGamepadConnectData&)> gamepad_disconnect;
};

// Converts SDL events for one window into the same normalized records consumed by Flight's ingress
// backend. It owns no event loop: pass events obtained from Host::poll_event or Host::wait_event.
class FLIGHT_HOST_SDL_API InputDispatcher final {
 public:
  explicit InputDispatcher(std::uint32_t window_id, InputSink sink);
  ~InputDispatcher();

  InputDispatcher(const InputDispatcher&) = delete;
  InputDispatcher& operator=(const InputDispatcher&) = delete;
  InputDispatcher(InputDispatcher&&) = delete;
  InputDispatcher& operator=(InputDispatcher&&) = delete;

  [[nodiscard]] std::uint32_t window_id() const noexcept { return window_id_; }
  [[nodiscard]] bool dispatch(const SDL_Event& event);

 private:
  [[nodiscard]] bool enabled() const;
  [[nodiscard]] bool accepts_window(std::uint32_t event_window_id) const noexcept;
  [[nodiscard]] InputPointerData pointer_data(
      std::uint64_t timestamp,
      std::uint32_t pointer_id,
      double x,
      double y,
      double button,
      std::uint32_t buttons,
      double delta_x,
      double delta_y,
      String wheel_mode) const;

  std::uint32_t window_id_;
  InputSink sink_;
  InputPointerData last_pointer_;
  SDL_MouseButtonFlags mouse_buttons_{0};
  std::unordered_map<std::uint32_t, SDL_Gamepad*> gamepads_;
  std::unordered_map<std::uint32_t, String> gamepad_names_;
  std::unordered_map<std::uint64_t, bool> gamepad_trigger_state_;
};

} // namespace flight::host_sdl
