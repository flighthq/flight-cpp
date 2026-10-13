// Re-derived from the generated validateParticleEmitterConfig module at a627849. Every declaration but
// normalizeParticleEmitterConfig is emitted there, and three of the emitted bodies do not compile. See
// overrides/manifest.json (particles-validate-particle-emitter-config) for each change, why, and what remains.
#pragma once

#include <algorithm>
#include <string>
#include <utility>
#include <cmath>
#include <flight/number.hpp>
#include <flight/structural_ref.hpp>
#include <limits>
#include <optional>
#include <variant>
#include <flight/runtime.hpp>

static_assert(flight::runtime_contract.compiler_contract == "flight-runtime-contract/2", "Flight compiler/runtime contract mismatch");
static_assert(flight::runtime_contract.cpp_abi == 1, "Flight C++ runtime ABI mismatch");

namespace flight::types { struct ParticleConfigIssue; }
namespace flight::types { struct ParticleEmitterConfig; }

#include <flight/types/particle_config_issue.hpp>
#include <flight/types/particle_curve.hpp>
#include <flight/types/particle_emitter_config.hpp>

#include <flight/particles/particle_emitter_config.hpp>

namespace flight::particles {

inline flight::Array<flight::String> numeric_fields = flight::Array<flight::String>{flight::String("alphaEnd"), flight::String("alphaStart"), flight::String("burstCount"), flight::String("burstInterval"), flight::String("duration"), flight::String("colorEndB"), flight::String("colorEndG"), flight::String("colorEndR"), flight::String("colorEndVarianceB"), flight::String("colorEndVarianceG"), flight::String("colorEndVarianceR"), flight::String("colorStartB"), flight::String("colorStartG"), flight::String("colorStartR"), flight::String("colorStartVarianceB"), flight::String("colorStartVarianceG"), flight::String("colorStartVarianceR"), flight::String("directionX"), flight::String("directionY"), flight::String("directionZ"), flight::String("emitterConeAngle"), flight::String("emitterDepth"), flight::String("emitterHeight"), flight::String("emitterRadius"), flight::String("emitterWidth"), flight::String("frameCount"), flight::String("frameRate"), flight::String("gravityX"), flight::String("gravityY"), flight::String("gravityZ"), flight::String("lifetimeMax"), flight::String("lifetimeMin"), flight::String("maxParticles"), flight::String("regionIdMax"), flight::String("regionIdMin"), flight::String("rotationSpeedMax"), flight::String("rotationSpeedMin"), flight::String("scaleEnd"), flight::String("scaleMax"), flight::String("scaleMin"), flight::String("speedMax"), flight::String("speedMin"), flight::String("spawnRate"), flight::String("spread"), flight::String("velocityInheritance")};

inline flight::Array<flight::String> non_negative_fields = flight::Array<flight::String>{flight::String("burstCount"), flight::String("burstInterval"), flight::String("duration"), flight::String("emitterConeAngle"), flight::String("emitterDepth"), flight::String("emitterHeight"), flight::String("emitterRadius"), flight::String("emitterWidth"), flight::String("frameRate"), flight::String("lifetimeMin"), flight::String("lifetimeMax"), flight::String("maxParticles"), flight::String("scaleMax"), flight::String("scaleMin"), flight::String("speedMax"), flight::String("speedMin"), flight::String("spawnRate")};

inline bool is_finite_curve(std::optional<flight::Array<double>> curve) {
  if ((!curve.has_value() || (static_cast<double>(curve.value().size()) == 0.0))) {
    return false;
  }
  {
    double i = 0.0;
    while ((i < static_cast<double>(curve.value().size()))) {
      {
        if (!std::isfinite(curve.value().element(i))) {
          return false;
        }
      }
      (i += 1.0);
    }
  }
  return true;
}


// OVERRIDE: normalizeParticleEmitterConfig is supplied, and the three closed-key readers are re-written.
// normalizeParticleEmitterConfig is refused, correctly: it writes through `out as unknown as Record<string,
// number>` (cpp-erased-record-assertion-unrepresented). validateParticleEmitterConfig, reportInvertedRange
// and reportUnitRange emit, but do not compile. A read through a closed key erases the key to String as
// `row_get<T>(view, key)`, and `Number.isFinite` of a union becomes `std::isfinite` of its carrier
// (docs/compiler-requests/closed-key-reads-and-union-number-isfinite.md). numeric_fields, non_negative_fields,
// isFiniteCurve and reportCurve are the emitted code, unchanged.
//
// Every closed-key read goes through one dispatcher over the 45 numeric ParticleEmitterConfig members, read as
// rows. It names each member as a RowKey, so each has its generated binding, and the key tuple and its readers
// cannot disagree. A key outside that set holds no number. The source's readers then apply Number.isFinite,
// which is false for a non-number, so no issue is reported, as in TypeScript.
namespace validate_particle_emitter_config_detail {

template <typename... Keys>
struct NumericKeys {};

using NumericFieldKeys = NumericKeys<
    flight::RowKey<"alphaEnd">,
    flight::RowKey<"alphaStart">,
    flight::RowKey<"burstCount">,
    flight::RowKey<"burstInterval">,
    flight::RowKey<"duration">,
    flight::RowKey<"colorEndB">,
    flight::RowKey<"colorEndG">,
    flight::RowKey<"colorEndR">,
    flight::RowKey<"colorEndVarianceB">,
    flight::RowKey<"colorEndVarianceG">,
    flight::RowKey<"colorEndVarianceR">,
    flight::RowKey<"colorStartB">,
    flight::RowKey<"colorStartG">,
    flight::RowKey<"colorStartR">,
    flight::RowKey<"colorStartVarianceB">,
    flight::RowKey<"colorStartVarianceG">,
    flight::RowKey<"colorStartVarianceR">,
    flight::RowKey<"directionX">,
    flight::RowKey<"directionY">,
    flight::RowKey<"directionZ">,
    flight::RowKey<"emitterConeAngle">,
    flight::RowKey<"emitterDepth">,
    flight::RowKey<"emitterHeight">,
    flight::RowKey<"emitterRadius">,
    flight::RowKey<"emitterWidth">,
    flight::RowKey<"frameCount">,
    flight::RowKey<"frameRate">,
    flight::RowKey<"gravityX">,
    flight::RowKey<"gravityY">,
    flight::RowKey<"gravityZ">,
    flight::RowKey<"lifetimeMax">,
    flight::RowKey<"lifetimeMin">,
    flight::RowKey<"maxParticles">,
    flight::RowKey<"regionIdMax">,
    flight::RowKey<"regionIdMin">,
    flight::RowKey<"rotationSpeedMax">,
    flight::RowKey<"rotationSpeedMin">,
    flight::RowKey<"scaleEnd">,
    flight::RowKey<"scaleMax">,
    flight::RowKey<"scaleMin">,
    flight::RowKey<"speedMax">,
    flight::RowKey<"speedMin">,
    flight::RowKey<"spawnRate">,
    flight::RowKey<"spread">,
    flight::RowKey<"velocityInheritance">>;

// Calls `visit` with the member `field` names when it is one of the numeric members, and reports whether it was.
// Through a mutable row the member is visited by reference, so normalization writes the object itself.
template <typename View, typename Visit, typename... Keys>
bool visit_numeric_field(const View& view, const flight::String& field, Visit&& visit, NumericKeys<Keys...>) {
  const std::string name = field.to_utf8();
  return ((name == Keys::name.view() ? (visit(flight::row_get<Keys>(view)), true) : false) || ...);
}

template <typename View, typename Visit>
bool visit_numeric_field(const View& view, const flight::String& field, Visit&& visit) {
  return visit_numeric_field(view, field, std::forward<Visit>(visit), NumericFieldKeys{});
}

// Number.isFinite(config[field]): a finite number, and false for anything else.
template <typename View>
std::optional<double> finite_numeric_field(const View& view, const flight::String& field) {
  std::optional<double> out;
  visit_numeric_field(view, field, [&out](const double& value) {
    if (std::isfinite(value)) out = value;
  });
  return out;
}

// `${value}` / String(value) of a numeric member, which is the number's own text.
template <typename View>
flight::String numeric_field_text(const View& view, const flight::String& field) {
  flight::String out = flight::String("undefined");
  visit_numeric_field(view, field, [&out](const double& value) { out = flight::to_string(value); });
  return out;
}

inline void append_issue(flight::Array<flight::Ref<flight::types::ParticleConfigIssue>> issues, const flight::String& field,
                         const flight::String& message, const flight::String& severity) {
  issues.push(flight::make_ref<flight::types::ParticleConfigIssue>(flight::types::ParticleConfigIssue{.field = field, .message = message, .severity = severity}));
}

} // namespace validate_particle_emitter_config_detail

// OVERRIDE: normalizeParticleEmitterConfig, from the source. `out` and `defaults` are two fresh configs. Every
// numeric member of `out` that is not finite takes the default's value, written into `out` itself. The result
// is `{ ...out, ... }`, a NEW object holding out's members, so retained curve arrays keep their identity. In
// it, curves that are empty or hold a non-finite sample become null, and the listed members are floored and
// clamped in source order. regionIdMax is clamped against the normalized regionIdMin, as the source writes it.
inline flight::Ref<flight::types::ParticleEmitterConfig> normalize_particle_emitter_config(std::optional<flight::Ref<alpha_end_alpha_start_blend_mode_color_end_b_color_end_g_color_end_r_color_end_variance_b_color_end_variance_g_color_end_variance_r_color_start_b_color_start_g_color_start_r_color_start_variance_b_color_start_variance_g_color_start_variance_r_direction_x_direction_y_direction_z_gravity_x_gravity_y_gravity_z_emitter_cone_angle_emitter_depth_emitter_height_emitter_radius_emitter_shape_emitter_width_burst_count_burst_interval_duration_loop_frame_count_frame_rate_lifetime_max_lifetime_min_max_particles_region_id_max_region_id_min_scale_end_scale_max_scale_min_speed_max_speed_min_spawn_rate_spread_rotation_speed_max_rotation_speed_min_velocity_inheritance_alpha_curve_color_curve_scale_curve_world_space_009102f54c361689>> config = std::nullopt) {
  namespace detail = validate_particle_emitter_config_detail;
  const flight::Ref<flight::types::ParticleEmitterConfig> out = create_particle_emitter_config(config);
  const flight::Ref<flight::types::ParticleEmitterConfig> defaults = create_particle_emitter_config();
  const flight::StructuralRef<flight::RowOf<flight::Ref<flight::types::ParticleEmitterConfig>>> mutable_out(out);
  const flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::ParticleEmitterConfig>>>> defaults_rec(defaults);
  for (auto field : numeric_fields) {
    detail::visit_numeric_field(mutable_out, field, [&](double& value) {
      if (!std::isfinite(value)) {
        detail::visit_numeric_field(defaults_rec, field, [&value](const double& fallback) { value = fallback; });
      }
    });
  }
  flight::Ref<flight::types::ParticleEmitterConfig> normalized = flight::make_ref<flight::types::ParticleEmitterConfig>(*out);
  normalized->alpha_curve = is_finite_curve(out->alpha_curve) ? out->alpha_curve : std::nullopt;
  normalized->color_curve = is_finite_curve(out->color_curve) ? out->color_curve : std::nullopt;
  normalized->scale_curve = is_finite_curve(out->scale_curve) ? out->scale_curve : std::nullopt;
  normalized->max_particles = std::max(0.0, std::floor(out->max_particles));
  normalized->burst_count = std::max(0.0, std::floor(out->burst_count));
  normalized->burst_interval = std::max(0.0, out->burst_interval);
  normalized->duration = std::max(0.0, out->duration);
  normalized->frame_count = std::max(1.0, std::floor(out->frame_count));
  normalized->frame_rate = std::max(0.0, out->frame_rate);
  normalized->region_id_min = std::max(0.0, std::floor(out->region_id_min));
  normalized->region_id_max = std::max(std::max(0.0, std::floor(out->region_id_min)), std::floor(out->region_id_max));
  normalized->spawn_rate = std::max(0.0, out->spawn_rate);
  normalized->lifetime_min = std::max(0.0, out->lifetime_min);
  normalized->lifetime_max = std::max(0.0, out->lifetime_max);
  normalized->speed_min = std::max(0.0, out->speed_min);
  normalized->speed_max = std::max(0.0, out->speed_max);
  normalized->scale_min = std::max(0.0, out->scale_min);
  normalized->scale_max = std::max(0.0, out->scale_max);
  normalized->emitter_cone_angle = std::max(0.0, out->emitter_cone_angle);
  normalized->emitter_depth = std::max(0.0, out->emitter_depth);
  normalized->emitter_radius = std::max(0.0, out->emitter_radius);
  normalized->emitter_width = std::max(0.0, out->emitter_width);
  normalized->emitter_height = std::max(0.0, out->emitter_height);
  return normalized;
}

inline void report_curve(flight::Array<flight::Ref<flight::types::ParticleConfigIssue>> issues, std::optional<flight::Array<double>> curve, flight::String field, double stride) {
  if (!curve.has_value()) {
    return;
  }
  if ((static_cast<double>(curve.value().size()) == 0.0)) {
    issues.push(flight::make_ref<flight::types::ParticleConfigIssue>(flight::types::ParticleConfigIssue{.field = field, .message = flight::String("") + flight::to_string(field) + flight::String(" is empty and will be ignored"), .severity = flight::String("warning")}));
    return;
  }
  if ((std::fmod(static_cast<double>(curve.value().size()), stride) != 0.0)) {
    issues.push(flight::make_ref<flight::types::ParticleConfigIssue>(flight::types::ParticleConfigIssue{.field = field, .message = flight::String("") + flight::to_string(field) + flight::String(" length (") + flight::to_string(static_cast<double>(curve.value().size())) + flight::String(") is not a multiple of ") + flight::to_string(stride) + flight::String(""), .severity = flight::String("warning")}));
  }
  {
    double i = 0.0;
    while ((i < static_cast<double>(curve.value().size()))) {
      {
        if (!std::isfinite(curve.value().element(i))) {
          issues.push(flight::make_ref<flight::types::ParticleConfigIssue>(flight::types::ParticleConfigIssue{.field = field, .message = flight::String("") + flight::to_string(field) + flight::String(" contains a non-finite sample at index ") + flight::to_string(i) + flight::String(""), .severity = flight::String("error")}));
          break;
        }
      }
      (i += 1.0);
    }
  }
}

// OVERRIDE: reportInvertedRange, emitted as row_get<T>(config, String) and std::isfinite of a union. A warning
// is reported only when both members hold finite numbers and min > max, in the source's message.
inline void report_inverted_range(flight::Array<flight::Ref<flight::types::ParticleConfigIssue>> issues, flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::ParticleEmitterConfig>>>> config, flight::String min_field, flight::String max_field) {
  namespace detail = validate_particle_emitter_config_detail;
  const auto min = detail::finite_numeric_field(config, min_field);
  const auto max = detail::finite_numeric_field(config, max_field);
  if (min.has_value() && max.has_value() && (min.value() > max.value())) {
    detail::append_issue(issues, min_field, min_field + flight::String(" (") + flight::to_string(min.value()) + flight::String(") is greater than ") + max_field + flight::String(" (") + flight::to_string(max.value()) + flight::String(")"), flight::String("warning"));
  }
}

// OVERRIDE: reportUnitRange, for the same two defects. A finite value outside [0, 1] is a warning.
inline void report_unit_range(flight::Array<flight::Ref<flight::types::ParticleConfigIssue>> issues, flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::ParticleEmitterConfig>>>> config, flight::String field) {
  namespace detail = validate_particle_emitter_config_detail;
  const auto value = detail::finite_numeric_field(config, field);
  if (value.has_value() && ((value.value() < 0.0) || (value.value() > 1.0))) {
    detail::append_issue(issues, field, field + flight::String(" (") + flight::to_string(value.value()) + flight::String(") is outside the expected 0–1 range"), flight::String("warning"));
  }
}

// OVERRIDE: validateParticleEmitterConfig. The emission's two field loops read `config[field]` as
// row_get<double>(config, String). Here they go through the dispatcher, and the rest is the emitted body. Issues
// come in source order: non-finite errors, then negative warnings, then lifetime, capacity and frame-count, then
// inverted ranges, unit ranges, and curves.
inline flight::Array<flight::Ref<flight::types::ParticleConfigIssue>> validate_particle_emitter_config(flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::ParticleEmitterConfig>>>> config) {
  namespace detail = validate_particle_emitter_config_detail;
  flight::Array<flight::Ref<flight::types::ParticleConfigIssue>> issues = flight::Array<flight::Ref<flight::types::ParticleConfigIssue>>{};
  for (auto field : numeric_fields) {
    if (!detail::finite_numeric_field(config, field).has_value()) {
      detail::append_issue(issues, field, field + flight::String(" must be a finite number (got ") + detail::numeric_field_text(config, field) + flight::String(")"), flight::String("error"));
    }
  }
  for (auto field_2 : non_negative_fields) {
    const auto value_2 = detail::finite_numeric_field(config, field_2);
    if (value_2.has_value() && (value_2.value() < 0.0)) {
      detail::append_issue(issues, field_2, field_2 + flight::String(" must not be negative (got ") + flight::to_string(value_2.value()) + flight::String(")"), flight::String("warning"));
    }
  }
  if ((std::isfinite(flight::row_get<flight::RowKey<"lifetimeMax">>(config)) && (flight::row_get<flight::RowKey<"lifetimeMax">>(config) <= 0.0))) {
    issues.push(flight::make_ref<flight::types::ParticleConfigIssue>(flight::types::ParticleConfigIssue{.field = flight::String("lifetimeMax"), .message = flight::String("lifetimeMax must be > 0 or particles die instantly"), .severity = flight::String("warning")}));
  }
  if ((std::isfinite(flight::row_get<flight::RowKey<"maxParticles">>(config)) && (flight::row_get<flight::RowKey<"maxParticles">>(config) <= 0.0))) {
    issues.push(flight::make_ref<flight::types::ParticleConfigIssue>(flight::types::ParticleConfigIssue{.field = flight::String("maxParticles"), .message = flight::String("maxParticles must be >= 1 or nothing ever spawns"), .severity = flight::String("warning")}));
  }
  if ((std::isfinite(flight::row_get<flight::RowKey<"frameCount">>(config)) && (flight::row_get<flight::RowKey<"frameCount">>(config) < 1.0))) {
    issues.push(flight::make_ref<flight::types::ParticleConfigIssue>(flight::types::ParticleConfigIssue{.field = flight::String("frameCount"), .message = flight::String("frameCount must be >= 1"), .severity = flight::String("warning")}));
  }
  report_inverted_range(issues, config, flight::String("lifetimeMin"), flight::String("lifetimeMax"));
  report_inverted_range(issues, config, flight::String("speedMin"), flight::String("speedMax"));
  report_inverted_range(issues, config, flight::String("scaleMin"), flight::String("scaleMax"));
  report_inverted_range(issues, config, flight::String("rotationSpeedMin"), flight::String("rotationSpeedMax"));
  report_unit_range(issues, config, flight::String("alphaStart"));
  report_unit_range(issues, config, flight::String("alphaEnd"));
  report_curve(issues, flight::row_get<flight::RowKey<"alphaCurve">>(config), flight::String("alphaCurve"), 1.0);
  report_curve(issues, flight::row_get<flight::RowKey<"colorCurve">>(config), flight::String("colorCurve"), 3.0);
  report_curve(issues, flight::row_get<flight::RowKey<"scaleCurve">>(config), flight::String("scaleCurve"), 1.0);
  return issues;
}

} // namespace flight::particles
