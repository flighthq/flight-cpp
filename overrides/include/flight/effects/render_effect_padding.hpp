// Derived from @flighthq/effects/packages/effects/src/renderEffectPadding.ts
// at 7e2fc7df5378313ceaf21ba8d6ef39cc18a6f4ad.
// Override: derivedFrom sha256 06d0ad44a5efb9bd94a436867128ceba7b5ba4ce00739b437c5a35a459d7db00
#pragma once

#include <cmath>
#include <cstddef>
#include <functional>
#include <optional>
#include <utility>
#include <variant>

#include <flight/array.hpp>
#include <flight/map.hpp>
#include <flight/number.hpp>
#include <flight/structural_ref.hpp>
#include <flight/types/effect.hpp>
#include <flight/types/effect_padding.hpp>
#include <flight/types/render_registry_signals.hpp>
#include <flight/types/render_state.hpp>

namespace flight::effects {

// The Effects package is still pinned before the RenderEffect -> Effect rename while
// Types already carries the new names. Keep this module's public spelling and retain
// the current native owners underneath it.
using RenderEffect = flight::types::Effect;
using RenderEffectPadding = flight::types::EffectPadding;
using RenderEffectPaddingExplanation = flight::types::EffectPaddingExplanation;
using RenderEffectPaddingResolver = flight::types::EffectPaddingResolver;
using RenderEffectView =
    flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<RenderEffect>>>>;
using RenderEffectPaddingView =
    flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<RenderEffectPadding>>>>;
using RenderEffectInput = std::variant<flight::Array<RenderEffectView>, RenderEffectView>;
using RenderRegistryMiss =
    std::function<void(flight::types::RenderRegistryTable, flight::String)>;

inline flight::Ref<flight::types::RenderStateRuntime> render_effect_padding_runtime(
    const flight::Ref<flight::types::RenderState>& state) {
  return state->entity_runtime_key.value();
}

inline std::optional<RenderRegistryMiss> render_effect_padding_miss_emitter(
    const flight::Ref<flight::types::RenderStateRuntime>& runtime) {
  const auto& slot = runtime->registry_miss;
  if (!slot.has_value()) return std::nullopt;
  const auto emitter = slot.value();
  return RenderRegistryMiss{[emitter](flight::types::RenderRegistryTable registry,
                                      flight::String kind) {
    (*emitter)(registry, std::move(kind));
  }};
}

inline flight::Ref<RenderEffectPadding> get_gaussian_render_effect_padding(
    double blur_x, double blur_y) {
  const double horizontal = std::ceil(flight::maximum(0.0, blur_x) * 3.0);
  const double vertical = std::ceil(flight::maximum(0.0, blur_y) * 3.0);
  return flight::make_ref<RenderEffectPadding>(RenderEffectPadding{
      .bottom = vertical,
      .left = horizontal,
      .right = horizontal,
      .top = vertical,
  });
}

inline flight::Ref<RenderEffectPadding> get_directional_render_effect_padding(
    double blur_x, double blur_y, double offset_x, double offset_y) {
  const auto gaussian = get_gaussian_render_effect_padding(blur_x, blur_y);
  const double dx = std::abs(offset_x) < 1e-10 ? 0.0 : offset_x;
  const double dy = std::abs(offset_y) < 1e-10 ? 0.0 : offset_y;
  return flight::make_ref<RenderEffectPadding>(RenderEffectPadding{
      .bottom = std::ceil(gaussian->bottom + flight::maximum(0.0, dy)),
      .left = std::ceil(gaussian->left + flight::maximum(0.0, -dx)),
      .right = std::ceil(gaussian->right + flight::maximum(0.0, dx)),
      .top = std::ceil(gaussian->top + flight::maximum(0.0, -dy)),
  });
}

inline void register_render_effect_padding_resolver(
    const flight::Ref<flight::types::RenderState>& state, flight::String kind,
    std::optional<RenderEffectPaddingResolver> resolver) {
  const auto runtime = render_effect_padding_runtime(state);
  const auto table = runtime->registries->effect_padding_resolvers;
  if (!resolver.has_value()) {
    if (table.has_value()) {
      auto replacement = table->clone();
      replacement.erase(kind);
      runtime->registries->effect_padding_resolvers = std::move(replacement);
    }
    return;
  }

  auto replacement = table.has_value()
                         ? table->clone()
                         : flight::Map<flight::String, RenderEffectPaddingResolver>{};
  replacement.set(std::move(kind), std::move(resolver).value());
  runtime->registries->effect_padding_resolvers = std::move(replacement);
}

inline bool has_earlier_kind(
    const std::optional<flight::Array<RenderEffectView>>& effects,
    std::size_t end, const flight::String& kind) {
  if (!effects.has_value()) return false;
  for (std::size_t index = 0; index < end; ++index) {
    if (flight::row_get<flight::RowKey<"kind">>(
            effects->element(static_cast<double>(index))) == kind) {
      return true;
    }
  }
  return false;
}

inline double sanitize_padding(double value) {
  return std::isfinite(value) ? flight::maximum(0.0, value) : 0.0;
}

inline void write_render_effect_padding(
    const flight::Ref<RenderEffectPadding>& out,
    const flight::Ref<flight::types::RenderState>& state,
    const RenderEffectInput& effects,
    std::optional<flight::Array<flight::String>> missing_kinds,
    std::optional<RenderRegistryMiss> emit_miss) {
  const std::optional<flight::Array<RenderEffectView>> list =
      std::holds_alternative<flight::Array<RenderEffectView>>(effects)
          ? std::optional<flight::Array<RenderEffectView>>{
                std::get<flight::Array<RenderEffectView>>(effects)}
          : std::nullopt;
  const std::size_t length = list.has_value() ? list->size() : 1U;
  const auto resolvers =
      render_effect_padding_runtime(state)->registries->effect_padding_resolvers;

  double bottom = 0.0;
  double left = 0.0;
  double right = 0.0;
  double top = 0.0;
  for (std::size_t index = 0; index < length; ++index) {
    const RenderEffectView effect =
        list.has_value() ? list->element(static_cast<double>(index))
                         : std::get<RenderEffectView>(effects);
    const flight::String kind =
        flight::row_get<flight::RowKey<"kind">>(effect);
    const auto resolver = resolvers.has_value() ? resolvers->get(kind) : std::nullopt;
    if (!resolver.has_value()) {
      if (!has_earlier_kind(list, index, kind)) {
        if (missing_kinds.has_value()) missing_kinds->push(kind);
        if (emit_miss.has_value()) {
          (*emit_miss)(flight::types::RenderRegistryTable::EffectPaddingResolver,
                       kind);
        }
      }
      continue;
    }

    // The row view keeps the original exact native owner. A resolver that knows a
    // concrete effect may validate and recover it; this module neither casts nor copies it.
    const RenderEffectPaddingView padding = (*resolver)(effect);
    bottom += sanitize_padding(
        flight::row_get<flight::RowKey<"bottom">>(padding));
    left += sanitize_padding(
        flight::row_get<flight::RowKey<"left">>(padding));
    right += sanitize_padding(
        flight::row_get<flight::RowKey<"right">>(padding));
    top += sanitize_padding(flight::row_get<flight::RowKey<"top">>(padding));
  }

  // Commit only after every resolver has returned. If one throws, the caller-owned
  // output remains unchanged just as it does before the source reaches these writes.
  out->bottom = bottom;
  out->left = left;
  out->right = right;
  out->top = top;
}

inline flight::Ref<RenderEffectPaddingExplanation> explain_render_effect_padding(
    const flight::Ref<flight::types::RenderState>& state,
    const RenderEffectInput& effects) {
  flight::Array<flight::String> missing_kinds;
  const auto padding = flight::make_ref<RenderEffectPadding>(
      RenderEffectPadding{.bottom = 0.0, .left = 0.0, .right = 0.0, .top = 0.0});
  write_render_effect_padding(padding, state, effects,
                              std::optional{missing_kinds}, std::nullopt);
  return flight::make_ref<RenderEffectPaddingExplanation>(
      RenderEffectPaddingExplanation{
          .missing_kinds = missing_kinds,
          .padding = RenderEffectPaddingView(padding),
          .status = missing_kinds.empty() ? flight::String("complete")
                                          : flight::String("missing-resolver"),
      });
}

inline flight::Ref<RenderEffectPadding> compute_render_effect_padding(
    const flight::Ref<flight::types::RenderState>& state,
    const RenderEffectInput& effects,
    std::optional<flight::Ref<RenderEffectPadding>> out = std::nullopt) {
  if (out.has_value()) {
    write_render_effect_padding(
        *out, state, effects, std::nullopt,
        render_effect_padding_miss_emitter(render_effect_padding_runtime(state)));
    return *out;
  }

  const flight::Array<RenderEffectView> list =
      std::holds_alternative<flight::Array<RenderEffectView>>(effects)
          ? std::get<flight::Array<RenderEffectView>>(effects)
          : flight::Array<RenderEffectView>{std::get<RenderEffectView>(effects)};
  const auto explanation = explain_render_effect_padding(
      state, RenderEffectInput{std::in_place_type<flight::Array<RenderEffectView>>,
                               list});
  const auto emit_miss =
      render_effect_padding_miss_emitter(render_effect_padding_runtime(state));
  if (emit_miss.has_value()) {
    for (const auto& kind : explanation->missing_kinds) {
      (*emit_miss)(flight::types::RenderRegistryTable::EffectPaddingResolver,
                   kind);
    }
  }

  const auto padding =
      explanation->padding.template shared_exact_object<RenderEffectPadding>();
  if (!padding) {
    throw flight::Error(flight::String(
        "computeRenderEffectPadding lost the exact padding owner"));
  }
  return padding;
}

}  // namespace flight::effects
