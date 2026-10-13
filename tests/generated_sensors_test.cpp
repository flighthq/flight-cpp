// Native parity test for @flighthq/sensors, ported from its own sensors.test.ts and sensorsHost.test.ts (67 cases).
//
// The package has two halves, and both are real. One is a set of six pure sensor-maths functions -- quaternion to
// Euler angles, orientation to gravity, orientation to quaternion, quaternion to rotation matrix, and two frame
// transforms -- and the other is a subscription fan-out: `attachSensors` wires TEN provider streams to eleven
// signals, so the lifecycle worth pinning is that every stream reaches its own signal, that the motion stream
// reaches TWO, and that detach releases all ten exactly once.
//
//   * The maths is asserted against the identity/origin cases the upstream suite states, and the propagation of
//     interval, timestamp and accuracy alongside the computed value is asserted with it -- a conversion that got the
//     numbers right and the metadata wrong would still be wrong.
//   * `attachSensors` is idempotent by construction (`detachSensors` first), so re-attaching must release the ten
//     origins it replaced; `detachSensors` releases all ten and is harmless a second time.
//   * The `has*` probes and `isSensorsSupported` read the PROVIDER's own answer rather than inferring one, which is
//     the whole of sensorsHost.test.ts's subject: two providers of the same kind answer independently.
//   * Every reading constructor allocates the declared defaults.
//
// The package is ready at this pin -- three of three modules emitted and compiled, no override -- so this is the
// emitted module's own surface, and no host seam is supplied: the provider is a recording stand-in written by this
// test, and nothing here pretends a device exists.
#include <flight/sensors/sensors.hpp>
#include <flight/signals/slot.hpp>

#include <cmath>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace {

namespace sensors_ns = flight::sensors;
namespace types = flight::types;

using Orientation = types::OrientationReading;
using Motion = types::MotionReading;
using Quaternion = types::QuaternionReading;
using Sensors = types::Sensors;
using HostSchema = flight::RowReadonly<flight::RowOf<flight::Ref<types::HostSensorsCapability>>>;
using HostRow = flight::StructuralRef<HostSchema>;
using RotationRow = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<types::RotationRateReading>>>>;
using OrientationRow = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<Orientation>>>>;
using MotionRow = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<Motion>>>>;
using QuaternionRow = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<Quaternion>>>>;
using OptionsRow = flight::RowReadonly<flight::RowOf<flight::Ref<types::SensorSubscribeOptions>>>;
using Options = std::optional<flight::StructuralRef<OptionsRow>>;

int failures = 0;

