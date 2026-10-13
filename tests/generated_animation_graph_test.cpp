// The generated @flighthq/animation clip, blend tree and state machine at a627849, exercised natively. Ported
// from packages/animation/src/animationClip.test.ts, animationBlendTree.test.ts and
// animationStateMachine.test.ts:
// - clip: duration derived from the latest keyframe or event (or an explicit override), events copied and
//   sorted, invalid or unreachable event times rejected, and sampling that visits every channel in order;
// - blend tree ordering: an N-way weighted pose normalized per target identity, one-sided targets kept in
//   first-appearance order, additive deltas applied after the normalized override pose, identity as an
//   additive-only base, non-positive weights skipped, and a shared player advanced exactly once;
// - state machine lifecycle and settlement: a named initial state, rejection of empty / duplicate /
//   missing / incompatible states, the timed-transition lifecycle (source stays current until the
//   transition completes, curved weight, zero-duration transitions), and rejected transition requests.
#include <flight/animation/animation_blend_tree.hpp>
#include <flight/animation/animation_clip.hpp>
#include <flight/animation/animation_player.hpp>
#include <flight/animation/animation_state_machine.hpp>
#include <flight/animation/animation_track.hpp>

#include <cmath>
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

bool close(double actual, double expected) { return std::abs(actual - expected) < 0.005; }

template <typename Pointer>
auto view(const Pointer& value) {
  using Element = typename Pointer::element_type;
  return flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<Element>>>>(value);
}

namespace anim = flight::animation;
using Buffer = std::variant<flight::Array<double>, flight::Float32Array>;
using Numbers = flight::SequenceView<double>;
using TrackOptions = anim::times_values_components_interpolation_quaternion_easing_segment_easings_9b0eadfed5e06042;
using flight::types::AnimationPlayer;

double at(const Buffer& b, double index) {
  return std::visit(
      [&](const auto& held) -> double {
        using Held = std::decay_t<decltype(held)>;
        if constexpr (std::is_same_v<Held, flight::Array<double>>) return held.element(index);
        else return held[static_cast<std::size_t>(index)];
      },
      b);
}

flight::Any target() { return flight::Any::object(flight::make_ref<flight::types::AnimationClipEvent>()); }

flight::Ref<flight::types::AnimationTrack> track(flight::Array<double> times, flight::Array<double> values,
                                                 std::optional<double> components = std::nullopt, bool quaternion = false) {
  auto opts = flight::make_ref<TrackOptions>();
  opts->times = Numbers(times);
  opts->values = Numbers(values);
  opts->components = components;
  if (quaternion) opts->quaternion = true;
  return anim::create_animation_track(opts);
}

flight::Ref<flight::types::AnimationClip> clipOf(std::vector<std::pair<flight::Ref<flight::types::AnimationTrack>, flight::Any>> specs,
                                                 std::optional<double> duration = std::nullopt) {
  flight::Array<flight::Ref<flight::types::AnimationChannel>> channels;
  for (const auto& [t, ref] : specs) channels.push(anim::create_animation_channel(t, ref));
  return anim::create_animation_clip(channels, duration);
}

// The upstream fixture: a constant value, or a ramp value -> value + 10 over `duration`.
flight::Ref<AnimationPlayer> player(const flight::Any& t, double value, double duration = 0) {
  auto times = duration > 0 ? flight::Array<double>{0.0, duration} : flight::Array<double>{0.0};
  auto values = duration > 0 ? flight::Array<double>{value, value + 10} : flight::Array<double>{value};
  return anim::create_animation_player(clipOf({{track(times, values), t}}));
}

flight::Ref<flight::types::AnimationBlendTreeInput> input(const flight::Ref<AnimationPlayer>& p, std::optional<double> weight = std::nullopt,
                                                          std::optional<bool> additive = std::nullopt) {
  return anim::create_animation_blend_tree_input(p, weight, additive);
}

