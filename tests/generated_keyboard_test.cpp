// Native parity test for @flighthq/keyboard, ported from its own keyboard.test.ts (23 cases).
//
// The package is the SOFT (on-screen) keyboard facade: visibility, height and the show/hide/resize signals over a
// swappable host backend. It is not the hardware keyboard -- there are no key codes, no modifier state, no chords and
// no composition here, and the package declares none of them. What it does own, and what this file pins:
//
//   * Delegation. Six setters (show/hide and four style/resize/assist switches) are thin over one host entry point
//     each; the module neither normalizes their arguments nor rewrites their results, so a backend's
//     'operation-failed' reaches the caller unchanged and a rejected backend task stays a rejection.
//   * The info reads. `getSoftKeyboardInfo` writes into the caller's `out` and returns THAT object -- reference
//     identity, not a copy -- while the two convenience reads use the module's own scratch, so a caller's object is
//     never aliased by them.
//   * The attach state machine, which is the only place with real logic. It subscribes, then on each host change
//     compares the new height against the height captured at attach: 0 -> positive fires onShow with the new height,
//     positive -> 0 fires onHide, a change while visible fires onResize, and a change that does not cross the
//     visible boundary or does not move the height fires NOTHING. Attach re-subscribes (replacing any previous
//     subscription) and a failed acquisition registers nothing.
#include <flight/keyboard/keyboard.hpp>
#include <flight/signals/slot.hpp>

#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

namespace keyboard = flight::keyboard;
namespace types = flight::types;
namespace signals = flight::signals;

using Info = types::SoftKeyboardInfo;
using ChangeRow = flight::RowReadonly<flight::RowOf<flight::Ref<types::HostSoftKeyboardChangeCapability>>>;
using InfoRow = flight::RowReadonly<flight::RowOf<flight::Ref<types::HostSoftKeyboardInfoCapability>>>;
using VisibilityRow = flight::RowReadonly<flight::RowOf<flight::Ref<types::HostSoftKeyboardVisibilityCapability>>>;
using AccessoryRow = flight::RowReadonly<flight::RowOf<flight::Ref<types::HostSoftKeyboardAccessoryBarCapability>>>;
using ResizeModeRow = flight::RowReadonly<flight::RowOf<flight::Ref<types::HostSoftKeyboardResizeModeWriteCapability>>>;
using ScrollAssistRow = flight::RowReadonly<flight::RowOf<flight::Ref<types::HostSoftKeyboardScrollAssistCapability>>>;
using StyleRow = flight::RowReadonly<flight::RowOf<flight::Ref<types::HostSoftKeyboardStyleCapability>>>;

int failures = 0;

