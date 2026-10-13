// Focused behavior tests for @flighthq/ipc.
//
// The package emits whole at a627849 -- 3/3 modules, no refusals, no override -- so this is the
// emission's own behavior under test. Every case is ported from the module's upstream suite
// (packages/ipc/src/ipc.test.ts), which is the specification, and the starred ones are the cases that
// suite marks as the reason the module exists:
//
//   * onceIpcMessage delivers the first message, releases the subscription, and is idempotent; its
//     returned stop works before any message arrives; and a provider that delivers SYNCHRONOUSLY during
//     subscribe -- which returns the unsubscribe only afterwards -- still ends up released;
//   * onIpcMessage delivers every message on its channel, only its own channel, releases exactly its own
//     subscription on a shared channel, and routes to the host it was given when two hosts are live.
//
// The host capabilities are plain operation-tight functions, so each case below supplies its own fake
// provider and nothing is shared between cases.
#include <flight/executor.hpp>
#include <flight/ipc/ipc.hpp>

#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <utility>
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
using MessageCapability = flight::types::HostIpcMessageCapability;
using Args = flight::Array<flight::Any>;

// A minimal message host: one listener list per channel, so the test can deliver on a channel and count
// what is still subscribed. The unsubscribe the capability returns removes exactly the listener it was
// created for.
struct FakeMessageHost {
  struct Listener {
    int id;
    std::string channel;
    std::function<void(Args)> deliver;
  };

  std::vector<Listener> listeners;
  int next_id = 1;

  flight::Ref<MessageCapability> capability() {
    auto host = flight::make_ref<MessageCapability>();
    host->subscribe = [this](String channel, std::function<void(Args)> listener) {
      // Identified by an id, not by an address: a vector reallocates on growth, so a pointer taken at
      // subscription time would dangle as soon as another listener arrives.
      const int id = next_id++;
      listeners.push_back(Listener{id, channel.to_utf8(), std::move(listener)});
      return std::function<void()>([this, id]() {
        for (auto it = listeners.begin(); it != listeners.end(); ++it) {
          if (it->id == id) {
            listeners.erase(it);
            return;
          }
        }
      });
    };
    return host;
  }

  void deliver(const char* channel, std::initializer_list<const char*> arguments = {}) {
    Args args = Args{};
    for (const char* argument : arguments) args.push(flight::Any(String(argument)));
    for (const auto& entry : listeners) {
      if (entry.channel == channel) entry.deliver(args);
    }
  }

  double subscriber_count(const char* channel) const {
    double count = 0.0;
    for (const auto& entry : listeners) {
      if (entry.channel == channel) ++count;
    }
    return count;
  }
};

std::string first_argument(const Args& args) {
  return args.size() > 0.0 ? args.element(0.0).as_string().to_utf8() : std::string("<none>");
}

}  // namespace

