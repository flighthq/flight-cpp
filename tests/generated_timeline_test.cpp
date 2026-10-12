// The generated @flighthq/timeline package at a627849, exercised natively. Expectations are ported from
// packages/timeline/src/timeline.test.ts: defaults, labels, seeking and clamping, play/stop, the update
// return value and frame accounting (frameRate-null stepping, timed advance, multi-frame skip, loop and
// once), the lifecycle signals, and the frame-script registry.
//
// Realized frames are observed through onEnterFrame, which fires for every realized frame. The upstream
// tests also observe constructFrame and frame-script invocation, which need a Node2D target; the timeline
// itself only forwards the target, so those two calls are not covered here.
#include <flight/signals/slot.hpp>
#include <flight/timeline/timeline.hpp>

#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}

namespace timeline = flight::timeline;
using flight::types::Timeline;
using SourceOptions = timeline::total_frames_frame_rate_labels_cues_construct_frame_a8754a82464a8856;
using Frame = std::variant<double, flight::String>;

struct Label {
  const char* name;
  double frame;
};

// The upstream fixture: totalFrames 4 and frameRate 10 unless given; null frameRate steps per update.
flight::Ref<Timeline> make(double totalFrames = 4.0, std::optional<double> frameRate = 10.0, std::vector<Label> labels = {}) {
  auto options = flight::make_ref<SourceOptions>();
  options->total_frames = totalFrames;
  options->frame_rate = frameRate.has_value()
                            ? std::variant<double, flight::Null, flight::Undefined>{std::in_place_type<double>, *frameRate}
                            : std::variant<double, flight::Null, flight::Undefined>{std::in_place_type<flight::Null>, flight::null};
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
  auto t = timeline::create_timeline();
  t->source = timeline::create_timeline_source(options);
  return t;
}

template <typename Pointer>
auto view(const Pointer& value) {
  using Element = typename Pointer::element_type;
  return flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<Element>>>>(value);
}

// Every realized frame, in order, as onEnterFrame reports it.
std::vector<double>& record(const flight::Ref<Timeline>& t) {
  static std::vector<std::vector<double>> store;
  store.emplace_back();
  auto& frames = store.back();
  auto signals = timeline::enable_timeline_signals(t);
  flight::signals::connect_signal(
      signals->on_enter_frame,
      std::function<void(flight::Ref<flight::types::TimelineFrameEvent>)>(
          [&frames](flight::Ref<flight::types::TimelineFrameEvent> event) { frames.push_back(event->frame); }),
      std::nullopt);
  return frames;
}

void defaults() {
  auto t = timeline::create_timeline();
  check(t->current_frame == 1.0 && !t->is_playing && t->last_frame_update == -1.0, "starts at frame 1, stopped, lastFrameUpdate -1");
  check(t->play_mode == flight::String("loop"), "playMode defaults to loop");
  check(!t->frame_scripts.has_value() && !t->signals.has_value(), "frameScripts and signals default to null");
  auto source = timeline::create_timeline_source(flight::make_ref<SourceOptions>());
  check(source->total_frames == 1.0 && !source->frame_rate.has_value() && source->labels.size() == 0, "source defaults");
}

void labels() {
  auto t = make(10.0, 10.0, {{"intro", 1.0}, {"run", 3.0}, {"jump", 7.0}});
  auto run = timeline::find_timeline_label(view(t), flight::String("run"));
  check(run.has_value() && run.value()->frame == 3.0, "find returns the matching label");
  check(!timeline::find_timeline_label(view(t), flight::String("missing")).has_value(), "find returns null for an unknown name");
  t->current_frame = 5.0;
  auto current = timeline::get_timeline_current_label(view(t));
  check(current.has_value() && current.value()->name == flight::String("run"), "current label is the last at or before the frame");
  t->current_frame = 7.0;
  check(timeline::get_timeline_current_label(view(t)).value()->name == flight::String("jump"), "current label at exactly its frame");
  check(!timeline::get_timeline_current_label(view(make())).has_value(), "no labels: no current label");
}

