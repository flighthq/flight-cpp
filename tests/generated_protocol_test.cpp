// Focused behavior tests for @flighthq/protocol.
//
// The package emits whole at a627849 -- 3/3 modules, no refusals, no override -- so this is the
// emission's own behavior under test. Every case is ported from the module's upstream suite
// (packages/protocol/src/protocol.test.ts), which is the specification:
//
//   * the URL grammar: the parser's scheme/path/query split with `+`-to-space decoding and a malformed
//     escape preserved, and the builder's path normalization with the empty query key dropped;
//   * the scheme rules: custom grammar accepted, reserved and malformed rejected, and every delegating
//     entry point validating BEFORE it touches the provider;
//   * the batch registration prevalidating the whole list -- no provider call at all -- and then
//     attempting EVERY entry and aggregating, so one failure does not stop the rest;
//   * acquisition and release: attach replaces a prior subscription, detach releases exactly once
//     across repeated calls, dispose clears the handler's listeners, and two live hosts each serve
//     their own handler.
#include <flight/executor.hpp>
// ONE UPSTREAM CASE IS AN ABSENCE AND IS RECORDED RATHER THAN ASSERTED. `protocolHost.test.ts` requires the
// module to expose no ambient resolver family -- `explainProtocolBackend`, `getProtocolBackend`,
// `installProtocolHostBackend`, `observeProtocolHostResult`, `resetProtocolBackendForTest`,
// `setProtocolBackend`. Measured: a C++ qualified lookup of a name a namespace does not declare is a HARD error
// rather than a substitution failure -- `requires { flight::protocol::set_protocol_backend; }` fails to compile
// with "no member named 'set_protocol_backend' in namespace 'flight::protocol'" instead of evaluating false -- so
// the absence cannot be checked in-process. The positive half is asserted below (the module's whole surface is the
// operations this file calls and nothing else) and the absence itself is this note, not a dressed-up runtime check.
#include <flight/protocol/protocol.hpp>

#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
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
using Handler = flight::types::ProtocolHandler;
using Parsed = flight::types::ParsedProtocolUrl;
using PartialParsedRow = flight::StructuralRef<
    flight::RowReadonly<flight::RowPartial<flight::RowOf<flight::Ref<Parsed>>>>>;

std::string text(const std::optional<String>& value) {
  return value.has_value() ? value.value().to_utf8() : std::string("<none>");
}

// A protocol host whose pieces are as explicit as the module's own parameters: one slot per operation.
struct FakeProtocolHost {
  std::function<void(String)> open_listener;
  int subscribes = 0;
  int unsubscribes = 0;
  std::vector<std::string> calls;
  std::function<void(String)> deliver;

  flight::Ref<flight::types::HostProtocolOpenCapability> open_capability() {
    auto host = flight::make_ref<flight::types::HostProtocolOpenCapability>();
    host->subscribe = [this](std::function<void(String)> listener) {
      ++subscribes;
      open_listener = std::move(listener);
      return std::function<void()>([this]() {
        ++unsubscribes;
        open_listener = nullptr;
      });
    };
    return host;
  }
};

}  // namespace

