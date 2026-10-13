// Derived from @flighthq/tween/packages/tween/src/colorTween.ts at a627849, which the emitter refused whole.
// See overrides/manifest.json (tween-color-tween-shared-record-target) for what it supplies, why, and what remains.
#pragma once
#include <flight/any.hpp>
#include <flight/erased_ref.hpp>
#include <flight/math.hpp>
#include <flight/record.hpp>
#include <flight/structural_ref.hpp>
#include <functional>
#include <optional>
#include <flight/runtime.hpp>

static_assert(flight::runtime_contract.compiler_contract == "flight-runtime-contract/2", "Flight compiler/runtime contract mismatch");
static_assert(flight::runtime_contract.cpp_abi == 1, "Flight C++ runtime ABI mismatch");

namespace flight::types { struct TweenManager; }
namespace flight::types { struct TweenOptions; }
namespace flight::types { template <typename T> struct Tween; }

#include <flight/signals/slot.hpp>
#include <flight/types/tween.hpp>
#include <flight/types/tween_manager.hpp>
#include <flight/types/tween_options.hpp>
#include <flight/tween/_internal_internal.hpp>

namespace flight::tween {

using flight::types::Tween;

// OVERRIDE: `type ColorComponents = { b: number; g: number; r: number }`.
//
// The tween machinery reads and writes its target through the object's named row view, and that view is
// bound from the SDK's member table, which is built from the declarations the emitter produced. This
// module was the only one to declare `g` and `r`, and it was refused, so the table has no entry for
// them. `color_components` binds the three members itself, on the object's one row owner, exactly as a
// table entry would; `bind_named` keeps an existing binding, so a regenerated table that does carry them
// changes nothing.
struct ColorComponents : public flight::ReferenceEnabled {
  double b;
  double g;
  double r;
};

namespace override_detail {

[[nodiscard]] inline flight::Ref<ColorComponents> color_components(double b, double g, double r) {
  auto components = flight::make_ref<ColorComponents>(ColorComponents{.b = b, .g = g, .r = r});
  const auto owner = flight::detail::owner_for(components);
  ColorComponents* fields = components.get();
  owner->bind_named("b", [fields]() -> decltype(auto) { return (fields->b); });
  owner->bind_named("g", [fields]() -> decltype(auto) { return (fields->g); });
  owner->bind_named("r", [fields]() -> decltype(auto) { return (fields->r); });
  return components;
}

} // namespace override_detail

// OVERRIDE: createColorTween, refused at the closure's write to `target`
// (docs/compiler-requests/captured-referent-mutation.md).
//
// The source it stands for:
//   export function createColorTween(manager, target: Record<string, number>, property, duration, toColor, options?) {
//     const fromColor = target[property] ?? 0;
//     const components: ColorComponents = { b: fromColor & 0xff, g: (fromColor >> 8) & 0xff, r: (fromColor >> 16) & 0xff };
//     const tween = makeTween(components, duration, { b: toColor & 0xff, g: (toColor >> 8) & 0xff, r: (toColor >> 16) & 0xff },
//                             options, manager.defaultEase);
//     addTweenToManager(manager, tween, options?.overwrite ?? true, target, [property]);
//     connectSignal(tween.onUpdate, () => {
//       target[property] = ((Math.round(components.r) & 0xff) << 16) | ((Math.round(components.g) & 0xff) << 8) |
//                          (Math.round(components.b) & 0xff);
//     });
//     return tween;
//   }
//
// The refusal is about the write's destination: it must land on the caller's record, not on a copy. A
// `flight::Record` is that shared reference already -- copies of one record share their entries, as two
// bindings of one JavaScript object do -- so the closure captures the record by value and writes the
// caller's own entries. The tween is registered under the record's identity, and reads `components`
// through the reference the tween's target holds, so the closure sees every value the update loop wrote.
//
// The returned tween is a `Tween<any>`, the instantiation the manager's list stores; a distinct
// `Tween<ColorComponents>` would be a second object that the manager never updates
// (docs/compiler-requests/generic-owner-into-widest-container.md). Its target is the components object.
inline flight::Ref<flight::types::Tween<flight::Any>> create_color_tween(flight::Ref<flight::types::TweenManager> manager, flight::Record<flight::String, double> target, flight::String property, double duration, double to_color, std::optional<flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::TweenOptions>>>>> options = std::nullopt) {
  const double from_color = target.get(property).value_or(0.0);
  auto components = override_detail::color_components(
      flight::bitwise_and(from_color, 255.0),
      flight::bitwise_and(flight::signed_right_shift(from_color, 8.0), 255.0),
      flight::bitwise_and(flight::signed_right_shift(from_color, 16.0), 255.0));
  auto to_components = override_detail::color_components(
      flight::bitwise_and(to_color, 255.0),
      flight::bitwise_and(flight::signed_right_shift(to_color, 8.0), 255.0),
      flight::bitwise_and(flight::signed_right_shift(to_color, 16.0), 255.0));
  auto tween = flight::tween::make_tween<flight::Any>(flight::Any::object(components), duration, flight::Any::object(to_components), options, manager->default_ease);
  const bool overwrite = ([&]() -> bool { auto nullish_coalesce_left = ([&]() -> std::optional<bool> { auto optional_chain_receiver = options; if (!optional_chain_receiver.has_value()) return std::nullopt; return flight::row_get<flight::RowKey<"overwrite">>(optional_chain_receiver.value()); }()); if (nullish_coalesce_left.has_value()) return nullish_coalesce_left.value(); return true; }());
  flight::tween::add_tween_to_manager<flight::Any>(manager, tween, overwrite, target.erased_ref(), flight::Array<flight::String>{property});
  flight::signals::connect_signal(tween->on_update, std::function<void()>([target, property, components]() mutable {
    target.set(property, flight::bitwise_or(
        flight::bitwise_or(flight::left_shift(flight::bitwise_and(flight::round(components->r), 255.0), 16.0),
                           flight::left_shift(flight::bitwise_and(flight::round(components->g), 255.0), 8.0)),
        flight::bitwise_and(flight::round(components->b), 255.0)));
  }), std::nullopt);
  return tween;
}

} // namespace flight::tween