void seeking() {
  auto t = make(10.0);
  timeline::goto_and_play_timeline(t, Frame{5.0});
  check(t->current_frame == 5.0 && t->is_playing, "gotoAndPlay seeks and plays");
  timeline::goto_and_stop_timeline(t, Frame{7.0});
  check(t->current_frame == 7.0 && !t->is_playing, "gotoAndStop seeks and stops");
  timeline::goto_and_stop_timeline(t, Frame{99.0});
  check(t->current_frame == 10.0, "seek clamps high");
  timeline::goto_and_stop_timeline(t, Frame{-3.0});
  check(t->current_frame == 1.0, "seek clamps low");

  auto labelled = make(10.0, 10.0, {{"run", 3.0}});
  timeline::goto_and_stop_timeline(labelled, Frame{flight::String("run")});
  check(labelled->current_frame == 3.0, "a label name resolves to its frame");
  bool threw = false;
  try {
    timeline::goto_and_play_timeline(labelled, Frame{flight::String("nope")});
  } catch (...) {
    threw = true;
  }
  check(threw, "an unknown label throws");

  auto stepping = make(3.0);
  stepping->is_playing = true;
  timeline::next_frame_timeline(stepping);
  check(stepping->current_frame == 2.0 && !stepping->is_playing, "nextFrame advances one and stops");
  timeline::next_frame_timeline(stepping);
  timeline::next_frame_timeline(stepping);
  check(stepping->current_frame == 3.0, "nextFrame clamps at the last frame");
  timeline::prev_frame_timeline(stepping);
  check(stepping->current_frame == 2.0, "prevFrame retreats one");
  timeline::prev_frame_timeline(stepping);
  timeline::prev_frame_timeline(stepping);
  check(stepping->current_frame == 1.0, "prevFrame clamps at frame 1");
}

void playback() {
  auto t = make();
  t->time_elapsed = 42.0;
  timeline::play_timeline(t);
  check(t->is_playing && t->time_elapsed == 0.0, "play sets isPlaying and resets timeElapsed");
  timeline::stop_timeline(t);
  check(!t->is_playing, "stop clears isPlaying");
  auto single = make(1.0);
  timeline::play_timeline(single);
  check(!single->is_playing, "play is a no-op when totalFrames < 2");
}

void updates() {
  {
    auto t = make();
    auto& frames = record(t);
    check(timeline::update_timeline(t, 0.0), "the first stopped update realizes frame 1");
    check(!timeline::update_timeline(t, 0.0) && frames == std::vector<double>{1.0}, "a repeated stopped update realizes nothing");
  }
  {
    auto t = make(4.0, 10.0);
    timeline::play_timeline(t);
    check(timeline::update_timeline(t, 0.0), "timed: the initial frame is realized");
    check(!timeline::update_timeline(t, 50.0), "timed: half a frame realizes nothing");
    check(timeline::update_timeline(t, 50.0), "timed: a full frame time realizes the next");
  }
  {
    auto t = make(4.0, std::nullopt);
    auto& frames = record(t);
    timeline::play_timeline(t);
    timeline::update_timeline(t, 0.0);
    check(t->last_frame_update == 1.0 && t->current_frame == 2.0, "frameRate-null: realizes, then advances the next-call playhead");
    timeline::update_timeline(t, 0.0);
    timeline::update_timeline(t, 0.0);
    check(frames == std::vector<double>{1.0, 2.0, 3.0}, "frameRate-null advances one frame per update");
  }
  {
    auto t = make(3.0, std::nullopt);
    auto& frames = record(t);
    timeline::play_timeline(t);
    for (int i = 0; i < 4; ++i) timeline::update_timeline(t, 0.0);
    check(frames == std::vector<double>{1.0, 2.0, 3.0, 1.0}, "loop mode wraps to frame 1");
  }
  {
    auto t = make(3.0, std::nullopt);
    auto& frames = record(t);
    t->play_mode = flight::String("once");
    timeline::play_timeline(t);
    for (int i = 0; i < 4; ++i) timeline::update_timeline(t, 0.0);
    check(frames == std::vector<double>{1.0, 2.0, 3.0} && !t->is_playing, "once mode stops at the last frame");
  }
  {
    auto t = make(10.0, 10.0);
    auto& frames = record(t);
    timeline::play_timeline(t);
    timeline::update_timeline(t, 0.0);
    timeline::update_timeline(t, 350.0);
    check(frames == std::vector<double>{1.0, 4.0}, "a large deltaTime skips to the landing frame only");
  }
}

