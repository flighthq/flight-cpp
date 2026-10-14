// Focused behavior tests for @flighthq/mediasession.
//
// The package emits whole at a627849 -- three modules, no refusals -- so nothing here supplies a header.
// What it checks is that the emitted lowering preserves what the pinned TypeScript actually promises.
// The upstream suite is the behavioral specification; every group below is a case it states, run against
// the emitted C++ rather than a local invention:
//
//   * a provider subscription's unsubscribe is released exactly once, and a *throwing* unsubscribe keeps
//     its map entry so a later detach can retry the same origin;
//   * dispose detaches and then clears the signal even when detaching throws, propagating the original;
//   * destroy uses a Set, so two distinct providers are destroyed once each in insertion order, the
//     sibling is attempted after one throws, and the first error is the one rethrown -- whether that
//     error is a class or a plain value, and with the retained failure being the first one either way;
//   * position state is classified locally -- the three invalid cases are answered without invoking the
//     provider at all -- and playback state misuse is a TypeError before the provider sees it.
//
// The aliased case -- one provider passed through *both* capability slots, which the source documents as
// destroying once -- is asserted in the destroy group, and every other call in this file passes two
// DISTINCT providers so that the two are never confused.
#include <flight/mediasession/mediasession.hpp>

#include <functional>
#include <iostream>
#include <memory>
#include <optional>

namespace {

int failures = 0;

bool check(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
  return condition;
}

using Cap = flight::types::HostMediaSessionCapability;
using Act = flight::types::HostMediaSessionActionCapability;
using DetailsRow = flight::StructuralRef<
    flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::MediaSessionActionDetails>>>>;
using MetadataRow = flight::StructuralRef<
    flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::MediaSessionMetadata>>>>;
using PositionRow = flight::StructuralRef<
    flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::MediaSessionPositionState>>>>;
using CommandsRow = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<Cap>>>>;
using ActionsRow = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<Act>>>>;

flight::types::MediaSessionClearMetadataOutcome ok_clear_metadata() {
  return flight::make_ref<flight::types::MediaSessionOperationOutcome<flight::String>>(
      flight::types::MediaSessionOperationOutcome<flight::String>{.reason = flight::String("ok")});
}

std::string reason_of(const flight::Ref<flight::types::MediaSessionOperationOutcome<flight::String>>& outcome) {
  return outcome ? outcome->reason.to_utf8() : std::string("<null>");
}

}  // namespace

