// The generated @flighthq/animation blending and crossfade at a627849, exercised natively. Ported from
// packages/animation/src/animationBlend.test.ts and animationCrossfade.test.ts:
// - interpolation: weighted accumulation and its normalization, additive samples (with output aliasing),
//   linear blends with alpha clamping, and shortest-arc quaternion slerp in blends, accumulation and
//   crossfades;
// - timing and settlement: a crossfade advances both players by the same delta, follows its curve over
//   clamped normalized time, completes a zero-duration transition at once, and retires on elapsed time,
//   not on a curve that overshoots 1;
// - ordering: channels are matched by target identity (not position), one-sided channels are retained,
//   and sampling visits them in that stable order;
// - lifecycle: scratch buffers are owned and reused, and incompatible or duplicate targets are rejected.
#include <flight/animation/animation_blend.hpp>
#include <flight/animation/animation_clip.hpp>
#include <flight/animation/animation_crossfade.hpp>
#include <flight/animation/animation_player.hpp>
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
const double kSqrtHalf = std::sqrt(0.5);

Buffer buf(std::initializer_list<double> values) { return Buffer{flight::Array<double>(values)}; }
Numbers nums(std::initializer_list<double> values) { return Numbers(flight::Array<double>(values)); }

double at(const Buffer& b, double index) {
  return std::visit(
      [&](const auto& held) -> double {
        using Held = std::decay_t<decltype(held)>;
        if constexpr (std::is_same_v<Held, flight::Array<double>>) return held.element(index);
        else return held[static_cast<std::size_t>(index)];
      },
      b);
}

bool equals(const Buffer& b, std::vector<double> expected) {
  for (std::size_t i = 0; i < expected.size(); ++i) {
    if (!close(at(b, static_cast<double>(i)), expected[i])) return false;
  }
  return true;
}

// A distinct identity to key a channel by, like the upstream tests' `{}`.
flight::Any target() { return flight::Any::object(flight::make_ref<flight::types::AnimationClipEvent>()); }

using TrackOptions = anim::times_values_components_interpolation_quaternion_easing_segment_easings_9b0eadfed5e06042;

flight::Ref<flight::types::AnimationTrack> track(flight::Array<double> times, flight::Array<double> values,
                                                 std::optional<double> components = std::nullopt, bool quaternion = false) {
  auto opts = flight::make_ref<TrackOptions>();
  opts->times = Numbers(times);
  opts->values = Numbers(values);
  opts->components = components;
  if (quaternion) opts->quaternion = true;
  return anim::create_animation_track(opts);
}

flight::Ref<flight::types::AnimationPlayer> player(const flight::Any& t, flight::Array<double> values,
                                                   std::optional<double> components = std::nullopt, bool quaternion = false) {
  flight::Array<flight::Ref<flight::types::AnimationChannel>> channels;
  channels.push(anim::create_animation_channel(track(flight::Array<double>{0.0, 1.0}, values, components, quaternion), t));
  return anim::create_animation_player(anim::create_animation_clip(channels));
}

flight::Ref<flight::types::AnimationPlayer> clipPlayer(std::vector<std::pair<flight::Any, double>> channelSpecs) {
  flight::Array<flight::Ref<flight::types::AnimationChannel>> channels;
  for (const auto& [t, value] : channelSpecs) {
    channels.push(anim::create_animation_channel(track(flight::Array<double>{0.0}, flight::Array<double>{value}), t));
  }
  return anim::create_animation_player(anim::create_animation_clip(channels));
}

std::string thrownMessage(const std::function<void()>& run) {
  try {
    run();
  } catch (const std::exception& error) {
    return error.what();
  }
  return {};
}

