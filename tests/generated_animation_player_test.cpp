// The generated @flighthq/animation player at a627849, exercised natively. Ported from
// packages/animation/src/animationPlayer.test.ts, the player's whole contract:
// - timing: dt * speed advance, backward play, Repeat wrapping (including a dt that crosses several
//   boundaries), PingPong reflection with speed reversal, and the no-ops (paused, zero-duration clip);
// - settlement: clamping and stopping when not looping, the finite repeat budget spent one unit per
//   wrap, and onFinished / onLooped emission;
// - ordering: clip events crossed during an advance, in traversal order, with boundary markers fired
//   once per crossed Repeat boundary and not doubled on a PingPong reflection;
// - lifecycle: defaults, clone (shares the clip, starts signal-free), idempotent signal enabling,
//   play / seek / stop and normalized time.
#include <flight/animation/animation_clip.hpp>
#include <flight/animation/animation_player.hpp>
#include <flight/signals/slot.hpp>

#include <cmath>
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

bool close(double actual, double expected) { return std::abs(actual - expected) < 0.005; }

template <typename Pointer>
auto view(const Pointer& value) {
  using Element = typename Pointer::element_type;
  return flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<Element>>>>(value);
}

namespace anim = flight::animation;
using flight::types::AnimationPlayer;
using Options = anim::loop_loop_mode_playing_repeat_count_speed_time_b86d86cd08eca121;

struct Opts {
  std::optional<bool> loop;
  const char* loopMode = nullptr;
  std::optional<bool> playing;
  std::optional<double> repeatCount;
  std::optional<double> speed;
  std::optional<double> time;
};

flight::Ref<AnimationPlayer> playerWith(const flight::Ref<flight::types::AnimationClip>& clip, Opts o) {
  auto options = flight::make_ref<Options>();
  options->loop = o.loop;
  if (o.loopMode != nullptr) options->loop_mode = flight::String(o.loopMode);
  options->playing = o.playing;
  options->repeat_count = o.repeatCount;
  options->speed = o.speed;
  options->time = o.time;
  return anim::create_animation_player(clip, view(options));
}

flight::Ref<AnimationPlayer> player(double duration, Opts o = {}) {
  return playerWith(anim::create_animation_clip({}, duration), o);
}

// A clip whose events sit at `times`, each carrying its time as payload.
flight::Ref<AnimationPlayer> eventPlayer(double duration, std::vector<double> times, Opts o = {}) {
  flight::Array<flight::Ref<flight::types::AnimationClipEvent>> events;
  for (double t : times) {
    events.push(anim::create_animation_clip_event(t, flight::String(std::to_string(t).c_str()), flight::Any(t)));
  }
  return playerWith(anim::create_animation_clip({}, duration, events), o);
}

template <typename Variant>
auto signalOf(const Variant& slot) {
  using Pointer = std::variant_alternative_t<0, Variant>;
  return std::holds_alternative<Pointer>(slot) ? std::get<Pointer>(slot) : Pointer{};
}

// Records the time of every clip event the player emits.
std::vector<double>& recordEvents(const flight::Ref<AnimationPlayer>& p) {
  static std::vector<std::vector<double>> store;
  store.emplace_back();
  auto& out = store.back();
  anim::enable_animation_player_signals(p);
  using Event = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::AnimationClipEvent>>>>;
  flight::signals::connect_signal(signalOf(p->on_event),
                                  std::function<void(Event)>([&out](Event e) { out.push_back(flight::row_get<flight::RowKey<"time">>(e)); }),
                                  std::nullopt);
  return out;
}

void timing() {
  {
    auto p = player(10, {.loop = false, .speed = 2});
    anim::advance_animation_player(p, 1);
    check(p->time == 2, "advances by dt * speed");
  }
  {
    auto p = player(10, {.loop = true, .time = 9});
    anim::advance_animation_player(p, 3);
    check(close(p->time, 2) && p->playing, "Repeat wraps modulo duration and keeps playing");
  }
  {
    auto p = player(0, {.time = 0});
    anim::advance_animation_player(p, 1);
    check(p->time == 0, "a zero-duration clip is a no-op");
  }
  {
    auto p = player(10, {.playing = false, .time = 3});
    anim::advance_animation_player(p, 5);
    check(p->time == 3, "paused is a no-op");
  }
  {
    auto p = player(10, {.speed = -1, .time = 5});
    anim::advance_animation_player(p, 1);
    check(close(p->time, 4), "negative speed plays backward");
  }
  {
    auto p = player(10, {.loop = true, .speed = -1, .time = 0.5});
    anim::advance_animation_player(p, 2);
    check(close(p->time, 8.5) && p->playing, "backward Repeat wraps into [0, duration)");
  }
  {
    auto p = player(10, {.loopMode = "PingPong", .speed = 1, .time = 9});
    anim::advance_animation_player(p, 2);
    check(close(p->time, 9) && p->speed == -1 && p->playing, "PingPong reflects at the end and reverses speed");
  }
  {
    auto p = player(10, {.loopMode = "PingPong", .speed = -1, .time = 1});
    anim::advance_animation_player(p, 2);
    check(close(p->time, 1) && p->speed == 1 && p->playing, "PingPong reflects at the start and reverses speed");
  }
}

