#include <flight/shading/contract.hpp>

#include <iostream>
#include <memory>
#include <optional>
#include <utility>
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

// The modifier system's carriers are the patched value unions: stacks and options hold
// ModifierValue, definitions enter and leave the registry as ModifierDefinitionValue. These helpers
// build the arms that name the exact owner the assertions below already use, so every identity check
// still compares the same stored object.
flight::types::ModifierValue modifier_value(
    const flight::Ref<flight::types::Modifier>& modifier) {
  return flight::types::ModifierValue{
      std::in_place_type<flight::Ref<flight::types::Modifier>>, modifier};
}

flight::types::ModifierDefinitionValue definition_value(
    const flight::Ref<flight::types::ModifierDefinition>& definition) {
  return flight::types::ModifierDefinitionValue{
      std::in_place_type<flight::Ref<flight::types::ModifierDefinition>>, definition};
}

flight::Ref<flight::types::ModifierDefinition> definition_ref(
    const flight::types::ModifierDefinitionValue& value) {
  return std::get<flight::Ref<flight::types::ModifierDefinition>>(value);
}

template <typename T>
flight::String signature(
    const flight::Ref<flight::types::ModifierDefinition>& definition,
    const std::shared_ptr<T>& modifier) {
  if (!definition->get_define_signature.has_value()) return flight::String("<missing>");
  return definition->get_define_signature.value()(modifier_view(modifier));
}

bool texture_is(const std::optional<flight::types::Texture>& value,
                const flight::Ref<flight::types::Texture2D>& expected) {
  if (!value.has_value()) return false;
  const auto* texture = std::get_if<flight::Ref<flight::types::Texture2D>>(&value.value());
  return texture != nullptr && *texture == expected;
}

flight::Ref<flight::types::Modifier> make_modifier(flight::String kind,
                                                   const flight::String& slot) {
  const auto modifier = flight::make_ref<flight::types::Modifier>();
  modifier->kind = std::move(kind);
  modifier->slot = slot;
  return modifier;
}

