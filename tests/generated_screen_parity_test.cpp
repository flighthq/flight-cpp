// Behavioural parity for @flighthq/screen, driven by the upstream Vitest fixture in
// .dependencies/flight/packages/screen/src/screen.test.ts.
//
// The package is `assisted` at a627849: 3/3 modules emitted, 3/3 headers compiled, no refusals and no
// overrides -- so everything below pins the emitted code directly.
//
// The host seam is SUPPLIED BY THE TEST, exactly as the upstream fixture supplies it: a recording
// HostScreenQueryCapability whose methods answer with test data. Nothing here invents a production
// display, browser or SDL host, and no seam is stubbed to succeed on its own.
//
// Two semantics get particular attention because they are easy to lose and are directly observable:
//   * the CALLER-OWNED out parameter -- these functions return the caller's own object. The upstream
//     fixture's out-ALIASES-input cases are NOT reachable here and are recorded in place with the exact
//     compile-time boundary rather than faked;
//   * the SCRATCH ARRAY -- the derived screen queries share one module-level array, so the host is
//     handed the same array identity on every call. That is asserted by identity, not by content.

#include <flight/screen/screen.hpp>
#include <flight/types/screen.hpp>
#include <flight/types/screen_mode.hpp>

#include <cmath>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const char* message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

void check_close(double got, double want, const char* message) {
  if (std::abs(got - want) < 1e-9) return;
  std::cerr << "FAIL: " << message << "\n  want " << want << "\n  got  " << got << '\n';
  ++failures;
}

using ScreenRef = flight::Ref<flight::types::ScreenInfo>;
// The INPUT carriers: the emitted parameter is a row over the named Vector2Like / RectangleLike, so an
// input point or rectangle is a Ref<Vector2> / Ref<Rectangle>.
using InputPoint = flight::Ref<flight::types::Vector2>;
using InputRect = flight::Ref<flight::types::Rectangle>;
// The OUTPUT carriers: the emitted out parameter is the anonymous x/y (and x/y/width/height) shape.
using OutPoint = flight::Ref<flight::types::x_y_8365950bd60f783f>;
using OutRect = flight::Ref<flight::types::x_y_width_height_544099c876e1fbdc>;

// The upstream fixture's createTestScreen: the documented defaults, then the fields the fixture names.
ScreenRef make_screen(double id, double x, double y, double width, double height,
                      double scale_factor = 1.0, flight::String label = flight::String("")) {
  const auto screen = flight::screen::create_screen_info();
  screen->id = id;
  screen->x = x;
  screen->y = y;
  screen->width = width;
  screen->height = height;
  screen->work_width = width;
  screen->work_height = height;
  screen->scale_factor = scale_factor;
  screen->label = label;
  return screen;
}

InputPoint make_point(double x, double y) {
  return flight::make_ref<flight::types::Vector2>(flight::types::Vector2{.x = x, .y = y});
}

InputRect make_rect(double x, double y, double width, double height) {
  const auto rect = flight::make_ref<flight::types::Rectangle>();
  rect->x = x;
  rect->y = y;
  rect->width = width;
  rect->height = height;
  return rect;
}

OutPoint out_point(double x = -1.0, double y = -1.0) {
  return flight::make_ref<flight::types::x_y_8365950bd60f783f>(
      flight::types::x_y_8365950bd60f783f{.x = x, .y = y});
}

OutRect out_rect(double x = -1.0, double y = -1.0, double width = -1.0, double height = -1.0) {
  return flight::make_ref<flight::types::x_y_width_height_544099c876e1fbdc>(
      flight::types::x_y_width_height_544099c876e1fbdc{.x = x, .y = y, .width = width, .height = height});
}

// A recording query host. `get_screens` records the ARRAY IDENTITY it was handed, which is how the
// scratch-array contract is checked.
struct RecordingQuery {
  std::vector<ScreenRef> screens;
  int get_screens_calls = 0;
  std::vector<const void*> seen_arrays;
  ScreenRef primary;
  int primary_calls = 0;
  OutPoint cursor;
};