void check(bool condition, const std::string& message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

void check_equal(const flight::String& value, const flight::String& expected, const std::string& message) {
  if (value == expected) return;
  std::cerr << "FAIL: " << message << " (got " << value.to_utf8() << ", want " << expected.to_utf8() << ")\n";
  ++failures;
}

flight::Ref<Info> info_of(bool visible, double height, double x = 0.0, double y = 0.0, double width = 0.0) {
  return flight::make_ref<Info>(Info{.visible = visible, .height = height, .x = x, .y = y, .width = width});
}

// The host reads through the caller's object, so the fake overwrites every field exactly as the real backend does.
flight::StructuralRef<InfoRow> info_row(const flight::Ref<Info>& data) {
  auto capability = flight::make_ref<types::HostSoftKeyboardInfoCapability>();
  capability->get_info = [data](flight::Ref<Info> out) {
    *out = *data;
    return out;
  };
  return flight::make_structural_ref<InfoRow>(flight::row_field<flight::RowKey<"getInfo">>(capability->get_info));
}

struct ChangeBackend {
  std::function<void()> listener;
  std::function<void()> unsubscribe = []() {};
  int unsubscribe_calls = 0;
  std::string result = "ok";

  flight::StructuralRef<ChangeRow> row() {
    auto capability = flight::make_ref<types::HostSoftKeyboardChangeCapability>();
    capability->subscribe = [this](std::function<void()> slot) {
      listener = slot;
      auto subscription = flight::make_ref<types::SoftKeyboardChangeSubscription>();
      subscription->result = flight::String(result);
      if (result == "ok") {
        subscription->unsubscribe = std::function<void()>([this]() { ++unsubscribe_calls; });
      }
      return flight::Task<flight::Ref<types::SoftKeyboardChangeSubscription>>::resolve(subscription);
    };
    return flight::make_structural_ref<ChangeRow>(flight::row_field<flight::RowKey<"subscribe">>(capability->subscribe));
  }
};

// A pair of change+info backends wired to one mutable info record, which the test moves between states.
struct AttachFixture {
  flight::Ref<Info> data = info_of(false, 0.0);
  ChangeBackend change;
  flight::Ref<types::SoftKeyboard> keyboard = keyboard::create_soft_keyboard();
  std::vector<std::string> events;   // the signal that fired, in order
  std::vector<double> heights;       // its payload, for the two signals that carry one

  AttachFixture() {
    signals::connect_signal<std::function<void(double)>>(
        keyboard->on_show, [this](double height) { events.push_back("show"); heights.push_back(height); }, std::nullopt);
    signals::connect_signal<std::function<void()>>(
        keyboard->on_hide, [this]() { events.push_back("hide"); heights.push_back(0.0); }, std::nullopt);
    signals::connect_signal<std::function<void(double)>>(
        keyboard->on_resize, [this](double height) { events.push_back("resize"); heights.push_back(height); },
        std::nullopt);
  }

  bool fired(const std::vector<std::string>& expected, const std::vector<double>& expected_heights) const {
    return events == expected && heights == expected_heights;
  }

  void move_to(bool visible, double height) {
    data->visible = visible;
    data->height = height;
    if (change.listener) change.listener();
  }

  flight::String attach() { return keyboard::attach_soft_keyboard(change.row(), info_row(data), keyboard).get(); }
};

// --- Attach: acquisition and the transition table -------------------------------------------------------------

void test_attach_reports_ok_and_registers_the_subscription() {
  AttachFixture f;
  check_equal(f.attach(), types::soft_keyboard_attach_ok_kind, "a successful subscribe reports ok");
  check(static_cast<bool>(f.change.listener), "and the host was given a listener to call");

  keyboard::detach_soft_keyboard(f.keyboard);
  check(f.change.unsubscribe_calls == 1, "so a later detach can release the subscription it registered");
}

void test_attach_reports_acquisition_failed_and_registers_nothing() {
  AttachFixture f;
  f.change.result = "acquisition-failed";
  check_equal(f.attach(), types::soft_keyboard_attach_acquisition_failed_kind,
              "a subscribe that refuses reports acquisition-failed");

  // The host returned no unsubscribe, so detach must not try to call one -- upstream returns early on the same
  // condition, and the null unsubscribe is what would otherwise be dereferenced.
  keyboard::detach_soft_keyboard(f.keyboard);
  check(f.change.unsubscribe_calls == 0, "and nothing is registered, so detach releases nothing");
}

void test_height_zero_to_positive_fires_show_with_the_new_height() {
  AttachFixture f;
  f.attach();
  f.move_to(true, 300.0);
  check(f.fired({"show"}, {300.0}), "a height rising off zero is a show, carrying the height");
}

void test_height_positive_to_zero_fires_hide() {
  AttachFixture f;
  f.data = info_of(true, 300.0);
  f.attach();
  f.move_to(false, 0.0);
  check(f.fired({"hide"}, {0.0}), "a height falling to zero is a hide");
}

void test_height_change_while_visible_fires_resize() {
  AttachFixture f;
  f.data = info_of(true, 300.0);
  f.attach();
  f.move_to(true, 400.0);
  check(f.fired({"resize"}, {400.0}), "a height change while visible is a resize");
}

void test_a_change_that_moves_nothing_fires_nothing() {
  AttachFixture f;
  f.data = info_of(true, 300.0);
  f.attach();
  f.move_to(true, 300.0);
  check(f.events.empty(), "a reported change with the same height while visible emits nothing");

  f.move_to(false, 0.0);
  check(f.fired({"hide"}, {0.0}), "and the transition it was hiding still fires once it happens");

  f.move_to(false, 0.0);
  check(f.fired({"hide"}, {0.0}), "while a further change with no height behind it emits nothing");
}

void test_the_baseline_is_the_height_at_attach_not_the_first_change() {
  AttachFixture f;
  f.data = info_of(true, 300.0);
  f.attach();
  // A keyboard that is ALREADY visible when attached has no up->down edge to report, so its first movement is a
  // resize rather than a show.
  f.move_to(true, 400.0);
  check(f.fired({"resize"}, {400.0}),
        "the pre-change height is captured at attach, so an already-visible keyboard resizes rather than shows");
}

void test_hiding_resets_the_baseline() {
  AttachFixture f;
  f.attach();
  f.move_to(true, 300.0);
  f.move_to(false, 0.0);
  // A hide rebases the baseline to zero, so the next rise is a fresh show rather than a resize against the height
  // the keyboard had before it went away.
  f.move_to(true, 250.0);
  check(f.fired({"show", "hide", "show"}, {300.0, 0.0, 250.0}),
        "hiding resets the baseline, so a second rise is a show at the new height");
}

void test_transitions_report_in_order() {
  AttachFixture f;
  f.attach();
  f.move_to(true, 300.0);
  f.move_to(true, 400.0);
  f.move_to(false, 0.0);
  check(f.fired({"show", "resize", "hide"}, {300.0, 400.0, 0.0}),
        "a show, a resize and a hide report once each, in the order they happened");
}

void test_attach_replaces_any_previous_subscription() {
  AttachFixture f;
  f.attach();
  f.attach();
  check(f.change.unsubscribe_calls == 1, "re-attaching releases the subscription it replaces, exactly once");

  // Only the newest listener is live: the replaced one was released, so it must not be the one driving signals.
  f.move_to(true, 300.0);
  check(f.fired({"show"}, {300.0}), "and the replacement drives the transitions");
}

// --- Detach and dispose ---------------------------------------------------------------------------------------

void test_detach_is_idempotent_and_safe_when_never_attached() {
  AttachFixture f;
  keyboard::detach_soft_keyboard(f.keyboard);
  check(true, "detaching a keyboard that was never attached does nothing and does not throw");

  f.attach();
  keyboard::detach_soft_keyboard(f.keyboard);
  keyboard::detach_soft_keyboard(f.keyboard);
  check(f.change.unsubscribe_calls == 1, "a second detach of an attached keyboard releases nothing further");
}

void test_dispose_releases_the_subscription() {
  AttachFixture f;
  f.attach();
  keyboard::dispose_soft_keyboard(f.keyboard);
  check(f.change.unsubscribe_calls == 1, "disposing a keyboard releases its subscription");
  keyboard::detach_soft_keyboard(f.keyboard);
  check(f.change.unsubscribe_calls == 1, "and leaves nothing behind for a following detach to release");
}

// --- The entity and the info reads ----------------------------------------------------------------------------

void test_create_soft_keyboard_carries_three_distinct_signals() {
  auto keyboard = keyboard::create_soft_keyboard();
  check(keyboard->on_show != nullptr && keyboard->on_hide != nullptr && keyboard->on_resize != nullptr,
        "a fresh keyboard carries show, hide and resize signals");
  // The three signals carry different signatures, so identity is compared through the erased pointer.
  const void* show = keyboard->on_show.get();
  const void* hide = keyboard->on_hide.get();
  const void* resize = keyboard->on_resize.get();
  check(show != hide && show != resize && hide != resize,
        "and each is its own signal rather than one shared by several fields");
}

void test_info_reads_report_the_backend_value() {
  check(keyboard::get_soft_keyboard_height(info_row(info_of(false, 250.0))) == 250.0,
        "the height read reports the backend's height");
  check(keyboard::get_soft_keyboard_height(info_row(info_of(false, 0.0))) == 0.0, "including a height of zero");
  check(keyboard::is_soft_keyboard_visible(info_row(info_of(true, 300.0))), "visibility reports a visible keyboard");
  check(!keyboard::is_soft_keyboard_visible(info_row(info_of(false, 0.0))),
        "and reports a hidden one as not visible");
}

void test_get_info_writes_the_out_object_and_returns_that_very_object() {
  auto out = info_of(false, 0.0);
  auto result = keyboard::get_soft_keyboard_info(info_row(info_of(true, 300.0, 10.0, 20.0, 400.0)), out);

  check(result.get() == out.get(), "the out object is returned itself, not a copy of it");
  check(out->visible && out->height == 300.0 && out->x == 10.0 && out->y == 20.0 && out->width == 400.0,
        "and every field the backend reported landed on it");
}

void test_the_scratch_reads_do_not_alias_a_callers_object() {
  auto out = info_of(false, 0.0);
  keyboard::get_soft_keyboard_info(info_row(info_of(true, 300.0)), out);
  // The convenience reads share one module-level scratch; the caller's object is a different one and must not be
  // written through.
  check(keyboard::get_soft_keyboard_height(info_row(info_of(false, 111.0))) == 111.0,
        "a following height read answers for its own backend");
  check(out->height == 300.0 && out->visible, "and leaves the object a previous call filled in untouched");
}

// --- Delegation to the host capabilities ----------------------------------------------------------------------

struct SetterBackend {
  std::string last_call;
  bool last_bool = false;
  flight::String last_string;
  std::string result = "ok";
  bool reject = false;

  void record(const std::string& call) { last_call = call; }

  flight::Task<types::SoftKeyboardVisibilityResult> visibility() {
    return reject ? flight::Task<types::SoftKeyboardVisibilityResult>::reject(std::string("backend down"))
                  : flight::Task<types::SoftKeyboardVisibilityResult>::resolve(flight::String(result));
  }
  flight::Task<types::SoftKeyboardSetterResult> setter() {
    return reject ? flight::Task<types::SoftKeyboardSetterResult>::reject(std::string("backend down"))
                  : flight::Task<types::SoftKeyboardSetterResult>::resolve(flight::String(result));
  }
};

void test_visibility_setters_delegate_and_pass_results_through() {
  SetterBackend backend;
  auto show_capability = flight::make_ref<types::HostSoftKeyboardVisibilityCapability>();
  show_capability->show = [&backend]() { backend.record("show"); return backend.visibility(); };
  show_capability->hide = [&backend]() { backend.record("hide"); return backend.visibility(); };
  auto row = flight::make_structural_ref<VisibilityRow>(flight::row_field<flight::RowKey<"show">>(show_capability->show),
                                                        flight::row_field<flight::RowKey<"hide">>(show_capability->hide));

  check_equal(keyboard::show_soft_keyboard(row).get(), types::soft_keyboard_visibility_ok_kind,
              "show returns the backend's result");
  check(backend.last_call == "show", "after calling that backend's show");
  check_equal(keyboard::hide_soft_keyboard(row).get(), types::soft_keyboard_visibility_ok_kind,
              "hide returns the backend's result");
  check(backend.last_call == "hide", "after calling that backend's hide");

  backend.result = "operation-failed";
  check_equal(keyboard::show_soft_keyboard(row).get(), types::soft_keyboard_visibility_operation_failed_kind,
              "and a failing backend's result reaches the caller unchanged");
}

void test_the_four_setters_delegate_with_their_argument_intact() {
  SetterBackend backend;

  auto accessory = flight::make_ref<types::HostSoftKeyboardAccessoryBarCapability>();
  accessory->set_accessory_bar_visible = [&backend](bool visible) { backend.last_bool = visible; return backend.setter(); };
  auto accessory_row = flight::make_structural_ref<AccessoryRow>(
      flight::row_field<flight::RowKey<"setAccessoryBarVisible">>(accessory->set_accessory_bar_visible));

  auto resize_mode = flight::make_ref<types::HostSoftKeyboardResizeModeWriteCapability>();
  resize_mode->set_resize_mode = [&backend](flight::String mode) { backend.last_string = mode; return backend.setter(); };
  auto resize_row = flight::make_structural_ref<ResizeModeRow>(
      flight::row_field<flight::RowKey<"setResizeMode">>(resize_mode->set_resize_mode));

  auto scroll_assist = flight::make_ref<types::HostSoftKeyboardScrollAssistCapability>();
  scroll_assist->set_scroll_assist_enabled = [&backend](bool enabled) { backend.last_bool = enabled; return backend.setter(); };
  auto scroll_row = flight::make_structural_ref<ScrollAssistRow>(
      flight::row_field<flight::RowKey<"setScrollAssistEnabled">>(scroll_assist->set_scroll_assist_enabled));

  auto style = flight::make_ref<types::HostSoftKeyboardStyleCapability>();
  style->set_style = [&backend](flight::String kind) { backend.last_string = kind; return backend.setter(); };
  auto style_row = flight::make_structural_ref<StyleRow>(flight::row_field<flight::RowKey<"setStyle">>(style->set_style));

  check_equal(keyboard::set_soft_keyboard_accessory_bar_visible(accessory_row, true).get(),
              types::soft_keyboard_setter_ok_kind, "the accessory bar setter reports the backend's result");
  check(backend.last_bool, "and passed the flag through as given");

  keyboard::set_soft_keyboard_accessory_bar_visible(accessory_row, false).get();
  check(!backend.last_bool, "and passes a false through as false rather than dropping it");

  check_equal(keyboard::set_soft_keyboard_resize_mode(resize_row, types::soft_keyboard_resize_body_kind).get(),
              types::soft_keyboard_setter_ok_kind, "the resize mode setter reports the backend's result");
  check_equal(backend.last_string, types::soft_keyboard_resize_body_kind, "and passed the mode through by value");

  check_equal(keyboard::set_soft_keyboard_scroll_assist_enabled(scroll_row, true).get(),
              types::soft_keyboard_setter_ok_kind, "the scroll assist setter reports the backend's result");
  check(backend.last_bool, "and passed its flag through");

  check_equal(keyboard::set_soft_keyboard_style(style_row, types::soft_keyboard_style_dark_kind).get(),
              types::soft_keyboard_setter_ok_kind, "the style setter reports the backend's result");
  check_equal(backend.last_string, types::soft_keyboard_style_dark_kind, "and passed the style through by value");

  backend.result = "operation-unavailable";
  check_equal(keyboard::set_soft_keyboard_style(style_row, types::soft_keyboard_style_dark_kind).get(),
              types::soft_keyboard_setter_operation_unavailable_kind,
              "and an unavailable backend is reported as such rather than folded into a failure");
}

void test_a_rejected_backend_task_stays_a_rejection() {
  SetterBackend backend;
  backend.reject = true;
  auto show_capability = flight::make_ref<types::HostSoftKeyboardVisibilityCapability>();
  show_capability->show = [&backend]() { return backend.visibility(); };
  show_capability->hide = [&backend]() { return backend.visibility(); };
  auto row = flight::make_structural_ref<VisibilityRow>(flight::row_field<flight::RowKey<"show">>(show_capability->show),
                                                        flight::row_field<flight::RowKey<"hide">>(show_capability->hide));

  bool rejected = false;
  try {
    keyboard::show_soft_keyboard(row).get();
  } catch (const std::exception&) {
    rejected = true;
  }
  check(rejected, "a rejected backend task reaches the caller as a rejection, not as a value");
}

} // namespace

