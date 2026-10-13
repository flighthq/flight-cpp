// Native parity test for @flighthq/app's window, ported from appWindow.test.ts.
//
// The window is the package's widest surface -- 90 upstream cases over roughly sixty functions -- and most of them
// are one-line delegations to a host capability. What this file covers is the part that is NOT a delegation, plus a
// representative sample of the delegations so the routing shape is pinned:
//
//   * The terminal-close rule. `closeWindow` closes the window and emits onClose; a SECOND close answers true
//     without reaching the host a second time, and a successful re-attach clears that terminal state so the window
//     can be closed again. `notifyWindowClosed` is the host's own terminal notification and is guarded the same way,
//     so a native callback racing a `closeWindow` still produces exactly one close.
//   * Close is PINNED to the host that attached the window: closing through a different host still closes, through
//     the origin, and the other host sees no calls at all.
//   * The veto. `requestWindowClose` emits onCloseRequest and reports whether a listener cancelled -- a listener
//     that does not cancel lets the close proceed.
//   * The change-guards the events carry. `onMove` fires only when the position actually moved, and `restoreWindow`
//     returns early unless the window is minimized or maximized.
//   * The two pieces of arithmetic: `computeWindowDeviceTransform` writes a uniform devicePixelRatio scale with no
//     translation, and `getWindowBounds` fills the caller's object and returns THAT object.
//
// Coverage boundary, stated rather than implied: functions whose host parameter is a MERGED row (a `RowMerge` of a
// capability row and a required companion row -- `centerWindow`, `setWindowOpacity`, `setWindowResizable`,
// `setWindowIcon`, `setWindowMenuBarVisible`, `flashWindowFrame` and several `attachWindow*` families) are not
// covered here. Constructing a merged row is a separate exercise from calling the function, and no case in this file
// invents one. Neither are app core or appHost, per the parcel.
//
// The module ships from the hand-written override `app-window-ref-member-access-and-optional-calls` (status
// complete, remaining None), so this pins the behavior of that supply.
#include <flight/app/app_window.hpp>
#include <flight/signals/slot.hpp>

#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace {

namespace app = flight::app;
namespace types = flight::types;
namespace signals = flight::signals;

using Window = types::AppWindow;
using Bounds = types::WindowBounds;
using AnyPlainFn = std::function<void()>;
using AnyStringFn = std::function<void(flight::String)>;
using Outcome = std::optional<flight::String>;

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

std::string utf8(const flight::String& value) { return value.to_utf8(); }

// One recording host holding every capability this file drives, sharing one call log so a case can assert both
// "which capability was asked" and "in what order".
struct Host {
  std::vector<std::string> calls;
  std::function<bool()> on_close_request;
  std::function<void()> on_close;
  std::function<void(double, double)> on_move;
  std::function<void(double, double, double)> on_resize;
  std::function<void(bool)> on_visibility;

  flight::Ref<types::HostWindowAttachCapability> attach_capability;
  flight::Ref<types::HostWindowLifecycleCapability> lifecycle_capability;
  flight::Ref<types::HostWindowGeometryCapability> geometry_capability;
  flight::Ref<types::HostWindowVisibilityCapability> visibility_capability;
  flight::Ref<types::HostWindowFocusCapability> focus_capability;
  flight::Ref<types::HostWindowStateCapability> state_capability;
  flight::Ref<types::HostWindowAppearanceCapability> appearance_capability;
  flight::Ref<types::HostWindowZOrderCapability> zorder_capability;
  flight::Ref<types::HostWindowFullscreenCapability> fullscreen_capability;
  flight::Ref<types::HostWindowProgressCapability> progress_capability;
  flight::Ref<types::HostWindowShadowCapability> shadow_capability;
  flight::Ref<types::HostInputFocusCapability> input_focus_capability;
  flight::Ref<types::HostInputPointerLockCapability> pointer_lock_capability;

  Host() { build(); }