flight::Ref<flight::types::HostScreenQueryCapability> query_host(RecordingQuery& state) {
  const auto host = flight::make_ref<flight::types::HostScreenQueryCapability>();
  host->get_screens = [&state](
                          flight::Array<flight::Ref<flight::types::ScreenInfo>> into) {
    state.get_screens_calls += 1;
    state.seen_arrays.push_back(static_cast<const void*>(into.identity()));
    into.resize(0.0);
    for (const auto& screen : state.screens) into.push(screen);
    return into;
  };
  host->get_primary_screen = [&state](flight::Ref<flight::types::ScreenInfo> out) {
    state.primary_calls += 1;
    if (state.primary != nullptr) *out = *state.primary;
    return out;
  };
  host->get_cursor_position = [&state](OutPoint out) {
    if (state.cursor != nullptr) *out = *state.cursor;
    return out;
  };
  return host;
}

}  // namespace

int main() {
  // ---- dipToScreenPoint / dipToScreenRect / screenToDipPoint / screenToDipRect -------------------
  //
  // Desktop DIPs to screen-local pixels is (dip - screen origin) * scaleFactor for position and
  // * scaleFactor for extent; the inverse divides. Each returns the caller's own object, and each must
  // be safe when the caller passes the same object as the input.

  {
    const auto screen = make_screen(1.0, 100.0, 50.0, 800.0, 600.0, 2.0);

    const auto out = out_point();
    const auto returned = flight::screen::dip_to_screen_point(screen, make_point(112.0, 73.0), out);
    check(returned == out, "dip->screen point: the caller's own object is returned");
    check_close(out->x, 24.0, "dip->screen point: x is (dip - origin) * scale");
    check_close(out->y, 46.0, "dip->screen point: y is (dip - origin) * scale");

    // UNREACHABLE THROUGH THIS API, and measured rather than assumed: the upstream fixture's aliasing
    // cases -- dipToScreenPoint(screen, point, point) -- pass ONE object as both the row input and the
    // out. Here those are unrelated C++ types: the input is a row over the named Vector2Like (Ref<Vector2>)
    // and the out is the anonymous x_y_8365950bd60f783f shape. Passing one as the other is a hard
    // compile error inside the runtime ("structural reference source has an incompatible object type"),
    // so no argument can alias. The value cases above still pin the arithmetic and the returned identity.

    const auto rect_out = out_rect();
    const auto rect_returned =
        flight::screen::dip_to_screen_rect(screen, make_rect(112.0, 73.0, 30.0, 40.0), rect_out);
    check(rect_returned == rect_out, "dip->screen rect: the caller's own object is returned");
    check_close(rect_out->x, 24.0, "dip->screen rect: x is converted");
    check_close(rect_out->y, 46.0, "dip->screen rect: y is converted");
    check_close(rect_out->width, 60.0, "dip->screen rect: the extent is scaled, not offset");
    check_close(rect_out->height, 80.0, "dip->screen rect: the height is scaled");

  }

  {
    // The inverse rounds-trips: pixels to desktop DIPs divides the extent and re-adds the origin.
    const auto screen = make_screen(1.0, 100.0, 50.0, 800.0, 600.0, 2.0);

    const auto out = out_point();
    check(flight::screen::screen_to_dip_point(screen, make_point(24.0, 46.0), out) == out,
          "screen->dip point: the caller's own object is returned");
    check_close(out->x, 112.0, "screen->dip point: x re-adds the origin after dividing");
    check_close(out->y, 73.0, "screen->dip point: y re-adds the origin after dividing");


    const auto rect_out = out_rect();
    flight::screen::screen_to_dip_rect(screen, make_rect(24.0, 46.0, 60.0, 80.0), rect_out);
    check_close(rect_out->x, 112.0, "screen->dip rect: x is converted");
    check_close(rect_out->width, 30.0, "screen->dip rect: the extent is divided, not offset");
    check_close(rect_out->height, 40.0, "screen->dip rect: the height is divided");

  }

  // ---- createScreenInfo / createScreenMode -------------------------------------------------------

  {
    // Documented defaults AND sentinels: -1 means "the host did not report this", which is deliberately
    // different from 0 (a real measurement of zero) and from 1 for scaleFactor.
    const auto screen = flight::screen::create_screen_info();
    check_close(screen->id, 0.0, "defaults: id starts at zero");
    check_close(screen->x, 0.0, "defaults: x starts at zero");
    check_close(screen->y, 0.0, "defaults: y starts at zero");
    check_close(screen->width, 0.0, "defaults: width starts at zero");
    check_close(screen->height, 0.0, "defaults: height starts at zero");
    check_close(screen->scale_factor, 1.0, "defaults: scaleFactor is unity, not zero");
    check(!screen->is_primary, "defaults: isPrimary starts false");
    check_close(screen->rotation, -1.0, "defaults: rotation is the unknown sentinel");
    check(screen->orientation == flight::String("Landscape"), "defaults: orientation starts Landscape");
    check_close(screen->refresh_rate, -1.0, "defaults: refreshRate is the unknown sentinel");
    check_close(screen->color_depth, -1.0, "defaults: colorDepth is the unknown sentinel");
    check_close(screen->pixel_depth, -1.0, "defaults: pixelDepth is the unknown sentinel");
    check_close(screen->physical_width, -1.0, "defaults: physicalWidth is the unknown sentinel");
    check_close(screen->physical_height, -1.0, "defaults: physicalHeight is the unknown sentinel");
    check(!screen->is_hdr, "defaults: isHdr starts false");
    check(screen->color_space == flight::String("srgb"), "defaults: colorSpace starts srgb");
    check_close(screen->max_luminance, -1.0, "defaults: maxLuminance is the unknown sentinel");
    check_close(screen->depth_per_component, -1.0, "defaults: depthPerComponent is the unknown sentinel");
    check_close(screen->dpi, -1.0, "defaults: dpi is the unknown sentinel");
    check(screen->label == flight::String(""), "defaults: label starts empty");
    check(!screen->internal, "defaults: internal starts false");
    check(screen->touch_support == flight::String("unknown"), "defaults: touchSupport starts unknown");
    check(!screen->monochrome, "defaults: monochrome starts false");

    const auto mode = flight::screen::create_screen_mode();
    check_close(mode->width, 0.0, "mode: width starts at zero");
    check_close(mode->height, 0.0, "mode: height starts at zero");
    check_close(mode->refresh_rate, -1.0, "mode: refreshRate is the unknown sentinel");
    check_close(mode->color_depth, -1.0, "mode: colorDepth is the unknown sentinel");
    check(mode->pixel_format == flight::String(""), "mode: pixelFormat starts empty");
  }

  // ---- the pure extractions ----------------------------------------------------------------------

  {
    // Bounds are read straight off the screen, INCLUDING a negative origin -- a secondary screen to the
    // left of the primary is a real desktop layout, not an error.
    const auto screen = make_screen(1.0, -1280.0, 120.0, 1280.0, 720.0);
    const auto out = out_rect();
    check(flight::screen::get_screen_bounds(screen, out) == out,
          "bounds: the caller's own rectangle is returned");
    check_close(out->x, -1280.0, "bounds: a negative desktop origin is preserved");
    check_close(out->y, 120.0, "bounds: the y origin is taken from the screen");
    check_close(out->width, 1280.0, "bounds: the width is taken from the screen");
    check_close(out->height, 720.0, "bounds: the height is taken from the screen");

    // The work area is the same origin with the work dimensions.
    const auto work = out_rect();
    flight::screen::get_screen_work_area(screen, work);
    check_close(work->x, -1280.0, "workArea: the desktop origin is kept");
    check_close(work->y, 120.0, "workArea: the desktop y origin is kept");
    check_close(work->width, 1280.0, "workArea: the work width is taken");
    check_close(work->height, 720.0, "workArea: the work height is taken");
  }

  {
    // The current mode is derived from the screen's own metrics, and a stale pixelFormat is CLEARED
    // rather than carried: the host never reported one for the current mode.
    const auto screen = make_screen(1.0, 0.0, 0.0, 1920.0, 1080.0);
    screen->refresh_rate = 144.0;
    screen->color_depth = 30.0;
    const auto mode = flight::screen::create_screen_mode();
    mode->pixel_format = flight::String("stale");
    check(flight::screen::get_screen_current_mode(screen, mode) == mode,
          "currentMode: the caller's own object is returned");
    check_close(mode->width, 1920.0, "currentMode: the width is the screen's");
    check_close(mode->height, 1080.0, "currentMode: the height is the screen's");
    check_close(mode->refresh_rate, 144.0, "currentMode: the refresh rate is the screen's");
    check_close(mode->color_depth, 30.0, "currentMode: the colour depth is the screen's");
    check(mode->pixel_format == flight::String(""),
          "currentMode: an unknown format is CLEARED, not carried over");
  }

  // ---- the host-delegating queries ---------------------------------------------------------------

  {
    RecordingQuery state;
    state.primary = make_screen(3.0, 10.0, 20.0, 1920.0, 1080.0);
    state.primary->is_primary = true;
    const auto host = query_host(state);
    const auto out = flight::screen::create_screen_info();

    check(flight::screen::get_primary_screen(host, out) == out,
          "primary: the caller-owned object is returned");
    check(state.primary_calls == 1, "primary: the host is asked exactly once");
    check_close(out->id, 3.0, "primary: the host's id is copied in");
    check_close(out->x, 10.0, "primary: the host's origin is copied in");
    check(out->is_primary, "primary: the primary flag is copied in");

    state.cursor = out_point(500.0, 400.0);
    const auto cursor = out_point();
    check(flight::screen::get_screen_cursor_position(host, cursor) == cursor,
          "cursor: the caller-owned point is returned");
    check_close(cursor->x, 500.0, "cursor: the host's cursor x is copied in");
    check_close(cursor->y, 400.0, "cursor: the host's cursor y is copied in");
  }

  {
    // getScreens fills the caller's array and returns it.
    RecordingQuery state;
    state.screens = {make_screen(1.0, 0.0, 0.0, 100.0, 100.0), make_screen(2.0, 300.0, 0.0, 200.0, 150.0)};
    const auto host = query_host(state);
    const auto into = flight::Array<flight::Ref<flight::types::ScreenInfo>>{};
    check(flight::screen::get_screens(host, into) == into,
          "screens: the caller-owned array is returned");
    check(static_cast<double>(into.size()) == 2.0, "screens: every host screen is copied in");
    check_close(into.element(1.0)->id, 2.0, "screens: the order the host gave is kept");
  }

  // ---- getScreenById -----------------------------------------------------------------------------

  {
    RecordingQuery state;
    state.screens = {make_screen(10.0, 0.0, 0.0, 100.0, 100.0),
                     make_screen(20.0, 300.0, 0.0, 200.0, 150.0, 2.0, flight::String("external"))};
    const auto host = query_host(state);
    const auto out = flight::screen::create_screen_info();

    const auto found = flight::screen::get_screen_by_id(host, 20.0, out);
    check(found.has_value() && found.value() == out,
          "byId: a match returns the caller's own object");
    check_close(out->id, 20.0, "byId: the matching id is copied");
    check_close(out->x, 300.0, "byId: the matching origin is copied");
    check_close(out->scale_factor, 2.0, "byId: the matching scale is copied");
    check(out->label == flight::String("external"), "byId: the matching label is copied");

    check(!flight::screen::get_screen_by_id(host, 99.0, out).has_value(),
          "byId: no match resolves nothing");
  }

  {
    // SCRATCH ARRAY IDENTITY. Every derived query shares one module-level array, so the host is handed
    // the SAME array object on each call -- asserted by identity, not by content. A per-call array would
    // pass a content check and fail this one.
    RecordingQuery state;
    state.screens = {make_screen(1.0, 0.0, 0.0, 100.0, 100.0), make_screen(2.0, 300.0, 0.0, 200.0, 150.0)};
    const auto host = query_host(state);
    const auto out = flight::screen::create_screen_info();

    flight::screen::get_screen_by_id(host, 1.0, out);
    flight::screen::get_screen_containing_rect(host, make_rect(10.0, 10.0, 20.0, 20.0), out);
    flight::screen::get_screen_nearest_point(host, make_point(10.0, 10.0), out);
    flight::screen::get_screen_nearest_rect(host, make_rect(10.0, 10.0, 20.0, 20.0), out);

    check(state.get_screens_calls == 4, "scratch: each derived query enumerates the screens once");
    check(state.seen_arrays.size() == 4, "scratch: four enumerations were recorded");
    bool all_same = state.seen_arrays.size() == 4;
    for (const auto* seen : state.seen_arrays) {
      if (seen != state.seen_arrays[0]) all_same = false;
    }
    check(all_same, "scratch: every derived query is handed the SAME array identity");
  }

  // ---- the geometry selections -------------------------------------------------------------------

  {
    // Nearest point: a point INSIDE a screen selects that screen (half-open containment first), and a
    // point outside every screen selects the nearest CENTRE.
    RecordingQuery state;
    state.screens = {make_screen(1.0, 0.0, 0.0, 100.0, 100.0), make_screen(2.0, 300.0, 0.0, 200.0, 150.0)};
    const auto host = query_host(state);
    const auto out = flight::screen::create_screen_info();

    const auto inside = flight::screen::get_screen_nearest_point(host, make_point(10.0, 10.0), out);
    check(inside == out, "nearestPoint: the caller's own object is returned");
    check_close(out->id, 1.0, "nearestPoint: containment wins over distance");

    flight::screen::get_screen_nearest_point(host, make_point(1000.0, 1000.0), out);
    check_close(out->id, 2.0, "nearestPoint: an outside point selects the nearest centre");
  }

  {
    // A tie keeps the FIRST screen: the upstream fixture pins that, and it is the difference between a
    // stable and an arbitrary answer.
    RecordingQuery state;
    state.screens = {make_screen(1.0, 0.0, 0.0, 100.0, 100.0), make_screen(2.0, 400.0, 0.0, 100.0, 100.0)};
    const auto host = query_host(state);
    const auto out = flight::screen::create_screen_info();

    // A point equidistant from both centres (each centre is 50 away from x=250).
    flight::screen::get_screen_nearest_point(host, make_point(250.0, -1000.0), out);
    check_close(out->id, 1.0, "nearestPoint: an exact centre-distance tie keeps the first screen");
  }

  {
    // No screens at all: the documented default is filled into the caller's object rather than leaving
    // it untouched or resolving null.
    RecordingQuery state;
    const auto host = query_host(state);
    const auto out = make_screen(99.0, 10.0, 20.0, 30.0, 40.0);
    flight::screen::get_screen_nearest_point(host, make_point(0.0, 0.0), out);
    check_close(out->id, 0.0, "empty: no screens fills the documented default id");
    check_close(out->x, 0.0, "empty: no screens fills the documented default origin");
    check_close(out->scale_factor, 1.0, "empty: the default scale is unity, not zero");
  }

  {
    // Containing rect: the largest OVERLAP wins when screens overlap.
    RecordingQuery state;
    state.screens = {make_screen(1.0, 0.0, 0.0, 100.0, 100.0), make_screen(2.0, 100.0, 80.0, 100.0, 100.0)};
    const auto host = query_host(state);
    const auto out = flight::screen::create_screen_info();
    flight::screen::get_screen_containing_rect(host, make_rect(50.0, 50.0, 100.0, 60.0), out);
    check_close(out->id, 1.0, "containingRect: the screen with the largest overlap wins");

    // Nearest rect: a screen that fully CONTAINS the rectangle wins over one that merely is closest.
    RecordingQuery state2;
    state2.screens = {make_screen(1.0, 0.0, 0.0, 100.0, 100.0), make_screen(2.0, 100.0, 80.0, 100.0, 100.0)};
    const auto host2 = query_host(state2);
    const auto out2 = flight::screen::create_screen_info();
    flight::screen::get_screen_nearest_rect(host2, make_rect(10.0, 10.0, 20.0, 20.0), out2);
    check_close(out2->id, 1.0, "nearestRect: a fully-containing screen is preferred");
  }

  return failures == 0 ? 0 : 1;
}
