#include <flight/socket/contract.hpp>
#include <flight/statechart/enable_statechart_guards.hpp>

#include <functional>
#include <iostream>
#include <optional>
#include <variant>

namespace {

bool check(bool condition, const char* message) {
  if (!condition) std::cerr << message << '\n';
  return condition;
}

}  // namespace

int main() {
  using flight::String;
  using namespace flight::types;

  std::function<void()> host_open;
  std::function<void(flight::socket::ReadonlySocketMessage)> host_message;
  std::function<void(flight::socket::ReadonlySocketCloseInfo)> host_close;
  int close_calls = 0;
  int send_calls = 0;
  std::optional<double> close_code;
  std::optional<String> close_reason;
  const auto connection = flight::make_ref<SocketConnection>(SocketConnection{
      .send_socket_frame = [&](std::variant<flight::ArrayBuffer, String> data) {
        ++send_calls;
        return std::get<String>(data) == String("payload");
      },
      .close_socket_connection = [&](std::optional<double> code,
                                     std::optional<String> reason) {
        ++close_calls;
        close_code = code;
        close_reason = std::move(reason);
      },
  });
  const auto host = flight::make_ref<HostSocketCapability>(HostSocketCapability{
      .open_socket = [&](auto options, auto events) {
        if (options->url != String("wss://flight.test")) {
          return std::optional<flight::Ref<SocketConnection>>{};
        }
        host_open = events->handle_socket_open;
        host_message = events->handle_socket_message;
        host_close = events->handle_socket_close;
        return std::optional<flight::Ref<SocketConnection>>{connection};
      },
      .open_tcp_socket = std::nullopt,
  });
  const auto options = flight::make_ref<SocketOptions>(SocketOptions{
      .url = String("wss://flight.test"),
      .protocols = std::nullopt,
      .binary_type = std::nullopt,
  });

  int guard_calls = 0;
  flight::types::SocketGuardNotice last_notice;
  flight::socket::set_socket_guard(SocketGuard{[&](SocketGuardNotice notice) {
    ++guard_calls;
    last_notice = std::move(notice);
  }});
  const auto socket = flight::socket::create_socket(
      flight::socket::ReadonlyHostSocket(host),
      flight::socket::ReadonlySocketOptions(options));
  const auto signals = flight::socket::enable_socket_signals(socket);
  const auto signals_again = flight::socket::enable_socket_signals(socket);
  int opens = 0;
  int messages = 0;
  int closes = 0;
  signals->on_socket_open->emit = [&]() { ++opens; };
  signals->on_socket_message->emit = [&](auto) { ++messages; };
  signals->on_socket_close->emit = [&](auto) { ++closes; };

  host_open();
  const auto message = flight::make_ref<SocketMessage>(SocketMessage{
      .data = String("incoming"),
      .binary = false,
  });
  flight::socket::detach_socket(socket);
  host_message(flight::socket::ReadonlySocketMessage(message));
  flight::socket::attach_socket(socket);
  host_message(flight::socket::ReadonlySocketMessage(message));
  if (!check(signals == signals_again, "socket signals were not allocated once") ||
      !check(opens == 1 && messages == 1,
             "socket attach or detach changed event delivery") ||
      !check(flight::socket::get_socket_ready_state(
                 flight::socket::ReadonlySocket(socket)) == String("open"),
             "socket open event did not update readyState") ||
      !check(flight::socket::send_socket_message(
                 flight::socket::ReadonlySocket(socket), String("payload")) &&
                 send_calls == 1,
             "socket frame did not reach its exact backend")) {
    return 1;
  }

  flight::socket::close_socket(socket, 1000.0, String("done"));
  const auto close_info = flight::make_ref<SocketCloseInfo>(SocketCloseInfo{
      .code = 1000.0,
      .reason = String("done"),
      .was_clean = true,
  });
  host_close(flight::socket::ReadonlySocketCloseInfo(close_info));
  flight::socket::dispose_socket(socket);
  flight::socket::dispose_socket(socket);
  flight::socket::attach_socket(socket);
  if (!check(close_calls == 1 && close_code == 1000.0 &&
                 close_reason == String("done"),
             "socket close arguments or idempotence changed") ||
      !check(closes == 1 && socket->runtime->disposed &&
                 !socket->runtime->delivering &&
                 socket->runtime->ready_state == String("closed"),
             "socket close event or terminal disposal changed") ||
      !check(!flight::socket::send_socket_message(
                  flight::socket::ReadonlySocket(socket), String("ignored")) &&
                 guard_calls == 1 &&
                 last_notice->operation == String("sendSocketMessage") &&
                 last_notice->reason == String("disposed") &&
                 last_notice->socket.shared_object() == socket,
             "disposed send guard lost its operation or socket identity") ||
      !check(!flight::socket::open_tcp_socket(
                  flight::socket::ReadonlyHostSocket(host),
                  flight::socket::ReadonlyTcpSocketOptions(
                      flight::make_ref<TcpSocketOptions>(TcpSocketOptions{
                          .host = String("localhost"),
                          .port = 1234.0,
                      })))
                  .has_value(),
             "absent raw TCP capability did not remain absent")) {
    return 1;
  }
  flight::socket::set_socket_guard(std::nullopt);

  const auto make_chart = []() {
    return flight::make_ref<Statechart>(Statechart{
        .inputs = {},
        .name = std::nullopt,
        .regions = {},
    });
  };
  const auto chart = make_chart();
  const auto other_chart = make_chart();
  const auto make_instance = [](flight::Ref<Statechart> value) {
    return flight::make_ref<StatechartInstance>(StatechartInstance{
        .entity_runtime_key = std::nullopt,
        .chart = flight::statechart::ReadonlyStatechart(value),
        .duration_guard = std::nullopt,
        .input_values = {},
        .region_blend = {},
        .region_duration = {},
        .region_elapsed = {},
        .region_states = {},
        .region_transitions = {},
        .signals = std::nullopt,
    });
  };
  const auto instance = make_instance(chart);
  const auto instance_row =
      flight::statechart::ReadonlyStatechartInstance(instance);
  flight::statechart::enable_statechart_guards(instance);
  if (!check(flight::statechart::are_statechart_guards_enabled(instance_row),
             "statechart guard identity was not recognized")) {
    return 1;
  }
  instance->duration_guard = StatechartDurationGuard{[](auto, auto) {}};
  if (!check(!flight::statechart::are_statechart_guards_enabled(instance_row),
             "a replacement statechart hook was mistaken for the built-in guard")) {
    return 1;
  }
  flight::statechart::enable_statechart_guards(instance);
  flight::statechart::disable_statechart_guards(instance);
  const auto first_id = flight::statechart::get_statechart_guard_chart_id(
      flight::statechart::ReadonlyStatechart(chart));
  const auto repeated_id = flight::statechart::get_statechart_guard_chart_id(
      flight::statechart::ReadonlyStatechart(chart));
  const auto other_id = flight::statechart::get_statechart_guard_chart_id(
      flight::statechart::ReadonlyStatechart(other_chart));
  if (!check(!flight::statechart::are_statechart_guards_enabled(instance_row),
             "statechart guard did not disable") ||
      !check(first_id == repeated_id && first_id != other_id,
             "statechart chart IDs did not preserve object identity")) {
    return 1;
  }

  return 0;
}
