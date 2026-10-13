// Focused native parity for @flighthq/camera-controls at Flight a627849.
//
// The camera2d example already covers a smoothed follow step. This suite adds controller ownership,
// ordered inputs, clamps, damping and wrapped angles, reset/snap lifecycle, framing failures, shake
// activation/reset, and the invariant that an updated 3D controller centers its subject on screen.
//
// MUTATION SENSITIVITY, measured 2026-10-13: four one-at-a-time generated-header mutations were
// compiled and run, then restored. The suite killed reversed follow deadzone correction, orbit pan
// using stale current azimuth, fly damping without shortest-arc wrapping, and omitted empty-sphere
// framing validation.
#include <flight/camera/contract.hpp>
#include <flight/camera_controls/contract.hpp>
#include <flight/geometry/contract.hpp>

#include <cmath>
#include <iostream>
#include <memory>
#include <numbers>
#include <optional>
#include <string>
#include <variant>

namespace {

namespace camera = flight::camera;
namespace controls = flight::camera_controls;
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
    double aspect = 1.0,
    double fov_y = std::numbers::pi / 2.0) {
  const auto options = flight::make_ref<types::PerspectiveProjectionOptions>(
      types::PerspectiveProjectionOptions{.aspect = aspect, .fov_y = fov_y});
  return camera::create_perspective_projection(readonly(options));
}

flight::Ref<types::OrthographicProjection> orthographic(
    double half_height = 1.0,
    double half_width = 1.0) {
  const auto options = flight::make_ref<types::OrthographicProjectionOptions>(
      types::OrthographicProjectionOptions{
          .half_height = half_height,
          .half_width = half_width,
      });
  return camera::create_orthographic_projection(readonly(options));
}

flight::Ref<types::Camera3D> camera3d() {
  const auto projection = perspective();
  const auto options = flight::make_ref<types::Camera3DOptions>(
      types::Camera3DOptions{
          .far = 100.0,
          .near = 0.1,
          .projection = types::Projection{projection},
      });
  return camera::create_camera3_d(readonly(options));
}

void follow_deadzone_damping_bounds_and_edge_time() {
  auto value = camera::create_camera2_d();
  auto options = flight::make_ref<types::Camera2DFollowOptions>(
      types::Camera2DFollowOptions{
          .deadzone_half_height = 100.0,
          .deadzone_half_width = 100.0,
          .smooth_time = 0.1,
          .world_bounds = std::nullopt,
      });

  controls::update_camera2_dfollow(
      value, 800.0, 600.0, 50.0, 30.0, 0.016, readonly(options));
  check(value->x == 0.0 && value->y == 0.0,
        "follow leaves a target inside the deadzone unchanged");

  options->smooth_time = 0.0;
  controls::update_camera2_dfollow(
      value, 800.0, 600.0, 300.0, 0.0, 0.016, readonly(options));
  check(value->x == 200.0 && value->y == 0.0,
        "unsmoothed follow moves only far enough to reach the deadzone edge");

  value->x = 0.0;
  options->deadzone_half_height = 0.0;
  options->deadzone_half_width = 0.0;
  options->smooth_time = 0.1;
  controls::update_camera2_dfollow(
      value, 800.0, 600.0, 100.0, 0.0, 0.016, readonly(options));
  check(value->x > 0.0 && value->x < 100.0 && value->y == 0.0,
        "positive dt damps follow toward its goal");

  controls::update_camera2_dfollow(
      value, 800.0, 600.0, 125.0, -20.0, 0.0, readonly(options));
  check(value->x == 125.0 && value->y == -20.0,
        "zero dt follows the authority's immediate-snap edge behavior");

  const auto world = geometry::create_rectangle(0.0, 0.0, 2000.0, 2000.0);
  options->smooth_time = 0.0;
  options->world_bounds = readonly(world);
  controls::update_camera2_dfollow(
      value, 800.0, 600.0, 1900.0, 1000.0, 0.016, readonly(options));
  check(value->x == 1600.0 && value->y == 1000.0,
        "follow clamps the camera center to a larger world");
  const auto visible = geometry::create_rectangle();
  camera::get_camera2_dvisible_bounds(
      readonly(value), 800.0, 600.0, visible);
  check(visible->x >= world->x - tolerance &&
            visible->y >= world->y - tolerance &&
            visible->x + visible->width <= world->x + world->width + tolerance &&
            visible->y + visible->height <= world->y + world->height + tolerance,
        "clamped follow keeps every visible edge inside the world");

  const auto small_world = geometry::create_rectangle(10.0, 20.0, 400.0, 200.0);
  options->world_bounds = readonly(small_world);
  controls::update_camera2_dfollow(
      value, 800.0, 600.0, 1900.0, 1000.0, 0.016, readonly(options));
  check(value->x == 210.0 && value->y == 120.0,
        "follow centers each axis whose world is smaller than the view");
}

