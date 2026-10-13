// Native parity test for @flighthq/input's ATTESTATION AND REMAINING INGRESS -- the binding registry and the
// signal-only half of the manager, complementary to the three tests that precede it.
//
// tests/generated_input_test.cpp pins the key-repeat timer on its own; generated_input_manager_test.cpp pins the
// dead zones; generated_input_state_test.cpp pins the held-state machine the manager maintains off its own signals.
// This file takes what those three leave:
//
//   * The six attach*/detach* families and their release bookkeeping -- inputManager.test.ts's "explicit input
//     ingress parameter" block. The module's contract is not that a host delivers input, but that it can hand a
//     source to exactly the capability entry point for its kind, that the release it gets back is invoked exactly
//     once however many times the caller detaches, and that the binding is selected by source AND kind rather than
//     by whichever was attached last.
//   * The ingress kinds the state machine does not consume -- relative pointer move, wheel and text. Their whole
//     contract is forwarding: the payload reaches the right subscribers untouched and the held-state snapshot is
//     not disturbed, because a move is not a button transition and typing is not a key press.
//   * The repeat timer wired to the manager, which is the only place its design is visible: it emits keydowns, and
//     the state machine's auto-repeat guard is what stops those keydowns re-arming the press edge every tick.
#include <flight/input/input_manager.hpp>

#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

namespace input = flight::input;
namespace types = flight::types;
namespace signals = flight::signals;

using Capability = types::HostInputIngressCapability;
using CapabilityRow = flight::RowReadonly<flight::RowOf<flight::Ref<Capability>>>;
using AttachResult = std::function<void()>;
using AttachOptions = std::optional<flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<types::AttachInputOptions>>>>>;
using AttachFn = std::function<AttachResult(types::InputIngressSource, flight::Ref<types::InputIngressSink>, AttachOptions)>;

using PointerRow = flight::RowReadonly<flight::RowOf<flight::Ref<types::InputPointerData>>>;
using TextRow = flight::RowReadonly<flight::RowOf<flight::Ref<types::InputTextData>>>;
using KbRow = flight::RowReadonly<flight::RowOf<flight::Ref<types::InputKeyboardData>>>;
using RepeatOptionsRow = flight::RowReadonly<flight::RowOf<flight::Ref<types::InputKeyRepeatOptions>>>;

using PointerFn = std::function<void(flight::StructuralRef<PointerRow>)>;
using TextFn = std::function<void(flight::StructuralRef<TextRow>)>;

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

// The six kinds, in the order inputManager.test.ts's case attaches them.
enum Kind { kGamepad, kKeyboard, kPointer, kRelativePointer, kText, kWheel, kKindCount };

const char* kind_name(int kind) {
  switch (kind) {
    case kGamepad: return "gamepad";
    case kKeyboard: return "keyboard";
    case kPointer: return "pointer";
    case kRelativePointer: return "relativePointer";
    case kText: return "text";
    default: return "wheel";
  }
}

flight::StructuralRef<PointerRow> pointer(double pointer_id, double button, double x = 0.0, double y = 0.0,
                                          double delta_x = 0.0, double delta_y = 0.0) {
  return flight::make_structural_ref<PointerRow>(flight::row_field<flight::RowKey<"pointerId">>(pointer_id),
                                                 flight::row_field<flight::RowKey<"button">>(button),
                                                 flight::row_field<flight::RowKey<"x">>(x),
                                                 flight::row_field<flight::RowKey<"y">>(y),
                                                 flight::row_field<flight::RowKey<"deltaX">>(delta_x),
                                                 flight::row_field<flight::RowKey<"deltaY">>(delta_y));
}

flight::StructuralRef<TextRow> text_row(const char* text) {
  return flight::make_structural_ref<TextRow>(flight::row_field<flight::RowKey<"text">>(flight::String(text)));
}

flight::StructuralRef<KbRow> keyboard(double key_code) {
  return flight::make_structural_ref<KbRow>(flight::row_field<flight::RowKey<"keyCode">>(key_code));
}

