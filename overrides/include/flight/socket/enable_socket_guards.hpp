// Derived from @flighthq/socket/packages/socket/src/enableSocketGuards.ts.
#pragma once

#include <optional>
#include <variant>

#include <flight/log/contract.hpp>
#include <flight/record.hpp>
#include <flight/runtime.hpp>
#include <flight/socket/socket.hpp>
#include <flight/types/log.hpp>
#include <flight/types/socket.hpp>

static_assert(flight::runtime_contract.compiler_contract == "flight-runtime-contract/2",
              "Flight compiler/runtime contract mismatch");
static_assert(flight::runtime_contract.cpp_abi == 1,
              "Flight C++ runtime ABI mismatch");

namespace flight::socket {

inline bool socket_guards_enabled = false;

inline void warn_on_socket_misuse(flight::types::SocketGuardNotice notice) {
  const auto url = flight::row_get<flight::RowKey<"url">>(notice->socket);
  const auto message = notice->reason == flight::String("no-connection")
                           ? flight::String(
                                 "createSocket: the host carries no socket capability for this "
                                 "transport, or the capability returned no connection — pass a "
                                 "host whose net.socket supports it")
                           : notice->operation + flight::String(
                                 ": socket is already disposed — call createSocket(...) to create "
                                 "a new socket");
  flight::log::log_once(
      flight::String("socket:") + notice->operation + flight::String(":") + notice->reason,
      flight::types::LogLevel::Warn,
      flight::types::LogData{
          std::in_place_type<flight::Record<flight::String, flight::Any>>,
          flight::Record<flight::String, flight::Any>{
              {flight::String("message"), message},
              {flight::String("operation"), notice->operation},
              {flight::String("reason"), notice->reason},
              {flight::String("url"), url},
          },
      },
      std::optional<flight::String>{flight::String("socket")});
}

inline bool are_socket_guards_enabled() { return socket_guards_enabled; }

inline void disable_socket_guards() {
  set_socket_guard(std::nullopt);
  socket_guards_enabled = false;
}

inline void enable_socket_guards() {
  set_socket_guard(flight::types::SocketGuard{warn_on_socket_misuse});
  socket_guards_enabled = true;
}

}  // namespace flight::socket
