// Derived from @flighthq/lighting/packages/lighting/src/lightAnalysis.ts
// at generated digest c7ef2696ab08ebb336a885dec08e30e13c1a5374039477c623468a05831ba12f.
#pragma once

#include <cmath>
#include <concepts>
#include <memory>
#include <type_traits>
#include <variant>

#include <flight/color/luminance.hpp>
#include <flight/runtime.hpp>
#include <flight/structural_ref.hpp>
#include <flight/types/ambient_light.hpp>
#include <flight/types/area_light.hpp>
#include <flight/types/bounding_sphere.hpp>
#include <flight/types/directional_light.hpp>
#include <flight/types/environment.hpp>
#include <flight/types/hemisphere_light.hpp>
#include <flight/types/light.hpp>
#include <flight/types/point_light.hpp>
#include <flight/types/spot_light.hpp>

static_assert(flight::runtime_contract.compiler_contract == "flight-runtime-contract/2", "Flight compiler/runtime contract mismatch");
static_assert(flight::runtime_contract.cpp_abi == 1, "Flight C++ runtime ABI mismatch");

namespace flight::lighting {

using AnalysisPointLightView = flight::StructuralRef<flight::RowReadonly<
    flight::RowOf<flight::Ref<flight::types::PointLight>>>>;
using AnalysisSpotLightView = flight::StructuralRef<flight::RowReadonly<
    flight::RowOf<flight::Ref<flight::types::SpotLight>>>>;
using PointOrSpotLightView = std::variant<AnalysisPointLightView, AnalysisSpotLightView>;

template <typename Type>
inline std::shared_ptr<Type> light_analysis_native_ref(const std::shared_ptr<Type>& value) {
  return value;
}

template <typename Schema>
inline flight::Ref<typename flight::StructuralRef<Schema>::object_type> light_analysis_native_ref(
    const flight::StructuralRef<Schema>& value) {
  return value.shared_object();
}

template <typename LightCarrier>
inline bool is_light_enabled(const LightCarrier& carrier) {
  const auto light = light_analysis_native_ref(carrier);
  if constexpr (requires { light->enabled; }) {
    using Enabled = std::remove_cvref_t<decltype(light->enabled)>;
    if constexpr (std::same_as<Enabled, bool>) return light->enabled != false;
  }
  return true;
}

inline double smoothstep(double edge0, double edge1, double value) {
  if (edge0 == edge1) return value < edge0 ? 0.0 : 1.0;
  const double t = flight::maximum(0.0, flight::minimum(1.0, (value - edge0) / (edge1 - edge0)));
  return t * t * (3.0 - 2.0 * t);
}

template <typename LightCarrier>
inline double get_light_luminance(const LightCarrier& carrier) {
  const auto light = light_analysis_native_ref(carrier);
  if (!is_light_enabled(light)) return 0.0;

  const auto& kind = light->kind;
  if (kind == flight::types::environment_kind) return 0.0;
  if (kind == flight::types::hemisphere_light_kind) {
    if constexpr (requires { light->ground_color; light->sky_color; light->intensity; }) {
      const double mean_color_luminance =
          (flight::color::get_color_luminance(light->ground_color) +
           flight::color::get_color_luminance(light->sky_color)) *
          0.5;
      return mean_color_luminance * light->intensity;
    }
    return 0.0;
  }

  const bool colored_kind = kind == flight::types::ambient_light_kind ||
                            kind == flight::types::area_light_kind ||
                            kind == flight::types::directional_light_kind ||
                            kind == flight::types::point_light_kind ||
                            kind == flight::types::spot_light_kind;
  if (!colored_kind) return 0.0;
  if constexpr (requires { light->color; light->intensity; }) {
    return flight::color::get_color_luminance(light->color) * light->intensity;
  }
  return 0.0;
}

template <typename LightCarrier>
inline void get_light_influence_bounds(
    flight::types::BoundingSphereLike out,
    const LightCarrier& carrier) {
  const auto light = light_analysis_native_ref(carrier);
  auto zero_center = [&] {
    out->center->x = 0.0;
    out->center->y = 0.0;
    out->center->z = 0.0;
  };

  if (!is_light_enabled(light)) {
    zero_center();
    out->radius = 0.0;
    return;
  }

  const auto& kind = light->kind;
  if (kind == flight::types::ambient_light_kind ||
      kind == flight::types::hemisphere_light_kind ||
      kind == flight::types::environment_kind ||
      kind == flight::types::directional_light_kind) {
    zero_center();
    out->radius = -1.0;
    return;
  }

  const bool spatial_kind = kind == flight::types::point_light_kind ||
                            kind == flight::types::spot_light_kind ||
                            kind == flight::types::area_light_kind;
  if (spatial_kind) {
    if constexpr (requires { light->range; light->position; }) {
      if (light->range < 0.0) {
        zero_center();
        out->radius = -1.0;
        return;
      }
      out->center->x = light->position->x;
      out->center->y = light->position->y;
      out->center->z = light->position->z;
      out->radius = light->range;
      return;
    }
  }

  zero_center();
  out->radius = -1.0;
}

template <typename LightCarrier, typename BoundsCarrier>
inline bool has_light_influence_on_bounds(
    const LightCarrier& light_carrier,
    const BoundsCarrier& bounds_carrier) {
  const auto light = light_analysis_native_ref(light_carrier);
  const auto bounds = light_analysis_native_ref(bounds_carrier);
  if (!is_light_enabled(light)) return false;

  const auto& kind = light->kind;
  if (kind == flight::types::ambient_light_kind ||
      kind == flight::types::hemisphere_light_kind ||
      kind == flight::types::environment_kind ||
      kind == flight::types::directional_light_kind) {
    return true;
  }
  const bool spatial_kind = kind == flight::types::point_light_kind ||
                            kind == flight::types::spot_light_kind ||
                            kind == flight::types::area_light_kind;
  if (!spatial_kind) return true;
  if constexpr (requires { light->range; light->position; }) {
    if (light->range < 0.0) return true;
    if (bounds->radius < 0.0) return false;
    const double dx = light->position->x - bounds->center->x;
    const double dy = light->position->y - bounds->center->y;
    const double dz = light->position->z - bounds->center->z;
    const double distance_squared = dx * dx + dy * dy + dz * dz;
    const double radius_sum = light->range + bounds->radius;
    return distance_squared <= radius_sum * radius_sum;
  }
  return true;
}

template <typename LightCarrier>
inline bool is_light_casting_shadow(const LightCarrier& carrier) {
  const auto light = light_analysis_native_ref(carrier);
  if (!is_light_enabled(light)) return false;
  const auto& kind = light->kind;
  if (kind == flight::types::ambient_light_kind ||
      kind == flight::types::hemisphere_light_kind ||
      kind == flight::types::environment_kind) {
    return false;
  }
  if constexpr (requires { light->casts_shadow; }) {
    using CastsShadow = std::remove_cvref_t<decltype(light->casts_shadow)>;
    if constexpr (std::same_as<CastsShadow, bool>) return light->casts_shadow == true;
  }
  return false;
}

template <typename LightCarrier, typename BoundsCarrier>
inline double get_light_contribution_at_bounding_sphere(
    const LightCarrier& light_carrier,
    const BoundsCarrier& bounds_carrier) {
  const auto light = light_analysis_native_ref(light_carrier);
  const auto bounds = light_analysis_native_ref(bounds_carrier);
  if (!is_light_enabled(light) || bounds->radius < 0.0) return 0.0;

  const double center_dx = bounds->center->x - light->position->x;
  const double center_dy = bounds->center->y - light->position->y;
  const double center_dz = bounds->center->z - light->position->z;
  const double center_distance = std::hypot(center_dx, center_dy, center_dz);
  const double distance = flight::maximum(center_distance - bounds->radius, 0.0);
  const double distance_squared = distance * distance;

  double window = 1.0;
  if (light->range > 0.0) {
    const double factor = distance_squared / (light->range * light->range);
    const double windowed = flight::maximum(0.0, flight::minimum(1.0, 1.0 - factor * factor));
    window = windowed * windowed;
  }
  const double attenuation = std::pow(flight::maximum(distance, 0.01), light->decay);
  double contribution = get_light_luminance(light) * window / attenuation;

  if (light->kind == flight::types::spot_light_kind) {
    if constexpr (requires {
                    light->direction;
                    light->outer_cone_cos;
                    light->inner_cone_cos;
                  }) {
      const double direction_length =
          std::hypot(light->direction->x, light->direction->y, light->direction->z);
      const double inverse_ray_length = center_distance > 0.0 ? 1.0 / center_distance : 0.0;
      const double inverse_direction_length = direction_length > 0.0 ? 1.0 / direction_length : 0.0;
      const double cosine =
          (light->direction->x * center_dx + light->direction->y * center_dy +
           light->direction->z * center_dz) *
          inverse_ray_length * inverse_direction_length;
      contribution *= smoothstep(
          light->outer_cone_cos,
          light->inner_cone_cos,
          center_distance > 0.0 ? cosine : 1.0);
    }
  }
  return contribution;
}

template <typename BoundsCarrier>
inline double get_light_contribution_at_bounding_sphere(
    const PointOrSpotLightView& light,
    const BoundsCarrier& bounds) {
  return std::visit(
      [&](const auto& value) {
        return get_light_contribution_at_bounding_sphere(value, bounds);
      },
      light);
}

} // namespace flight::lighting