flight::Ref<flight::types::AnimationBlendTree> tree(std::vector<flight::Ref<flight::types::AnimationBlendTreeInput>> inputs) {
  flight::Array<flight::Ref<flight::types::AnimationBlendTreeInput>> list;
  for (const auto& i : inputs) list.push(i);
  return anim::create_animation_blend_tree(list);
}

flight::Ref<flight::types::AnimationStateMachineState> state(const char* name, std::vector<flight::Ref<AnimationPlayer>> players) {
  std::vector<flight::Ref<flight::types::AnimationBlendTreeInput>> inputs;
  for (const auto& p : players) inputs.push_back(input(p));
  return anim::create_animation_state_machine_state(flight::String(name), tree(inputs));
}

flight::Ref<flight::types::AnimationStateMachine> machine(std::vector<flight::Ref<flight::types::AnimationStateMachineState>> states,
                                                          std::optional<std::variant<double, flight::String>> initial = std::nullopt) {
  flight::Array<flight::Ref<flight::types::AnimationStateMachineState>> list;
  for (const auto& s : states) list.push(s);
  return anim::create_animation_state_machine(list, initial);
}

bool currentIs(const flight::Ref<flight::types::AnimationStateMachine>& m, const char* name) {
  return flight::row_get<flight::RowKey<"name">>(anim::get_animation_state_machine_current_state(view(m))) == flight::String(name);
}

std::string thrown(const std::function<void()>& run) {
  try {
    run();
  } catch (const std::exception& error) {
    return error.what();
  }
  return {};
}

bool mentions(const std::string& message, const char* fragment) { return message.find(fragment) != std::string::npos; }

void clips() {
  auto none = flight::Any();
  check(clipOf({{track({0.0, 1.0}, {0.0, 1.0}), none}, {track({0.0, 2.5}, {0.0, 1.0}), none}})->duration == 2.5,
        "duration is the latest keyframe across channels");
  check(clipOf({{track({0.0, 1.0}, {0.0, 1.0}), none}}, 10)->duration == 10, "an explicit duration overrides");

  auto late = anim::create_animation_clip_event(3, flight::String("late"));
  auto early = anim::create_animation_clip_event(1, flight::String("early"));
  flight::Array<flight::Ref<flight::types::AnimationClipEvent>> events{late, early};
  auto clip = anim::create_animation_clip({}, std::nullopt, events);
  check(!(clip->events == events) && clip->events.element(0.0) == early && clip->events.element(1.0) == late,
        "events are copied and sorted by time");
  check(clip->duration == 3, "the latest event time extends the derived duration");

  check(mentions(thrown([] {
                   anim::create_animation_clip({}, std::nullopt,
                                               flight::Array<flight::Ref<flight::types::AnimationClipEvent>>{
                                                   anim::create_animation_clip_event(-1, flight::String("bad"))});
                 }),
                 "finite non-negative"),
        "a negative event time is rejected");
  check(mentions(thrown([] {
                   anim::create_animation_clip({}, 1,
                                               flight::Array<flight::Ref<flight::types::AnimationClipEvent>>{
                                                   anim::create_animation_clip_event(2, flight::String("late"))});
                 }),
                 "exceeds"),
        "an event past an explicit duration is rejected");

  auto a = target(), b = target();
  auto sampled = clipOf({{track({0.0, 1.0}, {0.0, 10.0}), a}, {track({0.0, 1.0}, {100.0, 200.0}), b}});
  std::vector<std::pair<double, double>> seen;
  std::vector<flight::Any> targets;
  anim::sample_animation_clip(Buffer{flight::Array<double>{0.0}}, view(sampled), 0.5, [&](Buffer s, auto channel, double index) {
    seen.push_back({index, at(s, 0)});
    targets.push_back(flight::row_get<flight::RowKey<"targetRef">>(channel));
  });
  check(seen == std::vector<std::pair<double, double>>{{0, 5}, {1, 150}} && targets[0] == a && targets[1] == b,
        "sampling visits every channel in order with its sample and target");
  int calls = 0;
  anim::sample_animation_clip(Buffer{flight::Array<double>{0.0}}, view(anim::create_animation_clip({}, 1)), 0,
                              [&](Buffer, auto, double) { ++calls; });
  check(calls == 0, "a channel-less clip visits nothing");
}

