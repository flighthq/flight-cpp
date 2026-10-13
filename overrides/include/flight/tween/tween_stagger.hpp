// Re-derived from the generated tween stagger module at a627849. createTweenStagger is carried verbatim, and
// computeStaggerDelay, which the emitter refused, is supplied in its place. See overrides/manifest.json
// (tween-stagger-origin-selection) for what changed, why, and what remains.
#pragma once
#include <cmath>
#include <flight/math.hpp>
#include <flight/structural_ref.hpp>
#include <functional>
#include <optional>
#include <utility>
#include <variant>
#include <flight/runtime.hpp>

static_assert(flight::runtime_contract.compiler_contract == "flight-runtime-contract/2", "Flight compiler/runtime contract mismatch");
static_assert(flight::runtime_contract.cpp_abi == 1, "Flight C++ runtime ABI mismatch");

namespace flight::types { struct TweenManager; }
namespace flight::types { struct TweenOptions; }
namespace flight::types { struct TweenPropertyDetail; }
namespace flight::types { struct TweenStaggerOptions; }
namespace flight::types { template <typename T> struct Signal; }
namespace flight::types { template <typename T> struct Tween; }

#include <flight/types/easing_function.hpp>
#include <flight/types/signal.hpp>
#include <flight/types/tween.hpp>
#include <flight/types/tween_manager.hpp>
#include <flight/types/tween_options.hpp>
#include <flight/types/tween_property_detail.hpp>
#include <flight/types/tween_stagger_options.hpp>
#include <flight/tween/tween.hpp>

