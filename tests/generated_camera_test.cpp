// Focused native parity for @flighthq/camera at Flight a627849.
//
// The pre-existing camera2d example already exercises its steady view matrix, visible bounds,
// parallax, anchored zoom, and camera-controls follow integration. This suite adds the package
// behavior that example does not own: 2D projection round-trips and failure seams, 3D projection
// ownership and cache transitions, picking round-trips, culling, frustum-corner mutation,
// reflection, shadow fitting, and degenerate matrices.
//
// MUTATION SENSITIVITY, measured 2026-10-13: eleven one-at-a-time audit mutations were diffed,
// compiled, run, and restored. The frozen suite killed six; the five survivors identified the
// default-value, kind-predicate, guard-lifecycle, and near-clip assertions added below. The amended
// suite kills all eleven, in addition to the earlier rotation, anchored-zoom, cache, and frustum-
// assignment probes.
#include <flight/camera/contract.hpp>
#include <flight/geometry/aabb.hpp>
#include <flight/geometry/bounding_sphere.hpp>
#include <flight/geometry/ray3d.hpp>
#include <flight/geometry/rectangle.hpp>

#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <numbers>
#include <optional>
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

void check_close(double actual, double expected, const std::string& message) {
  check(std::abs(actual - expected) <= tolerance, message);
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

flight::Ref<types::Camera3D> camera3d(
    const flight::Ref<types::PerspectiveProjection>& projection,
    double near = 0.1, double far = 100.0) {
  const auto options = flight::make_ref<types::Camera3DOptions>(
      types::Camera3DOptions{
          .far = far,
          .near = near,
          .projection = types::Projection{projection},
      });
  return camera::create_camera3_d(readonly(options));
}

void set_standard_view(const flight::Ref<types::Camera3D>& value) {
  const auto eye = geometry::create_vector3(0.0, 0.0, 5.0);
  const auto target = geometry::create_vector3(0.0, 0.0, 0.0);
  const auto up = geometry::create_vector3(0.0, 1.0, 0.0);
  camera::set_camera3_dview_matrix4_from_look_at(
      value, readonly(eye), readonly(target), readonly(up));
}

void camera2d_projection_viewport_and_failures() {
  const auto default_value = camera::create_camera2_d();
  check(default_value->zoom == 1.0,
        "Camera2D construction defaults omitted zoom to one");

  const double rotation = std::numbers::pi / 5.0;
  const auto options = flight::make_ref<types::Camera2DOptions>(
      types::Camera2DOptions{
          .rotation = rotation,
          .x = 30.0,
          .y = -20.0,
          .zoom = 1.5,
      });
  const auto value = camera::create_camera2_d(readonly(options));
  const auto screen = geometry::create_vector2();
  const auto world = geometry::create_vector2();

  camera::project_camera2_dpoint(
      readonly(value), 800.0, 600.0, 40.0, -20.0, screen);
  check_close(screen->x, 400.0 + 15.0 * std::cos(rotation),
              "2D view pans before rotating and zooming x");
  check_close(screen->y, 300.0 - 15.0 * std::sin(rotation),
              "2D view pans before rotating and zooming y");
  camera::unproject_camera2_dpoint(
      readonly(value), 800.0, 600.0, screen->x, screen->y, world);
  check_close(world->x, 40.0, "2D project/unproject round-trips world x");
  check_close(world->y, -20.0, "2D project/unproject round-trips world y");

  camera::set_camera2_dlook_at(value, -12.0, 18.0);
  camera::project_camera2_dpoint(
      readonly(value), 0.0, 0.0, -12.0, 18.0, screen);
  check_close(screen->x, 0.0, "zero-width viewport still centers at its origin x");
  check_close(screen->y, 0.0, "zero-height viewport still centers at its origin y");

  camera::unproject_camera2_dpoint(
      readonly(value), 1024.0, 512.0, 113.0, 379.0, world);
  const double anchored_x = world->x;
  const double anchored_y = world->y;
  camera::zoom_camera2_dat_screen_point(
      value, 1024.0, 512.0, 113.0, 379.0, 3.25);
  camera::unproject_camera2_dpoint(
      readonly(value), 1024.0, 512.0, 113.0, 379.0, world);
  check(value->zoom == 3.25, "anchored zoom mutates the camera in place");
  check_close(world->x, anchored_x,
              "anchored zoom retains the rotated world point x");
  check_close(world->y, anchored_y,
              "anchored zoom retains the rotated world point y");

  const auto bounds = geometry::create_rectangle();
  camera::get_camera2_dvisible_bounds(
      readonly(value), 1024.0, 512.0, bounds);
  const double right = bounds->x + bounds->width;
  const double bottom = bounds->y + bounds->height;
  for (const auto& corner :
       {std::pair{0.0, 0.0}, std::pair{1024.0, 0.0},
        std::pair{1024.0, 512.0}, std::pair{0.0, 512.0}}) {
    camera::unproject_camera2_dpoint(
        readonly(value), 1024.0, 512.0, corner.first, corner.second, world);
    check(world->x >= bounds->x - tolerance && world->x <= right + tolerance &&
              world->y >= bounds->y - tolerance && world->y <= bottom + tolerance,
          "rotated visible AABB encloses every unprojected viewport corner");
  }

  int guard_calls = 0;
  double guarded_zoom = 1.0;
  using Camera2DView = flight::StructuralRef<
      flight::RowReadonly<flight::RowOf<flight::Ref<types::Camera2D>>>>;
  camera::set_camera2_dvisible_bounds_guard(
      std::function<void(Camera2DView)>([&](Camera2DView guarded) {
        ++guard_calls;
        guarded_zoom = flight::row_get<flight::RowKey<"zoom">>(guarded);
      }));
  value->zoom = 0.0;
  camera::get_camera2_dvisible_bounds(readonly(value), 64.0, 32.0, bounds);
  camera::set_camera2_dvisible_bounds_guard(std::nullopt);
  check(guard_calls == 1 && guarded_zoom == 0.0,
        "degenerate 2D view reports exactly one failure with current state");
  check(std::isfinite(bounds->x + bounds->width) &&
            std::isfinite(bounds->y + bounds->height) &&
            bounds->width == std::numeric_limits<double>::max(),
        "degenerate 2D view fails open with finite conservative edges");
}

void projection_ownership_and_camera3d_state() {
  const auto default_perspective_options =
      flight::make_ref<types::PerspectiveProjectionOptions>(
          types::PerspectiveProjectionOptions{.fov_y = 1.0});
  const auto default_perspective = camera::create_perspective_projection(
      readonly(default_perspective_options));
  check(default_perspective->aspect == 1.0,
        "perspective construction defaults omitted aspect to one");

  const auto projection = perspective(1.0, 1.0);
  const auto value = camera3d(projection);
  check(std::holds_alternative<flight::Ref<types::PerspectiveProjection>>(
            value->projection) &&
            std::get<flight::Ref<types::PerspectiveProjection>>(value->projection) ==
                projection,
        "camera retains the caller-owned projection identity");
  check(value->view != value->inverse_view_projection &&
            value->jitter->x == 0.0 && value->jitter->y == 0.0,
        "camera owns distinct view/cache matrices and zero jitter state");

  camera::set_camera3_daspect(value, 1.75);
  check(projection->aspect == 1.75,
        "aspect transition mutates the retained perspective projection");
  camera::set_camera3_djitter(value, 0.25, -0.5);
  check(value->jitter->x == 0.25 && value->jitter->y == -0.5,
        "jitter transition mutates both camera offsets");

  int view_guard_calls = 0;
  using Camera3DView = flight::StructuralRef<
      flight::RowReadonly<flight::RowOf<flight::Ref<types::Camera3D>>>>;
  camera::set_camera3_dview_guard(
      std::function<void(Camera3DView)>([&](Camera3DView guarded) {
        ++view_guard_calls;
        check(flight::row_get<flight::RowKey<"view">>(guarded) == value->view,
              "view guard observes the camera-owned view");
      }));
  const auto view_owner = value->view;
  const auto assigned_view = geometry::create_matrix4();
  assigned_view->m.set_index(0.0, 4.0);
  camera::set_camera3_dview_matrix4_from_matrix4(
      value, readonly(assigned_view));
  camera::set_camera3_dview_guard(std::nullopt);
  check(view_guard_calls == 1 && value->view == view_owner,
        "matrix assignment writes through the stable view owner and notifies once");
  set_standard_view(value);

  const auto view_projection = geometry::create_matrix4();
  camera::get_camera3_dview_projection_matrix4(
      view_projection, readonly(value), 1.75);
  check(camera::update_camera3_dinverse_view_projection(
            readonly(value), 1.75),
        "valid view-projection refreshes the camera cache");
  const double cached = value->inverse_view_projection->m.get_index(0.0);
  value->near = 1.0;
  value->far = 1.0;
  check(!camera::update_camera3_dinverse_view_projection(
             readonly(value), 1.75) &&
            value->inverse_view_projection->m.get_index(0.0) == cached,
        "singular cache refresh fails without destroying the last valid matrix");

  const auto source = geometry::create_matrix4();
  source->m.set_index(0.0, 2.5);
  const auto raw_options = flight::make_ref<types::RawProjectionOptions>(
      types::RawProjectionOptions{.matrix = readonly(source)});
  const auto raw = camera::create_raw_projection(readonly(raw_options));
  check(raw->matrix != source && raw->matrix->m.get_index(0.0) == 2.5,
        "raw projection construction owns a matrix copy");
  source->m.set_index(0.0, 9.0);
  check(raw->matrix->m.get_index(0.0) == 2.5,
        "later source-matrix mutation cannot retarget a raw projection");

  const auto orthographic_options =
      flight::make_ref<types::OrthographicProjectionOptions>(
          types::OrthographicProjectionOptions{
              .half_height = 2.0,
              .half_width = 3.0,
          });
  const auto orthographic = camera::create_orthographic_projection(
      readonly(orthographic_options));
  const types::Projection orthographic_projection{orthographic};
  const types::Projection perspective_projection{projection};
  const types::Projection raw_projection{raw};
  check(camera::is_orthographic_projection(orthographic_projection) &&
            !camera::is_orthographic_projection(perspective_projection) &&
            camera::is_perspective_projection(perspective_projection) &&
            !camera::is_perspective_projection(raw_projection) &&
            camera::is_raw_projection(raw_projection) &&
            !camera::is_raw_projection(orthographic_projection),
        "projection kind predicates discriminate all three variant alternatives");

  const auto clip_plane = geometry::create_plane(0.0, 1.0, 0.0, -2.0);
  const auto clipped_options = flight::make_ref<types::Camera3DOptions>(
      types::Camera3DOptions{
          .far = 100.0,
          .near = 0.1,
          .near_clip_plane =
              std::variant<flight::Ref<types::Plane>, flight::Null,
                           flight::Undefined>{
                  std::in_place_type<flight::Ref<types::Plane>>, clip_plane},
          .projection = types::Projection{projection},
      });
  const auto clipped = camera::create_camera3_d(readonly(clipped_options));
  check(clipped->near_clip_plane.has_value() &&
            clipped->near_clip_plane.value() == clip_plane,
        "camera construction retains a supplied near-clip plane identity");
}

void camera_guard_lifecycle() {
  camera::disable_camera_guards();
  check(!camera::are_camera_guards_enabled(),
        "camera guards begin the explicit lifecycle disabled");
  camera::enable_camera_guards();
  check(camera::are_camera_guards_enabled(),
        "enabling camera guards updates their public state");
  camera::disable_camera_guards();
  check(!camera::are_camera_guards_enabled(),
        "disabling camera guards restores their public state");
}

void picking_culling_corners_and_depth() {
  const auto projection = perspective();
  const auto value = camera3d(projection);
  set_standard_view(value);

  const auto world = geometry::create_vector3(0.75, -0.4, 0.0);
  const auto ndc = geometry::create_vector3();
  check(camera::get_camera3_dworld_to_screen(
            ndc, readonly(value), readonly(world), 1.5),
        "world point in front of the camera projects to NDC");
  const auto ray = geometry::create_ray3_d();
  check(camera::get_camera3_dscreen_to_world_ray(
            ray, readonly(value), ndc->x, ndc->y, 1.5),
        "projected NDC unprojects to a world ray");
  const auto plane = geometry::create_plane(0.0, 0.0, 1.0, 0.0);
  const auto hit = geometry::create_vector3();
  check(camera::intersect_camera3_dray_with_plane(
            hit, readonly(ray), readonly(plane)),
        "round-trip ray intersects the source world plane");
  check_close(hit->x, world->x, "3D screen/world round-trip restores x");
  check_close(hit->y, world->y, "3D screen/world round-trip restores y");
  check_close(hit->z, world->z, "3D screen/world round-trip restores z");

  const auto inside = geometry::create_vector3(0.0, 0.0, 0.0);
  const auto behind = geometry::create_vector3(0.0, 0.0, 10.0);
  check(camera::is_point_in_camera3_dfrustum(
            readonly(value), readonly(inside), 1.0) &&
            !camera::is_point_in_camera3_dfrustum(
                readonly(value), readonly(behind), 1.0),
        "frustum visibility distinguishes front from behind-camera points");
  const auto sphere = geometry::create_bounding_sphere(0.0, 0.0, 0.0, 1.0);
  check(camera::is_sphere_in_camera3_dfrustum(
            readonly(value), readonly(sphere), 1.0),
        "front-facing sphere remains visible");
  sphere->radius = -1.0;
  check(!camera::is_sphere_in_camera3_dfrustum(
             readonly(value), readonly(sphere), 1.0),
        "negative-radius sphere is culled as empty");

  using AabbView = flight::StructuralRef<
      flight::RowReadonly<flight::RowOf<flight::Ref<types::AabbLike>>>>;
  const auto box = geometry::create_aabb(-1.0, -1.0, -1.0, 1.0, 1.0, 1.0);
  check(camera::is_box_in_camera3_dfrustum(
            readonly(value), readonly_as<AabbView>(box), 1.0),
        "front-facing box intersects the camera frustum");
  box->min->z = 8.0;
  box->max->z = 12.0;
  check(!camera::is_box_in_camera3_dfrustum(
             readonly(value), readonly_as<AabbView>(box), 1.0),
        "behind-camera box is culled after in-place bounds mutation");

  flight::Array<types::Vector3Like> corners{
      geometry::create_vector3(), geometry::create_vector3(),
      geometry::create_vector3(), geometry::create_vector3(),
      geometry::create_vector3(), geometry::create_vector3(),
      geometry::create_vector3(), geometry::create_vector3(),
  };
  check(camera::get_camera3_dfrustum_corners(corners, readonly(value), 2.0),
        "invertible view-projection writes all frustum corners");
  check(corners.element(0.0)->z > corners.element(4.0)->z &&
            corners.element(0.0)->x < 0.0 && corners.element(1.0)->x > 0.0 &&
            std::abs(corners.element(4.0)->x) >
                std::abs(corners.element(0.0)->x),
        "frustum corner mutation preserves near/far order and perspective spread");

  value->near = 2.0;
  value->far = 2.0;
  check(!camera::get_camera3_dscreen_to_world_ray(
             ray, readonly(value), 0.0, 0.0, 1.0) &&
            !camera::get_camera3_dfrustum_corners(
                corners, readonly(value), 1.0) &&
            camera::get_camera3_dlinear_depth(readonly(value), 0.0) == 0.0 &&
            camera::get_camera3_dview_space_z(readonly(value), 0.0) == 0.0,
        "degenerate clip range fails ray/corner inversion and returns zero depth");
  check(!camera::get_camera3_dworld_to_screen(
             ndc, readonly(value), readonly(behind), 1.0),
        "world-to-screen rejects a behind-camera point");
}

void reflection_and_shadow_transitions() {
  const auto projection = perspective();
  const auto original = camera3d(projection);
  const auto eye = geometry::create_vector3(2.0, 3.0, 5.0);
  const auto target = geometry::create_vector3(0.0, 0.0, 0.0);
  const auto up = geometry::create_vector3(0.0, 1.0, 0.0);
  camera::set_camera3_dview_matrix4_from_look_at(
      original, readonly(eye), readonly(target), readonly(up));
  camera::set_camera3_djitter(original, 0.125, -0.25);

  const auto reflected = camera3d(perspective());
  const auto restored = camera3d(perspective());
  const auto mirror = geometry::create_plane(0.0, 1.0, 0.0, 0.0);
  camera::reflect_camera3_dby_plane(
      reflected, readonly(original), readonly(mirror));
  camera::reflect_camera3_dby_plane(
      restored, readonly(reflected), readonly(mirror));
  bool restored_view = true;
  for (double index = 0.0; index < 16.0; index += 1.0) {
    restored_view = restored_view &&
        std::abs(restored->view->m.get_index(index) -
                 original->view->m.get_index(index)) <= tolerance;
  }
  check(restored_view && restored->jitter->x == original->jitter->x &&
            restored->jitter->y == original->jitter->y &&
            std::get<flight::Ref<types::PerspectiveProjection>>(
                reflected->projection) == projection,
        "double reflection restores view while retaining projection and jitter state");

  using AabbView = flight::StructuralRef<
      flight::RowReadonly<flight::RowOf<flight::Ref<types::AabbLike>>>>;
  const auto bounds = geometry::create_aabb(-2.0, -1.0, -3.0, 4.0, 5.0, 6.0);
  const auto light = geometry::create_vector3(0.0, -1.0, 0.0);
  camera::configure_directional_shadow_camera3_dtight_fit(
      reflected, readonly(light), readonly_as<AabbView>(bounds), 1.25);
  check(std::holds_alternative<flight::Ref<types::OrthographicProjection>>(
            reflected->projection),
        "shadow fit transitions a perspective camera to orthographic");
  const auto orthographic =
      std::get<flight::Ref<types::OrthographicProjection>>(reflected->projection);
  check(orthographic->half_width > 0.0 && orthographic->half_height > 0.0 &&
            reflected->near > 0.0 && reflected->far > reflected->near,
        "shadow fit produces positive padded extents and ordered depth");

  const auto empty = geometry::create_aabb();
  const auto zero = geometry::create_vector3(0.0, 0.0, 0.0);
  camera::configure_directional_shadow_camera3_dtight_fit(
      reflected, readonly(zero), readonly_as<AabbView>(empty));
  const auto fallback =
      std::get<flight::Ref<types::OrthographicProjection>>(reflected->projection);
  check(std::isfinite(fallback->half_width) && fallback->half_width > 0.0 &&
            std::isfinite(fallback->half_height) && fallback->half_height > 0.0,
        "empty bounds and zero light direction fall back to a finite unit fit");
}

}  // namespace

int main() {
  camera2d_projection_viewport_and_failures();
  projection_ownership_and_camera3d_state();
  picking_culling_corners_and_depth();
  reflection_and_shadow_transitions();
  camera_guard_lifecycle();

  if (failures != 0) {
    std::cerr << failures << " camera parity assertion(s) failed\n";
    return 1;
  }
  std::cout << "generated camera parity tests passed\n";
  return 0;
}