void check_constructor_data() {
  namespace shading = flight::shading;
  namespace types = flight::types;

  const auto texture = flight::make_ref<types::Texture2D>();
  const types::Texture texture_value{
      std::in_place_type<flight::Ref<types::Texture2D>>, texture};
  const auto scroll = flight::make_ref<types::Vector2>(types::Vector2{.x = 0.1, .y = 0.0});
  const auto secondary_scroll =
      flight::make_ref<types::Vector2>(types::Vector2{.x = -0.05, .y = 0.02});

  const auto disabled_normal_options = flight::make_ref<types::AnimatedNormalModifierOptions>();
  disabled_normal_options->map = std::nullopt;
  disabled_normal_options->scroll = scroll;
  const auto disabled_normal =
      shading::create_animated_normal_modifier(readonly_view(disabled_normal_options));
  check(disabled_normal->kind == types::animated_normal_modifier_kind &&
            disabled_normal->slot == types::modifier_slot->normal,
        "animated-normal constructor fixes kind and Normal slot");
  check(!disabled_normal->map.has_value() && disabled_normal->strength == 1.0,
        "animated-normal accepts a null map and defaults strength exactly");
  check(disabled_normal->scroll == scroll && !disabled_normal->secondary_map.has_value() &&
            !disabled_normal->secondary_scroll.has_value(),
        "animated-normal retains scroll identity and leaves the second layer absent");

  const auto layered_normal_options = flight::make_ref<types::AnimatedNormalModifierOptions>();
  layered_normal_options->map = texture_value;
  layered_normal_options->scroll = scroll;
  layered_normal_options->strength = 0.5;
  layered_normal_options->secondary_map = texture_value;
  layered_normal_options->secondary_scroll = secondary_scroll;
  const auto layered_normal =
      shading::create_animated_normal_modifier(readonly_view(layered_normal_options));
  check(texture_is(layered_normal->map, texture) &&
            texture_is(layered_normal->secondary_map, texture),
        "animated-normal retains both texture owners");
  check(layered_normal->scroll == scroll &&
            layered_normal->secondary_scroll.value() == secondary_scroll &&
            layered_normal->strength == 0.5,
        "animated-normal retains vector owners and explicit strength");

  const auto dissolve_options = flight::make_ref<types::DissolveModifierOptions>();
  dissolve_options->threshold = 0.3;
  const auto dissolve = shading::create_dissolve_modifier(readonly_view(dissolve_options));
  check(dissolve->kind == types::dissolve_modifier_kind &&
            dissolve->slot == types::modifier_slot->effect && dissolve->threshold == 0.3,
        "dissolve constructor fixes identity and preserves threshold");
  check(dissolve->edge_color == static_cast<double>(0xff6600ffu) &&
            dissolve->edge_width == 0.05 && dissolve->scale == 8.0 &&
            !dissolve->map.has_value(),
        "dissolve defaults preserve packed RGBA and scalar layout");
  dissolve_options->edge_color = static_cast<double>(0x00ffddffu);
  dissolve_options->edge_width = 0.1;
  dissolve_options->map = texture_value;
  const auto mapped_dissolve =
      shading::create_dissolve_modifier(readonly_view(dissolve_options));
  check(mapped_dissolve->edge_color == static_cast<double>(0x00ffddffu) &&
            mapped_dissolve->edge_width == 0.1 && texture_is(mapped_dissolve->map, texture),
        "dissolve preserves exact packed color, width, and map identity");

  const auto emissive_options = flight::make_ref<types::EmissiveModifierOptions>();
  emissive_options->color = static_cast<double>(0x1234abffu);
  const auto emissive = shading::create_emissive_modifier(readonly_view(emissive_options));
  check(emissive->kind == types::emissive_modifier_kind &&
            emissive->slot == types::modifier_slot->emissive &&
            emissive->color == static_cast<double>(0x1234abffu),
        "emissive constructor preserves kind, slot, and packed RGBA");
  check(emissive->strength == 1.0 &&
            emissive->facing == types::emissive_modifier_facing->ignore &&
            emissive->facing_softness == 0.0 && !emissive->mask.has_value(),
        "emissive defaults preserve scalar and optional layout");
  emissive_options->strength = 4.0;
  emissive_options->facing = types::emissive_modifier_facing->away_from_light;
  emissive_options->facing_softness = 0.25;
  emissive_options->mask = texture_value;
  const auto gated_emissive =
      shading::create_emissive_modifier(readonly_view(emissive_options));
  check(gated_emissive->strength == 4.0 &&
            gated_emissive->facing == types::emissive_modifier_facing->away_from_light &&
            gated_emissive->facing_softness == 0.25 &&
            texture_is(gated_emissive->mask, texture),
        "emissive preserves explicit scalars and mask owner");

  const auto env_reflect = shading::create_env_reflect_modifier();
  check(env_reflect->kind == types::env_reflect_modifier_kind &&
            env_reflect->slot == types::modifier_slot->effect &&
            env_reflect->tint == static_cast<double>(0xffffffffu),
        "environment reflection defaults preserve identity and packed tint");
  check(env_reflect->intensity == 1.0 && env_reflect->fresnel_bias == 0.04 &&
            env_reflect->roughness == 0.0,
        "environment reflection defaults preserve shader scalars");
  const auto env_options = flight::make_ref<types::EnvReflectModifierOptions>();
  env_options->tint = static_cast<double>(0x88ccffffu);
  env_options->intensity = 0.5;
  env_options->fresnel_bias = 0.1;
  env_options->roughness = 0.6;
  const auto custom_env = shading::create_env_reflect_modifier(readonly_view(env_options));
  check(custom_env->tint == static_cast<double>(0x88ccffffu) &&
            custom_env->intensity == 0.5 && custom_env->fresnel_bias == 0.1 &&
            custom_env->roughness == 0.6,
        "environment reflection preserves explicit packed and scalar data");

  const auto fog_options = flight::make_ref<types::FogModifierOptions>();
  fog_options->color = static_cast<double>(0x1122ddffu);
  const auto fog = shading::create_fog_modifier(readonly_view(fog_options));
  check(fog->kind == types::fog_modifier_kind && fog->slot == types::modifier_slot->effect &&
            fog->color == static_cast<double>(0x1122ddffu),
        "fog constructor preserves identity and packed RGBA");
  check(fog->mode == types::fog_modifier_mode->linear && fog->near == 0.0 &&
            fog->far == 1.0 && fog->density == 1.0,
        "fog defaults preserve exact curve parameters");
  fog_options->mode = types::fog_modifier_mode->exponential2;
  fog_options->near = 2.0;
  fog_options->far = 90.0;
  fog_options->density = 0.05;
  const auto custom_fog = shading::create_fog_modifier(readonly_view(fog_options));
  check(custom_fog->mode == types::fog_modifier_mode->exponential2 &&
            custom_fog->near == 2.0 && custom_fog->far == 90.0 &&
            custom_fog->density == 0.05,
        "fog preserves explicit mode and distance parameters");

  const auto rim_options = flight::make_ref<types::RimModifierOptions>();
  rim_options->color = static_cast<double>(0x010203ffu);
  const auto rim = shading::create_rim_modifier(readonly_view(rim_options));
  check(rim->kind == types::rim_modifier_kind && rim->slot == types::modifier_slot->effect &&
            rim->color == static_cast<double>(0x010203ffu),
        "rim constructor preserves identity and packed RGBA");
  check(rim->power == 3.0 && rim->intensity == 1.0 && rim->bias == 0.0,
        "rim defaults preserve exact falloff parameters");
  rim_options->power = 5.0;
  rim_options->intensity = 2.0;
  rim_options->bias = 0.1;
  const auto custom_rim = shading::create_rim_modifier(readonly_view(rim_options));
  check(custom_rim->power == 5.0 && custom_rim->intensity == 2.0 &&
            custom_rim->bias == 0.1,
        "rim preserves explicit falloff parameters");

  const auto toon_options = flight::make_ref<types::ToonModifierOptions>();
  toon_options->steps = 4.0;
  const auto toon = shading::create_toon_modifier(readonly_view(toon_options));
  check(toon->kind == types::toon_modifier_kind && toon->slot == types::modifier_slot->effect &&
            toon->steps == 4.0 && toon->smoothness == 0.0,
        "toon preserves step count and defaults smoothness");
  toon_options->smoothness = 0.2;
  const auto smooth_toon = shading::create_toon_modifier(readonly_view(toon_options));
  check(smooth_toon->smoothness == 0.2, "toon preserves explicit smoothness");

  const auto displacement_options = flight::make_ref<types::VertexDisplaceModifierOptions>();
  displacement_options->source = types::vertex_displace_modifier_source->sine;
  displacement_options->amplitude = 0.2;
  const auto displacement =
      shading::create_vertex_displace_modifier(readonly_view(displacement_options));
  check(displacement->kind == types::vertex_displace_modifier_kind &&
            displacement->slot == types::modifier_slot->vertex &&
            displacement->source == types::vertex_displace_modifier_source->sine &&
            displacement->amplitude == 0.2,
        "vertex displacement preserves identity, source, and amplitude");
  check(displacement->frequency == 1.0 && displacement->speed == 1.0 &&
            displacement->direction.has_value() && displacement->direction.value()->x == 1.0 &&
            displacement->direction.value()->y == 0.0 &&
            displacement->direction.value()->z == 0.0 && !displacement->axis.has_value() &&
            !displacement->map.has_value(),
        "vertex displacement defaults preserve exact vector layout");
  const auto axis = flight::make_ref<types::Vector3>(types::Vector3{.x = 0.0, .y = 1.0, .z = 0.0});
  const auto direction =
      flight::make_ref<types::Vector3>(types::Vector3{.x = 0.0, .y = 0.0, .z = 1.0});
  displacement_options->source = types::vertex_displace_modifier_source->height_map;
  displacement_options->axis = axis;
  displacement_options->map = texture_value;
  displacement_options->frequency = 3.0;
  displacement_options->speed = 2.0;
  displacement_options->direction = direction;
  const auto mapped_displacement =
      shading::create_vertex_displace_modifier(readonly_view(displacement_options));
  check(mapped_displacement->axis.value() == axis &&
            mapped_displacement->direction.value() == direction &&
            texture_is(mapped_displacement->map, texture),
        "vertex displacement retains axis, direction, and map owners");
  check(mapped_displacement->frequency == 3.0 && mapped_displacement->speed == 2.0,
        "vertex displacement preserves explicit wave parameters");

  const auto material = shading::create_shaded_material();
  check(material->kind == types::shaded_material_kind &&
            material->diffuse == static_cast<double>(0xffffffffu) &&
            material->specular == static_cast<double>(0xffffffffu),
        "shaded material defaults preserve kind and packed colors");
  check(material->alpha_mode == flight::String("opaque") && material->alpha_cutoff == 0.5 &&
            material->blend_mode == types::blend_mode->normal && !material->double_sided,
        "shaded material defaults preserve exact Material3D trailer");
  check(material->shininess == 32.0 && material->normal_scale == 1.0 &&
            !material->diffuse_map.has_value() && !material->specular_map.has_value() &&
            !material->normal_map.has_value() && material->modifiers.empty(),
        "shaded material defaults preserve scalar, map, and stack layout");

  const auto authored_modifier = make_modifier("acme.Authored", types::modifier_slot->effect);
  const auto material_options = flight::make_ref<types::ShadedMaterialOptions>();
  material_options->alpha_cutoff = 0.25;
  material_options->alpha_mode =
      flight::make_ref<types::MaterialAlphaMode>(types::MaterialAlphaMode("mask"));
  material_options->double_sided = true;
  material_options->diffuse = static_cast<double>(0x102030ffu);
  material_options->diffuse_map = texture;
  material_options->normal_map = texture;
  material_options->normal_scale = 0.75;
  material_options->shininess = 8.0;
  material_options->specular = static_cast<double>(0x506070ffu);
  material_options->specular_map = texture;
  material_options->modifiers = flight::Array<types::ModifierValue>{modifier_value(authored_modifier)};
  const auto custom_material = shading::create_shaded_material(readonly_view(material_options));
  check(custom_material->alpha_mode == flight::String("mask") &&
            custom_material->alpha_cutoff == 0.25 && custom_material->double_sided,
        "shaded material forwards mask and sidedness parameters");
  check(custom_material->diffuse == static_cast<double>(0x102030ffu) &&
            custom_material->specular == static_cast<double>(0x506070ffu) &&
            custom_material->normal_scale == 0.75 && custom_material->shininess == 8.0,
        "shaded material preserves exact packed colors and scalar data");
  check(texture_is(custom_material->diffuse_map, texture) &&
            texture_is(custom_material->normal_map, texture) &&
            texture_is(custom_material->specular_map, texture),
        "shaded material retains every texture parameter owner");
  check(custom_material->modifiers.size() == 1 &&
            std::get<flight::Ref<types::Modifier>>(custom_material->modifiers[0]) ==
                authored_modifier,
        "shaded material retains modifier-array element identity");
}

