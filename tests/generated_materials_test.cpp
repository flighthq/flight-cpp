#include <flight/materials/anisotropy_pbr_extension.hpp>
#include <flight/materials/classic_materials.hpp>
#include <flight/materials/clearcoat_pbr_extension.hpp>
#include <flight/materials/color_scale_bias.hpp>
#include <flight/materials/custom_shader_material.hpp>
#include <flight/materials/extended_pbr_material.hpp>
#include <flight/materials/iridescence_pbr_extension.hpp>
#include <flight/materials/material.hpp>
#include <flight/materials/material3d.hpp>
#include <flight/materials/material_presets.hpp>
#include <flight/materials/material_validation.hpp>
#include <flight/materials/pbr_materials.hpp>
#include <flight/materials/phong_to_pbr.hpp>
#include <flight/materials/sheen_pbr_extension.hpp>
#include <flight/materials/specular_pbr_extension.hpp>
#include <flight/materials/standard_material.hpp>
#include <flight/materials/transmission_volume_pbr_extension.hpp>
#include <flight/materials/unlit_materials.hpp>
#include <flight/materials/wrapped_diffuse_pbr_extension.hpp>

#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace {

int failures = 0;
std::string mutation;

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

template <typename T>
using ReadonlyRow = flight::StructuralRef<
    flight::RowReadonly<flight::RowOf<flight::Ref<T>>>>;

template <typename T>
using PartialOptions = flight::StructuralRef<flight::RowReadonly<
    flight::RowPartial<flight::RowOf<flight::Ref<T>>>>>;

template <typename T>
ReadonlyRow<T> readonly(const std::shared_ptr<T>& value) {
  return ReadonlyRow<T>(value);
}

template <typename T>
ReadonlyRow<flight::types::Material> as_material(const std::shared_ptr<T>& value) {
  return ReadonlyRow<flight::types::Material>(
      flight::StructuralRef<
          flight::RowWritable<flight::RowOf<flight::Ref<T>>>>(value));
}

template <typename T>
PartialOptions<T> options(const std::shared_ptr<flight::RowOwner>& owner) {
  return PartialOptions<T>::from_owner(owner);
}

bool near(double actual, double expected, double tolerance = 1e-9) {
  return std::abs(actual - expected) <= tolerance;
}

}  // namespace

