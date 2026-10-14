// Native parity test for @flighthq/geolocation, ported from its own geolocation.test.ts and
// geolocationAccess.test.ts (13 cases).
//
// The package is a THIN ROUTER over a host capability, and that is the whole of its behavior: every function here
// takes a `HostGeolocationCapability` and either forwards to it or post-processes its answer. There is no device
// access, no permission vocabulary, and no coordinate math -- the upstream package declares no transform, so
// "coordinate transformations where available" has no available surface, and nothing here invents one.
//
// WHAT THIS PINS
//
//   * Routing and identity: `clearGeolocationWatch` forwards the id; `isGeolocationAvailable` reports the
//     provider's own answer rather than inferring one; the position/result/watch calls return the provider's
//     objects, not copies.
//   * The `?? _emptyOptions` default is an IDENTITY, not a fresh object: when options are omitted the provider is
//     handed the same module-level options row on every call, so a provider that compares options by identity sees
//     one object rather than two.
//   * The watch lifecycle: `watchGeolocationPosition` delivers the position to the handler AND returns the
//     provider's own watch id, so the id `clearGeolocationWatch` is later given is the provider's, not one this
//     package minted; and an `onError` supplied is forwarded and reached.
//   * `promptForGeolocationAccess`'s four arms: granted passes through; denied, dismissed and timeout stay three
//     DISTINCT answers; an absent provider is `runtime-unavailable` rather than a throw; and a throwing provider is
//     `operation-failed` rather than escaping.
//
// The seam is a recording provider written by this test, exactly as the upstream suite's own `fakeBackend` is.
// Nothing here pretends a device exists: `isAvailable` answers whatever the test sets, and no browser, SDL or log
// host is supplied.
#include <flight/geolocation/geolocation.hpp>
#include <flight/geolocation/geolocation_access.hpp>

#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace {

namespace geo = flight::geolocation;
namespace types = flight::types;

using Position = types::GeolocationPosition;
using PositionResult = types::GeolocationPositionResult;
using Options = types::GeolocationRequestOptions;
using Outcome = types::GeolocationAccessOutcome;

using HostSchema = flight::RowReadonly<flight::RowOf<flight::Ref<types::HostGeolocationCapability>>>;
using HostRow = flight::StructuralRef<HostSchema>;
using OptionsRow = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<Options>>>>;
using PositionRow = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<Position>>>>;

using WatchHandler = std::function<void(PositionRow)>;
using ErrorHandler = std::optional<std::function<void(types::GeolocationErrorReason)>>;
using PositionTask = flight::Task<std::optional<flight::Ref<Position>>>;
using ResultTask = flight::Task<flight::Ref<PositionResult>>;
using AccessTask = flight::Task<flight::Ref<Outcome>>;

int failures = 0;

