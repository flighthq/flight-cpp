// Derived from @flighthq/statechart/packages/statechart/src/enableStatechartGuards.ts.
#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <variant>

#include <flight/log/contract.hpp>
#include <flight/record.hpp>
#include <flight/runtime.hpp>
#include <flight/structural_ref.hpp>
#include <flight/types/log.hpp>
#include <flight/types/statechart.hpp>
#include <flight/weak_map.hpp>

static_assert(flight::runtime_contract.compiler_contract == "flight-runtime-contract/2",
              "Flight compiler/runtime contract mismatch");
static_assert(flight::runtime_contract.cpp_abi == 1,
              "Flight C++ runtime ABI mismatch");

namespace flight::statechart {

using ReadonlyStatechart = flight::StructuralRef<
    flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::Statechart>>>>;
using ReadonlyStatechartInstance = flight::StructuralRef<flight::RowReadonly<
    flight::RowOf<flight::Ref<flight::types::StatechartInstance>>>>;
using ReadonlyStatechartExplanation = flight::StructuralRef<flight::RowReadonly<
    flight::RowOf<flight::Ref<flight::types::StatechartTransitionExplanation>>>>;
using StatechartGuardFunction = void (*)(
    ReadonlyStatechartInstance, ReadonlyStatechartExplanation);

inline flight::WeakMap<std::shared_ptr<flight::RowOwner>, double>
    statechart_guard_chart_ids;
inline double next_statechart_guard_chart_id = 1.0;

inline double get_statechart_guard_chart_id(const ReadonlyStatechart& chart) {
  const auto owner = chart.shared_owner();
  auto id = statechart_guard_chart_ids.get(owner);
  if (!id.has_value()) {
    id = next_statechart_guard_chart_id++;
    statechart_guard_chart_ids.set(owner, id.value());
  }
  return id.value();
}

inline void warn_missing_statechart_region_duration(
    ReadonlyStatechartInstance instance,
    ReadonlyStatechartExplanation explanation) {
  const auto chart = flight::row_get<flight::RowKey<"chart">>(instance);
  const auto region_index =
      flight::row_get<flight::RowKey<"regionIndex">>(explanation);
  const auto source_state_index =
      flight::row_get<flight::RowKey<"sourceStateIndex">>(explanation);
  const auto transition_index =
      flight::row_get<flight::RowKey<"transitionIndex">>(explanation);

  double exit_time_ratio = -1.0;
  const auto region =
      flight::row_get<flight::RowKey<"regions">>(chart).get(region_index);
  if (region.has_value()) {
    const auto state =
        flight::row_get<flight::RowKey<"states">>(region.value())
            .get(source_state_index);
    if (state.has_value()) {
      const auto transition =
          flight::row_get<flight::RowKey<"transitions">>(state.value())
              .get(transition_index);
      if (transition.has_value()) {
        exit_time_ratio =
            flight::row_get<flight::RowKey<"exitTimeRatio">>(transition.value());
      }
    }
  }

  const auto duration =
      flight::row_get<flight::RowKey<"regionDuration">>(instance).get(region_index);
  const double region_duration = duration.value_or(-1.0);
  flight::log::log_once(
      flight::String("statechart:missing-region-duration:") +
          flight::to_string(get_statechart_guard_chart_id(chart)) + flight::String(":") +
          flight::to_string(region_index) + flight::String(":") +
          flight::to_string(source_state_index) + flight::String(":") +
          flight::to_string(transition_index),
      flight::types::LogLevel::Warn,
      flight::types::LogData{
          std::in_place_type<flight::Record<flight::String, flight::Any>>,
          flight::Record<flight::String, flight::Any>{
              {flight::String("exitTimeRatio"), exit_time_ratio},
              {flight::String("message"),
               flight::String(
                   "advanceStatechartInstance: exitTimeRatio requires a positive region duration; "
                   "the transition was treated as having no exit-time requirement — call "
                   "setStatechartRegionDuration when the region enters a state.")},
              {flight::String("regionDuration"), region_duration},
              {flight::String("regionIndex"), region_index},
              {flight::String("sourceStateIndex"), source_state_index},
              {flight::String("status"),
               flight::row_get<flight::RowKey<"status">>(explanation)},
              {flight::String("targetStateIndex"),
               flight::row_get<flight::RowKey<"targetStateIndex">>(explanation)},
              {flight::String("transitionIndex"), transition_index},
          },
      },
      std::optional<flight::String>{flight::String("statechart")});
}

inline bool are_statechart_guards_enabled(
    ReadonlyStatechartInstance instance) {
  const auto guard =
      flight::row_get<flight::RowKey<"durationGuard">>(instance);
  if (!guard.has_value()) return false;
  const auto target = guard.value().template target<StatechartGuardFunction>();
  return target != nullptr &&
         *target == &warn_missing_statechart_region_duration;
}

inline void disable_statechart_guards(
    flight::Ref<flight::types::StatechartInstance> instance) {
  instance->duration_guard = std::nullopt;
}

inline void enable_statechart_guards(
    flight::Ref<flight::types::StatechartInstance> instance) {
  instance->duration_guard = flight::types::StatechartDurationGuard{
      &warn_missing_statechart_region_duration};
}

}  // namespace flight::statechart