flight::StructuralRef<RepeatOptionsRow> repeat_options(double delay, double interval) {
  return flight::make_structural_ref<RepeatOptionsRow>(flight::row_field<flight::RowKey<"delay">>(delay),
                                                       flight::row_field<flight::RowKey<"interval">>(interval));
}

// A host capability that records what it was handed and counts releases per kind. The backend is a value the
// caller owns -- the module holds no reference to the source beyond the binding it registers.
struct Backend {
  std::vector<std::string> attached;                 // "<kind>"
  std::vector<const void*> sources;                  // the exact source identity per attachment
  std::vector<flight::Ref<types::InputIngressSink>> sinks;
  std::vector<bool> options_seen;                    // whether an options row was handed through, per attachment
  std::vector<int> releases = std::vector<int>(kKindCount, 0);
  std::vector<int> attached_count = std::vector<int>(kKindCount, 0);

  AttachFn entry(int kind) {
    return [this, kind](types::InputIngressSource source, flight::Ref<types::InputIngressSink> sink, AttachOptions options) {
      attached.push_back(kind_name(kind));
      sources.push_back(source.identity());
      sinks.push_back(sink);
      options_seen.push_back(options.has_value());
      attached_count[kind] += 1;
      return AttachResult([this, kind]() { releases[kind] += 1; });
    };
  }

  flight::StructuralRef<CapabilityRow> capability() {
    return flight::make_structural_ref<CapabilityRow>(flight::row_field<flight::RowKey<"attachGamepad">>(entry(kGamepad)),
                                                      flight::row_field<flight::RowKey<"attachKeyboard">>(entry(kKeyboard)),
                                                      flight::row_field<flight::RowKey<"attachPointer">>(entry(kPointer)),
                                                      flight::row_field<flight::RowKey<"attachRelativePointer">>(entry(kRelativePointer)),
                                                      flight::row_field<flight::RowKey<"attachText">>(entry(kText)),
                                                      flight::row_field<flight::RowKey<"attachWheel">>(entry(kWheel)));
  }
};

types::InputIngressSource source_of(const std::shared_ptr<int>& object) {
  return types::InputIngressSource(object);
}

// --- The six attach*/detach* families -------------------------------------------------------------------------

void test_each_family_reaches_its_own_entry_point_with_the_manager_sink() {
  Backend backend;
  auto capability = backend.capability();
  auto manager = input::create_input_manager();
  auto expected_sink = input::get_input_ingress_sink(manager);
  std::vector<std::shared_ptr<int>> objects{std::make_shared<int>(0), std::make_shared<int>(1), std::make_shared<int>(2),
                                            std::make_shared<int>(3), std::make_shared<int>(4), std::make_shared<int>(5)};

  input::attach_gamepad_input(capability, manager, source_of(objects[0]));
  input::attach_keyboard_input(capability, manager, source_of(objects[1]));
  input::attach_pointer_input(capability, manager, source_of(objects[2]));
  input::attach_relative_pointer_input(capability, manager, source_of(objects[3]));
  input::attach_text_input(capability, manager, source_of(objects[4]));
  input::attach_wheel_input(capability, manager, source_of(objects[5]));

  check(backend.attached == std::vector<std::string>({"gamepad", "keyboard", "pointer", "relativePointer", "text", "wheel"}),
        "each family calls the capability entry point for its own kind, in the order attached");
  bool exact_sources = backend.sources.size() == objects.size();
  for (std::size_t i = 0; exact_sources && i < objects.size(); i++) {
    exact_sources = backend.sources[i] == static_cast<const void*>(objects[i].get());
  }
  check(exact_sources, "and hands each its own source identity, not a coalesced one");
  bool same_sink = true;
  for (const auto& sink : backend.sinks) same_sink = same_sink && sink.get() == expected_sink.get();
  check(same_sink, "while every family receives the one ingress sink the manager owns");

  input::detach_gamepad_input(manager, source_of(objects[0]));
  input::detach_keyboard_input(manager, source_of(objects[1]));
  input::detach_pointer_input(manager, source_of(objects[2]));
  input::detach_relative_pointer_input(manager, source_of(objects[3]));
  input::detach_text_input(manager, source_of(objects[4]));
  input::detach_wheel_input(manager, source_of(objects[5]));
  bool each_released_once = true;
  for (int kind = 0; kind < kKindCount; kind++) each_released_once = each_released_once && backend.releases[kind] == 1;
  check(each_released_once, "and each detach invokes its own release exactly once");
}

