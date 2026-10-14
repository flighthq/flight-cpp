// Focused behavior tests for @flighthq/permissions.
//
// The package emits whole at a627849 -- 3/3 modules, no refusals, no override -- and is `assisted`
// because one existing emission repair (`conditional-absent-branch-optional`) touches it.
//
// FIVE upstream suites specify this package, with 49 cases between them, and this header used to name only
// three of the five and count none of them -- `getPermissionState.test.ts` (14 cases) and
// `permission.test.ts` (3 named-export cases) were not mentioned at all. All five are now enumerated in
// docs/permissions-fixture-coverage.md:
//
//   getPermissionStates.test.ts 6   requestPermission.test.ts 17   getPermissionState.test.ts 14
//   geolocationDelegation.test.ts 9   permission.test.ts 3 (the three named exports)
//
// The emphasis is the two things the suites exist for:
//
//   * the BATCH: owners are captured before any work starts, so a provider transition during one result
//     cannot redirect a later entry; the result keeps input order and repeated names even when the work
//     resolves out of order; and an empty batch resolves no owner at all;
//   * HOST IDENTITY: each name routes to its own operation-tight provider -- a repeated MIDI name goes to
//     the MIDI capability, the geolocation prompt belongs to the geolocation capability, and a name with
//     no request route never reaches any provider.
//
// THREE DEFECTS FOUND BY A LATER AUDIT OF THIS FILE, and one of them is why the audit was asked for: the
// pass summary used to be printed about ninety lines BEFORE the end, so every assertion below it could fail
// while the run still emitted "all checks passed". The assertions in that region were added by the previous
// arc to close a coverage gap, and their verification was therefore worthless as evidence -- a grep for the
// banner passes either way. The banner is now the last statement; the region is pinned by mutations that
// fail only it, and they are caught.
//
// The other two:
//   1. THE STORAGE PERSISTENCE REQUEST projection had NO coverage. Two mutations of its body survived the
//      whole suite, which is how the gap was found. Its six upstream rows are now asserted, mirroring the
//      query table.
//   2. AN ABSENT GEOLOCATION PROVIDER was unasserted: a mutation rewriting that arm's reason to
//      operation-failed survived. Now covered.
#include <flight/executor.hpp>
#include <flight/permissions/permission.hpp>

#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace {

int failures = 0;

bool check(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
  return condition;
}

using flight::String;
using QueryOutcome = flight::types::PermissionQueryOutcome;
using RequestOutcome = flight::types::PermissionRequestOutcome;
using Permissions = flight::types::HostPermissionsCapability;
using Midi = flight::types::HostMidiPermissionCapability;
using Geolocation = flight::types::HostGeolocationCapability;

bool query_ok(const QueryOutcome& outcome, const char* state) {
  return outcome.index() == 1 && std::get<1>(outcome)->reason == String("ok") &&
         std::get<1>(outcome)->state == String(state);
}

std::string query_reason(const QueryOutcome& outcome) {
  if (outcome.index() == 0) return std::get<0>(outcome)->reason.to_utf8();
  if (outcome.index() == 1) return std::get<1>(outcome)->reason.to_utf8();
  return std::get<2>(outcome)->reason.to_utf8();
}

std::string request_reason(const RequestOutcome& outcome) {
  if (outcome.index() == 0) return std::get<0>(outcome)->reason.to_utf8();
  if (outcome.index() == 1) return std::get<1>(outcome)->reason.to_utf8();
  return std::get<2>(outcome)->reason.to_utf8();
}

QueryOutcome query_state(const char* reason, const char* state) {
  auto arm = flight::make_ref<flight::types::reason_state_86decb2e3186b77a>();
  arm->reason = String(reason);
  arm->state = String(state);
  return QueryOutcome{std::in_place_index<1>, arm};
}

QueryOutcome query_failed() {
  auto arm = flight::make_ref<flight::types::reason_5a15b37101877c7d>();
  arm->reason = String("operation-failed");
  return QueryOutcome{std::in_place_index<0>, arm};
}

