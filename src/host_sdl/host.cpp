#include <flight/host_sdl/host.hpp>

#include "detail.hpp"

#include <algorithm>
#include <limits>
#include <utility>

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_timer.h>

namespace flight::host_sdl {

Host::Host(SDL_InitFlags subsystems) : subsystems_(subsystems) {
  if (subsystems_ == 0) throw std::invalid_argument("SDL host requires at least one subsystem");
  if (!SDL_InitSubSystem(subsystems_)) {
    subsystems_ = 0;
    detail::throw_sdl_error("SDL_InitSubSystem");
  }
}

Host::~Host() noexcept { reset(); }

Host::Host(Host&& other) noexcept : subsystems_(std::exchange(other.subsystems_, 0)) {}

Host& Host::operator=(Host&& other) noexcept {
  if (this == &other) return *this;
  reset();
  subsystems_ = std::exchange(other.subsystems_, 0);
  return *this;
}

SDL_InitFlags Host::subsystems() const noexcept { return subsystems_; }

bool Host::poll_event(SDL_Event& event) const noexcept { return SDL_PollEvent(&event); }

void Host::wait_event(SDL_Event& event) const {
  detail::require_sdl(SDL_WaitEvent(&event), "SDL_WaitEvent");
}

bool Host::wait_event_for(SDL_Event& event, std::chrono::milliseconds timeout) const {
  if (timeout.count() < 0) throw std::invalid_argument("SDL event timeout cannot be negative");
  const auto maximum = static_cast<std::chrono::milliseconds::rep>(std::numeric_limits<Sint32>::max());
  const auto milliseconds = static_cast<Sint32>(std::min(timeout.count(), maximum));
  SDL_ClearError();
  if (SDL_WaitEventTimeout(&event, milliseconds)) return true;
  const char* error = SDL_GetError();
  if (error != nullptr && error[0] != '\0') detail::throw_sdl_error("SDL_WaitEventTimeout");
  return false;
}

std::uint64_t Host::ticks_nanoseconds() noexcept { return SDL_GetTicksNS(); }

void Host::reset() noexcept {
  if (subsystems_ == 0) return;
  SDL_QuitSubSystem(subsystems_);
  subsystems_ = 0;
  if (SDL_WasInit(0) == 0) SDL_Quit();
}

} // namespace flight::host_sdl