  void build() {
    attach_capability = flight::make_ref<types::HostWindowAttachCapability>();
    attach_capability->attach = [this](flight::Ref<Window>, flight::Any, flight::String ownership) {
      calls.push_back("attach:" + utf8(ownership));
      return true;
    };

    lifecycle_capability = flight::make_ref<types::HostWindowLifecycleCapability>();
    lifecycle_capability->close = [this](flight::Ref<Window>) { calls.push_back("close"); };
    lifecycle_capability->open = [this](flight::Ref<Window>, flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<types::WindowOptions>>>>) {
      calls.push_back("open");
      return true;
    };
    lifecycle_capability->subscribe_close = [this](std::function<bool()> on_request, std::function<void()> on_closed) {
      calls.push_back("subscribeClose");
      on_close_request = std::move(on_request);
      on_close = std::move(on_closed);
      return std::function<void()>([this]() {
        calls.push_back("releaseClose");
        on_close_request = {};
        on_close = {};
      });
    };

    geometry_capability = flight::make_ref<types::HostWindowGeometryCapability>();
    geometry_capability->get_bounds = [this](flight::Ref<Window>, flight::Ref<Bounds> out) {
      calls.push_back("getBounds");
      out->x = 1.0;
      out->y = 2.0;
      out->width = 3.0;
      out->height = 4.0;
      return out;
    };
    geometry_capability->set_position = [this](flight::Ref<Window>, double x, double y) {
      calls.push_back("setPosition:" + std::to_string(static_cast<long>(x)) + "," + std::to_string(static_cast<long>(y)));
    };
    geometry_capability->set_size = [this](flight::Ref<Window>, double w, double h) {
      calls.push_back("setSize:" + std::to_string(static_cast<long>(w)) + "," + std::to_string(static_cast<long>(h)));
    };
    geometry_capability->subscribe_move = [this](std::function<void(double, double)> listener) {
      calls.push_back("subscribeMove");
      on_move = std::move(listener);
      return std::function<void()>([this]() {
        calls.push_back("releaseMove");
        on_move = {};
      });
    };

    visibility_capability = flight::make_ref<types::HostWindowVisibilityCapability>();
    visibility_capability->hide = [this](flight::Ref<Window>) { calls.push_back("hide"); };
    visibility_capability->show = [this](flight::Ref<Window>) { calls.push_back("show"); };
    visibility_capability->subscribe_visibility = [this](std::function<void(bool)> listener) {
      calls.push_back("subscribeVisibility");
      on_visibility = std::move(listener);
      return std::function<void()>([this]() {
        calls.push_back("releaseVisibility");
        on_visibility = {};
      });
    };

    focus_capability = flight::make_ref<types::HostWindowFocusCapability>();
    focus_capability->focus = [this](flight::Ref<Window>) { calls.push_back("focus"); };

    state_capability = flight::make_ref<types::HostWindowStateCapability>();
    state_capability->minimize = [this](flight::Ref<Window> w) { calls.push_back("minimize"); w->minimized = true; };
    state_capability->maximize = [this](flight::Ref<Window> w) { calls.push_back("maximize"); w->maximized = true; };
    state_capability->restore = [this](flight::Ref<Window> w) {
      calls.push_back("restore");
      w->minimized = false;
      w->maximized = false;
    };

    appearance_capability = flight::make_ref<types::HostWindowAppearanceCapability>();
    appearance_capability->set_title = [this](flight::Ref<Window>, flight::String title) {
      calls.push_back("setTitle:" + utf8(title));
    };

    zorder_capability = flight::make_ref<types::HostWindowZOrderCapability>();
    zorder_capability->set_always_on_top = [this](flight::Ref<Window>, bool value) {
      calls.push_back(std::string("setAlwaysOnTop:") + (value ? "true" : "false"));
    };

    fullscreen_capability = flight::make_ref<types::HostWindowFullscreenCapability>();
    fullscreen_capability->set_fullscreen = [this](flight::Ref<Window>, bool value) {
      calls.push_back(std::string("setFullscreen:") + (value ? "true" : "false"));
    };

    progress_capability = flight::make_ref<types::HostWindowProgressCapability>();
    progress_capability->set_progress = [this](flight::Ref<Window>, double value) {
      calls.push_back("setProgress:" + std::to_string(static_cast<long>(value)));
    };

    shadow_capability = flight::make_ref<types::HostWindowShadowCapability>();
    shadow_capability->set_has_shadow = [this](flight::Ref<Window>, bool value) {
      calls.push_back(std::string("setHasShadow:") + (value ? "true" : "false"));
    };

    input_focus_capability = flight::make_ref<types::HostInputFocusCapability>();
    input_focus_capability->subscribe = [this](flight::Ref<types::InputTargetHandle>, std::function<void()>, std::function<void()>) {
      calls.push_back("subscribeInputFocus");
      return std::function<void()>([this]() { calls.push_back("releaseInputFocus"); });
    };

    pointer_lock_capability = flight::make_ref<types::HostInputPointerLockCapability>();
    pointer_lock_capability->request = [this](flight::Ref<types::InputTargetHandle>) {
      calls.push_back("requestLock");
      return flight::Task<types::InputPointerLockRequestOutcome>::resolve(flight::make_ref<types::reason_43a745d20647bfb6>(
          types::reason_43a745d20647bfb6{.reason = flight::String("ok")}));
    };
    pointer_lock_capability->exit = [this]() {
      calls.push_back("exitLock");
      return flight::Task<types::InputPointerLockExitOutcome>::resolve(flight::make_ref<types::reason_43a745d20647bfb6>(
          types::reason_43a745d20647bfb6{.reason = flight::String("ok")}));
    };
  }