void signals() {
  {
    auto t = make(4.0, std::nullopt);
    auto events = std::make_shared<std::vector<std::pair<double, double>>>();
    auto group = timeline::enable_timeline_signals(t);
    check(timeline::enable_timeline_signals(t) == group && t->signals.has_value(), "enable is idempotent and stores the group");
    flight::signals::connect_signal(
        group->on_enter_frame,
        std::function<void(flight::Ref<flight::types::TimelineFrameEvent>)>(
            [events](flight::Ref<flight::types::TimelineFrameEvent> e) { events->push_back({e->frame, e->previous_frame}); }),
        std::nullopt);
    timeline::play_timeline(t);
    timeline::update_timeline(t, 0.0);
    timeline::update_timeline(t, 0.0);
    check(events->size() == 2 && (*events)[0].first == 1.0 && (*events)[1] == std::pair{2.0, 1.0}, "onEnterFrame reports frame and previousFrame");
  }
  {
    auto t = make(4.0, std::nullopt);
    auto order = std::make_shared<std::vector<std::string>>();
    auto group = timeline::enable_timeline_signals(t);
    using Handler = std::function<void(flight::Ref<flight::types::TimelineFrameEvent>)>;
    flight::signals::connect_signal(group->on_exit_frame, Handler([order](auto) { order->push_back("exit"); }), std::nullopt);
    flight::signals::connect_signal(group->on_enter_frame, Handler([order](auto) { order->push_back("enter"); }), std::nullopt);
    flight::signals::connect_signal(group->on_frame_constructed, Handler([order](auto) { order->push_back("constructed"); }), std::nullopt);
    timeline::play_timeline(t);
    timeline::update_timeline(t, 0.0);
    check(*order == std::vector<std::string>{"exit", "enter", "constructed"}, "exit, enter, constructed order");
  }
  {
    auto t = make(2.0, std::nullopt);
    auto looped = std::make_shared<bool>(false);
    flight::signals::connect_signal(timeline::enable_timeline_signals(t)->on_loop, std::function<void()>([looped] { *looped = true; }), std::nullopt);
    timeline::play_timeline(t);
    for (int i = 0; i < 3; ++i) timeline::update_timeline(t, 0.0);
    check(*looped, "onLoop fires on wrap");
  }
  {
    auto t = make(2.0, std::nullopt);
    t->play_mode = flight::String("once");
    auto completed = std::make_shared<bool>(false);
    flight::signals::connect_signal(timeline::enable_timeline_signals(t)->on_complete, std::function<void()>([completed] { *completed = true; }), std::nullopt);
    timeline::play_timeline(t);
    for (int i = 0; i < 3; ++i) timeline::update_timeline(t, 0.0);
    check(*completed && !t->is_playing, "onComplete fires and playback stops in once mode");
  }
  {
    auto t = make();
    timeline::enable_timeline_signals(t);
    timeline::dispose_timeline_signals(t);
    check(!t->signals.has_value(), "dispose clears signals");
    timeline::dispose_timeline_signals(t);
    check(timeline::enable_timeline_signals(t) != nullptr && t->signals.has_value(), "enable re-arms after dispose");
  }
}

void scripts() {
  auto t = make(10.0, 10.0, {{"run", 3.0}});
  flight::types::FrameScript script = [](auto, double) {};
  check(timeline::get_timeline_frame_script_frames(view(t)).size() == 0, "no scripts: an empty frame list");
  check(!timeline::get_timeline_frame_script(view(t), Frame{2.0}).has_value(), "no scripts: no script");
  timeline::add_timeline_frame_script(t, Frame{5.0}, script);
  timeline::add_timeline_frame_script(t, Frame{flight::String("run")}, script);
  auto frames = timeline::get_timeline_frame_script_frames(view(t));
  check(frames.size() == 2 && frames.element(0) == 5.0 && frames.element(1) == 3.0, "frames in insertion order; a label resolves");
  check(timeline::get_timeline_frame_script(view(t), Frame{flight::String("run")}).has_value(), "get by label");
  check(!timeline::get_timeline_frame_script(view(t), Frame{2.0}).has_value(), "no script on an unscripted frame");
  timeline::remove_timeline_frame_script(t, Frame{5.0});
  check(!timeline::get_timeline_frame_script(view(t), Frame{5.0}).has_value() && t->frame_scripts.has_value(), "remove one");
  timeline::remove_timeline_frame_script(t, Frame{3.0});
  check(!t->frame_scripts.has_value(), "removing the last script restores null storage");
  timeline::add_timeline_frame_script(t, Frame{1.0}, script);
  timeline::clear_timeline_frame_scripts(t);
  check(!t->frame_scripts.has_value(), "clear restores null storage");
}

}  // namespace

int main() {
  defaults();
  labels();
  seeking();
  playback();
  updates();
  signals();
  scripts();
  return failures == 0 ? 0 : 1;
}
