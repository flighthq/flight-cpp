#pragma once

#include <cstdint>
#include <functional>
#include <unordered_map>

#include <SDL3/SDL_events.h>

#include <flight/host_sdl/export.hpp>
#include <flight/string.hpp>

namespace flight::host_sdl {

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
  std::unordered_map<std::uint32_t, String> gamepad_names_;
  std::unordered_map<std::uint64_t, bool> gamepad_trigger_state_;
};

} // namespace flight::host_sdl