void check_ordering_and_failures() {
  namespace shading = flight::shading;
  namespace types = flight::types;

  const auto vertex = make_modifier("Vertex", types::modifier_slot->vertex);
  const auto normal = make_modifier("Normal", types::modifier_slot->normal);
  const auto diffuse = make_modifier("Diffuse", types::modifier_slot->diffuse);
  const auto specular = make_modifier("Specular", types::modifier_slot->specular);
  const auto emissive = make_modifier("Emissive", types::modifier_slot->emissive);
  const auto effect_a = make_modifier("EffectA", types::modifier_slot->effect);
  const auto effect_b = make_modifier("EffectB", types::modifier_slot->effect);
  const auto unknown_a = make_modifier("UnknownA", flight::String("Ambient"));
  const auto unknown_b = make_modifier("UnknownB", flight::String("acme.Weird"));
  const flight::Array<types::ModifierValue> authored{
      modifier_value(unknown_b), modifier_value(effect_b), modifier_value(specular),
      modifier_value(vertex), modifier_value(effect_a), modifier_value(emissive),
      modifier_value(normal), modifier_value(unknown_a), modifier_value(diffuse)};
  const auto ordered = shading::order_modifier_stack(authored);
  const flight::Array<types::ModifierValue> expected{
      modifier_value(vertex), modifier_value(normal), modifier_value(diffuse),
      modifier_value(specular), modifier_value(emissive), modifier_value(effect_b),
      modifier_value(effect_a), modifier_value(unknown_b), modifier_value(unknown_a)};
  check(ordered.size() == expected.size(), "ordering retains every authored modifier");
  bool exact_order = ordered.size() == expected.size();
  for (std::size_t index = 0; exact_order && index < expected.size(); ++index) {
    exact_order = ordered[index] == expected[index];
  }
  check(exact_order,
        "ordering is Vertex-Normal-Diffuse-Specular-Emissive-Effect then stable unknowns");
  check(std::get<flight::Ref<types::Modifier>>(authored[0]) == unknown_b &&
            std::get<flight::Ref<types::Modifier>>(authored[1]) == effect_b &&
            std::get<flight::Ref<types::Modifier>>(authored[2]) == specular,
        "ordering does not mutate the authored stack");
  check(shading::order_modifier_stack({}).empty(), "ordering preserves an empty stack");

  check(shading::is_built_in_modifier_slot(types::modifier_slot->vertex) &&
            shading::is_built_in_modifier_slot(types::modifier_slot->normal) &&
            shading::is_built_in_modifier_slot(types::modifier_slot->diffuse) &&
            shading::is_built_in_modifier_slot(types::modifier_slot->specular) &&
            shading::is_built_in_modifier_slot(types::modifier_slot->emissive) &&
            shading::is_built_in_modifier_slot(types::modifier_slot->effect),
        "all six built-in shader slots are recognized");
  check(!shading::is_built_in_modifier_slot(flight::String("Ambient")) &&
            !shading::is_built_in_modifier_slot(flight::String("Shadow")) &&
            !shading::is_built_in_modifier_slot(flight::String("acme.Weird")) &&
            !shading::is_built_in_modifier_slot(flight::String("diffuse")) &&
            !shading::is_built_in_modifier_slot(flight::String("")),
        "reserved, vendor, case-mismatched, and empty slots fail recognition");

  const auto registry = shading::create_modifier_registry();
  check(registry->definitions.empty(), "a fresh modifier registry is empty");
  shading::register_built_in_modifiers(readonly_view(registry));
  const auto missing_a = make_modifier("acme.Missing", types::modifier_slot->effect);
  const auto missing_b = make_modifier("acme.Other", types::modifier_slot->normal);
  const auto registered =
      make_modifier(types::emissive_modifier_kind, types::modifier_slot->emissive);
  const auto missing = shading::get_unregistered_modifier_kinds(
      readonly_view(registry),
      flight::Array<types::ModifierValue>{modifier_value(missing_a), modifier_value(registered),
                                          modifier_value(missing_b), modifier_value(missing_a)});
  check(missing.size() == 2 && missing[0] == flight::String("acme.Missing") &&
            missing[1] == flight::String("acme.Other"),
        "unregistered failures deduplicate in first-seen order");
  check(!shading::is_modifier_stack_valid(
            readonly_view(registry),
            flight::Array<types::ModifierValue>{modifier_value(registered), modifier_value(missing_a)}),
        "a stack fails validation when any kind is unregistered");
  check(shading::is_modifier_stack_valid(
            readonly_view(registry),
            flight::Array<types::ModifierValue>{modifier_value(registered)}) &&
            shading::is_modifier_stack_valid(readonly_view(registry),
                                             flight::Array<types::ModifierValue>{}),
        "registered and empty stacks pass validation");
  check(!shading::resolve_modifier(readonly_view(registry), flight::String("acme.Missing"))
             .has_value(),
        "a registry miss returns the null sentinel");

  const auto fresh = shading::create_modifier_registry();
  const auto custom_definition = flight::make_ref<types::ModifierDefinition>();
  custom_definition->kind = flight::String("acme.Fresh");
  custom_definition->slot = types::modifier_slot->effect;
  shading::register_modifier(readonly_view(registry), definition_value(custom_definition));
  check(!shading::resolve_modifier(readonly_view(fresh), custom_definition->kind).has_value(),
        "modifier registries do not share definition storage");
}

}  // namespace

