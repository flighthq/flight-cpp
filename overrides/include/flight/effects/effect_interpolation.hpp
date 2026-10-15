// Re-derived from the generated a627849 effect_interpolation module.
//
// The 7e2fc7d copy of this override existed to bridge the pre-rename module: it preserved
// RenderEffect* public names while naming the current Effect owners, because the generated module it
// shadowed still named removed headers. The a627849 move retires that reason -- the generated module
// now spells every name itself -- so what remains here is only what the emitter still refuses, under
// the generated module's own surface.
#pragma once

// PARTIAL: the emitter still refuses BOTH declarations this module exists to supply -- the
// EFFECT_FIELD_ROLES binding and lerpEffect -- for one reason, recorded verbatim in the generated
// file: `imported type EffectFieldRoles names an ambient type this runtime profile does not bind`.
// Everything else is the emitter's own output, reproduced here unchanged because an override replaces
// the whole header rather than patching it.
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

static_assert(flight::runtime_contract.compiler_contract == "flight-runtime-contract/2",
              "Flight compiler/runtime contract mismatch");
static_assert(flight::runtime_contract.cpp_abi == 1, "Flight C++ runtime ABI mismatch");

#include <flight/types/bevel_effect.hpp>
#include <flight/types/convolution_effect.hpp>
#include <flight/types/drop_shadow_effect.hpp>
#include <flight/types/effect.hpp>
#include <flight/types/effect_field_role.hpp>
#include <flight/types/effect_padding.hpp>
#include <flight/types/entity.hpp>
#include <flight/types/inner_glow_effect.hpp>
#include <flight/types/inner_shadow_effect.hpp>
#include <flight/types/outline_effect.hpp>
#include <flight/types/outer_glow_effect.hpp>