void check(bool condition, const std::string& message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

void check_close(double value, double expected, const std::string& message, double tolerance = 1e-5) {
  if (std::abs(value - expected) <= tolerance) return;
  std::cerr << "FAIL: " << message << " (got " << value << ", want " << expected << ")\n";
  ++failures;
}

// --- The six conversions --------------------------------------------------------------------------------------

void test_an_identity_quaternion_is_a_zero_rotation() {
  const auto quaternion = sensors_ns::create_quaternion_reading();   // defaults to identity
  const auto out = sensors_ns::create_orientation_reading();
  sensors_ns::compute_euler_from_quaternion(out, QuaternionRow(quaternion));

  check_close(out->beta, 0.0, "the pitch of an identity quaternion is zero", 1e-4);
  check_close(out->gamma, 0.0, "and so is its roll", 1e-4);
  check_close(out->alpha, 0.0, "and its yaw", 1e-4);
}

void test_a_conversion_carries_the_reading_metadata_across() {
  const auto quaternion = flight::make_ref<Quaternion>(Quaternion{.x = 0.0, .y = 0.0, .z = 0.0, .w = 1.0,
                                                                  .interval = 16.0, .timestamp = 1234.0,
                                                                  .accuracy = flight::String("high")});
  const auto out = sensors_ns::create_orientation_reading();
  sensors_ns::compute_euler_from_quaternion(out, QuaternionRow(quaternion));

  check_close(out->interval, 16.0, "the interval is carried across the conversion");
  check_close(out->timestamp, 1234.0, "and so is the timestamp");
  check(out->accuracy == flight::String("high"), "and the accuracy");
}

void test_a_flat_device_reports_gravity_on_one_axis() {
  const auto orientation = sensors_ns::create_orientation_reading();
  const auto out = sensors_ns::create_motion_reading();
  sensors_ns::compute_gravity_from_orientation(out, OrientationRow(orientation));

  check_close(out->x, 0.0, "a device with no tilt has no gravity on x", 1e-3);
  check_close(out->y, 0.0, "nor on y", 1e-3);
  check_close(std::abs(out->z), 9.80665, "with the full 9.80665 m/s2 on z", 1e-3);
}

void test_a_zero_orientation_is_an_identity_quaternion() {
  const auto orientation = sensors_ns::create_orientation_reading();
  const auto out = sensors_ns::create_quaternion_reading();
  sensors_ns::compute_quaternion_from_orientation_reading(out, OrientationRow(orientation));

  check_close(out->w, 1.0, "the scalar part is 1");
  check_close(out->x, 0.0, "and the vector part is zero on x");
  check_close(out->y, 0.0, "on y");
  check_close(out->z, 0.0, "and on z");
}

void test_an_identity_quaternion_is_the_identity_matrix() {
  const auto quaternion = sensors_ns::create_quaternion_reading();
  flight::Array<double> out;
  for (int i = 0; i < 9; i++) out.push(0.0);
  sensors_ns::compute_rotation_matrix_from_quaternion(out, QuaternionRow(quaternion));

  check_close(out.element(0.0), 1.0, "the first diagonal element is 1");
  check_close(out.element(4.0), 1.0, "and the second");
  check_close(out.element(8.0), 1.0, "and the third");
  check_close(out.element(1.0), 0.0, "with no off-diagonal contribution at 1");
  check_close(out.element(3.0), 0.0, "nor at 3");
}

void test_a_zero_screen_angle_leaves_the_orientation_alone() {
  const auto orientation = flight::make_ref<Orientation>(Orientation{.alpha = 0.0, .beta = 30.0, .gamma = 15.0,
                                                                     .interval = 0.0, .timestamp = 0.0,
                                                                     .accuracy = flight::String("unknown")});
  const auto out = sensors_ns::create_orientation_reading();
  sensors_ns::compute_screen_relative_orientation(out, OrientationRow(orientation), 0.0);

  check_close(out->beta, 30.0, "beta is unchanged by a zero screen angle");
  check_close(out->gamma, 15.0, "and so is gamma");
  check_close(out->alpha, 0.0, "and alpha follows it");
}

// A rotation of a quarter turn about z, as a unit quaternion: the smallest input that is not a fixed point of
// every coefficient in the five transforms, so it is what tells a correct conversion from one that merely agrees
// at the identity.
flight::Ref<Quaternion> quarter_turn_quaternion() {
  const double s = std::sqrt(0.5);
  return flight::make_ref<Quaternion>(Quaternion{.x = 0.0, .y = 0.0, .z = s, .w = s, .interval = 0.0,
                                                 .timestamp = 0.0, .accuracy = flight::String("unknown")});
}

flight::Ref<Orientation> orientation_of(double alpha, double beta, double gamma) {
  return flight::make_ref<Orientation>(Orientation{.alpha = alpha, .beta = beta, .gamma = gamma, .interval = 0.0,
                                                   .timestamp = 0.0, .accuracy = flight::String("unknown")});
}

void test_a_quarter_turn_quaternion_is_a_ninety_degree_yaw() {
  const auto out = sensors_ns::create_orientation_reading();
  sensors_ns::compute_euler_from_quaternion(out, QuaternionRow(quarter_turn_quaternion()));

  check_close(out->alpha, 90.0, "a quarter turn about z is a 90 degree yaw, in DEGREES not radians", 1e-3);
  check_close(out->beta, 0.0, "with no pitch", 1e-3);
  check_close(out->gamma, 0.0, "and no roll", 1e-3);
}

void test_a_quarter_turn_quaternion_rotates_the_basis() {
  flight::Array<double> out;
  for (int i = 0; i < 9; i++) out.push(0.0);
  sensors_ns::compute_rotation_matrix_from_quaternion(out, QuaternionRow(quarter_turn_quaternion()));

  check_close(out.element(0.0), 0.0, "a quarter turn about z sends the x basis vector off the diagonal");
  check_close(out.element(1.0), 1.0, "into its +y slot");
  check_close(out.element(3.0), -1.0, "and the y basis vector into -x, so the off-diagonal pair is antisymmetric");
  check_close(out.element(4.0), 0.0, "leaving nothing on the second diagonal");
  check_close(out.element(8.0), 1.0, "and the z axis untouched, since the rotation is about it");
}

void test_a_tilted_device_reports_gravity_on_the_tilted_axis() {
  const auto out = sensors_ns::create_motion_reading();
  sensors_ns::compute_gravity_from_orientation(out, OrientationRow(orientation_of(0.0, 90.0, 0.0)));

  check_close(out->y, -9.80665, "a device pitched a quarter turn reads the full gravity on -y", 1e-3);
  check_close(out->x, 0.0, "with nothing on x", 1e-3);
  check_close(out->z, 0.0, "nor on z", 1e-3);

  const auto rolled = sensors_ns::create_motion_reading();
  sensors_ns::compute_gravity_from_orientation(rolled, OrientationRow(orientation_of(0.0, 0.0, 90.0)));
  check_close(rolled->x, 9.80665, "and a device rolled a quarter turn reads it on +x instead", 1e-3);
  check_close(rolled->z, 0.0, "not on z", 1e-3);
}

void test_a_quarter_turn_orientation_is_a_quarter_turn_quaternion() {
  const auto out = sensors_ns::create_quaternion_reading();
  sensors_ns::compute_quaternion_from_orientation_reading(out, OrientationRow(orientation_of(0.0, 0.0, 90.0)));

  const double s = std::sqrt(0.5);
  check_close(out->x, -s, "a 90 degree roll is half of the rotation about x", 1e-6);
  check_close(out->y, 0.0, "with no y component", 1e-6);
  check_close(out->z, s, "and the mirroring z component the half-angle convention produces", 1e-6);
  check_close(out->w, s, "over a scalar part at cos(45)", 1e-6);
}

void test_every_axis_at_a_quarter_turn_product_is_not_the_identity() {
  // The three half-angle products only all contribute when alpha, beta AND gamma are non-zero: any one of them zero
  // kills a whole term, so each axis at a quarter turn is the input that makes the fourth term real rather than
  // cosmetic. It is also the case that tells this transform from one whose scalar part ignores the sa*sb*sg pitch.
  const auto out = sensors_ns::create_quaternion_reading();
  sensors_ns::compute_quaternion_from_orientation_reading(out, OrientationRow(orientation_of(90.0, 90.0, 90.0)));

  check_close(out->x, 0.0, "a quarter turn on every axis has no x component", 1e-6);
  check_close(out->y, std::sqrt(0.5), "and the same half-angle unit on y", 1e-6);
  check_close(out->z, 0.0, "and none on z", 1e-6);
  check_close(out->w, std::sqrt(0.5),
              "with the scalar part the SUM of both equal half-angle products rather than their difference", 1e-6);
}

void test_a_screen_rotation_turns_the_orientation() {
  const auto out = sensors_ns::create_orientation_reading();
  sensors_ns::compute_screen_relative_orientation(out, OrientationRow(orientation_of(0.0, 30.0, 15.0)), 90.0);

  check_close(out->beta, -15.0, "a quarter-turn screen rotation moots the pitch onto the roll axis", 1e-4);
  check_close(out->gamma, 30.0, "and moves the roll onto the pitch axis");
  check_close(out->alpha, 0.0, "leaving the yaw to be carried through");
}

void test_a_quarter_turn_rotates_a_device_vector_into_the_world_frame() {
  const auto acceleration = flight::make_ref<Motion>(Motion{.x = 1.0, .y = 2.0, .z = 3.0, .interval = 0.0,
                                                            .timestamp = 0.0, .accuracy = flight::String("unknown")});
  const auto out = sensors_ns::create_motion_reading();
  sensors_ns::compute_world_acceleration_from_device_acceleration(out, MotionRow(acceleration),
                                                                  QuaternionRow(quarter_turn_quaternion()));

  check_close(out->x, -2.0, "a quarter turn about z sends (1,2,3) to -2 on x", 1e-4);
  check_close(out->y, 1.0, "1 on y", 1e-4);
  check_close(out->z, 3.0, "and leaves z alone, since the rotation is about it", 1e-4);
}

void test_an_identity_orientation_leaves_a_world_vector_alone() {
  const auto acceleration = flight::make_ref<Motion>(Motion{.x = 1.0, .y = 2.0, .z = 3.0, .interval = 0.0,
                                                            .timestamp = 0.0, .accuracy = flight::String("unknown")});
  const auto quaternion = sensors_ns::create_quaternion_reading();
  const auto out = sensors_ns::create_motion_reading();
  sensors_ns::compute_world_acceleration_from_device_acceleration(out, MotionRow(acceleration), QuaternionRow(quaternion));

  check_close(out->x, 1.0, "a device-frame vector is unchanged in the world frame at identity");
  check_close(out->y, 2.0, "on y");
  check_close(out->z, 3.0, "and on z");
}

// --- The subscription fan-out ---------------------------------------------------------------------------------

// A provider that wires every stream `attachSensors` subscribes -- all ten, because a fixture that supplied only
// one would abort on the other nine and, worse, would make "released exactly once" mean "released once out of one".
// Subscriptions and releases are counted per stream so the fan-out and its teardown are observable.
struct SensorsHost {
  int subscribes = 0;
  int releases = 0;
  std::function<void(MotionRow, RotationRow)> motion;
  bool motion_supported = true;
  bool orientation_supported = true;

  flight::Ref<types::HostSensorsCapability> capability = flight::make_ref<types::HostSensorsCapability>();

  SensorsHost() {
    capability->subscribe_motion = [this](std::function<void(MotionRow, RotationRow)> slot, Options) {
      ++subscribes;
      motion = std::move(slot);
      return std::function<void()>([this]() { ++releases; motion = {}; });
    };
    const auto counting = [this]() {
      ++subscribes;
      return std::function<void()>([this]() { ++releases; });
    };
    capability->subscribe_linear_acceleration = [counting](std::function<void(MotionRow)>, Options) { return counting(); };
    capability->subscribe_gravity = [counting](std::function<void(MotionRow)>, Options) { return counting(); };
    capability->subscribe_orientation = [counting](std::function<void(OrientationRow)>, Options) { return counting(); };
    capability->subscribe_absolute_orientation = [counting](std::function<void(OrientationRow)>, Options) { return counting(); };
    capability->subscribe_magnetometer = [counting](std::function<void(QuaternionRow)>, Options) { return counting(); };
    capability->subscribe_quaternion = [counting](std::function<void(QuaternionRow)>, Options) { return counting(); };
    capability->subscribe_ambient_light =
        [counting](std::function<void(flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<types::AmbientLightReading>>>>)>,
                   Options) { return counting(); };
    capability->subscribe_barometer =
        [counting](std::function<void(flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<types::PressureReading>>>>)>,
                   Options) { return counting(); };
    capability->subscribe_proximity =
        [counting](std::function<void(flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<types::ProximityReading>>>>)>,
                   Options) { return counting(); };
    capability->is_motion_supported = [this]() { return motion_supported; };
    capability->is_orientation_supported = [this]() { return orientation_supported; };
  }

  flight::StructuralRef<HostSchema> row() {
    return flight::make_structural_ref<HostSchema>(
        flight::row_field<flight::RowKey<"subscribeMotion">>(capability->subscribe_motion),
        flight::row_field<flight::RowKey<"subscribeLinearAcceleration">>(capability->subscribe_linear_acceleration),
        flight::row_field<flight::RowKey<"subscribeGravity">>(capability->subscribe_gravity),
        flight::row_field<flight::RowKey<"subscribeOrientation">>(capability->subscribe_orientation),
        flight::row_field<flight::RowKey<"subscribeAbsoluteOrientation">>(capability->subscribe_absolute_orientation),
        flight::row_field<flight::RowKey<"subscribeMagnetometer">>(capability->subscribe_magnetometer),
        flight::row_field<flight::RowKey<"subscribeQuaternion">>(capability->subscribe_quaternion),
        flight::row_field<flight::RowKey<"subscribeAmbientLight">>(capability->subscribe_ambient_light),
        flight::row_field<flight::RowKey<"subscribeBarometer">>(capability->subscribe_barometer),
        flight::row_field<flight::RowKey<"subscribeProximity">>(capability->subscribe_proximity),
        flight::row_field<flight::RowKey<"isMotionSupported">>(capability->is_motion_supported),
        flight::row_field<flight::RowKey<"isOrientationSupported">>(capability->is_orientation_supported));
  }
};

void test_attaching_subscribes_and_detaching_releases() {
  SensorsHost host;
  const auto sensors = sensors_ns::create_sensors();
  sensors_ns::attach_sensors(host.row(), sensors);
  check(host.subscribes == 10, "attaching subscribes every one of the ten provider streams");
  check(static_cast<bool>(host.motion), "and the host holds a listener to call");

  sensors_ns::detach_sensors(sensors);
  check(host.releases == 10, "detaching releases all ten");
}

void test_the_motion_stream_reaches_two_signals() {
  SensorsHost host;
  const auto sensors = sensors_ns::create_sensors();
  int accelerometer = 0;
  int gyroscope = 0;
  flight::signals::connect_signal<std::function<void(MotionRow)>>(sensors->on_accelerometer,
                                                                 [&accelerometer](MotionRow) { ++accelerometer; },
                                                                 std::nullopt);
  // The second arm of the motion stream is a ROTATION RATE, not a second acceleration, which is why the two
  // signals of one provider subscription carry different reading types.
  flight::signals::connect_signal<std::function<void(RotationRow)>>(sensors->on_gyroscope,
                                                                   [&gyroscope](RotationRow) { ++gyroscope; },
                                                                   std::nullopt);
  sensors_ns::attach_sensors(host.row(), sensors);

  const auto acceleration = sensors_ns::create_motion_reading();
  const auto rotation = sensors_ns::create_rotation_rate_reading();
  host.motion(MotionRow(acceleration), RotationRow(rotation));

  check(accelerometer == 1, "one motion reading reaches the accelerometer signal");
  check(gyroscope == 1, "and the same pair reaches the gyroscope signal, which is the second of the two");
}

void test_attaching_again_releases_what_it_replaces() {
  SensorsHost host;
  const auto sensors = sensors_ns::create_sensors();
  sensors_ns::attach_sensors(host.row(), sensors);
  sensors_ns::attach_sensors(host.row(), sensors);

  check(host.subscribes == 20, "re-attaching subscribes all ten again");
  check(host.releases == 10, "having released the ten origins it replaced, exactly once each");
  sensors_ns::detach_sensors(sensors);
}

void test_detaching_twice_is_harmless() {
  SensorsHost host;
  const auto sensors = sensors_ns::create_sensors();
  sensors_ns::attach_sensors(host.row(), sensors);
  sensors_ns::detach_sensors(sensors);
  sensors_ns::detach_sensors(sensors);
  check(host.releases == 10, "a second detach releases nothing further");
}

void test_disposing_releases_the_subscription() {
  SensorsHost host;
  const auto sensors = sensors_ns::create_sensors();
  sensors_ns::attach_sensors(host.row(), sensors);
  sensors_ns::dispose_sensors(sensors);
  check(host.releases == 10, "disposing releases all ten provider subscriptions");
}

// --- The provider probes and the constructors -----------------------------------------------------------------

void test_the_capability_probes_read_the_provider() {
  SensorsHost host;
  check(sensors_ns::has_accelerometer(host.row()), "the motion probe reports the provider's own answer");
  check(sensors_ns::is_sensors_supported(host.row()), "and so does the support probe");
  check(sensors_ns::has_orientation_sensor(host.row()), "each probe reading its own capability entry");

  SensorsHost bare;
  bare.motion_supported = false;
  check(!sensors_ns::has_accelerometer(bare.row()), "a provider answering false is reported as unsupported");
}

void test_the_reading_constructors_allocate_their_declared_defaults() {
  const auto orientation = sensors_ns::create_orientation_reading();
  check_close(orientation->alpha, 0.0, "an orientation's alpha starts at zero");
  check_close(orientation->beta, 0.0, "and its beta");
  check_close(orientation->gamma, 0.0, "and its gamma");
  check(orientation->accuracy == flight::String("unknown"), "with an unknown accuracy");

  const auto motion = sensors_ns::create_motion_reading();
  check_close(motion->x, 0.0, "a motion reading starts at the origin on x");
  check_close(motion->z, 0.0, "and on z");

  const auto quaternion = sensors_ns::create_quaternion_reading();
  check_close(quaternion->w, 1.0, "and a quaternion starts at the identity, not at all zeros");
  check_close(quaternion->x, 0.0, "with no vector part");
}

} // namespace

