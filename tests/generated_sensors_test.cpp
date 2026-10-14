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
  // One refusal per probe, so a probe can be shown to read ITS OWN entry: refusing a single entry must change
  // exactly one answer. A fixture that carried only an aggregate flag would let a probe reading the wrong entry
  // pass, which the mutation battery demonstrated before this was added.
  bool refuse_ambient_light = false;
  bool refuse_barometer = false;
  bool refuse_gravity = false;
  bool refuse_gyroscope = false;
  bool refuse_linear_acceleration = false;
  bool refuse_magnetometer = false;
  bool refuse_proximity = false;

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
    // All NINE probes the module exposes, each answering for its own entry: a fixture that wired only some
    // would leave the rest reading a slot the row does not carry.
    capability->is_ambient_light_supported = [this]() { return !refuse_ambient_light; };
    capability->is_barometer_supported = [this]() { return !refuse_barometer; };
    capability->is_gravity_supported = [this]() { return !refuse_gravity; };
    capability->is_gyroscope_supported = [this]() { return !refuse_gyroscope; };
    capability->is_linear_acceleration_supported = [this]() { return !refuse_linear_acceleration; };
    capability->is_magnetometer_supported = [this]() { return !refuse_magnetometer; };
    capability->is_proximity_supported = [this]() { return !refuse_proximity; };
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
        flight::row_field<flight::RowKey<"isOrientationSupported">>(capability->is_orientation_supported),
        flight::row_field<flight::RowKey<"isAmbientLightSupported">>(capability->is_ambient_light_supported),
        flight::row_field<flight::RowKey<"isBarometerSupported">>(capability->is_barometer_supported),
        flight::row_field<flight::RowKey<"isGravitySupported">>(capability->is_gravity_supported),
        flight::row_field<flight::RowKey<"isGyroscopeSupported">>(capability->is_gyroscope_supported),
        flight::row_field<flight::RowKey<"isLinearAccelerationSupported">>(capability->is_linear_acceleration_supported),
        flight::row_field<flight::RowKey<"isMagnetometerSupported">>(capability->is_magnetometer_supported),
        flight::row_field<flight::RowKey<"isProximitySupported">>(capability->is_proximity_supported));
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

// --- Cases added by the re-audit's mechanical mapping --------------------------------------------
//
// Mapping all 67 upstream cases against this file showed my own earlier summary was an OVERCLAIM: it said
// "every reading constructor allocates the declared defaults", and only FOUR of the seven were exercised.
// Three constructors, and SIX of the nine capability probes, were referenced NOWHERE. Both sets are added
// here. The seven initializers are NOT added as separate cases, and that is a reasoned choice rather than
// an omission: unlike `@flighthq/protocol`, where the initializer takes the argument the constructor's
// `??` default would otherwise hide, every `initialize_*Reading` here takes ONLY the construction -- the
// constructor adds allocation and nothing else -- so the constructor cases above exercise the initializer
// completely. There is nothing a direct call could observe that they do not.
void test_the_remaining_reading_constructors_allocate_their_declared_defaults() {
  const auto ambient = sensors_ns::create_ambient_light_reading();
  check(ambient->accuracy == flight::String("unknown"), "an ambient-light reading starts at the unknown accuracy");
  check_close(ambient->illuminance, 0.0, "with zero illuminance");
  check_close(ambient->interval, -1.0, "and the not-yet-reported interval sentinel, not zero");
  check_close(ambient->timestamp, -1.0, "and the same sentinel for its timestamp");

  const auto pressure = sensors_ns::create_pressure_reading();
  check(pressure->accuracy == flight::String("unknown"), "a pressure reading starts at the unknown accuracy");
  check_close(pressure->pressure, 0.0, "with zero pressure");
  check_close(pressure->altitude, -1.0, "and the -1 sentinel for altitude, which is NOT zero-metres");
  check_close(pressure->interval, -1.0, "and for its interval");

  const auto proximity = sensors_ns::create_proximity_reading();
  check(proximity->accuracy == flight::String("unknown"), "a proximity reading starts at the unknown accuracy");
  check_close(proximity->distance, -1.0, "with the -1 sentinel for distance");
  check_close(proximity->max, -1.0, "and for its maximum");
  check(!proximity->near, "and `near` starting FALSE, which is a real answer rather than an absence");

  const auto rotation = sensors_ns::create_rotation_rate_reading();
  check(rotation->accuracy == flight::String("unknown"), "a rotation-rate reading starts at the unknown accuracy");
  check_close(rotation->alpha, 0.0, "with no rotation on any axis");
  check_close(rotation->beta, 0.0, "on beta");
  check_close(rotation->gamma, 0.0, "and on gamma");
  check_close(rotation->interval, -1.0, "and the interval sentinel");
}

