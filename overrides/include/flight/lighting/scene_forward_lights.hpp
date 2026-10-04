// Derived from @flighthq/lighting/packages/lighting/src/sceneForwardLights.ts
// at generated digest b0ebbeb3b8e52b9435fff1caa4e6471039e00ff056d74455b06642ef123fe903.
#pragma once

#include <optional>

#include <flight/lighting/light_analysis.hpp>
#include <flight/runtime.hpp>
#include <flight/structural_ref.hpp>
#include <flight/types/bounding_sphere.hpp>
#include <flight/types/light_layer_mask.hpp>
#include <flight/types/point_light.hpp>
#include <flight/types/scene3_dforward_light_selection.hpp>
#include <flight/types/scene3_dlight_block.hpp>
#include <flight/types/scene3_dlights.hpp>
#include <flight/types/spot_light.hpp>

static_assert(flight::runtime_contract.compiler_contract == "flight-runtime-contract/2", "Flight compiler/runtime contract mismatch");
static_assert(flight::runtime_contract.cpp_abi == 1, "Flight C++ runtime ABI mismatch");

namespace flight::lighting {

using PointLightView = flight::StructuralRef<flight::RowReadonly<
    flight::RowOf<flight::Ref<flight::types::PointLight>>>>;
using SpotLightView = flight::StructuralRef<flight::RowReadonly<
    flight::RowOf<flight::Ref<flight::types::SpotLight>>>>;
using BoundingSphereView = flight::StructuralRef<flight::RowReadonly<
    flight::RowOf<flight::types::BoundingSphereLike>>>;
using Scene3DLightsView = flight::StructuralRef<flight::RowReadonly<
    flight::RowOf<flight::types::Scene3DLightsLike>>>;

template <typename LightView>
inline double select_strongest_lights(
    const std::optional<flight::Array<LightView>>& lights,
    const BoundingSphereView& bounds,
    double receiver_layer_mask,
    flight::Array<LightView> selected_lights,
    flight::Int32Array selected_indices,
    flight::Float64Array selected_scores,
    flight::Float64Array selected_priorities) {
  double selected_count = 0.0;
  if (!lights.has_value()) return selected_count;

  for (double input_index = 0.0;
       input_index < static_cast<double>(lights->size());
       input_index += 1.0) {
    const auto light = lights->element(input_index);
    const auto native_light = light.shared_object();
    if (flight::bitwise_and(native_light->layer_mask, receiver_layer_mask) == 0.0) continue;
    const double score = get_light_contribution_at_bounding_sphere(light, bounds);
    if (!(score > 0.0)) continue;

    const double priority = native_light->priority;
    double insert_at = selected_count;
    while (insert_at > 0.0) {
      const double previous = insert_at - 1.0;
      if (priority < selected_priorities.get_index(previous)) break;
      if (priority == selected_priorities.get_index(previous)) {
        if (score < selected_scores.get_index(previous)) break;
        if (score == selected_scores.get_index(previous) &&
            input_index > selected_indices.get_index(previous)) {
          break;
        }
      }
      insert_at -= 1.0;
    }
    if (insert_at >= flight::types::max_forward_lights) continue;

    const double next_count =
        flight::minimum(selected_count + 1.0, flight::types::max_forward_lights);
    for (double index = next_count - 1.0; index > insert_at; index -= 1.0) {
      selected_lights.element(index) = selected_lights.element(index - 1.0);
      selected_indices.set_index(index, selected_indices.get_index(index - 1.0));
      selected_scores.set_index(index, selected_scores.get_index(index - 1.0));
      selected_priorities.set_index(index, selected_priorities.get_index(index - 1.0));
    }
    selected_lights.element(insert_at) = light;
    selected_indices.set_index(insert_at, input_index);
    selected_scores.set_index(insert_at, score);
    selected_priorities.set_index(insert_at, priority);
    selected_count = next_count;
  }
  return selected_count;
}

// OVERRIDE: the source arrays begin sparse, but every slot below selected_count is written before it
// is read. Dense default StructuralRefs therefore preserve every observable access in this module.
inline flight::Int32Array scratch_selected_point_indices =
    flight::Int32Array(flight::types::max_forward_lights);
inline flight::Array<PointLightView> scratch_selected_point_lights =
    flight::Array<PointLightView>(flight::types::max_forward_lights);
inline flight::Float64Array scratch_selected_point_scores =
    flight::Float64Array(flight::types::max_forward_lights);
inline flight::Float64Array scratch_selected_point_priorities =
    flight::Float64Array(flight::types::max_forward_lights);
inline flight::Int32Array scratch_selected_spot_indices =
    flight::Int32Array(flight::types::max_forward_lights);
inline flight::Array<SpotLightView> scratch_selected_spot_lights =
    flight::Array<SpotLightView>(flight::types::max_forward_lights);
inline flight::Float64Array scratch_selected_spot_scores =
    flight::Float64Array(flight::types::max_forward_lights);
inline flight::Float64Array scratch_selected_spot_priorities =
    flight::Float64Array(flight::types::max_forward_lights);

inline void select_scene3_dforward_lights(
    flight::Ref<flight::types::Scene3DForwardLightSelection> out,
    const Scene3DLightsView& lights,
    const BoundingSphereView& bounds,
    std::optional<double> receiver_layer_mask = std::nullopt) {
  const auto native_lights = lights.shared_object();
  const double layer_mask =
      receiver_layer_mask.value_or(flight::types::all_light_layers);

  const double point_count = select_strongest_lights(
      native_lights->point,
      bounds,
      layer_mask,
      scratch_selected_point_lights,
      scratch_selected_point_indices,
      scratch_selected_point_scores,
      scratch_selected_point_priorities);
  const double spot_count = select_strongest_lights(
      native_lights->spot,
      bounds,
      layer_mask,
      scratch_selected_spot_lights,
      scratch_selected_spot_indices,
      scratch_selected_spot_scores,
      scratch_selected_spot_priorities);

  auto out_points = out->point;
  auto out_spots = out->spot;
  auto out_indices = out->indices;
  out_indices.resize(0.0);
  out_points.resize(0.0);
  out_spots.resize(0.0);
  for (double index = 0.0; index < point_count; index += 1.0) {
    out_indices.push(scratch_selected_point_indices.get_index(index));
    out_points.push(scratch_selected_point_lights.element(index));
  }
  for (double index = 0.0; index < spot_count; index += 1.0) {
    out_indices.push(flight::bitwise_not(scratch_selected_spot_indices.get_index(index)));
    out_spots.push(scratch_selected_spot_lights.element(index));
  }
}

} // namespace flight::lighting
