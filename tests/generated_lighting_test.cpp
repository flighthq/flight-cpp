#include <flight/geometry/bounding_sphere.hpp>
#include <flight/geometry/vector3.hpp>
#include <flight/lighting/ambient_light.hpp>
#include <flight/lighting/area_light.hpp>
#include <flight/lighting/directional_light.hpp>
#include <flight/lighting/environment.hpp>
#include <flight/lighting/hemisphere_light.hpp>
#include <flight/lighting/light_analysis.hpp>
#include <flight/lighting/light_intensity.hpp>
#include <flight/lighting/light_probe.hpp>
#include <flight/lighting/point_light.hpp>
#include <flight/lighting/scene_forward_lights.hpp>
#include <flight/lighting/scene_lights.hpp>
#include <flight/lighting/spot_light.hpp>

#include <cmath>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <variant>

namespace {

int failures = 0;
std::string mutation;

void check(bool condition, const char* message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

bool near(double actual, double expected, double tolerance = 1e-6) {
  return std::abs(actual - expected) <= tolerance;
}

template <typename T>
using ReadonlyRef = flight::StructuralRef<
    flight::RowReadonly<flight::RowOf<flight::Ref<T>>>>;

template <typename T>
ReadonlyRef<T> readonly(const std::shared_ptr<T>& value) {
  return ReadonlyRef<T>(value);
}

flight::Ref<flight::types::Vector3> vector3(double x, double y, double z) {
  return flight::geometry::create_vector3(x, y, z);
}

flight::Ref<flight::types::BoundingSphere> sphere(
    double x, double y, double z, double radius) {
  return flight::geometry::create_bounding_sphere(x, y, z, radius);
}

flight::Ref<flight::types::LightProbe> tagged_probe(double value) {
  auto probe = flight::lighting::create_light_probe(readonly(vector3(0, 0, 0)));
  probe->sh_coefficients.set_index(0, value);
  return probe;
}

}  // namespace

int main(int argc, char** argv) {
  namespace lighting = flight::lighting;
  namespace types = flight::types;

  if (argc == 2) mutation = argv[1];

  const auto ambient_options = flight::make_ref<types::AmbientLightOptions>();
  ambient_options->color = 0x102030ff;
  ambient_options->enabled = false;
  ambient_options->intensity = 2.5;
  ambient_options->intensity_unit = types::lux_light_unit;
  const auto ambient = lighting::create_ambient_light(readonly(ambient_options));
  check(ambient->kind == types::ambient_light_kind &&
            ambient->color == 0x102030ff && !ambient->enabled &&
            ambient->intensity == 2.5 &&
            ambient->intensity_unit == types::lux_light_unit,
        "ambient construction propagates every authored option exactly");
  const auto ambient_clone = lighting::clone_ambient_light(readonly(ambient));
  check(ambient_clone != ambient && ambient_clone->color == ambient->color &&
            ambient_clone->enabled == ambient->enabled &&
            ambient_clone->intensity == ambient->intensity,
        "ambient cloning creates a distinct owner and copies scalar state");

  const auto hemisphere_options = flight::make_ref<types::HemisphereLightOptions>();
  hemisphere_options->ground_color = 0x010203ff;
  hemisphere_options->sky_color = 0xa0b0c0ff;
  hemisphere_options->intensity = 3.0;
  hemisphere_options->intensity_unit = types::candela_light_unit;
  const auto hemisphere =
      lighting::create_hemisphere_light(readonly(hemisphere_options));
  check(hemisphere->kind == types::hemisphere_light_kind &&
            hemisphere->ground_color == 0x010203ff &&
            hemisphere->sky_color == 0xa0b0c0ff &&
            hemisphere->intensity == 3.0 &&
            hemisphere->intensity_unit == types::candela_light_unit,
        "hemisphere construction preserves colors, intensity, and units");

  const auto direction_source = vector3(0, -4, 3);
  flight::Array<double> cascade_splits{0.1, 0.35, 1.0};
  const auto directional_options =
      flight::make_ref<types::DirectionalLightOptions>();
  directional_options->cascade_count = 3.0;
  directional_options->cascade_splits = cascade_splits;
  directional_options->casts_shadow = true;
  directional_options->color = 0xffeeddff;
  directional_options->direction = readonly(direction_source);
  directional_options->normal_bias = 0.125;
  directional_options->pcf_radius = 1.5;
  directional_options->shadow_bias = -0.002;
  directional_options->shadow_far = 900.0;
  directional_options->shadow_map_size = 2048.0;
  directional_options->shadow_near = 0.25;
  directional_options->shadow_strength = 0.75;
  const auto directional =
      lighting::create_directional_light(readonly(directional_options));
  check(directional->kind == types::directional_light_kind &&
            directional->cascade_count == 3.0 &&
            directional->cascade_splits.identity() != cascade_splits.identity() &&
            directional->cascade_splits.size() == 3 &&
            directional->cascade_splits.get(1).value() == 0.35,
        "directional construction copies ordered cascade configuration");
  check(directional->casts_shadow && directional->normal_bias == 0.125 &&
            directional->pcf_radius == 1.5 &&
            directional->shadow_bias == -0.002 &&
            directional->shadow_far == 900.0 &&
            directional->shadow_map_size == 2048.0 &&
            directional->shadow_near == 0.25 &&
            directional->shadow_strength == 0.75,
        "directional construction preserves exact shadow configuration");
  check(directional->direction != direction_source &&
            near(directional->direction->x, 0.0) &&
            near(directional->direction->y, -0.8) &&
            near(directional->direction->z, 0.6),
        "directional construction owns and normalizes its direction");

  lighting::set_directional_light_target(directional, 1, 2, 3, 4, 6, 3);
  if (mutation == "--mutate-transform") directional->direction->x = 0.0;
  check(near(directional->direction->x, 0.6) &&
            near(directional->direction->y, 0.8) &&
            near(directional->direction->z, 0.0),
        "directional target derives a normalized world-space direction");
  lighting::set_directional_light_direction(directional, 0, 0, 0);
  check(near(directional->direction->x, 0.6) &&
            near(directional->direction->y, 0.8),
        "zero directional input leaves the prior direction unchanged");

  auto directional_clone = lighting::clone_directional_light(readonly(directional));
  if (mutation == "--mutate-identity") directional_clone = directional;
  check(directional_clone != directional &&
            directional_clone->direction != directional->direction &&
            directional_clone->cascade_splits.identity() !=
                directional->cascade_splits.identity() &&
            directional_clone->cascade_splits.get(2).value() == 1.0,
        "directional cloning owns fresh vector and cascade storage");

  const auto point_position = vector3(2, 3, 4);
  const auto point_options = flight::make_ref<types::PointLightOptions>();
  point_options->casts_shadow = true;
  point_options->layer_mask = 0b0101;
  point_options->priority = 7.0;
  point_options->color = 0xff0000ff;
  point_options->decay = 1.5;
  point_options->intensity = 4.0;
  point_options->intensity_unit = types::lumen_light_unit;
  point_options->normal_bias = 0.25;
  point_options->pcf_radius = 2.0;
  point_options->position = readonly(point_position);
  point_options->range = 12.0;
  point_options->shadow_bias = 0.01;
  point_options->shadow_far = 80.0;
  point_options->shadow_map_size = 512.0;
  point_options->shadow_near = 0.1;
  point_options->shadow_strength = 0.6;
  const auto point = lighting::create_point_light(readonly(point_options));
  check(point->kind == types::point_light_kind && point->position != point_position &&
            point->position->x == 2.0 && point->position->y == 3.0 &&
            point->position->z == 4.0 && point->layer_mask == 5.0 &&
            point->priority == 7.0 && point->range == 12.0,
        "point construction copies position and forwards selection fields");
  check(point->casts_shadow && point->normal_bias == 0.25 &&
            point->pcf_radius == 2.0 && point->shadow_bias == 0.01 &&
            point->shadow_far == 80.0 && point->shadow_map_size == 512.0 &&
            point->shadow_near == 0.1 && point->shadow_strength == 0.6,
        "point construction preserves all authored shadow data");
  const auto point_clone = lighting::clone_point_light(readonly(point));
  check(point_clone != point && point_clone->position != point->position &&
            point_clone->position->z == point->position->z &&
            point_clone->layer_mask == point->layer_mask &&
            point_clone->priority == point->priority,
        "point cloning creates distinct light and position owners");

  const auto spot_options = flight::make_ref<types::SpotLightOptions>();
  spot_options->casts_shadow = true;
  spot_options->direction = readonly(vector3(0, 0, -2));
  spot_options->position = readonly(vector3(1, 2, 3));
  spot_options->inner_cone_degrees = 20.0;
  spot_options->outer_cone_degrees = 50.0;
  spot_options->spot_blend = 1.5;
  spot_options->layer_mask = 0b0011;
  spot_options->priority = 9.0;
  spot_options->range = 15.0;
  spot_options->shadow_strength = 0.4;
  const auto spot = lighting::create_spot_light(readonly(spot_options));
  check(spot->kind == types::spot_light_kind && spot->casts_shadow &&
            spot->layer_mask == 3.0 && spot->priority == 9.0 &&
            spot->range == 15.0 && spot->shadow_strength == 0.4 &&
            spot->spot_blend == 1.0 &&
            near(spot->inner_cone_cos, std::cos(20.0 * flight::pi / 180.0)) &&
            near(spot->outer_cone_cos, std::cos(50.0 * flight::pi / 180.0)),
        "spot construction preserves cone, selection, and shadow options");
  const auto cone = flight::make_ref<types::SpotLightConeAngles>();
  lighting::get_spot_light_cone_degrees(cone, readonly(spot));
  check(near(cone->inner_degrees, 20.0) && near(cone->outer_degrees, 50.0),
        "spot cone degrees round-trip through stored cosine values");
  lighting::set_spot_light_target(spot, 4, 2, 3);
  check(near(spot->direction->x, 1.0) && near(spot->direction->y, 0.0) &&
            near(spot->direction->z, 0.0),
        "spot target derives a normalized direction from its position");
  lighting::set_spot_light_target(spot, 1, 2, 3);
  check(near(spot->direction->x, 1.0),
        "spot target equal to position preserves the prior direction");
  const auto spot_clone = lighting::clone_spot_light(readonly(spot));
  check(spot_clone != spot && spot_clone->position != spot->position &&
            spot_clone->direction != spot->direction &&
            spot_clone->inner_cone_cos == spot->inner_cone_cos,
        "spot cloning creates fresh vector owners and copies cone state");

  const auto area = lighting::create_area_light();
  area->right->x = 2.0;
  area->up->z = 3.0;
  const auto orientation_direction = vector3(0, -8, 0);
  const auto orientation_right = vector3(0, 0, 5);
  const auto orientation_up = vector3(4, 0, 0);
  lighting::set_area_light_orientation(
      area, readonly(orientation_direction), readonly(orientation_right),
      readonly(orientation_up));
  check(near(area->direction->y, -1.0) && near(area->right->z, 2.0) &&
            near(area->up->x, 3.0),
        "area orientation normalizes axes while preserving half-extents");
  const auto zero = vector3(0, 0, 0);
  lighting::set_area_light_orientation(area, readonly(zero), readonly(zero),
                                       readonly(zero));
  check(near(area->direction->y, -1.0) && near(area->right->z, 2.0) &&
            near(area->up->x, 3.0),
        "zero area axes leave every prior orientation component unchanged");
  const auto area_clone = lighting::clone_area_light(readonly(area));
  check(area_clone != area && area_clone->position != area->position &&
            area_clone->direction != area->direction &&
            area_clone->right != area->right && area_clone->up != area->up,
        "area cloning gives every transform vector a distinct owner");

  const auto texture = flight::make_ref<types::TextureCube>();
  const auto environment_options = flight::make_ref<types::EnvironmentOptions>();
  environment_options->enabled = false;
  environment_options->intensity = 2.0;
  environment_options->environment =
      decltype(environment_options->environment){
          std::in_place_type<flight::Ref<types::TextureCube>>, texture};
  const auto environment =
      lighting::create_environment(readonly(environment_options));
  const auto environment_clone = lighting::clone_environment(readonly(environment));
  check(!environment->enabled && environment->intensity == 2.0 &&
            environment->environment.has_value() &&
            std::get<flight::Ref<types::TextureCube>>(
                environment->environment.value()) == texture,
        "environment construction preserves the exact texture variant owner");
  check(environment_clone != environment &&
            environment_clone->environment.has_value() &&
            std::get<flight::Ref<types::TextureCube>>(
                environment_clone->environment.value()) == texture,
        "environment cloning creates a light owner but shares the GPU texture");

  double exposed = lighting::apply_light_exposure(3.0, -1.0);
  if (mutation == "--mutate-intensity") exposed = 3.0;
  check(exposed == 1.5 &&
            lighting::get_light_linear_intensity(types::lux_light_unit,
                                                 100000.0) == 1.0 &&
            lighting::get_light_linear_intensity(types::candela_light_unit,
                                                 100000.0) == 1.0 &&
            near(lighting::get_light_linear_intensity(
                     types::lumen_light_unit, 400000.0 * flight::pi),
                 1.0) &&
            lighting::get_light_linear_intensity(types::unitless_light_unit,
                                                 2.25) == 2.25,
        "exposure and every intensity unit retain their numeric anchors");
  check(near(lighting::convert_light_intensity(
                 types::candela_light_unit, types::lumen_light_unit, 7.0),
             28.0 * flight::pi) &&
            near(lighting::convert_light_intensity(
                     types::lumen_light_unit, types::candela_light_unit,
                     28.0 * flight::pi),
                 7.0),
        "candela and lumen conversion round-trips through linear intensity");
  const auto photometric = flight::make_ref<types::PhotometricLightLike>();
  photometric->intensity = 0.0;
  photometric->intensity_unit = types::lux_light_unit;
  lighting::set_light_photometric_intensity(photometric, 25000.0);
  check(photometric->intensity == 0.25 &&
            near(lighting::get_light_photometric_intensity(readonly(photometric)),
                 25000.0),
        "photometric accessors preserve authored units across set/get");

  const auto analysis_point_options = flight::make_ref<types::PointLightOptions>();
  analysis_point_options->casts_shadow = true;
  analysis_point_options->intensity = 2.0;
  analysis_point_options->position = readonly(vector3(0, 0, 0));
  analysis_point_options->range = 5.0;
  const auto analysis_point =
      lighting::create_point_light(readonly(analysis_point_options));
  const auto bounds = sphere(2, 0, 0, 0);
  check(near(lighting::get_light_luminance(analysis_point), 2.0) &&
            lighting::is_light_enabled(analysis_point) &&
            lighting::is_light_casting_shadow(analysis_point),
        "analysis reads luminance and enabled/shadow gates from the exact owner");
  const auto influence = sphere(99, 99, 99, 99);
  lighting::get_light_influence_bounds(influence, analysis_point);
  if (mutation == "--mutate-shadow") analysis_point->casts_shadow = false;
  check(influence->center->x == 0.0 && influence->center->y == 0.0 &&
            influence->radius == 5.0 &&
            lighting::has_light_influence_on_bounds(analysis_point,
                                                    readonly(bounds)) &&
            lighting::is_light_casting_shadow(analysis_point),
        "analysis preserves finite influence bounds, overlap, and shadow state");
  const auto contribution = lighting::get_light_contribution_at_bounding_sphere(
      lighting::PointOrSpotLightRef{
          std::in_place_type<flight::Ref<types::PointLight>>, analysis_point},
      readonly(bounds));
  check(near(contribution, 0.47472768, 1e-8),
        "point contribution combines linear luminance, distance, and range window");
  analysis_point->enabled = false;
  check(lighting::get_light_luminance(analysis_point) == 0.0 &&
            !lighting::has_light_influence_on_bounds(analysis_point,
                                                     readonly(bounds)) &&
            !lighting::is_light_casting_shadow(analysis_point),
        "disabled lights contribute no luminance, influence, or shadow");
  check(lighting::get_light_luminance(environment) == 0.0,
        "environment analysis intentionally reports no packed-color luminance");

  const auto scene = lighting::create_scene3_dlights();
  check(!scene->ambient.has_value() && !scene->directional.has_value() &&
            scene->hemisphere.has_value() && scene->hemisphere->empty() &&
            scene->point.has_value() && scene->point->empty() &&
            scene->spot.has_value() && scene->spot->empty(),
        "scene-light construction materializes null singles and empty arrays");

  flight::Array<ReadonlyRef<types::PointLight>> ordered_points;
  for (double x : {8.0, 7.0, 6.0, 5.0, 4.0, 3.0}) {
    const auto options = flight::make_ref<types::PointLightOptions>();
    options->position = readonly(vector3(x, 0, 0));
    if (x == 8.0) options->priority = 10.0;
    options->range = -1.0;
    ordered_points.push(readonly(lighting::create_point_light(readonly(options))));
  }
  const auto hidden_options = flight::make_ref<types::PointLightOptions>();
  hidden_options->intensity = 1000.0;
  hidden_options->layer_mask = 0b10;
  hidden_options->position = readonly(vector3(0, 0, 0));
  hidden_options->range = -1.0;
  const auto hidden = lighting::create_point_light(readonly(hidden_options));
  ordered_points.push(readonly(hidden));
  scene->point = ordered_points;

  flight::Array<ReadonlyRef<types::SpotLight>> ordered_spots;
  for (double x : {8.0, 7.0, 6.0, 5.0, 4.0, 3.0}) {
    const auto options = flight::make_ref<types::SpotLightOptions>();
    options->direction = readonly(vector3(-1, 0, 0));
    options->inner_cone_degrees = 45.0;
    options->outer_cone_degrees = 60.0;
    options->position = readonly(vector3(x, 0, 0));
    options->range = -1.0;
    ordered_spots.push(readonly(lighting::create_spot_light(readonly(options))));
  }
  scene->spot = ordered_spots;

  const auto selection = flight::make_ref<types::Scene3DForwardLightSelection>();
  selection->indices = {};
  selection->point = {};
  selection->spot = {};
  lighting::select_scene3_dforward_lights(selection, readonly(scene),
                                          readonly(sphere(0, 0, 0, 0)), 0b01);
  if (mutation == "--mutate-order") {
    selection->point.element(0) = ordered_points.element(2);
  }
  check(selection->point.size() == 4 && selection->spot.size() == 4 &&
            selection->indices.size() == 8 &&
            selection->point.element(0).shared_exact_object<types::PointLight>() ==
                ordered_points.element(0).shared_exact_object<types::PointLight>() &&
            selection->point.element(3).shared_exact_object<types::PointLight>() ==
                ordered_points.element(3).shared_exact_object<types::PointLight>() &&
            selection->spot.element(0).shared_exact_object<types::SpotLight>() ==
                ordered_spots.element(5).shared_exact_object<types::SpotLight>() &&
            selection->spot.element(3).shared_exact_object<types::SpotLight>() ==
                ordered_spots.element(2).shared_exact_object<types::SpotLight>() &&
            selection->indices.get(0).value() == 0.0 &&
            selection->indices.get(3).value() == 3.0 &&
            selection->indices.get(4).value() == -6.0 &&
            selection->indices.get(7).value() == -3.0,
        "forward selection applies priority, ranks both families independently, and encodes stable source indices");
  check(selection->point.element(0).shared_exact_object<types::PointLight>() !=
            hidden,
        "forward selection applies layer masks before consuming the budget");

  const auto probe_position = vector3(1, 2, 3);
  flight::Float32Array coefficients(types::light_probe_sh_floats);
  coefficients.set_index(0, 1.0);
  coefficients.set_index(1, 2.0);
  coefficients.set_index(2, 4.0);
  const auto probe =
      lighting::create_light_probe(readonly(probe_position), coefficients);
  coefficients.set_index(0, 9.0);
  probe_position->x = 99.0;
  check(probe->position != probe_position && probe->position->x == 1.0 &&
            probe->sh_coefficients.identity() != coefficients.identity() &&
            probe->sh_coefficients.get_index(0) == 1.0,
        "probe construction copies both position and coefficient storage");
  flight::Float32Array irradiance(3.0);
  lighting::evaluate_light_probe_sh(irradiance, readonly(vector3(50, 0, 0)),
                                    probe->sh_coefficients);
  check(near(irradiance.get_index(0), 0.2820948) &&
            near(irradiance.get_index(1), 0.5641896) &&
            near(irradiance.get_index(2), 1.1283792),
        "SH evaluation normalizes direction and applies the exact l=0 basis");
  lighting::evaluate_light_probe_sh(probe->sh_coefficients,
                                    readonly(vector3(1, 0, 0)),
                                    probe->sh_coefficients);
  check(near(probe->sh_coefficients.get_index(0), 0.2820948) &&
            near(probe->sh_coefficients.get_index(1), 0.5641896),
        "SH evaluation is alias-safe when output is the coefficient buffer");

  flight::Array<flight::Ref<types::LightProbe>> probes;
  for (double tag = 0; tag < 8; ++tag) probes.push(tagged_probe(tag));
  const auto grid_bounds = flight::make_ref<types::AabbLike>();
  grid_bounds->min = vector3(0, 0, 0);
  grid_bounds->max = vector3(1, 1, 1);
  const auto grid = lighting::create_light_probe_grid(
      probes, readonly(grid_bounds), readonly(vector3(2, 2, 2)));
  flight::Float32Array sampled(types::light_probe_sh_floats);
  const bool sampled_center = lighting::sample_light_probe_grid(
      sampled, readonly(vector3(0.5, 0.5, 0.5)), readonly(grid));
  if (mutation == "--mutate-probe") sampled.set_index(0, 7.0);
  check(sampled_center && near(sampled.get_index(0), 3.5),
        "probe-grid sampling performs exact trilinear interpolation");
  probes.element(7)->enabled = false;
  const bool sampled_corner = lighting::sample_light_probe_grid(
      sampled, readonly(vector3(1, 1, 1)), readonly(grid));
  check(!sampled_corner,
        "probe-grid sampling reports failure when every weighted probe is disabled");

  bool bad_coefficients_threw = false;
  try {
    lighting::create_light_probe(readonly(vector3(0, 0, 0)),
                                 flight::Float32Array(9.0));
  } catch (const flight::Error&) {
    bad_coefficients_threw = true;
  }
  bool bad_grid_threw = false;
  try {
    lighting::create_light_probe_grid(
        flight::Array<flight::Ref<types::LightProbe>>{tagged_probe(0)},
        readonly(grid_bounds), readonly(vector3(2, 2, 2)));
  } catch (const flight::Error&) {
    bad_grid_threw = true;
  }
  if (mutation == "--mutate-failure") bad_grid_threw = false;
  check(bad_coefficients_threw && bad_grid_threw,
        "probe constructors reject wrong coefficient and grid cardinalities");

  return failures == 0 ? 0 : 1;
}