void test_detach_is_idempotent() {
  Backend backend;
  auto capability = backend.capability();
  auto manager = input::create_input_manager();
  auto object = std::make_shared<int>(0);
  auto source = source_of(object);

  input::attach_keyboard_input(capability, manager, source);
  input::detach_keyboard_input(manager, source);
  input::detach_keyboard_input(manager, source);
  input::detach_keyboard_input(manager, source);
  check(backend.releases[kKeyboard] == 1, "detaching a kind that is no longer bound releases nothing further");

  input::detach_pointer_input(manager, source);
  input::detach_text_input(manager, source);
  check(backend.releases[kKeyboard] == 1 && backend.releases[kPointer] == 0,
        "and detaching a kind that was never bound on this source is silent");
}

void test_detach_selects_by_kind_within_one_source() {
  Backend backend;
  auto capability = backend.capability();
  auto manager = input::create_input_manager();
  auto object = std::make_shared<int>(0);
  auto source = source_of(object);

  input::attach_keyboard_input(capability, manager, source);
  input::attach_pointer_input(capability, manager, source);
  input::attach_wheel_input(capability, manager, source);

  input::detach_keyboard_input(manager, source);
  check(backend.releases[kKeyboard] == 1, "detaching one kind releases that kind");
  check(backend.releases[kPointer] == 0 && backend.releases[kWheel] == 0,
        "and leaves the other kinds bound to the same source");

  input::detach_wheel_input(manager, source);
  check(backend.releases[kWheel] == 1 && backend.releases[kPointer] == 0, "each kind detaches independently");

  input::detach_pointer_input(manager, source);
  check(backend.releases[kPointer] == 1, "and the last one still releases its own origin");
}

void test_detach_selects_by_source() {
  Backend backend;
  auto capability = backend.capability();
  auto manager = input::create_input_manager();
  auto first_object = std::make_shared<int>(0);
  auto second_object = std::make_shared<int>(1);
  auto first = source_of(first_object);
  auto second = source_of(second_object);

  input::attach_keyboard_input(capability, manager, first);
  input::attach_keyboard_input(capability, manager, second);
  check(backend.attached_count[kKeyboard] == 2, "the same kind binds to two sources independently");

  input::detach_keyboard_input(manager, first);
  check(backend.releases[kKeyboard] == 1, "detaching one source releases exactly one origin");

  input::detach_keyboard_input(manager, second);
  check(backend.releases[kKeyboard] == 2, "and the other source keeps its binding until it is detached itself");
}

void test_reattach_releases_the_previous_origin_once() {
  Backend first_backend;
  Backend second_backend;
  auto capability = first_backend.capability();
  auto other_capability = second_backend.capability();
  auto manager = input::create_input_manager();
  auto object = std::make_shared<int>(0);
  auto source = source_of(object);

  input::attach_pointer_input(capability, manager, source);
  check(first_backend.releases[kPointer] == 0, "the first attachment is live until it is replaced");

  input::attach_pointer_input(other_capability, manager, source);
  check(first_backend.releases[kPointer] == 1,
        "re-attaching the same source releases the origin it had, so the old host stops delivering");
  check(second_backend.attached_count[kPointer] == 1, "and the new origin is bound");

  input::detach_pointer_input(manager, source);
  input::detach_pointer_input(manager, source);
  check(first_backend.releases[kPointer] == 1 && second_backend.releases[kPointer] == 1,
        "so a later detach reaches the new origin once and never the old one again");
}

