#include <flight/materials/classic_materials.hpp>
#include <flight/materials/extended_pbr_material.hpp>
#include <flight/materials/material_presets.hpp>
#include <flight/materials/pbr_materials.hpp>
#include <flight/materials/surface_material.hpp>
#include <flight/materials/unlit_materials.hpp>

#include <iostream>

namespace {

int failures = 0;

void check(bool condition, const char* message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

template <typename Material>
void check_surface_defaults(const Material& material, const char* family) {
  check(material->alpha_cutoff == 0.5, family);
  check(material->alpha_mode == flight::String("opaque"), family);
  check(material->blend_mode == flight::types::blend_mode->normal, family);
  check(!material->double_sided, family);
}

}  // namespace

int main() {
  namespace materials = flight::materials;
  namespace types = flight::types;

  const auto surface_options = flight::make_ref<types::SurfaceMaterial>();
  surface_options->alpha_cutoff = 0.25;
  surface_options->alpha_mode = flight::String("mask");
  surface_options->blend_mode = types::blend_mode->add;
  surface_options->double_sided = true;
  using SurfaceOptions = flight::RowReadonly<
      flight::RowPartial<flight::RowOf<flight::Ref<types::SurfaceMaterial>>>>;
  const auto surface = materials::create_surface_material<flight::Ref<types::SurfaceMaterial>>(
      flight::String("TestSurface"),
      std::optional<flight::StructuralRef<SurfaceOptions>>{
          flight::StructuralRef<SurfaceOptions>(surface_options)});
  check(surface->kind == flight::String("TestSurface"),
        "surface construction preserves the requested kind");
  check(surface->alpha_cutoff == 0.25 && surface->alpha_mode == flight::String("mask"),
        "surface construction forwards alpha options");
  check(surface->blend_mode == types::blend_mode->add && surface->double_sided,
        "surface construction forwards blend and sidedness options");
  check(materials::is_surface_material_masked(surface),
        "surface alpha-mode queries observe forwarded options");

  const auto classic = materials::create_blinn_phong_material();
  check(classic->kind == types::blinn_phong_material_kind,
        "classic construction retains its concrete kind");
  check_surface_defaults(classic, "classic construction applies the shared surface defaults");
  check(classic->shininess == 32.0 && classic->normal_scale == 1.0,
        "classic construction applies its upstream defaults");

  const auto specular = materials::create_specular_glossiness_pbr_material();
  check(specular->kind == types::specular_glossiness_pbr_material_kind,
        "specular PBR construction retains its concrete kind");
  check_surface_defaults(specular, "specular PBR construction applies the shared surface defaults");
  check(specular->glossiness == 1.0 && specular->occlusion_strength == 1.0,
        "specular PBR construction reads its complete structural schema");

  const auto standard = materials::create_standard_pbr_material();
  check(standard->kind == types::standard_pbr_material_kind,
        "standard PBR construction retains its concrete kind");
  check_surface_defaults(standard, "standard PBR construction applies the shared surface defaults");
  check(standard->metallic == 0.0 && standard->roughness == 1.0,
        "standard PBR construction applies its upstream defaults");

  const auto unlit = materials::create_unlit_material();
  check(unlit->kind == types::unlit_material_kind,
        "unlit construction retains its concrete kind");
  check_surface_defaults(unlit, "unlit construction applies the shared surface defaults");
  check(unlit->base_color == 4294967295.0 && !unlit->base_color_map.has_value(),
        "unlit construction applies its upstream defaults");

  const auto extended = materials::create_extended_pbr_material();
  check(extended->kind == types::extended_pbr_material_kind &&
            extended->extensions.empty(),
        "extended PBR construction supplies its kind and empty extension list");
  check(extended->standard != nullptr && extended->standard->metallic == 0.0 &&
            extended->standard->roughness == 1.0,
        "extended PBR construction creates the upstream standard block defaults");

  auto override_owner = std::make_shared<flight::RowOwner>();
  override_owner->set_named_value("roughness", std::optional<double>{0.6});
  const auto preset_overrides =
      materials::StandardPbrMaterialOptions::from_owner(override_owner);
  const auto gold = materials::create_gold_standard_pbr_material(preset_overrides);
  check(gold->base_color == 4292280575.0 && gold->metallic == 1.0 &&
            gold->roughness == 0.6,
        "preset fields apply before and caller partial fields override them");

  const auto aluminum = materials::create_aluminum_standard_pbr_material();
  const auto carbon = materials::create_carbon_standard_pbr_material();
  const auto iron = materials::create_iron_standard_pbr_material();
  const auto marble = materials::create_marble_standard_pbr_material();
  const auto plastic = materials::create_plastic_standard_pbr_material();
  const auto rubber = materials::create_rubber_standard_pbr_material();
  const auto silver = materials::create_silver_standard_pbr_material();
  const auto skin = materials::create_skin_standard_pbr_material();
  const auto wood = materials::create_wood_standard_pbr_material();
  check(aluminum->base_color == 2964369663.0 && carbon->base_color == 437918463.0 &&
            iron->base_color == 1145324799.0 && marble->base_color == 4126537215.0 &&
            plastic->base_color == 4294967295.0 && rubber->base_color == 471604479.0 &&
            silver->base_color == 3233857791.0 && skin->base_color == 4291598847.0 &&
            wood->base_color == 2337942527.0,
        "all upstream standard presets retain their packed base colors");

  auto transmission_owner = std::make_shared<flight::RowOwner>();
  transmission_owner->set_named_value("transmission", std::optional<double>{0.4});
  auto standard_owner = std::make_shared<flight::RowOwner>();
  standard_owner->set_named_value("roughness", std::optional<double>{0.2});
  const auto glass_options = flight::make_ref<types::GlassExtendedPbrMaterialOptions>();
  glass_options->alpha_mode = flight::String("blend");
  glass_options->double_sided = true;
  glass_options->transmission_volume =
      materials::TransmissionVolumeOptions::from_owner(transmission_owner);
  glass_options->standard =
      materials::StandardPbrPropertiesOptions::from_owner(standard_owner);
  const auto glass = materials::create_glass_extended_pbr_material(glass_options);
  check(glass->alpha_mode == flight::String("blend") && glass->double_sided,
        "glass forwards the shared surface options");
  check(glass->standard->base_color == 4294967295.0 &&
            glass->standard->metallic == 0.0 && glass->standard->roughness == 0.2,
        "glass merges its standard defaults with nested caller options");
  const auto extension = glass->extensions.get(0.0);
  check(extension.has_value() &&
            std::holds_alternative<
                flight::Ref<types::TransmissionVolumePbrExtension>>(
                extension.value()) &&
            std::get<flight::Ref<types::TransmissionVolumePbrExtension>>(
                extension.value())
                    ->kind == types::transmission_volume_pbr_extension_kind,
        "glass retains one ordered transmission extension through its base view");
  const auto transmission =
      std::get<flight::Ref<types::TransmissionVolumePbrExtension>>(
          extension.value());
  check(transmission->transmission == 0.4 && transmission->ior == 1.5,
        "glass retains the same concrete extension owner and nested overrides");

  return failures == 0 ? 0 : 1;
}
