// The generated @flighthq/movieclip package at a627849, exercised natively through its re-derived override.
// Expectations are ported from packages/movieclip/src/movieClip.test.ts: construction defaults, the
// null-timeline no-ops and idle defaults, binding a source (which targets the clip and realizes frame 1),
// timeline reuse, playback and seeking, labels, the frame-script registry and its firing on the clip, the
// constructFrame callback, and the clip's signal group (idempotence, re-arming, disposal).
//
// A MovieClip is a real Node2D target here, so constructFrame and frame scripts fire, unlike the timeline
// package's own native test, which can only drive a null target. createMovieClip options and the two runtime
// accessors are not supplied by the override and so are not exercised; see overrides/manifest.json.
#include <flight/movieclip/movie_clip.hpp>
#include <flight/signals/slot.hpp>
#include <flight/timeline/timeline.hpp>

#include <iostream>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}

namespace movieclip = flight::movieclip;
namespace timeline = flight::timeline;
using flight::types::MovieClip;
using flight::types::Node2D;
using SourceOptions = timeline::total_frames_frame_rate_labels_cues_construct_frame_a8754a82464a8856;
using Frame = std::variant<double, flight::String>;

struct Label {
  const char* name;
  double frame;
};

// createTimelineSource with the upstream test's fields. frameRate null steps one frame per update.
flight::Ref<flight::types::TimelineSource> source(double totalFrames, bool steppedPerUpdate = false,
                                                  std::vector<double>* constructed = nullptr,
                                                  std::vector<Label> labels = {}) {
  auto options = flight::make_ref<SourceOptions>();
  options->total_frames = totalFrames;
  if (steppedPerUpdate) options->frame_rate = std::variant<double, flight::Null, flight::Undefined>{std::in_place_type<flight::Null>, flight::null};
  if (constructed != nullptr) {
    options->construct_frame = std::function<void(Node2D, double)>([constructed](Node2D, double frame) { constructed->push_back(frame); });
  }
  if (!labels.empty()) {
    flight::Array<flight::Ref<flight::types::TimelineLabel>> list;
    for (const auto& label : labels) {
      auto entry = flight::make_ref<flight::types::TimelineLabel>();
      entry->name = flight::String(label.name);
      entry->frame = label.frame;
      list.push(entry);
    }
    options->labels = list;
  }
  return timeline::create_timeline_source(options);
}

// createTimeline({ source, target: clip }).
flight::Ref<flight::types::Timeline> attach(const flight::Ref<MovieClip>& clip, flight::Ref<flight::types::TimelineSource> src) {
  auto t = timeline::create_timeline();
  t->source = std::move(src);
  t->target = flight::structural_ref_cast<Node2D>(clip);
  clip->data->timeline = t;
  return t;
}

void construction() {
  auto clip = movieclip::create_movie_clip();
  check(!clip->data->timeline.has_value(), "a new clip has no timeline");
  check(clip->kind == flight::types::movie_clip_kind, "a new clip has the MovieClip kind");
  check(movieclip::create_movie_clip() != clip, "each clip is a new object");
  check(!movieclip::create_movie_clip_data()->timeline.has_value(), "createMovieClipData defaults to a null timeline");
}

void idle_defaults() {
  auto clip = movieclip::create_movie_clip();
  check(movieclip::get_movie_clip_current_frame(clip) == 1, "no timeline: current frame 1");
  check(movieclip::get_movie_clip_total_frames(clip) == 1, "no timeline: total frames 1");
  check(!movieclip::is_movie_clip_playing(clip), "no timeline: not playing");
  check(!movieclip::get_movie_clip_current_label(clip).has_value(), "no timeline: no label");
  check(!movieclip::get_movie_clip_frame_script(clip, Frame{1.0}).has_value(), "no timeline: no script");
  movieclip::add_movie_clip_frame_script(clip, Frame{1.0}, [](Node2D, double) {});
  movieclip::remove_movie_clip_frame_script(clip, Frame{1.0});
  movieclip::play_movie_clip(clip);
  movieclip::stop_movie_clip(clip);
  movieclip::next_frame_movie_clip(clip);
  movieclip::prev_frame_movie_clip(clip);
  movieclip::goto_and_play_movie_clip(clip, Frame{2.0});
  movieclip::goto_and_stop_movie_clip(clip, Frame{2.0});
  movieclip::update_movie_clip(clip, 16);
  check(!clip->data->timeline.has_value(), "no-timeline controls are no-ops and create nothing");
}

