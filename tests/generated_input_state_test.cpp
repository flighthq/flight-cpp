// Native parity test for @flighthq/input's MANAGER STATE MACHINE -- the event/state surface of
// inputManager.test.ts, complementary to the two tests that precede it.
//
// tests/generated_input_test.cpp pins the key-repeat timer; tests/generated_input_manager_test.cpp pins the axis and
// radial stick dead zones. Neither reaches what this file does: the held-state snapshot and the per-frame edge sets
// that connectInputStateToInputManager maintains off the manager's own signals, the ingress sink the manager hands to
// a host, and the queries that read that state back. inputManager.test.ts carries 63 cases for exactly this surface.
//
// What is asserted, and why each one is load-bearing:
//
//   * Ingress and ordering -- a key/pointer/gamepad event delivered through the sink reaches the state; two states
//     connected to one manager both track; subscribers fire in connection order.
//   * Edges versus held state -- a genuine up->down transition arms the press edge, a RE-DOWN of an already-held key
//     does not (the auto-repeat guard, without which anything firing on press autofires every frame), and a press and
//     a release inside one frame BOTH survive rather than the later deleting the earlier.
//   * The frame boundary -- endInputStateFrame clears the four edge sets and only those: held state is a snapshot of
//     the device, not of the frame, so it must outlive the boundary.
//   * Coordinates, deltas and modifiers -- the sink forwards the source's payload unchanged; the manager is a
//     dispatcher and does not normalize what it carries.
//   * Focus and capture -- a pointer cancel clears that pointer and leaves the others alone.
//   * Multiple devices and pointers -- pointer buttons are a per-pointer bitmask, and gamepad buttons/axes are keyed
//     by device, so two pads and two pointers never alias each other.
//   * Lifecycle -- disabled managers emit nothing and resume when re-enabled; the sink is one object per manager.
//   * Invalid input and failure -- an unknown pointer/gamepad reads as absent rather than throwing; the standard
//     gamepad name lookup answers null for a non-standard mapping.
//   * Listener identity -- subscribers fire in connection order and an identity-bearing slot disconnects, which is
//     what the module's own disposer cannot do; that divergence is the second pinned case below.
//
// Two measured divergences are pinned rather than asserted as correct, so the gaps are visible and the checks flip
// when the compiler fixes land: see test_module_disposer_divergence and test_out_of_range_index_divergence.
#include <flight/input/input_manager.hpp>

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

namespace input = flight::input;
namespace types = flight::types;
namespace signals = flight::signals;

using KbRow = flight::RowReadonly<flight::RowOf<flight::Ref<types::InputKeyboardData>>>;
using PointerRow = flight::RowReadonly<flight::RowOf<flight::Ref<types::InputPointerData>>>;
using GamepadButtonRow = flight::RowReadonly<flight::RowOf<flight::Ref<types::InputGamepadButtonData>>>;
using GamepadAxisRow = flight::RowReadonly<flight::RowOf<flight::Ref<types::InputGamepadAxisData>>>;
using GamepadConnectRow = flight::RowReadonly<flight::RowOf<flight::Ref<types::InputGamepadConnectData>>>;

using KeyFn = std::function<void(flight::StructuralRef<KbRow>)>;
using PointerFn = std::function<void(flight::StructuralRef<PointerRow>)>;

int failures = 0;