flight::Ref<Geolocation> geolocation(const char* reason) {
  auto host = flight::make_ref<Geolocation>();
  host->prompt_for_access = [reason]() {
    auto outcome = flight::make_ref<flight::types::GeolocationAccessOutcome>();
    outcome->reason = String(reason);
    return flight::Task<flight::Ref<flight::types::GeolocationAccessOutcome>>::resolve(outcome);
  };
  return host;
}

flight::Ref<Permissions> permissions_provider() {
  auto host = flight::make_ref<Permissions>();
  host->query_permission = [](String name) {
    return flight::Task<QueryOutcome>::resolve(query_state("ok", name == String("camera") ? "granted" : "prompt"));
  };
  return host;
}

}  // namespace

int main() {
  const auto executor = std::make_shared<flight::QueueExecutor>();
  const flight::ExecutorScope scope(executor);

  // The batch: input order and repeated names survive out-of-order resolution, and every entry is
  // pinned to the provider captured before any work started.
  {
    std::vector<flight::Task<QueryOutcome>::Resolver> resolvers;
    int calls = 0;
    auto host = flight::make_ref<Permissions>();
    host->query_permission = [&resolvers, &calls](String) {
      ++calls;
      return flight::Task<QueryOutcome>::create(
          [&resolvers](flight::Task<QueryOutcome>::Resolver resolve) { resolvers.push_back(resolve); });
    };

    flight::Array<String> names = flight::Array<String>{};
    names.push(String("camera"));
    names.push(String("microphone"));
    names.push(String("camera"));
    const auto pending = flight::permissions::get_permission_states(host, std::nullopt, std::nullopt, names);

    check(calls == 3, "every entry starts its own query, including the repeated name");
    check(resolvers.size() == 3, "and every query is still unsettled");

    // The provider the batch captured is the one each entry uses: replacing the capability's function
    // after the call cannot redirect any of them.
    host->query_permission = [](String) {
      return flight::Task<QueryOutcome>::resolve(query_failed());
    };

    resolvers[2](query_state("ok", "prompt"));
    resolvers[0](query_state("ok", "granted"));
    resolvers[1](query_state("ok", "denied"));
    const auto outcomes = pending.get();
    check(outcomes.size() == 3.0, "the batch returns every outcome");
    check(query_ok(outcomes.element(0.0), "granted"), "the first entry keeps its own result");
    check(query_ok(outcomes.element(1.0), "denied"), "the second keeps its own result");
    check(query_ok(outcomes.element(2.0), "prompt"),
          "and the repeated entry keeps its own, in input position despite resolving first");
  }

  // An empty batch resolves no owner at all.
  {
    int calls = 0;
    auto host = flight::make_ref<Permissions>();
    host->query_permission = [&calls](String) {
      ++calls;
      return flight::Task<QueryOutcome>::resolve(query_failed());
    };
    const auto outcomes =
        flight::permissions::get_permission_states(host, std::nullopt, std::nullopt, flight::Array<String>{})
            .get();
    check(outcomes.size() == 0.0, "an empty batch resolves to an empty list");
    check(calls == 0, "without consulting the provider at all");
  }

  // A repeated MIDI name is pinned to the MIDI capability, not to the permissions provider.
  {
    int midi_calls = 0;
    int permissions_calls = 0;
    auto midi = flight::make_ref<Midi>();
    midi->get_permission = [&midi_calls]() {
      ++midi_calls;
      return flight::Task<QueryOutcome>::resolve(query_state("ok", "granted"));
    };
    auto host = flight::make_ref<Permissions>();
    host->query_permission = [&permissions_calls](String) {
      ++permissions_calls;
      return flight::Task<QueryOutcome>::resolve(query_failed());
    };

    flight::Array<String> names = flight::Array<String>{};
    names.push(String("midi"));
    names.push(String("midi"));
    const auto outcomes =
        flight::permissions::get_permission_states(host, std::optional<flight::Ref<Midi>>{midi}, std::nullopt, names)
            .get();
    check(outcomes.size() == 2.0 && query_ok(outcomes.element(0.0), "granted") &&
              query_ok(outcomes.element(1.0), "granted"),
          "both repeated MIDI entries answer from the MIDI capability");
    check(midi_calls == 2, "which was asked once per entry");
    check(permissions_calls == 0, "and the permissions provider was not consulted for them");
  }

  // A name whose own provider was not supplied reports that capability's absence, and never degrades
  // into a query through a different provider.
  {
    const auto outcome = flight::permissions::get_permission_state(
                             flight::make_ref<Permissions>(), std::nullopt, std::nullopt, String("midi"))
                             .get();
    check(query_reason(outcome) == "unsupported",
          "a MIDI query with no MIDI capability reports unsupported");
  }

  // The geolocation request routes through the geolocation capability, with its own outcome vocabulary.
  {
    const auto provider = permissions_provider();
    int prompts = 0;
    auto host = flight::make_ref<Geolocation>();
    host->prompt_for_access = [&prompts](const char* reason) {
      return [&prompts, reason]() {
        ++prompts;
        auto outcome = flight::make_ref<flight::types::GeolocationAccessOutcome>();
        outcome->reason = String(reason);
        return flight::Task<flight::Ref<flight::types::GeolocationAccessOutcome>>::resolve(outcome);
      };
    }("granted");

    const auto granted = flight::permissions::request_permission(
                             provider, std::nullopt, std::optional<flight::Ref<Geolocation>>{host},
                             String("geolocation"))
                             .get();
    check(request_reason(granted) == "granted" && granted.index() == 1 &&
              std::get<1>(granted)->state == String("granted"),
          "a granted geolocation prompt projects to granted with its state");
    check(prompts == 1, "and the provider was asked exactly once");

    // dismissed carries the prompt state, and cleanup-failed keeps the granted decision.
    auto dismissed_host = geolocation("dismissed");
    const auto dismissed = flight::permissions::request_permission(
                               provider, std::nullopt, std::optional<flight::Ref<Geolocation>>{dismissed_host},
                               String("geolocation"))
                               .get();
    check(request_reason(dismissed) == "dismissed" && dismissed.index() == 1 &&
              std::get<1>(dismissed)->state == String("prompt"),
          "a dismissed prompt reports the prompt state");
    const auto cleaned =
        flight::permissions::request_permission(provider, std::nullopt,
                                                std::optional<flight::Ref<Geolocation>>{geolocation("cleanup-failed")},
                                                String("geolocation"))
            .get();
    check(request_reason(cleaned) == "cleanup-failed" && cleaned.index() == 1 &&
              std::get<1>(cleaned)->state == String("granted"),
          "a cleanup failure keeps the granted decision");

    // A timeout is a REASON WITH NO STATE: it reports a deadline, not a decision.
    const auto timed_out =
        flight::permissions::request_permission(provider, std::nullopt,
                                                std::optional<flight::Ref<Geolocation>>{geolocation("timeout")},
                                                String("geolocation"))
            .get();
    check(request_reason(timed_out) == "timeout", "a timeout is carried through as its own reason");
    check(timed_out.index() == 0, "as a reason-only outcome, with no state invented for it");
  }

  // The request dispatch: names with no request route never reach a provider, and an unknown name is
  // reported as unsupported.
  {
    int media_calls = 0;
    int prompts = 0;
    auto host = flight::make_ref<Permissions>();
    host->request_media_access = [&media_calls](String) {
      ++media_calls;
      auto arm = flight::make_ref<flight::types::reason_state_cc6a9749ee69e3e5>();
      arm->reason = String("granted");
      arm->state = String("granted");
      return flight::Task<RequestOutcome>::resolve(RequestOutcome{std::in_place_index<1>, arm});
    };
    auto geolocation_host = flight::make_ref<Geolocation>();
    geolocation_host->prompt_for_access = [&prompts]() {
      ++prompts;
      auto outcome = flight::make_ref<flight::types::GeolocationAccessOutcome>();
      outcome->reason = String("granted");
      return flight::Task<flight::Ref<flight::types::GeolocationAccessOutcome>>::resolve(outcome);
    };
    const auto with_geolocation = std::optional<flight::Ref<Geolocation>>{geolocation_host};

    check(request_reason(flight::permissions::request_permission(host, std::nullopt, with_geolocation,
                                                                  String("camera"))
                             .get()) == "granted",
          "camera is routed to the media-access provider");
    check(media_calls == 1, "which was asked once");

    check(request_reason(flight::permissions::request_permission(host, std::nullopt, with_geolocation,
                                                                  String("midi"))
                             .get()) == "no-request-route",
          "midi has no request route");
    check(request_reason(flight::permissions::request_permission(host, std::nullopt, with_geolocation,
                                                                  String("clipboard-read"))
                             .get()) == "no-request-route",
          "and neither does clipboard-read");
    check(request_reason(flight::permissions::request_permission(host, std::nullopt, with_geolocation,
                                                                  String("push"))
                             .get()) == "no-request-route",
          "nor push");
    check(request_reason(flight::permissions::request_permission(host, std::nullopt, with_geolocation,
                                                                  String("something-else"))
                             .get()) == "unsupported",
          "an unknown name is reported as unsupported");
    check(media_calls == 1 && prompts == 0,
          "and none of those names reached a provider");
  }

  // An absent geolocation provider is reported as runtime-unavailable, not as a denial and not by
  // silently succeeding. A mutation rewriting this arm's reason to operation-failed survived the suite
  // before this case existed, which is how the gap was found.
  {
    const auto outcome = flight::permissions::request_permission(
                             flight::make_ref<Permissions>(), std::nullopt, std::nullopt,
                             String("geolocation"))
                             .get();
    check(request_reason(outcome) == "runtime-unavailable",
          "a geolocation request with no geolocation provider reports runtime-unavailable");
    check(outcome.index() == 0, "as a reason-only outcome, with no state invented for it");
  }

  // The notification query projects the host's permission vocabulary into Flight's.
  {
    auto host = flight::make_ref<Permissions>();
    auto notification = flight::make_ref<flight::types::HostNotificationPermissionCapability>();
    notification->get_permission = []() {
      auto arm = flight::make_ref<flight::types::permission_reason_2a03a8a4faab9e18>();
      arm->reason = String("ok");
      arm->permission = String("default");
      return flight::Task<flight::types::NotificationPermissionQueryOutcome>::resolve(
          flight::types::NotificationPermissionQueryOutcome{std::in_place_index<0>, arm});
    };
    host->notification = notification;

    const auto outcome = flight::permissions::get_permission_state(
                             host, std::nullopt, std::nullopt, String("notifications"))
                             .get();
    check(query_ok(outcome, "prompt"),
          "the host's 'default' permission projects to Flight's 'prompt' state");
  }

  // THE STORAGE PERSISTENCE PROJECTION, which this suite never touched: `persistent-storage` appears
  // zero times above, and all six rows below come from the upstream table. The rule they pin is that the
  // projection maps the OWNER'S OUTCOME to a reason and carries its permissionState through as state
  // INDEPENDENTLY -- it never reconciles the two. The first row is the proof: an owner that reports
  // outcome 'persistent' with permissionState 'denied' must project to reason 'ok' AND state 'granted'.
  // An implementation that "made them consistent" would report denied, or drop the state, and pass a
  // weaker test.
  {
    // `has_state_key` is the TS-VISIBLE distinction and the only one asserted about the union shape: a
    // result that carries a `state` property (possibly null) is a different object from one that has no
    // such property at all, and the fixture's 'operation-failed' row is the latter. Which C++ arm holds
    // a PRESENT state is an emitter choice invisible to TypeScript, so it is deliberately not asserted.
    struct Row {
      const char* owner_outcome;
      std::optional<const char*> owner_state;
      const char* expected_reason;
      std::optional<const char*> expected_state;
      bool has_state_key;
      const char* message;
    };
    const Row rows[] = {
        {"persistent", "denied", "ok", "granted", true,
         "a 'persistent' owner projects to reason ok and state granted, whatever its permissionState said"},
        {"operation-failed", "granted", "operation-failed", std::nullopt, false,
         "an 'operation-failed' owner projects to a reason with no state invented for it"},
        {"best-effort", "granted", "best-effort", "granted", true,
         "a best-effort owner carries a granted state through"},
        {"best-effort", "denied", "best-effort", "denied", true,
         "a best-effort owner carries a denied state through"},
        {"best-effort", "prompt", "best-effort", "prompt", true,
         "a best-effort owner carries a prompt state through"},
        {"best-effort", std::nullopt, "best-effort", std::nullopt, true,
         "a best-effort owner with NO state keeps an absent state rather than inventing one"},
    };

    for (const auto& row : rows) {
      auto persistence = flight::make_ref<flight::types::StoragePersistenceOutcome>();
      persistence->outcome = String(row.owner_outcome);
      if (row.owner_state.has_value()) {
        persistence->permission_state = String(row.owner_state.value());
      }

      int reads = 0;
      auto query_owner = flight::make_ref<flight::types::HostPreferencesPersistenceQueryCapability>();
      query_owner->get_persistence = [&persistence, &reads]() {
        ++reads;
        return flight::Task<flight::Ref<flight::types::StoragePersistenceOutcome>>::resolve(persistence);
      };

      const auto outcome = flight::permissions::get_permission_state(
                               flight::make_ref<Permissions>(), std::nullopt, query_owner,
                               String("persistent-storage"))
                               .get();
      if (!check(reads == 1, "the persistence owner is asked exactly once per query")) {
        return 1;
      }
      if (!check(query_reason(outcome) == row.expected_reason, row.message)) {
        return 1;
      }
      // A state KEY exists only in the two arms that declare one; the reason-only arm has none.
      const bool has_state_key = outcome.index() != 0;
      if (!check(has_state_key == row.has_state_key, row.message)) {
        return 1;
      }
      const auto state = outcome.index() == 1   ? std::optional<std::string>(std::get<1>(outcome)->state.to_utf8())
                         : outcome.index() == 2 ? std::get<2>(outcome)->state.has_value()
                                                      ? std::optional<std::string>(
                                                            std::get<2>(outcome)->state.value().to_utf8())
                                                      : std::nullopt
                                                : std::nullopt;
      if (!check(state.has_value() == row.expected_state.has_value() &&
                     (!state.has_value() || state.value() == row.expected_state.value()),
                 row.message)) {
        return 1;
      }
    }
  }

  // An absent persistence owner is reported structurally: the query does not fall through to the
  // permissions provider, which cannot answer a storage question.
  {
    const auto outcome = flight::permissions::get_permission_state(
                             flight::make_ref<Permissions>(), std::nullopt, std::nullopt,
                             String("persistent-storage"))
                             .get();
    if (!check(query_reason(outcome) == "unsupported",
               "a storage query with no persistence owner reports unsupported")) {
      return 1;
    }
  }

  // THE STORAGE PERSISTENCE REQUEST projection. This is the REQUEST-side sibling of the query table above,
  // and it had NO coverage at all: two mutations of its body were written for this audit and both SURVIVED
  // the whole suite, which is how the gap was found rather than guessed. The rows are the upstream table.
  // The first row is the same proof in the request vocabulary that the query table's first row is in the
  // query one: an owner reporting 'persistent' with permissionState 'prompt' must project to reason
  // 'granted' with state 'granted'. The projection decides; it does not read the owner's state back.
  {
    struct RequestRow {
      const char* owner_outcome;
      std::optional<const char*> owner_state;
      const char* expected_reason;
      std::optional<const char*> expected_state;
      bool has_state_key;
      const char* message;
    };
    const RequestRow rows[] = {
        {"persistent", "prompt", "granted", "granted", true,
         "a 'persistent' request owner projects to granted/granted whatever permissionState it reported"},
        {"operation-failed", "denied", "operation-failed", std::nullopt, false,
         "an 'operation-failed' request owner projects to a reason with no state invented for it"},
        {"best-effort", "granted", "best-effort", "granted", true,
         "a best-effort request owner carries a granted state through"},
        {"best-effort", "denied", "best-effort", "denied", true,
         "a best-effort request owner carries a denied state through"},
        {"best-effort", "prompt", "best-effort", "prompt", true,
         "a best-effort request owner carries a prompt state through"},
        {"best-effort", std::nullopt, "best-effort", std::nullopt, true,
         "a best-effort request owner with NO state keeps an absent state rather than inventing one"},
    };

    for (const auto& row : rows) {
      auto persistence = flight::make_ref<flight::types::StoragePersistenceOutcome>();
      persistence->outcome = String(row.owner_outcome);
      if (row.owner_state.has_value()) {
        persistence->permission_state = String(row.owner_state.value());
      }

      int requests = 0;
      auto request_owner = flight::make_ref<flight::types::HostPreferencesPersistenceRequestCapability>();
      request_owner->request_persistence = [&persistence, &requests]() {
        ++requests;
        return flight::Task<flight::Ref<flight::types::StoragePersistenceOutcome>>::resolve(persistence);
      };

      const auto outcome = flight::permissions::request_permission(
                               flight::make_ref<Permissions>(), request_owner, std::nullopt,
                               String("persistent-storage"))
                               .get();
      if (!check(requests == 1, "the persistence request owner is asked exactly once per request")) {
        return 1;
      }
      if (!check(request_reason(outcome) == row.expected_reason, row.message)) {
        return 1;
      }
      const bool has_state_key = outcome.index() != 0;
      if (!check(has_state_key == row.has_state_key, row.message)) {
        return 1;
      }
      const auto state = outcome.index() == 1
                             ? std::optional<std::string>(std::get<1>(outcome)->state.to_utf8())
                         : outcome.index() == 2
                             ? std::get<2>(outcome)->state.has_value()
                                   ? std::optional<std::string>(std::get<2>(outcome)->state.value().to_utf8())
                                   : std::nullopt
                             : std::nullopt;
      if (!check(state.has_value() == row.expected_state.has_value() &&
                     (!state.has_value() || state.value() == row.expected_state.value()),
                 row.message)) {
        return 1;
      }
    }
  }

  // An absent persistence-REQUEST owner is reported as unsupported. Upstream's case also asserts the
  // QUERY owner is not consulted; on this surface that half is structural rather than behavioural -- the
  // request entry point has no query-owner parameter at all, so there is nothing to cross into.
  {
    const auto outcome = flight::permissions::request_permission(
                             flight::make_ref<Permissions>(), std::nullopt, std::nullopt,
                             String("persistent-storage"))
                             .get();
    if (!check(request_reason(outcome) == "unsupported",
               "a storage request with no persistence-request owner reports unsupported")) {
      return 1;
    }
    if (!check(outcome.index() == 0, "as a reason-only outcome, with no state invented for it")) {
      return 1;
    }
  }

  // The summary is printed LAST. It used to sit hundreds of lines earlier -- every assertion after it
  // could fail while the run still emitted this line, so anything grepping for it read a failing run as
  // green. Found by auditing a sibling suite for the same defect.
  if (failures == 0) std::cout << "generated permissions: all checks passed\n";
  return failures == 0 ? 0 : 1;
}
