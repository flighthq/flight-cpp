// Additive native parity for @flighthq/camera at Flight a627849.
//
// The integrated 8e214245 generated_camera_test.cpp and
// generated_camera_controls_test.cpp remain the authoritative live baseline. This target contains
// only semantic axes whose isolated mutants survived both live targets: basis extraction, cube
// capture, view diagnostics, intersection failure branches, orthographic texel/aspect behavior,
// regular shadow fitting, nondegenerate depth, and oblique clipping.
#include <flight/camera/basis.hpp>
#include <flight/camera/camera.hpp>
#include <flight/camera/cube_capture.hpp>
#include <flight/camera/depth.hpp>
#include <flight/camera/enable_camera_guards.hpp>
#include <flight/camera/explain_camera3_dview.hpp>
#include <flight/camera/intersection.hpp>
#include <flight/camera/projection.hpp>
#include <flight/camera/reflection.hpp>
#include <flight/camera/shadow_camera.hpp>
#include <flight/camera/visible_bounds.hpp>
#include <flight/geometry/aabb.hpp>
#include <flight/geometry/bounding_sphere.hpp>
#include <flight/geometry/matrix4.hpp>
#include <flight/geometry/plane.hpp>
#include <flight/geometry/ray3d.hpp>
#include <flight/geometry/vector3.hpp>
#include <flight/types/texture_cube_face.hpp>

#include <cmath>
#include <iostream>
#include <memory>
#include <numbers>
#include <string>
#include <utility>
#include <variant>

