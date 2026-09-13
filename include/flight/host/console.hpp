#pragma once

#include <iostream>

#include <flight/string.hpp>

namespace flight::host {

struct Console final {};

inline constexpr Console console;

inline void console_debug(const String& message) {
  std::clog << message.to_utf8() << '\n';
}

} // namespace flight::host
