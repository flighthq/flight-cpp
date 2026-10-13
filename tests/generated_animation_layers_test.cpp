// The generated @flighthq/animation layer stack and root motion at a627849, exercised natively. Ported from
// packages/animation/src/animationLayerStack.test.ts and animationRootMotion.test.ts. The layer stack
// module carries six emission repairs (channel projections and array row writes), so these checks also
// certify that repaired code:
// - ordering: override masks and additive layers applied in stable target order, target correspondence
//   computed from only the selected source channels, and a transitioning state-machine layer sampled
//   at its blend weight;
// - lifecycle: layer construction and validation of channel subsets, shared players and sources
//   advanced once through reused scratch, layer weights, incompatible tracks rejected;
// - root motion: forward, reverse and multi-loop displacement, quaternion composition across loop
//   boundaries, identity for a zero-duration clip, undersized output left untouched, and non-finite
//   ranges rejected before any output is written.
#include <flight/animation/animation_blend_tree.hpp>
#include <flight/animation/animation_clip.hpp>
#include <flight/animation/animation_layer_stack.hpp>
#include <flight/animation/animation_player.hpp>
#include <flight/animation/animation_root_motion.hpp>
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
const double kSqrtHalf = std::sqrt(0.5);

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

flight::Ref<flight::types::AnimationClip> clipOf(const flight::Ref<flight::types::AnimationTrack>& t, const flight::Any& ref) {
  flight::Array<flight::Ref<flight::types::AnimationChannel>> channels;
  channels.push(anim::create_animation_channel(t, ref));
  return anim::create_animation_clip(channels);
}

flight::Ref<AnimationPlayer> player(const flight::Any& t, double value, double duration = 0) {
  auto times = duration > 0 ? flight::Array<double>{0.0, duration} : flight::Array<double>{0.0};
  auto values = duration > 0 ? flight::Array<double>{value, value + 10} : flight::Array<double>{value};
  return anim::create_animation_player(clipOf(track(times, values), t));
}

flight::Ref<flight::types::AnimationBlendTree> tree(std::vector<flight::Ref<AnimationPlayer>> players) {
  flight::Array<flight::Ref<flight::types::AnimationBlendTreeInput>> inputs;
  for (const auto& p : players) inputs.push(anim::create_animation_blend_tree_input(p));
  return anim::create_animation_blend_tree(inputs);
}

flight::Ref<flight::types::AnimationLayer> treeLayer(const flight::Ref<flight::types::AnimationBlendTree>& t,
                                                     std::optional<bool> additive = std::nullopt,
                                                     std::optional<flight::Array<double>> channelIndices = std::nullopt,
                                                     std::optional<double> weight = std::nullopt) {
  auto opts = flight::make_ref<flight::types::AnimationLayerOptions>();
  opts->additive = additive;
  opts->channel_indices = channelIndices;
  opts->weight = weight;
  return anim::create_animation_blend_tree_layer(t, view(opts));
}

flight::Ref<flight::types::AnimationLayerStack> stackOf(std::vector<flight::Ref<flight::types::AnimationLayer>> layers) {
  flight::Array<flight::Ref<flight::types::AnimationLayer>> list;
  for (const auto& l : layers) list.push(l);
  return anim::create_animation_layer_stack(list);
}

