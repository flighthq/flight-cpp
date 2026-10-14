// Re-derived from the generated partial renderEffectInterpolation module.
#pragma once

// PARTIAL: the mixed-pin SDK has renamed RenderEffect to Effect and retains no
// erased base owner that can recover a concrete effect. Exact native owners can
// interpolate through their shared NamedProperties storage; different owners
// with the same kind stop at that boundary instead of being cast or copied.
#include <algorithm>
#include <concepts>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

#include <flight/color/lerp_color.hpp>
#include <flight/record.hpp>
#include <flight/runtime.hpp>
#include <flight/structural_ref.hpp>
#include <flight/types/effect.hpp>
#include <flight/types/effect_field_role.hpp>

static_assert(flight::runtime_contract.compiler_contract == "flight-runtime-contract/2",
              "Flight compiler/runtime contract mismatch");
static_assert(flight::runtime_contract.cpp_abi == 1,
              "Flight C++ runtime ABI mismatch");

namespace flight::effects {

// Preserve the public names of the generated 7e2fc7d module while using the
// equivalent a627849 type owners that are actually present in this mixed tree.
using RenderEffect = flight::types::Effect;
using RenderEffectFieldRole = flight::types::EffectFieldRole;
using RenderEffectFieldRoles = flight::types::EffectFieldRoles;
using RenderEffectKindFieldRoles = flight::types::EffectKindFieldRoles;

template <typename Left, typename Right>
inline bool can_lerp_render_effects(std::shared_ptr<Left> a,
                                    std::shared_ptr<Right> b) {
  return a->kind == b->kind;
}

inline RenderEffectFieldRoles render_effect_field_roles = {
    {flight::String("BevelEffect"),
     {{flight::String("highlightColor"), flight::String("packedColor")},
      {flight::String("shadowColor"), flight::String("packedColor")}}},
    {flight::String("ConvolutionEffect"),
     {{flight::String("color"), flight::String("packedColor")}}},
    {flight::String("DropShadowEffect"),
     {{flight::String("color"), flight::String("packedColor")}}},
    {flight::String("InnerGlowEffect"),
     {{flight::String("color"), flight::String("packedColor")}}},
    {flight::String("InnerShadowEffect"),
     {{flight::String("color"), flight::String("packedColor")}}},
    {flight::String("OuterGlowEffect"),
     {{flight::String("color"), flight::String("packedColor")}}},
    {flight::String("OutlineEffect"),
     {{flight::String("color"), flight::String("packedColor")}}},
    {flight::String("ScreenSpaceFogEffect"),
     {{flight::String("color"), flight::String("packedColor")}}},
    {flight::String("VignetteEffect"),
     {{flight::String("color"), flight::String("packedColor")}}},
    {flight::String("VolumetricLightEffect"),
     {{flight::String("lightColor"), flight::String("packedColor")}}},
};

namespace detail {

enum class RenderEffectFieldKind { number, boolean, string };

struct RenderEffectFieldSnapshot {
  flight::String key;
  flight::Any left;
  flight::Any right;
  RenderEffectFieldKind kind;
};

inline bool contains_render_effect_key(
    const std::vector<RenderEffectFieldSnapshot>& fields,
    const flight::String& key) {
  return std::any_of(fields.begin(), fields.end(), [&](const auto& field) {
    return field.key == key;
  });
}

inline void collect_render_effect_field(
    std::vector<RenderEffectFieldSnapshot>& fields,
    const flight::NamedProperties& left,
    const flight::NamedProperties& right,
    const flight::String& key) {
  if (key == flight::String("kind") ||
      contains_render_effect_key(fields, key)) {
    return;
  }

  const flight::Any left_value = left.get(key);
  const flight::Any right_value = right.get(key);
  if (left_value.kind() == flight::AnyKind::number ||
      right_value.kind() == flight::AnyKind::number) {
    fields.push_back({key, left_value, right_value,
                      RenderEffectFieldKind::number});
  } else if (left_value.kind() == flight::AnyKind::boolean ||
             right_value.kind() == flight::AnyKind::boolean) {
    fields.push_back({key, left_value, right_value,
                      RenderEffectFieldKind::boolean});
  } else if (left_value.kind() == flight::AnyKind::string ||
             right_value.kind() == flight::AnyKind::string) {
    fields.push_back(
        {key, left_value, right_value, RenderEffectFieldKind::string});
  }
}

inline bool is_packed_render_effect_color(
    const RenderEffectFieldRoles& roles, const flight::String& effect_kind,
    const flight::String& field) {
  const auto kind_roles = roles.get(effect_kind);
  if (!kind_roles.has_value()) return false;
  const auto role = kind_roles->get(field);
  return role.has_value() && role.value() == flight::String("packedColor");
}

inline void write_render_effect_field(const flight::NamedProperties& out,
                                      const flight::String& key,
                                      const flight::Any& value) {
  if (!out.set(key, value)) {
    throw std::logic_error(
        "lerpRenderEffect could not write the retained exact-owner field");
  }
}

template <typename ExactEffect>
inline bool lerp_exact_render_effect(
    std::shared_ptr<ExactEffect> a, std::shared_ptr<ExactEffect> b, double t,
    std::shared_ptr<ExactEffect> out,
    const RenderEffectFieldRoles& roles) {
  const double tc = flight::maximum(0.0, flight::minimum(1.0, t));
  const auto left = flight::named_properties(a);
  const auto right = flight::named_properties(b);
  const auto output = flight::named_properties(out);

  std::vector<RenderEffectFieldSnapshot> fields;
  for (const auto& key : left.keys()) {
    collect_render_effect_field(fields, left, right, key);
  }
  for (const auto& key : right.keys()) {
    collect_render_effect_field(fields, left, right, key);
  }

  // Match the source's three write passes while retaining a complete snapshot
  // before the first mutation, including when out aliases either endpoint.
  for (const auto& field : fields) {
    if (field.kind != RenderEffectFieldKind::number) continue;
    if (field.left.is_undefined() || field.right.is_undefined()) {
      write_render_effect_field(output, field.key,
                                tc < 0.5 ? field.left : field.right);
      continue;
    }
    const double left_value = field.left.as_number();
    const double right_value = field.right.as_number();
    const double value = is_packed_render_effect_color(roles, a->kind,
                                                        field.key)
                             ? flight::color::lerp_color(left_value,
                                                         right_value, tc)
                             : left_value + (right_value - left_value) * tc;
    write_render_effect_field(output, field.key, flight::Any(value));
  }
  for (const auto& field : fields) {
    if (field.kind != RenderEffectFieldKind::boolean) continue;
    write_render_effect_field(output, field.key,
                              tc < 0.5 ? field.left : field.right);
  }
  for (const auto& field : fields) {
    if (field.kind != RenderEffectFieldKind::string) continue;
    write_render_effect_field(output, field.key,
                              tc < 0.5 ? field.left : field.right);
  }
  return true;
}

}  // namespace detail

template <typename Left, typename Right, typename Output>
inline bool lerp_render_effect(
    std::shared_ptr<Left> a, std::shared_ptr<Right> b, double t,
    std::shared_ptr<Output> out,
    RenderEffectFieldRoles roles = render_effect_field_roles) {
  if (a->kind != b->kind) return false;
  if constexpr (std::same_as<Left, Right> && std::same_as<Left, Output>) {
    return detail::lerp_exact_render_effect(std::move(a), std::move(b), t,
                                            std::move(out), roles);
  } else {
    throw flight::Error(flight::String(
        "lerpRenderEffect same-kind interpolation requires one retained exact effect owner"));
  }
}

}  // namespace flight::effects