  void emit_closed() { if (on_close) on_close(); }
  bool emit_close_request() { return on_close_request ? on_close_request() : false; }

  // The module reads several of these members through the capability REFERENCE rather than by row key -- `close`,
  // `subscribeClose`, `subscribeMove` and `subscribeVisibility` have no key in the generated member table at all --
  // so the capabilities are handed over as Refs and convert to rows at the call site. A row assembled from
  // row_field cells could not carry those four members, and one that mixed them with keyed fields would not
  // compile. That is a property of the table, recorded in docs/parity/plat.md, not of this module.
};



flight::Any native_handle() { return flight::Any(41.0); }

// --- Creation and defaults ------------------------------------------------------------------------------------

void test_a_fresh_window_carries_every_signal_and_its_declared_defaults() {
  const auto win = app::create_app_window();

  check(win->on_activate && win->on_close && win->on_close_request && win->on_deactivate && win->on_drop_file &&
            win->on_focus_in && win->on_focus_out && win->on_fullscreen_changed && win->on_maximize &&
            win->on_minimize && win->on_move && win->on_orientation_changed && win->on_render_context_lost &&
            win->on_render_context_restored && win->on_resize && win->on_restore,
        "a fresh window carries all sixteen signals");

  check(utf8(win->title) == "" && utf8(win->icon) == "", "with an empty title and icon");
  check_close(win->x, 0.0, "positioned at the origin");
  check_close(win->y, 0.0, "on both axes");
  check_close(win->width, 0.0, "with no size yet");
  check_close(win->height, 0.0, "on either axis");
  check_close(win->device_pixel_ratio, 1.0, "and a device pixel ratio of one, not zero");
  check(!win->minimized && !win->maximized && !win->fullscreen && !win->focused,
        "neither minimized, maximized, fullscreen nor focused");
  check(win->visible && win->resizable, "but visible and resizable");
  check(!win->always_on_top && !win->skip_taskbar, "not always-on-top and not hidden from the taskbar");
  check_close(win->opacity, 1.0, "fully opaque");
  check_close(win->min_width, 0.0, "with no minimum width");
  check_close(win->max_width, -1.0, "and the unbounded sentinel for the maximum width, not zero");
  check_close(win->max_height, -1.0, "on both axes");
}

// --- Attaching ------------------------------------------------------------------------------------------------

void test_attaching_reports_the_hosts_answer_and_passes_the_ownership_on() {
  Host host;
  const auto win = app::create_app_window();

  check(app::attach_window(host.attach_capability, host.lifecycle_capability, win, native_handle(), flight::String("host")),
        "attaching returns the host's own answer");
  check(host.calls == std::vector<std::string>({"attach:host"}), "the host saw the attach with its ownership");

  Host second;
  check(app::attach_window(second.attach_capability, second.lifecycle_capability, win, native_handle(), flight::String("flight")),
        "a second host can attach it too");
  check(second.calls == std::vector<std::string>({"attach:flight"}), "with its own ownership");
}