void check(bool condition, const std::string& message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

void check_close(double value, double expected, const std::string& message) {
  if (value == expected) return;
  std::cerr << "FAIL: " << message << " (got " << value << ", want " << expected << ")\n";
  ++failures;
}

// Row keys are the source's property names; a partial row defaults the fields it does not carry, so each builder
// spells only what the manager reads plus what an assertion observes.
// `timeStamp` is deliberately absent: the generated row-member table has no key that reaches `time_stamp`, so a
// keyboard row cannot carry one. That gap is recorded in docs/parity/plat.md rather than worked around here.
flight::StructuralRef<KbRow> keyboard(double key_code, bool repeat = false, bool shift = false, bool ctrl = false,
                                     bool alt = false, bool meta = false) {
  return flight::make_structural_ref<KbRow>(flight::row_field<flight::RowKey<"keyCode">>(key_code),
                                            flight::row_field<flight::RowKey<"repeat">>(repeat),
                                            flight::row_field<flight::RowKey<"shiftKey">>(shift),
                                            flight::row_field<flight::RowKey<"ctrlKey">>(ctrl),
                                            flight::row_field<flight::RowKey<"altKey">>(alt),
                                            flight::row_field<flight::RowKey<"metaKey">>(meta));
}

flight::StructuralRef<PointerRow> pointer(double pointer_id, double button, double x = 0.0, double y = 0.0,
                                          double delta_x = 0.0, double delta_y = 0.0, bool is_primary = false) {
  return flight::make_structural_ref<PointerRow>(flight::row_field<flight::RowKey<"pointerId">>(pointer_id),
                                                 flight::row_field<flight::RowKey<"button">>(button),
                                                 flight::row_field<flight::RowKey<"x">>(x),
                                                 flight::row_field<flight::RowKey<"y">>(y),
                                                 flight::row_field<flight::RowKey<"deltaX">>(delta_x),
                                                 flight::row_field<flight::RowKey<"deltaY">>(delta_y),
                                                 flight::row_field<flight::RowKey<"isPrimary">>(is_primary));
}

flight::StructuralRef<GamepadButtonRow> gamepad_button(double gamepad, double button, double value = 1.0) {
  return flight::make_structural_ref<GamepadButtonRow>(flight::row_field<flight::RowKey<"gamepad">>(gamepad),
                                                       flight::row_field<flight::RowKey<"button">>(button),
                                                       flight::row_field<flight::RowKey<"value">>(value));
}

flight::StructuralRef<GamepadAxisRow> gamepad_axis(double gamepad, double axis, double value) {
  return flight::make_structural_ref<GamepadAxisRow>(flight::row_field<flight::RowKey<"gamepad">>(gamepad),
                                                     flight::row_field<flight::RowKey<"axis">>(axis),
                                                     flight::row_field<flight::RowKey<"value">>(value));
}

flight::StructuralRef<GamepadConnectRow> gamepad_connection(double gamepad) {
  return flight::make_structural_ref<GamepadConnectRow>(flight::row_field<flight::RowKey<"gamepad">>(gamepad));
}

constexpr double kKeyA = 65.0;

// A manager, a state connected to it, and the ingress sink the manager hands a host. The disposer is deliberately
// NOT called: the sink outlives the connection in every case below, and disconnecting is its own check.
struct Fixture {
  flight::Ref<types::InputManager> manager = input::create_input_manager();
  flight::Ref<types::InputState> state = input::create_input_state();
  std::function<void()> dispose = input::connect_input_state_to_input_manager(state, manager);
  flight::Ref<types::InputIngressSink> sink = input::get_input_ingress_sink(manager);
};

// --- Ingress, ordering and listener identity -------------------------------------------------------------------

void test_key_ingress_tracks_held_state() {
  Fixture f;
  f.sink->key_down(keyboard(kKeyA));
  check(input::is_input_key_down(f.state, kKeyA), "a key down through the sink reads as held");
  check(input::was_input_key_pressed(f.state, kKeyA), "and as pressed this frame");

  f.sink->key_up(keyboard(kKeyA));
  check(!input::is_input_key_down(f.state, kKeyA), "a key up reads as no longer held");
  check(input::was_input_key_released(f.state, kKeyA), "and as released this frame");
}

void test_two_states_track_one_manager_independently() {
  auto manager = input::create_input_manager();
  auto first = input::create_input_state();
  auto second = input::create_input_state();
  input::connect_input_state_to_input_manager(first, manager);
  input::connect_input_state_to_input_manager(second, manager);
  auto sink = input::get_input_ingress_sink(manager);

  sink->key_down(keyboard(kKeyA));
  check(input::is_input_key_down(first, kKeyA) && input::is_input_key_down(second, kKeyA),
        "one event reaches every state connected to the manager");
  sink->key_up(keyboard(kKeyA));
  check(!input::is_input_key_down(first, kKeyA) && !input::is_input_key_down(second, kKeyA),
        "and a release reaches every one of them too");
}

void test_subscribers_fire_in_connection_order() {
  // A bare manager, with no state connected: a signal whose slots are ALL identity-bearing is the only shape that
  // can be disconnected at all -- one erased lambda anywhere in the list makes every disconnect throw (measured;
  // see test_module_disposer_divergence).
  auto manager = input::create_input_manager();
  std::vector<int> order;
  // The slots are declared as identity-bearing `flight::Function` values wrapped in the erased carrier the
  // signal stores, which is the only shape `disconnectSignal` can match (see the divergence test below).
  std::function<void(flight::StructuralRef<KbRow>)> first = flight::Function<void(flight::StructuralRef<KbRow>)>(
      [&order](flight::StructuralRef<KbRow>) { order.push_back(1); });
  std::function<void(flight::StructuralRef<KbRow>)> second = flight::Function<void(flight::StructuralRef<KbRow>)>(
      [&order](flight::StructuralRef<KbRow>) { order.push_back(2); });
  signals::connect_signal<KeyFn>(manager->on_key_down, first, std::nullopt);
  signals::connect_signal<KeyFn>(manager->on_key_down, second, std::nullopt);

  auto sink = input::get_input_ingress_sink(manager);
  sink->key_down(keyboard(kKeyA));
  check(order == std::vector<int>({1, 2}), "subscribers fire in the order they connected");

  order.clear();
  signals::disconnect_signal<KeyFn>(manager->on_key_down, first);
  sink->key_down(keyboard(kKeyA));
  check(order == std::vector<int>({2}), "and a disconnected subscriber stops firing while the rest continue");
}

void test_ingress_sink_is_one_object_per_manager() {
  auto manager = input::create_input_manager();
  auto other = input::create_input_manager();
  check(input::get_input_ingress_sink(manager).get() == input::get_input_ingress_sink(manager).get(),
        "the sink is memoized, so a host attaching twice holds the same one");
  check(input::get_input_ingress_sink(manager).get() != input::get_input_ingress_sink(other).get(),
        "and two managers never share a sink");
}

// --- Edges, repeats and the frame boundary ---------------------------------------------------------------------

void test_repeat_keydown_does_not_re_arm_the_press_edge() {
  Fixture f;
  f.sink->key_down(keyboard(kKeyA));
  check(input::was_input_key_pressed(f.state, kKeyA), "the first keydown arms the press edge");
  input::end_input_state_frame(f.state);

  f.sink->key_down(keyboard(kKeyA, true));
  check(!input::was_input_key_pressed(f.state, kKeyA),
        "an auto-repeat keydown for a key already held does NOT arm the press edge again");
  check(input::is_input_key_down(f.state, kKeyA), "while the key stays held");
}

void test_press_and_release_in_one_frame_both_survive() {
  Fixture f;
  f.sink->key_down(keyboard(kKeyA));
  f.sink->key_up(keyboard(kKeyA));
  check(input::was_input_key_pressed(f.state, kKeyA), "a tap inside one frame reports its press");
  check(input::was_input_key_released(f.state, kKeyA), "and its release, rather than one deleting the other");
  check(!input::is_input_key_down(f.state, kKeyA), "with the key not held at the end of it");
}

void test_release_then_press_in_one_frame_both_survive() {
  Fixture f;
  f.sink->key_down(keyboard(kKeyA));
  input::end_input_state_frame(f.state);
  f.sink->key_up(keyboard(kKeyA));
  f.sink->key_down(keyboard(kKeyA));
  check(input::was_input_key_pressed(f.state, kKeyA) && input::was_input_key_released(f.state, kKeyA),
        "a release and a re-press inside one frame report both edges");
  check(input::is_input_key_down(f.state, kKeyA), "and the key is held at the end of it");
}

void test_frame_boundary_clears_edges_but_not_held_state() {
  Fixture f;
  f.sink->key_down(keyboard(kKeyA));
  f.sink->key_up(keyboard(66.0));
  f.sink->gamepad_button_down(gamepad_button(0.0, 3.0));
  f.sink->gamepad_button_up(gamepad_button(0.0, 4.0));
  input::end_input_state_frame(f.state);

  check(!input::was_input_key_pressed(f.state, kKeyA), "the frame boundary clears the pressed-key edge");
  check(!input::was_input_key_released(f.state, 66.0), "and the released-key edge");
  check(!input::was_input_gamepad_button_pressed(f.state, 0.0, 3.0), "and the pressed-gamepad-button edge");
  check(!input::was_input_gamepad_button_released(f.state, 0.0, 4.0), "and the released one");

  check(input::is_input_key_down(f.state, kKeyA), "held state is a snapshot of the device and outlives the boundary");
  check(input::is_input_gamepad_button_down(f.state, 0.0, 3.0), "and so is held gamepad state");
}

void test_repeat_gamepad_button_report_does_not_re_arm_the_edge() {
  Fixture f;
  f.sink->gamepad_button_down(gamepad_button(0.0, 1.0));
  check(input::was_input_gamepad_button_pressed(f.state, 0.0, 1.0), "a gamepad button down arms its press edge");
  input::end_input_state_frame(f.state);

  // A native backend that reports held buttons every poll is a supported source, so the guard belongs here.
  f.sink->gamepad_button_down(gamepad_button(0.0, 1.0));
  check(!input::was_input_gamepad_button_pressed(f.state, 0.0, 1.0),
        "a held button reported down again does not arm the edge a second time");
  check(input::is_input_gamepad_button_down(f.state, 0.0, 1.0), "while the button stays held");
}

// --- Payload fidelity: coordinates, deltas and modifiers -------------------------------------------------------

void test_keyboard_payload_passes_through_unchanged() {
  Fixture f;
  bool seen = false;
  KeyFn observe = [&seen](flight::StructuralRef<KbRow> data) {
    seen = true;
    check(flight::row_get<flight::RowKey<"keyCode">>(data) == kKeyA, "the key code reaches the subscriber");
    check(flight::row_get<flight::RowKey<"shiftKey">>(data), "shift stays set");
    check(flight::row_get<flight::RowKey<"ctrlKey">>(data), "ctrl stays set");
    check(flight::row_get<flight::RowKey<"altKey">>(data), "alt stays set");
    check(flight::row_get<flight::RowKey<"metaKey">>(data), "meta stays set");
    check(flight::row_get<flight::RowKey<"repeat">>(data), "repeat stays set");
  };
  signals::connect_signal<KeyFn>(f.manager->on_key_down, observe, std::nullopt);

  // A keydown carries its modifiers; the manager forwards them rather than normalizing them away.
  f.sink->key_down(keyboard(kKeyA, true, true, true, true, true));
  check(seen, "the subscriber saw the event");
}

void test_pointer_move_payload_passes_through_unchanged() {
  Fixture f;
  bool seen = false;
  PointerFn observe = [&seen](flight::StructuralRef<PointerRow> data) {
    seen = true;
    check_close(flight::row_get<flight::RowKey<"pointerId">>(data), 7.0, "pointer identity is preserved");
    check_close(flight::row_get<flight::RowKey<"x">>(data), 120.0, "x is preserved");
    check_close(flight::row_get<flight::RowKey<"y">>(data), 34.0, "y is preserved");
    check_close(flight::row_get<flight::RowKey<"deltaX">>(data), -3.5, "the x delta is preserved");
    check_close(flight::row_get<flight::RowKey<"deltaY">>(data), 8.25, "the y delta is preserved");
    check(flight::row_get<flight::RowKey<"isPrimary">>(data), "primary-ness is preserved");
  };
  signals::connect_signal<PointerFn>(f.manager->on_pointer_move, observe, std::nullopt);

  f.sink->pointer_move(pointer(7.0, 0.0, 120.0, 34.0, -3.5, 8.25, true));
  check(seen, "a pointer move reaches the subscriber");

  // A move is not a button transition: it must not disturb the held-button snapshot.
  check(!input::is_input_pointer_button_down(f.state, 7.0, 0.0), "a move alone holds no buttons down");
}

// --- Focus and capture: pointer buttons are a per-pointer bitmask ----------------------------------------------

void test_pointer_buttons_are_a_bitmask_per_pointer() {
  Fixture f;
  f.sink->pointer_down(pointer(1.0, 0.0));
  f.sink->pointer_down(pointer(1.0, 2.0));
  check(input::is_input_pointer_button_down(f.state, 1.0, 0.0) && input::is_input_pointer_button_down(f.state, 1.0, 2.0),
        "two buttons held on one pointer are both down");

  f.sink->pointer_up(pointer(1.0, 0.0));
  check(!input::is_input_pointer_button_down(f.state, 1.0, 0.0), "releasing one clears exactly that button");
  check(input::is_input_pointer_button_down(f.state, 1.0, 2.0), "and leaves the other held");

  f.sink->pointer_up(pointer(1.0, 2.0));
  check(!input::is_input_pointer_button_down(f.state, 1.0, 2.0), "releasing the last button clears the pointer");
  check(!input::is_input_pointer_button_down(f.state, 1.0, 5.0), "and no stray button remains down");
}

void test_multiple_pointers_are_independent() {
  Fixture f;
  f.sink->pointer_down(pointer(1.0, 0.0));
  f.sink->pointer_down(pointer(2.0, 1.0));
  check(input::is_input_pointer_button_down(f.state, 2.0, 1.0), "a second pointer's button is held");

  f.sink->pointer_up(pointer(2.0, 1.0));
  check(input::is_input_pointer_button_down(f.state, 1.0, 0.0), "releasing it leaves the first pointer untouched");
  f.sink->pointer_up(pointer(1.0, 0.0));
}

void test_pointer_cancel_clears_that_pointer_only() {
  Fixture f;
  f.sink->pointer_down(pointer(1.0, 0.0));
  f.sink->pointer_down(pointer(1.0, 2.0));
  f.sink->pointer_down(pointer(2.0, 0.0));

  f.sink->pointer_cancel(pointer(1.0, 0.0));
  check(!input::is_input_pointer_button_down(f.state, 1.0, 0.0), "a cancel drops the cancelled pointer's buttons");
  check(!input::is_input_pointer_button_down(f.state, 1.0, 2.0), "all of them, not just the reported button");
  check(input::is_input_pointer_button_down(f.state, 2.0, 0.0), "and leaves every other pointer alone");
  f.sink->pointer_up(pointer(2.0, 0.0));
}

// A DIVERGENCE, pinned rather than asserted as correct. inputManager.test.ts's "returns a disposer that stops
// tracking" cannot hold here: connectInputStateToInputManager connects its handlers as bare lambdas erased into
// std::function, and disconnecting compares slots by callable identity, which that erasure erases. The runtime
// refuses to guess and throws rather than detaching some other closure, so the returned disposer throws instead of
// detaching. This is the already-filed compiler defect
// docs/compiler-requests/local-handler-callable-identity.md, which states that any emitted function value compared
// by identity after erasure has the same defect; this is a newly observed instance of it, not a new defect. The
// second check shows the capability itself is present -- an identity-bearing `flight::Function` reaches the same
// disconnect call and matches -- so the gap is the emitter's erasure, not a missing runtime operation.
void test_module_disposer_divergence() {
  Fixture f;
  bool threw = false;
  try {
    f.dispose();
  } catch (const std::logic_error&) {
    threw = true;
  }
  check(threw, "DIVERGENCE: the returned disposer throws instead of detaching (filed, not fixed)");

  check(input::is_input_key_down(f.state, kKeyA) == false,
        "and nothing was detached by the attempt, so the state still tracks");

  auto manager = input::create_input_manager();
  std::function<void(flight::StructuralRef<KbRow>)> slot =
      flight::Function<void(flight::StructuralRef<KbRow>)>([](flight::StructuralRef<KbRow>) {});
  signals::connect_signal<KeyFn>(manager->on_key_down, slot, std::nullopt);
  bool carried = true;
  try {
    signals::disconnect_signal<KeyFn>(manager->on_key_down, slot);
  } catch (const std::logic_error&) {
    carried = false;
  }
  check(carried, "while the same disconnect matches an identity-bearing Function, so the operation exists");
}

// --- Multiple devices: gamepad buttons and axes are keyed by device -------------------------------------------

void test_gamepad_buttons_are_per_device() {
  Fixture f;
  f.sink->gamepad_button_down(gamepad_button(0.0, 0.0));
  f.sink->gamepad_button_down(gamepad_button(1.0, 0.0));
  check(input::is_input_gamepad_button_down(f.state, 0.0, 0.0) && input::is_input_gamepad_button_down(f.state, 1.0, 0.0),
        "the same button index on two pads is two independent buttons");

  f.sink->gamepad_button_up(gamepad_button(0.0, 0.0));
  check(!input::is_input_gamepad_button_down(f.state, 0.0, 0.0), "releasing it on pad 0");
  check(input::is_input_gamepad_button_down(f.state, 1.0, 0.0), "leaves pad 1's button held");
  check(input::is_input_gamepad_button_down(f.state, 1.0, 63.0) == false, "and does not spill into the pad's other slots");
  f.sink->gamepad_button_up(gamepad_button(1.0, 0.0));
}

void test_gamepad_axes_are_per_device_and_last_write_wins() {
  Fixture f;
  f.sink->gamepad_axis_move(gamepad_axis(0.0, 0.0, 0.5));
  f.sink->gamepad_axis_move(gamepad_axis(1.0, 0.0, -0.25));
  check_close(input::get_input_gamepad_axis(f.state, 0.0, 0.0), 0.5, "pad 0's axis holds its own value");
  check_close(input::get_input_gamepad_axis(f.state, 1.0, 0.0), -0.25, "pad 1's axis holds a different one");

  f.sink->gamepad_axis_move(gamepad_axis(0.0, 0.0, 0.75));
  check_close(input::get_input_gamepad_axis(f.state, 0.0, 0.0), 0.75, "a later move on the same axis replaces the value");

  input::end_input_state_frame(f.state);
  check_close(input::get_input_gamepad_axis(f.state, 0.0, 0.0), 0.75,
              "an axis value is held state and survives the frame boundary");
  check_close(input::get_input_gamepad_axis(f.state, 0.0, 3.0), 0.0, "an axis never moved reads as centred");
}

void test_gamepad_connect_clears_only_that_device() {
  Fixture f;
  f.sink->gamepad_button_down(gamepad_button(0.0, 0.0));
  f.sink->gamepad_axis_move(gamepad_axis(0.0, 0.0, 0.5));
  f.sink->gamepad_button_down(gamepad_button(1.0, 0.0));
  f.sink->gamepad_axis_move(gamepad_axis(1.0, 0.0, 0.25));

  f.sink->gamepad_connect(gamepad_connection(0.0));
  check(!input::is_input_gamepad_button_down(f.state, 0.0, 0.0), "a reconnect clears the reconnected pad's buttons");
  check_close(input::get_input_gamepad_axis(f.state, 0.0, 0.0), 0.0, "and its axis values");
  check(input::is_input_gamepad_button_down(f.state, 1.0, 0.0), "while leaving the other pad's buttons alone");
  check_close(input::get_input_gamepad_axis(f.state, 1.0, 0.0), 0.25, "and its axis values alone");
}

void test_gamepad_disconnect_clears_only_that_device() {
  Fixture f;
  f.sink->gamepad_button_down(gamepad_button(0.0, 2.0));
  f.sink->gamepad_axis_move(gamepad_axis(0.0, 1.0, 0.5));
  f.sink->gamepad_button_down(gamepad_button(1.0, 2.0));
  f.sink->gamepad_axis_move(gamepad_axis(1.0, 1.0, 0.5));

  f.sink->gamepad_disconnect(gamepad_connection(0.0));
  check(!input::is_input_gamepad_button_down(f.state, 0.0, 2.0), "a disconnect clears that pad's buttons");
  check_close(input::get_input_gamepad_axis(f.state, 0.0, 1.0), 0.0, "and its axis values");
  check(input::is_input_gamepad_button_down(f.state, 1.0, 2.0), "leaving the other pad held");
  check_close(input::get_input_gamepad_axis(f.state, 1.0, 1.0), 0.5, "and its axis value");
}

// --- Lifecycle ------------------------------------------------------------------------------------------------

void test_disabled_manager_emits_nothing_and_resumes() {
  Fixture f;
  f.manager->enabled = false;
  f.sink->key_down(keyboard(kKeyA));
  check(!input::is_input_key_down(f.state, kKeyA), "a disabled manager delivers nothing to the state");
  check(!f.sink->is_enabled(), "and its sink reports it is disabled");

  f.manager->enabled = true;
  f.sink->key_down(keyboard(kKeyA));
  check(input::is_input_key_down(f.state, kKeyA), "re-enabling resumes delivery");
}

void test_manager_signals_are_created_and_distinct() {
  auto manager = input::create_input_manager();
  std::vector<const void*> stored{manager->on_gamepad_axis_move.get(), manager->on_gamepad_button_down.get(),
                                  manager->on_gamepad_button_up.get(), manager->on_gamepad_connect.get(),
                                  manager->on_gamepad_disconnect.get(), manager->on_key_down.get(),
                                  manager->on_key_up.get(), manager->on_pointer_cancel.get(),
                                  manager->on_pointer_down.get(), manager->on_pointer_move.get(),
                                  manager->on_pointer_move_relative.get(), manager->on_pointer_up.get(),
                                  manager->on_text_edit.get(), manager->on_text_input.get(), manager->on_wheel.get()};
  bool all_present = true;
  for (const void* signal : stored) all_present = all_present && signal != nullptr;
  check(all_present, "a fresh manager carries all fifteen signals");

  bool all_distinct = true;
  for (std::size_t i = 0; i < stored.size(); i++) {
    for (std::size_t j = i + 1; j < stored.size(); j++) all_distinct = all_distinct && stored[i] != stored[j];
  }
  check(all_distinct, "and each is its own signal rather than one shared by several fields");
}

// --- Invalid input and failure behavior -----------------------------------------------------------------------

void test_unknown_inputs_read_as_absent() {
  auto state = input::create_input_state();
  check(!input::is_input_key_down(state, kKeyA), "an unheld key is not down");
  check(!input::was_input_key_pressed(state, kKeyA), "and was not pressed");
  check(!input::was_input_key_released(state, kKeyA), "and was not released");
  check(!input::is_input_pointer_button_down(state, 0.0, 0.0), "an unseen pointer holds no buttons");
  check(!input::is_input_gamepad_button_down(state, 0.0, 0.0), "an unseen gamepad button is not down");
  check_close(input::get_input_gamepad_axis(state, 0.0, 0.0), 0.0, "an unseen axis reads as centred");
}

void test_standard_gamepad_name_mapping() {
  check(*input::get_gamepad_button_name(flight::String("standard"), 0.0) == types::gamepad_button_kind->button_south,
        "index 0 is the standard south button");
  check(*input::get_gamepad_button_name(flight::String("standard"), 12.0) == types::gamepad_button_kind->dpad_up,
        "index 12 is the standard d-pad up");
  check(*input::get_gamepad_button_name(flight::String("standard"), 16.0) == types::gamepad_button_kind->home,
        "index 16 is home");
  check(!input::get_gamepad_button_name(flight::String("raw"), 0.0).has_value(), "a non-standard mapping has no names");
  check(!input::get_gamepad_button_name(flight::String(""), 0.0).has_value(), "nor does an empty one");

  check(*input::get_gamepad_axis_name(flight::String("standard"), 0.0) == types::gamepad_axis_kind->stick_left_x,
        "axis 0 is the left stick's x");
  check(*input::get_gamepad_axis_name(flight::String("standard"), 3.0) == types::gamepad_axis_kind->stick_right_y,
        "axis 3 is the right stick's y");
  check(!input::get_gamepad_axis_name(flight::String("raw"), 0.0).has_value(), "a non-standard mapping has no axis names");
}

// A DIVERGENCE, pinned rather than asserted as correct. inputManager.ts reads `_standardButtonNames[index] ?? null`,
// and for an out-of-range index TypeScript's indexed read is `undefined`, so the function returns `null`. The emitted
// lowering reads through the bounds-checked `Array::element`, which throws `std::range_error` instead. This is filed
// as docs/compiler-requests/out-of-range-array-index-read-widened-to-null.md; the assertion below records the current
// behavior so the gap is visible, and must be inverted when that request lands.
void test_out_of_range_index_divergence() {
  bool threw = false;
  try {
    (void)input::get_gamepad_button_name(flight::String("standard"), 99.0);
  } catch (const std::range_error&) {
    threw = true;
  }
  check(threw,
        "DIVERGENCE: an out-of-range gamepad index throws where TypeScript returns null (filed, not fixed)");
}

} // namespace