void test_detach_releases_the_registry_hold_on_the_source() {
  Backend backend;
  auto capability = backend.capability();
  auto manager = input::create_input_manager();
  auto object = std::make_shared<int>(0);
  {
    auto source = source_of(object);
    const long baseline = object.use_count();
    input::attach_text_input(capability, manager, source);
    input::attach_wheel_input(capability, manager, source);
    check(object.use_count() == baseline + 1,
          "the registry holds one reference per source, however many kinds are bound to it");

    input::detach_text_input(manager, source);
    check(object.use_count() == baseline + 1, "detaching one of two kinds keeps the binding for the other");

    input::detach_wheel_input(manager, source);
    check(object.use_count() == baseline, "and detaching the last kind drops the registry's hold entirely");
  }
}

void test_detach_does_not_disable_the_manager() {
  Backend backend;
  auto capability = backend.capability();
  auto manager = input::create_input_manager();
  auto state = input::create_input_state();
  input::connect_input_state_to_input_manager(state, manager);
  auto object = std::make_shared<int>(0);
  auto source = source_of(object);

  input::attach_keyboard_input(capability, manager, source);
  input::detach_keyboard_input(manager, source);

  // Detach unregisters the host's origin; it does not turn the manager off. A sink call that reaches the manager
  // still normalizes, which is what distinguishes detach from disable.
  backend.sinks.back()->key_down(keyboard(65.0));
  check(input::is_input_key_down(state, 65.0), "detaching a source does not disable the manager");
  check(manager->enabled, "and the manager is still enabled");
}

// --- Ingress the state machine does not consume ---------------------------------------------------------------

// Every field of InputPointerData that the generated row-member table can carry, so the payload assertion is
// exhaustive rather than a sample: button, buttons, x, y, deltaX, deltaY, isPrimary, pointerId, pressure, width,
// height and the four modifier flags. `tiltX`, `tiltY`, `twist`, `pointerType` and `wheelMode` are declared on the
// type but no row key reaches them, which is recorded in docs/parity/plat.md.
flight::StructuralRef<PointerRow> pointer_full() {
  return flight::make_structural_ref<PointerRow>(
      flight::row_field<flight::RowKey<"pointerId">>(3.0), flight::row_field<flight::RowKey<"button">>(2.0),
      flight::row_field<flight::RowKey<"buttons">>(5.0), flight::row_field<flight::RowKey<"x">>(10.0),
      flight::row_field<flight::RowKey<"y">>(20.0), flight::row_field<flight::RowKey<"deltaX">>(4.5),
      flight::row_field<flight::RowKey<"deltaY">>(-1.25), flight::row_field<flight::RowKey<"isPrimary">>(true),
      flight::row_field<flight::RowKey<"pressure">>(0.5), flight::row_field<flight::RowKey<"width">>(2.0),
      flight::row_field<flight::RowKey<"height">>(3.0), flight::row_field<flight::RowKey<"altKey">>(true),
      flight::row_field<flight::RowKey<"ctrlKey">>(true), flight::row_field<flight::RowKey<"metaKey">>(true),
      flight::row_field<flight::RowKey<"shiftKey">>(true));
}

void test_relative_pointer_move_is_forwarded_and_has_no_state_effect() {
  auto manager = input::create_input_manager();
  auto state = input::create_input_state();
  input::connect_input_state_to_input_manager(state, manager);
  auto sink = input::get_input_ingress_sink(manager);

  bool seen = false;
  PointerFn observe = [&seen](flight::StructuralRef<PointerRow> data) {
    seen = true;
    check_close(flight::row_get<flight::RowKey<"deltaX">>(data), 4.5, "the relative x delta is forwarded");
    check_close(flight::row_get<flight::RowKey<"deltaY">>(data), -1.25, "the relative y delta is forwarded");
    check_close(flight::row_get<flight::RowKey<"pointerId">>(data), 3.0, "with the pointer it came from");
    check_close(flight::row_get<flight::RowKey<"x">>(data), 10.0, "and its position");
    check_close(flight::row_get<flight::RowKey<"buttons">>(data), 5.0, "and the button mask it carried");
    check(flight::row_get<flight::RowKey<"isPrimary">>(data), "and whether it is the primary pointer");
    check_close(flight::row_get<flight::RowKey<"pressure">>(data), 0.5, "and its pressure");
    check_close(flight::row_get<flight::RowKey<"width">>(data), 2.0, "and its contact width");
    check_close(flight::row_get<flight::RowKey<"height">>(data), 3.0, "and its contact height");
    check(flight::row_get<flight::RowKey<"altKey">>(data) && flight::row_get<flight::RowKey<"ctrlKey">>(data) &&
              flight::row_get<flight::RowKey<"metaKey">>(data) && flight::row_get<flight::RowKey<"shiftKey">>(data),
          "and all four modifier flags");
  };
  signals::connect_signal<PointerFn>(manager->on_pointer_move_relative, observe, std::nullopt);

  sink->pointer_down(pointer(3.0, 0.0));
  sink->pointer_move_relative(pointer_full());
  check(seen, "a relative move reaches onPointerMoveRelative");

  check(input::is_input_pointer_button_down(state, 3.0, 0.0),
        "and a move is not a button transition, so the held button is untouched");
  sink->pointer_up(pointer(3.0, 0.0));
}