void test_the_object_handed_to_the_host_is_the_window_itself() {
  Host host;
  const auto win = app::create_app_window();
  flight::Ref<Window> seen;
  host.attach_capability->attach = [&seen](flight::Ref<Window> target, flight::Any, flight::String) {
    seen = target;
    return true;
  };

  app::attach_window(host.attach_capability, host.lifecycle_capability, win, native_handle(), flight::String("host"));
  check(seen.get() == win.get(), "the host receives the very window that was attached, not a copy");
}

// --- The close lifecycle --------------------------------------------------------------------------------------

void test_closing_closes_once_and_then_remembers_that_it_did() {
  Host host;
  const auto win = app::create_app_window();
  int closes = 0;
  signals::connect_signal<AnyPlainFn>(win->on_close, [&closes]() { ++closes; }, std::nullopt);

  app::attach_window(host.attach_capability, host.lifecycle_capability, win, native_handle(), flight::String("host"));
  check(app::close_window(host.lifecycle_capability, win), "closing a live window reports success");
  check(host.calls == std::vector<std::string>({"attach:host", "close"}), "and reached the host's close");

  check(app::close_window(host.lifecycle_capability, win), "closing it again also reports success");
  check(host.calls == std::vector<std::string>({"attach:host", "close"}),
        "but does NOT reach the host a second time -- the window is already terminal");
  check(closes == 1, "and the close was announced once");
}

void test_the_hosts_own_terminal_notification_is_guarded_the_same_way() {
  Host host;
  const auto win = app::create_app_window();
  int closes = 0;
  signals::connect_signal<AnyPlainFn>(win->on_close, [&closes]() { ++closes; }, std::nullopt);
  app::attach_window(host.attach_capability, host.lifecycle_capability, win, native_handle(), flight::String("host"));

  app::notify_window_closed(win);
  app::notify_window_closed(win);
  check(closes == 1, "a host reporting the window closed twice still announces one close");
}

void test_a_synchronous_native_close_and_a_close_call_converge_on_one_close() {
  Host host;
  const auto win = app::create_app_window();
  int closes = 0;
  signals::connect_signal<AnyPlainFn>(win->on_close, [&closes]() { ++closes; }, std::nullopt);
  // The host closes the window synchronously from inside its own close call, which is what a native window
  // manager does when it destroys the window at once.
  host.lifecycle_capability->close = [&win](flight::Ref<Window>) { app::notify_window_closed(win); };
  app::attach_window(host.attach_capability, host.lifecycle_capability, win, native_handle(), flight::String("host"));

  app::close_window(host.lifecycle_capability, win);
  app::notify_window_closed(win);
  check(closes == 1, "the native close and the explicit close do not both announce a close");
}

void test_reattaching_clears_the_terminal_state() {
  Host host;
  const auto win = app::create_app_window();
  int closes = 0;
  signals::connect_signal<AnyPlainFn>(win->on_close, [&closes]() { ++closes; }, std::nullopt);

  app::attach_window(host.attach_capability, host.lifecycle_capability, win, native_handle(), flight::String("host"));
  app::close_window(host.lifecycle_capability, win);
  app::attach_window(host.attach_capability, host.lifecycle_capability, win, native_handle(), flight::String("host"));
  app::close_window(host.lifecycle_capability, win);

  check(closes == 2, "a window that re-attaches can be closed again, and announces each close");
}

void test_close_is_pinned_to_the_host_that_attached_the_window() {
  Host origin;
  Host other;
  const auto win = app::create_app_window();

  app::attach_window(origin.attach_capability, origin.lifecycle_capability, win, native_handle(), flight::String("host"));
  check(app::close_window(other.lifecycle_capability, win), "closing through a different host still closes the window");
  check(origin.calls == std::vector<std::string>({"attach:host", "close"}),
        "and it went to the host that attached it");
  check(other.calls.empty(), "while the other host was never asked to close anything");
}

void test_a_close_request_reports_whether_a_listener_vetoed_it() {
  const auto win = app::create_app_window();
  check(app::request_window_close(win), "with no listener the close may proceed");

  int requested = 0;
  signals::connect_signal<AnyPlainFn>(win->on_close_request, [&requested]() { ++requested; }, std::nullopt);
  check(app::request_window_close(win), "a listener that does not cancel lets it proceed");
  check(requested == 1, "and was asked");

  signals::connect_signal<AnyPlainFn>(win->on_close_request,
                                      [&win]() { signals::cancel_signal<std::function<void()>>(win->on_close_request); },
                                      std::nullopt);
  check(!app::request_window_close(win), "a listener that cancels vetoes the request");
}

