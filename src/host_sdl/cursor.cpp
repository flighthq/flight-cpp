#include <flight/host_sdl/cursor.hpp>

#include "detail.hpp"

#include <array>
#include <cstddef>
#include <stdexcept>
#include <string_view>
#include <utility>

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_mouse.h>

namespace flight::host_sdl {
namespace {

[[nodiscard]] SDL_SystemCursor resolve_system_cursor(std::string_view cursor) noexcept {
  if (cursor == "text" || cursor == "vertical-text") return SDL_SYSTEM_CURSOR_TEXT;
  if (cursor == "wait") return SDL_SYSTEM_CURSOR_WAIT;
  if (cursor == "crosshair" || cursor == "cell") return SDL_SYSTEM_CURSOR_CROSSHAIR;
  if (cursor == "progress") return SDL_SYSTEM_CURSOR_PROGRESS;
  if (cursor == "nwse-resize") return SDL_SYSTEM_CURSOR_NWSE_RESIZE;
  if (cursor == "nesw-resize") return SDL_SYSTEM_CURSOR_NESW_RESIZE;
  if (cursor == "ew-resize" || cursor == "col-resize") return SDL_SYSTEM_CURSOR_EW_RESIZE;
  if (cursor == "ns-resize" || cursor == "row-resize") return SDL_SYSTEM_CURSOR_NS_RESIZE;
  if (cursor == "move" || cursor == "all-scroll" || cursor == "grab" ||
      cursor == "grabbing") {
    return SDL_SYSTEM_CURSOR_MOVE;
  }
  if (cursor == "no-drop" || cursor == "not-allowed") {
    return SDL_SYSTEM_CURSOR_NOT_ALLOWED;
  }
  if (cursor == "pointer" || cursor == "alias" || cursor == "copy") {
    return SDL_SYSTEM_CURSOR_POINTER;
  }
  if (cursor == "nw-resize") return SDL_SYSTEM_CURSOR_NW_RESIZE;
  if (cursor == "n-resize") return SDL_SYSTEM_CURSOR_N_RESIZE;
  if (cursor == "ne-resize") return SDL_SYSTEM_CURSOR_NE_RESIZE;
  if (cursor == "e-resize") return SDL_SYSTEM_CURSOR_E_RESIZE;
  if (cursor == "se-resize") return SDL_SYSTEM_CURSOR_SE_RESIZE;
  if (cursor == "s-resize") return SDL_SYSTEM_CURSOR_S_RESIZE;
  if (cursor == "sw-resize") return SDL_SYSTEM_CURSOR_SW_RESIZE;
  if (cursor == "w-resize") return SDL_SYSTEM_CURSOR_W_RESIZE;
  return SDL_SYSTEM_CURSOR_DEFAULT;
}

} // namespace

struct SdlCursorBackend::State final {
  ~State() noexcept {
    for (auto* cursor : native_cursors) {
      if (cursor == nullptr) continue;
      if (SDL_WasInit(SDL_INIT_VIDEO) != 0 && SDL_GetCursor() == cursor) {
        static_cast<void>(SDL_SetCursor(SDL_GetDefaultCursor()));
      }
      SDL_DestroyCursor(cursor);
    }
  }

  [[nodiscard]] SDL_Cursor* cursor(SDL_SystemCursor system_cursor) noexcept {
    const auto index = static_cast<std::size_t>(system_cursor);
    auto*& cursor = native_cursors[index];
    if (cursor != nullptr) return cursor;
    cursor = SDL_CreateSystemCursor(system_cursor);
    if (cursor == nullptr) SDL_ClearError();
    return cursor;
  }

  std::array<SDL_Cursor*, SDL_SYSTEM_CURSOR_COUNT> native_cursors{};
  std::optional<String> current;
};

SdlCursorBackend::SdlCursorBackend() : state_(std::make_shared<State>()) {}

SdlCursorBackend::~SdlCursorBackend() noexcept = default;

SdlCursorBackend::SdlCursorBackend(const SdlCursorBackend&) noexcept = default;

SdlCursorBackend& SdlCursorBackend::operator=(const SdlCursorBackend&) noexcept = default;

SdlCursorBackend::SdlCursorBackend(SdlCursorBackend&&) noexcept = default;

SdlCursorBackend& SdlCursorBackend::operator=(SdlCursorBackend&&) noexcept = default;

void SdlCursorBackend::set_cursor(std::optional<String> cursor) const {
  if (state_ == nullptr) throw std::logic_error("moved-from SDL cursor backend");
  if (SDL_WasInit(SDL_INIT_VIDEO) == 0) {
    throw std::logic_error("SDL cursor backend requires the video subsystem");
  }

  if (cursor && *cursor == String("none")) {
    detail::require_sdl(SDL_HideCursor(), "SDL_HideCursor");
    state_->current = std::move(cursor);
    return;
  }

  const auto system_cursor = resolve_system_cursor(cursor ? cursor->to_utf8() : std::string_view{});
  auto* native_cursor = state_->cursor(system_cursor);
  if (native_cursor == nullptr) {
    native_cursor = SDL_GetDefaultCursor();
    if (native_cursor == nullptr) SDL_ClearError();
  }
  if (native_cursor != nullptr) detail::require_sdl(SDL_SetCursor(native_cursor), "SDL_SetCursor");
  detail::require_sdl(SDL_ShowCursor(), "SDL_ShowCursor");
  state_->current = std::move(cursor);
}

std::optional<String> SdlCursorBackend::current_cursor() const {
  return state_ == nullptr ? std::nullopt : state_->current;
}

} // namespace flight::host_sdl
