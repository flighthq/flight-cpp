// Native parity test for @flighthq/notification, over its PARTIAL override (38 upstream cases).
//
// THE PACKAGE IS NOT A CLEAN EMISSION, and the difference shapes this file. The emitter refuses
// `notification.ts` itself -- refusal `unsupported-ir` at line 346, "Object.assign requires one closed
// callable-object target and an exact compatible object literal source", filed as
// object-assign-onto-structural-record -- so `contract.ts` and `index.ts` fall with it as dependency-refused. The
// generated `notification.hpp` therefore does NOT carry a usable module, and the tree ships a hand-written
// override instead, recorded in `overrides/manifest.json` as `notification-subscription-lifecycle` with status
// **partial**: it supplies the module while deliberately leaving surface out, and it holds the package below
// shippable. The tier agrees: `@flighthq/notification` is partial, 2 of 3 modules emitted, 3 of 3 headers compiled.
//
// WHAT THIS FILE COVERS, AND WHAT IT DELIBERATELY DOES NOT
//
// Covered, because the override implements it for real:
//
//   * The host routing calls. Each dispatches through the EXACT capability trait and returns the provider's own
//     outcome -- asserted by IDENTITY for the ones whose upstream case uses `resolves.toBe(outcome)`, so a
//     version that rebuilt the outcome object instead of forwarding it fails.
//   * The two resource constructors, including `createNotificationResource`'s tag defaulting to the empty string
//     and `createScheduledNotificationResource` carrying the caller's request and schedule BY IDENTITY.
//   * `releaseNotificationAttachment`: the attachment's own release outcome is returned, and a REJECTED release
//     becomes `operation-failed` rather than escaping to the caller.
//   * `closeNotification` and `cancelScheduledNotification`, which are the two real state machines here: no bound
//     handler is `operation-failed`; a bound handler that succeeds marks the resource closed, so the NEXT call is
//     `already-closed` and the handler is never run twice. A handler that FAILS must not mark it closed.
//   * The five subscription factories and the five detach wrappers, which report `not-attached` before an attach.
//
// NOT covered, and this is the deliberate part: the five `attachNotification*Subscription` and the five
// `disposeNotification*Subscription` wrappers are STUBS in the override -- the header says so, each attach always
// reports `operation-failed` with `attachFailed: true`, and each dispose always reports `already-disposed`. Their
// real behaviour depends on `createNotificationSubscription`, `attachNotificationSubscription` and
// `disposeNotificationSubscription`, which are the three functions the emitter REFUSED and the override marks
// `NOT GENERATED`. Asserting the stubs would certify a stub as parity, which is exactly what this repository's
// rule forbids, so those ten functions are left uncovered and their upstream cases are counted as unreachable
// rather than passing. `docs/parity/plat.md` records the same split.
#include <flight/notification/notification.hpp>

#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace {

namespace notif = flight::notification;
namespace types = flight::types;

using CloseOutcome = types::NotificationCloseOutcome;
using CancelOutcome = types::NotificationCancelOutcome;
using ReleaseOutcome = types::NotificationEventReleaseOutcome;
using Reason = types::reason_43a745d20647bfb6;
using DeliveryArm = types::notification_reason_20d622d94be300b5;
using ActiveListArm = types::notifications_reason_4e4b4bd41c47e2e0;
using DetachOutcome = types::NotificationSubscriptionDetachOutcome;

using RequestRow = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<types::NotificationRequest>>>>;
using ScheduleRow = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<types::NotificationSchedule>>>>;

int failures = 0;

