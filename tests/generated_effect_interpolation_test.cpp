#include <flight/effects/render_effect_interpolation.hpp>
#include <flight/types/bevel_effect.hpp>
#include <flight/types/bloom_effect.hpp>
#include <flight/types/chromatic_aberration_effect.hpp>
#include <flight/types/vignette_effect.hpp>

#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>

// Mutation evidence (2026-10-13): the focused test killed disabling packed-
// color dispatch and changing the boolean snap boundary from t < 0.5 to <=.

namespace {

bool check(bool condition, const char* message) {
  if (!condition) std::cerr << message << '\n';
  return condition;
}

template <typename Effect>
flight::Ref<Effect> make_effect(const char* kind) {
  auto effect = flight::make_ref<Effect>();
  effect->kind = flight::String(kind);
  return effect;
}

std::uint32_t channel_of(double color, unsigned index) {
  return (static_cast<std::uint32_t>(color) >> (24U - index * 8U)) & 0xffU;
}

}  // namespace

int main() {
  using flight::String;
  using flight::effects::RenderEffectFieldRoles;

  const auto& roles = flight::effects::render_effect_field_roles;
  const auto role_keys = roles.enumerable_keys();
  const auto bevel_roles = roles.get(String("BevelEffect"));
  const auto vignette_roles = roles.get(String("VignetteEffect"));
  auto roles_copy = roles;
  if (!check(role_keys.size() == 10 &&
                 role_keys.element(0.0) == String("BevelEffect") &&
                 role_keys.element(9.0) == String("VolumetricLightEffect"),
             "effect field-role kind inventory or order drifted") ||
      !check(bevel_roles.has_value() && bevel_roles->size() == 2 &&
                 bevel_roles->get(String("highlightColor")) ==
                     String("packedColor") &&
                 bevel_roles->get(String("shadowColor")) ==
                     String("packedColor"),
             "bevel packed-color field roles drifted") ||
      !check(vignette_roles.has_value() &&
                 vignette_roles->get(String("color")) ==
                     String("packedColor"),
             "vignette packed-color field role drifted") ||
      !check(roles_copy.identity() == roles.identity(),
             "field-role record copy lost source object identity")) {
    return 1;
  }

  auto bloom_a = make_effect<flight::types::BloomEffect>("BloomEffect");
  bloom_a->threshold = 0.2;
  bloom_a->radius = 4.0;
  auto bloom_b = make_effect<flight::types::BloomEffect>("BloomEffect");
  bloom_b->threshold = 0.8;
  bloom_b->radius = 16.0;
  auto bloom_out = make_effect<flight::types::BloomEffect>("BloomEffect");
  const auto bloom_out_identity = bloom_out.get();
  if (!check(flight::effects::can_lerp_render_effects(bloom_a, bloom_b),
             "same-kind effects were rejected") ||
      !check(flight::effects::lerp_render_effect(bloom_a, bloom_b, 0.5,
                                                 bloom_out),
             "same-owner interpolation did not report success") ||
      !check(bloom_out.get() == bloom_out_identity &&
                 bloom_out->threshold == 0.5 && bloom_out->radius == 10.0,
             "scalar interpolation lost output identity or values")) {
    return 1;
  }

  auto different_kind = make_effect<flight::types::VignetteEffect>(
      "VignetteEffect");
  bloom_out->threshold = 0.99;
  if (!check(!flight::effects::can_lerp_render_effects(bloom_a,
                                                        different_kind),
             "different effect kinds were accepted") ||
      !check(!flight::effects::lerp_render_effect(
                 bloom_a, different_kind, 0.5, bloom_out),
             "different-kind interpolation did not report failure") ||
      !check(bloom_out->threshold == 0.99,
             "different-kind interpolation mutated the output")) {
    return 1;
  }

  bloom_out->threshold = 0.5;
  flight::effects::lerp_render_effect(bloom_a, bloom_b, -1.0, bloom_out);
  if (!check(bloom_out->threshold == 0.2 && bloom_out->radius == 4.0,
             "negative interpolation time was not clamped to a")) {
    return 1;
  }
  flight::effects::lerp_render_effect(bloom_a, bloom_b, 2.0, bloom_out);
  if (!check(bloom_out->threshold == 0.8 && bloom_out->radius == 16.0,
             "large interpolation time was not clamped to b")) {
    return 1;
  }
  flight::effects::lerp_render_effect(
      bloom_a, bloom_b, std::numeric_limits<double>::quiet_NaN(), bloom_out);
  if (!check(std::isnan(bloom_out->threshold.value()) &&
                 std::isnan(bloom_out->radius.value()),
             "NaN interpolation time did not preserve Math.min/max semantics")) {
    return 1;
  }

  bloom_a->threshold = 0.0;
  bloom_a->radius = 0.0;
  bloom_b->threshold = 1.0;
  bloom_b->radius = 10.0;
  const auto bloom_a_identity = bloom_a.get();
  flight::effects::lerp_render_effect(bloom_a, bloom_b, 0.5, bloom_a);
  if (!check(bloom_a.get() == bloom_a_identity && bloom_a->threshold == 0.5 &&
                 bloom_a->radius == 5.0,
             "aliased interpolation did not use the endpoint snapshot")) {
    return 1;
  }

  bloom_a->threshold = std::nullopt;
  bloom_b->threshold = 0.8;
  bloom_out->threshold = 0.5;
  flight::effects::lerp_render_effect(bloom_a, bloom_b, 0.4, bloom_out);
  if (!check(!bloom_out->threshold.has_value(),
             "missing numeric endpoint did not snap to absence below half")) {
    return 1;
  }
  flight::effects::lerp_render_effect(bloom_a, bloom_b, 0.5, bloom_out);
  if (!check(bloom_out->threshold == 0.8,
             "missing numeric endpoint did not snap to b at half")) {
    return 1;
  }

  auto vignette_a = make_effect<flight::types::VignetteEffect>(
      "VignetteEffect");
  vignette_a->color = 0xff0000ff;
  vignette_a->intensity = 0.0;
  auto vignette_b = make_effect<flight::types::VignetteEffect>(
      "VignetteEffect");
  vignette_b->color = 0x0000ffff;
  vignette_b->intensity = 1.0;
  auto vignette_out = make_effect<flight::types::VignetteEffect>(
      "VignetteEffect");
  flight::effects::lerp_render_effect(vignette_a, vignette_b, 0.5,
                                      vignette_out);
  if (!check(vignette_out->color.has_value() &&
                 channel_of(vignette_out->color.value(), 1) == 0 &&
                 std::floor(vignette_out->color.value()) ==
                     vignette_out->color.value(),
             "packed color was interpolated as a scalar") ||
      !check(vignette_out->intensity == 0.5,
             "ordinary scalar beside a packed color was not linear")) {
    return 1;
  }

  RenderEffectFieldRoles scalar_roles;
  flight::effects::lerp_render_effect(vignette_a, vignette_b, 0.5,
                                      vignette_out, scalar_roles);
  const double scalar_start = static_cast<double>(0xff0000ffU);
  const double scalar_end = static_cast<double>(0x0000ffffU);
  const double scalar_color =
      scalar_start + (scalar_end - scalar_start) * 0.5;
  if (!check(vignette_out->color == scalar_color,
             "caller-supplied field roles were ignored")) {
    return 1;
  }

  auto bevel_a = make_effect<flight::types::BevelEffect>("BevelEffect");
  bevel_a->highlight_color = 0xff0000ff;
  bevel_a->shadow_color = 0xff0000ff;
  bevel_a->bevel_type = String("inner");
  auto bevel_b = make_effect<flight::types::BevelEffect>("BevelEffect");
  bevel_b->highlight_color = 0x0000ffff;
  bevel_b->shadow_color = 0x0000ffff;
  bevel_b->bevel_type = String("outer");
  auto bevel_out = make_effect<flight::types::BevelEffect>("BevelEffect");
  flight::effects::lerp_render_effect(bevel_a, bevel_b, 0.5, bevel_out);
  if (!check(bevel_out->highlight_color.has_value() &&
                 bevel_out->shadow_color.has_value() &&
                 channel_of(bevel_out->highlight_color.value(), 1) == 0 &&
                 channel_of(bevel_out->shadow_color.value(), 1) == 0,
             "multi-color effect did not apply every declared role") ||
      !check(bevel_out->bevel_type == String("outer"),
             "string field did not snap to b at half")) {
    return 1;
  }

  auto boolean_a =
      make_effect<flight::types::ChromaticAberrationEffect>(
          "ChromaticAberrationEffect");
  boolean_a->radial = false;
  auto boolean_b =
      make_effect<flight::types::ChromaticAberrationEffect>(
          "ChromaticAberrationEffect");
  boolean_b->radial = true;
  auto boolean_out =
      make_effect<flight::types::ChromaticAberrationEffect>(
          "ChromaticAberrationEffect");
  flight::effects::lerp_render_effect(boolean_a, boolean_b, 0.4,
                                      boolean_out);
  if (!check(boolean_out->radial == false,
             "boolean field did not snap to a below half")) {
    return 1;
  }
  flight::effects::lerp_render_effect(boolean_a, boolean_b, 0.5,
                                      boolean_out);
  if (!check(boolean_out->radial == true,
             "boolean field did not snap to b at half")) {
    return 1;
  }

  different_kind->kind = String("BloomEffect");
  bool cross_owner_failed_loudly = false;
  try {
    static_cast<void>(flight::effects::lerp_render_effect(
        bloom_a, different_kind, 0.5, bloom_out));
  } catch (const flight::Error& error) {
    cross_owner_failed_loudly =
        error.message() == String(
            "lerpRenderEffect same-kind interpolation requires one retained exact effect owner");
  }
  if (!check(cross_owner_failed_loudly,
             "same-kind cross-owner interpolation crossed the partial boundary")) {
    return 1;
  }

  return 0;
}
