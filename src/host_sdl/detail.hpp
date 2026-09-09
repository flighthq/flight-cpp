#pragma once

#include <stdexcept>
#include <string>
#include <string_view>

#include <SDL3/SDL_error.h>

namespace flight::host_sdl::detail {

[[noreturn]] inline void throw_sdl_error(std::string_view operation) {
  std::string message(operation);
  const char* error = SDL_GetError();
  if (error != nullptr && error[0] != '\0') {
    message.append(": ");
    message.append(error);
  }
  throw std::runtime_error(message);
}

inline void require_sdl(bool result, std::string_view operation) {
  if (!result) throw_sdl_error(operation);
}

} // namespace flight::host_sdl::detail