void binding() {
  {
    std::vector<double> constructed;
    auto clip = movieclip::create_movie_clip();
    movieclip::set_movie_clip_source(clip, source(4, false, &constructed));
    check(movieclip::get_movie_clip_total_frames(clip) == 4, "setMovieClipSource binds the source");
    auto target = clip->data->timeline.value()->target;
    check(target.has_value() && target.value() == flight::structural_ref_cast<Node2D>(clip), "the timeline targets the clip itself");
    check(constructed == std::vector<double>{1}, "binding realizes frame 1 once");
  }
  {
    auto clip = movieclip::create_movie_clip();
    auto existing = timeline::create_timeline();
    existing->current_frame = 2;
    clip->data->timeline = existing;
    movieclip::set_movie_clip_source(clip, source(5));
    check(clip->data->timeline.value() == existing && existing->source.value()->total_frames == 5, "setMovieClipSource reuses the clip's timeline");
  }
}

void playback() {
  {
    auto clip = movieclip::create_movie_clip();
    auto t = attach(clip, source(5));
    t->current_frame = 3;
    check(movieclip::get_movie_clip_current_frame(clip) == 3, "current frame reads the timeline");
  }
  {
    auto clip = movieclip::create_movie_clip();
    auto t = attach(clip, source(5));
    movieclip::goto_and_play_movie_clip(clip, Frame{3.0});
    check(t->current_frame == 3 && t->is_playing && movieclip::is_movie_clip_playing(clip), "gotoAndPlay seeks and plays");
    movieclip::goto_and_stop_movie_clip(clip, Frame{2.0});
    check(t->current_frame == 2 && !t->is_playing, "gotoAndStop seeks and stops");
    movieclip::next_frame_movie_clip(clip);
    check(t->current_frame == 3, "nextFrame advances one");
    movieclip::prev_frame_movie_clip(clip);
    check(t->current_frame == 2, "prevFrame retreats one");
    movieclip::play_movie_clip(clip);
    check(t->is_playing, "play starts the timeline");
    movieclip::stop_movie_clip(clip);
    check(!t->is_playing, "stop halts the timeline");
  }
  {
    std::vector<double> constructed;
    auto clip = movieclip::create_movie_clip();
    attach(clip, source(3, true, &constructed));
    timeline::play_timeline(clip->data->timeline.value());
    movieclip::update_movie_clip(clip, 16);
    movieclip::update_movie_clip(clip, 16);
    check(constructed == std::vector<double>{1, 2}, "update advances a playing clip and constructs each frame");
  }
  {
    std::vector<double> constructed;
    auto clip = movieclip::create_movie_clip();
    attach(clip, source(3, false, &constructed));
    movieclip::update_movie_clip(clip, 0);
    check(constructed == std::vector<double>{1}, "the first update constructs frame 1 even when stopped");
  }
  {
    auto clip = movieclip::create_movie_clip();
    movieclip::set_movie_clip_source(clip, source(5, false, nullptr, {{"idle", 1}, {"run", 3}}));
    movieclip::goto_and_stop_movie_clip(clip, Frame{3.0});
    auto label = movieclip::get_movie_clip_current_label(clip);
    check(label.has_value() && label.value()->name == flight::String("run"), "current label at the current frame");
    movieclip::goto_and_stop_movie_clip(clip, Frame{flight::String("idle")});
    check(movieclip::get_movie_clip_current_frame(clip) == 1, "a label name seeks the clip");
  }
}