void test_the_hosts_close_request_is_routed_to_the_window() {
  Host host;
  const auto win = app::create_app_window();
  int requested = 0;
  signals::connect_signal<AnyPlainFn>(win->on_close_request, [&requested]() { ++requested; }, std::nullopt);

  const auto close_capability = flight::make_ref<app::close_open_subscribe_close_0d83114619f29e62>();
  close_capability->close = host.lifecycle_capability->close;
  close_capability->open = host.lifecycle_capability->open;
  close_capability->subscribe_close = host.lifecycle_capability->subscribe_close;
  app::attach_window_close(close_capability, win);
  check(host.emit_close_request() == false, "the host's own close request reaches the window and is not vetoed");
  check(requested == 1, "having asked the listener");

  host.emit_closed();
}

// --- The visibility, focus and move events --------------------------------------------------------------------

void test_visibility_events_reach_the_window_and_the_host_is_released_on_detach() {
  Host host;
  const auto win = app::create_app_window();
  int deactivated = 0;
  int activated = 0;
  signals::connect_signal<AnyPlainFn>(win->on_deactivate, [&deactivated]() { ++deactivated; }, std::nullopt);
  signals::connect_signal<AnyPlainFn>(win->on_activate, [&activated]() { ++activated; }, std::nullopt);

  const auto visibility_capability = flight::make_ref<app::hide_show_subscribe_visibility_957602125e62c21e>();
  visibility_capability->hide = host.visibility_capability->hide;
  visibility_capability->show = host.visibility_capability->show;
  visibility_capability->subscribe_visibility = host.visibility_capability->subscribe_visibility;
  app::attach_window_visibility(visibility_capability, win);
  host.on_visibility(false);
  check(deactivated == 1, "the page going hidden deactivates the window");
  host.on_visibility(true);
  check(activated == 1, "and coming back activates it");

  app::detach_window_visibility(win);
  check(host.calls.back() == "releaseVisibility", "detaching releases the host's subscription");
}

void test_a_move_that_changes_nothing_does_not_announce_a_move() {
  Host host;
  const auto win = app::create_app_window();
  win->x = 50.0;
  win->y = 50.0;
  int moves = 0;
  signals::connect_signal<AnyPlainFn>(win->on_move, [&moves]() { ++moves; }, std::nullopt);

  const auto geometry_carrier = flight::make_ref<
      app::center_get_bounds_set_position_set_size_subscribe_move_subscribe_resize_b965b94b2ed7c687>();
  geometry_carrier->get_bounds = host.geometry_capability->get_bounds;
  geometry_carrier->set_position = host.geometry_capability->set_position;
  geometry_carrier->set_size = host.geometry_capability->set_size;
  geometry_carrier->subscribe_move = host.geometry_capability->subscribe_move;
  app::attach_window_move(geometry_carrier, win);

  host.on_move(50.0, 50.0);
  check(moves == 0, "a reported position identical to the current one announces nothing");

  host.on_move(100.0, 200.0);
  check(moves == 1, "while a real move announces one");
  check_close(win->x, 100.0, "and updates the window's x");
  check_close(win->y, 200.0, "and its y");
}

void test_the_input_focus_seam_is_subscribed_and_replaced_cleanly() {
  Host origin;
  Host replacement;
  const auto win = app::create_app_window();
  const auto target = flight::make_ref<types::InputTargetHandle>();

  app::attach_window_focus(origin.input_focus_capability, win, target);
  app::attach_window_focus(replacement.input_focus_capability, win, target);
  app::detach_window_focus(win);

  check(origin.calls == std::vector<std::string>({"subscribeInputFocus", "releaseInputFocus"}),
        "attaching a replacement releases the origin provider");
  check(replacement.calls == std::vector<std::string>({"subscribeInputFocus", "releaseInputFocus"}),
        "and detaching releases the replacement");
}

// --- Delegation -----------------------------------------------------------------------------------------------