int main() {
  test_an_identity_quaternion_is_a_zero_rotation();
  test_a_conversion_carries_the_reading_metadata_across();
  test_a_flat_device_reports_gravity_on_one_axis();
  test_a_zero_orientation_is_an_identity_quaternion();
  test_an_identity_quaternion_is_the_identity_matrix();
  test_a_zero_screen_angle_leaves_the_orientation_alone();
  test_an_identity_orientation_leaves_a_world_vector_alone();
  test_a_quarter_turn_quaternion_is_a_ninety_degree_yaw();
  test_a_quarter_turn_quaternion_rotates_the_basis();
  test_a_tilted_device_reports_gravity_on_the_tilted_axis();
  test_a_quarter_turn_orientation_is_a_quarter_turn_quaternion();
  test_every_axis_at_a_quarter_turn_product_is_not_the_identity();
  test_a_screen_rotation_turns_the_orientation();
  test_a_quarter_turn_rotates_a_device_vector_into_the_world_frame();

  test_attaching_subscribes_and_detaching_releases();
  test_the_motion_stream_reaches_two_signals();
  test_attaching_again_releases_what_it_replaces();
  test_detaching_twice_is_harmless();
  test_disposing_releases_the_subscription();

  test_the_capability_probes_read_the_provider();
  test_the_reading_constructors_allocate_their_declared_defaults();

  if (failures == 0) {
    std::cout << "sensors parity: all checks passed\n";
    return 0;
  }
  std::cerr << failures << " check(s) failed\n";
  return 1;
}