int main() {
  test_attach_reports_ok_and_registers_the_subscription();
  test_attach_reports_acquisition_failed_and_registers_nothing();
  test_height_zero_to_positive_fires_show_with_the_new_height();
  test_height_positive_to_zero_fires_hide();
  test_height_change_while_visible_fires_resize();
  test_a_change_that_moves_nothing_fires_nothing();
  test_the_baseline_is_the_height_at_attach_not_the_first_change();
  test_hiding_resets_the_baseline();
  test_transitions_report_in_order();
  test_attach_replaces_any_previous_subscription();

  test_detach_is_idempotent_and_safe_when_never_attached();
  test_dispose_releases_the_subscription();

  test_create_soft_keyboard_carries_three_distinct_signals();
  test_info_reads_report_the_backend_value();
  test_get_info_writes_the_out_object_and_returns_that_very_object();
  test_the_scratch_reads_do_not_alias_a_callers_object();

  test_visibility_setters_delegate_and_pass_results_through();
  test_the_four_setters_delegate_with_their_argument_intact();
  test_a_rejected_backend_task_stays_a_rejection();

  if (failures == 0) {
    std::cout << "keyboard parity: all checks passed\n";
    return 0;
  }
  std::cerr << failures << " check(s) failed\n";
  return 1;
}
