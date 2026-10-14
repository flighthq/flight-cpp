// Native parity test for @flighthq/input's dead-zone contract -- complementary to the existing input test.
//
// tests/generated_input_test.cpp already covers the key-repeat timer, and it is untouched here. This file covers the
// other closed-form half of the module: the axis and radial stick dead zones, ported from inputManager.test.ts's own
// cases for them. They are pure, documented, and were otherwise uncovered.
//
//   * applyGamepadAxisDeadZone: a value inside [-deadZone, deadZone] maps to 0, INCLUDING exactly at the boundary; a
//     value outside is rescaled linearly onto [-1, 1] so the live range stays continuous -- full deflection is still
//     exactly 1, and a dead zone of 0 returns the raw value rather than clamping it.
//   * applyGamepadStickDeadZone: the RADIAL magnitude is what is compared, not either axis, so a diagonal input just
//     outside the zone is live while an axis-aligned one of the same magnitude is not; the direction is preserved and
//     the magnitude is rescaled onto [0, 1]. It is documented as alias-safe, which is asserted rather than assumed:
//     passing the output object's own fields as the inputs must give the answer the inputs imply.
//
// A LATER unit discharged part of what this header used to defer. Mapped mechanically against inputManager.test.ts's
// 63 cases, the STATE half of the module was covered by NEITHER local test -- between them they called exactly one
// manager function -- so the constructors, the frame boundary, the `connectInputStateToInputManager` transitions, the
// is_/was_ queries and the gamepad name maps are covered below. The deferral is now narrower and still honest: what
// remains uncovered is the SIX INGRESS ATTACHMENT FAMILIES (gamepad, keyboard, pointer, relative-pointer, text,
// wheel) with their detaches and the explicit-ingress identity cases, which need a host seam this file does not
// build.
#include <flight/input/input_manager.hpp>
#include <flight/signals/slot.hpp>

#include <cmath>
#include <iostream>
#include <optional>