void test_attach_options_are_forwarded_including_their_absence() {
  Backend backend;
  auto capability = backend.capability();
  auto manager = input::create_input_manager();
  auto object = std::make_shared<int>(0);
  auto source = source_of(object);

  input::attach_keyboard_input(capability, manager, source);
  check(!backend.options_seen.back(), "an attach with no options hands the capability none");

  auto options = flight::make_structural_ref<flight::RowReadonly<flight::RowOf<flight::Ref<types::AttachInputOptions>>>>();
  input::attach_keyboard_input(capability, manager, source, options);
  check(backend.options_seen.back(), "and an attach that has them hands them through rather than dropping them");
  input::detach_keyboard_input(manager, source);
}

void test_wheel_is_forwarded_and_has_no_state_effect() {
  auto manager = input::create_input_manager();
  auto state = input::create_input_state();
  input::connect_input_state_to_input_manager(state, manager);
  auto sink = input::get_input_ingress_sink(manager);

  bool seen = false;
  PointerFn observe = [&seen](flight::StructuralRef<PointerRow> data) {
    seen = true;
    check_close(flight::row_get<flight::RowKey<"deltaY">>(data), -120.0, "the wheel delta is forwarded");
    check_close(flight::row_get<flight::RowKey<"x">>(data), 10.0, "with the position it happened at");
  };
  signals::connect_signal<PointerFn>(manager->on_wheel, observe, std::nullopt);

  sink->wheel(pointer(0.0, 0.0, 10.0, 20.0, 0.0, -120.0));
  check(seen, "a wheel event reaches onWheel");
  check(!input::is_input_pointer_button_down(state, 0.0, 0.0), "and holds no buttons down");
}

void test_text_ingress_is_forwarded_and_has_no_state_effect() {
  auto manager = input::create_input_manager();
  auto state = input::create_input_state();
  input::connect_input_state_to_input_manager(state, manager);
  auto sink = input::get_input_ingress_sink(manager);

  std::vector<std::string> seen;
  TextFn observe = [&seen](flight::StructuralRef<TextRow> data) {
    seen.push_back(std::string(flight::row_get<flight::RowKey<"text">>(data).to_utf8()));
  };
  signals::connect_signal<TextFn>(manager->on_text_input, observe, std::nullopt);
  signals::connect_signal<TextFn>(manager->on_text_edit, observe, std::nullopt);

  sink->text_input(text_row("a"));
  sink->text_edit(text_row("b"));
  sink->text_input(text_row("c"));
  check(seen == std::vector<std::string>({"a", "b", "c"}),
        "text input and edit reach their signals, each in the order it was delivered");
  check(!input::is_input_key_down(state, 65.0), "and text is not a key press, so no held state appears");
}

// --- The repeat timer wired to the manager --------------------------------------------------------------------

double ticked(std::size_t expected_at_least) {
  std::size_t total = 0;
  while (true) {
    const std::size_t pumped = flight::host::pump_timers();
    total += pumped;
    if (pumped == 0 || total >= expected_at_least) break;
  }
  return static_cast<double>(total);
}

