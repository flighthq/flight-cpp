// Re-derived from the generated movieClip module at a627849. Every function the emitter produced is carried
// verbatim; the declarations it refused are supplied below the emitted ones. See overrides/manifest.json
// (movieclip-timeline-node-construction) for what is supplied, how, and what remains.
#pragma once

#include <flight/structural_ref.hpp>
#include <functional>
#include <optional>
#include <variant>
#include <flight/runtime.hpp>
#include <flight/weak_map.hpp>

#include <flight/types/movie_clip.hpp>
#include <flight/types/timeline_source.hpp>

#include <flight/entity/entity.hpp>
#include <flight/scene2d/display_object.hpp>
#include <flight/timeline/timeline.hpp>
#include <flight/types/contract.hpp>

namespace flight::movieclip {

using flight::types::MovieClip;
using flight::types::MovieClipData;
using flight::types::MovieClipRuntime;

inline void add_movie_clip_frame_script(flight::Ref<flight::types::MovieClip> clip, std::variant<double, flight::String> frame, flight::types::FrameScript script) {
  auto data = clip->data;
  if (!data->timeline.has_value()) {
    return;
  }
  flight::timeline::add_timeline_frame_script(data->timeline.value(), frame, script);
}

inline double get_movie_clip_current_frame(flight::Ref<flight::types::MovieClip> clip) {
  auto data = clip->data;
  return ([&]() -> double { auto nullish_coalesce_left = ([&]() -> std::optional<double> { auto optional_chain_receiver = data->timeline; if (!optional_chain_receiver.has_value()) return std::nullopt; return optional_chain_receiver.value()->current_frame; }()); if (nullish_coalesce_left.has_value()) return nullish_coalesce_left.value(); return 1.0; }());
}

inline std::optional<flight::Ref<flight::types::TimelineLabel>> get_movie_clip_current_label(flight::Ref<flight::types::MovieClip> clip) {
  auto data = clip->data;
  if (!data->timeline.has_value()) {
    return std::nullopt;
  }
  return flight::timeline::get_timeline_current_label(flight::structural_ref_cast<flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::Timeline>>>>>(flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<flight::types::Timeline>>>>(data->timeline.value())));
}

inline std::optional<std::function<void(flight::types::Node2D, double)>> get_movie_clip_frame_script(flight::Ref<flight::types::MovieClip> clip, std::variant<double, flight::String> frame) {
  auto data = clip->data;
  if (!data->timeline.has_value()) {
    return std::nullopt;
  }
  return flight::timeline::get_timeline_frame_script(flight::structural_ref_cast<flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::Timeline>>>>>(flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<flight::types::Timeline>>>>(data->timeline.value())), frame);
}

inline double get_movie_clip_total_frames(flight::Ref<flight::types::MovieClip> clip) {
  auto data = clip->data;
  return ([&]() -> double { auto nullish_coalesce_left = ([&]() -> std::optional<double> { auto optional_chain_receiver = ([&]() -> std::variant<flight::Ref<flight::types::TimelineSource>, flight::Null, flight::Undefined> { auto optional_chain_receiver = data->timeline; if (!optional_chain_receiver.has_value()) return std::variant<flight::Ref<flight::types::TimelineSource>, flight::Null, flight::Undefined>{std::in_place_type<flight::Undefined>, flight::undefined}; return ([&]() -> std::variant<flight::Ref<flight::types::TimelineSource>, flight::Null, flight::Undefined> { auto optional_chain_projected = optional_chain_receiver.value()->source; if (!optional_chain_projected.has_value()) return std::variant<flight::Ref<flight::types::TimelineSource>, flight::Null, flight::Undefined>{std::in_place_type<flight::Null>, flight::null}; return std::variant<flight::Ref<flight::types::TimelineSource>, flight::Null, flight::Undefined>{std::in_place_type<flight::Ref<flight::types::TimelineSource>>, optional_chain_projected.value()}; }()); }()); if (!std::holds_alternative<flight::Ref<flight::types::TimelineSource>>(optional_chain_receiver)) return std::nullopt; return std::get<flight::Ref<flight::types::TimelineSource>>(optional_chain_receiver)->total_frames; }()); if (nullish_coalesce_left.has_value()) return nullish_coalesce_left.value(); return 1.0; }());
}

inline void goto_and_play_movie_clip(flight::Ref<flight::types::MovieClip> clip, std::variant<double, flight::String> frame) {
  auto data = clip->data;
  if (!data->timeline.has_value()) {
    return;
  }
  flight::timeline::goto_and_play_timeline(data->timeline.value(), frame);
}

inline void goto_and_stop_movie_clip(flight::Ref<flight::types::MovieClip> clip, std::variant<double, flight::String> frame) {
  auto data = clip->data;
  if (!data->timeline.has_value()) {
    return;
  }
  flight::timeline::goto_and_stop_timeline(data->timeline.value(), frame);
}

// Emitted body corrected per docs/compiler-requests/partial-row-three-state-return.md: the emitter returned the
// partial row's optional where its own lambda declared a three-state variant. `data?.timeline ?? null` keeps
// the same timeline when present and stores null when either the receiver or the member is absent.
inline void initialize_movie_clip_data(flight::types::EntityConstruction<flight::Ref<flight::types::MovieClipData>> out, std::optional<flight::StructuralRef<flight::RowReadonly<flight::RowPartial<flight::RowOf<flight::Ref<flight::types::MovieClipData>>>>>> data = std::nullopt) {
  std::optional<flight::Ref<flight::types::Timeline>> timeline = std::nullopt;
  if (data.has_value()) {
    auto projected = flight::row_get<flight::RowKey<"timeline">>(data.value());
    if (projected.has_value()) timeline = projected.value();
  }
  flight::row_set<flight::RowKey<"timeline">>(out, timeline);
}

inline flight::Ref<flight::types::MovieClipData> create_movie_clip_data(std::optional<flight::StructuralRef<flight::RowReadonly<flight::RowPartial<flight::RowOf<flight::Ref<flight::types::MovieClipData>>>>>> data = std::nullopt) {
  flight::types::EntityConstruction<flight::Ref<flight::types::MovieClipData>> out = flight::entity::allocate_entity<flight::Ref<flight::types::MovieClipData>>();
  initialize_movie_clip_data(out, data);
  return flight::entity::finish_entity<flight::Ref<flight::types::MovieClipData>>(out);
}

inline bool is_movie_clip_playing(flight::Ref<flight::types::MovieClip> clip) {
  auto data = clip->data;
  return ([&]() -> bool { auto nullish_coalesce_left = ([&]() -> std::optional<bool> { auto optional_chain_receiver = data->timeline; if (!optional_chain_receiver.has_value()) return std::nullopt; return optional_chain_receiver.value()->is_playing; }()); if (nullish_coalesce_left.has_value()) return nullish_coalesce_left.value(); return false; }());
}

inline void next_frame_movie_clip(flight::Ref<flight::types::MovieClip> clip) {
  auto data = clip->data;
  if (!data->timeline.has_value()) {
    return;
  }
  flight::timeline::next_frame_timeline(data->timeline.value());
}

inline void play_movie_clip(flight::Ref<flight::types::MovieClip> clip) {
  auto data = clip->data;
  if (!data->timeline.has_value()) {
    return;
  }
  flight::timeline::play_timeline(data->timeline.value());
}

inline void prev_frame_movie_clip(flight::Ref<flight::types::MovieClip> clip) {
  auto data = clip->data;
  if (!data->timeline.has_value()) {
    return;
  }
  flight::timeline::prev_frame_timeline(data->timeline.value());
}

inline void remove_movie_clip_frame_script(flight::Ref<flight::types::MovieClip> clip, std::variant<double, flight::String> frame) {
  auto data = clip->data;
  if (!data->timeline.has_value()) {
    return;
  }
  flight::timeline::remove_timeline_frame_script(data->timeline.value(), frame);
}

inline void stop_movie_clip(flight::Ref<flight::types::MovieClip> clip) {
  auto data = clip->data;
  if (!data->timeline.has_value()) {
    return;
  }
  flight::timeline::stop_timeline(data->timeline.value());
}

inline void update_movie_clip(flight::Ref<flight::types::MovieClip> clip, double delta_time) {
  auto data = clip->data;
  if (!data->timeline.has_value()) {
    return;
  }
  flight::timeline::update_timeline(data->timeline.value(), delta_time);
}

// Supplied: the refused declarations.

// createMovieClip without options. The emitted MovieClip owner flattens the Node2D intersection, and
// createNode2D's data and runtime factory slots are typed for Node2DData and Node2DRuntime, so the source's
// call through createNode2D has no C++ form. This builds the same defaults initializeNode and the six trait
// initializers write, with the clip's own MovieClipData and the Node2D runtime the emitted slot can hold.
// Options are deliberately not accepted: a call that passes them fails to compile rather than losing them.
inline flight::Ref<flight::types::MovieClip> create_movie_clip() {
  return flight::make_ref<flight::types::MovieClip>(flight::types::MovieClip{
      .data = create_movie_clip_data(),
      .enabled = true,
      .kind = flight::types::movie_clip_kind,
      .name = std::nullopt,
      .entity_runtime_key = flight::scene2d::create_node_2d_runtime(),
      .alpha = 1.0,
      .visible = true,
      .blend_mode = std::nullopt,
      .clip = std::nullopt,
      .material = std::nullopt,
      .material_data = std::nullopt,
      .pivot_x = 0.0,
      .pivot_y = 0.0,
      .rotation = 0.0,
      .scale_x = 1.0,
      .scale_y = 1.0,
      .skew_x = 0.0,
      .skew_y = 0.0,
      .x = 0.0,
      .y = 0.0,
  });
}

// The clip's MovieClipSignals group, keyed by clip identity. The emitted runtime slot is typed as the Node2D
// runtime, which has no movieClipSignals cell, so the group the source keeps there lives here instead. Keys
// are weak: the table never keeps a clip alive, and an entry is dropped once its clip is gone.
inline const flight::WeakMap<flight::Ref<void>, flight::Ref<flight::types::TimelineSignals>> movie_clip_signals_slots;

inline std::optional<flight::Ref<flight::types::TimelineSignals>> get_movie_clip_signals(
    flight::Ref<flight::types::MovieClip> clip) {
  return movie_clip_signals_slots.get(clip);
}

inline flight::Ref<flight::types::TimelineSignals> enable_movie_clip_signals(flight::Ref<flight::types::MovieClip> clip) {
  if (auto existing = movie_clip_signals_slots.get(clip)) return existing.value();
  auto data = clip->data;
  if (!data->timeline.has_value()) data->timeline = flight::timeline::create_timeline();
  auto signals = flight::timeline::enable_timeline_signals(data->timeline.value());
  movie_clip_signals_slots.set(clip, signals);
  return signals;
}

inline void dispose_movie_clip_signals(flight::Ref<flight::types::MovieClip> clip) {
  auto data = clip->data;
  if (data->timeline.has_value()) flight::timeline::dispose_timeline_signals(data->timeline.value());
  (void)movie_clip_signals_slots.erase(clip);
}

// setMovieClipSource: the emitter refused `timeline.target = clip` because the clip is not stored in the
// target's declared Node2D arm. The same clip identity is viewed through that arm instead.
inline void set_movie_clip_source(flight::Ref<flight::types::MovieClip> clip,
                                  flight::Ref<flight::types::TimelineSource> source) {
  auto data = clip->data;
  auto timeline = data->timeline.has_value() ? data->timeline.value() : flight::timeline::create_timeline();
  timeline->source = std::move(source);
  timeline->target = flight::structural_ref_cast<flight::types::Node2D>(clip);
  data->timeline = timeline;
  flight::timeline::goto_and_stop_timeline(timeline, timeline->current_frame);
}

} // namespace flight::movieclip