namespace {

namespace input = flight::input;
namespace types = flight::types;

int failures = 0;

void check(bool condition, const char* message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

bool close(double value, double expected, double tolerance = 1e-12) {
  return std::abs(value - expected) <= tolerance * std::max(1.0, std::abs(expected));
}

// The axis dead zone: zero inside, a continuous linear rescale outside.
void test_axis_dead_zone() {
  check(input::apply_gamepad_axis_dead_zone(0.0, 0.2) == 0.0, "an axis value inside the zone is zero");
  check(input::apply_gamepad_axis_dead_zone(0.2, 0.2) == 0.0, "exactly at the boundary is still inside it");
  check(input::apply_gamepad_axis_dead_zone(-0.2, 0.2) == 0.0, "and so is the negative boundary");
  check(input::apply_gamepad_axis_dead_zone(0.1, 0.2) == 0.0, "and anything between them");

  check(close(input::apply_gamepad_axis_dead_zone(1.0, 0.2), 1.0), "full deflection stays full deflection");
  check(close(input::apply_gamepad_axis_dead_zone(0.6, 0.2), 0.5),
        "and the live range is rescaled linearly between the zone and 1");
  check(close(input::apply_gamepad_axis_dead_zone(-0.6, 0.2), -0.5), "the rescale preserves the sign");
  check(input::apply_gamepad_axis_dead_zone(0.5, 0.2) > 0.0 && input::apply_gamepad_axis_dead_zone(0.5, 0.2) < 1.0,
        "a value just outside the zone lands strictly inside the live range");

  check(input::apply_gamepad_axis_dead_zone(0.7, 0.0) == 0.7, "a dead zone of zero returns the raw value");
  check(input::apply_gamepad_axis_dead_zone(-0.35, -1.0) == -0.35, "and so does a nonsensical negative zone");
}

// The stick dead zone is radial: the magnitude decides, not either axis.
void test_stick_dead_zone_is_radial() {
  const auto out = flight::make_ref<types::x_y_8365950bd60f783f>(types::x_y_8365950bd60f783f{.x = 0.0, .y = 0.0});
  const double zone = 0.2;

  input::apply_gamepad_stick_dead_zone(out, 0.1, 0.1, zone);
  check(out->x == 0.0 && out->y == 0.0, "a stick inside the radial zone reads as centred");

  // Magnitude 0.3, axis-aligned: live, and rescaled to (0, 1).
  input::apply_gamepad_stick_dead_zone(out, 0.3, 0.0, zone);
  check(out->x > 0.0 && out->y == 0.0, "an axis-aligned push outside the zone is live on that axis only");
  check(close(out->x, (0.3 - zone) / (1.0 - zone)), "and its magnitude is the linear rescale");

  // The same magnitude on the diagonal is live too -- radial, not per-axis.
  const double diagonal = 0.3 / std::sqrt(2.0);
  input::apply_gamepad_stick_dead_zone(out, diagonal, diagonal, zone);
  check(out->x > 0.0 && out->y > 0.0, "a diagonal push of the same magnitude is live as well");
  check(close(std::sqrt(out->x * out->x + out->y * out->y), (0.3 - zone) / (1.0 - zone)),
        "and its rescaled magnitude matches the axis-aligned one, which per-axis zones would not");

  input::apply_gamepad_stick_dead_zone(out, 1.0, 0.0, zone);
  check(close(out->x, 1.0) && out->y == 0.0, "full deflection reaches exactly 1");
  input::apply_gamepad_stick_dead_zone(out, 0.4, 0.3, 0.0);
  check(close(out->x, 0.4) && close(out->y, 0.3), "a dead zone of zero passes the input through");
}

// The documented alias-safety: the result must come from the inputs, not from the object being written.
void test_stick_dead_zone_is_alias_safe() {
  const auto value = flight::make_ref<types::x_y_8365950bd60f783f>(types::x_y_8365950bd60f783f{.x = 0.6, .y = 0.0});
  const double zone = 0.2;
  const double expected = (0.6 - zone) / (1.0 - zone);

  // The output object IS the input: its own fields are read for x and y before anything is written.
  input::apply_gamepad_stick_dead_zone(value, value->x, value->y, zone);
  check(close(value->x, expected) && value->y == 0.0,
        "passing the output's own fields as the inputs still answers from those inputs");
}

}  // namespace


// --- The state half of the same module ------------------------------------------------------------------------
//
// Every read below goes through `connectInputStateToInputManager`, so the CONNECTION is what populates the state
// rather than the test writing into it directly: a connect that dropped a transition would fail here.

using State = types::InputState;
using StateRow = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<State>>>>;
using KeyRow = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<types::InputKeyboardData>>>>;
using PointerRow = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<types::InputPointerData>>>>;
using ButtonRow = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<types::InputGamepadButtonData>>>>;
using AxisRow = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<types::InputGamepadAxisData>>>>;

void test_the_state_and_manager_constructors_start_where_the_source_says() {
  const auto state = input::create_input_state();
  check(state->keys_down.size() == 0.0 && state->just_pressed_keys.size() == 0.0 &&
            state->just_released_keys.size() == 0.0 && state->gamepad_buttons_down.size() == 0.0 &&
            state->just_pressed_gamepad_buttons.size() == 0.0 &&
            state->just_released_gamepad_buttons.size() == 0.0 && state->pointer_buttons_down.size() == 0.0 &&
            state->axis_values.size() == 0.0,
        "createInputState starts with every map and set empty");

  const auto manager = input::create_input_manager();
  check(manager->enabled, "createInputManager is enabled by default");
  check(manager->on_key_down != nullptr && manager->on_key_up != nullptr &&
            manager->on_pointer_down != nullptr && manager->on_pointer_up != nullptr &&
            manager->on_pointer_cancel != nullptr && manager->on_wheel != nullptr &&
            manager->on_gamepad_axis_move != nullptr && manager->on_gamepad_button_down != nullptr &&
            manager->on_gamepad_button_up != nullptr && manager->on_gamepad_connect != nullptr &&
            manager->on_gamepad_disconnect != nullptr,
        "and carries every input signal it publishes through");

  const auto signals = input::create_input_signals();
  check(signals->on_key_down != nullptr && signals->on_key_up != nullptr &&
            signals->on_pointer_down != nullptr && signals->on_pointer_up != nullptr &&
            signals->on_pointer_cancel != nullptr && signals->on_pointer_move != nullptr &&
            signals->on_pointer_move_relative != nullptr && signals->on_wheel != nullptr &&
            signals->on_text_input != nullptr && signals->on_text_edit != nullptr &&
            signals->on_gamepad_axis_move != nullptr && signals->on_gamepad_button_down != nullptr &&
            signals->on_gamepad_button_up != nullptr && signals->on_gamepad_connect != nullptr &&
            signals->on_gamepad_disconnect != nullptr,
        "createInputSignals creates all fifteen signals, not a subset of them");
}

void test_connect_tracks_the_keyboard_transitions() {
  const auto state = input::create_input_state();
  const auto manager = input::create_input_manager();
  const auto keep_alive = input::connect_input_state_to_input_manager(state, manager);
  (void)keep_alive;
  const auto state_row = StateRow(state);

  const auto press = flight::make_ref<types::InputKeyboardData>();
  press->key_code = 65.0;
  press->repeat = false;
  const auto repeat = flight::make_ref<types::InputKeyboardData>();
  repeat->key_code = 65.0;
  repeat->repeat = true;
  const auto release = flight::make_ref<types::InputKeyboardData>();
  release->key_code = 65.0;
  release->repeat = false;

  flight::signals::emit_signal<std::function<void(KeyRow)>>(manager->on_key_down, KeyRow(press));
  check(input::is_input_key_down(state_row, 65.0), "a keydown holds the key down");
  check(input::was_input_key_pressed(state_row, 65.0), "and reports it as pressed this frame");

  input::end_input_state_frame(state);
  check(input::was_input_key_pressed(state_row, 65.0) == false,
        "endInputStateFrame clears the pressed edge");
  check(input::is_input_key_down(state_row, 65.0),
        "while the key is still HELD, which the frame boundary does not clear");

  flight::signals::emit_signal<std::function<void(KeyRow)>>(manager->on_key_down, KeyRow(repeat));
  check(input::was_input_key_pressed(state_row, 65.0) == false,
        "an auto-repeat keydown of a key that is already held is NOT a press");

  flight::signals::emit_signal<std::function<void(KeyRow)>>(manager->on_key_up, KeyRow(release));
  check(input::was_input_key_released(state_row, 65.0), "a keyup reports the released edge");
  check(input::is_input_key_down(state_row, 65.0) == false, "and the key is no longer held");

  input::end_input_state_frame(state);
  check(input::was_input_key_released(state_row, 65.0) == false,
        "which endInputStateFrame clears too, so the edge lasts one frame");

  flight::signals::emit_signal<std::function<void(KeyRow)>>(manager->on_key_down, KeyRow(press));
  flight::signals::emit_signal<std::function<void(KeyRow)>>(manager->on_key_up, KeyRow(release));
  check(input::was_input_key_pressed(state_row, 65.0) && input::was_input_key_released(state_row, 65.0),
        "a key pressed and released within ONE frame reports BOTH edges");

  input::end_input_state_frame(state);
  flight::signals::emit_signal<std::function<void(KeyRow)>>(manager->on_key_up, KeyRow(release));
  flight::signals::emit_signal<std::function<void(KeyRow)>>(manager->on_key_down, KeyRow(press));
  check(input::was_input_key_pressed(state_row, 65.0) && input::was_input_key_released(state_row, 65.0),
        "and so does a key released and pressed again within one frame");

  // The DISPOSER is NOT called here, and that case is recorded rather than asserted: `disconnect()`
  // aborts with `callable identity is unavailable after std::function erasure`, which is
  // docs/compiler-requests/local-handler-callable-identity.md -- a handler connected from a local
  // closure cannot be disconnected by its own cleanup closure, because the emitted `std::function`
  // erased the identity the disconnect compares. So `connectInputStateToInputManager :: returns a
  // disposer that stops tracking` is UNREACHABLE at this pin, and no workaround is carried.
}

void test_connect_tracks_pointer_buttons_and_cancel() {
  const auto state = input::create_input_state();
  const auto manager = input::create_input_manager();
  const auto keep_alive = input::connect_input_state_to_input_manager(state, manager);
  (void)keep_alive;
  const auto state_row = StateRow(state);

  const auto down = flight::make_ref<types::InputPointerData>();
  down->pointer_id = 1.0;
  down->button = 2.0;
  const auto up = flight::make_ref<types::InputPointerData>();
  up->pointer_id = 1.0;
  up->button = 2.0;

  check(input::is_input_pointer_button_down(state_row, 1.0, 2.0) == false, "a button starts up");
  flight::signals::emit_signal<std::function<void(PointerRow)>>(manager->on_pointer_down, PointerRow(down));
  check(input::is_input_pointer_button_down(state_row, 1.0, 2.0), "a pointerdown holds its button");
  check(input::is_input_pointer_button_down(state_row, 1.0, 0.0) == false,
        "and only ITS button, since the held buttons are a bitmask");

  flight::signals::emit_signal<std::function<void(PointerRow)>>(manager->on_pointer_up, PointerRow(up));
  check(input::is_input_pointer_button_down(state_row, 1.0, 2.0) == false, "a pointerup releases it");

  flight::signals::emit_signal<std::function<void(PointerRow)>>(manager->on_pointer_down, PointerRow(down));
  flight::signals::emit_signal<std::function<void(PointerRow)>>(manager->on_pointer_cancel, PointerRow(down));
  check(input::is_input_pointer_button_down(state_row, 1.0, 2.0) == false,
        "and a pointercancel clears the pointer's buttons wholesale");

}

void test_connect_tracks_gamepad_buttons_and_axes() {
  const auto state = input::create_input_state();
  const auto manager = input::create_input_manager();
  const auto keep_alive = input::connect_input_state_to_input_manager(state, manager);
  (void)keep_alive;
  const auto state_row = StateRow(state);

  const auto button_down = flight::make_ref<types::InputGamepadButtonData>();
  button_down->gamepad = 1.0;
  button_down->button = 3.0;
  flight::signals::emit_signal<std::function<void(ButtonRow)>>(manager->on_gamepad_button_down,
                                                               ButtonRow(button_down));
  check(input::is_input_gamepad_button_down(state_row, 1.0, 3.0),
        "a gamepad button down holds that gamepad's button");
  check(input::was_input_gamepad_button_pressed(state_row, 1.0, 3.0), "and is a pressed edge this frame");
  check(input::is_input_gamepad_button_down(state_row, 0.0, 3.0) == false,
        "the gamepad index is part of the key, so another gamepad is unaffected");

  const auto axis = flight::make_ref<types::InputGamepadAxisData>();
  axis->gamepad = 1.0;
  axis->axis = 2.0;
  axis->value = 0.5;
  flight::signals::emit_signal<std::function<void(AxisRow)>>(manager->on_gamepad_axis_move, AxisRow(axis));
  check(close(input::get_input_gamepad_axis(state_row, 1.0, 2.0), 0.5), "an axis move records its value");
  check(input::get_input_gamepad_axis(state_row, 1.0, 9.0) == 0.0,
        "and an axis never moved reads 0 rather than throwing");

  const auto button_up = flight::make_ref<types::InputGamepadButtonData>();
  button_up->gamepad = 1.0;
  button_up->button = 3.0;
  flight::signals::emit_signal<std::function<void(ButtonRow)>>(manager->on_gamepad_button_up,
                                                              ButtonRow(button_up));
  check(input::was_input_gamepad_button_released(state_row, 1.0, 3.0), "a gamepad button up is a released edge");

  // The frame boundary clears the GAMEPAD edges too. The mutation battery found this unasserted: dropping the
  // gamepad clear from endInputStateFrame went unnoticed, because only the KEYBOARD edges were re-checked
  // after a frame.
  input::end_input_state_frame(state);
  check(input::was_input_gamepad_button_released(state_row, 1.0, 3.0) == false,
        "which endInputStateFrame clears like the keyboard edge");
  check(input::is_input_gamepad_button_down(state_row, 1.0, 3.0) == false,
        "and the held set is untouched by the boundary, since that button really is up");

}

void test_the_gamepad_name_maps() {
  check(input::get_gamepad_button_name(flight::String("standard"), 0.0).has_value(),
        "a standard mapping names button 0");
  check(input::get_gamepad_button_name(flight::String("standard"), 99.0).has_value() == false,
        "and an out-of-range index names nothing rather than inventing one");
  check(input::get_gamepad_button_name(flight::String("xbox"), 0.0).has_value() == false,
        "while a non-standard mapping names nothing at all");

  check(input::get_gamepad_axis_name(flight::String("standard"), 0.0).has_value(), "an axis index is named");
  check(input::get_gamepad_axis_name(flight::String("xbox"), 0.0).has_value() == false,
        "and a non-standard mapping names no axis either");
}

// --- The six ingress attachment families ----------------------------------------------------------------------
//
// The host seam is a `HostInputIngressCapability` row -- six attach slots, one per family -- so this is a recording
// host of the same shape every other package's fixture is: each slot records the source and the sink it was handed
// and answers with a release function that counts its own invocation. Nothing is faked about the ingress itself;
// what is under test is which slot a family reaches, what it is given, and what a detach releases.

using IngressOptionsRow = std::optional<
    flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<types::AttachInputOptions>>>>>;
using IngressSlot = std::function<std::function<void()>(
    types::InputIngressSource, flight::Ref<types::InputIngressSink>, IngressOptionsRow)>;
using IngressHostSchema =
    flight::RowReadonly<flight::RowOf<flight::Ref<types::HostInputIngressCapability>>>;

struct IngressHost {
  int calls[6] = {0, 0, 0, 0, 0, 0};
  int releases[6] = {0, 0, 0, 0, 0, 0};
  flight::ErasedRef seen_source[6];
  flight::Ref<types::InputIngressSink> seen_sink[6];

