#pragma once

#include <flight/host_sdl/export.hpp>

namespace flight::types {
struct ClipboardTextBackend;
}

namespace flight::host_sdl {

// Produces the exact generated Flight plain-text clipboard record over SDL's main-thread clipboard
// API. Rich, image, bookmark, and change-notification capabilities remain absent.
class FLIGHT_HOST_SDL_SDK_CLIPBOARD_API SdkClipboardTextBackend final {
 public:
  [[nodiscard]] flight::types::ClipboardTextBackend backend() const;
};

} // namespace flight::host_sdl
