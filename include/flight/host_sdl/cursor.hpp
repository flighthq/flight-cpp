#pragma once

#include <memory>
#include <optional>

#include <flight/host_sdl/export.hpp>
#include <flight/string.hpp>

namespace flight::host_sdl {

// Applies Flight's CSS-compatible cursor names through SDL's system cursor set. Copies share the
// selected cursor and its native allocation. SDL cursor operations must run on the main thread while
// the video subsystem is active.
class FLIGHT_HOST_SDL_API SdlCursorBackend final {
 public:
  SdlCursorBackend();
  ~SdlCursorBackend() noexcept;

  SdlCursorBackend(const SdlCursorBackend&) noexcept;
  SdlCursorBackend& operator=(const SdlCursorBackend&) noexcept;
  SdlCursorBackend(SdlCursorBackend&&) noexcept;
  SdlCursorBackend& operator=(SdlCursorBackend&&) noexcept;

  void set_cursor(std::optional<String> cursor) const;
  [[nodiscard]] std::optional<String> current_cursor() const;

 private:
  struct State;
  std::shared_ptr<State> state_;
};

} // namespace flight::host_sdl