void test_repeat_ticks_do_not_re_arm_the_press_edge() {
  auto manager = input::create_input_manager();
  auto state = input::create_input_state();
  input::connect_input_state_to_input_manager(state, manager);
  auto sink = input::get_input_ingress_sink(manager);

  auto timer = input::create_input_key_repeat_timer(repeat_options(0.0, 0.0));
  int ticks = 0;
  timer->start([&sink, &ticks]() {
    ++ticks;
    sink->key_down(keyboard(65.0));
  });

  check(ticks == 1, "start fires the callback immediately, so the press is not delayed to the first repeat");
  check(input::is_input_key_down(state, 65.0) && input::was_input_key_pressed(state, 65.0),
        "and the key is held with its press edge armed");

  input::end_input_state_frame(state);
  ticked(3);
  check(ticks >= 3, "the delay and interval keep emitting keydowns");
  check(input::is_input_key_down(state, 65.0), "the key stays held across the repeats");
  check(!input::was_input_key_pressed(state, 65.0),
        "and no repeat re-arms the press edge, which is the auto-repeat guard the timer relies on");

  timer->stop();
  const int after_stop = ticks;
  ticked(1);
  check(ticks == after_stop, "stop() cancels both the pending delay and the running interval");
}

void test_restarting_the_timer_cancels_the_previous_cycle() {
  auto manager = input::create_input_manager();
  auto sink = input::get_input_ingress_sink(manager);
  auto timer = input::create_input_key_repeat_timer(repeat_options(0.0, 0.0));

  int first = 0;
  int second = 0;
  timer->start([&sink, &first]() { ++first; sink->key_down(keyboard(66.0)); });
  ticked(2);
  const int first_after_first_cycle = first;

  timer->start([&sink, &second]() { ++second; sink->key_down(keyboard(67.0)); });
  ticked(2);
  check(second >= 2, "the restarted cycle emits");
  check(first == first_after_first_cycle,
        "and the cycle it replaced emits nothing further, so one key cannot drive two repeat streams");
  timer->stop();
}

void test_repeat_into_a_disabled_manager_delivers_nothing() {
  auto manager = input::create_input_manager();
  auto state = input::create_input_state();
  input::connect_input_state_to_input_manager(state, manager);
  auto sink = input::get_input_ingress_sink(manager);

  manager->enabled = false;
  auto timer = input::create_input_key_repeat_timer(repeat_options(0.0, 0.0));
  int ticks = 0;
  timer->start([&sink, &ticks]() { ++ticks; sink->key_down(keyboard(65.0)); });
  ticked(2);
  check(ticks >= 2, "the timer itself keeps ticking while the manager is disabled");
  check(!input::is_input_key_down(state, 65.0), "but none of those ticks reach the state");

  manager->enabled = true;
  ticked(2);
  check(input::is_input_key_down(state, 65.0), "and re-enabling lets the next tick through");
  timer->stop();
}

} // namespace

int main() {
  test_each_family_reaches_its_own_entry_point_with_the_manager_sink();
  test_detach_is_idempotent();
  test_detach_selects_by_kind_within_one_source();
  test_detach_selects_by_source();
  test_reattach_releases_the_previous_origin_once();
  test_detach_releases_the_registry_hold_on_the_source();
  test_detach_does_not_disable_the_manager();

  test_relative_pointer_move_is_forwarded_and_has_no_state_effect();
  test_attach_options_are_forwarded_including_their_absence();
  test_wheel_is_forwarded_and_has_no_state_effect();
  test_text_ingress_is_forwarded_and_has_no_state_effect();

  test_repeat_ticks_do_not_re_arm_the_press_edge();
  test_restarting_the_timer_cancels_the_previous_cycle();
  test_repeat_into_a_disabled_manager_delivers_nothing();

  if (failures == 0) {
    std::cout << "input binding and repeat parity: all checks passed\n";
    return 0;
  }
  std::cerr << failures << " check(s) failed\n";
  return 1;
}