void accumulation() {
  {
    auto acc = anim::create_animation_sample_accumulator(2);
    anim::accumulate_animation_sample(acc, nums({2, 4}), 2);
    anim::accumulate_animation_sample(acc, nums({100, 100}), 0);
    anim::accumulate_animation_sample(acc, nums({100, 100}), -1);
    check(acc->values[0] == 4 && acc->values[1] == 8 && acc->weight == 2, "positive weights accumulate; zero and negative are ignored");
  }
  {
    auto acc = anim::create_animation_sample_accumulator(4, true);
    anim::accumulate_animation_sample(acc, nums({0, 0, kSqrtHalf, kSqrtHalf}), 1);
    anim::accumulate_animation_sample(acc, nums({0, 0, -kSqrtHalf, -kSqrtHalf}), 1);
    auto out = buf({0, 0, 0, 0});
    check(anim::finish_animation_sample(out, view(acc)) && close(at(out, 2), kSqrtHalf) && close(at(out, 3), kSqrtHalf),
          "equivalent quaternion hemispheres align instead of cancelling");
  }
  {
    auto acc = anim::create_animation_sample_accumulator(2);
    anim::accumulate_animation_sample(acc, nums({0, 10}), 1);
    anim::accumulate_animation_sample(acc, nums({10, 20}), 3);
    auto out = buf({0, 0});
    check(anim::finish_animation_sample(out, view(acc)) && equals(out, {7.5, 17.5}), "finish normalizes by total weight");
    auto untouched = buf({7});
    check(!anim::finish_animation_sample(untouched, view(anim::create_animation_sample_accumulator(1))) && equals(untouched, {7}),
          "an empty accumulator returns false and leaves the output");
  }
  {
    auto acc = anim::create_animation_sample_accumulator(3);
    check(acc->components == 3 && acc->values.size() == 3 && acc->values[0] == 0 && acc->weight == 0, "a new accumulator is zeroed");
    auto values = acc->values;
    anim::accumulate_animation_sample(acc, nums({2, 4, 6}), 1);
    anim::reset_animation_sample_accumulator(acc);
    check(acc->values == values && acc->values[1] == 0 && acc->weight == 0, "reset clears state but keeps the owned buffer");
  }
}

void blending() {
  {
    flight::Array<double> out{1, 2, 3};
    anim::add_animation_sample(Buffer{out}, Numbers(out), nums({2, -2, 4}), 0.5);
    check(out.element(0.0) == 2 && out.element(1.0) == 1 && out.element(2.0) == 5, "additive samples, aliasing the base");
  }
  {
    auto out = buf({0, 0, 0, 0});
    anim::add_animation_sample(out, nums({0, 0, 0, 1}), nums({0, 0, 1, 0}), 0.5, true);
    check(close(at(out, 2), kSqrtHalf) && close(at(out, 3), kSqrtHalf), "a weighted quaternion delta multiplies onto the base");
  }
  {
    flight::Array<double> a{0, 10};
    anim::blend_animation_samples(Buffer{a}, Numbers(a), nums({10, 20}), 0.25);
    flight::Array<double> b{10, 20};
    anim::blend_animation_samples(Buffer{b}, nums({0, 10}), Numbers(b), 0.25);
    check(a.element(0.0) == 2.5 && a.element(1.0) == 12.5 && b.element(0.0) == 2.5 && b.element(1.0) == 12.5,
          "blend aliasing either input");
  }
  {
    auto out = buf({0, 0});
    anim::blend_animation_samples(out, nums({0, 10}), nums({10, 20}), 0.25);
    check(equals(out, {2.5, 12.5}), "linear blend");
    anim::blend_animation_samples(out, nums({0, 10}), nums({10, 20}), 2);
    check(equals(out, {10, 20}), "alpha clamps to 1");
  }
  {
    auto out = buf({0, 0, 0, 0});
    anim::blend_animation_samples(out, nums({0, 0, 0, 1}), nums({0, 0, -1, 0}), 0.5, true);
    check(close(std::abs(at(out, 2)), kSqrtHalf) && close(std::abs(at(out, 3)), kSqrtHalf), "quaternion blend slerps the shortest arc");
  }
}

