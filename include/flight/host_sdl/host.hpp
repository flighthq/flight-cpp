#pragma once

#include <chrono>
#include <cstdint>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>

#include <flight/host_sdl/export.hpp>

namespace flight::host_sdl {

class FLIGHT_HOST_SDL_API Host final {
 public:
  explicit Host(SDL_InitFlags subsystems = SDL_INIT_VIDEO | SDL_INIT_EVENTS);
  ~Host() noexcept;

  Host(const Host&) = delete;
  Host& operator=(const Host&) = delete;
  Host(Host&& other) noexcept;
  Host& operator=(Host&& other) noexcept;

  [[nodiscard]] SDL_InitFlags subsystems() const noexcept;
  [[nodiscard]] bool poll_event(SDL_Event& event) const noexcept;
  void wait_event(SDL_Event& event) const;
  [[nodiscard]] bool wait_event_for(SDL_Event& event, std::chrono::milliseconds timeout) const;

  [[nodiscard]] static std::uint64_t ticks_nanoseconds() noexcept;

 private:
  void reset() noexcept;

  SDL_InitFlags subsystems_{0};
};

} // namespace flight::host_sdl
