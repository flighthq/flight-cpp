// Normal semantic heritage must keep the original SWF parser usable and retain
// the same concrete allocation, inherited cells and shared-array lifetime.
#include <flight/effects/blur_effect.hpp>
#include <flight/swf/swf_filter.hpp>
#include <flight/types/effect.hpp>
#include <flight/types/entity.hpp>

#include <cstdio>
#include <memory>
#include <type_traits>

#define CHECK_EFFECT_BASE(Type) \
  static_assert(std::is_base_of_v<flight::types::Effect, flight::types::Type>); \
  static_assert(std::is_convertible_v<flight::Ref<flight::types::Type>, \
                                    flight::Ref<flight::types::Effect>>)
CHECK_EFFECT_BASE(BevelEffect);
CHECK_EFFECT_BASE(BlurEffect);
CHECK_EFFECT_BASE(ConvolutionEffect);
CHECK_EFFECT_BASE(DropShadowEffect);
CHECK_EFFECT_BASE(GradientBevelEffect);
CHECK_EFFECT_BASE(GradientGlowEffect);
CHECK_EFFECT_BASE(InnerGlowEffect);
CHECK_EFFECT_BASE(InnerShadowEffect);
CHECK_EFFECT_BASE(OuterGlowEffect);
#undef CHECK_EFFECT_BASE

template <typename Left, typename Right>
static bool same_owner(const Left& left, const Right& right) {
  return !left.owner_before(right) && !right.owner_before(left);
}

// Original byte lists supplied by builder6's qualified public-header consumer.
static int run(const char* name, flight::Uint8Array data,
               const char* expected_kind) {
  auto reader = flight::make_ref<flight::swf::SwfReader>(
      data, 0.0, static_cast<double>(data.size()));
  flight::Array<flight::Ref<flight::types::Effect>> effects;
  flight::Array<flight::Ref<flight::types::Adjustment>> adjustments;
  if (!flight::swf::read_swf_filter_list(reader, effects, adjustments,
                                        std::nullopt)) {
    std::printf("%s: parse failed\n", name);
    return 1;
  }
  if (effects.size() != 1 || adjustments.size() != 0) return 2;
  const auto effect = effects.element(0.0);
  if (!effect || effect->kind != flight::String(expected_kind)) return 3;
  auto alias = effects;
  if (alias.element(0.0).get() != effect.get() ||
      !same_owner(alias.element(0.0), effect)) return 4;
  effects.clear();
  if (!alias.empty() || effect->kind != flight::String(expected_kind)) return 5;
  return 0;
}

static int owner_and_cells() {
  auto concrete = flight::effects::create_blur_effect();
  flight::Ref<flight::types::Effect> base = concrete;
  if (!concrete || base.get() != concrete.get() || !same_owner(base, concrete))
    return 1;
  if (&base->kind != &concrete->kind ||
      &base->entity_runtime_key != &concrete->entity_runtime_key) return 2;
  if (base->kind != flight::String("BlurEffect")) return 3;

  auto runtime = flight::make_ref<flight::types::EntityRuntime>();
  base->entity_runtime_key = runtime;
  if (!concrete->entity_runtime_key.has_value() ||
      concrete->entity_runtime_key.value().get() != runtime.get() ||
      !same_owner(concrete->entity_runtime_key.value(), runtime)) return 4;
  concrete->entity_runtime_key.reset();
  if (base->entity_runtime_key.has_value()) return 5;

  std::weak_ptr<flight::types::BlurEffect> lifetime = concrete;
  flight::Array<flight::Ref<flight::types::Effect>> owners{base};
  auto alias = owners;
  concrete.reset();
  base.reset();
  if (lifetime.expired()) return 6;
  alias.clear();
  if (!owners.empty() || !lifetime.expired()) return 7;
  return 0;
}

int main() {
  using flight::Uint8Array;
  int result = run("drop", Uint8Array{1,0, 0,0,0,255, 0,0,0,0, 0,0,0,0,
                                    0,0,0,0, 0,0,0,0, 0,0, 0},
                   "DropShadowEffect");
  if (result) return 10 + result;
  result = run("inner-shadow", Uint8Array{1,0, 0,0,0,255, 0,0,0,0, 0,0,0,0,
                                         0,0,0,0, 0,0,0,0, 0,0, 128},
               "InnerShadowEffect");
  if (result) return 20 + result;
  result = run("outer-glow", Uint8Array{1,2, 0,0,0,255, 0,0,0,0, 0,0,0,0,
                                       0,0, 0}, "OuterGlowEffect");
  if (result) return 30 + result;
  result = run("inner-glow", Uint8Array{1,2, 0,0,0,255, 0,0,0,0, 0,0,0,0,
                                       0,0, 128}, "InnerGlowEffect");
  if (result) return 40 + result;
  result = run("blur", Uint8Array{1,1, 0,0,0,0, 0,0,0,0, 1}, "BlurEffect");
  if (result) return 50 + result;
  result = owner_and_cells();
  if (result) return 60 + result;
  std::puts("SWF original public header: five parsed lists and explicit owner/cell/lifetime checks passed");
  return 0;
}