namespace flight::effects {

using flight::types::BevelEffect;
using flight::types::ConvolutionEffect;
using flight::types::DropShadowEffect;
using flight::types::InnerGlowEffect;
using flight::types::InnerShadowEffect;
using flight::types::OuterGlowEffect;
using flight::types::OutlineEffect;

// The emitter's own output for the source's canLerpEffects, unchanged.
inline bool can_lerp_effects(flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::Effect>>>> a, flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::Effect>>>> b) {
  return (flight::row_get<flight::RowKey<"kind">>(a) == flight::row_get<flight::RowKey<"kind">>(b));
}

// EFFECT_FIELD_ROLES, refused as an ambient-typed binding. Every key, value and insertion order is the
// source's; the type is the current EffectFieldRoles rather than the removed EffectFieldRoles owner.
inline flight::types::EffectFieldRoles effect_field_roles = {
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

enum class EffectFieldKind { number, boolean, string };

// One field's two endpoint values, snapshotted before any write so an aliased `out` cannot change what
// a later field reads.
struct EffectFieldSnapshot {
  flight::String key;
  flight::Any left;
  flight::Any right;
  EffectFieldKind kind;
};

inline bool contains_effect_key(const std::vector<EffectFieldSnapshot>& fields,
                                const flight::String& key) {
  return std::any_of(fields.begin(), fields.end(),
                     [&](const auto& field) { return field.key == key; });
}

// The source classifies a key by the FIRST of the two records that carries a classified value, which is
// why it walks `a` fully and then `b` only for keys `a` did not classify. Reproduced here rather than
// unioned, because the classified kind decides which write pass a field lands in.
inline void collect_effect_field(std::vector<EffectFieldSnapshot>& fields,
                                 const flight::NamedProperties& left,
                                 const flight::NamedProperties& right,
                                 const flight::String& key) {
  if (key == flight::String("kind") || contains_effect_key(fields, key)) return;
  const flight::Any left_value = left.get(key);
  const flight::Any right_value = right.get(key);
  if (left_value.kind() == flight::AnyKind::number ||
      right_value.kind() == flight::AnyKind::number) {
    fields.push_back({key, left_value, right_value, EffectFieldKind::number});
  } else if (left_value.kind() == flight::AnyKind::boolean ||
             right_value.kind() == flight::AnyKind::boolean) {
    fields.push_back({key, left_value, right_value, EffectFieldKind::boolean});
  } else if (left_value.kind() == flight::AnyKind::string ||
             right_value.kind() == flight::AnyKind::string) {
    fields.push_back({key, left_value, right_value, EffectFieldKind::string});
  }
}

inline bool is_packed_effect_color(const flight::types::EffectFieldRoles& roles,
                                   const flight::String& effect_kind,
                                   const flight::String& field) {
  const auto kind_roles = roles.get(effect_kind);
  if (!kind_roles.has_value()) return false;
  const auto role = kind_roles->get(field);
  return role.has_value() && role.value() == flight::String("packedColor");
}

inline void write_effect_field(const flight::NamedProperties& out,
                               const flight::String& key,
                               const flight::Any& value) {
  if (!out.set(key, value)) {
    throw std::logic_error(
        "lerpEffect could not write the retained exact-owner field");
  }
}

template <typename ExactEffect>
inline bool lerp_exact_effect(std::shared_ptr<ExactEffect> a,
                              std::shared_ptr<ExactEffect> b, double t,
                              std::shared_ptr<ExactEffect> out,
                              const flight::types::EffectFieldRoles& roles) {
  const double tc = flight::maximum(0.0, flight::minimum(1.0, t));
  const auto left = flight::named_properties(a);
  const auto right = flight::named_properties(b);
  const auto output = flight::named_properties(out);

  std::vector<EffectFieldSnapshot> fields;
  for (const auto& key : left.keys()) collect_effect_field(fields, left, right, key);
  for (const auto& key : right.keys()) collect_effect_field(fields, left, right, key);

  // The source's three write passes, after a complete snapshot, including when out aliases an endpoint.
  for (const auto& field : fields) {
    if (field.kind != EffectFieldKind::number) continue;
    if (field.left.is_undefined() || field.right.is_undefined()) {
      write_effect_field(output, field.key, tc < 0.5 ? field.left : field.right);
      continue;
    }
    const double left_value = field.left.as_number();
    const double right_value = field.right.as_number();
    const double value = is_packed_effect_color(roles, a->kind, field.key)
                             ? flight::color::lerp_color(left_value, right_value, tc)
                             : left_value + (right_value - left_value) * tc;
    write_effect_field(output, field.key, flight::Any(value));
  }
  for (const auto& field : fields) {
    if (field.kind != EffectFieldKind::boolean) continue;
    write_effect_field(output, field.key, tc < 0.5 ? field.left : field.right);
  }
  for (const auto& field : fields) {
    if (field.kind != EffectFieldKind::string) continue;
    write_effect_field(output, field.key, tc < 0.5 ? field.left : field.right);
  }
  return true;
}

}  // namespace detail

// lerpEffect, refused for the same unbound EffectFieldRoles ambient type.
//
// The source declares `a: Readonly<Effect>, b: Readonly<Effect>, out: Effect`, and the emitter's carrier
// for those is an erased structural view over flight::types::Effect. An erased view cannot recover a
// concrete effect owner, so the retained-exact-owner form is the only faithful lowering: same-kind calls
// across two different native owner types stop at an explicit error rather than reinterpreting, casting,
// copying, or partially writing an owner. The kind mismatch returns before any mutation.
template <typename Left, typename Right, typename Output>
inline bool lerp_effect(std::shared_ptr<Left> a, std::shared_ptr<Right> b, double t,
                        std::shared_ptr<Output> out,
                        const flight::types::EffectFieldRoles& roles = effect_field_roles) {
  if (a->kind != b->kind) return false;
  if constexpr (std::same_as<Left, Right> && std::same_as<Left, Output>) {
    return detail::lerp_exact_effect(std::move(a), std::move(b), t, std::move(out), roles);
  } else {
    throw flight::Error(flight::String(
        "lerpEffect same-kind interpolation requires one retained exact effect owner"));
  }
}

}  // namespace flight::effects