int main() {
  const auto executor = std::make_shared<flight::QueueExecutor>();
  const flight::ExecutorScope scope(executor);

  // createProtocolHandler: an entity carrying its own open-url signal.
  {
    const auto handler = flight::protocol::create_protocol_handler();
    check(handler != nullptr && handler->on_open_url != nullptr,
          "createProtocolHandler builds an entity with its open-url signal");
  }

  // attachProtocolHandler: live opens reach the handler, and a reattach replaces the prior subscription.
  {
    FakeProtocolHost host;
    const auto capability = host.open_capability();
    const auto handler = flight::protocol::create_protocol_handler();
    std::vector<std::string> received;
    flight::signals::connect_signal<std::function<void(String)>>(
        handler->on_open_url, [&received](String url) { received.push_back(url.to_utf8()); });

    flight::protocol::attach_protocol_handler(capability, handler);
    check(host.subscribes == 1, "attach subscribes once");
    host.open_listener(String("flight://one"));
    check(received.size() == 1 && received[0] == "flight://one", "and a live open reaches the signal");

    flight::protocol::attach_protocol_handler(capability, handler);
    check(host.subscribes == 2, "reattach subscribes again");
    check(host.unsubscribes == 1, "after releasing the prior subscription exactly once");
    host.open_listener(String("flight://two"));
    check(received.size() == 2 && received[1] == "flight://two",
          "and the new subscription is the live one");
  }

  // detachProtocolHandler: the current provider is unsubscribed once, however often detach runs.
  {
    FakeProtocolHost host;
    const auto capability = host.open_capability();
    const auto handler = flight::protocol::create_protocol_handler();
    flight::protocol::attach_protocol_handler(capability, handler);
    flight::protocol::detach_protocol_handler(handler);
    flight::protocol::detach_protocol_handler(handler);
    check(host.unsubscribes == 1, "detach releases the current subscription exactly once");
  }

  // disposeProtocolHandler: detaches and clears the handler's signal listeners.
  {
    FakeProtocolHost host;
    const auto capability = host.open_capability();
    const auto handler = flight::protocol::create_protocol_handler();
    int received = 0;
    flight::signals::connect_signal<std::function<void(String)>>(
        handler->on_open_url, [&received](String) { ++received; });
    flight::protocol::attach_protocol_handler(capability, handler);
    flight::protocol::dispose_protocol_handler(handler);
    check(host.unsubscribes == 1, "dispose detaches");
    check(!handler->on_open_url->data.has_value(), "and clears the signal");

    const auto other = flight::protocol::create_protocol_handler();
    flight::protocol::attach_protocol_handler(capability, other);
    host.open_listener(String("flight://ignored"));
    check(received == 0, "so a later open does not reach the disposed handler");
  }

  // Two live hosts, each serving the handler it was given.
  {
    FakeProtocolHost first_host;
    FakeProtocolHost second_host;
    const auto first_capability = first_host.open_capability();
    const auto second_capability = second_host.open_capability();
    const auto first_handler = flight::protocol::create_protocol_handler();
    const auto second_handler = flight::protocol::create_protocol_handler();
    std::vector<std::string> received;
    flight::signals::connect_signal<std::function<void(String)>>(
        first_handler->on_open_url, [&received](String) { received.push_back("first"); });
    flight::signals::connect_signal<std::function<void(String)>>(
        second_handler->on_open_url, [&received](String) { received.push_back("second"); });

    flight::protocol::attach_protocol_handler(first_capability, first_handler);
    flight::protocol::attach_protocol_handler(second_capability, second_handler);
    second_host.open_listener(String("flight://two"));
    first_host.open_listener(String("flight://one"));
    check(received.size() == 2 && received[0] == "second" && received[1] == "first",
          "each handler listens to the host it was attached to");
  }

  // createProtocolUrl: path normalization, component encoding, and the empty query key dropped.
  {
    const auto parts = flight::make_ref<Parsed>();
    parts->scheme = String("flight");
    parts->host = String("open");
    parts->path = String("file");
    parts->query.set(String("name"), String("a b"));
    parts->query.set(String(""), String("ignored"));
    check(flight::protocol::create_protocol_url(PartialParsedRow(parts)) ==
              String("flight://open/file?name=a%20b"),
          "the path is normalized and query components are encoded, with an empty key dropped");

    const auto authority_free = flight::make_ref<Parsed>();
    authority_free->scheme = String("flight");
    authority_free->path = String("/open");
    check(flight::protocol::create_protocol_url(PartialParsedRow(authority_free)) == String("flight:/open"),
          "a URL with no host is authority-free");

    // The absent part is what `?? ` defaults, and `authority_free` above is that case: its row carries no
    // host, so the empty default is what makes the URL authority-free. An object's members are present
    // even when their strings are empty, so an object cannot express absence; the upstream suite tests
    // an absent host exactly this way, through the partial row the builder is declared to take.
  }

  // parseProtocolUrl: the round trip, the decoded query, and the malformed cases.
  {
    const auto parsed = flight::protocol::parse_protocol_url(String("FLIGHT://open/file?name=a%20b&empty"));
    check(parsed.has_value(), "a well-formed URL parses");
    if (parsed.has_value()) {
      check(parsed.value()->scheme == String("flight"), "the scheme is lower-cased");
      check(parsed.value()->host == String("open"), "the host is separated");
      check(parsed.value()->path == String("/file"), "the path is separated");
      check(text(parsed.value()->query.get(String("name"))) == "a b",
            "and a percent-encoded query value is decoded");
      check(parsed.value()->query.has(String("empty")) &&
                text(parsed.value()->query.get(String("empty"))) == "",
            "a valueless query key is present with an empty value");
    }

    check(!flight::protocol::parse_protocol_url(String("not-a-url")).has_value(),
          "a URL with no scheme delimiter does not parse");
    check(!flight::protocol::parse_protocol_url(String("")).has_value(), "nor does an empty string");

    const auto malformed_escape = flight::protocol::parse_protocol_url(String("flight:/open?value=%ZZ"));
    check(malformed_escape.has_value() && text(malformed_escape.value()->query.get(String("value"))) == "%ZZ",
          "a malformed escape is preserved rather than dropped");

    const auto plus = flight::protocol::parse_protocol_url(String("flight:/open?q=a+b"));
    check(plus.has_value() && text(plus.value()->query.get(String("q"))) == "a b",
          "a plus in a query value decodes to a space");
  }

  // isValidProtocolScheme: grammar accepted, reserved and malformed rejected.
  {
    check(flight::protocol::is_valid_protocol_scheme(String("flight+desktop")),
          "custom grammar is accepted");
    check(!flight::protocol::is_valid_protocol_scheme(String("https")), "a reserved scheme is rejected");
    check(!flight::protocol::is_valid_protocol_scheme(String("1flight")),
          "a scheme that does not start with a letter is rejected");
    check(!flight::protocol::is_valid_protocol_scheme(String("")), "an empty scheme is rejected");
    check(!flight::protocol::is_valid_protocol_scheme(String("Flight!")), "an illegal character is rejected");
  }

  // Every delegating entry point validates before touching its provider.
  {
    struct Queried {
      std::vector<std::string> calls;
    } seen;
    auto default_host = flight::make_ref<flight::types::HostProtocolDefaultCapability>();
    default_host->is_default = [&seen](String scheme) {
      seen.calls.push_back("isDefault:" + scheme.to_utf8());
      return true;
    };
    default_host->set_as_default = [&seen](String scheme) {
      seen.calls.push_back("setAsDefault:" + scheme.to_utf8());
      return true;
    };
    auto query_host = flight::make_ref<flight::types::HostProtocolRegistrationQueryCapability>();
    query_host->is_registered = [&seen](String scheme) {
      seen.calls.push_back("isRegistered:" + scheme.to_utf8());
      return true;
    };
    auto registration_host = flight::make_ref<flight::types::HostProtocolRegistrationCapability>();
    registration_host->get_registered_schemes = []() {
      return flight::Array<String>{String("flight")};
    };
    registration_host->register_ = [&seen](String scheme) {
      seen.calls.push_back("register:" + scheme.to_utf8());
      return scheme != String("fail");
    };
    auto unregistration_host = flight::make_ref<flight::types::HostProtocolUnregistrationCapability>();
    unregistration_host->unregister = [&seen](String scheme) {
      seen.calls.push_back("unregister:" + scheme.to_utf8());
      return scheme != String("fail");
    };

    check(!flight::protocol::is_protocol_scheme_default(default_host, String("https")),
          "an invalid scheme is not a default");
    check(!flight::protocol::is_protocol_scheme_registered(query_host, String("https")),
          "an invalid scheme is not registered");
    check(!flight::protocol::register_protocol_scheme(registration_host, String("https")),
          "an invalid scheme is not registered through the host");
    check(!flight::protocol::unregister_protocol_scheme(unregistration_host, String("https")),
          "and an invalid scheme is not unregistered through the host");
    // The setter's OWN gate, which the mutation battery found unasserted: `setAsDefault` was only ever
    // called with a VALID scheme here, so removing its validity check went unnoticed. Its sibling
    // `removeAsDefault` had this case; `setAsDefault` did not.
    check(!flight::protocol::set_protocol_scheme_as_default(default_host, String("https")),
          "and an invalid scheme is refused by the setter's own gate");

    check(seen.calls.empty(), "no provider was consulted for any of them");

    check(flight::protocol::is_protocol_scheme_default(default_host, String("flight")),
          "a valid scheme reaches the provider");
    check(flight::protocol::register_protocol_scheme(registration_host, String("flight")),
          "and so does a valid registration");
    check(flight::protocol::set_protocol_scheme_as_default(default_host, String("flight")),
          "the default setters delegate too");

    check(flight::protocol::get_registered_protocol_schemes(registration_host).size() == 1.0,
          "getRegisteredProtocolSchemes returns the provider's own list");
  }

  // The batch operations prevalidate the whole list, then attempt EVERY entry and aggregate.
  {
    std::vector<std::string> calls;
    auto registration_host = flight::make_ref<flight::types::HostProtocolRegistrationCapability>();
    registration_host->get_registered_schemes = []() { return flight::Array<String>{}; };
    registration_host->register_ = [&calls](String scheme) {
      calls.push_back(scheme.to_utf8());
      return scheme != String("fail");
    };

    flight::Array<String> containing_invalid = flight::Array<String>{};
    containing_invalid.push(String("flight"));
    containing_invalid.push(String("https"));
    check(!flight::protocol::register_protocol_schemes(registration_host, containing_invalid),
          "a batch containing an invalid scheme fails");
    check(calls.empty(), "and no provider call happens at all");

    flight::Array<String> containing_failure = flight::Array<String>{};
    containing_failure.push(String("flight"));
    containing_failure.push(String("fail"));
    check(!flight::protocol::register_protocol_schemes(registration_host, containing_failure),
          "a batch whose provider refuses an entry fails");
    check(calls.size() == 2.0 && calls[0] == "flight" && calls[1] == "fail",
          "but every entry was attempted, in order, and aggregated");

    std::vector<std::string> unregister_calls;
    auto unregistration_host = flight::make_ref<flight::types::HostProtocolUnregistrationCapability>();
    unregistration_host->unregister = [&unregister_calls](String scheme) {
      unregister_calls.push_back(scheme.to_utf8());
      return scheme != String("fail");
    };
    flight::Array<String> unregistration_batch = flight::Array<String>{};
    unregistration_batch.push(String("flight"));
    unregistration_batch.push(String("fail"));
    check(!flight::protocol::unregister_protocol_schemes(unregistration_host, unregistration_batch),
          "the unregistration batch aggregates the same way");
    check(unregister_calls.size() == 2.0, "attempting every entry");
  }

  // --- Cases added by the mutation battery, which found four holes and two unexercised entry points ----
  //
  // A 18-mutation battery against the emitted module killed fourteen and SURVIVED four, each a real hole
  // rather than an equivalence, and two public functions were referenced NOWHERE in this file. The blocks
  // below close all of them; each survivor is named where its case sits.
  //
  // 1. isValidProtocolScheme folds the input before testing it, but every case above is either already lower
  //    case or illegal for a second reason (`Flight!` fails on the `!`), so dropping the fold was invisible.
  {
    check(flight::protocol::is_valid_protocol_scheme(String("MyApp")),
          "an UPPERCASE scheme is valid, so the grammar is tested case-insensitively");
    check(flight::protocol::is_valid_protocol_scheme(String("FLIGHT+DESKTOP")),
          "and so is a mixed-case one with grammar punctuation");
    check(!flight::protocol::is_valid_protocol_scheme(String("HTTPS")),
          "while a reserved scheme is still rejected when it arrives in capitals");
  }

  // 2. parseProtocolUrl checks the scheme's SHAPE as well as its delimiter, which no case above exercised:
  //    `not-a-url` has no colon at all, so it never reaches the shape test.
  {
    check(!flight::protocol::parse_protocol_url(String("1bad:payload")).has_value(),
          "a colon-delimited URL whose scheme starts with a digit does not parse");
    check(!flight::protocol::parse_protocol_url(String("bad scheme:payload")).has_value(),
          "nor does one whose scheme carries a space");
  }

  // 3. A URL with NO authority: the host-extraction branch is taken only when the rest starts with `//`,
  //    and the cases above that lack one never assert the host or the path, so taking it unconditionally
  //    was invisible.
  {
    const auto authority_free = flight::protocol::parse_protocol_url(String("flight:/open"));
    check(authority_free.has_value(), "an authority-free URL parses");
    if (authority_free.has_value()) {
      check(authority_free.value()->host == String(""), "and has an EMPTY host rather than one read from its path");
      check(authority_free.value()->path == String("/open"), "with its whole path preserved");
    }
  }

  // 4. createProtocolUrl defaults an absent scheme to `unknown`, which the cases above never reach because
  //    each one sets a scheme.
  {
    // The cases above wrap a FULL object in the partial-row type, and an object's members are always present --
    // the comment above says so -- so the `??` defaults were never reached. A genuine PARTIAL row is one built
    // from row_fields, where a slot that is not named is ABSENT rather than empty.
    using PartialSchema = flight::RowReadonly<flight::RowPartial<flight::RowOf<flight::Ref<Parsed>>>>;
    const auto only_path = flight::make_structural_ref<PartialSchema>(
        flight::row_field<flight::RowKey<"path">>(String("open")));
    check(flight::protocol::create_protocol_url(only_path) == String("unknown:/open"),
          "a PARTIAL row with no scheme builds as `unknown`, with the path still given a leading slash");

    const auto only_scheme = flight::make_structural_ref<PartialSchema>(
        flight::row_field<flight::RowKey<"scheme">>(String("flight")));
    check(flight::protocol::create_protocol_url(only_scheme) == String("flight:"),
          "and a partial row with no host is authority-free, which an object with an empty host cannot express");
  }

  // 5. Two public entry points this file never called at all: the launch-url read and the default REMOVAL.
  {
    auto launch = flight::make_ref<flight::types::HostProtocolLaunchCapability>();
    launch->get_launch_url = []() { return std::optional<String>{String("flight://open")}; };
    check(text(flight::protocol::get_protocol_launch_url(launch)) == "flight://open",
          "getProtocolLaunchUrl returns the provider's launch URL");
    const auto absent_launch = flight::make_ref<flight::types::HostProtocolLaunchCapability>();
    absent_launch->get_launch_url = []() { return std::optional<String>{}; };
    check(!flight::protocol::get_protocol_launch_url(absent_launch).has_value(),
          "and reports absence when the provider has none");

    int removals = 0;
    auto defaults = flight::make_ref<flight::types::HostProtocolDefaultCapability>();
    defaults->remove_as_default = [&removals](String) {
      ++removals;
      return true;
    };
    check(flight::protocol::remove_protocol_scheme_as_default(defaults, String("flight")),
          "removeProtocolSchemeAsDefault reports the provider's answer");
    check(removals == 1, "having routed exactly once");
    check(!flight::protocol::remove_protocol_scheme_as_default(defaults, String("https")),
          "a reserved scheme is refused by the validity gate");
    check(removals == 1, "and the provider is never reached for it");
  }

  // 6. initializeProtocolHandler is the constructor's initializer, and was never called directly.
  {
    flight::types::EntityConstruction<flight::Ref<Handler>> out =
        flight::entity::allocate_entity<flight::Ref<Handler>>();
    flight::protocol::initialize_protocol_handler(out);
    const auto handler = flight::entity::finish_entity<flight::Ref<Handler>>(out);
    check(handler != nullptr && handler->on_open_url != nullptr,
          "initializeProtocolHandler is what gives a fresh entity its open-url signal");
  }

  if (failures == 0) std::cout << "generated protocol: all checks passed\n";
  return failures == 0 ? 0 : 1;
}