void check(bool condition, const std::string& message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

void check_text(const flight::String& value, const std::string& expected, const std::string& message) {
  if (value.to_utf8() == expected) return;
  std::cerr << "FAIL: " << message << " (got \"" << value.to_utf8() << "\", want \"" << expected << "\")\n";
  ++failures;
}

// Every outcome in this package is a variant whose arms all carry a `reason` string, so one visitor reads them all.
template <typename Outcome>
std::string reason_of(const Outcome& outcome) {
  return std::visit([](const auto& carrier) -> std::string { return carrier->reason.to_utf8(); }, outcome);
}

flight::Ref<Reason> ok_reason() {
  auto carrier = flight::make_ref<Reason>();
  carrier->reason = flight::String("ok");
  return carrier;
}

// The success arms of the two lifecycle-shaped outcomes are the shared `reason` carrier at index 1.
types::NotificationLifecycleOutcome lifecycle_ok(flight::Ref<Reason> carrier) {
  return types::NotificationLifecycleOutcome{std::in_place_index<1>, std::move(carrier)};
}

types::NotificationPendingListOutcome pending_ok(flight::Ref<Reason> carrier) {
  return types::NotificationPendingListOutcome{std::in_place_index<1>, std::move(carrier)};
}

types::NotificationActiveListOutcome active_ok(flight::Ref<Reason> carrier) {
  return types::NotificationActiveListOutcome{std::in_place_index<1>, std::move(carrier)};
}

types::NotificationPermissionQueryOutcome permission_ok(flight::Ref<Reason> carrier) {
  return types::NotificationPermissionQueryOutcome{std::in_place_index<1>, std::move(carrier)};
}

// --- the host routing calls -------------------------------------------------------------------------------------

void test_closing_everything_forwards_the_providers_own_outcome() {
  const auto carrier = ok_reason();
  int calls = 0;
  auto host = flight::make_ref<types::HostNotificationCloseCapability>();
  host->close_all_notifications = [&calls, carrier]() {
    ++calls;
    return flight::Task<types::NotificationLifecycleOutcome>::resolve(lifecycle_ok(carrier));
  };

  const auto outcome = notif::close_all_notifications(host).get();

  check(calls == 1, "the provider's own close-all entry point is the one called");
  check(std::get<1>(outcome).get() == carrier.get(),
        "and the object it returned is what the caller receives -- the same object, not a rebuilt copy");
  check(reason_of(outcome) == "ok", "carrying the provider's own reason");
}

void test_destroying_capabilities_dispatches_through_lifecycle() {
  const auto carrier = ok_reason();
  int destroys = 0;
  auto host = flight::make_ref<types::HostNotificationLifecycleCapability>();
  host->destroy = [&destroys, carrier]() {
    ++destroys;
    return flight::Task<types::NotificationLifecycleOutcome>::resolve(lifecycle_ok(carrier));
  };

  const auto outcome = notif::destroy_notification_capabilities(host).get();

  check(destroys == 1, "destroy reaches the lifecycle trait");
  check(std::get<1>(outcome).get() == carrier.get(), "and hands back its outcome by identity");
}

void test_the_active_list_is_the_providers_list() {
  const auto notifications = flight::Array<flight::Ref<types::Notification>>{};
  auto arm = flight::make_ref<ActiveListArm>();
  arm->notifications = notifications;
  arm->reason = flight::String("ok");
  auto host = flight::make_ref<types::HostNotificationActiveListCapability>();
  host->get_active_notifications = [arm]() {
    return flight::Task<types::NotificationActiveListOutcome>::resolve(
        types::NotificationActiveListOutcome{std::in_place_index<0>, arm});
  };

  const auto outcome = notif::get_active_notifications(host).get();

  check(std::get<0>(outcome).get() == arm.get(), "the provider's active-list carrier is returned by identity");
  check(std::get<0>(outcome)->notifications.size() == 0, "with the list exactly as the provider built it");
}

void test_the_permission_query_and_request_use_their_own_entries() {
  const auto carrier = ok_reason();
  int queries = 0;
  int requests = 0;
  auto host = flight::make_ref<types::HostNotificationPermissionCapability>();
  host->get_permission = [&queries, carrier]() {
    ++queries;
    return flight::Task<types::NotificationPermissionQueryOutcome>::resolve(permission_ok(carrier));
  };
  const auto requested = flight::make_ref<types::NotificationPermissionRequestOutcome>();
  requested->reason = flight::String("granted");
  host->request_permission = [&requests, requested]() {
    ++requests;
    return flight::Task<flight::Ref<types::NotificationPermissionRequestOutcome>>::resolve(requested);
  };

  const auto query = notif::get_notification_permission(host).get();
  const auto request = notif::request_notification_permission(host).get();

  check(queries == 1 && requests == 1, "the query and the request are two distinct provider entries");
  check(std::get<1>(query).get() == carrier.get(), "the query returns its own outcome by identity");
  check(request.get() == requested.get(), "and so does the request");
  check_text(request->reason, "granted", "carrying the grant the provider reported");
}

void test_the_pending_list_is_the_providers_list() {
  const auto carrier = ok_reason();
  int calls = 0;
  auto host = flight::make_ref<types::HostNotificationSchedulingCapability>();
  host->get_pending_notifications = [&calls, carrier]() {
    ++calls;
    return flight::Task<types::NotificationPendingListOutcome>::resolve(pending_ok(carrier));
  };

  const auto outcome = notif::get_pending_notifications(host).get();

  check(calls == 1, "the pending list is read from the scheduling trait");
  check(std::get<1>(outcome).get() == carrier.get(), "and returned by identity");
}

void test_scheduling_forwards_the_request_and_schedule_it_was_given() {
  const auto request = flight::make_ref<types::NotificationRequest>();
  request->title = flight::String("Later");
  const auto schedule = flight::make_ref<types::NotificationSchedule>();
  schedule->at = 1234.0;

  const void* seen_request = nullptr;
  const void* seen_schedule = nullptr;
  auto arm = flight::make_ref<Reason>();
  arm->reason = flight::String("ok");
  auto host = flight::make_ref<types::HostNotificationSchedulingCapability>();
  host->schedule_notification = [&seen_request, &seen_schedule, arm](RequestRow given_request,
                                                                     ScheduleRow given_schedule) {
    seen_request = given_request.identity();
    seen_schedule = given_schedule.identity();
    return flight::Task<types::NotificationScheduleOutcome>::resolve(
        types::NotificationScheduleOutcome{std::in_place_index<2>, arm});
  };

  const auto outcome = notif::schedule_notification(host, request, schedule).get();

  check(seen_request == RequestRow(request).identity(),
        "the caller's own request object reaches the provider, not a copy of it");
  check(seen_schedule == ScheduleRow(schedule).identity(), "and its own schedule object too");
  check(std::get<2>(outcome).get() == arm.get(), "and the provider's outcome comes back by identity");
}

void test_showing_a_notification_returns_the_providers_delivery_arm() {
  const auto notification = flight::make_ref<types::Notification>();
  notification->id = flight::String("shown");
  auto arm = flight::make_ref<DeliveryArm>();
  arm->notification = notification;
  arm->reason = flight::String("accepted");
  double seen_at = -1.0;
  auto host = flight::make_ref<types::HostNotificationDeliveryCapability>();
  host->notify = [&seen_at, arm](RequestRow request) {
    seen_at = request.identity() ? 1.0 : 0.0;
    return flight::Task<types::NotificationDeliveryOutcome>::resolve(
        types::NotificationDeliveryOutcome{std::in_place_index<1>, arm});
  };

  const auto outcome = notif::show_notification(host, flight::make_ref<types::NotificationRequest>()).get();

  check(seen_at == 1.0, "the provider's notify is the entry point reached");
  check(std::get<1>(outcome).get() == arm.get(), "and its delivery carrier is returned by identity");
  check(std::get<1>(outcome)->notification.get() == notification.get(),
        "carrying the notification object the provider put in it");
  check(reason_of(outcome) == "accepted", "and its reason");
}

// A provider FAILURE is preserved rather than collapsed. The upstream suite names these cases for the
// permission query ("preserves query failure instead of collapsing it to denied") and the pending list
// ("preserves provider failure instead of returning an empty success"), and both functions are one-line
// forwards -- so the property holds by construction. That is an INFERENCE, which is why it is observed
// here instead: a provider's own denial arm is fed in and asserted to come back BY IDENTITY.
void test_a_provider_failure_is_preserved_rather_than_collapsed() {
  auto permission = flight::make_ref<types::HostNotificationPermissionCapability>();
  const auto denied = flight::make_ref<types::permission_reason_2a03a8a4faab9e18>();
  denied->reason = flight::String("denied");
  permission->get_permission = [denied]() {
    return flight::Task<types::NotificationPermissionQueryOutcome>::resolve(
        types::NotificationPermissionQueryOutcome{std::in_place_index<0>, denied});
  };

  const auto outcome = notif::get_notification_permission(permission).get();
  check(std::get<0>(outcome).get() == denied.get(),
        "a provider's denial comes back as ITS OWN object, not collapsed into an empty success");
  check(reason_of(outcome) == "denied", "carrying the provider's own reason");

  auto scheduling = flight::make_ref<types::HostNotificationSchedulingCapability>();
  const auto none_pending = flight::make_ref<types::notifications_reason_6255c80019d34f04>();
  none_pending->reason = flight::String("unsupported");
  scheduling->get_pending_notifications = [none_pending]() {
    return flight::Task<types::NotificationPendingListOutcome>::resolve(
        types::NotificationPendingListOutcome{std::in_place_index<0>, none_pending});
  };

  const auto pending = notif::get_pending_notifications(scheduling).get();
  check(std::get<0>(pending).get() == none_pending.get(),
        "and the pending list's provider failure is preserved the same way");
}

// --- the resource constructors ----------------------------------------------------------------------------------

void test_a_notification_resource_starts_with_its_given_facts() {
  const auto notification = notif::create_notification_resource(flight::String("n1"), flight::String("Title"),
                                                                flight::String("group"));
  check_text(notification->id, "n1", "the id it was created with");
  check_text(notification->title, "Title", "the title it was created with");
  check_text(notification->tag, "group", "and the tag it was created with");
  check(!notification->entity_runtime_key.has_value(),
        "with its runtime-key slot present but unset, so the entity carries no attached runtime");

  const auto untagged = notif::create_notification_resource(flight::String("n2"), flight::String("Title"));
  check_text(untagged->tag, "", "an omitted tag is the EMPTY STRING, not an absent value or the id");
}

void test_the_initializers_are_the_constructors_and_default_the_tag_themselves() {
  // The initializers are PUBLIC, and the mutation battery is what made that matter: `createNotificationResource`
  // resolves an omitted tag to "" BEFORE it calls the initializer, so the initializer's own `value_or` default is
  // never reached through the constructor. Calling it directly is the only way to exercise it -- and the mutation
  // that changed that default survived every constructor-level assertion.
  flight::Ref<flight::types::EntityConstruction<flight::Ref<types::Notification>>> out =
      flight::entity::allocate_entity<flight::Ref<types::Notification>>();
  notif::initialize_notification_resource(out, flight::String("i1"), flight::String("Title"), std::nullopt);
  const auto untagged = flight::entity::finish_entity<flight::Ref<types::Notification>>(out);
  check_text(untagged->id, "i1", "the initializer writes the id it is given");
  check_text(untagged->title, "Title", "and the title");
  check_text(untagged->tag, "", "and defaults an ABSENT tag to the empty string, in the initializer itself");

  flight::Ref<flight::types::EntityConstruction<flight::Ref<types::Notification>>> tagged_out =
      flight::entity::allocate_entity<flight::Ref<types::Notification>>();
  notif::initialize_notification_resource(tagged_out, flight::String("i2"), flight::String("Title"),
                                          flight::String("group"));
  check_text(flight::entity::finish_entity<flight::Ref<types::Notification>>(tagged_out)->tag, "group",
             "while a tag it is given is used as-is");

  const auto request = flight::make_ref<types::NotificationRequest>();
  request->title = flight::String("Later");
  const auto schedule = flight::make_ref<types::NotificationSchedule>();
  schedule->at = 3.0;
  flight::Ref<flight::types::EntityConstruction<flight::Ref<types::ScheduledNotification>>> scheduled_out =
      flight::entity::allocate_entity<flight::Ref<types::ScheduledNotification>>();
  notif::initialize_scheduled_notification_resource(scheduled_out, flight::String("i3"), request, schedule);
  const auto scheduled = flight::entity::finish_entity<flight::Ref<types::ScheduledNotification>>(scheduled_out);
  check_text(scheduled->id, "i3", "the scheduled initializer writes its id");
  check(scheduled->request.identity() == RequestRow(request).identity(),
        "and stores the request it was given by identity");
  check(scheduled->schedule.identity() == ScheduleRow(schedule).identity(), "and the schedule too");
}

void test_a_scheduled_notification_carries_the_callers_request_and_schedule() {
  const auto request = flight::make_ref<types::NotificationRequest>();
  request->title = flight::String("Later");
  const auto schedule = flight::make_ref<types::NotificationSchedule>();
  schedule->at = 7.0;

  const auto scheduled = notif::create_scheduled_notification_resource(flight::String("s1"), request, schedule);

  check_text(scheduled->id, "s1", "the id it was created with");
  check(scheduled->request.identity() == RequestRow(request).identity(),
        "the request it carries is the CALLER'S object, not a copy of it");
  check(scheduled->schedule.identity() == ScheduleRow(schedule).identity(), "and so is the schedule");
}

// --- releasing an attachment ------------------------------------------------------------------------------------

void test_releasing_an_attachment_returns_its_own_outcome() {
  const auto released = flight::make_ref<ReleaseOutcome>();
  released->reason = flight::String("ok");
  const auto attachment = flight::make_ref<types::NotificationEventAttachment>();
  attachment->release = [released]() { return flight::Task<flight::Ref<ReleaseOutcome>>::resolve(released); };

  const auto outcome = notif::release_notification_attachment(attachment).get();

  check(outcome.get() == released.get(), "the attachment's own release outcome is returned by identity");
  check_text(outcome->reason, "ok", "carrying its reason");
}

void test_a_failing_release_becomes_operation_failed_rather_than_escaping() {
  const auto attachment = flight::make_ref<types::NotificationEventAttachment>();
  attachment->release = []() {
    return flight::Task<flight::Ref<ReleaseOutcome>>::reject(
        flight::Error(flight::String("the backend refused the release")));
  };

  const auto outcome = notif::release_notification_attachment(attachment).get();

  check_text(outcome->reason, "operation-failed",
             "a release that throws is reported as a failed operation, not propagated to the caller");
}

// --- close: the pinned-handler state machine --------------------------------------------------------------------

void test_closing_without_a_bound_handler_is_operation_failed() {
  const auto notification = notif::create_notification_resource(flight::String("shared"), flight::String("A"));

  const auto outcome = notif::close_notification(notification).get();

  check_text(outcome->reason, "operation-failed",
             "a notification with no pinned close is a failed operation, not a silent success");
}

void test_closing_uses_only_the_bound_handler_and_is_idempotent() {
  const auto notification = notif::create_notification_resource(flight::String("same"), flight::String("A"));
  int closes = 0;
  notif::bind_notification_close(notification, [&closes]() {
    ++closes;
    auto outcome = flight::make_ref<CloseOutcome>();
    outcome->reason = flight::String("ok");
    return flight::Task<flight::Ref<CloseOutcome>>::resolve(outcome);
  });

  const auto first = notif::close_notification(notification).get();
  const auto second = notif::close_notification(notification).get();

  check_text(first->reason, "ok", "the bound handler's own outcome is returned");
  check_text(second->reason, "already-closed", "and the next call reports it as already closed");
  check(second.get() != first.get(), "as a distinct outcome object, not the first one again");
  check(closes == 1, "while the handler ran exactly ONCE for the pair");
}

void test_a_failed_close_does_not_mark_the_notification_closed() {
  const auto notification = notif::create_notification_resource(flight::String("retry"), flight::String("A"));
  int closes = 0;
  notif::bind_notification_close(notification, [&closes]() {
    ++closes;
    auto outcome = flight::make_ref<CloseOutcome>();
    // The FIRST attempt fails; the second succeeds. If a failure were treated as a close, the second call
    // would report already-closed and the handler would never run again.
    outcome->reason = flight::String(closes == 1 ? "operation-failed" : "ok");
    return flight::Task<flight::Ref<CloseOutcome>>::resolve(outcome);
  });

  const auto failed = notif::close_notification(notification).get();
  const auto retried = notif::close_notification(notification).get();

  check_text(failed->reason, "operation-failed", "a failed close reports the failure");
  check_text(retried->reason, "ok", "and the notification is still closable, because the failure did not mark it");
  check(closes == 2, "so the bound handler is reached again rather than short-circuited");
}

// --- cancel: the same machine over a different resource ---------------------------------------------------------

void test_cancelling_without_a_bound_handler_is_operation_failed() {
  const auto request = flight::make_ref<types::NotificationRequest>();
  request->title = flight::String("Later");
  const auto schedule = flight::make_ref<types::NotificationSchedule>();
  schedule->at = 1.0;
  const auto scheduled = notif::create_scheduled_notification_resource(flight::String("s"), request, schedule);

  const auto outcome = notif::cancel_scheduled_notification(scheduled).get();

  check_text(outcome->reason, "operation-failed", "a scheduled notification with no pinned cancel fails");
}

void test_cancelling_is_pinned_and_idempotent() {
  const auto request = flight::make_ref<types::NotificationRequest>();
  request->title = flight::String("Later");
  const auto schedule = flight::make_ref<types::NotificationSchedule>();
  schedule->at = 1.0;
  const auto scheduled = notif::create_scheduled_notification_resource(flight::String("same"), request, schedule);

  int cancels = 0;
  notif::bind_scheduled_notification_cancel(scheduled, [&cancels]() {
    ++cancels;
    auto outcome = flight::make_ref<CancelOutcome>();
    outcome->reason = flight::String("ok");
    return flight::Task<flight::Ref<CancelOutcome>>::resolve(outcome);
  });

  const auto first = notif::cancel_scheduled_notification(scheduled).get();
  const auto second = notif::cancel_scheduled_notification(scheduled).get();

  check_text(first->reason, "ok", "the bound cancel's own outcome is returned");
  check_text(second->reason, "already-cancelled", "and the next call reports it as already cancelled");
  check(cancels == 1, "while the handler ran exactly once");
}

// --- the subscription factories and their detach wrappers -------------------------------------------------------

void test_every_subscription_factory_allocates_an_entity_with_an_inert_signal() {
  const auto action = notif::create_notification_action_subscription();
  const auto click = notif::create_notification_click_subscription();
  const auto dismiss = notif::create_notification_dismiss_subscription();
  const auto received = notif::create_notification_received_subscription();
  const auto reply = notif::create_notification_reply_subscription();

  check(!action->entity_runtime_key.has_value() && !click->entity_runtime_key.has_value() &&
            !dismiss->entity_runtime_key.has_value() && !received->entity_runtime_key.has_value() &&
            !reply->entity_runtime_key.has_value(),
        "each factory returns an entity whose runtime-key slot is present but unset");
  check(static_cast<bool>(action->on_notification_action) && static_cast<bool>(click->on_notification_click) &&
            static_cast<bool>(dismiss->on_notification_dismiss) &&
            static_cast<bool>(received->on_notification_received) && static_cast<bool>(reply->on_notification_reply),
        "and each carries a live signal, so it is inert but subscribable rather than absent");
  check(static_cast<const void*>(action.get()) != static_cast<const void*>(click.get()),
        "and the five are distinct entities");
}

void test_detaching_before_an_attach_is_not_attached() {
  const auto action = notif::detach_notification_action_subscription(
      notif::create_notification_action_subscription()).get();
  const auto click =
      notif::detach_notification_click_subscription(notif::create_notification_click_subscription()).get();
  const auto dismiss =
      notif::detach_notification_dismiss_subscription(notif::create_notification_dismiss_subscription()).get();
  const auto received =
      notif::detach_notification_received_subscription(notif::create_notification_received_subscription()).get();
  const auto reply =
      notif::detach_notification_reply_subscription(notif::create_notification_reply_subscription()).get();

  check(reason_of(action) == "not-attached" && reason_of(click) == "not-attached" &&
            reason_of(dismiss) == "not-attached" && reason_of(received) == "not-attached" &&
            reason_of(reply) == "not-attached",
        "all five detach wrappers are harmless before an attach and report it as not-attached");
}

} // namespace