int main() {
  namespace shading = flight::shading;
  namespace types = flight::types;

  check_constructor_data();
  check_ordering_and_failures();

  const auto registry = shading::create_modifier_registry();
  shading::register_built_in_modifiers(readonly_view(registry));
  check(registry->definitions.size() == 8, "all eight built-ins register");
  const auto check_builtin = [&](const auto& definition,
                                 const flight::String& kind,
                                 const flight::String& slot,
                                 bool has_signature) {
    const auto registered = shading::resolve_modifier(readonly_view(registry), kind);
    check(registered.has_value() && definition_ref(registered.value()) == definition,
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
  shading::register_modifier(readonly_view(registry), definition_value(replacement));
  check(definition_ref(shading::resolve_modifier(readonly_view(registry), replacement->kind).value()) == replacement,
        "registry replacement is last-write-wins");
  shading::register_built_in_modifiers(readonly_view(registry));
  check(definition_ref(shading::resolve_modifier(readonly_view(registry), replacement->kind).value()) ==
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
      [](flight::types::ReadonlyModifierValue) { return flight::String("n"); }};
  const auto effect_definition = flight::make_ref<types::ModifierDefinition>();
  effect_definition->kind = flight::String("acme.Effect");
  effect_definition->slot = types::modifier_slot->effect;
  shading::register_modifier(readonly_view(key_registry), definition_value(normal_definition));
  shading::register_modifier(readonly_view(key_registry), definition_value(effect_definition));

  const auto normal = flight::make_ref<types::Modifier>();
  normal->kind = normal_definition->kind;
  normal->slot = normal_definition->slot;
  const auto effect = flight::make_ref<types::Modifier>();
  effect->kind = effect_definition->kind;
  effect->slot = effect_definition->slot;
  const flight::Array<types::ModifierValue> authored{modifier_value(effect), modifier_value(normal)};
  check(shading::get_modifier_define_key(authored, readonly_view(key_registry)) ==
            flight::String("acme.Normal:n+acme.Effect"),
        "define keys use readonly registry lookup and canonical slot order");
  check(shading::get_modifier_define_key(authored) ==
            flight::String("acme.Normal+acme.Effect"),
        "a missing registry falls back to bare kinds");

  if (failures == 0) std::cout << "shading registries and signatures match Flight\n";
  return failures == 0 ? 0 : 1;
}