int main() {
  const auto executor = std::make_shared<flight::QueueExecutor>();
  const flight::ExecutorScope scope(executor);

  // invokeIpc forwards the channel and the argument list and returns the provider's own response.
  {
    std::string seen_channel;
    std::string seen_argument;
    auto host = flight::make_ref<flight::types::HostIpcInvokeCapability>();
    host->invoke = [&](String channel, Args args) {
      seen_channel = channel.to_utf8();
      seen_argument = first_argument(args);
      return flight::Task<flight::Any>::resolve(flight::Any(String("pong")));
    };

    Args args = Args{};
    args.push(flight::Any(String("payload")));
    const auto response = flight::ipc::invoke_ipc(host, String("ping"), args).get();
    check(seen_channel == "ping", "invokeIpc forwards the channel");
    check(seen_argument == "payload", "and the arguments");
    check(response.as_string() == String("pong"), "and returns the provider's response");
  }

  // onceIpcMessage: the first message, then the subscription is gone.
  {
    FakeMessageHost host;
    const auto capability = host.capability();
    std::vector<std::string> seen;
    static_cast<void>(flight::ipc::once_ipc_message(capability, String("ping"),
                                                    [&seen](Args args) { seen.push_back(first_argument(args)); }));
    check(host.subscriber_count("ping") == 1.0, "the subscription is registered");

    host.deliver("ping", {"1"});
    host.deliver("ping", {"2"});
    check(seen.size() == 1 && seen[0] == "1", "only the first message is delivered");
    check(host.subscriber_count("ping") == 0.0, "and the subscription is released by that delivery");
  }

  // onceIpcMessage: the returned stop works before any message arrives.
  {
    FakeMessageHost host;
    const auto capability = host.capability();
    int calls = 0;
    const auto stop = flight::ipc::once_ipc_message(capability, String("ping"), [&calls](Args) { ++calls; });
    stop();
    check(host.subscriber_count("ping") == 0.0, "the returned stop releases the subscription");
    host.deliver("ping", {"1"});
    check(calls == 0, "and a message after it is not delivered");
  }

  // onceIpcMessage: repeated stop calls release exactly once.
  {
    int releases = 0;
    auto host = flight::make_ref<MessageCapability>();
    host->subscribe = [&releases](String, std::function<void(Args)>) {
      return std::function<void()>([&releases]() { ++releases; });
    };
    const auto stop = flight::ipc::once_ipc_message(host, String("ping"), [](Args) {});
    stop();
    stop();
    check(releases == 1, "stop is idempotent: the subscription is released exactly once");
  }

  // ★ The acquisition race: a provider that delivers DURING subscribe returns its unsubscribe only
  // afterwards, so the release that just ran had nothing to release. The guard after subscribe must
  // release it anyway, or the subscription leaks.
  {
    bool released = false;
    auto host = flight::make_ref<MessageCapability>();
    host->subscribe = [&released](String, std::function<void(Args)> listener) {
      Args args = Args{};
      args.push(flight::Any(String("42")));
      listener(args);
      return std::function<void()>([&released]() { released = true; });
    };

    std::vector<std::string> seen;
    static_cast<void>(flight::ipc::once_ipc_message(host, String("ping"),
                                                    [&seen](Args args) { seen.push_back(first_argument(args)); }));
    check(seen.size() == 1 && seen[0] == "42", "a synchronously delivered message still reaches the listener");
    check(released, "and the subscription is released even though it arrived after the delivery");
  }

  // onIpcInvoke registers a handler for this channel and returns this registration's release.
  {
    std::string seen_channel;
    std::function<std::variant<flight::Task<flight::Any>, flight::Any>(Args)> registered;
    auto host = flight::make_ref<flight::types::HostIpcHandleCapability>();
    host->handle = [&](String channel,
                       std::function<std::variant<flight::Task<flight::Any>, flight::Any>(Args)> handler) {
      seen_channel = channel.to_utf8();
      registered = std::move(handler);
      return std::function<void()>([&registered]() { registered = nullptr; });
    };

    const auto stop = flight::ipc::on_ipc_invoke(host, String("double"), [](Args args) -> decltype(auto) {
      return std::variant<flight::Task<flight::Any>, flight::Any>{
          std::in_place_index<1>, flight::Any(String("doubled:" ) + first_argument(args))};
    });
    check(seen_channel == "double", "onIpcInvoke registers on the channel it was given");

    Args args = Args{};
    args.push(flight::Any(String("4")));
    const auto outcome = registered(args);
    check(outcome.index() == 1 && std::get<1>(outcome).as_string() == String("doubled:4"),
          "the handler receives the arguments and its answer is returned");
    stop();
    check(!registered, "and the returned release removes this registration");
  }

  // onIpcMessage: every message on its channel, with the sent arguments.
  {
    FakeMessageHost host;
    const auto capability = host.capability();
    std::vector<std::string> seen;
    static_cast<void>(flight::ipc::on_ipc_message(capability, String("ping"),
                                                  [&seen](Args args) { seen.push_back(first_argument(args)); }));
    host.deliver("ping", {"1"});
    host.deliver("ping", {"2"});
    check(seen.size() == 2 && seen[0] == "1" && seen[1] == "2",
          "every message on the channel is delivered, in order");
  }

  // onIpcMessage: only its own channel.
  {
    FakeMessageHost host;
    const auto capability = host.capability();
    int calls = 0;
    static_cast<void>(flight::ipc::on_ipc_message(capability, String("wanted"), [&calls](Args) { ++calls; }));
    host.deliver("other", {"1"});
    check(calls == 0, "another channel's message is not delivered");
  }

  // ★ Origin-pinned: releasing one listener must not disturb another on the same channel.
  {
    FakeMessageHost host;
    const auto capability = host.capability();
    std::vector<std::string> seen;
    const auto stop_dropped = flight::ipc::on_ipc_message(
        capability, String("shared"), [&seen](Args) { seen.push_back("dropped"); });
    static_cast<void>(flight::ipc::on_ipc_message(capability, String("shared"),
                                                  [&seen](Args) { seen.push_back("kept"); }));
    stop_dropped();
    host.deliver("shared", {"1"});
    check(seen.size() == 1 && seen[0] == "kept", "the other listener on the same channel still receives");
    check(host.subscriber_count("shared") == 1.0, "and exactly one subscription remains");
  }

  // ★ Two hosts live at once, each serving its own subscription.
  {
    FakeMessageHost first_host;
    FakeMessageHost second_host;
    const auto first = first_host.capability();
    const auto second = second_host.capability();
    std::vector<std::string> seen;
    static_cast<void>(flight::ipc::on_ipc_message(first, String("c"), [&seen](Args) { seen.push_back("first"); }));
    static_cast<void>(flight::ipc::on_ipc_message(second, String("c"), [&seen](Args) { seen.push_back("second"); }));

    second_host.deliver("c", {"1"});
    first_host.deliver("c", {"1"});
    check(seen.size() == 2 && seen[0] == "second" && seen[1] == "first",
          "each subscription is routed to the host it was given");
  }

  // sendIpcMessage and sendIpcMessageTo forward their channel, target and arguments unchanged.
  {
    std::string seen_channel;
    std::string seen_argument;
    auto send_host = flight::make_ref<flight::types::HostIpcSendCapability>();
    send_host->send = [&](String channel, Args args) {
      seen_channel = channel.to_utf8();
      seen_argument = first_argument(args);
    };
    Args args = Args{};
    args.push(flight::Any(String("hello")));
    flight::ipc::send_ipc_message(send_host, String("notify"), args);
    check(seen_channel == "notify" && seen_argument == "hello",
          "sendIpcMessage forwards the channel and the arguments");

    std::string seen_target;
    auto targeted_host = flight::make_ref<flight::types::HostIpcTargetedSendCapability<String>>();
    targeted_host->send = [&](String target, String channel, Args received) {
      seen_target = target.to_utf8();
      seen_channel = channel.to_utf8();
      seen_argument = first_argument(received);
    };
    flight::ipc::send_ipc_message_to<String>(targeted_host, String("window-7"), String("notify"), args);
    check(seen_target == "window-7", "sendIpcMessageTo forwards the target unchanged");
    check(seen_channel == "notify" && seen_argument == "hello", "with its channel and arguments");
  }

  if (failures == 0) std::cout << "generated ipc: all checks passed\n";
  // A host that KEEPS delivering to a listener after its unsubscribe has run. A compliant host does
  // not, which is exactly why nothing above can tell the difference: a mutation removing the listener's
  // `if (done) return` guard SURVIVES every other assertion in this file, because the fake host above
  // drops the listener on release and the second delivery never reaches it. The upstream source guards
  // that case explicitly, so it is pinned here rather than left to a host's good manners.
  {
    std::function<void(Args)> captured;
    int unsubscribes = 0;
    const auto rogue = flight::make_ref<MessageCapability>();
    rogue->subscribe = [&captured, &unsubscribes](String, std::function<void(Args)> listener) {
      captured = listener;
      return [&unsubscribes]() { ++unsubscribes; };
    };

    std::vector<std::string> seen;
    const auto stop = flight::ipc::once_ipc_message(rogue, String("ping"), [&seen](Args args) {
      seen.push_back(first_argument(args));
    });

    captured({"1"});
    check(seen.size() == 1 && seen[0] == "1", "the first message reaches the listener");
    check(unsubscribes == 1, "and that delivery released the subscription");

    // The host still holds the listener it was given and calls it anyway.
    captured({"2"});
    check(seen.size() == 1,
          "a delivery to an ALREADY-RELEASED subscription is not passed to the listener");
    captured({"3"});
    check(seen.size() == 1, "and neither is a third");
    static_cast<void>(stop);
  }

  return failures == 0 ? 0 : 1;
}