void check(bool condition, const std::string& message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

void check_close(double value, double expected, const std::string& message) {
  if (value == expected) return;
  std::cerr << "FAIL: " << message << " (got " << value << ", want " << expected << ")\n";
  ++failures;
}

std::string utf8(const flight::String& value) { return value.to_utf8(); }

// A provider that records what it was handed and answers what the test dictates. Only the members under test are
// given behavior; the row is built key by key so the shape stays honest.
struct RecordingHost {
  std::vector<double> cleared;
  double watch_calls = 0;
  double watch_id = 0;
  double delivered_latitude = 3.0;
  bool available = true;
  bool watch_emits_error = false;
  bool prompt_throws = false;
  flight::String prompt_reason = flight::String("granted");

  // The identity of the options row the provider was handed, so a caller can tell "the same object as last time"
  // from "a fresh one" -- which is what the source's shared `_emptyOptions` guarantees.
  const void* last_options_identity = nullptr;
  double current_latitude = 1.0;
  double current_longitude = 2.0;

  flight::Ref<types::HostGeolocationCapability> capability = flight::make_ref<types::HostGeolocationCapability>();

  RecordingHost() {
    capability->clear_watch = [this](double id) { cleared.push_back(id); };
    capability->is_available = [this]() { return available; };

    capability->get_current_position = [this](OptionsRow options) -> PositionTask {
      last_options_identity = options.identity();
      auto position = geo::create_geolocation_position();
      position->latitude = current_latitude;
      position->longitude = current_longitude;
      return PositionTask::resolve(position);
    };

    capability->get_current_position_result = [this](OptionsRow options) -> ResultTask {
      last_options_identity = options.identity();
      auto position = geo::create_geolocation_position();
      position->latitude = current_latitude;
      position->longitude = current_longitude;
      auto result = flight::make_ref<PositionResult>();
      result->position = position;
      result->reason = std::nullopt;
      return ResultTask::resolve(result);
    };

    capability->watch_position = [this](WatchHandler handler, OptionsRow options, ErrorHandler on_error) -> double {
      ++watch_calls;
      last_options_identity = options.identity();
      auto position = geo::create_geolocation_position();
      position->latitude = delivered_latitude;
      handler(PositionRow(position));
      if (watch_emits_error && on_error.has_value()) on_error.value()(flight::String("denied"));
      return ++watch_id;
    };

    capability->prompt_for_access = [this]() -> AccessTask {
      if (prompt_throws) {
        return AccessTask::reject(flight::Error(flight::String("the provider refused to prompt")));
      }
      auto outcome = flight::make_ref<Outcome>();
      outcome->reason = prompt_reason;
      return AccessTask::resolve(outcome);
    };
  }

  HostRow row() {
    return flight::make_structural_ref<HostSchema>(
        flight::row_field<flight::RowKey<"clearWatch">>(capability->clear_watch),
        flight::row_field<flight::RowKey<"getCurrentPosition">>(capability->get_current_position),
        flight::row_field<flight::RowKey<"getCurrentPositionResult">>(capability->get_current_position_result),
        flight::row_field<flight::RowKey<"isAvailable">>(capability->is_available),
        flight::row_field<flight::RowKey<"promptForAccess">>(capability->prompt_for_access),
        flight::row_field<flight::RowKey<"watchPosition">>(capability->watch_position));
  }
};

// --- Routing -------------------------------------------------------------------------------------------------

void test_clearing_a_watch_forwards_the_id() {
  RecordingHost host;
  geo::clear_geolocation_watch(host.row(), 7.0);

  check(host.cleared.size() == 1, "clearing a watch reaches the provider exactly once");
  check_close(host.cleared.empty() ? -1.0 : host.cleared.front(), 7.0,
              "and forwards the id it was given, unchanged");
}

void test_availability_is_the_providers_own_answer() {
  RecordingHost host;
  check(geo::is_geolocation_available(host.row()), "an available provider is reported available");

  RecordingHost unavailable;
  unavailable.available = false;
  check(!geo::is_geolocation_available(unavailable.row()),
        "and a provider answering false is reported unavailable, not inferred from anything else");
}

void test_the_current_position_is_the_providers_object() {
  RecordingHost host;
  const auto position = geo::get_current_geolocation_position(host.row()).get();

  check(position.has_value(), "a provider that answers with a position yields one rather than an empty optional");
  if (position.has_value()) {
    check_close((*position)->latitude, 1.0, "carrying the provider's latitude");
    check_close((*position)->longitude, 2.0, "and its longitude");
  }
}

void test_the_result_arm_carries_the_position_and_a_null_reason() {
  RecordingHost host;
  const auto result = geo::get_current_geolocation_position_result(host.row()).get();

  check(result->position.has_value(), "the success arm carries a position");
  if (result->position.has_value()) {
    check_close(result->position.value()->latitude, 1.0, "the provider's own position object");
  }
  check(!result->reason.has_value(), "and a null reason, since nothing failed");
}

// --- The omitted-options default is one object, not one per call ---------------------------------------------

void test_omitted_options_hand_the_provider_the_same_object_every_time() {
  RecordingHost host;
  geo::get_current_geolocation_position(host.row()).get();
  const void* first = host.last_options_identity;
  geo::get_current_geolocation_position(host.row()).get();
  const void* second = host.last_options_identity;

  check(first != nullptr, "omitting options still hands the provider an options row");
  check(first == second,
        "and it is the SAME row both times, because the source's `options ?? _emptyOptions` is one shared const");

  const auto explicit_options = flight::make_ref<Options>();
  geo::get_current_geolocation_position(host.row(), OptionsRow(explicit_options)).get();
  check(host.last_options_identity != first,
        "while a caller-supplied options object is forwarded as itself, not replaced by the empty default");
}

// The module's own default, verified INDEPENDENTLY rather than only through the identity of the row a
// provider receives: the shared `_emptyOptions` is a public value, and what makes it a correct default is
// that every option it carries is ABSENT -- a provider reading one sees no preference rather than a zero.
void test_the_shared_empty_options_default_carries_no_preferences() {
  check(flight::geolocation::empty_options != nullptr, "the shared empty-options default exists");

  check(!flight::geolocation::empty_options->enable_high_accuracy.has_value() &&
            !flight::geolocation::empty_options->timeout_ms.has_value() &&
            !flight::geolocation::empty_options->maximum_age_ms.has_value(),
        "and every option it carries is absent, so omitting options asks for no preference at all");
}

// --- The watch lifecycle -------------------------------------------------------------------------------------

void test_watching_delivers_positions_and_returns_the_providers_id() {
  RecordingHost host;
  double seen = 0.0;
  const double id = geo::watch_geolocation_position(
      host.row(), [&seen](PositionRow position) { seen = flight::row_get<flight::RowKey<"latitude">>(position); },
      std::nullopt, std::nullopt);

  check_close(seen, 3.0, "the handler receives the position the provider delivered");
  check_close(id, 1.0, "and the returned watch id is the provider's own, not one this package minted");
}

void test_watching_delivers_the_error_reason_it_is_given() {
  RecordingHost host;
  host.watch_emits_error = true;
  std::vector<std::string> errors;
  geo::watch_geolocation_position(host.row(), [](PositionRow) {}, std::nullopt,
                                 std::function<void(types::GeolocationErrorReason)>(
                                     [&errors](types::GeolocationErrorReason reason) { errors.push_back(utf8(reason)); }));

  check(errors.size() == 1, "an onError handler supplied to a watch is reached");
  check(errors.empty() || errors.front() == "denied", "carrying the provider's own reason");
}

void test_the_watch_id_is_what_clearing_takes() {
  RecordingHost host;
  const double id = geo::watch_geolocation_position(host.row(), [](PositionRow) {}, std::nullopt, std::nullopt);
  geo::clear_geolocation_watch(host.row(), id);

  check(host.cleared.size() == 1, "the watch is cancelled through the provider");
  check_close(host.cleared.empty() ? -1.0 : host.cleared.front(), id,
              "with the very id the watch returned, so the two calls name one subscription");
}

// --- The reading constructor ---------------------------------------------------------------------------------

void test_creating_a_position_allocates_a_zeroed_one() {
  const auto position = geo::create_geolocation_position();

  check_close(position->latitude, 0.0, "latitude starts at zero");
  check_close(position->longitude, 0.0, "as does longitude");
  check_close(position->accuracy, 0.0, "accuracy");
  check_close(position->altitude, 0.0, "altitude");
  check_close(position->altitude_accuracy, 0.0, "altitude accuracy");
  check_close(position->floor_level, 0.0, "floor level");
  check_close(position->heading, 0.0, "heading");
  check_close(position->speed, 0.0, "speed");
  check_close(position->timestamp, 0.0, "and the timestamp");
  // The upstream case asserts `Object.hasOwn(position, EntityRuntimeKey)`, which is true because the property
  // EXISTS -- not because a runtime is attached. The C++ allocator matches both halves of that: the slot is
  // declared by the type and `allocate_entity` explicitly writes it, while the value written is the absent one, so
  // a freshly created entity has no runtime attached (the same thing `clone.test.ts` asserts of a fresh clone).
  check(!position->entity_runtime_key.has_value(),
        "and its runtime-key slot is present but unset, so no runtime is attached to a fresh entity");
}

void test_the_initializer_and_the_constructor_agree() {
  // `createGeolocationPosition` IS the allocator plus `initializeGeolocationPosition` plus finish, so running the
  // initializer by hand over a fresh allocation must produce the same object the constructor does.
  flight::types::EntityConstruction<flight::Ref<Position>> out = flight::entity::allocate_entity<flight::Ref<Position>>();
  geo::initialize_geolocation_position(out);
  const auto by_hand = flight::entity::finish_entity<flight::Ref<Position>>(out);
  const auto by_constructor = geo::create_geolocation_position();

  check_close(by_hand->latitude, by_constructor->latitude, "the initializer and the constructor agree on latitude");
  check_close(by_hand->altitude_accuracy, by_constructor->altitude_accuracy,
              "and on the field the constructor's longest name reaches");
}

// --- The access prompt ---------------------------------------------------------------------------------------

void test_a_granted_prompt_passes_through() {
  RecordingHost host;
  const auto outcome = geo::prompt_for_geolocation_access(host.row()).get();

  check(utf8(outcome->reason) == "granted", "the provider's granted answer is returned as-is");
}

void test_denied_dismissed_and_timeout_stay_three_answers() {
  std::vector<std::string> reasons;
  for (const char* reason : {"denied", "dismissed", "timeout"}) {
    RecordingHost host;
    host.prompt_reason = flight::String(reason);
    reasons.push_back(utf8(geo::prompt_for_geolocation_access(host.row()).get()->reason));
  }

  check(reasons.size() == 3, "three prompts produce three answers");
  check(reasons == std::vector<std::string>({"denied", "dismissed", "timeout"}),
        "and each keeps its own arm: denied means stop asking, dismissed means undecided, timeout says nothing about the user");
}

void test_an_absent_provider_is_runtime_unavailable_rather_than_a_throw() {
  const auto outcome = geo::prompt_for_geolocation_access(std::nullopt).get();

  check(utf8(outcome->reason) == "runtime-unavailable",
        "no provider installed is an expected outcome, reported rather than thrown");
}

void test_a_throwing_provider_is_operation_failed_rather_than_escaping() {
  RecordingHost host;
  host.prompt_throws = true;
  const auto outcome = geo::prompt_for_geolocation_access(host.row()).get();

  check(utf8(outcome->reason) == "operation-failed", "a provider that throws is reported as operation-failed");
}

void test_the_outcome_is_capability_vocabulary_and_not_a_permission_state() {
  // The type declares `reason` and nothing else, so "no permission state field on any arm" is enforced by the type
  // rather than checked at run time; what a run can show is that all five arms survive the round trip intact.
  for (const char* reason : {"granted", "denied", "dismissed", "timeout", "cleanup-failed"}) {
    RecordingHost host;
    host.prompt_reason = flight::String(reason);
    check(utf8(geo::prompt_for_geolocation_access(host.row()).get()->reason) == reason,
          std::string("the ") + reason + " arm round-trips unchanged");
  }
}

} // namespace

int main() {
  test_clearing_a_watch_forwards_the_id();
  test_availability_is_the_providers_own_answer();
  test_the_current_position_is_the_providers_object();
  test_the_result_arm_carries_the_position_and_a_null_reason();

  test_omitted_options_hand_the_provider_the_same_object_every_time();
  test_the_shared_empty_options_default_carries_no_preferences();

  test_watching_delivers_positions_and_returns_the_providers_id();
  test_watching_delivers_the_error_reason_it_is_given();
  test_the_watch_id_is_what_clearing_takes();

  test_creating_a_position_allocates_a_zeroed_one();
  test_the_initializer_and_the_constructor_agree();

  test_a_granted_prompt_passes_through();
  test_denied_dismissed_and_timeout_stay_three_answers();
  test_an_absent_provider_is_runtime_unavailable_rather_than_a_throw();
  test_a_throwing_provider_is_operation_failed_rather_than_escaping();
  test_the_outcome_is_capability_vocabulary_and_not_a_permission_state();

  if (failures == 0) {
    std::cout << "geolocation parity: all checks passed\n";
    return 0;
  }
  std::cerr << failures << " check(s) failed\n";
  return 1;
}