void test_the_simple_commands_reach_their_own_capability() {
  Host host;
  const auto win = app::create_app_window();

  app::focus_window(host.focus_capability, win);
  app::minimize_window(host.state_capability, win);
  app::maximize_window(host.state_capability, win);
  app::restore_window(host.state_capability, win);

  check(host.calls == std::vector<std::string>({"focus", "minimize", "maximize", "restore"}),
        "each command reached its own capability, in the order invoked");
}

void test_show_and_hide_guard_on_the_current_visibility() {
  Host host;
  const auto win = app::create_app_window();
  check(win->visible, "a fresh window is visible");

  app::show_window(host.visibility_capability, win);
  check(host.calls.empty(), "showing an already-visible window does not reach the host");

  app::hide_window(host.visibility_capability, win);
  check(host.calls == std::vector<std::string>({"hide"}), "hiding a visible window does");
  check(!win->visible, "and records it hidden");

  app::show_window(host.visibility_capability, win);
  check(host.calls == std::vector<std::string>({"hide", "show"}), "showing a hidden one reaches the host again");
  check(win->visible, "and records it visible");
}

void test_the_fullscreen_setter_guards_on_the_current_value() {
  Host host;
  const auto win = app::create_app_window();
  int changes = 0;
  signals::connect_signal<AnyPlainFn>(win->on_fullscreen_changed, [&changes]() { ++changes; }, std::nullopt);

  app::set_window_fullscreen(host.fullscreen_capability, win, false);
  check(host.calls.empty() && changes == 0,
        "setting fullscreen to the value the window already has does nothing at all");

  app::set_window_fullscreen(host.fullscreen_capability, win, true);
  check(host.calls == std::vector<std::string>({"setFullscreen:true"}),
        "while a real change reaches the host");
  check(win->fullscreen && changes == 1, "records the new value and announces it once");
}

void test_restoring_a_window_that_is_neither_minimized_nor_maximized_does_nothing() {
  Host host;
  const auto win = app::create_app_window();

  app::restore_window(host.state_capability, win);
  check(host.calls.empty(), "restoring an already-restored window does not reach the host");

  win->minimized = true;
  app::restore_window(host.state_capability, win);
  check(host.calls == std::vector<std::string>({"restore"}), "but a minimized one is restored");
}

void test_the_setters_forward_their_arguments() {
  Host host;
  const auto win = app::create_app_window();

  app::set_window_title(host.appearance_capability, win, flight::String("Subject"));
  app::set_window_position(host.geometry_capability, win, 10.0, 20.0);
  app::set_window_size(host.geometry_capability, win, 640.0, 480.0);
  app::set_window_always_on_top(host.zorder_capability, win, true);
  app::set_window_progress(host.progress_capability, win, 3.0);
  app::set_window_has_shadow(host.shadow_capability, win, true);

  check(host.calls == std::vector<std::string>({"setTitle:Subject", "setPosition:10,20", "setSize:640,480",
                                                "setAlwaysOnTop:true", "setProgress:3", "setHasShadow:true"}),
        "every setter passed its argument through unchanged");
  check_close(win->x, 10.0, "and the position setter recorded the position on the window");
  check_close(win->width, 640.0, "as the size setter did its size");
  check(win->always_on_top, "and the z-order setter its flag");
}

// --- The remaining plain-row surface ---------------------------------------------------------------------------
//
// These six are the window functions whose host parameter is a SINGLE capability row -- the ones the first pass
// left unsampled. The merged-row families remain uncovered, and the reason is measured rather than assumed: a
// `RowMerge` over two capabilities cannot be built from the runtime types available to a caller here. Writing one
// cell by cell fails to compile (the write site resolves the merged schema's members to void: "structural row
// schema does not contain the requested property"), and PROJECTING an existing capability row into the merged
// schema compiles but throws `std::bad_cast` at run time. The emitter's own two idioms do not apply either: the
// allocate-as-merged form (node.hpp) needs that same write path, and the cast-a-wider-row form (command.hpp) needs
// one existing row carrying BOTH halves, which no type here provides -- the merged member `center` lives on a
// companion type that nothing wider combines with the geometry row. So the merged families are left uncovered
// rather than covered with an invented row, and the table is not broadened to reach them.

