// Derived from @flighthq/socket/packages/socket/src/socket.ts.
#pragma once

#include <functional>
#include <optional>
#include <utility>
#include <variant>

#include <flight/array_buffer.hpp>
#include <flight/entity/entity.hpp>
#include <flight/runtime.hpp>
#include <flight/signals/emitter.hpp>
#include <flight/signals/signal.hpp>
#include <flight/structural_ref.hpp>
#include <flight/types/socket.hpp>

static_assert(flight::runtime_contract.compiler_contract == "flight-runtime-contract/2",
              "Flight compiler/runtime contract mismatch");
static_assert(flight::runtime_contract.cpp_abi == 1,
              "Flight C++ runtime ABI mismatch");

namespace flight::socket {

using flight::types::HostSocketCapability;
using flight::types::Socket;
using flight::types::SocketCloseInfo;
using flight::types::SocketConnection;
using flight::types::SocketEventSink;
using flight::types::SocketGuard;
using flight::types::SocketGuardNotice;
using flight::types::SocketMessage;
using flight::types::SocketOptions;
using flight::types::SocketReadyState;
using flight::types::SocketRuntime;
using flight::types::SocketSignals;
using flight::types::TcpSocketConnection;
using flight::types::TcpSocketOptions;

using ReadonlySocket = flight::StructuralRef<
    flight::RowReadonly<flight::RowOf<flight::Ref<Socket>>>>;
using ReadonlySocketOptions = flight::StructuralRef<
    flight::RowReadonly<flight::RowOf<flight::Ref<SocketOptions>>>>;
using ReadonlyTcpSocketOptions = flight::StructuralRef<
    flight::RowReadonly<flight::RowOf<flight::Ref<TcpSocketOptions>>>>;
using ReadonlyHostSocket = flight::StructuralRef<
    flight::RowReadonly<flight::RowOf<flight::Ref<HostSocketCapability>>>>;
using ReadonlySocketMessage = flight::StructuralRef<
    flight::RowReadonly<flight::RowOf<flight::Ref<SocketMessage>>>>;
using ReadonlySocketCloseInfo = flight::StructuralRef<
    flight::RowReadonly<flight::RowOf<flight::Ref<SocketCloseInfo>>>>;

inline std::optional<SocketGuard> socket_guard;

inline void emit_socket_guard(
    flight::String operation,
    flight::String reason,
    ReadonlySocket socket) {
  if (!socket_guard.has_value()) return;
  const auto notice = flight::make_ref<
      flight::types::operation_reason_socket_33737934b106de78>(
      flight::types::operation_reason_socket_33737934b106de78{
          .operation = std::move(operation),
          .reason = std::move(reason),
          .socket = std::move(socket),
      });
  socket_guard.value()(notice);
}

inline flight::Ref<SocketEventSink> make_socket_event_sink(
    flight::Ref<SocketRuntime> runtime) {
  return flight::make_ref<SocketEventSink>(SocketEventSink{
      .handle_socket_open = [runtime]() {
        if (!runtime->delivering) return;
        runtime->ready_state = flight::String("open");
        if (runtime->signals.has_value()) {
          flight::signals::emit_signal(runtime->signals.value()->on_socket_open);
        }
      },
      .handle_socket_message = [runtime](ReadonlySocketMessage message) {
        if (!runtime->delivering) return;
        if (runtime->signals.has_value()) {
          flight::signals::emit_signal(
              runtime->signals.value()->on_socket_message, std::move(message));
        }
      },
      .handle_socket_close = [runtime](ReadonlySocketCloseInfo info) {
        if (!runtime->delivering) return;
        runtime->ready_state = flight::String("closed");
        if (runtime->signals.has_value()) {
          flight::signals::emit_signal(
              runtime->signals.value()->on_socket_close, std::move(info));
        }
      },
      .handle_socket_error = [runtime]() {
        if (!runtime->delivering) return;
        if (runtime->signals.has_value()) {
          flight::signals::emit_signal(runtime->signals.value()->on_socket_error);
        }
      },
  });
}

inline void attach_socket(flight::Ref<Socket> socket) {
  if (socket->runtime->disposed) return;
  socket->runtime->delivering = true;
}

inline void close_socket(
    flight::Ref<Socket> socket,
    std::optional<double> code = std::nullopt,
    std::optional<flight::String> reason = std::nullopt) {
  const auto runtime = socket->runtime;
  if (runtime->disposed) {
    emit_socket_guard(
        flight::String("closeSocket"), flight::String("disposed"), ReadonlySocket(socket));
    return;
  }
  if (runtime->ready_state == flight::String("closing") ||
      runtime->ready_state == flight::String("closed")) {
    return;
  }
  runtime->ready_state = flight::String("closing");
  if (runtime->connection.has_value()) {
    runtime->connection.value()->close_socket_connection(std::move(code), std::move(reason));
  }
}

inline flight::Ref<Socket> create_socket(
    ReadonlyHostSocket host_socket,
    ReadonlySocketOptions options) {
  const auto runtime = flight::make_ref<SocketRuntime>(SocketRuntime{
      .connection = std::nullopt,
      .signals = std::nullopt,
      .ready_state = flight::String("connecting"),
      .delivering = true,
      .disposed = false,
  });
  const auto construction =
      flight::entity::allocate_entity<flight::Ref<Socket>>();
  construction->url = flight::row_get<flight::RowKey<"url">>(options);
  construction->runtime = runtime;
  const auto socket = flight::entity::finish_entity<flight::Ref<Socket>>(construction);
  const auto sink = make_socket_event_sink(runtime);
  runtime->connection =
      flight::row_get<flight::RowKey<"openSocket">>(host_socket)(options, sink);
  if (!runtime->connection.has_value()) {
    emit_socket_guard(
        flight::String("createSocket"), flight::String("no-connection"),
        ReadonlySocket(socket));
  }
  return socket;
}

inline void detach_socket(flight::Ref<Socket> socket) {
  socket->runtime->delivering = false;
}

inline void dispose_socket(flight::Ref<Socket> socket) {
  if (socket->runtime->disposed) return;
  close_socket(socket);
  detach_socket(socket);
  const auto runtime = socket->runtime;
  runtime->connection = std::nullopt;
  runtime->signals = std::nullopt;
  runtime->ready_state = flight::String("closed");
  runtime->disposed = true;
}

inline flight::Ref<SocketSignals> enable_socket_signals(
    flight::Ref<Socket> socket) {
  const auto runtime = socket->runtime;
  if (runtime->disposed) {
    emit_socket_guard(
        flight::String("enableSocketSignals"), flight::String("disposed"),
        ReadonlySocket(socket));
  }
  if (!runtime->signals.has_value()) {
    const auto construction =
        flight::entity::allocate_entity<flight::Ref<SocketSignals>>();
    construction->on_socket_open =
        flight::signals::create_signal<std::function<void()>>();
    construction->on_socket_message = flight::signals::create_signal<
        std::function<void(ReadonlySocketMessage)>>();
    construction->on_socket_close = flight::signals::create_signal<
        std::function<void(ReadonlySocketCloseInfo)>>();
    construction->on_socket_error =
        flight::signals::create_signal<std::function<void()>>();
    runtime->signals =
        flight::entity::finish_entity<flight::Ref<SocketSignals>>(construction);
  }
  return runtime->signals.value();
}

inline SocketReadyState get_socket_ready_state(ReadonlySocket socket) {
  return flight::row_get<flight::RowKey<"runtime">>(socket)->ready_state;
}

inline std::optional<flight::Ref<TcpSocketConnection>> open_tcp_socket(
    ReadonlyHostSocket host_socket,
    ReadonlyTcpSocketOptions options) {
  const auto open =
      flight::row_get<flight::RowKey<"openTcpSocket">>(host_socket);
  if (!open.has_value()) return std::nullopt;
  return open.value()(std::move(options));
}

inline bool send_socket_message(
    ReadonlySocket socket,
    std::variant<flight::ArrayBuffer, flight::String> data) {
  const auto runtime = flight::row_get<flight::RowKey<"runtime">>(socket);
  if (runtime->disposed) {
    emit_socket_guard(
        flight::String("sendSocketMessage"), flight::String("disposed"), socket);
    return false;
  }
  if (runtime->ready_state != flight::String("open") ||
      !runtime->connection.has_value()) {
    return false;
  }
  return runtime->connection.value()->send_socket_frame(std::move(data));
}

inline void set_socket_guard(std::optional<SocketGuard> guard) {
  socket_guard = std::move(guard);
}

}  // namespace flight::socket