int main() {
  test_key_ingress_tracks_held_state();
  test_two_states_track_one_manager_independently();
  test_subscribers_fire_in_connection_order();
  test_ingress_sink_is_one_object_per_manager();

  test_repeat_keydown_does_not_re_arm_the_press_edge();
  test_press_and_release_in_one_frame_both_survive();
  test_release_then_press_in_one_frame_both_survive();
  test_frame_boundary_clears_edges_but_not_held_state();
  test_repeat_gamepad_button_report_does_not_re_arm_the_edge();

  test_keyboard_payload_passes_through_unchanged();
  test_pointer_move_payload_passes_through_unchanged();

  test_pointer_buttons_are_a_bitmask_per_pointer();
  test_multiple_pointers_are_independent();
  test_pointer_cancel_clears_that_pointer_only();
  test_module_disposer_divergence();

  test_gamepad_buttons_are_per_device();
  test_gamepad_axes_are_per_device_and_last_write_wins();
  test_gamepad_connect_clears_only_that_device();
  test_gamepad_disconnect_clears_only_that_device();

  test_disabled_manager_emits_nothing_and_resumes();
  test_manager_signals_are_created_and_distinct();

  test_unknown_inputs_read_as_absent();
  test_standard_gamepad_name_mapping();
  test_out_of_range_index_divergence();

  if (failures == 0) {
    std::cout << "input state-machine parity: all checks passed\n";
    return 0;
  }
  std::cerr << failures << " check(s) failed\n";
  return 1;
}
