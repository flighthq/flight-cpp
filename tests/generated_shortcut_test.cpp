// Focused behavior tests for @flighthq/shortcut's explicit-dependency module.
//
// The package is `assisted` at a627849: `shortcut.ts` emits whole and
// `shortcutExplicitDependency.ts` is supplied by overrides/include/flight/shortcut/
// shortcut_explicit_dependency.hpp, which carries the emitted text plus the four declarations the emitter
// refuses (three query constants and attachGlobalShortcut).
//
// Every case below is ported from the module's own upstream suite
// (packages/shortcut/src/shortcutExplicitDependency.test.ts), which is the specification:
//
//   * attach maps a native refusal, a provider fault, an existing attachment and an existing same-chord
//     registration to their own outcomes, and the first live registration is not overwritten;
//   * create returns an entity carrying the normalized accelerator and its own trigger signal, and a
//     parse failure is reported before any provider is involved;
//   * detach releases through the CREATOR-pinned provider after a host replacement, and a failed native
//     release stays attached for an exact retry;
//   * dispose clears the consumer signal even when the release fails, and leaves the retry possible;
//   * the query pair parses before asking and distinguishes registered, not-registered and a provider
//     fault.
//
// All fourteen upstream cases are ported. The in-progress one ("reports an in-progress same-chord
// acquisition without invoking the provider twice") needs a provider whose subscribe task is still
// suspended while a second attach runs; `flight::Task::create` builds exactly that, so the case is ported
// directly rather than approached through the already-registered case.
#include <flight/executor.hpp>
#include <flight/shortcut/shortcut_explicit_dependency.hpp>

#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <variant>

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
using Trigger = flight::types::HostShortcutTriggerCapability;
using Query = flight::types::HostShortcutQueryCapability;
using Subscription = flight::types::ShortcutTriggerSubscription;
using SubscribeOutcome = flight::types::ShortcutTriggerSubscribeOutcome;
using UnsubscribeOutcome = flight::types::ShortcutTriggerUnsubscribeOutcome;
using AttachOutcome = flight::types::GlobalShortcutAttachOutcome;
using QueryOutcome = flight::types::GlobalShortcutQueryOutcome;

using SubscribeRow = flight::StructuralRef<
    flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::reason_subscription_c16c6a57c767b33b>>>>;
using RefusedRow =
    flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::reason_4890f49c03969010>>>>;

std::string reason_of(const AttachOutcome& outcome) {
  return flight::row_get<flight::RowKey<"reason">>(outcome).to_utf8();
}

std::string query_reason_of(const QueryOutcome& outcome) {
  if (outcome.index() == 0) return "<parse-error>";
  return flight::row_get<flight::RowKey<"reason">>(std::get<1>(outcome)).to_utf8();
}

SubscribeOutcome subscribed(const flight::Ref<Subscription>& subscription) {
  return SubscribeOutcome{
      std::in_place_index<1>,
      SubscribeRow(flight::make_ref<flight::types::reason_subscription_c16c6a57c767b33b>(
          flight::types::reason_subscription_c16c6a57c767b33b{.reason = String("subscribed"),
                                                              .subscription = subscription}))};
}

SubscribeOutcome refused() {
  return SubscribeOutcome{
      std::in_place_index<0>,
      RefusedRow(flight::make_ref<flight::types::reason_4890f49c03969010>(
          flight::types::reason_4890f49c03969010{.reason = String("refused")}))};
}

flight::Ref<UnsubscribeOutcome> unsubscribe_outcome(const char* reason) {
  return flight::make_ref<UnsubscribeOutcome>(UnsubscribeOutcome{.reason = String(reason)});
}

flight::Ref<flight::types::GlobalShortcut> shortcut_for(const char* accelerator) {
  const auto created = flight::shortcut::create_global_shortcut(String(accelerator));
  const auto* arm = std::get_if<1>(&created);
  if (arm == nullptr) return nullptr;
  return flight::row_get<flight::RowKey<"shortcut">>(*arm);
}

}  // namespace

