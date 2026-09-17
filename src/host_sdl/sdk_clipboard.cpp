#include <flight/host_sdl/sdk_clipboard.hpp>

#include <flight/types/clipboard.hpp>

#include <optional>
#include <string>

#include <SDL3/SDL_clipboard.h>
#include <SDL3/SDL_stdinc.h>

namespace flight::host_sdl {

flight::types::HostClipboardTextProvider SdkClipboardTextBackend::backend() const {
  flight::types::HostClipboardTextProvider result;
  result.entity_runtime_key = std::nullopt;
  result.clear = [] {
    return flight::Task<bool>::resolve(SDL_ClearClipboardData());
  };
  result.has_text = [] {
    return flight::Task<bool>::resolve(SDL_HasClipboardText());
  };
  result.read_text = [] {
    char* text = SDL_GetClipboardText();
    if (text == nullptr) return flight::Task<flight::String>::resolve(flight::String());
    const flight::String value(text);
    SDL_free(text);
    return flight::Task<flight::String>::resolve(value);
  };
  result.write_text = [](flight::String text) {
    const std::string value = text.to_utf8();
    return flight::Task<bool>::resolve(SDL_SetClipboardText(value.c_str()));
  };
  return result;
}

} // namespace flight::host_sdl