void fly_ownership_ordering_clamps_and_view() {
  const auto seed_position = geometry::create_vector3(1.0, 2.0, 3.0);
  auto options = flight::make_ref<types::FlyCameraControllerOptions>();
  options->max_pitch = 0.5;
  options->min_pitch = -0.5;
  options->position = readonly_as<
      flight::StructuralRef<
          flight::RowReadonly<flight::RowOf<types::Vector3Like>>>>(
      seed_position);
  options->smooth_time = 0.5;
  options->yaw = 0.0;
  options->pitch = 0.0;
  const auto source = controls::create_fly_camera_controller(readonly(options));
  seed_position->x = 99.0;
  check(source->position->x == 1.0,
        "fly construction owns an independent position");

  const auto clone = controls::clone_fly_camera_controller(readonly(source));
  check(clone->position != source->position,
        "fly clone owns an independent position");
  clone->position->x = 10.0;
  check(source->position->x == 1.0,
        "fly clone mutation cannot reach the source position");

  const auto out = controls::create_fly_camera_controller();
  out->entity_runtime_key = flight::make_ref<types::EntityRuntime>(
      types::EntityRuntime{.binding = std::nullopt, .uid = flight::String("fly")});
  const auto out_runtime = out->entity_runtime_key;
  const auto out_position = out->position;
  controls::copy_fly_camera_controller(out, readonly(source));
  check(out->entity_runtime_key == out_runtime && out->position == out_position &&
            out->position != source->position,
        "fly copy preserves destination owners while copying value state");

  controls::look_fly_camera_controller(source, std::numbers::pi / 2.0, 2.0);
  check_close(source->goal_yaw, std::numbers::pi / 2.0,
              "fly look accumulates yaw input");
  check(source->goal_pitch == 0.5,
        "fly look clamps pitch input to its configured range");
  controls::move_fly_camera_controller(source, 2.0, 3.0, 1.0);
  check(source->position->x == 4.0 && source->position->y == 3.0 &&
            source->position->z == 1.0,
        "fly movement uses current yaw before the queued look is applied");

  const auto value = camera3d();
  controls::update_fly_camera_controller(source, value, 0.016);
  check(source->yaw > 0.0 && source->yaw < source->goal_yaw,
        "fly update damps rather than snapping for positive dt");
  const auto ahead = geometry::create_vector3(
      source->position->x + std::sin(source->yaw) * std::cos(source->pitch),
      source->position->y + std::sin(source->pitch),
      source->position->z - std::cos(source->yaw) * std::cos(source->pitch));
  const auto ndc = geometry::create_vector3();
  check(camera::get_camera3_dworld_to_screen(
            ndc, readonly(value), readonly_as<
                flight::StructuralRef<
                    flight::RowReadonly<flight::RowOf<types::Vector3Like>>>>(
                ahead),
            1.0) &&
            std::abs(ndc->x) <= tolerance && std::abs(ndc->y) <= tolerance,
        "fly update centers its forward point on screen");

  source->yaw = std::numbers::pi - 0.05;
  source->goal_yaw = -std::numbers::pi + 0.05;
  controls::update_fly_camera_controller(source, value, 0.016);
  check(source->yaw > std::numbers::pi - 0.05 &&
            source->yaw < std::numbers::pi + 0.05,
        "fly damping takes the shortest arc across the yaw seam");
  source->goal_yaw = -1.25;
  controls::update_fly_camera_controller(source, value, -0.1);
  check(source->yaw == -1.25,
        "non-positive fly dt takes the authority's snap path");
  source->goal_pitch = 2.0;
  source->goal_yaw = 0.3;
  controls::snap_fly_camera_controller(source);
  check(source->pitch == 0.5 && source->yaw == 0.3,
        "explicit fly snap applies clamped goals immediately");

  const auto owned_position = source->position;
  auto reset = flight::make_ref<types::FlyCameraControllerOptions>();
  reset->max_pitch = 0.25;
  reset->pitch = 0.2;
  reset->position = readonly_as<
      flight::StructuralRef<
          flight::RowReadonly<flight::RowOf<types::Vector3Like>>>>(
      geometry::create_vector3(7.0, 8.0, 9.0));
  reset->yaw = -0.75;
  controls::reset_fly_camera_controller(source, readonly(reset));
  check(source->position == owned_position && source->position->x == 7.0 &&
            source->position->y == 8.0 && source->position->z == 9.0 &&
            source->pitch == 0.2 && source->goal_pitch == 0.2 &&
            source->yaw == -0.75 && source->goal_yaw == -0.75,
        "fly reset preserves position ownership and resets coupled state");
}