std::vector<std::pair<flight::Any, double>> sampleTree(const flight::Ref<flight::types::AnimationBlendTree>& t, double width = 1, double fill = 0) {
  std::vector<std::pair<flight::Any, double>> seen;
  flight::Array<double> out;
  for (int i = 0; i < width; ++i) out.push(fill);
  anim::sample_animation_blend_tree(Buffer{out}, view(t), [&](Buffer s, auto channel, double) {
    seen.push_back({flight::row_get<flight::RowKey<"targetRef">>(channel), at(s, 0)});
  });
  return seen;
}

void blend_trees() {
  {
    auto shared = player(target(), 0, 2);
    auto t = tree({input(shared, 0.25), input(shared, 0.75)});
    auto players = t->players;
    check(t->players.size() == 1 && t->players.element(0.0) == shared, "a shared player is listed once");
    anim::advance_animation_blend_tree(t, 0.5);
    check(shared->time == 0.5 && t->players == players, "a shared player advances once, through the same list");
  }
  {
    auto x = target();
    auto t = tree({input(player(x, 1)), input(player(x, 2))});
    check(t->channels.size() == 1, "inputs on one target share one channel");
  }
  {
    auto x = target();
    auto seen = sampleTree(tree({input(player(x, 0), 1), input(player(x, 10), 2), input(player(x, 20), 1)}));
    check(seen.size() == 1 && seen[0].first == x && seen[0].second == 10, "an N-way pose is normalized per target");
  }
  {
    auto a = target(), b = target();
    auto seen = sampleTree(tree({input(player(a, 3)), input(player(b, 7))}));
    check(seen.size() == 2 && seen[0].first == a && seen[0].second == 3 && seen[1].first == b && seen[1].second == 7,
          "one-sided targets keep first-appearance order");
  }
  {
    auto x = target();
    auto seen = sampleTree(tree({input(player(x, 10)), input(player(x, 4), 0.5, true)}));
    check(seen.size() == 1 && seen[0].second == 12, "additive deltas apply after the normalized override pose");
  }
  {
    auto x = target();
    auto seen = sampleTree(tree({input(player(x, 8), 0.25, true)}), 1, 99);
    check(seen.size() == 1 && seen[0].second == 2, "an additive-only channel uses identity as its base");
  }
  {
    auto seen = sampleTree(tree({input(player(target(), 8), 0)}), 1, 99);
    check(seen.empty(), "targets whose weights are all non-positive are skipped");
  }
  {
    auto x = target();
    auto one = track({0.0}, {1.0});
    flight::Array<flight::Ref<flight::types::AnimationChannel>> duplicateChannels{anim::create_animation_channel(one, x),
                                                                                anim::create_animation_channel(one, x)};
    auto duplicate = anim::create_animation_player(anim::create_animation_clip(duplicateChannels));
    check(mentions(thrown([&] { tree({input(duplicate)}); }), "duplicate targetRef"), "a duplicate target is rejected");
    auto vector = anim::create_animation_player(clipOf({{track({0.0}, {1.0, 2.0}, 2), x}}));
    check(mentions(thrown([&] { tree({input(player(x, 1)), input(vector)}); }), "different component widths"),
          "incompatible widths on one target are rejected");
  }
  {
    auto t = tree({input(player(target(), 1)), input(player(target(), 2))});
    check(anim::set_animation_blend_tree_input_weight(t, 1, 0.75) && t->inputs.element(1.0)->weight == 0.75, "set weight updates a present input");
    check(!anim::set_animation_blend_tree_input_weight(t, 5, 0.5), "set weight rejects an absent index");
  }
}

