#include <flight/effects/effect_padding.hpp>
#include <flight/types/blur_effect.hpp>

#include <cmath>
#include <iostream>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

// Mutation evidence (2026-10-13): the focused test killed replacing the
// immutable resolver-map clone with an aliasing copy and disabling duplicate-
// missing-kind suppression.

namespace {

// The emitted module names the current owners directly, so these are local spellings of the same
// types rather than the retired flight::effects aliases the old override defined. Keeping them in
// flight::effects would assert a surface no emitted header declares.
using Effect = flight::types::Effect;
using EffectView = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::Effect>>>>;
using EffectInput = std::variant<flight::Array<EffectView>, EffectView>;
using Padding = flight::types::EffectPadding;
using PaddingResolver = flight::types::EffectPaddingResolver;
using PaddingView = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::EffectPadding>>>>;
using RenderState = flight::types::RenderState;
using RenderStateRuntime = flight::types::RenderStateRuntime;

bool check(bool condition, const char* message) {
  if (!condition) std::cerr << message << '\n';
  return condition;
}

flight::Ref<RenderState> make_state() {
  auto runtime = flight::make_ref<RenderStateRuntime>();
  runtime->registry_miss = std::nullopt;
  runtime->registries = flight::make_ref<flight::types::RenderRegistries>();

  auto state = flight::make_ref<RenderState>();
  state->entity_runtime_key = runtime;
  return state;
}

flight::Ref<Effect> make_effect(const char* kind) {
  auto effect = flight::make_ref<Effect>();
  effect->kind = flight::String(kind);
  return effect;
}

EffectView view(const flight::Ref<Effect>& effect) { return EffectView(effect); }

flight::Ref<Padding> make_padding(double bottom, double left, double right,
                                  double top) {
  return flight::make_ref<Padding>(Padding{
      .bottom = bottom,
      .left = left,
      .right = right,
      .top = top,
  });
}

PaddingResolver constant_padding(double bottom, double left, double right,
                                 double top) {
  return [=](EffectView) {
    return PaddingView(make_padding(bottom, left, right, top));
  };
}

bool padding_is(const flight::Ref<Padding>& padding, double bottom, double left,
                double right, double top) {
  return padding->bottom == bottom && padding->left == left &&
         padding->right == right && padding->top == top;
}

bool padding_is(const PaddingView& padding, double bottom, double left,
                double right, double top) {
  return flight::row_get<flight::RowKey<"bottom">>(padding) == bottom &&
         flight::row_get<flight::RowKey<"left">>(padding) == left &&
         flight::row_get<flight::RowKey<"right">>(padding) == right &&
         flight::row_get<flight::RowKey<"top">>(padding) == top;
}

EffectInput single(const flight::Ref<Effect>& effect) {
  return EffectInput{std::in_place_type<EffectView>, view(effect)};
}

EffectInput list(std::initializer_list<flight::Ref<Effect>> effects) {
  flight::Array<EffectView> views;
  for (const auto& effect : effects) views.push(view(effect));
  return EffectInput{std::in_place_type<flight::Array<EffectView>>,
                     std::move(views)};
}

void install_miss_emitter(
    const flight::Ref<RenderState>& state,
    std::vector<std::pair<flight::types::RenderRegistryTable, flight::String>>&
        misses) {
  using Emitter = flight::types::callable_clear_signals_682a2ad3a9f4953f;
  auto emitter = flight::make_ref<Emitter>();
  emitter->callable = [&misses](flight::types::RenderRegistryTable registry,
                                flight::String kind) {
    misses.emplace_back(registry, std::move(kind));
  };
  emitter->clear = [] {};
  emitter->signals = flight::make_ref<flight::types::RenderRegistrySignals>();
  state->entity_runtime_key.value()->registry_miss = emitter;
}

}  // namespace