void orbit_ownership_ordering_clamps_and_view() {
  const auto seed_target = geometry::create_vector3(1.0, 2.0, 3.0);
  auto options = flight::make_ref<types::OrbitCameraControllerOptions>();
  options->azimuth = 0.0;
  options->distance = 5.0;
  options->max_distance = 8.0;
  options->max_polar = 1.0;
  options->min_distance = 2.0;
  options->min_polar = -1.0;
  options->polar = 0.0;
  options->smooth_time = 0.5;
  options->target = readonly_as<
      flight::StructuralRef<
          flight::RowReadonly<flight::RowOf<types::Vector3Like>>>>(
      seed_target);
  const auto source = controls::create_orbit_camera_controller(readonly(options));
  seed_target->x = 99.0;
  check(source->target->x == 1.0,
        "orbit construction owns an independent target");

  const auto clone = controls::clone_orbit_camera_controller(readonly(source));
  check(clone->target != source->target,
        "orbit clone owns an independent target");
  const auto out = controls::create_orbit_camera_controller();
  out->entity_runtime_key = flight::make_ref<types::EntityRuntime>(
      types::EntityRuntime{.binding = std::nullopt, .uid = flight::String("orbit")});
  const auto out_runtime = out->entity_runtime_key;
  const auto out_target = out->target;
  controls::copy_orbit_camera_controller(out, readonly(source));
  check(out->entity_runtime_key == out_runtime && out->target == out_target &&
            out->target != source->target,
        "orbit copy preserves destination owners while copying value state");

  controls::rotate_orbit_camera_controller(
      source, std::numbers::pi / 2.0, 2.0);
  check(source->goal_polar == 1.0,
        "orbit rotation clamps polar input");
  controls::pan_orbit_camera_controller(source, 2.0, 0.0);
  check_close(source->target->x, 1.0,
              "orbit pan uses queued azimuth for its right axis x");
  check_close(source->target->z, 1.0,
              "orbit pan uses queued azimuth for its right axis z");
  controls::dolly_orbit_camera_controller(source, 100.0);
  check(source->goal_distance == 8.0,
        "orbit dolly clamps to maximum distance");
  controls::dolly_orbit_camera_controller(source, -100.0);
  check(source->goal_distance == 2.0,
        "orbit dolly clamps to minimum distance");

  const auto view_plane = controls::create_orbit_camera_controller();
  view_plane->goal_polar = std::numbers::pi / 4.0;
  controls::pan_orbit_camera_controller_in_view_plane(view_plane, 0.0, 2.0);
  check_close(view_plane->target->y, std::numbers::sqrt2,
              "view-plane pan follows screen-up y");
  check_close(view_plane->target->z, -std::numbers::sqrt2,
              "view-plane pan follows screen-up z");

  source->goal_azimuth = -std::numbers::pi + 0.05;
  source->azimuth = std::numbers::pi - 0.05;
  source->goal_polar = 0.5;
  source->goal_distance = 6.0;
  const auto value = camera3d();
  controls::update_orbit_camera_controller(source, value, 0.016);
  check(source->azimuth > std::numbers::pi - 0.05 &&
            source->azimuth < std::numbers::pi + 0.05 &&
            source->distance > 5.0 && source->distance < 6.0,
        "orbit update damps distance and takes the shortest azimuth arc");
  const auto ndc = geometry::create_vector3();
  check(camera::get_camera3_dworld_to_screen(
            ndc, readonly(value), readonly_as<
                flight::StructuralRef<
                    flight::RowReadonly<flight::RowOf<types::Vector3Like>>>>(
                source->target),
            1.0) &&
            std::abs(ndc->x) <= tolerance && std::abs(ndc->y) <= tolerance,
        "orbit update centers its owned target on screen");

  source->goal_azimuth = -0.4;
  source->goal_polar = 2.0;
  source->goal_distance = 20.0;
  controls::update_orbit_camera_controller(source, value, 0.0);
  check(source->azimuth == -0.4 && source->polar == 1.0 &&
            source->distance == 8.0,
        "zero orbit dt snaps every current value to its clamped goal");
  source->goal_azimuth = 1.25;
  source->goal_polar = -2.0;
  source->goal_distance = -10.0;
  controls::snap_orbit_camera_controller(source);
  check(source->azimuth == 1.25 && source->polar == -1.0 &&
            source->distance == 2.0,
        "explicit orbit snap applies every clamped goal immediately");

  const auto owned_target = source->target;
  auto reset = flight::make_ref<types::OrbitCameraControllerOptions>();
  reset->azimuth = 0.75;
  reset->distance = 4.0;
  reset->max_distance = 6.0;
  reset->polar = 0.25;
  reset->target = readonly_as<
      flight::StructuralRef<
          flight::RowReadonly<flight::RowOf<types::Vector3Like>>>>(
      geometry::create_vector3(4.0, 5.0, 6.0));
  controls::reset_orbit_camera_controller(source, readonly(reset));
  check(source->target == owned_target && source->target->x == 4.0 &&
            source->target->y == 5.0 && source->target->z == 6.0 &&
            source->azimuth == 0.75 && source->goal_azimuth == 0.75 &&
            source->distance == 4.0 && source->goal_distance == 4.0,
        "orbit reset preserves target ownership and resets coupled state");
}