void state_machines() {
  {
    auto x = target();
    auto shared = player(x, 0, 4);
    auto destination = player(x, 20, 4);
    auto m = machine({state("idle", {shared}), state("walk", {shared, destination})});
    auto scratch = m->advance_scratch;
    check(anim::transition_animation_state_machine(m, flight::String("walk"), 1), "a transition starts");
    anim::advance_animation_state_machine(m, 0.5);
    check(shared->time == 0.5 && destination->time == 0.5, "both states advance, and the shared player only once");
    check(m->advance_scratch == scratch && m->advance_scratch.size() == 2, "players are de-duplicated in owned scratch");
    check(m->transition_weight == 0.5, "the transition weight is linear in elapsed time");
    anim::advance_animation_state_machine(m, 0.5);
    check(currentIs(m, "walk") && !anim::is_animation_state_machine_transitioning(view(m)), "the transition completes by duration");
  }
  {
    auto m = machine({state("idle", {player(target(), 0)}), state("walk", {player(target(), 2)})}, flight::String("walk"));
    check(currentIs(m, "walk"), "the named initial state is current");
  }
  {
    check(mentions(thrown([] { machine({}); }), "at least one state"), "an empty machine is rejected");
    auto a = state("same", {player(target(), 0)});
    auto b = state("same", {player(target(), 1)});
    check(mentions(thrown([&] { machine({a, b}); }), "duplicate state"), "duplicate state names are rejected");
    check(mentions(thrown([&] { machine({a}, flight::String("missing")); }), "does not exist"), "a missing initial name is rejected");
    check(mentions(thrown([&] { machine({a}, 0.5); }), "does not exist"), "a non-integral initial index is rejected");
    auto x = target();
    auto vector = anim::create_animation_player(clipOf({{track({0.0}, {0.0, 1.0}, 2), x}}));
    check(mentions(thrown([&] { machine({state("scalar", {player(x, 0)}), state("vector", {vector})}); }), "incompatible"),
          "incompatible tracks across states are rejected");
  }
  {
    auto m = machine({state("a", {player(target(), 0)}), state("b", {player(target(), 1)})});
    check(!anim::is_animation_state_machine_transitioning(view(m)), "not transitioning at rest");
    anim::transition_animation_state_machine(m, flight::String("b"), 1);
    check(anim::is_animation_state_machine_transitioning(view(m)) && currentIs(m, "a"),
          "the source stays current while transitioning");
  }
  {
    auto m = machine({state("a", {player(target(), 0)}), state("b", {player(target(), 1)})});
    check(anim::transition_animation_state_machine(m, flight::String("b"), 2, flight::types::EasingFunction([](double t) { return t * t; })),
          "a curved transition starts");
    anim::advance_animation_state_machine(m, 1);
    check(m->transition_weight == 0.25, "the curve shapes the transition weight");
    anim::advance_animation_state_machine(m, 1);
    check(anim::transition_animation_state_machine(m, flight::String("a"), 0) && currentIs(m, "a"),
          "a zero-duration transition completes at once");
  }
  {
    auto m = machine({state("a", {player(target(), 0)}), state("b", {player(target(), 1)})});
    check(!anim::transition_animation_state_machine(m, flight::String("missing"), 1), "an unknown target state is rejected");
    check(!anim::transition_animation_state_machine(m, 0.5, 1), "a non-integral index is rejected");
    check(!anim::transition_animation_state_machine(m, flight::String("a"), 1), "transitioning to the current state is rejected");
    check(anim::transition_animation_state_machine(m, flight::String("b"), 1), "a valid transition starts");
    check(!anim::transition_animation_state_machine(m, flight::String("a"), 1), "an overlapping transition is rejected");
  }
}

}  // namespace

int main() {
  clips();
  blend_trees();
  state_machines();
  return failures == 0 ? 0 : 1;
}