flight::Ref<flight::types::AnimationStateMachine> machineOf(std::vector<std::pair<const char*, flight::Ref<flight::types::AnimationBlendTree>>> states) {
  flight::Array<flight::Ref<flight::types::AnimationStateMachineState>> list;
  for (const auto& [name, t] : states) list.push(anim::create_animation_state_machine_state(flight::String(name), t));
  return anim::create_animation_state_machine(list);
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

void layer_lifecycle() {
  {
    auto x = target();
    auto shared = player(x, 0, 4);
    auto sharedTree = tree({shared});
    auto machine = machineOf({{"only", tree({shared})}});
    auto stack = stackOf({treeLayer(sharedTree), treeLayer(sharedTree), anim::create_animation_state_machine_layer(machine)});
    auto scratch = stack->advance_scratch;
    anim::advance_animation_layer_stack(stack, 0.5);
    check(shared->time == 0.5, "a player shared by every layer advances once");
    check(stack->advance_scratch == scratch && stack->advance_scratch.size() == 1 && stack->advance_scratch.element(0.0) == shared,
          "players are de-duplicated in reused scratch");
    check(stack->blend_trees.size() == 1 && stack->blend_trees.element(0.0) == sharedTree && stack->state_machines.size() == 1 &&
              stack->state_machines.element(0.0) == machine,
          "sources are listed once each");
  }
  {
    auto source = tree({player(target(), 1), player(target(), 2)});
    auto layer = treeLayer(source, true, flight::Array<double>{1.0, 0.0}, 0.5);
    check(layer->blend_tree.has_value() && layer->blend_tree.value() == source && !layer->state_machine.has_value(), "a blend-tree layer");
    check(layer->channel_indices.has_value() && layer->channel_indices->size() == 2 && layer->channel_indices->element(0.0) == 0 &&
              layer->channel_indices->element(1.0) == 1,
          "the channel subset is stored sorted");
    check(layer->additive && layer->weight == 0.5, "options are applied");
    check(mentions(thrown([&] { treeLayer(source, std::nullopt, flight::Array<double>{0.0, 0.0}); }), "duplicated"),
          "a duplicated channel index is rejected");
    check(mentions(thrown([&] { treeLayer(source, std::nullopt, flight::Array<double>{2.0}); }), "does not exist"),
          "a missing channel index is rejected");
  }
  {
    auto source = machineOf({{"only", tree({player(target(), 1)})}});
    auto layer = anim::create_animation_state_machine_layer(source);
    check(!layer->blend_tree.has_value() && layer->state_machine.has_value() && layer->state_machine.value() == source,
          "a state-machine layer");
  }
  {
    auto stack = stackOf({treeLayer(tree({player(target(), 1)}))});
    check(anim::set_animation_layer_weight(stack, 0, 0.25) && stack->layers.element(0.0)->weight == 0.25, "set weight updates a layer");
    check(!anim::set_animation_layer_weight(stack, 2, 1), "set weight rejects an absent index");
  }
  {
    auto x = target();
    auto vector = anim::create_animation_player(clipOf(track({0.0}, {1.0, 2.0}, 2), x));
    check(mentions(thrown([&] { stackOf({treeLayer(tree({player(x, 1)})), treeLayer(tree({vector}))}); }), "incompatible tracks"),
          "incompatible tracks for one target are rejected");
  }
}

void layer_ordering() {
  {
    auto a = target(), b = target();
    auto stack = stackOf({treeLayer(tree({player(a, 1), player(b, 2)}), std::nullopt, flight::Array<double>{1.0})});
    check(stack->channels.size() == 1 && flight::row_get<flight::RowKey<"targetRef">>(stack->channels.element(0.0)->channel) == b,
          "correspondence uses only the selected channels");
  }
  {
    auto body = target(), arm = target();
    auto base = tree({player(body, 4), player(arm, 10)});
    auto upper = tree({player(body, 100), player(arm, 30)});
    auto additive = tree({player(arm, 8)});
    auto stack = stackOf({treeLayer(base), treeLayer(upper, std::nullopt, flight::Array<double>{1.0}, 0.5), treeLayer(additive, true, std::nullopt, 0.25)});
    std::vector<std::pair<flight::Any, double>> seen;
    anim::sample_animation_layer_stack(Buffer{flight::Array<double>{0.0}}, view(stack), [&](Buffer s, auto channel, double) {
      seen.push_back({flight::row_get<flight::RowKey<"targetRef">>(channel), at(s, 0)});
    });
    check(seen.size() == 2 && seen[0].first == body && seen[0].second == 4 && seen[1].first == arm && seen[1].second == 22,
          "an override mask and an additive layer apply in stable target order");
  }
  {
    auto x = target();
    auto machine = machineOf({{"from", tree({player(x, 0)})}, {"to", tree({player(x, 20)})}});
    anim::transition_animation_state_machine(machine, flight::String("to"), 2);
    machine->transition_elapsed = 1;
    machine->transition_weight = 0.5;
    std::vector<double> seen;
    anim::sample_animation_layer_stack(Buffer{flight::Array<double>{0.0}}, view(stackOf({anim::create_animation_state_machine_layer(machine)})),
                                       [&](Buffer s, auto, double) { seen.push_back(at(s, 0)); });
    check(seen == std::vector<double>{10}, "a transitioning state-machine layer samples at its blend weight");
  }
  {
    auto stack = stackOf({treeLayer(tree({player(target(), 8)}), true, std::nullopt, 0.25)});
    flight::Array<double> out{99};
    check(anim::sample_animation_layer_stack_channel(Buffer{out}, view(stack), 0) && out.element(0.0) == 2,
          "an additive-only target uses identity as its base");
    check(!anim::sample_animation_layer_stack_channel(Buffer{out}, view(stack), 4) && out.element(0.0) == 2,
          "an absent channel leaves the output alone");
  }
}

flight::Ref<flight::types::AnimationRootMotionExtractor> extractor(const flight::Ref<flight::types::AnimationTrack>& t) {
  return anim::create_animation_root_motion_extractor(view(clipOf(t, target())), 0);
}

void root_motion() {
  {
    auto t = track({0.0, 1.0}, {2.0, 6.0});
    auto clip = clipOf(t, target());
    auto e = anim::create_animation_root_motion_extractor(view(clip), 0);
    check(e->channel_index == 0 && e->cycle_delta[0] == 4, "the cycle delta is precomputed for the chosen channel");
    check(mentions(thrown([] { anim::create_animation_root_motion_extractor(view(anim::create_animation_clip({})), 0); }), "does not exist"),
          "a missing channel index is rejected");
    check(mentions(thrown([] { extractor(track({0.0}, {0.0, 0.0, 1.0}, 3, true)); }), "four components"),
          "a quaternion channel without four components is rejected");
  }
  {
    auto e = extractor(track({0.0, 1.0}, {0, 0, 0, 10, 2, -4}, 3));
    auto fromMotion = e->from_motion;
    flight::Float32Array out(3.0);
    check(anim::extract_animation_root_motion(Buffer{out}, e, 0.75, 1.25) && out[0] == 5 && out[1] == 1 && out[2] == -2, "forward displacement");
    check(anim::extract_animation_root_motion(Buffer{out}, e, 1.25, 0.75) && out[0] == -5 && out[1] == -1 && out[2] == 2, "reverse displacement");
    check(anim::extract_animation_root_motion(Buffer{out}, e, 0, 2.5) && out[0] == 25 && out[1] == 5 && out[2] == -10,
          "multi-loop displacement accumulates whole cycles");
    check(e->from_motion == fromMotion, "scratch is reused, not replaced");
  }
  {
    auto e = extractor(track({0.0, 1.0}, {0, 0, 0, 1, 0, 0, kSqrtHalf, kSqrtHalf}, 4, true));
    flight::Float32Array out(4.0);
    anim::extract_animation_root_motion(Buffer{out}, e, 0, 2);
    check(close(std::abs(out[2]), 1) && close(std::abs(out[3]), 0), "two quarter-turn cycles compose to a half turn");
    anim::extract_animation_root_motion(Buffer{out}, e, 1.5, 0.5);
    check(close(out[2], -kSqrtHalf) && close(out[3], kSqrtHalf), "a reverse range across a boundary composes the inverse");
  }
  {
    auto e = extractor(track({0.0}, {4.0, 8.0}, 2));
    flight::Array<double> out{9, 9};
    check(anim::extract_animation_root_motion(Buffer{out}, e, 0, 3) && out.element(0.0) == 0 && out.element(1.0) == 0,
          "a zero-duration clip writes identity");
    flight::Array<double> shortOut{7};
    check(!anim::extract_animation_root_motion(Buffer{shortOut}, e, 0, 1) && shortOut.element(0.0) == 7,
          "an undersized output is refused and left alone");
  }
  {
    auto e = extractor(track({0.0, 1.0}, {0, 0, 0, 1, 0, 0, kSqrtHalf, kSqrtHalf}, 4, true));
    flight::Float32Array out{1, 2, 3, 4};
    check(mentions(thrown([&] { anim::extract_animation_root_motion(Buffer{out}, e, 0, INFINITY); }), "finite numbers") &&
              mentions(thrown([&] { anim::extract_animation_root_motion(Buffer{out}, e, -INFINITY, 0); }), "finite numbers"),
          "non-finite ranges are rejected");
    check(out[0] == 1 && out[1] == 2 && out[2] == 3 && out[3] == 4, "rejection happens before the output is written");
  }
}

}  // namespace

int main() {
  layer_lifecycle();
  layer_ordering();
  root_motion();
  return failures == 0 ? 0 : 1;
}