namespace flight::tween {

using flight::types::Tween;


// OVERRIDE: computeStaggerDelay, refused at the `from` comparisons
// (docs/compiler-requests/narrowed-member-multi-alternative-selection.md).
//
// The source it stands for:
//   function computeStaggerDelay(index, count, each, from: 'center' | 'end' | 'start' | number, staggerEase?): number {
//     if (count <= 1) return 0;
//     let normalizedPosition: number;
//     if (from === 'start') normalizedPosition = index / (count - 1);
//     else if (from === 'end') normalizedPosition = (count - 1 - index) / (count - 1);
//     else if (from === 'center') { const center = (count - 1) / 2; normalizedPosition = Math.abs(index - center) / center; }
//     else {
//       const origin = Math.max(0, Math.min(from, count - 1));
//       const maxDistance = Math.max(origin, count - 1 - origin);
//       normalizedPosition = maxDistance > 0 ? Math.abs(index - origin) / maxDistance : 0;
//     }
//     const eased = staggerEase !== undefined ? staggerEase(normalizedPosition) : normalizedPosition;
//     return eased * each * (count - 1);
//   }
//
// `from` is the emitted `std::variant<double, flight::String>`. Each string comparison is true only for
// the string alternative holding that literal, and the final branch is the numeric alternative, which is
// the only member of the declared union left once the three literals are excluded.
inline double compute_stagger_delay(double index, double count, double each, std::variant<double, flight::String> from, std::optional<flight::types::EasingFunction> stagger_ease = std::nullopt) {
  if ((count <= 1.0)) {
    return 0.0;
  }
  const auto from_is = [&](const char* literal) {
    const auto* text = std::get_if<flight::String>(&from);
    return text != nullptr && *text == flight::String(literal);
  };
  double normalized_position;
  if (from_is("start")) {
    (normalized_position = (index / (count - 1.0)));
  }
  else if (from_is("end")) {
    (normalized_position = (((count - 1.0) - index) / (count - 1.0)));
  }
  else if (from_is("center")) {
    const double center = ((count - 1.0) / 2.0);
    (normalized_position = (std::abs((index - center)) / center));
  }
  else {
    const double origin = flight::maximum(0.0, flight::minimum(std::get<double>(from), (count - 1.0)));
    const double max_distance = flight::maximum(origin, ((count - 1.0) - origin));
    (normalized_position = ((max_distance > 0.0) ? (std::abs((index - origin)) / max_distance) : 0.0));
  }
  const double eased = (stagger_ease.has_value() ? stagger_ease.value()(normalized_position) : normalized_position);
  return ((eased * each) * (count - 1.0));
}

template <typename T>
inline flight::Array<flight::Ref<flight::types::Tween<T>>> create_tween_stagger(flight::Ref<flight::types::TweenManager> manager, flight::Array<T> targets, double duration, flight::types::NumericProps<T> property_map, std::optional<flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::TweenStaggerOptions>>>>> stagger = std::nullopt, std::optional<flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::TweenOptions>>>>> options = std::nullopt) {
  if ((static_cast<double>(targets.size()) == 0.0)) {
    return flight::Array<flight::Ref<flight::types::Tween<T>>>{};
  }
  const double each = ([&]() -> double { auto nullish_coalesce_left = ([&]() -> std::optional<double> { auto optional_chain_receiver = stagger; if (!optional_chain_receiver.has_value()) return std::nullopt; return flight::row_get<flight::RowKey<"each">>(optional_chain_receiver.value()); }()); if (nullish_coalesce_left.has_value()) return nullish_coalesce_left.value(); return 0.1; }());
  const std::variant<double, flight::String> from = ([&]() -> std::optional<std::variant<double, flight::String>> { auto optional_chain_receiver = stagger; if (!optional_chain_receiver.has_value()) return std::nullopt; return flight::row_get<flight::RowKey<"from">>(optional_chain_receiver.value()); }()).value_or(std::variant<double, flight::String>{std::in_place_type<flight::String>, flight::String("start")});
  std::optional<std::function<double(double)>> stagger_ease = ([&]() -> std::optional<flight::types::EasingFunction> { auto optional_chain_receiver = stagger; if (!optional_chain_receiver.has_value()) return std::nullopt; return flight::row_get<flight::RowKey<"staggerEase">>(optional_chain_receiver.value()); }());
  const double base_delay = ([&]() -> double { auto nullish_coalesce_left = ([&]() -> std::optional<double> { auto optional_chain_receiver = options; if (!optional_chain_receiver.has_value()) return std::nullopt; return flight::row_get<flight::RowKey<"delay">>(optional_chain_receiver.value()); }()); if (nullish_coalesce_left.has_value()) return nullish_coalesce_left.value(); return 0.0; }());
  const double count = static_cast<double>(targets.size());
  flight::Array<flight::Ref<flight::types::Tween<T>>> tweens = flight::Array<flight::Ref<flight::types::Tween<T>>>{};
  {
    double i = 0.0;
    while ((i < count)) {
      {
        const double stagger_offset = compute_stagger_delay(i, count, each, from, stagger_ease);
        auto tween = flight::tween::create_tween<T>(std::variant<flight::Ref<flight::types::TweenManager>, T>{std::in_place_type<flight::Ref<flight::types::TweenManager>>, manager}, targets.element(i), std::variant<flight::types::NumericProps<T>, double>{std::in_place_type<double>, duration}, property_map, ([&]() { auto&& structural_spread_source = options; if (structural_spread_source.has_value()) static_cast<void>(structural_spread_source.value()->delay); std::optional<std::function<double(double)>> structural_spread_field_ease; if (structural_spread_source.has_value()) structural_spread_field_ease = structural_spread_source.value()->ease; std::optional<bool> structural_spread_field_overwrite; if (structural_spread_source.has_value()) structural_spread_field_overwrite = structural_spread_source.value()->overwrite; std::optional<bool> structural_spread_field_reflect; if (structural_spread_source.has_value()) structural_spread_field_reflect = structural_spread_source.value()->reflect; std::optional<double> structural_spread_field_repeat; if (structural_spread_source.has_value()) structural_spread_field_repeat = structural_spread_source.value()->repeat; std::optional<bool> structural_spread_field_reverse; if (structural_spread_source.has_value()) structural_spread_field_reverse = structural_spread_source.value()->reverse; std::optional<bool> structural_spread_field_smart_rotation; if (structural_spread_source.has_value()) structural_spread_field_smart_rotation = structural_spread_source.value()->smart_rotation; std::optional<bool> structural_spread_field_snapping; if (structural_spread_source.has_value()) structural_spread_field_snapping = structural_spread_source.value()->snapping; auto structural_spread_field_delay = (base_delay + stagger_offset); return flight::make_structural_ref<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::TweenOptions>>>>(flight::row_field<flight::RowKey<"delay">>(std::move(structural_spread_field_delay)), flight::row_field<flight::RowKey<"ease">>(std::move(structural_spread_field_ease)), flight::row_field<flight::RowKey<"overwrite">>(std::move(structural_spread_field_overwrite)), flight::row_field<flight::RowKey<"reflect">>(std::move(structural_spread_field_reflect)), flight::row_field<flight::RowKey<"repeat">>(std::move(structural_spread_field_repeat)), flight::row_field<flight::RowKey<"reverse">>(std::move(structural_spread_field_reverse)), flight::row_field<flight::RowKey<"smartRotation">>(std::move(structural_spread_field_smart_rotation)), flight::row_field<flight::RowKey<"snapping">>(std::move(structural_spread_field_snapping))); }()));
        tweens.push(tween);
      }
      (i += 1.0);
    }
  }
  return tweens;
}

} // namespace flight::tween