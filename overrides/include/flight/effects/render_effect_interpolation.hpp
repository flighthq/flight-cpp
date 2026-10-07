// Hand-written override for the generated render-effect field-role table. The generated module uses
// aggregate designated initializers for TypeScript object literals whose declared carrier is Record;
// this version constructs that same nested Record storage explicitly.
#pragma once

#include <flight/runtime.hpp>
#include <flight/types/render_effect.hpp>
#include <flight/types/render_effect_field_role.hpp>

namespace flight::effects {

using flight::types::RenderEffect;
using flight::types::RenderEffectFieldRoles;
using flight::types::RenderEffectKindFieldRoles;

inline bool can_lerp_render_effects(flight::Ref<RenderEffect> a, flight::Ref<RenderEffect> b) {
  return a->kind == b->kind;
}

inline flight::Ref<RenderEffectFieldRoles> render_effect_field_roles =
    flight::make_ref<RenderEffectFieldRoles>(RenderEffectFieldRoles{
        {flight::String("BevelEffect"),
         RenderEffectKindFieldRoles{{flight::String("highlightColor"), flight::String("packedColor")},
                                    {flight::String("shadowColor"), flight::String("packedColor")}}},
        {flight::String("ConvolutionEffect"),
         RenderEffectKindFieldRoles{{flight::String("color"), flight::String("packedColor")}}},
        {flight::String("DropShadowEffect"),
         RenderEffectKindFieldRoles{{flight::String("color"), flight::String("packedColor")}}},
        {flight::String("InnerGlowEffect"),
         RenderEffectKindFieldRoles{{flight::String("color"), flight::String("packedColor")}}},
        {flight::String("InnerShadowEffect"),
         RenderEffectKindFieldRoles{{flight::String("color"), flight::String("packedColor")}}},
        {flight::String("OuterGlowEffect"),
         RenderEffectKindFieldRoles{{flight::String("color"), flight::String("packedColor")}}},
        {flight::String("OutlineEffect"),
         RenderEffectKindFieldRoles{{flight::String("color"), flight::String("packedColor")}}},
        {flight::String("ScreenSpaceFogEffect"),
         RenderEffectKindFieldRoles{{flight::String("color"), flight::String("packedColor")}}},
        {flight::String("VignetteEffect"),
         RenderEffectKindFieldRoles{{flight::String("color"), flight::String("packedColor")}}},
        {flight::String("VolumetricLightEffect"),
         RenderEffectKindFieldRoles{{flight::String("lightColor"), flight::String("packedColor")}}},
    });

// lerp_render_effect remains omitted for the same target-runtime reason recorded by the generated
// header: RenderEffect has no owner-preserving mutable named-property boundary.

}  // namespace flight::effects