void settlement() {
  {
    auto p = player(10, {.loop = false, .time = 9});
    anim::advance_animation_player(p, 5);
    check(p->time == 10 && !p->playing, "not looping: clamps at the end and stops");
  }
  {
    auto p = player(10, {.loop = false, .speed = -1, .time = 0.5});
    anim::advance_animation_player(p, 2);
    check(p->time == 0 && !p->playing, "not looping backward: clamps at 0 and stops");
  }
  {
    auto p = player(10, {.loop = true, .repeatCount = 0, .time = 9});
    anim::advance_animation_player(p, 2);
    check(p->time == 10 && !p->playing, "an exhausted repeat budget stops at the end");
  }
  {
    auto p = player(10, {.loop = true, .repeatCount = 1, .time = 9});
    anim::advance_animation_player(p, 2);
    check(close(p->time, 1) && p->playing, "the first wrap spends the budget");
    anim::advance_animation_player(p, 10);
    check(!p->playing, "the next boundary stops once the budget is spent");
  }
  {
    auto p = player(10, {.loop = true, .time = 9});
    anim::enable_animation_player_signals(p);
    int looped = 0;
    flight::signals::connect_signal(signalOf(p->on_looped), std::function<void()>([&] { ++looped; }), std::nullopt);
    anim::advance_animation_player(p, 3);
    check(looped == 1, "onLooped fires once for a wrap");
  }
  {
    auto p = player(10, {.loop = false, .time = 9});
    anim::enable_animation_player_signals(p);
    int finished = 0;
    flight::signals::connect_signal(signalOf(p->on_finished), std::function<void()>([&] { ++finished; }), std::nullopt);
    anim::advance_animation_player(p, 5);
    check(finished == 1, "onFinished fires once at the end of a non-looping run");
  }
}

void ordering() {
  {
    auto p = eventPlayer(10, {1, 3, 7}, {.loop = false});
    auto& crossed = recordEvents(p);
    anim::advance_animation_player(p, 4);
    check(crossed == std::vector<double>{1, 3}, "crossed events fire in forward order");
  }
  {
    auto p = eventPlayer(10, {0, 2, 10}, {.time = 9});
    auto& crossed = recordEvents(p);
    anim::advance_animation_player(p, 13);
    check(crossed == std::vector<double>{10, 0, 2, 10, 0, 2}, "boundary markers fire once per crossed Repeat boundary");
  }
  {
    auto p = eventPlayer(10, {0, 2, 8, 10}, {.speed = -1, .time = 1});
    auto& crossed = recordEvents(p);
    anim::advance_animation_player(p, 4);
    check(crossed == std::vector<double>{0, 10, 8}, "reverse crossings fire in reverse traversal order");
  }
  {
    auto p = eventPlayer(10, {8, 10}, {.loopMode = "PingPong", .time = 9});
    auto& crossed = recordEvents(p);
    anim::advance_animation_player(p, 3);
    check(crossed == std::vector<double>{10, 8}, "a PingPong boundary marker is not doubled on reflection");
  }
}

void lifecycle() {
  {
    auto p = player(5);
    check(p->loop && p->playing && p->speed == 1 && p->time == 0, "defaults: looping, playing, speed 1, time 0");
    check(p->loop_mode.has_value() && p->loop_mode.value() == flight::String("Repeat") && p->repeat_count.value_or(0) == -1,
          "defaults: Repeat mode, infinite repeats");
    check(std::holds_alternative<flight::Null>(p->on_finished) && std::holds_alternative<flight::Null>(p->on_looped) &&
              std::holds_alternative<flight::Null>(p->on_event),
          "defaults: no signals");
  }
  {
    auto p = player(10, {.loop = false, .loopMode = "PingPong", .repeatCount = 3, .speed = 2, .time = 4});
    anim::enable_animation_player_signals(p);
    auto clone = anim::clone_animation_player(view(p));
    check(clone != p && clone->clip == p->clip, "clone is a new player sharing the clip");
    check(!clone->loop && clone->loop_mode.value() == flight::String("PingPong") && clone->repeat_count.value() == 3 &&
              clone->speed == 2 && clone->time == 4,
          "clone copies the fields");
    check(std::holds_alternative<flight::Null>(clone->on_event) && std::holds_alternative<flight::Null>(clone->on_finished) &&
              std::holds_alternative<flight::Null>(clone->on_looped),
          "clone starts signal-free");
  }
  {
    auto p = player(5);
    anim::enable_animation_player_signals(p);
    auto finished = signalOf(p->on_finished);
    auto looped = signalOf(p->on_looped);
    auto event = signalOf(p->on_event);
    check(finished && looped && event, "enable allocates every signal");
    anim::enable_animation_player_signals(p);
    check(signalOf(p->on_finished) == finished && signalOf(p->on_looped) == looped && signalOf(p->on_event) == event,
          "enable is idempotent");
  }
  {
    auto p = player(10, {.time = 2.5});
    check(close(anim::get_animation_player_normalized_time(view(p)), 0.25), "normalized time is time / duration");
    anim::seek_animation_player(p, 10);
    check(anim::get_animation_player_normalized_time(view(p)) == 1, "normalized time at the end is 1");
    check(anim::get_animation_player_normalized_time(view(player(0))) == 0, "normalized time is 0 for a zero-duration clip");
  }
  {
    auto p = player(10, {.playing = false, .time = 3});
    anim::play_animation_player(p);
    check(p->playing && p->time == 3, "play resumes without moving the playhead");
    anim::seek_animation_player(p, 4);
    check(p->time == 4, "seek moves the playhead");
    anim::seek_animation_player(p, -1);
    check(p->time == 0, "seek clamps below 0");
    anim::seek_animation_player(p, 99);
    check(p->time == 10, "seek clamps above the duration");
    anim::stop_animation_player(p);
    check(!p->playing && p->time == 0, "stop clears playing and rewinds");
  }
}

}  // namespace

int main() {
  timing();
  settlement();
  ordering();
  lifecycle();
  return failures == 0 ? 0 : 1;
}