int main() {
  test_closing_everything_forwards_the_providers_own_outcome();
  test_destroying_capabilities_dispatches_through_lifecycle();
  test_the_active_list_is_the_providers_list();
  test_the_permission_query_and_request_use_their_own_entries();
  test_the_pending_list_is_the_providers_list();
  test_scheduling_forwards_the_request_and_schedule_it_was_given();
  test_showing_a_notification_returns_the_providers_delivery_arm();

  test_a_notification_resource_starts_with_its_given_facts();
  test_the_initializers_are_the_constructors_and_default_the_tag_themselves();
  test_a_scheduled_notification_carries_the_callers_request_and_schedule();

  test_a_provider_failure_is_preserved_rather_than_collapsed();
  test_releasing_an_attachment_returns_its_own_outcome();
  test_a_failing_release_becomes_operation_failed_rather_than_escaping();

  test_closing_without_a_bound_handler_is_operation_failed();
  test_closing_uses_only_the_bound_handler_and_is_idempotent();
  test_a_failed_close_does_not_mark_the_notification_closed();

  test_cancelling_without_a_bound_handler_is_operation_failed();
  test_cancelling_is_pinned_and_idempotent();

  test_every_subscription_factory_allocates_an_entity_with_an_inert_signal();
  test_detaching_before_an_attach_is_not_attached();

  if (failures == 0) {
    std::cout << "notification parity: all checks passed\n";
    return 0;
  }
  std::cerr << failures << " check(s) failed\n";
  return 1;
}
