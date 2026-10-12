#include <flight/shading/contract.hpp>

#include <iostream>
#include <memory>
#include <optional>
#include <variant>

namespace {

int failures = 0;

void check(bool condition, const char* message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

template <typename T>
auto readonly_view(const std::shared_ptr<T>& value) {
  using Writable = flight::StructuralRef<
      flight::RowWritable<flight::RowOf<flight::Ref<T>>>>;
  using Readonly = flight::StructuralRef<
      flight::RowReadonly<flight::RowOf<flight::Ref<T>>>>;
  return flight::structural_ref_cast<Readonly>(Writable(value));
}

template <typename T>
flight::shading::ModifierView modifier_view(const std::shared_ptr<T>& value) {
  return flight::structural_ref_cast<flight::shading::ModifierView>(readonly_view(value));
}

template <typename T>
flight::String signature(
    const flight::Ref<flight::types::ModifierDefinition>& definition,
    const std::shared_ptr<T>& modifier) {
  if (!definition->get_define_signature.has_value()) return flight::String("<missing>");
  return definition->get_define_signature.value()(modifier_view(modifier));
}

}  // namespace

int main() {
  namespace shading = flight::shading;
  namespace types = flight::types;

  const auto registry = shading::create_modifier_registry();
  shading::register_built_in_modifiers(registry);
  check(registry->definitions.size() == 8, "all eight built-ins register");
  const auto check_builtin = [&](const auto& definition,
                                 const flight::String& kind,
                                 const flight::String& slot,
                                 bool has_signature) {
    const auto registered = shading::resolve_modifier(readonly_view(registry), kind);
    check(registered.has_value() && registered.value() == definition,
          "registration retains each built-in definition owner");
    check(definition->kind == kind && definition->slot == slot,
          "each built-in definition retains its kind and slot");
    check(definition->get_define_signature.has_value() == has_signature,
          "only shape-varying built-ins install signature callbacks");
  };
  check_builtin(shading::animated_normal_modifier_definition,
                types::animated_normal_modifier_kind, types::modifier_slot->normal, true);
  check_builtin(shading::dissolve_modifier_definition,
                types::dissolve_modifier_kind, types::modifier_slot->effect, true);
  check_builtin(shading::emissive_modifier_definition,
                types::emissive_modifier_kind, types::modifier_slot->emissive, true);
  check_builtin(shading::env_reflect_modifier_definition,
                types::env_reflect_modifier_kind, types::modifier_slot->effect, false);
  check_builtin(shading::fog_modifier_definition,
                types::fog_modifier_kind, types::modifier_slot->effect, true);
  check_builtin(shading::rim_modifier_definition,
                types::rim_modifier_kind, types::modifier_slot->effect, false);
  check_builtin(shading::toon_modifier_definition,
                types::toon_modifier_kind, types::modifier_slot->effect, false);
  check_builtin(shading::vertex_displace_modifier_definition,
                types::vertex_displace_modifier_kind, types::modifier_slot->vertex, true);

  const auto replacement = flight::make_ref<types::ModifierDefinition>();
  replacement->kind = types::animated_normal_modifier_kind;
  replacement->slot = types::modifier_slot->effect;
  shading::register_modifier(readonly_view(registry), readonly_view(replacement));
  check(shading::resolve_modifier(readonly_view(registry), replacement->kind).value() == replacement,
        "registry replacement is last-write-wins");
  shading::register_built_in_modifiers(registry);
  check(shading::resolve_modifier(readonly_view(registry), replacement->kind).value() ==
            shading::animated_normal_modifier_definition,
        "re-registering built-ins restores their owners");

  const auto texture = flight::make_ref<types::Texture2D>();
  const types::Texture texture_value{
      std::in_place_type<flight::Ref<types::Texture2D>>, texture};

  const auto animated = flight::make_ref<types::AnimatedNormalModifier>();
  animated->kind = types::animated_normal_modifier_kind;
  animated->slot = types::modifier_slot->normal;
  check(signature(shading::animated_normal_modifier_definition, animated) == flight::String("0"),
        "animated normal distinguishes a null map");
  const auto erased_animated = flight::make_ref<types::Modifier>();
  erased_animated->kind = types::animated_normal_modifier_kind;
  erased_animated->slot = types::modifier_slot->normal;
  check(signature(shading::animated_normal_modifier_definition, erased_animated) ==
            flight::String("1"),
        "animated normal distinguishes an absent map cell from null");
  animated->map = texture_value;
  check(signature(shading::animated_normal_modifier_definition, animated) == flight::String("1"),
        "animated normal selects one layer");
  animated->secondary_map = texture_value;
  check(signature(shading::animated_normal_modifier_definition, animated) == flight::String("2"),
        "animated normal selects two layers");

  const auto dissolve = flight::make_ref<types::DissolveModifier>();
  dissolve->kind = types::dissolve_modifier_kind;
  dissolve->slot = types::modifier_slot->effect;
  check(signature(shading::dissolve_modifier_definition, dissolve).empty(),
        "procedural dissolve has no define suffix");
  dissolve->map = texture_value;
  check(signature(shading::dissolve_modifier_definition, dissolve) == flight::String("m"),
        "mapped dissolve selects its sampled variant");

  const auto emissive = flight::make_ref<types::EmissiveModifier>();
  emissive->kind = types::emissive_modifier_kind;
  emissive->slot = types::modifier_slot->emissive;
  check(signature(shading::emissive_modifier_definition, emissive).empty(),
        "plain emissive has no define suffix");
  emissive->mask = texture_value;
  check(signature(shading::emissive_modifier_definition, emissive) == flight::String("m"),
        "emissive mask contributes m");
  emissive->facing = types::emissive_modifier_facing->toward_light;
  check(signature(shading::emissive_modifier_definition, emissive) == flight::String("mg"),
        "emissive mask and facing gate compose in order");
  emissive->mask = std::nullopt;
  check(signature(shading::emissive_modifier_definition, emissive) == flight::String("g"),
        "emissive facing gate contributes g alone");
  emissive->facing = types::emissive_modifier_facing->ignore;
  check(signature(shading::emissive_modifier_definition, emissive).empty(),
        "the ignored facing mode does not split programs");

  const auto fog = flight::make_ref<types::FogModifier>();
  fog->kind = types::fog_modifier_kind;
  fog->slot = types::modifier_slot->effect;
  fog->mode = types::fog_modifier_mode->linear;
  check(signature(shading::fog_modifier_definition, fog) == flight::String("l"),
        "linear fog contributes l");
  fog->mode = types::fog_modifier_mode->exponential;
  check(signature(shading::fog_modifier_definition, fog) == flight::String("e"),
        "exponential fog contributes e");
  fog->mode = types::fog_modifier_mode->exponential2;
  check(signature(shading::fog_modifier_definition, fog) == flight::String("x"),
        "exponential-squared fog contributes x");

  const auto displace = flight::make_ref<types::VertexDisplaceModifier>();
  displace->kind = types::vertex_displace_modifier_kind;
  displace->slot = types::modifier_slot->vertex;
  displace->source = types::vertex_displace_modifier_source->sine;
  check(signature(shading::vertex_displace_modifier_definition, displace) == flight::String("s"),
        "sine displacement contributes s");
  displace->source = types::vertex_displace_modifier_source->height_map;
  check(signature(shading::vertex_displace_modifier_definition, displace) == flight::String("h"),
        "height-map displacement contributes h");
  displace->axis = flight::make_ref<types::Vector3>();
  check(signature(shading::vertex_displace_modifier_definition, displace) == flight::String("ha"),
        "fixed-axis displacement appends a");

  const auto key_registry = shading::create_modifier_registry();
  const auto normal_definition = flight::make_ref<types::ModifierDefinition>();
  normal_definition->kind = flight::String("acme.Normal");
  normal_definition->slot = types::modifier_slot->normal;
  normal_definition->get_define_signature = shading::SignatureFunction{
      [](shading::ModifierView) { return flight::String("n"); }};
  const auto effect_definition = flight::make_ref<types::ModifierDefinition>();
  effect_definition->kind = flight::String("acme.Effect");
  effect_definition->slot = types::modifier_slot->effect;
  shading::register_modifier(readonly_view(key_registry), readonly_view(normal_definition));
  shading::register_modifier(readonly_view(key_registry), readonly_view(effect_definition));

  const auto normal = flight::make_ref<types::Modifier>();
  normal->kind = normal_definition->kind;
  normal->slot = normal_definition->slot;
  const auto effect = flight::make_ref<types::Modifier>();
  effect->kind = effect_definition->kind;
  effect->slot = effect_definition->slot;
  const flight::Array<flight::Ref<types::Modifier>> authored{effect, normal};
  check(shading::get_modifier_define_key(authored, readonly_view(key_registry)) ==
            flight::String("acme.Normal:n+acme.Effect"),
        "define keys use readonly registry lookup and canonical slot order");
  check(shading::get_modifier_define_key(authored) ==
            flight::String("acme.Normal+acme.Effect"),
        "a missing registry falls back to bare kinds");

  if (failures == 0) std::cout << "shading registries and signatures match Flight\n";
  return failures == 0 ? 0 : 1;
}
