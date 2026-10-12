#include <flight/signals/slot.hpp>
#include <flight/socket/contract.hpp>

#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

// Native port of packages/socket/src/{socket,socketHost,explainSocketSendFailure,enableSocketGuards}.test.ts
// (a627849), through the socket-complete-lifecycle and socket-guard-reporting overrides and the generated
// explain_socket_send_failure. Signals are observed through connect_signal, as upstream does.
//
// Not ported, each for a stated reason:
// - enableSocketGuards' three warning cases: a guard warning goes through log_once -> log ->
//   merge_span_fields, a NotImplementedInThisProfile stub in the log override, so with a log sink
//   registered it throws instead of logging (see that override's `remaining`).
// - socketHost 'yields no connection when the host carries no socket capability' and openTcpSocket
//   'returns null when the host carries no socket backend': both pass an undefined capability, which
//   the C++ signatures (a non-optional ReadonlyHostSocket) cannot express.
// - the Object.freeze part of 'forwards binary data by identity ... without mutating the readonly socket':
//   there is no freeze; the send goes through the ReadonlySocket view, which is the native readonly form.

namespace {

using flight::String;
using namespace flight::types;
namespace socket_ns = flight::socket;
using Frame = std::variant<flight::ArrayBuffer, String>;

int failures = 0;

void check(bool condition, const char* message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

// The upstream fakeBackend(): records the event sink openSocket was handed, so a test can drive
// open/message/close/error, and captures every send and close.
struct FakeSocket {
  std::function<void()> open;
  std::function<void(socket_ns::ReadonlySocketMessage)> message;
  std::function<void(socket_ns::ReadonlySocketCloseInfo)> close;
  std::function<void()> error;
  std::vector<Frame> sent;
  std::vector<std::pair<std::optional<double>, std::optional<String>>> closes;
  bool open_returns_null = false;
  bool send_returns = true;
  std::optional<String> last_url;
  std::optional<flight::Array<String>> last_protocols;
  std::optional<String> last_binary_type;
  int open_calls = 0;
  flight::Ref<HostSocketCapability> backend;
};

std::shared_ptr<FakeSocket> fake_backend() {
  auto state = std::make_shared<FakeSocket>();
  std::weak_ptr<FakeSocket> weak = state;
  state->backend = flight::make_ref<HostSocketCapability>(HostSocketCapability{
      .open_socket = [weak](auto options, auto events) -> std::optional<flight::Ref<SocketConnection>> {
        auto fake = weak.lock();
        ++fake->open_calls;
        fake->open = events->handle_socket_open;
        fake->message = events->handle_socket_message;
        fake->close = events->handle_socket_close;
        fake->error = events->handle_socket_error;
        fake->last_url = options.shared_object()->url;
        fake->last_protocols = options.shared_object()->protocols;
        fake->last_binary_type = options.shared_object()->binary_type;
        if (fake->open_returns_null) return std::nullopt;
        return flight::make_ref<SocketConnection>(SocketConnection{
            .send_socket_frame = [weak](Frame data) {
              auto inner = weak.lock();
              inner->sent.push_back(data);
              return inner->send_returns;
            },
            .close_socket_connection = [weak](std::optional<double> code, std::optional<String> reason) {
              weak.lock()->closes.emplace_back(code, std::move(reason));
            },
        });
      },
      .open_tcp_socket = std::nullopt,
  });
  return state;
}

flight::Ref<Socket> create(const std::shared_ptr<FakeSocket>& fake, const char* url,
                           std::optional<flight::Array<String>> protocols = std::nullopt,
                           std::optional<String> binary_type = std::nullopt) {
  return socket_ns::create_socket(
      socket_ns::ReadonlyHostSocket(fake->backend),
      socket_ns::ReadonlySocketOptions(flight::make_ref<SocketOptions>(SocketOptions{.url = String(url), .protocols = std::move(protocols), .binary_type = std::move(binary_type)})));
}

String ready_state(const flight::Ref<Socket>& socket) { return socket_ns::get_socket_ready_state(socket_ns::ReadonlySocket(socket)); }

bool send(const flight::Ref<Socket>& socket, Frame data) { return socket_ns::send_socket_message(socket_ns::ReadonlySocket(socket), std::move(data)); }

flight::Ref<SocketCloseInfo> close_info(double code, const char* reason, bool was_clean) {
  return flight::make_ref<SocketCloseInfo>(SocketCloseInfo{.code = code, .reason = String(reason), .was_clean = was_clean});
}

flight::Ref<SocketMessage> message(Frame data, bool binary) { return flight::make_ref<SocketMessage>(SocketMessage{.data = std::move(data), .binary = binary}); }

std::function<void()> counter(int& count) {
  return [&count]() { ++count; };
}

void test_attach_detach() {
  {
    auto fake = fake_backend();
    auto socket = create(fake, "ws://x");
    auto signals = socket_ns::enable_socket_signals(socket);
    int opens = 0;
    flight::signals::connect_signal(signals->on_socket_open, counter(opens));
    socket_ns::detach_socket(socket);
    socket_ns::attach_socket(socket);
    fake->open();
    check(opens == 1, "attachSocket resumes delivery after a detach");
  }
  {
    auto fake = fake_backend();
    auto socket = create(fake, "ws://x");
    auto signals = socket_ns::enable_socket_signals(socket);
    int opens = 0;
    flight::signals::connect_signal(signals->on_socket_open, counter(opens));
    socket_ns::dispose_socket(socket);
    socket_ns::attach_socket(socket);
    fake->open();
    check(opens == 0 && !socket->runtime->delivering, "attachSocket cannot resume delivery after terminal disposal");
  }
  {
    auto fake = fake_backend();
    auto socket = create(fake, "ws://x");
    auto signals = socket_ns::enable_socket_signals(socket);
    int opens = 0;
    flight::signals::connect_signal(signals->on_socket_open, counter(opens));
    socket_ns::detach_socket(socket);
    fake->open();
    check(opens == 0, "detachSocket stops backend events from reaching the signals");
  }
}

void test_close() {
  {
    auto fake = fake_backend();
    auto socket = create(fake, "ws://x");
    fake->open();
    socket_ns::close_socket(socket, 1000.0, String("bye"));
    check(ready_state(socket) == String("closing") && fake->closes.size() == 1 && fake->closes[0].first == 1000.0 &&
              fake->closes[0].second == String("bye"),
          "closeSocket transitions to closing and forwards code/reason to the connection");
  }
  {
    auto fake = fake_backend();
    auto socket = create(fake, "ws://x");
    fake->open();
    socket_ns::close_socket(socket);
    fake->close(socket_ns::ReadonlySocketCloseInfo(close_info(1000.0, "", true)));
    check(ready_state(socket) == String("closed"), "closeSocket reaches closed once the backend close event arrives");
    socket_ns::close_socket(socket);
    check(fake->closes.size() == 1, "closeSocket is a no-op when already closed");
  }
  {
    auto fake = fake_backend();
    auto socket = create(fake, "ws://x");
    socket_ns::dispose_socket(socket);
    socket_ns::close_socket(socket);
    check(ready_state(socket) == String("closed") && fake->closes.size() == 1,
          "closeSocket stays closed and does not close the connection again after disposal");
  }
}

void test_create() {
  {
    auto fake = fake_backend();
    auto socket = create(fake, "ws://host/path");
    check(socket->url == String("ws://host/path") && ready_state(socket) == String("connecting") && fake->last_url == String("ws://host/path"),
          "createSocket opens through the backend in the connecting state and records the url");
  }
  {
    auto fake = fake_backend();
    static_cast<void>(create(fake, "ws://x", flight::Array<String>{String("a"), String("b")}, String("arraybuffer")));
    check(fake->last_protocols.has_value() && fake->last_protocols->size() == 2 && (*fake->last_protocols)[0] == String("a") &&
              (*fake->last_protocols)[1] == String("b") && fake->last_binary_type == String("arraybuffer"),
          "createSocket passes protocols and binaryType through to the backend");
  }
  {
    auto fake = fake_backend();
    fake->open_returns_null = true;
    auto socket = create(fake, "tcp://x");
    check(ready_state(socket) == String("connecting") && !send(socket, String("x")), "createSocket tolerates a null connection from an unsupported transport");
  }
  {
    auto fake = fake_backend();
    auto socket = create(fake, "ws://x");
    auto signals = socket_ns::enable_socket_signals(socket);
    std::vector<std::pair<Frame, bool>> received;
    flight::signals::connect_signal(signals->on_socket_message, std::function<void(socket_ns::ReadonlySocketMessage)>(
                                                                    [&](socket_ns::ReadonlySocketMessage m) {
                                                                      received.emplace_back(m.shared_object()->data,
                                                                                            m.shared_object()->binary);
                                                                    }));
    fake->open();
    fake->message(socket_ns::ReadonlySocketMessage(message(String("hi"), false)));
    flight::ArrayBuffer buffer(2);
    fake->message(socket_ns::ReadonlySocketMessage(message(buffer, true)));
    check(received.size() == 2 && std::get<String>(received[0].first) == String("hi") && !received[0].second &&
              std::get<flight::ArrayBuffer>(received[1].first) == buffer && received[1].second,
          "createSocket emits a text message with binary false and a binary message with binary true");
  }
  {
    auto fake = fake_backend();
    auto socket = create(fake, "ws://x");
    auto signals = socket_ns::enable_socket_signals(socket);
    std::vector<std::tuple<double, std::string, bool>> infos;
    flight::signals::connect_signal(signals->on_socket_close, std::function<void(socket_ns::ReadonlySocketCloseInfo)>(
                                                                  [&](socket_ns::ReadonlySocketCloseInfo info) {
                                                                    infos.emplace_back(info.shared_object()->code,
                                                                                       info.shared_object()->reason.to_utf8(),
                                                                                       info.shared_object()->was_clean);
                                                                  }));
    fake->close(socket_ns::ReadonlySocketCloseInfo(close_info(1006.0, "gone", false)));
    check(infos.size() == 1 && std::get<0>(infos[0]) == 1006.0 && std::get<1>(infos[0]) == "gone" && !std::get<2>(infos[0]),
          "createSocket emits close info with code, reason, and wasClean");
  }
  {
    auto fake = fake_backend();
    auto socket = create(fake, "ws://x");
    auto signals = socket_ns::enable_socket_signals(socket);
    int errors = 0;
    flight::signals::connect_signal(signals->on_socket_error, counter(errors));
    fake->error();
    check(errors == 1, "createSocket emits onSocketError");
  }
}

void test_dispose_and_signals() {
  {
    auto fake = fake_backend();
    auto socket = create(fake, "ws://x");
    auto signals = socket_ns::enable_socket_signals(socket);
    int messages = 0;
    flight::signals::connect_signal(signals->on_socket_message,
                                    std::function<void(socket_ns::ReadonlySocketMessage)>([&](socket_ns::ReadonlySocketMessage) { ++messages; }));
    fake->open();
    socket_ns::dispose_socket(socket);
    const bool closed_once = fake->closes.size() == 1;
    fake->message(socket_ns::ReadonlySocketMessage(message(String("x"), false)));
    check(closed_once && !socket->runtime->connection.has_value() && ready_state(socket) == String("closed") && messages == 0,
          "disposeSocket closes an open connection and detaches so later events fire no signal");
  }
  {
    auto fake = fake_backend();
    auto socket = create(fake, "ws://x");
    bool threw = false;
    try {
      socket_ns::dispose_socket(socket);
    } catch (...) {
      threw = true;
    }
    check(!threw, "disposeSocket is safe to call on a fresh socket");
  }
  {
    auto fake = fake_backend();
    auto socket = create(fake, "ws://x");
    check(!socket->runtime->signals.has_value(), "enableSocketSignals leaves a bare socket without signals");
    check(socket_ns::enable_socket_signals(socket) == socket_ns::enable_socket_signals(socket), "enableSocketSignals returns the same group on repeated calls");
  }
  {
    auto fake = fake_backend();
    auto socket = create(fake, "ws://x");
    socket_ns::dispose_socket(socket);
    const auto first = socket_ns::enable_socket_signals(socket);
    check(socket_ns::enable_socket_signals(socket) == first && !socket->runtime->delivering && ready_state(socket) == String("closed"),
          "enableSocketSignals returns an inert stable group without reviving a disposed socket");
  }
  {
    auto fake = fake_backend();
    auto socket = create(fake, "ws://x");
    const bool connecting = ready_state(socket) == String("connecting");
    fake->open();
    const bool open = ready_state(socket) == String("open");
    socket_ns::close_socket(socket);
    const bool closing = ready_state(socket) == String("closing");
    fake->close(socket_ns::ReadonlySocketCloseInfo(close_info(1000.0, "", true)));
    check(connecting && open && closing && ready_state(socket) == String("closed"),
          "getSocketReadyState reflects connecting -> open -> closing -> closed transitions");
  }
}

void test_open_tcp_socket() {
  const auto expected = flight::make_ref<TcpSocketConnection>(TcpSocketConnection{.readable = {}, .writable = {}, .close_tcp_socket_connection = []() {}});
  const auto options = flight::make_ref<TcpSocketOptions>(TcpSocketOptions{.host = String("db.internal"), .port = 5432.0});
  std::optional<socket_ns::ReadonlyTcpSocketOptions> received;
  int framed_calls = 0;
  const auto host = flight::make_ref<HostSocketCapability>(HostSocketCapability{
      .open_socket = [&](auto, auto) -> std::optional<flight::Ref<SocketConnection>> {
        ++framed_calls;
        return std::nullopt;
      },
      .open_tcp_socket = [&](socket_ns::ReadonlyTcpSocketOptions forwarded) -> std::optional<flight::Ref<TcpSocketConnection>> {
        received = forwarded;
        return expected;
      },
  });
  const auto opened = socket_ns::open_tcp_socket(socket_ns::ReadonlyHostSocket(host), socket_ns::ReadonlyTcpSocketOptions(options));
  check(opened.has_value() && opened.value() == expected && received.has_value() && received.value() == socket_ns::ReadonlyTcpSocketOptions(options),
        "openTcpSocket forwards the endpoint unchanged and returns the backend connection by identity");

  const auto framed_only = flight::make_ref<HostSocketCapability>(HostSocketCapability{
      .open_socket = [&](auto, auto) -> std::optional<flight::Ref<SocketConnection>> {
        ++framed_calls;
        return std::nullopt;
      },
      .open_tcp_socket = std::nullopt,
  });
  const auto local = flight::make_ref<TcpSocketOptions>(TcpSocketOptions{.host = String("localhost"), .port = 9000.0});
  check(!socket_ns::open_tcp_socket(socket_ns::ReadonlyHostSocket(framed_only), socket_ns::ReadonlyTcpSocketOptions(local)).has_value() && framed_calls == 0,
        "openTcpSocket returns null for a framed-only backend without attempting framed openSocket");

  const auto refusing = flight::make_ref<HostSocketCapability>(HostSocketCapability{
      .open_socket = [](auto, auto) -> std::optional<flight::Ref<SocketConnection>> { return std::nullopt; },
      .open_tcp_socket = [](socket_ns::ReadonlyTcpSocketOptions) -> std::optional<flight::Ref<TcpSocketConnection>> { return std::nullopt; },
  });
  const auto unreachable = flight::make_ref<TcpSocketOptions>(TcpSocketOptions{.host = String("unreachable.internal"), .port = 22.0});
  check(!socket_ns::open_tcp_socket(socket_ns::ReadonlyHostSocket(refusing), socket_ns::ReadonlyTcpSocketOptions(unreachable)).has_value(),
        "openTcpSocket preserves an explicit null from a backend that cannot open the endpoint");
}

void test_send() {
  {
    auto fake = fake_backend();
    auto socket = create(fake, "ws://x");
    fake->open();
    check(send(socket, String("ping")) && fake->sent.size() == 1 && std::get<String>(fake->sent[0]) == String("ping"),
          "sendSocketMessage sends through the connection and returns true when open");
  }
  {
    auto fake = fake_backend();
    auto socket = create(fake, "ws://x");
    check(!send(socket, String("ping")) && fake->sent.empty(), "sendSocketMessage returns false without throwing when not open");
  }
  {
    auto fake = fake_backend();
    auto socket = create(fake, "ws://x");
    fake->open();
    socket_ns::dispose_socket(socket);
    check(!send(socket, String("ping")) && fake->sent.empty(), "sendSocketMessage returns false after disposal without reaching the released connection");
  }
  {
    auto fake = fake_backend();
    fake->open_returns_null = true;
    auto socket = create(fake, "tcp://x");
    fake->open();
    check(ready_state(socket) == String("open") && !send(socket, String("ping")),
          "sendSocketMessage returns false for an open state whose backend supplied no connection");
  }
  {
    auto fake = fake_backend();
    fake->send_returns = false;
    auto socket = create(fake, "ws://x");
    fake->open();
    flight::ArrayBuffer frame(3);
    check(!send(socket, frame) && fake->sent.size() == 1 && std::get<flight::ArrayBuffer>(fake->sent[0]) == frame,
          "sendSocketMessage forwards binary data by identity and propagates false");
  }
}

void test_guard() {
  std::vector<std::string> notices;
  socket_ns::set_socket_guard(SocketGuard{[&](SocketGuardNotice notice) {
    notices.push_back(notice->operation.to_utf8() + ":" + notice->reason.to_utf8());
  }});
  auto unsupported = fake_backend();
  unsupported->open_returns_null = true;
  static_cast<void>(create(unsupported, "tcp://x"));
  auto supported = fake_backend();
  auto socket = create(supported, "ws://x");
  socket_ns::dispose_socket(socket);
  socket_ns::close_socket(socket);
  static_cast<void>(send(socket, String("x")));
  static_cast<void>(socket_ns::enable_socket_signals(socket));
  socket_ns::set_socket_guard(std::nullopt);
  socket_ns::close_socket(socket);
  check(notices == std::vector<std::string>({"createSocket:no-connection", "closeSocket:disposed", "sendSocketMessage:disposed", "enableSocketSignals:disposed"}),
        "setSocketGuard installs and removes the core notice hook");
}

void test_hosts() {
  std::vector<std::string> opened;
  const auto recording = [&](const char* label) {
    return flight::make_ref<HostSocketCapability>(HostSocketCapability{
        .open_socket = [&opened, label](auto, auto events) -> std::optional<flight::Ref<SocketConnection>> {
          opened.emplace_back(label);
          return flight::make_ref<SocketConnection>(SocketConnection{
              .send_socket_frame = [events](Frame) {
                events->handle_socket_open();
                return true;
              },
              .close_socket_connection = [](std::optional<double>, std::optional<String>) {},
          });
        },
        .open_tcp_socket = std::nullopt,
    });
  };
  const auto options = [](const char* url) {
    return socket_ns::ReadonlySocketOptions(flight::make_ref<SocketOptions>(SocketOptions{.url = String(url), .protocols = std::nullopt, .binary_type = std::nullopt}));
  };
  static_cast<void>(socket_ns::create_socket(socket_ns::ReadonlyHostSocket(recording("a")), options("wss://example.test")));
  check(opened == std::vector<std::string>({"a"}), "createSocket opens through the capability carried by the host it is given");
  opened.clear();
  static_cast<void>(socket_ns::create_socket(socket_ns::ReadonlyHostSocket(recording("first")), options("wss://one.test")));
  static_cast<void>(socket_ns::create_socket(socket_ns::ReadonlyHostSocket(recording("second")), options("wss://two.test")));
  check(opened == std::vector<std::string>({"first", "second"}), "createSocket keeps two hosts independent");
  opened.clear();
  const auto unused = recording("unused");
  static_cast<void>(socket_ns::create_socket(socket_ns::ReadonlyHostSocket(recording("used")), options("wss://x.test")));
  check(opened == std::vector<std::string>({"used"}) && unused != nullptr, "createSocket does not consult one host capability when another host is passed");
}

void test_explain() {
  const auto reason_of = [](const std::optional<SocketSendFailureExplanation>& value) {
    return value.has_value() ? std::visit([](const auto& arm) { return arm->reason.to_utf8() + "|" + arm->ready_state.to_utf8() + "|" + arm->url.to_utf8(); }, *value)
                             : std::string("null");
  };
  {
    auto fake = fake_backend();
    fake->open_returns_null = true;
    auto socket = create(fake, "tcp://host");
    check(reason_of(socket_ns::explain_socket_send_failure(socket_ns::ReadonlySocket(socket))) == "no-connection|connecting|tcp://host",
          "explainSocketSendFailure identifies an unsupported backend with no connection");
  }
  {
    auto fake = fake_backend();
    auto socket = create(fake, "ws://host");
    check(reason_of(socket_ns::explain_socket_send_failure(socket_ns::ReadonlySocket(socket))) == "not-open|connecting|ws://host",
          "explainSocketSendFailure reports the current non-open phase when a connection exists");
  }
  {
    auto fake = fake_backend();
    fake->send_returns = false;
    auto socket = create(fake, "ws://host");
    fake->open();
    const bool sent = send(socket, String("frame"));
    check(!sent && reason_of(socket_ns::explain_socket_send_failure(socket_ns::ReadonlySocket(socket))) == "null",
          "explainSocketSendFailure returns null once the socket can reach its backend, even when that backend returns false");
  }
  {
    auto fake = fake_backend();
    auto socket = create(fake, "ws://host");
    socket_ns::dispose_socket(socket);
    check(reason_of(socket_ns::explain_socket_send_failure(socket_ns::ReadonlySocket(socket))) == "disposed|closed|ws://host",
          "explainSocketSendFailure distinguishes terminal disposal from other closed or disconnected states");
  }
}

void test_guard_enablement() {
  socket_ns::enable_socket_guards();
  const bool enabled = socket_ns::are_socket_guards_enabled();
  socket_ns::disable_socket_guards();
  check(enabled && !socket_ns::are_socket_guards_enabled(), "areSocketGuardsEnabled reports whether the global guard hook is installed");

  // 'restores silent core behavior'. Upstream captures with createMemoryLogSink (initialize_log_entity
  // stub here), so a plain registered sink observes the same outcome: no entries, no throw.
  int entries = 0;
  LogSink recording = [&](flight::Ref<LogEntry>) { ++entries; };
  flight::log::add_log_sink(recording);
  socket_ns::enable_socket_guards();
  socket_ns::disable_socket_guards();
  bool threw = false;
  try {
    auto fake = fake_backend();
    fake->open_returns_null = true;
    auto socket = create(fake, "tcp://x");
    socket_ns::dispose_socket(socket);
    static_cast<void>(send(socket, String("x")));
  } catch (...) {
    threw = true;
  }
  flight::log::remove_log_sink(recording);
  check(!threw && entries == 0, "disableSocketGuards restores silent core behavior");
}

}  // namespace

int main() {
  test_attach_detach();
  test_close();
  test_create();
  test_dispose_and_signals();
  test_open_tcp_socket();
  test_send();
  test_guard();
  test_hosts();
  test_explain();
  test_guard_enablement();
  return failures == 0 ? 0 : 1;
}