int main() {
  using flight::String;

  // create / initialize
  {
    auto signal = flight::mediasession::create_media_session_action_signal(String("seekto"));
    check(signal != nullptr, "create_media_session_action_signal returns an entity");
    check(signal->action == String("seekto"), "the signal carries the action it was created for");
    check(signal->on_action != nullptr, "the signal is constructed with its own action signal");
  }

  // attach: forwards provider details, subscribes once, returns true
  {
    std::function<void(DetailsRow)> provider_handler;
    int subscribe_calls = 0;
    auto capability = flight::make_ref<Act>();
    capability->destroy = []() {};
    capability->subscribe = [&](flight::types::MediaSessionAction action,
                                std::function<void(DetailsRow)> handler)
        -> std::optional<std::function<void()>> {
      ++subscribe_calls;
      if (action != String("seekto")) return std::nullopt;
      provider_handler = std::move(handler);
      return std::function<void()>([]() {});
    };

    auto signal = flight::mediasession::create_media_session_action_signal(String("seekto"));
    int observed = 0;
    flight::signals::connect_signal<
        std::function<void(DetailsRow)>>(signal->on_action, [&observed](DetailsRow) { ++observed; });

    check(flight::mediasession::attach_media_session_action(capability, signal),
          "attach reports a provider subscription as attached");
    check(subscribe_calls == 1, "attach subscribes exactly once");

    auto details = flight::make_ref<flight::types::MediaSessionActionDetails>();
    details->action = String("seekto");
    details->seek_time = 8.0;
    details->fast_seek = true;
    provider_handler(DetailsRow(details));
    check(observed == 1, "provider details reach the signal's onAction listeners");

    // Reattaching releases the prior exact origin before subscribing again.
    check(flight::mediasession::attach_media_session_action(capability, signal),
          "reattach reports attached");
    check(subscribe_calls == 2, "reattach subscribes again after releasing the prior one");
    flight::mediasession::detach_media_session_action(signal);
  }

  // attach: a null provider subscription is a truthful refusal
  {
    auto capability = flight::make_ref<Act>();
    capability->destroy = []() {};
    capability->subscribe = [](flight::types::MediaSessionAction,
                               std::function<void(DetailsRow)>) -> std::optional<std::function<void()>> {
      return std::nullopt;
    };
    auto signal = flight::mediasession::create_media_session_action_signal(String("play"));
    check(!flight::mediasession::attach_media_session_action(capability, signal),
          "a null provider subscription attaches nothing and reports false");

    // Nothing was stored, so this detach has nothing to release; a later real subscription on the same
    // signal must still detach exactly once.
    flight::mediasession::detach_media_session_action(signal);
    int unsubscribe_calls = 0;
    capability->subscribe = [&unsubscribe_calls](flight::types::MediaSessionAction,
                                                 std::function<void(DetailsRow)>)
        -> std::optional<std::function<void()>> {
      return std::function<void()>([&unsubscribe_calls]() { ++unsubscribe_calls; });
    };
    check(flight::mediasession::attach_media_session_action(capability, signal),
          "the same signal attaches once the provider answers");
    flight::mediasession::detach_media_session_action(signal);
    check(unsubscribe_calls == 1, "and its own subscription is the one released");

    // Re-attaching must RELEASE the prior origin before subscribing the replacement. Without this the
    // mutation battery showed that an attach which only ADDS a subscription goes unnoticed: the first
    // origin is left live and its unsubscribe is never run.
    const int before_reattach = unsubscribe_calls;
    if (!check(flight::mediasession::attach_media_session_action(capability, signal),
               "attaching again to a fresh origin succeeds")) {
      return 1;
    }
    if (!check(flight::mediasession::attach_media_session_action(capability, signal),
               "and RE-attaching while attached succeeds")) {
      return 1;
    }
    if (!check(unsubscribe_calls == before_reattach + 1,
               "re-attaching released the prior origin before subscribing the replacement")) {
      return 1;
    }
    flight::mediasession::detach_media_session_action(signal);
    if (!check(unsubscribe_calls == before_reattach + 2, "and the final detach releases the replacement")) {
      return 1;
    }
  }

  // detach: releases exactly once, and a throwing unsubscribe keeps its entry for a retry
  {
    int unsubscribe_calls = 0;
    bool throwing = true;
    auto capability = flight::make_ref<Act>();
    capability->destroy = []() {};
    capability->subscribe = [&](flight::types::MediaSessionAction,
                               std::function<void(DetailsRow)>) -> std::optional<std::function<void()>> {
      return std::function<void()>([&unsubscribe_calls, &throwing]() {
        ++unsubscribe_calls;
        if (throwing) throw std::runtime_error("unsubscribe failed");
      });
    };
    auto signal = flight::mediasession::create_media_session_action_signal(String("pause"));
    check(flight::mediasession::attach_media_session_action(capability, signal), "attach before detach");

    bool threw = false;
    try {
      flight::mediasession::detach_media_session_action(signal);
    } catch (const std::exception&) {
      threw = true;
    }
    check(threw, "a throwing unsubscribe propagates");
    check(unsubscribe_calls == 1, "the unsubscribe was attempted once");

    throwing = false;
    flight::mediasession::detach_media_session_action(signal);
    check(unsubscribe_calls == 2,
          "the entry survived the throw, so a later detach retries the same origin");
    flight::mediasession::detach_media_session_action(signal);
    check(unsubscribe_calls == 2, "and the entry is released only after a successful release");
  }

  // dispose: clears the signal even when detaching throws, and propagates the original error
  {
    auto capability = flight::make_ref<Act>();
    capability->destroy = []() {};
    capability->subscribe = [](flight::types::MediaSessionAction,
                               std::function<void(DetailsRow)>) -> std::optional<std::function<void()>> {
      return std::function<void()>([]() { throw std::runtime_error("unsubscribe failed"); });
    };
    auto signal = flight::mediasession::create_media_session_action_signal(String("pause"));
    flight::mediasession::attach_media_session_action(capability, signal);
    // A listener is connected first, because the clearing below is only observable against a signal that
    // WAS live: with nothing connected the signal carries no data either way, and the assertion would hold
    // whether or not dispose cleared anything. That is what this pair of checks is for.
    flight::signals::connect_signal<std::function<void(DetailsRow)>>(signal->on_action, [](DetailsRow) {});
    check(signal->on_action->data.has_value(), "the signal is live before dispose");

    bool threw = false;
    std::string message;
    try {
      flight::mediasession::dispose_media_session_action_signal(signal);
    } catch (const std::exception& error) {
      threw = true;
      message = error.what();
    }
    check(threw && message == "unsubscribe failed", "dispose propagates the detach failure");
    check(!signal->on_action->data.has_value(), "dispose cleared the signal anyway");
  }

  // destroy: distinct providers, once each, in insertion order
  {
    std::string order;
    auto commands = flight::make_ref<Cap>();
    commands->clear_metadata = &ok_clear_metadata;
    commands->clear_position_state = []() { return ok_clear_metadata(); };
    commands->destroy = [&order]() { order += "commands;"; };
    commands->set_metadata = [](MetadataRow) { return ok_clear_metadata(); };
    commands->set_playback_state = [](flight::types::MediaSessionPlaybackState) { return ok_clear_metadata(); };
    commands->set_position_state = [](PositionRow) { return ok_clear_metadata(); };

    auto actions = flight::make_ref<Act>();
    actions->destroy = [&order]() { order += "actions;"; };
    actions->subscribe = [](flight::types::MediaSessionAction,
                            std::function<void(DetailsRow)>) -> std::optional<std::function<void()>> {
      return std::function<void()>([]() {});
    };

    flight::mediasession::destroy_media_session(commands, actions);
    check(order == "commands;actions;", "distinct providers are destroyed once each, in insertion order");
  }

  // destroy: one subject passed through BOTH capability slots is destroyed once, which is what the source
  // documents. This used to be the one case left unasserted here: the Set's element is a two-alternative
  // union, C++ compares a variant's index before its value, and the emitted lowering therefore answered
  // two calls for one subject. The rule that closes it is in the runtime's SameValueZero -- a union of
  // reference-backed subjects answers identity across its arms -- so the case is covered here rather than
  // filed, and docs/compiler-requests/union-set-identity-across-alternatives.md records the defect it
  // closes. The row views below are two schemas over one object, which is how a caller aliases: the
  // structural cast keeps the live owner.
  {
    int alias_destroys = 0;
    auto subject = flight::make_ref<Cap>();
    subject->destroy = [&alias_destroys]() { ++alias_destroys; };
    subject->clear_metadata = []() { return ok_clear_metadata(); };
    subject->clear_position_state = []() { return ok_clear_metadata(); };
    subject->set_metadata = [](MetadataRow) { return ok_clear_metadata(); };
    subject->set_playback_state = [](flight::types::MediaSessionPlaybackState) {
      return ok_clear_metadata();
    };
    subject->set_position_state = [](PositionRow) { return ok_clear_metadata(); };

    const CommandsRow commands_view(subject);
    const ActionsRow actions_view = flight::structural_ref_cast<ActionsRow>(commands_view);
    check(commands_view.identity() == actions_view.identity(),
          "the aliased views are two schemas over one subject");
    flight::mediasession::destroy_media_session(commands_view, actions_view);
    check(alias_destroys == 1, "one subject through both slots is destroyed exactly once");
  }

  // destroy: the sibling is still attempted after one throws, and the first error is rethrown
  {
    int action_destroys = 0;
    auto commands = flight::make_ref<Cap>();
    commands->clear_metadata = []() { return ok_clear_metadata(); };
    commands->clear_position_state = []() { return ok_clear_metadata(); };
    commands->destroy = []() { throw std::runtime_error("commands failed"); };
    commands->set_metadata = [](MetadataRow) { return ok_clear_metadata(); };
    commands->set_playback_state = [](flight::types::MediaSessionPlaybackState) { return ok_clear_metadata(); };
    commands->set_position_state = [](PositionRow) { return ok_clear_metadata(); };

    auto actions = flight::make_ref<Act>();
    actions->destroy = [&action_destroys]() { ++action_destroys; };
    actions->subscribe = [](flight::types::MediaSessionAction,
                            std::function<void(DetailsRow)>) -> std::optional<std::function<void()>> {
      return std::function<void()>([]() {});
    };

    bool threw = false;
    std::string message;
    try {
      flight::mediasession::destroy_media_session(commands, actions);
    } catch (const std::exception& error) {
      threw = true;
      message = error.what();
    }
    check(threw && message == "commands failed", "the first provider's error is the one rethrown");
    check(action_destroys == 1, "the distinct sibling provider is still destroyed");
  }

  // destroy: a failure that is NOT a class. Both handlers in this function only reify the caught value,
  // so both are emitted as `catch (...)`: the value reaches the handler, the slot reifies it with
  // std::current_exception(), and std::rethrow_exception hands the caller the same `int`. Before the
  // catch-handler-breadth patch the value escaped the handler, the loop was skipped, and the sibling was
  // never attempted.
  {
    int action_destroys = 0;
    auto commands = flight::make_ref<Cap>();
    commands->destroy = []() { throw 42; };
    auto actions = flight::make_ref<Act>();
    actions->destroy = [&action_destroys]() { ++action_destroys; };

    bool caught_number = false;
    int number = 0;
    try {
      flight::mediasession::destroy_media_session(commands, actions);
    } catch (int value) {
      caught_number = true;
      number = value;
    } catch (...) {
    }
    check(caught_number && number == 42,
          "a provider whose throw is not a class is rethrown to the caller as the value it was");
    check(action_destroys == 1, "and the distinct sibling is still attempted");
  }

  // destroy: retention is type-agnostic. The FIRST failure is kept whatever kind it is -- here a plain
  // value ahead of an exception -- because the record is `if nothing recorded yet, record`, not
  // `record the last`.
  {
    auto commands = flight::make_ref<Cap>();
    commands->destroy = []() { throw 7; };
    auto actions = flight::make_ref<Act>();
    actions->destroy = []() { throw std::runtime_error("actions failed"); };

    bool kept_number = false;
    int number = 0;
    try {
      flight::mediasession::destroy_media_session(commands, actions);
    } catch (int value) {
      kept_number = true;
      number = value;
    } catch (...) {
    }
    check(kept_number && number == 7,
          "the first failure is retained across later ones whatever kind it is");
  }

  // setPlaybackState: misuse is rejected before the provider observes it
  {
    int calls = 0;
    auto commands = flight::make_ref<Cap>();
    commands->clear_metadata = []() { return ok_clear_metadata(); };
    commands->clear_position_state = []() { return ok_clear_metadata(); };
    commands->destroy = []() {};
    commands->set_metadata = [](MetadataRow) { return ok_clear_metadata(); };
    commands->set_playback_state = [&calls](flight::types::MediaSessionPlaybackState) {
      ++calls;
      return ok_clear_metadata();
    };
    commands->set_position_state = [](PositionRow) { return ok_clear_metadata(); };

    bool threw = false;
    std::string message;
    try {
      flight::mediasession::set_media_session_playback_state(commands, String("buffering"));
    } catch (const std::exception& error) {
      threw = true;
      message = error.what();
    }
    check(threw, "an invalid playback state is a programmer error");
    check(message == "Invalid media-session playback state: buffering",
          "and it reports the offending value");
    check(calls == 0, "and the provider is never invoked for it");

    check(reason_of(flight::mediasession::set_media_session_playback_state(commands, String("playing"))) == "ok",
          "a valid playback state reaches the provider");
    check(calls == 1, "exactly once");

    // "none" is one of the THREE members of the union, not a fourth thing that happens to be spelled
    // differently: the mutation battery found this unasserted, so dropping `none` from the guard's
    // membership test looked harmless.
    // Reported as a check rather than left to throw: a version that classified `none` as invalid would
    // abort here, which is detection but says nothing about WHY. This says it.
    bool none_threw = false;
    std::string none_message;
    try {
      static_cast<void>(flight::mediasession::set_media_session_playback_state(commands, String("none")));
    } catch (const std::exception& error) {
      none_threw = true;
      none_message = error.what();
    }
    if (!check(!none_threw, ("`none` is a member of the union and is accepted" + none_message).c_str())) {
      return 1;
    }
    if (!check(calls == 2, "and it reached the provider rather than being classified away")) {
      return 1;
    }
  }

  // setPositionState: classified locally, provider untouched for each invalid case
  {
    int calls = 0;
    auto commands = flight::make_ref<Cap>();
    commands->clear_metadata = []() { return ok_clear_metadata(); };
    commands->clear_position_state = []() { return ok_clear_metadata(); };
    commands->destroy = []() {};
    commands->set_metadata = [](MetadataRow) { return ok_clear_metadata(); };
    commands->set_playback_state = [](flight::types::MediaSessionPlaybackState) { return ok_clear_metadata(); };
    commands->set_position_state = [&calls](PositionRow) {
      ++calls;
      return ok_clear_metadata();
    };

    auto state = flight::make_ref<flight::types::MediaSessionPositionState>();
    state->duration = 120.0;
    state->position = 30.0;
    state->playback_rate = 1.0;

    auto classify = [&](double duration, double position, double playback_rate) {
      state->duration = duration;
      state->position = position;
      state->playback_rate = playback_rate;
      return reason_of(flight::mediasession::set_media_session_position_state(commands, PositionRow(state)));
    };

    check(classify(-1.0, 0.0, 1.0) == "invalid-duration", "a non-positive duration is invalid");
    check(classify(0.0, 0.0, 1.0) == "invalid-duration", "a zero duration is invalid");
    check(classify(120.0, -1.0, 1.0) == "invalid-position", "a negative position is invalid");
    check(classify(120.0, 121.0, 1.0) == "invalid-position", "a position past the duration is invalid");
    check(classify(120.0, 30.0, 0.0) == "invalid-playback-rate", "a zero playback rate is invalid");
    check(classify(120.0, 30.0, -1.0) == "ok", "a negative playback rate is valid, as upstream states");
    check(calls == 1, "only the valid request reached the provider");

    // Invalid caller data wins over the provider being present: the position checks run first, so a
    // provider that would answer is never consulted for a request that cannot be valid anywhere.
    state->duration = 0.0;
    check(reason_of(flight::mediasession::set_media_session_position_state(commands, PositionRow(state))) ==
              "invalid-duration",
          "classification happens before the provider call");
    check(calls == 1, "and the provider count is unchanged");

    // The two boundaries the algorithm ACCEPTS. Only a position PAST the duration is rejected, and only a
    // NEGATIVE position is; the mutation battery found both boundaries unasserted, so an off-by-one at
    // either (`>` becoming `>=`, or `< 0` becoming `<= 0`) went unnoticed.
    if (!check(classify(120.0, 120.0, 1.0) == "ok",
               "a position EQUAL to the duration is valid, because only one PAST it is rejected")) {
      return 1;
    }
    if (!check(classify(120.0, 0.0, 1.0) == "ok",
               "and a zero position is valid, because only a negative one is rejected")) {
      return 1;
    }
    if (!check(calls == 3, "so both reached the provider rather than being classified away")) {
      return 1;
    }
  }

  // metadata and clear commands forward straight through
  {
    int metadata_calls = 0;
    int clear_calls = 0;
    auto commands = flight::make_ref<Cap>();
    commands->clear_metadata = [&clear_calls]() {
      ++clear_calls;
      return ok_clear_metadata();
    };
    commands->clear_position_state = [&clear_calls]() {
      ++clear_calls;
      return ok_clear_metadata();
    };
    commands->destroy = []() {};
    commands->set_metadata = [&metadata_calls](MetadataRow) {
      ++metadata_calls;
      return ok_clear_metadata();
    };
    commands->set_playback_state = [](flight::types::MediaSessionPlaybackState) { return ok_clear_metadata(); };
    commands->set_position_state = [](PositionRow) { return ok_clear_metadata(); };

    auto metadata = flight::make_ref<flight::types::MediaSessionMetadata>();
    metadata->title = String("Title");
    check(reason_of(flight::mediasession::set_media_session_metadata(commands, MetadataRow(metadata))) == "ok",
          "setMetadata forwards the metadata value");
    check(metadata_calls == 1, "and reaches the provider once");
    check(reason_of(flight::mediasession::clear_media_session_metadata(commands)) == "ok",
          "clearMetadata returns the provider's outcome");
    check(reason_of(flight::mediasession::clear_media_session_position_state(commands)) == "ok",
          "clearPositionState returns the provider's outcome");
    check(clear_calls == 2, "both clear commands reached the provider");
  }

  if (failures == 0) std::cout << "generated mediasession: all checks passed\n";
  return failures == 0 ? 0 : 1;
}
