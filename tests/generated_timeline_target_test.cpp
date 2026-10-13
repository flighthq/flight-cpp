// The generated @flighthq/timeline package at a627849 driving a real Node2D target, exercised natively.
// generated_timeline_test.cpp covers the timeline with a null target, which upstream allows but which
// never reaches the two calls a target unlocks: constructFrame and frame scripts. Here the target is a
// display object from scene2d's supplying override (DisplayObject and Node2D are one emitted type), and the
// expectations are the target-dependent half of packages/timeline/src/timeline.test.ts:
// - settlement: constructFrame fires once per realized frame with (target, frame), never again on
//   repeated stopped updates, and immediately on a seek;
// - frame scripts: fire once on frame entry with the target, not again while stopped on that frame,
//   accept a label as the frame, and stop firing once removed;
// - ordering: the per-frame order is exitFrame, enterFrame, constructFrame, frame script,
//   frameConstructed;
// - lifecycle: createTimeline applies its overrides, getTimelineLabels returns the source's own list,
//   and createTimelineSource carries its fields and calls constructFrame with the target it is given.
#include <flight/scene2d/display_object.hpp>
#include <flight/signals/slot.hpp>
#include <flight/timeline/timeline.hpp>

#include <iostream>
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

template <typename Pointer>
auto view(const Pointer& value) {
  using Element = typename Pointer::element_type;
  return flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<Element>>>>(value);
}

namespace tl = flight::timeline;
using flight::types::Node2D;
using flight::types::Timeline;
using SourceOptions = tl::total_frames_frame_rate_labels_cues_construct_frame_a8754a82464a8856;
using TimelineOptions =
    tl::entity_runtime_key_source_target_current_frame_cue_registry_frame_scripts_is_playing_time_elapsed_last_frame_update_play_mode_signals_e9540e1854a4d1d4;
using Frame = std::variant<double, flight::String>;
using Rate = std::variant<double, flight::Null, flight::Undefined>;

struct Label {
  const char* name;
  double frame;
};

struct Make {
  double totalFrames = 4;
  std::optional<double> frameRate = 10;  // nullopt means frameRate null: one frame per update
  std::vector<Label> labels;
  std::vector<double>* constructed = nullptr;
  std::optional<double> currentFrame;
};

// The upstream fixture: a timeline over a source, targeting a real display object, built through
// createTimeline's override object exactly as upstream's make() does.
struct Rig {
  flight::Ref<Timeline> timeline;
  Node2D target;
};

flight::Array<flight::Ref<flight::types::TimelineLabel>> labelList(const std::vector<Label>& labels) {
  flight::Array<flight::Ref<flight::types::TimelineLabel>> list;
  for (const auto& l : labels) {
    auto entry = flight::make_ref<flight::types::TimelineLabel>();
    entry->name = flight::String(l.name);
    entry->frame = l.frame;
    list.push(entry);
  }
  return list;
}

Rig make(Make m) {
  Node2D target = flight::scene2d::create_display_object();
  auto source = flight::make_ref<SourceOptions>();
  source->total_frames = m.totalFrames;
  source->frame_rate = m.frameRate.has_value() ? Rate{std::in_place_type<double>, *m.frameRate} : Rate{std::in_place_type<flight::Null>, flight::null};
  if (!m.labels.empty()) source->labels = labelList(m.labels);
  if (m.constructed != nullptr) {
    auto* out = m.constructed;
    source->construct_frame = std::function<void(Node2D, double)>([out, target](Node2D t, double frame) {
      if (t == target) out->push_back(frame);
    });
  }
  auto overrides = flight::make_ref<TimelineOptions>();
  overrides->source = tl::create_timeline_source(source);
  overrides->target = target;
  if (m.currentFrame.has_value()) overrides->current_frame = m.currentFrame;
  return {tl::create_timeline(overrides), target};
}

void construct_frame() {
  {
    std::vector<double> frames;
    auto r = make({.constructed = &frames});
    check(tl::update_timeline(r.timeline, 0), "the first stopped update realizes frame 1");
    check(!tl::update_timeline(r.timeline, 0), "a repeated stopped update realizes nothing");
    check(frames == std::vector<double>{1}, "constructFrame fires exactly once for frame 1, with the target");
  }
  {
    std::vector<double> frames;
    auto r = make({.frameRate = std::nullopt, .constructed = &frames});
    tl::play_timeline(r.timeline);
    tl::update_timeline(r.timeline, 16);
    tl::update_timeline(r.timeline, 16);
    check(frames == std::vector<double>{1, 2}, "constructFrame fires for each realized frame");
  }
  {
    std::vector<double> frames;
    auto r = make({.constructed = &frames});
    tl::goto_and_play_timeline(r.timeline, Frame{3.0});
    check(frames == std::vector<double>{3}, "a seek constructs the target frame immediately");
  }
  {
    std::vector<double> frames;
    auto r = make({.frameRate = std::nullopt, .constructed = &frames});
    tl::goto_and_stop_timeline(r.timeline, Frame{99.0});
    check(frames == std::vector<double>{4} && r.timeline->current_frame == 4, "a clamped seek constructs the clamped frame");
  }
}