void test_open_applies_the_options_to_the_window_and_delegates() {
  Host host;
  const auto win = app::create_app_window();
  const auto options = flight::make_structural_ref<flight::RowReadonly<flight::RowOf<flight::Ref<types::WindowOptions>>>>(
      flight::row_field<flight::RowKey<"title">>(std::optional<flight::String>{flight::String("Game")}),
      flight::row_field<flight::RowKey<"width">>(std::optional<double>{640.0}),
      flight::row_field<flight::RowKey<"height">>(std::optional<double>{480.0}),
      flight::row_field<flight::RowKey<"alwaysOnTop">>(std::optional<bool>{true}));

  check(app::open_window(host.lifecycle_capability, host.geometry_capability, win, options),
        "opening returns the host's own answer");
  check(utf8(win->title) == "Game", "the options' title landed on the window");
  check_close(win->width, 640.0, "as did its width");
  check_close(win->height, 480.0, "and its height");
  check(win->always_on_top, "and its always-on-top flag");
  check(host.calls == std::vector<std::string>({"open"}), "while the host was asked to open");
}

void test_the_size_constraint_setters_record_and_delegate() {
  Host host;
  const auto win = app::create_app_window();
  const auto constraints = flight::make_ref<types::HostWindowSizeConstraintsCapability>();
  constraints->set_minimum_size = [&host](flight::Ref<Window>, double w, double h) {
    host.calls.push_back("setMinimumSize:" + std::to_string(static_cast<long>(w)) + "," + std::to_string(static_cast<long>(h)));
  };
  constraints->set_maximum_size = [&host](flight::Ref<Window>, double w, double h) {
    host.calls.push_back("setMaximumSize:" + std::to_string(static_cast<long>(w)) + "," + std::to_string(static_cast<long>(h)));
  };

  app::set_window_minimum_size(constraints, win, 320.0, 240.0);
  app::set_window_maximum_size(constraints, win, 1920.0, 1080.0);

  check_close(win->min_width, 320.0, "the minimum width landed on the window");
  check_close(win->min_height, 240.0, "and its minimum height");
  check_close(win->max_width, 1920.0, "the maximum width too, replacing the unbounded sentinel");
  check_close(win->max_height, 1080.0, "and its maximum height");
  check(host.calls == std::vector<std::string>({"setMinimumSize:320,240", "setMaximumSize:1920,1080"}),
        "each having been delegated to the constraints capability");
}

void test_content_protection_and_attention_delegate() {
  Host host;
  const auto win = app::create_app_window();
  const auto protection = flight::make_ref<types::HostWindowContentProtectionCapability>();
  protection->set_content_protection = [&host](flight::Ref<Window>, bool enabled) {
    host.calls.push_back(std::string("setContentProtection:") + (enabled ? "true" : "false"));
  };
  const auto attention = flight::make_ref<types::HostWindowAttentionCapability>();
  attention->request_attention = [&host](flight::Ref<Window>, bool value) {
    host.calls.push_back(std::string("requestAttention:") + (value ? "true" : "false"));
  };

  app::set_window_content_protection(protection, win, true);
  app::request_window_attention(attention, win, false);

  check(host.calls == std::vector<std::string>({"setContentProtection:true", "requestAttention:false"}),
        "each reaching its own capability with its argument");
}

void test_naming_a_parent_passes_null_through_as_null() {
  Host host;
  const auto win = app::create_app_window();
  std::optional<flight::Ref<Window>> seen = flight::Ref<Window>();
  bool called = false;
  const auto hierarchy = flight::make_ref<types::HostWindowHierarchyCapability>();
  hierarchy->set_parent = [&seen, &called](flight::Ref<Window>, std::optional<flight::Ref<Window>> parent) {
    seen = parent;
    called = true;
  };

  app::set_window_parent(hierarchy, win, std::nullopt);
  check(called, "the hierarchy capability was asked");
  check(!seen.has_value(), "and told the window has no parent, rather than being passed a placeholder");
}

// --- The two pieces of arithmetic -----------------------------------------------------------------------------