void framing_success_and_failure_boundaries() {
  const auto controller = controls::create_orbit_camera_controller();
  const auto sphere = geometry::create_bounding_sphere(1.0, 2.0, 3.0, 2.0);
  const auto perspective_projection = perspective(1.0);
  check(controls::frame_orbit_camera_controller_to_sphere(
            controller, types::Projection{perspective_projection},
            readonly_as<flight::StructuralRef<
                flight::RowReadonly<flight::RowOf<types::BoundingSphereLike>>>>(
                sphere),
            1.0),
        "perspective framing accepts a non-empty sphere and viewport");
  check(controller->target->x == 1.0 && controller->target->y == 2.0 &&
            controller->target->z == 3.0 && controller->distance == 10.0,
        "perspective framing moves only target and goal distance");
  check_close(controller->goal_distance,
              2.0 / std::sin(std::numbers::pi / 4.0),
              "perspective framing uses the limiting half field of view");

  const auto ortho_projection = orthographic();
  check(controls::frame_orbit_camera_controller_to_sphere(
            controller, types::Projection{ortho_projection},
            readonly_as<flight::StructuralRef<
                flight::RowReadonly<flight::RowOf<types::BoundingSphereLike>>>>(
                sphere),
            2.0),
        "orthographic framing accepts a non-empty sphere and viewport");
  check(ortho_projection->half_height == 2.0 &&
            ortho_projection->half_width == 4.0,
        "orthographic framing preserves a wide viewport aspect");
  controls::set_orthographic_projection_frame_to_sphere(
      ortho_projection, 2.0, 0.5, 1.5);
  check(ortho_projection->half_height == 6.0 &&
            ortho_projection->half_width == 3.0,
        "orthographic framing preserves a tall viewport aspect and padding");

  const double target_x = controller->target->x;
  const double target_y = controller->target->y;
  const double target_z = controller->target->z;
  const double goal_distance = controller->goal_distance;
  const auto empty = geometry::create_bounding_sphere();
  check(!controls::frame_orbit_camera_controller_to_sphere(
            controller, types::Projection{perspective_projection},
            readonly_as<flight::StructuralRef<
                flight::RowReadonly<flight::RowOf<types::BoundingSphereLike>>>>(
                empty),
            1.0) &&
            !controls::frame_orbit_camera_controller_to_sphere(
                controller, types::Projection{perspective_projection},
                readonly_as<flight::StructuralRef<flight::RowReadonly<
                    flight::RowOf<types::BoundingSphereLike>>>>(sphere),
                0.0),
        "framing refuses empty spheres and invalid viewport aspects");
  check(controller->target->x == target_x && controller->target->y == target_y &&
            controller->target->z == target_z &&
            controller->goal_distance == goal_distance,
        "failed framing leaves controller state unchanged");
}