int main(int argc, char** argv) {
  namespace materials = flight::materials;
  namespace types = flight::types;

  if (argc == 2) mutation = argv[1];

  const auto surface_options = flight::make_ref<types::Material3DOptions>();
  surface_options->alpha_cutoff = 0.25;
  surface_options->alpha_mode = types::MaterialAlphaMode(flight::String("mask"));
  surface_options->blend_mode = types::blend_mode->add;
  surface_options->double_sided = true;
  const auto surface = materials::create_material_3d<flight::Ref<types::Material3D>>(
      flight::String("TestSurface"),
      std::optional<ReadonlyRow<types::Material3DOptions>>{
          ReadonlyRow<types::Material3DOptions>(surface_options)});
  check(surface->kind == flight::String("TestSurface"),
        "surface construction preserves the requested kind");
  check(std::holds_alternative<flight::Null>(surface->name) &&
            !std::holds_alternative<flight::Undefined>(surface->name),
        "material construction initializes name to null rather than undefined");
  check(surface->alpha_cutoff == 0.25 && surface->alpha_mode == flight::String("mask"),
        "surface construction forwards alpha options");
  check(surface->blend_mode == types::blend_mode->add && surface->double_sided,
        "surface construction forwards blend and sidedness options");
  check(materials::is_material_3d_masked(readonly(surface)) &&
            !materials::is_material_3d_opaque(readonly(surface)) &&
            !materials::is_material_3d_blended(readonly(surface)),
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

  const auto lambert = materials::create_lambert_material();
  const auto phong = materials::create_phong_material();
  check(lambert->kind == types::lambert_material_kind &&
            lambert->diffuse == 4294967295.0 && lambert->emissive == 255.0,
        "Lambert construction applies its exact packed-color defaults");
  check(phong->kind == types::phong_material_kind && phong->shininess == 32.0 &&
            phong->normal_scale == 1.0,
        "Phong construction applies its exact scalar defaults");

  const auto depth = materials::create_depth_material();
  const auto emissive = materials::create_emissive_material();
  const auto matcap = materials::create_matcap_material();
  const auto normal = materials::create_normal_material();
  const auto toon = materials::create_toon_material();
  const auto vertex = materials::create_vertex_color_material();
  const auto wireframe = materials::create_wireframe_material();
  check(depth->near == 0.0 && depth->far == 1.0 &&
            emissive->emissive == 4294967295.0 &&
            emissive->emissive_strength == 1.0,
        "depth and emissive materials retain their upstream numeric defaults");
  check(!matcap->matcap.has_value() && matcap->tint == 4294967295.0 &&
            !normal->normal_map.has_value() && normal->normal_scale == 1.0,
        "matcap and normal materials retain their null-map defaults");
  check(toon->base_color == 4294967295.0 && toon->steps == 3.0 &&
            vertex->tint == 4294967295.0 &&
            wireframe->color == 4294967295.0 && wireframe->thickness == 1.0,
        "toon, vertex-color, and wireframe defaults match upstream exactly");

  const auto standard2d = materials::create_standard_material();
  check(standard2d->kind == types::standard_material_kind &&
            std::holds_alternative<flight::Null>(standard2d->name),
        "standard 2D material preserves its native kind and null name sentinel");

  const auto extended = materials::create_extended_pbr_material();
  check(extended->kind == types::extended_pbr_material_kind &&
            extended->extensions.empty(),
        "extended PBR construction supplies its kind and empty extension list");
  check(extended->standard != nullptr && extended->standard->metallic == 0.0 &&
            extended->standard->roughness == 1.0,
        "extended PBR construction creates the upstream standard block defaults");

  standard->name = flight::String("canopy");
  const auto standard_view = as_material(standard);
  auto cloned = materials::clone_material(readonly(standard));
  if (mutation == "--mutate-identity") cloned = standard;
  check(static_cast<const void*>(cloned.get()) !=
            static_cast<const void*>(standard.get()) &&
            cloned->kind == types::standard_pbr_material_kind,
        "material cloning creates a distinct native entity with the same kind");
  check(std::holds_alternative<flight::String>(cloned->name) &&
            std::get<flight::String>(cloned->name) == flight::String("canopy") &&
            materials::equals_material(standard_view, as_material(cloned)),
        "material cloning retains authored scalar state and structural equality");
  check(cloned->metallic == standard->metallic &&
            cloned->roughness == standard->roughness,
        "material cloning copies concrete Standard-PBR scalar fields");
  const auto copied = materials::create_standard_pbr_material();
  materials::copy_material(copied, readonly(standard));
  check(copied->metallic == standard->metallic &&
            copied->roughness == standard->roughness &&
            copied->name == standard->name,
        "material copying retains concrete scalar fields and authored name");
  materials::copy_material(copied, readonly(copied));
  check(copied->kind == types::standard_pbr_material_kind,
        "material copying is alias-safe for the exact native owner");

  const auto matched =
      materials::get_material_of_kind<flight::Ref<types::StandardPbrMaterial>>(
          std::optional<ReadonlyRow<types::Material>>{standard_view},
          types::standard_pbr_material_kind);
  const auto mismatched =
      materials::get_material_of_kind<flight::Ref<types::StandardPbrMaterial>>(
          std::optional<ReadonlyRow<types::Material>>{standard_view},
          flight::String("OtherMaterial"));
  const auto absent =
      materials::get_material_of_kind<flight::Ref<types::StandardPbrMaterial>>(
          std::nullopt, types::standard_pbr_material_kind);
  check(matched.has_value() && matched.value() == standard &&
            !mismatched.has_value() && !absent.has_value(),
        "material discriminant selection retains the exact matching owner and rejects mismatch and absence");

  const auto standard_block = materials::create_standard_pbr_material_properties();
  standard_block->roughness = 0.25;
  const auto clearcoat_base = flight::make_ref<types::PbrExtension>();
  clearcoat_base->entity_runtime_key = std::nullopt;
  clearcoat_base->kind = types::clearcoat_pbr_extension_kind;
  const auto vendor_base = flight::make_ref<types::PbrExtension>();
  vendor_base->entity_runtime_key = std::nullopt;
  vendor_base->kind = flight::String("VendorPbrExtension");
  flight::Array<types::PbrExtensionValue> ordered_extensions{
      clearcoat_base, vendor_base};
  auto extended_owner = std::make_shared<flight::RowOwner>();
  extended_owner->set_named_value(
      "extensions",
      std::optional<flight::Array<types::PbrExtensionValue>>{
          ordered_extensions});
  extended_owner->set_named_value(
      "standard",
      std::optional<flight::Ref<types::StandardPbrMaterialProperties>>{
          standard_block});
  const auto ordered = materials::create_extended_pbr_material(
      options<types::ExtendedPbrMaterial>(extended_owner));
  if (mutation == "--mutate-order") {
    ordered->extensions =
        flight::Array<types::PbrExtensionValue>{vendor_base, clearcoat_base};
  }
  check(ordered->standard == standard_block &&
            ordered->extensions.identity() == ordered_extensions.identity(),
        "extended PBR preserves the exact standard block and extension-list identities");
  check(std::get<flight::Ref<types::PbrExtension>>(ordered->extensions.get(0.0).value())->kind ==
                types::clearcoat_pbr_extension_kind &&
            std::get<flight::Ref<types::PbrExtension>>(ordered->extensions.get(1.0).value())->kind ==
                flight::String("VendorPbrExtension"),
        "extended PBR retains extension program-selection order");

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
  check(glass->extensions.size() == 1 && extension.has_value() &&
            std::holds_alternative<flight::Ref<types::TransmissionVolumePbrExtension>>(
                extension.value()),
        "glass retains exactly one concrete transmission owner in the ordered value list");
  const auto transmission = std::get<flight::Ref<types::TransmissionVolumePbrExtension>>(
      extension.value());
  check(transmission->kind == types::transmission_volume_pbr_extension_kind &&
            transmission->transmission == 0.4 && transmission->ior == 1.5,
        "glass retains the concrete extension fields and nested caller overrides");
  const auto extension_list_alias = glass->extensions;
  const auto transmission_alias =
      std::get<flight::Ref<types::TransmissionVolumePbrExtension>>(
          extension_list_alias.get(0.0).value());
  transmission_alias->thickness = 2.75;
  check(extension_list_alias.identity() == glass->extensions.identity() &&
            transmission_alias == transmission && transmission->thickness == 2.75 &&
            !transmission_alias.owner_before(transmission) &&
            !transmission.owner_before(transmission_alias),
        "glass value-list aliases retain the same concrete allocation and control block");
  const auto default_glass = materials::create_glass_extended_pbr_material();
  const auto default_transmission =
      std::get<flight::Ref<types::TransmissionVolumePbrExtension>>(
          default_glass->extensions.get(0.0).value());
  check(default_glass->extensions.size() == 1 &&
            default_glass->extensions.identity() != glass->extensions.identity() &&
            default_glass->standard != glass->standard &&
            default_transmission != transmission && default_transmission->ior == 1.5 &&
            default_transmission->transmission == 1.0 &&
            default_transmission->thickness == 0.0 &&
            default_glass->standard->base_color == 4294967295.0 &&
            default_glass->standard->metallic == 0.0 &&
            default_glass->standard->roughness == 0.0,
        "default glass creates fresh ordered storage and concrete owners with source defaults");

  using UniformValue = std::variant<flight::Array<double>, double>;
  flight::Record<flight::String, UniformValue> uniforms;
  uniforms.set(flight::String("u_time"), UniformValue{1.5});
  uniforms.set(flight::String("u_scale"),
               UniformValue{flight::Array<double>{2.0, 3.0}});
  auto shader_owner = std::make_shared<flight::RowOwner>();
  shader_owner->set_named_value(
      "shaderKey", std::optional<flight::String>{flight::String("myShader")});
  shader_owner->set_named_value(
      "uniforms",
      std::optional<flight::Record<flight::String, UniformValue>>{uniforms});
  const auto shader = materials::create_custom_shader_material(
      options<types::CustomShaderMaterial>(shader_owner));
  if (mutation == "--mutate-shader") shader->shader_key = flight::String("other");
  check(shader->kind == types::custom_shader_material_kind &&
            shader->shader_key == flight::String("myShader"),
        "custom material links the requested shader key to its native kind");
  check(shader->uniforms.has_value() &&
            shader->uniforms->identity() == uniforms.identity() &&
            std::get<double>(shader->uniforms->get(flight::String("u_time")).value()) ==
                1.5 &&
            shader->uniforms->get(flight::String("u_scale")).has_value() &&
            std::get<flight::Array<double>>(
                shader->uniforms->get(flight::String("u_scale")).value())
                    .get(1.0)
                    .value() == 3.0,
        "custom material retains the uniform bag identity and exact scalar/vector values");

  const auto color_source = materials::create_color_scale_bias();
  const auto color_other = materials::create_color_scale_bias();
  const auto color_out = materials::create_color_scale_bias();
  materials::set_color_scale_bias(color_source, 2.0, 3.0, 4.0, 5.0,
                                  1.0, 2.0, 3.0, 4.0);
  materials::set_color_scale_bias(color_other, 10.0, 20.0, 30.0, 40.0,
                                  5.0, 6.0, 7.0, 8.0);
  materials::concat_color_scale_bias(color_out, readonly(color_source),
                                     readonly(color_other));
  if (mutation == "--mutate-color") color_out->red_bias = 0.0;
  check(color_out->red_scale == 20.0 && color_out->green_scale == 60.0 &&
            color_out->blue_scale == 120.0 && color_out->alpha_scale == 200.0,
        "color-scale concatenation multiplies all four channels exactly");
  check(color_out->red_bias == 11.0 && color_out->green_bias == 20.0 &&
            color_out->blue_bias == 31.0 && color_out->alpha_bias == 44.0,
        "color-scale concatenation applies source.scale * other.bias + source.bias");
  materials::set_color_scale_bias_bias_rgba(color_out, 305419896.0);
  check(materials::get_color_scale_bias_bias_rgba(readonly(color_out)) ==
                305419896.0 &&
            near(color_out->red_bias, 18.0 / 255.0) &&
            near(color_out->alpha_bias, 120.0 / 255.0),
        "packed RGBA color bias round-trips with exact channel order");
  materials::set_color_scale_bias_identity(color_out);
  check(materials::is_identity_color_scale_bias(readonly(color_out)),
        "color-scale identity reset restores all eight canonical values");

  auto spec_owner = std::make_shared<flight::RowOwner>();
  spec_owner->set_named_value("glossiness", std::optional<double>{0.75});
  spec_owner->set_named_value("specular", std::optional<double>{255.0});
  const auto spec_source = materials::create_specular_glossiness_pbr_material(
      options<types::SpecularGlossinessPbrMaterial>(spec_owner));
  const auto converted = materials::create_standard_pbr_material_properties();
  materials::convert_specular_glossiness_to_standard_pbr(
      converted, readonly(spec_source));
  if (mutation == "--mutate-conversion") converted->roughness = 0.5;
  check(near(converted->roughness, 0.25, 1e-12) && converted->metallic == 0.0,
        "specular-glossiness conversion maps glossiness and dielectric metallic exactly");
  check(near(materials::get_pbr_roughness_from_phong_shininess(32.0),
             0.24253562503633297, 1e-12) &&
            materials::get_pbr_metallic_from_phong_specular(
                4294967295.0, 255.0) == 1.0 &&
            near(std::exp2(materials::get_phong_to_pbr_light_exposure()),
                 std::acos(-1.0), 1e-12),
        "Phong conversion helpers preserve the upstream numeric mapping");

  const auto clamped = materials::create_standard_pbr_material_properties();
  clamped->metallic = 1.5;
  clamped->roughness = -1.0;
  clamped->occlusion_strength = 3.0;
  clamped->emissive_strength = -2.0;
  clamped->normal_scale = 5.0;
  const auto clamped_identity =
      materials::clamp_standard_pbr_material_properties(clamped);
  if (mutation == "--mutate-clamp") clamped->roughness = 0.5;
  check(clamped_identity == clamped && clamped->metallic == 1.0 &&
            clamped->roughness == 0.0 && clamped->occlusion_strength == 1.0 &&
            clamped->emissive_strength == 0.0 && clamped->normal_scale == 5.0,
        "PBR clamping mutates and returns the same native property block");
  check(materials::is_valid_material_weight(0.0) &&
            materials::is_valid_material_weight(1.0) &&
            !materials::is_valid_material_weight(-0.001) &&
            !materials::is_valid_material_weight(
                std::numeric_limits<double>::infinity()) &&
            materials::is_valid_material_ior(1.5) &&
            !materials::is_valid_material_ior(0.9) &&
            !materials::is_valid_material_ior(
                std::numeric_limits<double>::quiet_NaN()) &&
            materials::is_valid_pbr_uv_set(0.0) &&
            materials::is_valid_pbr_uv_set(1.0) &&
            !materials::is_valid_pbr_uv_set(2.0),
        "material validators accept only their finite canonical domains");

  auto anisotropy_owner = std::make_shared<flight::RowOwner>();
  anisotropy_owner->set_named_value("anisotropyMapUvSet",
                                    std::optional<double>{1.0});
  const auto anisotropy = materials::create_anisotropy_pbr_extension(
      options<types::AnisotropyPbrExtension>(anisotropy_owner));
  const auto clearcoat = materials::create_clearcoat_pbr_extension();
  const auto iridescence = materials::create_iridescence_pbr_extension();
  const auto sheen = materials::create_sheen_pbr_extension();
  const auto specular_extension = materials::create_specular_pbr_extension();
  const auto transport = materials::create_transmission_volume_pbr_extension();
  const auto wrapped = materials::create_wrapped_diffuse_pbr_extension();
  check(readonly(anisotropy).shared_exact_object<types::AnisotropyPbrExtension>() ==
                anisotropy &&
            !anisotropy->entity_runtime_key.has_value() &&
            anisotropy->anisotropy_strength == 0.0 &&
            anisotropy->anisotropy_map_uv_set == 1.0 &&
            materials::is_valid_anisotropy_pbr_extension(readonly(anisotropy)),
        "anisotropy construction retains exact entity identity, undefined runtime sentinel, defaults, and UV selection");
  check(materials::is_valid_clearcoat_pbr_extension(readonly(clearcoat)) &&
            materials::is_valid_iridescence_pbr_extension(readonly(iridescence)) &&
            materials::is_valid_sheen_pbr_extension(readonly(sheen)) &&
            materials::is_valid_specular_pbr_extension(readonly(specular_extension)) &&
            materials::is_valid_wrapped_diffuse_pbr_extension(readonly(wrapped)),
        "default PBR extension constructors produce valid concrete native entities");
  check(clearcoat->clearcoat == 0.0 && clearcoat->clearcoat_roughness == 0.0 &&
            clearcoat->clearcoat_normal_scale == 1.0 &&
            clearcoat->clearcoat_map_uv_set == 0.0 &&
            iridescence->iridescence == 0.0 && iridescence->iridescence_ior == 1.3 &&
            iridescence->iridescence_thickness_min == 100.0 &&
            iridescence->iridescence_thickness_max == 400.0,
        "clearcoat and iridescence constructors preserve exact glTF defaults");
  check(sheen->sheen_color == 255.0 && sheen->sheen_roughness == 0.0 &&
            sheen->sheen_color_map_uv_set == 0.0 &&
            specular_extension->specular == 1.0 &&
            specular_extension->specular_color == 4294967295.0 &&
            specular_extension->specular_map_uv_set == 0.0 &&
            wrapped->wrapped_diffuse_strength == 0.0 &&
            wrapped->wrapped_diffuse_color == 4294967295.0 &&
            wrapped->thickness == 0.0,
        "sheen, specular, and wrapped-diffuse constructors preserve exact numeric defaults");
  check(std::isinf(transport->attenuation_distance) && transport->ior == 1.5 &&
            transport->attenuation_color == 4294967295.0 &&
            transport->thickness == 0.0 && transport->transmission == 0.0 &&
            transport->thickness_map_uv_set == 0.0 &&
            transport->transmission_map_uv_set == 0.0 &&
            materials::is_valid_transmission_volume_pbr_extension(
                readonly(transport)),
        "transmission-volume construction preserves glTF transport defaults");
  anisotropy->anisotropy_strength = 2.0;
  transport->ior = 0.5;
  if (mutation == "--mutate-validation") {
    anisotropy->anisotropy_strength = 0.0;
  }
  check(!materials::is_valid_anisotropy_pbr_extension(readonly(anisotropy)) &&
            !materials::is_valid_transmission_volume_pbr_extension(
                readonly(transport)),
        "extension validators reject invalid strength and transport relationships");

  const auto explanation =
      flight::make_ref<types::MaterialConversionExplanation>();
  explanation->dropped_maps =
      flight::Array<flight::String>{flight::String("specularMap")};
  explanation->reason = flight::String("unsupported-by-target-model");
  std::vector<flight::String> seen_conversions;
  materials::set_material_conversion_guard(types::MaterialConversionGuard{
      [&](ReadonlyRow<types::MaterialConversionExplanation>,
          flight::String conversion) { seen_conversions.push_back(conversion); }});
  materials::report_material_conversion_drop(readonly(explanation),
                                             flight::String("first"));
  if (mutation != "--mutate-lifecycle") {
    materials::set_material_conversion_guard(std::nullopt);
  }
  materials::report_material_conversion_drop(readonly(explanation),
                                             flight::String("second"));
  materials::set_material_conversion_guard(std::nullopt);
  check(seen_conversions.size() == 1 &&
            seen_conversions.front() == flight::String("first"),
        "conversion guard routes while installed and stops immediately after uninstall");

  return failures == 0 ? 0 : 1;
}