void test_the_device_transform_is_a_uniform_scale_with_no_translation() {
  const auto win = app::create_app_window();
  win->device_pixel_ratio = 3.0;
  const auto out = flight::make_ref<types::Matrix>();
  out->tx = 9.0;
  out->ty = 9.0;

  const auto result = app::compute_window_device_transform(win, out);
  check(result.get() == out.get(), "the caller's matrix is returned itself");
  check_close(out->a, 3.0, "scaled by the device pixel ratio on x");
  check_close(out->d, 3.0, "and on y");
  check_close(out->b, 0.0, "with no skew");
  check_close(out->c, 0.0, "in either direction");
  check_close(out->tx, 0.0, "and the stale translation cleared on x");
  check_close(out->ty, 0.0, "and on y");
}

void test_bounds_are_read_into_the_callers_object() {
  Host host;
  const auto win = app::create_app_window();
  const auto out = flight::make_ref<Bounds>();

  const auto result = app::get_window_bounds(host.geometry_capability, win, out);
  check(result.get() == out.get(), "the caller's bounds object is returned itself");
  check_close(out->x, 1.0, "carrying the host's x");
  check_close(out->y, 2.0, "and y");
  check_close(out->width, 3.0, "and width");
  check_close(out->height, 4.0, "and height");
  check(host.calls == std::vector<std::string>({"getBounds"}), "read through the geometry capability's own entry");
}

// --- Pointer lock and teardown --------------------------------------------------------------------------------

void test_pointer_lock_requests_and_exits_are_delegated() {
  Host host;
  const auto target = flight::make_ref<types::InputTargetHandle>();

  check(utf8(app::lock_app_input_pointer(host.pointer_lock_capability, target).get()->reason) == "ok",
        "locking reports the host's outcome");
  check(utf8(app::exit_app_input_pointer_lock(host.pointer_lock_capability).get()->reason) == "ok",
        "and exiting does too");
  check(host.calls == std::vector<std::string>({"requestLock", "exitLock"}),
        "each having reached its own pointer-lock entry");
}

void test_teardown_stops_the_attached_observers() {
  Host host;
  const auto win = app::create_app_window();
  const auto visibility_capability = flight::make_ref<app::hide_show_subscribe_visibility_957602125e62c21e>();
  visibility_capability->hide = host.visibility_capability->hide;
  visibility_capability->show = host.visibility_capability->show;
  visibility_capability->subscribe_visibility = host.visibility_capability->subscribe_visibility;
  app::attach_window_visibility(visibility_capability, win);

  app::dispose_app_window(win);
  check(host.calls.back() == "releaseVisibility",
        "disposing a window runs the teardown of every observer attached to it");
}

} // namespace

int main() {
  test_a_fresh_window_carries_every_signal_and_its_declared_defaults();

  test_attaching_reports_the_hosts_answer_and_passes_the_ownership_on();
  test_the_object_handed_to_the_host_is_the_window_itself();

  test_closing_closes_once_and_then_remembers_that_it_did();
  test_the_hosts_own_terminal_notification_is_guarded_the_same_way();
  test_a_synchronous_native_close_and_a_close_call_converge_on_one_close();
  test_reattaching_clears_the_terminal_state();
  test_close_is_pinned_to_the_host_that_attached_the_window();
  test_a_close_request_reports_whether_a_listener_vetoed_it();
  test_the_hosts_close_request_is_routed_to_the_window();

  test_visibility_events_reach_the_window_and_the_host_is_released_on_detach();
  test_a_move_that_changes_nothing_does_not_announce_a_move();
  test_the_input_focus_seam_is_subscribed_and_replaced_cleanly();

  test_the_simple_commands_reach_their_own_capability();
  test_show_and_hide_guard_on_the_current_visibility();
  test_the_fullscreen_setter_guards_on_the_current_value();
  test_restoring_a_window_that_is_neither_minimized_nor_maximized_does_nothing();
  test_the_setters_forward_their_arguments();

  test_open_applies_the_options_to_the_window_and_delegates();
  test_the_size_constraint_setters_record_and_delegate();
  test_content_protection_and_attention_delegate();
  test_naming_a_parent_passes_null_through_as_null();

  test_the_device_transform_is_a_uniform_scale_with_no_translation();
  test_bounds_are_read_into_the_callers_object();

  test_pointer_lock_requests_and_exits_are_delegated();
  test_teardown_stops_the_attached_observers();

  if (failures == 0) {
    std::cout << "app window parity: all checks passed\n";
    return 0;
  }
  std::cerr << failures << " check(s) failed\n";
  return 1;
}
