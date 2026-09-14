#pragma once

#include <memory>
#include <optional>

#include <flight/host_sdl/export.hpp>
#include <flight/string.hpp>

namespace flight::types {
struct CursorBackend;
}

namespace flight::host_sdl {

// Owns the SDL cursor service and produces the exact generated Flight CursorBackend record. This is
// a build-tree preview adapter until the generated SDK target is installable.
class FLIGHT_HOST_SDL_SDK_CURSOR_API SdkCursorBackend final {
 public:
  SdkCursorBackend();
  ~SdkCursorBackend() noexcept;

  SdkCursorBackend(const SdkCursorBackend&) noexcept;
  SdkCursorBackend& operator=(const SdkCursorBackend&) noexcept;
  SdkCursorBackend(SdkCursorBackend&&) noexcept;
  SdkCursorBackend& operator=(SdkCursorBackend&&) noexcept;

  [[nodiscard]] flight::types::CursorBackend backend() const;
  [[nodiscard]] std::optional<String> current_cursor() const;

 private:
  struct State;
  std::shared_ptr<State> state_;
};

} // namespace flight::host_sdl