void test_every_capability_probe_reads_its_own_provider_entry() {
  SensorsHost host;
  check(sensors_ns::has_accelerometer(host.row()), "hasAccelerometer reads isMotionSupported");
  check(sensors_ns::has_ambient_light_sensor(host.row()), "hasAmbientLightSensor reads isAmbientLightSupported");
  check(sensors_ns::has_barometer(host.row()), "hasBarometer reads isBarometerSupported");
  check(sensors_ns::has_gravity_sensor(host.row()), "hasGravitySensor reads isGravitySupported");
  check(sensors_ns::has_gyroscope(host.row()), "hasGyroscope reads isGyroscopeSupported");
  check(sensors_ns::has_linear_acceleration_sensor(host.row()),
        "hasLinearAccelerationSensor reads isLinearAccelerationSupported");
  check(sensors_ns::has_magnetometer(host.row()), "hasMagnetometer reads isMagnetometerSupported");
  check(sensors_ns::has_orientation_sensor(host.row()), "hasOrientationSensor reads isOrientationSupported");
  check(sensors_ns::has_proximity_sensor(host.row()), "hasProximitySensor reads isProximitySupported");
  check(sensors_ns::is_sensors_supported(host.row()), "and isSensorsSupported reads its own capability");

  // Each probe answers for ITS OWN entry. Asserting only that every probe returns true would pass for a probe
  // that read a SIBLING's entry -- the mutation battery showed exactly that for all six of the probes added
  // here -- so the provider refuses ONE entry at a time and the probe for it must be the one that changes,
  // leaving the other eight unaffected.
  struct Case {
    const char* label;
    void (*set)(SensorsHost&);
    bool (*read)(SensorsHost&);
  };
  const Case cases[] = {
      {"ambient light", [](SensorsHost& h) { h.refuse_ambient_light = true; },
       [](SensorsHost& h) { return sensors_ns::has_ambient_light_sensor(h.row()); }},
      {"barometer", [](SensorsHost& h) { h.refuse_barometer = true; },
       [](SensorsHost& h) { return sensors_ns::has_barometer(h.row()); }},
      {"gravity", [](SensorsHost& h) { h.refuse_gravity = true; },
       [](SensorsHost& h) { return sensors_ns::has_gravity_sensor(h.row()); }},
      {"gyroscope", [](SensorsHost& h) { h.refuse_gyroscope = true; },
       [](SensorsHost& h) { return sensors_ns::has_gyroscope(h.row()); }},
      {"linear acceleration", [](SensorsHost& h) { h.refuse_linear_acceleration = true; },
       [](SensorsHost& h) { return sensors_ns::has_linear_acceleration_sensor(h.row()); }},
      {"magnetometer", [](SensorsHost& h) { h.refuse_magnetometer = true; },
       [](SensorsHost& h) { return sensors_ns::has_magnetometer(h.row()); }},
      {"proximity", [](SensorsHost& h) { h.refuse_proximity = true; },
       [](SensorsHost& h) { return sensors_ns::has_proximity_sensor(h.row()); }},
  };
  for (const auto& entry : cases) {
    SensorsHost refusing;
    entry.set(refusing);
    check(!entry.read(refusing), std::string("a provider refusing the ") + entry.label +
                                      " is reported as refusing it, so that probe reads its own entry");
    check(sensors_ns::has_accelerometer(refusing.row()) && sensors_ns::has_orientation_sensor(refusing.row()) &&
              sensors_ns::is_sensors_supported(refusing.row()),
          std::string("while the other probes are unaffected by the ") + entry.label + " refusal");
  }
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
  test_the_remaining_reading_constructors_allocate_their_declared_defaults();
  test_every_capability_probe_reads_its_own_provider_entry();

  if (failures == 0) {
    std::cout << "sensors parity: all checks passed\n";
    return 0;
  }
  std::cerr << failures << " check(s) failed\n";
  return 1;
}