void scripts() {
  {
    std::vector<double> fired;
    auto clip = movieclip::create_movie_clip();
    attach(clip, source(3, true));
    movieclip::add_movie_clip_frame_script(clip, Frame{2.0}, [&](Node2D target, double frame) {
      if (target == flight::structural_ref_cast<Node2D>(clip)) fired.push_back(frame);
    });
    movieclip::play_movie_clip(clip);
    movieclip::update_movie_clip(clip, 0);
    movieclip::update_movie_clip(clip, 0);
    check(fired == std::vector<double>{2}, "a frame script fires on its frame with the clip as target");
  }
  {
    std::vector<double> fired;
    auto clip = movieclip::create_movie_clip();
    attach(clip, source(3, true));
    movieclip::add_movie_clip_frame_script(clip, Frame{2.0}, [&](Node2D, double frame) { fired.push_back(frame); });
    movieclip::remove_movie_clip_frame_script(clip, Frame{2.0});
    movieclip::play_movie_clip(clip);
    movieclip::update_movie_clip(clip, 0);
    movieclip::update_movie_clip(clip, 0);
    check(fired.empty(), "a removed frame script does not fire");
  }
  {
    auto clip = movieclip::create_movie_clip();
    attach(clip, source(3));
    check(!movieclip::get_movie_clip_frame_script(clip, Frame{1.0}).has_value(), "no script on an unscripted frame");
    movieclip::add_movie_clip_frame_script(clip, Frame{2.0}, [](Node2D, double) {});
    check(movieclip::get_movie_clip_frame_script(clip, Frame{2.0}).has_value(), "the attached script is retrievable");
  }
}

void signals() {
  {
    auto clip = movieclip::create_movie_clip();
    check(!movieclip::get_movie_clip_signals(clip).has_value(), "no signals before enable");
    movieclip::dispose_movie_clip_signals(clip);
    auto early = movieclip::enable_movie_clip_signals(clip);
    check(early != nullptr && clip->data->timeline.has_value(), "enable works before a source and creates the timeline");
  }
  {
    auto clip = movieclip::create_movie_clip();
    movieclip::set_movie_clip_source(clip, source(3));
    auto group = movieclip::enable_movie_clip_signals(clip);
    check(movieclip::enable_movie_clip_signals(clip) == group, "enable is idempotent");
    auto stored = movieclip::get_movie_clip_signals(clip);
    check(stored.has_value() && stored.value() == group, "get returns the enabled group");
    check(clip->data->timeline.value()->signals.has_value() && clip->data->timeline.value()->signals.value() == group,
          "the clip's group is the timeline's group");
    check(group->on_enter_frame && group->on_exit_frame && group->on_frame_constructed && group->on_complete && group->on_loop,
          "every lifecycle signal is defined");

    movieclip::dispose_movie_clip_signals(clip);
    check(!movieclip::get_movie_clip_signals(clip).has_value(), "dispose clears the clip's group");
    check(!clip->data->timeline.value()->signals.has_value(), "dispose clears the timeline's signals");
    auto again = movieclip::enable_movie_clip_signals(clip);
    check(again != group && movieclip::get_movie_clip_signals(clip).value() == again, "enable re-arms with a new group");
  }
  {
    std::vector<double> frames;
    auto clip = movieclip::create_movie_clip();
    movieclip::set_movie_clip_source(clip, source(3, true));
    flight::signals::connect_signal(
        movieclip::enable_movie_clip_signals(clip)->on_enter_frame,
        std::function<void(flight::Ref<flight::types::TimelineFrameEvent>)>(
            [&](flight::Ref<flight::types::TimelineFrameEvent> event) { frames.push_back(event->frame); }),
        std::nullopt);
    movieclip::play_movie_clip(clip);
    movieclip::update_movie_clip(clip, 0);
    movieclip::update_movie_clip(clip, 0);
    check(frames == std::vector<double>{2}, "onEnterFrame fires when the clip's frame changes");
  }
  {
    auto a = movieclip::create_movie_clip(), b = movieclip::create_movie_clip();
    auto group = movieclip::enable_movie_clip_signals(a);
    check(!movieclip::get_movie_clip_signals(b).has_value() && movieclip::get_movie_clip_signals(a).value() == group,
          "signal groups are per clip");
  }
}

}  // namespace

int main() {
  construction();
  idle_defaults();
  binding();
  playback();
  scripts();
  signals();
  return failures == 0 ? 0 : 1;
}
