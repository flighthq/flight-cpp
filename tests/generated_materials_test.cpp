#include <flight/materials/classic_materials.hpp>
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

  return failures == 0 ? 0 : 1;
}
