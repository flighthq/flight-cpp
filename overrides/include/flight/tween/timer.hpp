// Re-derived from the generated tween timer module at a627849, which emitted nothing but the refusal of its one
// function; this supplies it. See overrides/manifest.json (tween-timer-empty-object-target) for what changed,
// why, and what remains.
#pragma once
#include <flight/any.hpp>
#include <flight/structural_ref.hpp>
#include <optional>
#include <variant>
#include <flight/runtime.hpp>

static_assert(flight::runtime_contract.compiler_contract == "flight-runtime-contract/2", "Flight compiler/runtime contract mismatch");
static_assert(flight::runtime_contract.cpp_abi == 1, "Flight C++ runtime ABI mismatch");

#include <flight/types/tween.hpp>
#include <flight/tween/tween.hpp>

namespace flight::types { struct TweenManager; }
namespace flight::types { struct TweenOptions; }
namespace flight::types { template <typename T> struct Tween; }

namespace flight::tween {

using flight::types::Tween;


// OVERRIDE: createTweenTimer, refused at the `{}` arguments
// (docs/compiler-requests/source-overload-call-through-selected-signature.md).
//
// The source it stands for:
//   export function createTweenTimer(manager: TweenManager, duration: number, options?: Readonly<TweenOptions>): Tween<object> {
//     return createTween(manager, {}, duration, {}, options);
//   }
//
// Each `{}` is a fresh empty object: the target gives the timer an identity of its own in the manager's
// map, and the property map has no keys, so the tween runs its timing, repeat and signals and writes
// nothing. Both are fresh allocations of a type with no members, called through the manager overload of
// createTween, which is the overload the source's argument list selects.
struct TweenTimerObject : public flight::ReferenceEnabled {};

inline flight::Ref<flight::types::Tween<flight::Any>> create_tween_timer(flight::Ref<flight::types::TweenManager> manager, double duration, std::optional<flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::TweenOptions>>>>> options = std::nullopt) {
  using PropsOrOptions = std::variant<flight::types::NumericProps<flight::Any>, flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::TweenOptions>>>>>;
  return flight::tween::create_tween<flight::Any>(
      std::variant<flight::Ref<flight::types::TweenManager>, flight::Any>{std::in_place_type<flight::Ref<flight::types::TweenManager>>, manager},
      std::variant<flight::Any, double>{std::in_place_type<flight::Any>, flight::Any::object(flight::make_ref<TweenTimerObject>())},
      std::variant<flight::types::NumericProps<flight::Any>, double>{std::in_place_type<double>, duration},
      PropsOrOptions{std::in_place_index<0>, flight::Any::object(flight::make_ref<TweenTimerObject>())},
      options);
}

} // namespace flight::tween