void shake_activation_decay_reset_and_bounds() {
  auto options = flight::make_ref<types::CameraShakeOptions>();
  options->decay = 2.0;
  options->frequency = 20.0;
  options->rotation_amplitude = 5.0;
  options->translation_amplitude = 1.0;
  const auto shake = controls::create_camera_shake(readonly(options));
  const auto offset = controls::create_camera_shake_offset();
  check(shake->trauma == 0.0 && shake->time == 0.0 &&
            offset->x == 0.0 && offset->rotation_z == 0.0,
        "shake and output start inactive and zeroed");

  offset->x = 9.0;
  offset->rotation_z = 9.0;
  controls::update_camera_shake(shake, 0.0, offset);
  check(offset->x == 0.0 && offset->rotation_z == 0.0,
        "inactive shake clears prior output even at zero dt");
  controls::add_camera_shake_trauma(shake, -1.0);
  check(shake->trauma == 0.0, "negative trauma clamps at zero");
  controls::add_camera_shake_trauma(shake, 2.0);
  check(shake->trauma == 1.0, "accumulated trauma clamps at one");

  controls::update_camera_shake(shake, 0.25, offset);
  check_close(shake->trauma, 0.5, "shake trauma decays linearly");
  check(shake->time == 0.25 &&
            std::abs(offset->x) <= 0.25 + tolerance &&
            std::abs(offset->y) <= 0.25 + tolerance &&
            std::abs(offset->z) <= 0.25 + tolerance &&
            std::abs(offset->rotation_x) <= 1.25 + tolerance &&
            std::abs(offset->rotation_y) <= 1.25 + tolerance &&
            std::abs(offset->rotation_z) <= 1.25 + tolerance,
        "shake output stays within trauma-squared amplitude bounds");

  const auto twin = controls::create_camera_shake(readonly(options));
  const auto twin_offset = controls::create_camera_shake_offset();
  controls::add_camera_shake_trauma(twin, 1.0);
  controls::update_camera_shake(twin, 0.25, twin_offset);
  check(twin_offset->x == offset->x && twin_offset->y == offset->y &&
            twin_offset->rotation_z == offset->rotation_z,
        "shake output is deterministic for equal state and time");

  controls::reset_camera_shake(shake);
  controls::update_camera_shake(shake, 0.016, offset);
  check(shake->trauma == 0.0 && shake->time == 0.016 &&
            offset->x == 0.0 && offset->y == 0.0 && offset->z == 0.0 &&
            offset->rotation_x == 0.0 && offset->rotation_y == 0.0 &&
            offset->rotation_z == 0.0,
        "shake reset disables all offset channels until new trauma arrives");
}

}  // namespace

int main() {
  follow_deadzone_damping_bounds_and_edge_time();
  fly_ownership_ordering_clamps_and_view();
  orbit_ownership_ordering_clamps_and_view();
  framing_success_and_failure_boundaries();
  shake_activation_decay_reset_and_bounds();

  if (failures != 0) {
    std::cerr << failures << " camera-controls parity assertion(s) failed\n";
    return 1;
  }
  std::cout << "generated camera-controls parity: all checks passed\n";
  return 0;
}
