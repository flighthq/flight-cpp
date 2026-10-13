#include <flight/statusbar/statusbar.hpp>

#include <iostream>
#include <optional>
#include <vector>

namespace {

bool check(bool condition, const char* message) {
  if (!condition) std::cerr << message << '\n';
  return condition;
}

} // namespace

int main() {
  using flight::String;
  using namespace flight::types;

  const auto bar = flight::statusbar::create_status_bar();
  const auto defaults = flight::statusbar::create_status_bar_info();
  if (!check(bar && bar->on_change, "status bar did not create its signal") ||
      !check(defaults->color == 0.0 && defaults->height == -1.0,
             "status info numeric defaults changed") ||
      !check(!defaults->overlays_content && defaults->style == String("default") &&
                 defaults->visible,
             "status info display defaults changed") ||
      !check(flight::statusbar::packed_rgba_to_hex_color(0x123456ff) == String("#123456"),
             "packed RGBA conversion changed")) {
    return 1;
  }

  double source_color = 10.0;
  double source_height = 24.0;
  bool source_overlays = false;
  String source_style("default");
  bool source_visible = true;
  int info_reads = 0;
  const auto info_provider = flight::make_ref<HostStatusBarInfoCapability>(
      HostStatusBarInfoCapability{.get_info = [&](flight::Ref<StatusBarInfo> out) {
        ++info_reads;
        out->color = source_color;
        out->height = source_height;
        out->overlays_content = source_overlays;
        out->style = source_style;
        out->visible = source_visible;
        return out;
      }});

  std::function<void()> host_change;
  int unsubscribes = 0;
  const auto change_provider = flight::make_ref<HostStatusBarChangeCapability>(
      HostStatusBarChangeCapability{.subscribe = [&](std::function<void()> listener) {
        host_change = std::move(listener);
        return [&]() { ++unsubscribes; };
      }});
  int status_events = 0;
  bar->on_change->emit = [&](auto) { ++status_events; };
  flight::statusbar::attach_status_bar(change_provider, info_provider, bar);
  host_change();
  flight::statusbar::attach_status_bar(change_provider, info_provider, bar);
  flight::statusbar::detach_status_bar(bar);
  flight::statusbar::detach_status_bar(bar);
  if (!check(status_events == 1 && info_reads == 1,
             "status change did not publish one fresh info read") ||
      !check(unsubscribes == 2, "attach replacement or idempotent detach changed")) {
    return 1;
  }

  std::vector<double> colors;
  std::vector<std::optional<bool>> color_animations;
  std::vector<bool> overlays;
  std::vector<String> styles;
  std::vector<bool> visibility;
  std::vector<std::optional<String>> visibility_animations;
  const auto color_provider = flight::make_ref<HostStatusBarColorCapability>(
      HostStatusBarColorCapability{.set_background_color =
                                       [&](double color, std::optional<bool> animated) {
                                         colors.push_back(color);
                                         color_animations.push_back(animated);
                                       }});
  const auto overlays_provider = flight::make_ref<HostStatusBarOverlaysCapability>(
      HostStatusBarOverlaysCapability{.set_overlays_content =
                                          [&](bool value) { overlays.push_back(value); }});
  const auto style_provider = flight::make_ref<HostStatusBarStyleCapability>(
      HostStatusBarStyleCapability{.set_style =
                                       [&](String value) { styles.push_back(std::move(value)); }});
  const auto visibility_provider = flight::make_ref<HostStatusBarVisibilityCapability>(
      HostStatusBarVisibilityCapability{.set_visible =
                                            [&](bool value, std::optional<String> animation) {
                                              visibility.push_back(value);
                                              visibility_animations.push_back(std::move(animation));
                                            }});

  flight::statusbar::set_status_bar_color(color_provider, 42.0, true);
  flight::statusbar::set_status_bar_overlays_content(overlays_provider, true);
  flight::statusbar::set_status_bar_style(style_provider, String("dark"));
  flight::statusbar::set_status_bar_visible(visibility_provider, false, String("fade"));
  if (!check(colors.back() == 42.0 && color_animations.back() == true,
             "direct color setter changed") ||
      !check(overlays.back(), "direct overlays setter changed") ||
      !check(styles.back() == String("dark"), "direct style setter changed") ||
      !check(!visibility.back() && visibility_animations.back() == String("fade"),
             "direct visibility setter changed")) {
    return 1;
  }
  colors.clear();
  color_animations.clear();
  overlays.clear();
  styles.clear();
  visibility.clear();
  visibility_animations.clear();

  const auto lower = flight::make_ref<StatusBarStyleEntry>(StatusBarStyleEntry{
      .animation = String("fade"),
      .color = 20.0,
      .overlays_content = std::nullopt,
      .style = String("dark"),
      .visible = false,
  });
  const auto upper = flight::make_ref<StatusBarStyleEntry>(StatusBarStyleEntry{
      .animation = std::nullopt,
      .color = 30.0,
      .overlays_content = true,
      .style = std::nullopt,
      .visible = std::nullopt,
  });
  const auto lower_handle = flight::statusbar::push_status_bar_style_entry(
      color_provider, info_provider, overlays_provider, style_provider, visibility_provider, lower);
  const auto upper_handle = flight::statusbar::push_status_bar_style_entry(
      color_provider, info_provider, overlays_provider, style_provider, visibility_provider, upper);
  if (!check(flight::statusbar::has_status_bar_style_entry(info_provider, lower_handle) &&
                 flight::statusbar::has_status_bar_style_entry(info_provider, upper_handle),
             "pushed style handles were not retained") ||
      !check(colors.size() == 2 && colors[0] == 20.0 && colors[1] == 30.0,
             "newest color did not win") ||
      !check(overlays.size() == 1 && overlays[0], "upper overlay did not apply") ||
      !check(styles.size() == 1 && styles[0] == String("dark"),
             "lower style did not fill an absent upper field") ||
      !check(visibility.size() == 1 && !visibility[0] &&
                 visibility_animations[0] == String("fade"),
             "visibility did not use the independently resolved animation")) {
    return 1;
  }

  flight::statusbar::pop_status_bar_style_entry(info_provider, upper_handle);
  if (!check(!flight::statusbar::has_status_bar_style_entry(info_provider, upper_handle) &&
                 colors.back() == 20.0 && overlays.back() == false,
             "pop did not restore the lower entry")) {
    return 1;
  }
  flight::statusbar::pop_status_bar_style_entry(info_provider, lower_handle);
  if (!check(!flight::statusbar::has_status_bar_style_entry(info_provider, lower_handle) &&
                 colors.back() == source_color && styles.back() == source_style &&
                 visibility.back() == source_visible,
             "empty stack did not restore its captured baseline")) {
    return 1;
  }

  const auto clear_handle = flight::statusbar::push_status_bar_style_entry(
      color_provider, info_provider, overlays_provider, style_provider, visibility_provider, lower);
  flight::statusbar::clear_status_bar_style_stack(info_provider);
  if (!check(!flight::statusbar::has_status_bar_style_entry(info_provider, clear_handle),
             "clear retained a style entry")) {
    return 1;
  }

  // THE THREE ENTRY POINTS THIS SUITE NEVER CALLED, and each carries a semantic of its own rather than
  // being a duplicate of one above. Their absence is why a whole function could regress unnoticed.
  //
  // getStatusBarHeight reads through the module's OWN scratch snapshot (the emitted body is
  // `host.getInfo(scratch_info)->height`), so the answer must come from the host it was PASSED and must
  // be re-read each call, not cached from the first.
  source_height = 20.0;
  if (!check(flight::statusbar::get_status_bar_height(info_provider) == 20.0,
             "getStatusBarHeight reads the height from the passed host")) {
    return 1;
  }
  source_height = 37.0;
  if (!check(flight::statusbar::get_status_bar_height(info_provider) == 37.0,
             "and re-reads it rather than caching the first answer")) {
    return 1;
  }

  // getStatusBarInfo fills and RETURNS THE CALLER'S OWN object -- a fresh one would satisfy a field
  // check while breaking every caller that holds the object it passed in. The stale style proves the
  // host's answer OVERWROTE what was there rather than being merged into it.
  {
    const auto out = flight::statusbar::create_status_bar_info();
    out->style = String("stale");
    source_style = String("dark");
    if (!check(flight::statusbar::get_status_bar_info(info_provider, out) == out,
               "getStatusBarInfo returns the caller's own object") ||
        !check(out->style == String("dark"),
               "and the caller's object carries what the passed host reported")) {
      return 1;
    }
  }

  // disposeStatusBar releases the subscription. It is attached HERE first, so the case is
  // self-contained whatever the attach path above left behind, and the SECOND dispose then proves the
  // release is by identity rather than by count.
  {
    flight::statusbar::attach_status_bar(change_provider, info_provider, bar);
    const int attached = unsubscribes;
    flight::statusbar::dispose_status_bar(bar);
    if (!check(unsubscribes == attached + 1,
               "disposeStatusBar releases the attached subscription")) {
      return 1;
    }
    flight::statusbar::dispose_status_bar(bar);
    if (!check(unsubscribes == attached + 1,
               "and a second dispose releases nothing, because the subscription is already gone")) {
      return 1;
    }
  }

  return 0;
}
