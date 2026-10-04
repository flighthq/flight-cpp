#pragma once

#include <cstddef>
#include <functional>
#include <optional>

#include <flight/entity/entity.hpp>
#include <flight/number.hpp>
#include <flight/runtime.hpp>
#include <flight/signals/emitter.hpp>
#include <flight/signals/signal.hpp>
#include <flight/types/status_bar.hpp>
#include <flight/weak_map.hpp>

namespace flight::statusbar {

using StatusBarInfoSignal =
    std::function<void(flight::StructuralRef<flight::RowReadonly<
        flight::RowOf<flight::Ref<flight::types::StatusBarInfo>>>>)>;

struct StyleStackEntry final : public flight::ReferenceEnabled {
  flight::Ref<flight::types::StatusBarStyleEntry> entry;
  flight::types::StatusBarStyleEntryHandle handle;
};

struct StyleStackState final : public flight::ReferenceEnabled {
  flight::Ref<flight::types::StatusBarInfo> applied;
  flight::Ref<flight::types::StatusBarInfo> baseline;
  flight::Ref<flight::types::HostStatusBarColorCapability> color_provider;
  flight::Array<flight::Ref<StyleStackEntry>> entries;
  flight::Ref<flight::types::HostStatusBarOverlaysCapability> overlays_provider;
  flight::Ref<flight::types::HostStatusBarStyleCapability> style_provider;
  flight::Ref<flight::types::HostStatusBarVisibilityCapability> visibility_provider;
};

inline constexpr flight::types::StatusBarStyleEntryHandle invalid_handle = -1.0;
inline flight::types::StatusBarStyleEntryHandle next_handle = 1.0;

inline flight::WeakMap<flight::Ref<flight::types::HostStatusBarInfoCapability>,
                       flight::Ref<StyleStackState>>
    style_stacks;
inline flight::WeakMap<flight::Ref<flight::types::StatusBar>, std::function<void()>>
    subscriptions;

inline void initialize_status_bar(
    flight::types::EntityConstruction<flight::Ref<flight::types::StatusBar>> out) {
  out->on_change = flight::signals::create_signal<StatusBarInfoSignal>();
}

[[nodiscard]] inline flight::Ref<flight::types::StatusBar> create_status_bar() {
  auto out = flight::entity::allocate_entity<flight::Ref<flight::types::StatusBar>>();
  initialize_status_bar(out);
  return flight::entity::finish_entity(out);
}

inline void initialize_status_bar_info(
    flight::types::EntityConstruction<flight::Ref<flight::types::StatusBarInfo>> out) {
  out->color = 0.0;
  out->height = -1.0;
  out->overlays_content = false;
  out->style = flight::String("default");
  out->visible = true;
}

[[nodiscard]] inline flight::Ref<flight::types::StatusBarInfo> create_status_bar_info() {
  auto out = flight::entity::allocate_entity<flight::Ref<flight::types::StatusBarInfo>>();
  initialize_status_bar_info(out);
  return flight::entity::finish_entity(out);
}

inline flight::Ref<flight::types::StatusBarInfo> scratch_info = create_status_bar_info();

[[nodiscard]] inline flight::Ref<flight::types::StatusBarInfo> get_status_bar_info(
    flight::Ref<flight::types::HostStatusBarInfoCapability> host_status_bar_info,
    flight::Ref<flight::types::StatusBarInfo> out) {
  return host_status_bar_info->get_info(out);
}

[[nodiscard]] inline double get_status_bar_height(
    flight::Ref<flight::types::HostStatusBarInfoCapability> host_status_bar_info) {
  return host_status_bar_info->get_info(scratch_info)->height;
}

[[nodiscard]] inline flight::String packed_rgba_to_hex_color(double color) {
  const auto rgb = flight::bitwise_and(flight::unsigned_right_shift(color, 8.0), 16777215.0);
  return flight::String("#") +
         flight::number_to_string(rgb, 16.0).pad_start(6.0, flight::String("0"));
}

inline void set_status_bar_color(
    flight::Ref<flight::types::HostStatusBarColorCapability> host_status_bar_color,
    double color,
    std::optional<bool> animated = std::nullopt) {
  host_status_bar_color->set_background_color(color, animated);
}

inline void set_status_bar_overlays_content(
    flight::Ref<flight::types::HostStatusBarOverlaysCapability> host_status_bar_overlays,
    bool overlay) {
  host_status_bar_overlays->set_overlays_content(overlay);
}

inline void set_status_bar_style(
    flight::Ref<flight::types::HostStatusBarStyleCapability> host_status_bar_style,
    flight::types::StatusBarStyle style) {
  host_status_bar_style->set_style(std::move(style));
}

inline void set_status_bar_visible(
    flight::Ref<flight::types::HostStatusBarVisibilityCapability> host_status_bar_visibility,
    bool visible,
    std::optional<flight::types::StatusBarAnimation> animation = std::nullopt) {
  host_status_bar_visibility->set_visible(visible, std::move(animation));
}

inline void detach_status_bar(flight::Ref<flight::types::StatusBar> bar) {
  auto unsubscribe = subscriptions.get(bar);
  if (!unsubscribe.has_value()) return;
  unsubscribe.value()();
  static_cast<void>(subscriptions.erase(bar));
}

inline void attach_status_bar(
    flight::Ref<flight::types::HostStatusBarChangeCapability> host_status_bar_change,
    flight::Ref<flight::types::HostStatusBarInfoCapability> host_status_bar_info,
    flight::Ref<flight::types::StatusBar> bar) {
  detach_status_bar(bar);
  auto unsubscribe = host_status_bar_change->subscribe([bar, host_status_bar_info]() {
    flight::signals::emit_signal(
        bar->on_change, host_status_bar_info->get_info(create_status_bar_info()));
  });
  subscriptions.set(bar, std::move(unsubscribe));
}

inline void dispose_status_bar(flight::Ref<flight::types::StatusBar> bar) {
  detach_status_bar(std::move(bar));
}

inline void apply_top_style_entry(
    flight::Ref<flight::types::HostStatusBarInfoCapability> host_status_bar_info,
    flight::Ref<StyleStackState> state) {
  std::optional<flight::types::StatusBarAnimation> animation;
  std::optional<double> color;
  std::optional<bool> overlays_content;
  std::optional<flight::types::StatusBarStyle> style;
  std::optional<bool> visible;

  for (std::size_t index = state->entries.size(); index > 0; --index) {
    const auto& entry = state->entries[index - 1]->entry;
    if (!animation.has_value() && entry->animation.has_value()) animation = entry->animation;
    if (!color.has_value() && entry->color.has_value()) color = entry->color;
    if (!overlays_content.has_value() && entry->overlays_content.has_value()) {
      overlays_content = entry->overlays_content;
    }
    if (!style.has_value() && entry->style.has_value()) style = entry->style;
    if (!visible.has_value() && entry->visible.has_value()) visible = entry->visible;
  }

  color = color.value_or(state->baseline->color);
  overlays_content = overlays_content.value_or(state->baseline->overlays_content);
  style = style.value_or(state->baseline->style);
  visible = visible.value_or(state->baseline->visible);

  if (color.value() != state->applied->color) {
    state->color_provider->set_background_color(color.value(), false);
  }
  if (overlays_content.value() != state->applied->overlays_content) {
    state->overlays_provider->set_overlays_content(overlays_content.value());
  }
  if (style.value() != state->applied->style) {
    state->style_provider->set_style(style.value());
  }
  if (visible.value() != state->applied->visible) {
    state->visibility_provider->set_visible(
        visible.value(), animation.value_or(flight::String("none")));
  }

  state->applied->color = color.value();
  state->applied->overlays_content = overlays_content.value();
  state->applied->style = style.value();
  state->applied->visible = visible.value();
  if (state->entries.empty()) static_cast<void>(style_stacks.erase(host_status_bar_info));
}

inline void clear_status_bar_style_stack(
    flight::Ref<flight::types::HostStatusBarInfoCapability> host_status_bar_info) {
  auto state = style_stacks.get(host_status_bar_info);
  if (!state.has_value() || state.value()->entries.empty()) return;
  state.value()->entries.clear();
  apply_top_style_entry(host_status_bar_info, state.value());
}

[[nodiscard]] inline bool has_status_bar_style_entry(
    flight::Ref<flight::types::HostStatusBarInfoCapability> host_status_bar_info,
    flight::types::StatusBarStyleEntryHandle handle) {
  if (handle == invalid_handle) return false;
  auto state = style_stacks.get(host_status_bar_info);
  if (!state.has_value()) return false;
  return state.value()->entries.some(
      [handle](const flight::Ref<StyleStackEntry>& entry) { return entry->handle == handle; });
}

inline void pop_status_bar_style_entry(
    flight::Ref<flight::types::HostStatusBarInfoCapability> host_status_bar_info,
    flight::types::StatusBarStyleEntryHandle handle) {
  if (handle == invalid_handle) return;
  auto state = style_stacks.get(host_status_bar_info);
  if (!state.has_value()) return;
  const auto index = state.value()->entries.find_index(
      [handle](const flight::Ref<StyleStackEntry>& entry) { return entry->handle == handle; });
  if (index == -1.0) return;
  static_cast<void>(state.value()->entries.splice(index, 1.0));
  apply_top_style_entry(host_status_bar_info, state.value());
}

[[nodiscard]] inline flight::types::StatusBarStyleEntryHandle push_status_bar_style_entry(
    flight::Ref<flight::types::HostStatusBarColorCapability> host_status_bar_color,
    flight::Ref<flight::types::HostStatusBarInfoCapability> host_status_bar_info,
    flight::Ref<flight::types::HostStatusBarOverlaysCapability> host_status_bar_overlays,
    flight::Ref<flight::types::HostStatusBarStyleCapability> host_status_bar_style,
    flight::Ref<flight::types::HostStatusBarVisibilityCapability> host_status_bar_visibility,
    flight::Ref<flight::types::StatusBarStyleEntry> entry) {
  auto state = style_stacks.get(host_status_bar_info);
  if (!state.has_value()) {
    const auto baseline = host_status_bar_info->get_info(create_status_bar_info());
    const auto applied = flight::make_ref<flight::types::StatusBarInfo>(
        flight::types::StatusBarInfo{
            .entity_runtime_key = std::nullopt,
            .color = baseline->color,
            .height = baseline->height,
            .overlays_content = baseline->overlays_content,
            .style = baseline->style,
            .visible = baseline->visible,
        });
    state = flight::make_ref<StyleStackState>(StyleStackState{
        .applied = applied,
        .baseline = baseline,
        .color_provider = host_status_bar_color,
        .entries = {},
        .overlays_provider = host_status_bar_overlays,
        .style_provider = host_status_bar_style,
        .visibility_provider = host_status_bar_visibility,
    });
    style_stacks.set(host_status_bar_info, state.value());
  }

  const auto handle = next_handle++;
  state.value()->entries.push(flight::make_ref<StyleStackEntry>(StyleStackEntry{
      .entry = std::move(entry),
      .handle = handle,
  }));
  apply_top_style_entry(host_status_bar_info, state.value());
  return handle;
}

} // namespace flight::statusbar
