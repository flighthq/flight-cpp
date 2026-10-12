// Focused behavior tests for the re-derived @flighthq/app AppRenderView runtime.
//
// The override's job is the pair of declarations the emitter refused: the runtime reader and the
// initializer. Both lowerings it takes deliberately are load-bearing, so each is exercised here rather
// than asserted in prose.
//
//   1. The runtime reader recovers the concrete owner from the entity runtime slot. A view that never
//      went through the initializer has no runtime, which is the source's own undefined case, and the
//      reader must fail loudly instead of reading through a null owner.
//   2. `synchronize` is connected to the window's resize signal and removed by identity. A callback
//      erased to an arbitrary lambda has no recoverable identity -- the runtime refuses to guess one --
//      so re-attaching a view would throw instead of replacing its connection. Attaching twice is
//      therefore the test: it must both succeed and leave exactly one live connection.
//
// Everything is driven directly: no window server, no timers, and no timing assumptions. The resize
// operation is a counter, so what is asserted is the device-pixel arithmetic and the connection life
// cycle, not how long anything took.
#include <flight/app/app_render_view.hpp>

#include <functional>
#include <iostream>
#include <memory>

namespace {

int failures = 0;

bool check(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
  return condition;
}

using Window = flight::Ref<flight::types::AppWindow>;
using State = flight::Ref<flight::types::RenderState>;
using Target = flight::Ref<flight::types::RenderTargetDimensions>;

// Live connections on the window's resize signal, read the way the emitted removal reads them: an
// emptied signal drops its data entirely, so absence and an empty slot list both mean "none".
double live_slots(const Window& window) {
  if (!window->on_resize->data.has_value()) {
    return 0.0;
  }
  return static_cast<double>(window->on_resize->data.value()->slots.size());
}

Window make_window(double width, double height, double device_pixel_ratio) {
  auto window = flight::make_ref<flight::types::AppWindow>();
  window->width = width;
  window->height = height;
  window->device_pixel_ratio = device_pixel_ratio;
  window->on_resize = flight::make_ref<flight::types::Signal<std::function<void()>>>();
  return window;
}

}  // namespace

int main() {
  auto window = make_window(100.5, 40.25, 2.0);
  auto render_state = flight::make_ref<flight::types::RenderState>();
  auto render_target = flight::make_ref<flight::types::RenderTargetDimensions>();
  auto viewport = flight::make_ref<flight::types::Viewport>();

  int resize_calls = 0;
  double last_width = -1.0;
  double last_height = -1.0;
  State seen_state;
  Target seen_target;
  auto resize = [&](State state, Target target, double width, double height) {
    resize_calls++;
    last_width = width;
    last_height = height;
    seen_state = state;
    seen_target = target;
  };

  auto view = flight::app::create_app_render_view<State, Target>(window, render_state, render_target, viewport,
                                                                 resize);

  // Creation links the window, state, target and viewport, then synchronizes once.
  check(resize_calls == 1, "creation synchronizes exactly once");
  check(last_width == 201.0 && last_height == 81.0,
        "logical window dimensions become rounded device-pixel dimensions");
  check(seen_state == render_state && seen_target == render_target,
        "the resize operation receives the very state and target the view was created with");
  check(viewport->device_pixel_ratio == 2.0 && viewport->width == 201.0 && viewport->height == 81.0,
        "the viewport receives the device pixel ratio and the resolved extent");
  check(viewport->x == 0.0 && viewport->y == 0.0, "the viewport origin is reset to the window origin");
  check(render_state->pixel_ratio == 2.0, "the render state receives the same device pixel ratio");
  check(live_slots(window) == 0.0, "creation subscribes to nothing");

  // Nothing about creation is retained in the view's own fields beyond the runtime: the reader is the
  // only way to reach it, and it must return the runtime the initializer stored.
  const auto runtime = flight::app::get_app_render_view_runtime(view);
  check(runtime != nullptr && !runtime->attached, "the retained runtime is the one creation stored, detached");

  // Attach: synchronize once and subscribe exactly once.
  flight::app::attach_app_render_view(view);
  check(resize_calls == 2, "attach synchronizes");
  check(live_slots(window) == 1.0, "attach subscribes exactly once");

  // Re-attach is the identity test. It removes the existing connection first, which the runtime only
  // allows for a callback carrying its own identity; an erased lambda would throw here instead.
  bool reattach_threw = false;
  try {
    flight::app::attach_app_render_view(view);
  } catch (const std::exception& error) {
    reattach_threw = true;
    std::cerr << "FAIL: re-attach threw: " << error.what() << '\n';
  }
  check(!reattach_threw, "re-attach removes the existing connection rather than failing to match it");
  check(resize_calls == 3, "re-attach synchronizes");
  check(live_slots(window) == 1.0, "re-attach leaves exactly one connection, not two");

  // The connected callback is live: driving the window's signal runs the same synchronize.
  window->on_resize->emit();
  check(resize_calls == 4, "the connected callback synchronizes when the window resizes");

  // A resize is read from the window each time, not cached from creation.
  window->width = 10.0;
  window->height = 5.0;
  window->on_resize->emit();
  check(viewport->width == 20.0 && viewport->height == 10.0, "a later resize re-reads the window extent");
  check(render_state->pixel_ratio == 2.0, "the device pixel ratio is unchanged by a later resize");

  // Detach cancels. The source's detach is idempotent and leaves the linked objects caller-owned.
  flight::app::detach_app_render_view(view);
  check(live_slots(window) == 0.0, "detach removes the connection");
  const int calls_at_detach = resize_calls;
  window->on_resize->emit();
  check(resize_calls == calls_at_detach, "a detached view no longer synchronizes on resize");
  bool second_detach_threw = false;
  try {
    flight::app::detach_app_render_view(view);
  } catch (const std::exception& error) {
    second_detach_threw = true;
    std::cerr << "FAIL: second detach threw: " << error.what() << '\n';
  }
  check(!second_detach_threw, "detach with nothing attached is an ordinary no-op");

  // The reader's absent case: a view that never went through the initializer has no runtime authority,
  // and reading it must fail loudly rather than dereference an empty slot.
  bool absent_threw = false;
  std::string absent_what;
  try {
    auto uninitialized = flight::make_ref<flight::types::AppRenderView<State, Target>>();
    flight::app::detach_app_render_view(uninitialized);
  } catch (const std::exception& error) {
    absent_threw = true;
    absent_what = error.what();
  }
  check(absent_threw, "a view with no retained runtime fails rather than reading through an absent owner");
  check(absent_what == "app: AppRenderView has no retained runtime authority",
        "and it reports which view lost its runtime authority");

  // The callback retains the view weakly, so the view's own death ends the synchronization instead of
  // being pinned alive by the connection the window holds. That is the deliberate lowering of the
  // source's strong closure capture, whose collectable cycle a reference-counted runtime cannot carry.
  flight::app::attach_app_render_view(view);
  check(live_slots(window) == 1.0, "the view is attached again before it is released");
  view.reset();
  const int calls_before_release = resize_calls;
  window->on_resize->emit();
  check(resize_calls == calls_before_release, "a released view's connection is inert, not dangling");
  check(live_slots(window) == 1.0, "and its connection is still the one the window holds");

  if (failures == 0) std::cout << "generated app render view: all checks passed\n";
  return failures == 0 ? 0 : 1;
}