void crossfade_timing() {
  {
    auto t = target();
    auto from = player(t, {0, 10});
    auto to = player(t, {20, 30});
    auto state = anim::create_animation_crossfade(from, to, 2);
    anim::advance_animation_crossfade(state, 0.5);
    check(from->time == 0.5 && to->time == 0.5 && state->elapsed == 0.5 && state->weight == 0.25,
          "both players advance together and the weight is linear");
  }
  {
    auto t = target();
    auto opts = flight::make_ref<flight::types::AnimationCrossfadeOptions>();
    opts->curve = flight::types::EasingFunction([](double x) { return x * x; });
    auto state = anim::create_animation_crossfade(player(t, {0, 1}), player(t, {1, 2}), 1, view(opts));
    anim::advance_animation_crossfade(state, 0.5);
    check(state->weight == 0.25, "the curve shapes normalized time");
    anim::advance_animation_crossfade(state, 2);
    check(state->weight == 1, "normalized time clamps before the curve");
  }
  {
    auto t = target();
    auto state = anim::create_animation_crossfade(player(t, {0, 1}), player(t, {1, 2}), 0);
    check(state->weight == 1 && anim::is_animation_crossfade_complete(view(state)), "a zero-duration crossfade completes at once");
  }
  {
    auto t = target();
    auto state = anim::create_animation_crossfade(player(t, {0, 1}), player(t, {1, 2}), 2);
    check(state->elapsed == 0 && state->weight == 0 && state->from_sample.size() >= 1 && state->to_sample.size() >= 1,
          "a new crossfade starts at zero with owned scratch");
    check(!anim::is_animation_crossfade_complete(view(state)), "not complete at the start");
    anim::advance_animation_crossfade(state, 2);
    check(anim::is_animation_crossfade_complete(view(state)), "complete once elapsed reaches the duration");
  }
  {
    auto t = target();
    auto opts = flight::make_ref<flight::types::AnimationCrossfadeOptions>();
    opts->curve = flight::types::EasingFunction([](double x) { return 1 + x; });
    auto state = anim::create_animation_crossfade(player(t, {0, 1}), player(t, {1, 2}), 1, view(opts));
    anim::advance_animation_crossfade(state, 0.7);
    check(state->weight > 1 && !anim::is_animation_crossfade_complete(view(state)), "an overshooting curve does not retire early");
    anim::advance_animation_crossfade(state, 0.31);
    check(anim::is_animation_crossfade_complete(view(state)), "it retires on elapsed time");
  }
}

void crossfade_matching() {
  auto shared = target(), fromOnly = target(), toOnly = target();
  auto from = clipPlayer({{shared, 2}, {fromOnly, 7}});
  auto to = clipPlayer({{toOnly, 9}, {shared, 6}});
  auto state = anim::create_animation_crossfade(from, to, 2);
  check(state->channels.size() == 3, "matched and one-sided channels are all retained");
  auto c0 = state->channels.element(0.0), c1 = state->channels.element(1.0), c2 = state->channels.element(2.0);
  check(c0->from_index == 0.0 && c0->to_index == 1.0 && c1->from_index == 1.0 && !c1->to_index.has_value() &&
            !c2->from_index.has_value() && c2->to_index == 0.0,
        "channels match by target, not position");

  anim::advance_animation_crossfade(state, 1);
  std::vector<std::pair<double, double>> seen;
  std::vector<flight::Any> targets;
  using Channel = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::AnimationChannel>>>>;
  anim::sample_animation_crossfade(buf({0}), view(state), [&](Buffer sampled, Channel channel, double index) {
    seen.push_back({index, at(sampled, 0)});
    targets.push_back(flight::row_get<flight::RowKey<"targetRef">>(channel));
  });
  check(seen == std::vector<std::pair<double, double>>{{0, 4}, {1, 7}, {2, 9}}, "sampling visits in stable order with blended values");
  check(targets.size() == 3 && targets[0] == shared && targets[1] == fromOnly && targets[2] == toOnly, "each visit names its target");
}