int main() {
  using flight::String;
  using flight::effects::compute_effect_padding;
  using flight::effects::explain_effect_padding;
  using flight::effects::get_directional_effect_padding;
  using flight::effects::get_gaussian_effect_padding;
  using flight::effects::register_effect_padding_resolver;

  const auto gaussian = get_gaussian_effect_padding(2.1, -4.0);
  const auto positive_direction =
      get_directional_effect_padding(2.0, 3.0, 4.25, 5.5);
  const auto negative_direction =
      get_directional_effect_padding(1.0, 1.0, -2.5, -4.25);
  const auto nan_gaussian = get_gaussian_effect_padding(
      std::numeric_limits<double>::quiet_NaN(), 1.0);
  if (!check(padding_is(gaussian, 0.0, 7.0, 7.0, 0.0),
             "Gaussian padding did not use sanitized three-sigma extents") ||
      !check(padding_is(positive_direction, 15.0, 6.0, 11.0, 9.0),
             "positive directional offsets reached the wrong sides") ||
      !check(padding_is(negative_direction, 3.0, 6.0, 3.0, 8.0),
             "negative directional offsets reached the wrong sides") ||
      !check(std::isnan(nan_gaussian->left) &&
                 std::isnan(nan_gaussian->right),
             "Gaussian padding lost Math.max NaN behavior")) {
    return 1;
  }

  const auto state = make_state();
  const auto other_state = make_state();
  const auto sample = make_effect("acme.Effect");
  const PaddingResolver first = constant_padding(1.0, 2.0, 3.0, 4.0);
  const PaddingResolver replacement = constant_padding(4.0, 3.0, 2.0, 1.0);
  register_effect_padding_resolver(
      state, String("acme.Effect"), std::optional<PaddingResolver>{first});
  const auto before =
      state->entity_runtime_key.value()->registries->effect_padding_resolvers;
  register_effect_padding_resolver(
      state, String("acme.Effect"),
      std::optional<PaddingResolver>{replacement});
  const auto after =
      state->entity_runtime_key.value()->registries->effect_padding_resolvers;
  if (!check(before.has_value() && after.has_value(),
             "resolver registration did not allocate a state-local table") ||
      !check(padding_is((*before->get(String("acme.Effect")))(view(sample)),
                        1.0, 2.0, 3.0, 4.0),
             "replacement mutated the earlier resolver-map snapshot") ||
      !check(padding_is((*after->get(String("acme.Effect")))(view(sample)),
                        4.0, 3.0, 2.0, 1.0),
             "replacement resolver was not installed") ||
      !check(!other_state->entity_runtime_key.value()
                  ->registries->effect_padding_resolvers.has_value(),
             "resolver registration leaked across render states")) {
    return 1;
  }

  register_effect_padding_resolver(state, String("acme.Effect"),
                                          std::nullopt);
  const auto removed =
      state->entity_runtime_key.value()->registries->effect_padding_resolvers;
  if (!check(removed.has_value() &&
                 !removed->has(String("acme.Effect")),
             "resolver removal left the active entry bound") ||
      !check(before->has(String("acme.Effect")),
             "resolver removal mutated an earlier map snapshot")) {
    return 1;
  }

  const auto chain_state = make_state();
  const auto spatial_a = make_effect("acme.Spatial");
  const auto pointwise = make_effect("acme.Pointwise");
  const auto spatial_b = make_effect("acme.Spatial");
  std::vector<const void*> seen_effects;
  PaddingResolver spatial_resolver = [&seen_effects](EffectView effect) {
    seen_effects.push_back(effect.identity());
    return PaddingView(make_padding(1.0, 2.0, 3.0, 4.0));
  };
  register_effect_padding_resolver(
      chain_state, String("acme.Spatial"),
      std::optional<PaddingResolver>{spatial_resolver});
  register_effect_padding_resolver(
      chain_state, String("acme.Pointwise"),
      std::optional<PaddingResolver>{constant_padding(0.0, 0.0, 0.0, 0.0)});
  const auto chain = list({spatial_a, pointwise, spatial_b});
  const auto chain_padding = compute_effect_padding(chain_state, chain);
  if (!check(padding_is(chain_padding, 2.0, 4.0, 6.0, 8.0),
             "sequential padding did not add every side") ||
      !check(seen_effects.size() == 2 &&
                 seen_effects[0] == view(spatial_a).identity() &&
                 seen_effects[1] == view(spatial_b).identity(),
             "resolver did not receive each exact retained effect owner")) {
    return 1;
  }

  const auto caller_out = make_padding(-1.0, -1.0, -1.0, -1.0);
  const auto caller_identity = caller_out.get();
  const auto returned =
      compute_effect_padding(chain_state, chain, caller_out);
  if (!check(returned.get() == caller_identity &&
                 padding_is(caller_out, 2.0, 4.0, 6.0, 8.0),
             "caller-owned output lost identity or final values")) {
    return 1;
  }

  const auto concrete_state = make_state();
  auto concrete_blur = flight::make_ref<flight::types::BlurEffect>();
  concrete_blur->kind = String("BlurEffect");
  concrete_blur->blur_x = 2.0;
  concrete_blur->blur_y = 3.0;
  using ConcreteBlurView = flight::StructuralRef<flight::RowWritable<
      flight::RowOf<flight::Ref<flight::types::BlurEffect>>>>;
  const EffectView widened_blur = ConcreteBlurView(concrete_blur);
  bool recovered_exact_blur = false;
  PaddingResolver concrete_resolver =
      [&recovered_exact_blur, concrete_blur](EffectView effect) {
        const auto recovered =
            effect.shared_exact_object<flight::types::BlurEffect>();
        recovered_exact_blur = recovered.get() == concrete_blur.get();
        return PaddingView(make_padding(9.0, 6.0, 6.0, 9.0));
      };
  register_effect_padding_resolver(
      concrete_state, String("BlurEffect"),
      std::optional<PaddingResolver>{concrete_resolver});
  const auto concrete_padding = compute_effect_padding(
      concrete_state,
      EffectInput{std::in_place_type<EffectView>, widened_blur});
  if (!check(recovered_exact_blur &&
                 padding_is(concrete_padding, 9.0, 6.0, 6.0, 9.0),
             "base Effect view did not retain the exact concrete owner")) {
    return 1;
  }

  const auto explain_state = make_state();
  const auto missing_a = make_effect("acme.MissingA");
  const auto known = make_effect("acme.Known");
  const auto missing_a_again = make_effect("acme.MissingA");
  const auto missing_b = make_effect("acme.MissingB");
  register_effect_padding_resolver(
      explain_state, String("acme.Known"),
      std::optional<PaddingResolver>{constant_padding(3.0, 4.0, 5.0, 6.0)});
  const auto mixed = list({missing_a, known, missing_a_again, missing_b});
  const auto explanation = explain_effect_padding(explain_state, mixed);
  if (!check(explanation->missing_kinds.size() == 2 &&
                 explanation->missing_kinds.element(0.0) ==
                     String("acme.MissingA") &&
                 explanation->missing_kinds.element(1.0) ==
                     String("acme.MissingB"),
             "explanation lost first-seen missing-kind order or deduplication") ||
      !check(explanation->status == String("missing-resolver") &&
                 padding_is(explanation->padding, 3.0, 4.0, 5.0, 6.0),
             "explanation lost resolved padding or missing status")) {
    return 1;
  }

  std::vector<std::pair<flight::types::RenderRegistryTable, String>> misses;
  install_miss_emitter(explain_state, misses);
  const auto emitted_padding =
      compute_effect_padding(explain_state, mixed);
  if (!check(padding_is(emitted_padding, 3.0, 4.0, 5.0, 6.0),
             "miss emission changed resolved padding") ||
      !check(misses.size() == 2 &&
                 misses[0].first == flight::types::RenderRegistryTable::EffectPaddingResolver &&
                 misses[0].second == String("acme.MissingA") &&
                 misses[1].second == String("acme.MissingB"),
             "compute did not emit one ordered shared miss per kind")) {
    return 1;
  }
  static_cast<void>(explain_effect_padding(explain_state, mixed));
  if (!check(misses.size() == 2,
             "explain unexpectedly emitted through the shared miss seam")) {
    return 1;
  }

  const auto emitted_out = make_padding(9.0, 9.0, 9.0, 9.0);
  compute_effect_padding(explain_state, mixed, emitted_out);
  if (!check(misses.size() == 4 &&
                 misses[2].second == String("acme.MissingA") &&
                 misses[3].second == String("acme.MissingB"),
             "caller-output compute did not emit ordered unique misses")) {
    return 1;
  }

  const auto empty_explanation = explain_effect_padding(
      explain_state,
      EffectInput{std::in_place_type<flight::Array<EffectView>>,
                  flight::Array<EffectView>{}});
  if (!check(empty_explanation->status == String("complete") &&
                 empty_explanation->missing_kinds.empty() &&
                 padding_is(empty_explanation->padding, 0.0, 0.0, 0.0, 0.0),
             "empty chain did not explain as complete zero padding")) {
    return 1;
  }

  const auto sanitize_state = make_state();
  const auto dirty = make_effect("acme.Dirty");
  register_effect_padding_resolver(
      sanitize_state, String("acme.Dirty"),
      std::optional<PaddingResolver>{constant_padding(
          -1.0, std::numeric_limits<double>::infinity(),
          std::numeric_limits<double>::quiet_NaN(), 7.0)});
  if (!check(padding_is(compute_effect_padding(sanitize_state,
                                                       single(dirty)),
                        0.0, 0.0, 0.0, 7.0),
             "non-finite or negative resolver padding was not sanitized")) {
    return 1;
  }

  const auto snapshot_state = make_state();
  const auto mutator = make_effect("acme.Mutator");
  const auto target = make_effect("acme.Target");
  PaddingResolver mutate_registry = [snapshot_state](EffectView) {
    register_effect_padding_resolver(
        snapshot_state, String("acme.Target"),
        std::optional<PaddingResolver>{constant_padding(50.0, 50.0, 50.0,
                                                        50.0)});
    return PaddingView(make_padding(1.0, 1.0, 1.0, 1.0));
  };
  register_effect_padding_resolver(
      snapshot_state, String("acme.Mutator"),
      std::optional<PaddingResolver>{mutate_registry});
  register_effect_padding_resolver(
      snapshot_state, String("acme.Target"),
      std::optional<PaddingResolver>{constant_padding(2.0, 2.0, 2.0, 2.0)});
  if (!check(padding_is(compute_effect_padding(
                            snapshot_state, list({mutator, target})),
                        3.0, 3.0, 3.0, 3.0),
             "mid-chain registration mutated the resolver snapshot") ||
      !check(padding_is(compute_effect_padding(snapshot_state,
                                                       single(target)),
                        50.0, 50.0, 50.0, 50.0),
             "mid-chain replacement was not visible to a later compute")) {
    return 1;
  }

  const auto throwing_state = make_state();
  const auto throwing_effect = make_effect("acme.Throw");
  PaddingResolver throwing_resolver = [](EffectView) -> PaddingView {
    throw flight::Error(String("resolver failed"));
  };
  register_effect_padding_resolver(
      throwing_state, String("acme.Throw"),
      std::optional<PaddingResolver>{throwing_resolver});
  const auto unchanged = make_padding(9.0, 8.0, 7.0, 6.0);
  bool threw = false;
  try {
    static_cast<void>(compute_effect_padding(
        throwing_state, single(throwing_effect), unchanged));
  } catch (const flight::Error& error) {
    threw = error.message() == String("resolver failed");
  }
  if (!check(threw && padding_is(unchanged, 9.0, 8.0, 7.0, 6.0),
             "resolver exception did not propagate before output mutation")) {
    return 1;
  }

  return 0;
}