namespace {

namespace camera = flight::camera;
namespace geometry = flight::geometry;
namespace types = flight::types;

int failures = 0;
constexpr double tolerance = 1e-5;

void check(bool condition, const std::string& message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

void check_close(double actual, double expected, const std::string& message,
                 double epsilon = tolerance) {
  check(std::abs(actual - expected) <= epsilon, message);
}

template <typename Type>
auto readonly(const std::shared_ptr<Type>& value) {
  using View = flight::StructuralRef<
      flight::RowReadonly<flight::RowOf<flight::Ref<Type>>>>;
  return flight::structural_ref_cast<View>(value);
}

template <typename View, typename Type>
View readonly_as(const std::shared_ptr<Type>& value) {
  return flight::structural_ref_cast<View>(readonly(value));
}

flight::Ref<types::PerspectiveProjection> perspective(
    double aspect = 1.0, double fov_y = std::numbers::pi / 2.0) {
  const auto options = flight::make_ref<types::PerspectiveProjectionOptions>(
      types::PerspectiveProjectionOptions{.aspect = aspect, .fov_y = fov_y});
  return camera::create_perspective_projection(readonly(options));
}

flight::Ref<types::OrthographicProjection> orthographic(
    double half_width, double half_height) {
  const auto options = flight::make_ref<types::OrthographicProjectionOptions>(
      types::OrthographicProjectionOptions{
          .half_height = half_height,
          .half_width = half_width,
      });
  return camera::create_orthographic_projection(readonly(options));
}

flight::Ref<types::Camera3D> camera3d(types::Projection projection,
                                      double near = 0.1,
                                      double far = 100.0) {
  const auto options = flight::make_ref<types::Camera3DOptions>(
      types::Camera3DOptions{
          .far = far,
          .near = near,
          .projection = std::move(projection),
      });
  return camera::create_camera3_d(readonly(options));
}

void basis_and_cube_capture() {
  const auto value = camera3d(types::Projection{perspective()});
  const auto eye = geometry::create_vector3(3.0, 4.0, 5.0);
  const auto target = geometry::create_vector3(0.0, 0.0, 0.0);
  const auto up_hint = geometry::create_vector3(0.0, 1.0, 0.0);
  camera::set_camera3_dview_matrix4_from_look_at(
      value, readonly(eye), readonly(target), readonly(up_hint));

  const auto forward = geometry::create_vector3();
  const auto position = geometry::create_vector3();
  const auto right = geometry::create_vector3();
  const auto up = geometry::create_vector3();
  camera::get_camera3_dforward(forward, readonly(value));
  camera::get_camera3_dposition(position, readonly(value));
  camera::get_camera3_dright(right, readonly(value));
  camera::get_camera3_dup(up, readonly(value));
  const double length = std::hypot(3.0, 4.0, 5.0);
  check_close(forward->x, -3.0 / length,
              "basis extracts the signed forward x component");
  check_close(forward->y, -4.0 / length,
              "basis extracts the signed forward y component");
  check_close(forward->z, -5.0 / length,
              "basis extracts the signed forward z component");
  check_close(position->x, 3.0, "basis restores camera position x");
  check_close(position->y, 4.0, "basis restores camera position y");
  check_close(position->z, 5.0, "basis restores camera position z");
  check_close(std::hypot(right->x, right->y, right->z), 1.0,
              "basis produces a unit right vector");
  check_close(std::hypot(up->x, up->y, up->z), 1.0,
              "basis produces a unit up vector");

  const auto cube = camera3d(types::Projection{perspective()}, 0.25, 250.0);
  const auto cube_position = geometry::create_vector3(7.0, -2.0, 9.0);
  camera::get_cube_capture_face_camera3_d(
      cube, readonly(cube_position), types::cube_face_positive_x);
  const auto cube_projection =
      std::get<flight::Ref<types::PerspectiveProjection>>(cube->projection);
  camera::get_camera3_dposition(position, readonly(cube));
  camera::get_camera3_dforward(forward, readonly(cube));
  check_close(cube_projection->aspect, 1.0,
              "cube capture always uses a square projection");
  check_close(cube_projection->fov_y, std::numbers::pi / 2.0,
              "cube capture uses a ninety-degree field of view");
  check(cube->near == 0.25 && cube->far == 250.0,
        "cube capture preserves the caller's clip range");
  check_close(position->x, 7.0, "cube capture preserves position x");
  check_close(position->y, -2.0, "cube capture preserves position y");
  check_close(position->z, 9.0, "cube capture preserves position z");
  check_close(forward->x, 1.0,
              "positive-x cube face looks along the positive x axis");
}

void view_diagnostics_and_projection_edges() {
  const auto value = camera3d(types::Projection{perspective()});
  const auto rigid = camera::explain_camera3_dview(readonly(value));
  check(rigid->is_orthonormal && !rigid->is_reflection,
        "identity view is a proper orthonormal transform");
  check_close(rigid->determinant, 1.0,
              "identity view diagnostic reports unit determinant");

  const auto reflection = geometry::create_matrix4(
      -1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0,
      0.0, 0.0, 1.0, 0.0, 5.0, 7.0, 9.0, 1.0);
  camera::set_camera3_dview_matrix4_from_matrix4(value, readonly(reflection));
  const auto reflected = camera::explain_camera3_dview(readonly(value));
  check(reflected->is_orthonormal && reflected->is_reflection,
        "reflection view stays orthonormal and reports its handedness");
  check_close(reflected->determinant, -1.0,
              "reflection diagnostic reports negative unit determinant");

  const auto scaled = geometry::create_matrix4(
      2.0, 0.0, 0.0, 0.0, 0.0, 2.0, 0.0, 0.0,
      0.0, 0.0, 2.0, 0.0, 0.0, 0.0, 0.0, 1.0);
  camera::set_camera3_dview_matrix4_from_matrix4(value, readonly(scaled));
  const auto non_rigid = camera::explain_camera3_dview(readonly(value));
  check(!non_rigid->is_orthonormal && non_rigid->scale_deviation == 1.0,
        "scaled view reports its non-orthonormal deviation");

  const auto projection = orthographic(10.0, 20.0);
  check_close(camera::get_orthographic_projection_texel_size(
                  readonly(projection), 50.0, 200.0),
              0.4,
              "orthographic texel size selects the larger world footprint");
  const auto orthographic_camera =
      camera3d(types::Projection{projection});
  camera::set_camera3_daspect(orthographic_camera, 1.5);
  check_close(projection->half_width, 30.0,
              "orthographic aspect derives width from retained half-height");
}

void projection_matrix_branches() {
  const auto raw_source = geometry::create_matrix4();
  raw_source->m.set_index(0.0, 2.5);
  raw_source->m.set_index(5.0, 3.5);
  raw_source->m.set_index(10.0, 4.5);
  const auto raw_options = flight::make_ref<types::RawProjectionOptions>(
      types::RawProjectionOptions{.matrix = readonly(raw_source)});
  const auto raw = camera::create_raw_projection(readonly(raw_options));
  const auto raw_out = geometry::create_matrix4();
  camera::set_projection_matrix4(
      raw_out, types::Projection{raw}, 99.0, 7.0, 8.0);
  bool raw_equal = true;
  for (double index = 0.0; index < 16.0; index += 1.0) {
    raw_equal = raw_equal &&
        raw_out->m.get_index(index) == raw->matrix->m.get_index(index);
  }
  check(raw_equal,
        "raw projection matrix is copied verbatim regardless of clip inputs");

  const auto orthographic_projection = orthographic(5.0, 3.0);
  const auto orthographic_out = geometry::create_matrix4();
  const auto orthographic_expected =
      geometry::create_orthographic_matrix4(-5.0, 5.0, -3.0, 3.0, 1.0, 20.0);
  camera::set_projection_matrix4(
      orthographic_out, types::Projection{orthographic_projection},
      99.0, 1.0, 20.0);
  bool orthographic_equal = true;
  for (double index = 0.0; index < 16.0; index += 1.0) {
    orthographic_equal =
        orthographic_equal &&
        std::abs(orthographic_out->m.get_index(index) -
                 orthographic_expected->m.get_index(index)) <= tolerance;
  }
  check(orthographic_equal,
        "orthographic projection matrix uses symmetric stored half-extents");
}

void intersection_failure_paths() {
  const auto value = camera3d(types::Projection{perspective()});
  const auto eye = geometry::create_vector3(0.0, 0.0, 10.0);
  const auto target = geometry::create_vector3(0.0, 0.0, 0.0);
  const auto up = geometry::create_vector3(0.0, 1.0, 0.0);
  camera::set_camera3_dview_matrix4_from_look_at(
      value, readonly(eye), readonly(target), readonly(up));

  using SphereView = flight::StructuralRef<
      flight::RowReadonly<flight::RowOf<types::BoundingSphereLike>>>;
  const auto sphere = geometry::create_bounding_sphere(0.0, 0.0, 5.0, 1.0);
  const auto ray = geometry::create_ray3_d();
  check(camera::get_camera3_dray_through_bounding_sphere(
            ray, readonly(value), readonly_as<SphereView>(sphere), 1.0),
        "camera produces a ray through a non-empty visible sphere");
  check_close(ray->direction->x, 0.0, "sphere ray is centered on x");
  check_close(ray->direction->y, 0.0, "sphere ray is centered on y");
  check_close(ray->direction->z, -1.0, "sphere ray points toward the sphere");
  sphere->radius = -1.0;
  check(!camera::get_camera3_dray_through_bounding_sphere(
             ray, readonly(value), readonly_as<SphereView>(sphere), 1.0),
        "camera rejects an empty bounding sphere");
  sphere->radius = 1.0;
  sphere->center->z = 15.0;
  check(!camera::get_camera3_dray_through_bounding_sphere(
             ray, readonly(value), readonly_as<SphereView>(sphere), 1.0),
        "camera rejects a sphere center behind the view");

  const auto parallel = geometry::create_ray3_d(0.0, 1.0, 0.0,
                                                1.0, 0.0, 0.0);
  const auto plane = geometry::create_plane(0.0, 1.0, 0.0, -1.0);
  const auto hit = geometry::create_vector3();
  check(!camera::intersect_camera3_dray_with_plane(
             hit, readonly(parallel), readonly(plane)),
        "parallel ray and plane have no intersection");
  const auto away = geometry::create_ray3_d(0.0, -5.0, 0.0,
                                            0.0, 1.0, 0.0);
  const auto lower_plane = geometry::create_plane(0.0, 1.0, 0.0, 10.0);
  check(!camera::intersect_camera3_dray_with_plane(
             hit, readonly(away), readonly(lower_plane)),
        "intersection behind the ray origin is rejected");
}

void depth_shadow_and_oblique_projection() {
  const auto perspective_camera =
      camera3d(types::Projection{perspective()}, 1.0, 9.0);
  check_close(camera::get_camera3_dlinear_depth(
                  readonly(perspective_camera), -1.0),
              -1.0, "perspective depth maps near NDC to negative near");
  check_close(camera::get_camera3_dlinear_depth(
                  readonly(perspective_camera), 1.0),
              -9.0, "perspective depth maps far NDC to negative far");
  check_close(camera::get_camera3_dlinear_depth(
                  readonly(perspective_camera), 0.0),
              -1.8, "perspective depth maps the NDC midpoint nonlinearly");
  check_close(camera::get_camera3_dview_space_z(
                  readonly(perspective_camera), 0.0),
              1.8, "view-space z negates linear depth");

  const auto orthographic_camera =
      camera3d(types::Projection{orthographic(1.0, 1.0)}, 1.0, 9.0);
  check_close(camera::get_camera3_dlinear_depth(
                  readonly(orthographic_camera), 0.0),
              -5.0, "orthographic depth maps the NDC midpoint linearly");

  using AabbView = flight::StructuralRef<
      flight::RowReadonly<flight::RowOf<flight::Ref<types::AabbLike>>>>;
  const auto bounds = geometry::create_aabb(-1.0, -1.0, -1.0,
                                             1.0, 1.0, 1.0);
  const auto light = geometry::create_vector3(0.0, -1.0, 0.0);
  const auto shadow = camera3d(types::Projection{perspective()});
  camera::configure_directional_shadow_camera3_d(
      shadow, readonly(light), readonly_as<AabbView>(bounds));
  const auto shadow_projection =
      std::get<flight::Ref<types::OrthographicProjection>>(shadow->projection);
  const double radius = std::numbers::sqrt3;
  check_close(shadow_projection->half_width, radius,
              "regular shadow fit uses the scene sphere radius");
  check_close(shadow->near, radius,
              "regular shadow fit places near at one radius");
  check_close(shadow->far, radius * 3.0,
              "regular shadow fit places far at three radii");
  check_close(shadow->view->m.get_index(14.0), -radius * 2.0,
              "regular shadow fit places the scene center two radii forward");

  const auto projection_matrix = geometry::create_matrix4();
  camera::set_projection_matrix4(
      projection_matrix, types::Projection{perspective(1.0, std::numbers::pi / 3.0)},
      1.0, 0.1, 100.0);
  const auto clip_plane = geometry::create_plane(0.0, 0.0, -1.0, -2.0);
  camera::apply_oblique_near_clip_plane(
      projection_matrix, readonly(clip_plane));
  check_close(projection_matrix->m.get_index(10.0), -1.0408163265,
              "oblique projection replaces the depth coefficient", 1e-4);
  check_close(projection_matrix->m.get_index(14.0), -4.0816326531,
              "oblique projection replaces the depth translation", 1e-4);
  const double z = -2.0;
  const double clip_z =
      projection_matrix->m.get_index(10.0) * z +
      projection_matrix->m.get_index(14.0);
  const double clip_w =
      projection_matrix->m.get_index(11.0) * z +
      projection_matrix->m.get_index(15.0);
  check_close(clip_z / clip_w, -1.0,
              "oblique projection maps its view-space plane to the near clip",
              1e-4);
}

void installed_guard_callbacks() {
  camera::disable_camera_guards();
  check(!camera::camera3_dview_guard.has_value() &&
            !camera::degenerate_visible_bounds_guard.has_value(),
        "disabled camera guards begin with both callbacks detached");
  camera::enable_camera_guards();
  check(camera::camera3_dview_guard.has_value() &&
            camera::degenerate_visible_bounds_guard.has_value(),
        "enabling camera guards installs both public callback seams");
  camera::disable_camera_guards();
  check(!camera::camera3_dview_guard.has_value() &&
            !camera::degenerate_visible_bounds_guard.has_value(),
        "disabling camera guards detaches both public callback seams");
}

}  // namespace

int main() {
  basis_and_cube_capture();
  view_diagnostics_and_projection_edges();
  projection_matrix_branches();
  intersection_failure_paths();
  depth_shadow_and_oblique_projection();
  installed_guard_callbacks();

  if (failures != 0) {
    std::cerr << failures << " camera edge parity assertion(s) failed\n";
    return 1;
  }
  std::cout << "generated camera edge parity tests passed\n";
  return 0;
}