  flight::Ref<types::HostInputIngressCapability> capability =
      flight::make_ref<types::HostInputIngressCapability>();

  IngressSlot slot_for(int index) {
    return [this, index](types::InputIngressSource source, flight::Ref<types::InputIngressSink> sink,
                         IngressOptionsRow) {
      ++calls[index];
      seen_source[index] = source;
      seen_sink[index] = sink;
      return std::function<void()>([this, index]() { ++releases[index]; });
    };
  }

  IngressHost() {
    capability->attach_gamepad = slot_for(0);
    capability->attach_keyboard = slot_for(1);
    capability->attach_pointer = slot_for(2);
    capability->attach_relative_pointer = slot_for(3);
    capability->attach_text = slot_for(4);
    capability->attach_wheel = slot_for(5);
  }

  flight::StructuralRef<IngressHostSchema> row() {
    return flight::make_structural_ref<IngressHostSchema>(
        flight::row_field<flight::RowKey<"attachGamepad">>(capability->attach_gamepad),
        flight::row_field<flight::RowKey<"attachKeyboard">>(capability->attach_keyboard),
        flight::row_field<flight::RowKey<"attachPointer">>(capability->attach_pointer),
        flight::row_field<flight::RowKey<"attachRelativePointer">>(capability->attach_relative_pointer),
        flight::row_field<flight::RowKey<"attachText">>(capability->attach_text),
        flight::row_field<flight::RowKey<"attachWheel">>(capability->attach_wheel));
  }
};

types::InputIngressSource origin() {
  return flight::ErasedRef(flight::make_ref<types::InputIngressSink>());
}

void test_each_family_attaches_through_its_own_host_entry() {
  IngressHost host;
  const auto manager = input::create_input_manager();
  const auto source = origin();

  input::attach_gamepad_input(host.row(), manager, source);
  input::attach_keyboard_input(host.row(), manager, source);
  input::attach_pointer_input(host.row(), manager, source);
  input::attach_relative_pointer_input(host.row(), manager, source);
  input::attach_text_input(host.row(), manager, source);
  input::attach_wheel_input(host.row(), manager, source);

  for (int i = 0; i < 6; ++i) {
    check(host.calls[i] == 1, "each family reaches exactly its own host slot");
  }
  for (int i = 0; i < 6; ++i) {
    check(host.seen_source[i] == source, "and every one is handed the EXACT source it was given");
    check(host.seen_sink[i] != nullptr, "with a live sink rather than nothing");
  }
  // One manager, one sink: the families share the manager's single ingress sink rather than each minting one.
  for (int i = 1; i < 6; ++i) {
    check(host.seen_sink[i].get() == host.seen_sink[0].get(),
          "and all six share the manager's one sink, so a family cannot strand its own");
  }
}

void test_detach_releases_only_its_own_family() {
  IngressHost host;
  const auto manager = input::create_input_manager();
  const auto source = origin();

  input::attach_gamepad_input(host.row(), manager, source);
  input::attach_keyboard_input(host.row(), manager, source);

  input::detach_keyboard_input(manager, source);
  check(host.releases[1] == 1, "detaching a family runs the release its own origin answered with");
  check(host.releases[0] == 0, "and leaves every OTHER family attached");

  input::detach_keyboard_input(manager, source);
  check(host.releases[1] == 1, "a second detach runs nothing, because the binding is gone");

  input::detach_gamepad_input(manager, source);
  check(host.releases[0] == 1, "and the other family releases when its own detach is called");
}

void test_reattaching_the_same_source_releases_the_old_origin_once() {
  IngressHost first;
  IngressHost second;
  const auto manager = input::create_input_manager();
  const auto source = origin();

  input::attach_gamepad_input(first.row(), manager, source);
  check(first.releases[0] == 0, "the first attachment is live");

  // The same source and the same family, now through a DIFFERENT host: the binding replaces, and the release it
  // holds -- the one the ORIGIN host answered with -- runs exactly once.
  input::attach_gamepad_input(second.row(), manager, source);
  check(first.releases[0] == 1, "reattaching releases the prior origin exactly once");
  check(second.releases[0] == 0, "while the replacement is live");

  input::detach_gamepad_input(manager, source);
  check(second.releases[0] == 1, "and detaching runs the REPLACEMENT's release, not the first host's again");
  check(first.releases[0] == 1, "which never runs a second time");
}

void test_the_sink_delivers_only_while_the_manager_is_enabled() {
  IngressHost host;
  const auto manager = input::create_input_manager();
  input::attach_keyboard_input(host.row(), manager, origin());

  int keys = 0;
  flight::signals::connect_signal<std::function<void(KeyRow)>>(
      manager->on_key_down, [&keys](KeyRow) { ++keys; });

  const auto data = flight::make_ref<types::InputKeyboardData>();
  data->key_code = 65.0;
  host.seen_sink[1]->key_down(KeyRow(data));
  check(keys == 1, "the sink the host was handed publishes into the manager's signal");

  manager->enabled = false;
  host.seen_sink[1]->key_down(KeyRow(data));
  check(keys == 1, "and publishes nothing while the manager is disabled");

  manager->enabled = true;
  host.seen_sink[1]->key_down(KeyRow(data));
  check(keys == 2, "resuming as soon as it is enabled again");
}

int main() {
  test_the_state_and_manager_constructors_start_where_the_source_says();
  test_connect_tracks_the_keyboard_transitions();
  test_connect_tracks_pointer_buttons_and_cancel();
  test_connect_tracks_gamepad_buttons_and_axes();
  test_the_gamepad_name_maps();

  test_each_family_attaches_through_its_own_host_entry();
  test_detach_releases_only_its_own_family();
  test_reattaching_the_same_source_releases_the_old_origin_once();
  test_the_sink_delivers_only_while_the_manager_is_enabled();

  test_axis_dead_zone();
  test_stick_dead_zone_is_radial();
  test_stick_dead_zone_is_alias_safe();
  if (failures == 0) std::cout << "generated input manager: all checks passed\n";
  return failures == 0 ? 0 : 1;
}
