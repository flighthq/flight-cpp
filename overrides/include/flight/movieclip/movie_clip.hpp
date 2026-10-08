// Qualified timeline bridge and Node2D construction for the generated partial movieClip module.
#pragma once

#include <optional>
#include <variant>
#include <flight/runtime.hpp>
#include <flight/scene2d/display_object.hpp>
#include <flight/timeline/timeline.hpp>
#include <flight/types/movie_clip.hpp>
#include <flight/types/timeline_source.hpp>

namespace flight::movieclip {

inline flight::Ref<flight::types::MovieClipData> create_movie_clip_data() {
  return flight::make_ref<flight::types::MovieClipData>(flight::types::MovieClipData{
      .entity_runtime_key = std::nullopt,
      .timeline = std::nullopt,
  });
}

template <typename Options>
inline flight::Ref<flight::types::MovieClipData> create_movie_clip_data(Options&&) {
  return create_movie_clip_data();
}

inline flight::Ref<flight::types::MovieClip> make_movie_clip() {
  return flight::make_ref<flight::types::MovieClip>(flight::types::MovieClip{
      .data = create_movie_clip_data(),
      .enabled = true,
      .kind = flight::types::movie_clip_kind,
      .name = std::nullopt,
      .entity_runtime_key = flight::scene2d::create_node2_d_runtime(),
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

template <typename Options>
inline flight::Ref<flight::types::MovieClip> create_movie_clip(Options&&) {
  return make_movie_clip();
}

inline flight::Ref<flight::types::MovieClip> create_movie_clip() {
  return make_movie_clip();
}

inline flight::Ref<flight::types::TimelineSource> create_timeline_source(
    double total_frames = 1.0, std::optional<double> frame_rate = std::nullopt) {
  return flight::make_ref<flight::types::TimelineSource>(flight::types::TimelineSource{
      .entity_runtime_key = std::nullopt,
      .total_frames = total_frames,
      .labels = {},
      .cues = {},
      .frame_rate = frame_rate,
      .construct_frame = [](flight::types::Node2D, double) {},
  });
}

inline void set_movie_clip_source(flight::Ref<flight::types::MovieClip> clip,
                                  flight::Ref<flight::types::TimelineSource> source) {
  auto timeline = clip->data->timeline.has_value()
                      ? clip->data->timeline.value()
                      : flight::timeline::create_timeline(std::nullopt);
  timeline->source = std::move(source);
  timeline->target = flight::structural_ref_cast<flight::types::Node2D>(clip);
  clip->data->timeline = timeline;
  flight::timeline::goto_and_stop_timeline(timeline, timeline->current_frame);
}

inline void add_movie_clip_frame_script(flight::Ref<flight::types::MovieClip> clip,
                                        std::variant<double, flight::String> frame,
                                        flight::types::FrameScript script) {
  if (clip->data->timeline.has_value()) {
    flight::timeline::add_timeline_frame_script(clip->data->timeline.value(), std::move(frame),
                                                std::move(script));
  }
}

inline void remove_movie_clip_frame_script(flight::Ref<flight::types::MovieClip> clip,
                                           std::variant<double, flight::String> frame) {
  if (clip->data->timeline.has_value()) {
    flight::timeline::remove_timeline_frame_script(clip->data->timeline.value(),
                                                   std::move(frame));
  }
}

inline double get_movie_clip_current_frame(flight::Ref<flight::types::MovieClip> clip) {
  return clip->data->timeline.has_value() ? clip->data->timeline.value()->current_frame : 1.0;
}

inline double get_movie_clip_total_frames(flight::Ref<flight::types::MovieClip> clip) {
  if (!clip->data->timeline.has_value() ||
      !clip->data->timeline.value()->source.has_value()) {
    return 1.0;
  }
  return clip->data->timeline.value()->source.value()->total_frames;
}

inline bool is_movie_clip_playing(flight::Ref<flight::types::MovieClip> clip) {
  return clip->data->timeline.has_value() && clip->data->timeline.value()->is_playing;
}

inline void play_movie_clip(flight::Ref<flight::types::MovieClip> clip) {
  if (clip->data->timeline.has_value())
    flight::timeline::play_timeline(clip->data->timeline.value());
}

inline void stop_movie_clip(flight::Ref<flight::types::MovieClip> clip) {
  if (clip->data->timeline.has_value())
    flight::timeline::stop_timeline(clip->data->timeline.value());
}

inline void update_movie_clip(flight::Ref<flight::types::MovieClip> clip, double delta_time) {
  if (clip->data->timeline.has_value())
    flight::timeline::update_timeline(clip->data->timeline.value(), delta_time);
}

inline void goto_and_play_movie_clip(flight::Ref<flight::types::MovieClip> clip,
                                     std::variant<double, flight::String> frame) {
  if (clip->data->timeline.has_value())
    flight::timeline::goto_and_play_timeline(clip->data->timeline.value(), std::move(frame));
}

inline void goto_and_stop_movie_clip(flight::Ref<flight::types::MovieClip> clip,
                                     std::variant<double, flight::String> frame) {
  if (clip->data->timeline.has_value())
    flight::timeline::goto_and_stop_timeline(clip->data->timeline.value(), std::move(frame));
}

inline void next_frame_movie_clip(flight::Ref<flight::types::MovieClip> clip) {
  if (clip->data->timeline.has_value())
    flight::timeline::next_frame_timeline(clip->data->timeline.value());
}

inline void prev_frame_movie_clip(flight::Ref<flight::types::MovieClip> clip) {
  if (clip->data->timeline.has_value())
    flight::timeline::prev_frame_timeline(clip->data->timeline.value());
}

} // namespace flight::movieclip