void frame_scripts() {
  {
    std::vector<double> fired;
    auto r = make({.frameRate = std::nullopt});
    auto target = r.target;
    tl::add_timeline_frame_script(r.timeline, Frame{2.0}, [&fired, target](Node2D t, double f) {
      if (t == target) fired.push_back(f);
    });
    tl::play_timeline(r.timeline);
    tl::update_timeline(r.timeline, 0);
    tl::update_timeline(r.timeline, 0);
    check(fired == std::vector<double>{2}, "a frame script fires once on entry, with the target");
    tl::stop_timeline(r.timeline);
    tl::update_timeline(r.timeline, 0);
    tl::update_timeline(r.timeline, 0);
    check(fired == std::vector<double>{2}, "it does not re-fire while stopped on its frame");
  }
  {
    std::vector<double> fired;
    auto r = make({.frameRate = std::nullopt, .labels = {{"run", 3}}});
    tl::add_timeline_frame_script(r.timeline, Frame{flight::String("run")}, [&](Node2D, double f) { fired.push_back(f); });
    tl::goto_and_stop_timeline(r.timeline, Frame{3.0});
    check(fired == std::vector<double>{3}, "a label resolves as the script's frame");
  }
  {
    std::vector<double> fired;
    auto r = make({.frameRate = std::nullopt});
    tl::add_timeline_frame_script(r.timeline, Frame{2.0}, [&](Node2D, double f) { fired.push_back(f); });
    tl::remove_timeline_frame_script(r.timeline, Frame{2.0});
    tl::play_timeline(r.timeline);
    tl::update_timeline(r.timeline, 0);
    tl::update_timeline(r.timeline, 0);
    check(fired.empty(), "a removed script does not fire");
  }
}

void per_frame_order() {
  std::vector<std::string> order;
  auto r = make({.frameRate = std::nullopt});
  auto source = flight::make_ref<SourceOptions>();
  source->total_frames = 4.0;
  source->frame_rate = Rate{std::in_place_type<flight::Null>, flight::null};
  source->construct_frame = std::function<void(Node2D, double)>([&](Node2D, double) { order.push_back("construct"); });
  r.timeline->source = tl::create_timeline_source(source);
  tl::add_timeline_frame_script(r.timeline, Frame{1.0}, [&](Node2D, double) { order.push_back("script"); });
  auto signals = tl::enable_timeline_signals(r.timeline);
  using Handler = std::function<void(flight::Ref<flight::types::TimelineFrameEvent>)>;
  flight::signals::connect_signal(signals->on_exit_frame, Handler([&](auto) { order.push_back("exit"); }), std::nullopt);
  flight::signals::connect_signal(signals->on_enter_frame, Handler([&](auto) { order.push_back("enter"); }), std::nullopt);
  flight::signals::connect_signal(signals->on_frame_constructed, Handler([&](auto) { order.push_back("constructed"); }), std::nullopt);
  tl::update_timeline(r.timeline, 0);
  check(order == std::vector<std::string>{"exit", "enter", "construct", "script", "constructed"},
        "per frame: exit, enter, constructFrame, script, frameConstructed");
}

void lifecycle() {
  {
    auto r = make({.frameRate = 24, .currentFrame = 3});
    check(r.timeline->current_frame == 3, "createTimeline applies a currentFrame override");
    check(r.timeline->source.has_value() && r.timeline->source.value()->frame_rate.has_value() &&
              r.timeline->source.value()->frame_rate.value() == 24,
          "createTimeline keeps the given source");
    check(r.timeline->target.has_value() && r.timeline->target.value() == r.target, "createTimeline keeps the given target");
  }
  {
    auto r = make({.labels = {{"idle", 1}, {"run", 3}}});
    auto labels = tl::get_timeline_labels(view(r.timeline));
    check(labels == r.timeline->source.value()->labels && labels.size() == 2, "getTimelineLabels returns the source's own list");
  }
  {
    std::vector<double> seen;
    auto options = flight::make_ref<SourceOptions>();
    options->total_frames = 3.0;
    options->frame_rate = Rate{std::in_place_type<double>, 12.0};
    options->labels = labelList({{"a", 2}});
    Node2D target = flight::scene2d::create_display_object();
    options->construct_frame = std::function<void(Node2D, double)>([&](Node2D t, double f) {
      if (t == target) seen.push_back(f);
    });
    auto s = tl::create_timeline_source(options);
    check(s->total_frames == 3 && s->frame_rate.value_or(0) == 12 && s->labels.size() == 1 &&
              s->labels.element(0.0)->name == flight::String("a") && s->labels.element(0.0)->frame == 2,
          "createTimelineSource carries its fields");
    s->construct_frame(target, 2);
    check(seen == std::vector<double>{2}, "constructFrame receives (target, frame)");
  }
}

}  // namespace

int main() {
  construct_frame();
  frame_scripts();
  per_frame_order();
  lifecycle();
  return failures == 0 ? 0 : 1;
}