void crossfade_sampling() {
  {
    auto t = target();
    auto state = anim::create_animation_crossfade(player(t, {0, 10}), player(t, {20, 40}), 2);
    anim::advance_animation_crossfade(state, 0.5);
    std::vector<double> seen;
    auto record = [&](Buffer sampled, auto, double) { seen.push_back(at(sampled, 0)); };
    anim::sample_animation_crossfade(buf({0}), view(state), record);
    anim::sample_animation_crossfade(Buffer{state->from_sample}, view(state), record);
    anim::sample_animation_crossfade(Buffer{state->to_sample}, view(state), record);
    check(seen.size() == 3 && close(seen[0], 11.25) && close(seen[1], 11.25) && close(seen[2], 11.25),
          "blends through a distinct output and when aliasing either scratch buffer");
  }
  {
    auto t = target();
    auto state = anim::create_animation_crossfade(player(t, {0, 10}), player(t, {20, 40}), 1);
    auto fromSample = state->from_sample, toSample = state->to_sample;
    std::vector<double> values;
    auto record = [&](Buffer sampled, auto, double) { values.push_back(at(sampled, 0)); };
    anim::sample_animation_crossfade(buf({0}), view(state), record);
    anim::advance_animation_crossfade(state, 2);
    anim::sample_animation_crossfade(buf({0}), view(state), record);
    check(values == std::vector<double>{0, 20}, "alpha endpoints are exact");
    check(state->from_sample == fromSample && state->to_sample == toSample, "scratch buffers are reused");
  }
  {
    auto t = target();
    auto from = player(t, {0, 0, 0, 1, 0, 0, 0, 1}, 4, true);
    auto to = player(t, {0, 0, -1, 0, 0, 0, -1, 0}, 4, true);
    auto state = anim::create_animation_crossfade(from, to, 1);
    anim::advance_animation_crossfade(state, 0.5);
    bool ok = false;
    anim::sample_animation_crossfade(buf({0, 0, 0, 0}), view(state), [&](Buffer s, auto, double) {
      ok = close(std::abs(at(s, 2)), kSqrtHalf) && close(std::abs(at(s, 3)), kSqrtHalf);
    });
    check(ok, "crossfade slerps quaternions over the shortest arc");
  }
}

void crossfade_rejection() {
  auto t = target();
  check(thrownMessage([&] { anim::create_animation_crossfade(player(t, {0, 1}), player(t, {0, 0, 1, 1}, 2), 1); })
                .find("different component widths") != std::string::npos,
        "rejects tracks of different widths on one target");
  check(thrownMessage([&] {
          anim::create_animation_crossfade(player(t, {0, 0, 0, 1, 0, 0, 0, 1}, 4, true), player(t, {0, 0, 0, 1, 0, 0, 0, 1}, 4), 1);
        }).find("incompatible quaternion flags") != std::string::npos,
        "rejects mismatched quaternion flags on one target");

  auto duplicate = target(), unique = target();
  auto one = track(flight::Array<double>{0.0}, flight::Array<double>{1.0});
  auto clipOf = [&](flight::Any a, flight::Any b) {
    flight::Array<flight::Ref<flight::types::AnimationChannel>> channels;
    channels.push(anim::create_animation_channel(one, a));
    channels.push(anim::create_animation_channel(one, b));
    return anim::create_animation_player(anim::create_animation_clip(channels));
  };
  check(thrownMessage([&] { anim::create_animation_crossfade(clipOf(duplicate, duplicate), clipOf(duplicate, unique), 1); })
                .find("source clip contains a duplicate targetRef") != std::string::npos,
        "rejects a duplicate target in the source clip");
  check(thrownMessage([&] { anim::create_animation_crossfade(clipOf(duplicate, unique), clipOf(duplicate, duplicate), 1); })
                .find("destination clip contains a duplicate targetRef") != std::string::npos,
        "rejects a duplicate target in the destination clip");
}

}  // namespace

int main() {
  accumulation();
  blending();
  crossfade_timing();
  crossfade_matching();
  crossfade_sampling();
  crossfade_rejection();
  return failures == 0 ? 0 : 1;
}