int main() {
  const auto executor = std::make_shared<flight::QueueExecutor>();
  const flight::ExecutorScope scope(executor);

  // createGlobalShortcut: the normalized accelerator, its own signal, and the parse failure.
  {
    const auto shortcut = shortcut_for("shift+ctrl+k");
    check(shortcut != nullptr, "a valid accelerator creates an entity");
    check(shortcut != nullptr && shortcut->accelerator == String("Control+Shift+K"),
          "the accelerator is carried normalized, in the canonical modifier order");
    check(shortcut != nullptr && shortcut->on_trigger != nullptr,
          "and the entity owns its own trigger signal");

    const auto single = shortcut_for("K");
    check(single != nullptr && single->accelerator == String("K"), "a bare key is a valid accelerator");

    const auto failed = flight::shortcut::create_global_shortcut(String(""));
    check(failed.index() == 0, "an empty accelerator is a parse failure, not a created shortcut");

    const auto unknown = flight::shortcut::create_global_shortcut(String("Control+Shift"));
    check(unknown.index() == 0, "a modifier sequence with no key is a parse failure");
    const auto unknown_key = flight::shortcut::create_global_shortcut(String("Control+NotAKey"));
    check(unknown_key.index() == 0, "an unknown key is a parse failure, as the upstream suite states");
  }

  // attachGlobalShortcut: the refusal, the registration, and the two guards.
  {
    int subscribes = 0;
    auto host = flight::make_ref<Trigger>();
    auto subscription = flight::make_ref<Subscription>();
    host->destroy = []() { return flight::Task<void>::resolve(); };
    host->subscribe = [&](String, std::function<void()>) {
      ++subscribes;
      return flight::Task<SubscribeOutcome>::resolve(subscribed(subscription));
    };
    host->unsubscribe = [](flight::Ref<Subscription>) {
      return flight::Task<flight::Ref<UnsubscribeOutcome>>::resolve(unsubscribe_outcome("unsubscribed"));
    };

    const auto shortcut = shortcut_for("K");
    check(reason_of(flight::shortcut::attach_global_shortcut(host, shortcut).get()) == "attached",
          "a first attach reports attached");
    check(subscribes == 1, "and subscribes once");
    check(reason_of(flight::shortcut::attach_global_shortcut(host, shortcut).get()) == "already-attached",
          "attaching the same shortcut twice reports already-attached");
    check(subscribes == 1, "without subscribing again");

    const auto other = shortcut_for("J");
    const auto same_chord = shortcut_for("K");
    check(reason_of(flight::shortcut::attach_global_shortcut(host, same_chord).get()) ==
              "already-registered",
          "a different shortcut on a live chord reports already-registered");
    check(subscribes == 1, "and does not overwrite the first registration");
    check(reason_of(flight::shortcut::detach_global_shortcut(host, shortcut).get()) == "detached",
          "the first registration is still the live one");

    // A native refusal, and a provider that faults.
    auto refusing = flight::make_ref<Trigger>();
    refusing->destroy = []() { return flight::Task<void>::resolve(); };
    refusing->subscribe = [](String, std::function<void()>) {
      return flight::Task<SubscribeOutcome>::resolve(refused());
    };
    refusing->unsubscribe = [](flight::Ref<Subscription>) {
      return flight::Task<flight::Ref<UnsubscribeOutcome>>::resolve(unsubscribe_outcome("unsubscribed"));
    };
    check(reason_of(flight::shortcut::attach_global_shortcut(refusing, other).get()) == "native-refused",
          "a native refusal reports itself before the shortcut is attached");

    auto throwing = flight::make_ref<Trigger>();
    throwing->destroy = []() { return flight::Task<void>::resolve(); };
    throwing->subscribe = [](String, std::function<void()>) -> flight::Task<SubscribeOutcome> {
      throw std::runtime_error("provider fault");
    };
    throwing->unsubscribe = [](flight::Ref<Subscription>) {
      return flight::Task<flight::Ref<UnsubscribeOutcome>>::resolve(unsubscribe_outcome("unsubscribed"));
    };
    check(reason_of(flight::shortcut::attach_global_shortcut(throwing, other).get()) ==
              "trigger-provider-failed",
          "an attempted provider fault reports failure without inventing provider absence");
  }

  // An acquisition still in flight holds its chord: a second attach for the same chord is refused while the
  // first is outstanding, and the provider is asked exactly once.
  {
    int subscribes = 0;
    std::function<void(SubscribeOutcome)> settle;
    auto host = flight::make_ref<Trigger>();
    host->destroy = []() { return flight::Task<void>::resolve(); };
    host->subscribe = [&](String, std::function<void()>) -> flight::Task<SubscribeOutcome> {
      ++subscribes;
      // A deferred task settled by the test rather than by the provider: this is the one place the port needs
      // a subscribe that is still suspended when the second attach runs.
      return flight::Task<SubscribeOutcome>::create([&settle](auto resolver) {
        settle = [resolver](SubscribeOutcome value) mutable { resolver(std::move(value)); };
      });
    };
    host->unsubscribe = [](flight::Ref<Subscription>) {
      return flight::Task<flight::Ref<UnsubscribeOutcome>>::resolve(unsubscribe_outcome("unsubscribed"));
    };

    const auto first = shortcut_for("Control+P");
    const auto same_chord = shortcut_for("ctrl+p");
    auto outstanding = flight::shortcut::attach_global_shortcut(host, first);
    check(subscribes == 1, "the first attach reached the provider");
    check(settle != nullptr, "and the provider left the request outstanding");

    check(reason_of(flight::shortcut::attach_global_shortcut(host, same_chord).get()) ==
              "registration-in-progress",
          "a second attach for the same chord while the first is in flight reports registration-in-progress");
    check(subscribes == 1, "and never invokes the provider a second time");

    settle(subscribed(flight::make_ref<Subscription>()));
    check(reason_of(outstanding.get()) == "attached",
          "once the outstanding acquisition settles, the first attach reports attached");
    check(reason_of(flight::shortcut::detach_global_shortcut(host, first).get()) == "detached",
          "and the chord it held is released again");
  }

  // The trigger reaches the shortcut's signal, and the subscription is owned by the attachment.
  {
    std::function<void()> registered;
    auto host = flight::make_ref<Trigger>();
    host->destroy = []() { return flight::Task<void>::resolve(); };
    host->subscribe = [&](String, std::function<void()> listener) {
      registered = std::move(listener);
      return flight::Task<SubscribeOutcome>::resolve(subscribed(flight::make_ref<Subscription>()));
    };
    host->unsubscribe = [](flight::Ref<Subscription>) {
      return flight::Task<flight::Ref<UnsubscribeOutcome>>::resolve(unsubscribe_outcome("unsubscribed"));
    };

    const auto shortcut = shortcut_for("L");
    int triggers = 0;
    flight::signals::connect_signal<std::function<void()>>(shortcut->on_trigger, [&triggers]() { ++triggers; });
    check(reason_of(flight::shortcut::attach_global_shortcut(host, shortcut).get()) == "attached",
          "the shortcut attached for the trigger check");
    registered();
    check(triggers == 1, "the provider's trigger reaches the shortcut's signal");
  }

  // detachGlobalShortcut: the creator-pinned provider, and a failed release retained for a retry.
  {
    int first_unsubscribes = 0;
    int second_unsubscribes = 0;
    bool fail_release = true;
    auto first = flight::make_ref<Trigger>();
    first->destroy = []() { return flight::Task<void>::resolve(); };
    first->subscribe = [](String, std::function<void()>) {
      return flight::Task<SubscribeOutcome>::resolve(subscribed(flight::make_ref<Subscription>()));
    };
    first->unsubscribe = [&](flight::Ref<Subscription>) {
      ++first_unsubscribes;
      return flight::Task<flight::Ref<UnsubscribeOutcome>>::resolve(
          unsubscribe_outcome(fail_release ? "unknown-subscription" : "unsubscribed"));
    };
    auto replacement = flight::make_ref<Trigger>();
    replacement->destroy = []() { return flight::Task<void>::resolve(); };
    replacement->subscribe = [](String, std::function<void()>) {
      return flight::Task<SubscribeOutcome>::resolve(subscribed(flight::make_ref<Subscription>()));
    };
    replacement->unsubscribe = [&](flight::Ref<Subscription>) {
      ++second_unsubscribes;
      return flight::Task<flight::Ref<UnsubscribeOutcome>>::resolve(unsubscribe_outcome("unsubscribed"));
    };

    const auto shortcut = shortcut_for("N");
    check(reason_of(flight::shortcut::attach_global_shortcut(first, shortcut).get()) == "attached",
          "attach through the first provider");

    // A failed release keeps the attachment, so the shortcut is still attached.
    check(reason_of(flight::shortcut::detach_global_shortcut(replacement, shortcut).get()) ==
              "trigger-provider-failed",
          "a failed native release reports failure");
    check(first_unsubscribes == 1 && second_unsubscribes == 0,
          "and the release went to the CREATOR-pinned provider, not the replacement");
    check(reason_of(flight::shortcut::attach_global_shortcut(first, shortcut).get()) == "already-attached",
          "a failed release leaves the shortcut attached");

    // The retry with the release succeeding detaches exactly once more, through the same provider.
    fail_release = false;
    check(reason_of(flight::shortcut::detach_global_shortcut(replacement, shortcut).get()) == "detached",
          "the retry detaches");
    check(first_unsubscribes == 2 && second_unsubscribes == 0,
          "still through the creator-pinned provider");
    check(reason_of(flight::shortcut::detach_global_shortcut(first, shortcut).get()) == "not-attached",
          "and a second detach reports not-attached");

    const auto unattached = shortcut_for("M");
    check(reason_of(flight::shortcut::detach_global_shortcut(first, unattached).get()) == "not-attached",
          "detaching a shortcut that was never attached reports not-attached");
  }

  // disposeGlobalShortcut: the signal is cleared even when the release fails, and the retry survives.
  {
    bool fail_release = true;
    auto host = flight::make_ref<Trigger>();
    host->destroy = []() { return flight::Task<void>::resolve(); };
    host->subscribe = [](String, std::function<void()>) {
      return flight::Task<SubscribeOutcome>::resolve(subscribed(flight::make_ref<Subscription>()));
    };
    host->unsubscribe = [&](flight::Ref<Subscription>) {
      return flight::Task<flight::Ref<UnsubscribeOutcome>>::resolve(
          unsubscribe_outcome(fail_release ? "unknown-subscription" : "unsubscribed"));
    };

    const auto shortcut = shortcut_for("P");
    int triggers = 0;
    flight::signals::connect_signal<std::function<void()>>(shortcut->on_trigger, [&triggers]() { ++triggers; });
    static_cast<void>(flight::shortcut::attach_global_shortcut(host, shortcut).get());
    check(shortcut->on_trigger->data.has_value(),
          "the consumer signal is live while the shortcut is attached");
    check(reason_of(flight::shortcut::dispose_global_shortcut(host, shortcut).get()) ==
              "trigger-provider-failed",
          "dispose reports the failed release");
    // The clearing is only observable against a signal that WAS live: with nothing connected the signal has no
    // data either way, so this pair of checks is what makes the clearing itself the thing under test.
    check(!shortcut->on_trigger->data.has_value(), "and clears the consumer signal anyway");
    check(reason_of(flight::shortcut::attach_global_shortcut(host, shortcut).get()) == "already-attached",
          "a failed release remains attached so a later detach can retry it");
  }

  // destroyShortcutTrigger: the provider's own destroy boundary is awaited.
  {
    int destroys = 0;
    auto host = flight::make_ref<Trigger>();
    host->destroy = [&destroys]() {
      ++destroys;
      return flight::Task<void>::resolve();
    };
    host->subscribe = [](String, std::function<void()>) {
      return flight::Task<SubscribeOutcome>::resolve(subscribed(flight::make_ref<Subscription>()));
    };
    host->unsubscribe = [](flight::Ref<Subscription>) {
      return flight::Task<flight::Ref<UnsubscribeOutcome>>::resolve(unsubscribe_outcome("unsubscribed"));
    };
    flight::shortcut::destroy_shortcut_trigger(host).get();
    check(destroys == 1, "destroyShortcutTrigger awaits the provider's destroy");
  }

  // The query pair: parse before asking, then the three answers.
  {
    int queries = 0;
    bool registered = true;
    bool fault = false;
    auto host = flight::make_ref<Query>();
    host->is_registered = [&](String) {
      ++queries;
      if (fault) throw std::runtime_error("query fault");
      return flight::Task<bool>::resolve(registered);
    };

    check(query_reason_of(flight::shortcut::query_global_shortcut_registration(host, String("")).get()) ==
              "<parse-error>",
          "an unparseable accelerator is reported before the provider is asked");
    check(queries == 0, "and the provider was never asked");

    check(query_reason_of(flight::shortcut::query_global_shortcut_registration(host, String("K")).get()) ==
              "registered",
          "a registered chord reports registered");
    registered = false;
    check(query_reason_of(flight::shortcut::query_global_shortcut_registration(host, String("K")).get()) ==
              "not-registered",
          "an unregistered chord reports not-registered");
    fault = true;
    check(query_reason_of(flight::shortcut::query_global_shortcut_registration(host, String("K")).get()) ==
              "query-provider-failed",
          "a provider fault reports itself rather than an answer");
    check(queries == 3, "and the provider was asked exactly once per valid call");

    fault = false;
    check(query_reason_of(flight::shortcut::query_global_shortcut_conflict(host, String("K")).get()) ==
              "not-registered",
          "the conflict query uses the same witness and outcome");
  }

  if (failures == 0) std::cout << "generated shortcut: all checks passed\n";
  return failures == 0 ? 0 : 1;
}
