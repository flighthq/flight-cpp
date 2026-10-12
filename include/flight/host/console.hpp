#pragma once

#include <iostream>

#include <flight/string.hpp>

namespace flight::host {

struct Console final {};

inline constexpr Console console;

// The console methods a Flight program selects by level. Each writes one line: `log` and `info` are
// ordinary output, `warn` and `error` are diagnostics, and `debug` keeps its existing unbuffered log
// stream. None formats, prefixes, or filters -- the caller already decided what to write and whether to.
inline void console_debug(const String& message) {
  std::clog << message.to_utf8() << '\n';
}

inline void console_log(const String& message) {
  std::cout << message.to_utf8() << '\n';
}

inline void console_info(const String& message) {
  std::cout << message.to_utf8() << '\n';
}

inline void console_warn(const String& message) {
  std::cerr << message.to_utf8() << '\n';
}

inline void console_error(const String& message) {
  std::cerr << message.to_utf8() << '\n';
}

} // namespace flight::host
