#include <flight/host_sdl/sdk_platform.hpp>

#include <flight/types/platform.hpp>

#include <bit>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

#include <SDL3/SDL_locale.h>
#include <SDL3/SDL_platform.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_touch.h>

namespace flight::host_sdl {
namespace {

[[nodiscard]] flight::String platform_name(std::string_view platform) {
  if (platform == "Windows") return flight::String("windows");
  if (platform == "macOS") return flight::String("macos");
  if (platform == "Linux") return flight::String("linux");
  if (platform == "iOS") return flight::String("ios");
  if (platform == "Android") return flight::String("android");
  return flight::String("unknown");
}

[[nodiscard]] flight::String platform_kind(const flight::String& name) {
  if (name == flight::String("ios") || name == flight::String("android")) {
    return flight::String("mobile");
  }
  if (name == flight::String("windows") || name == flight::String("macos") ||
      name == flight::String("linux")) {
    return flight::String("desktop");
  }
  return flight::String("unknown");
}

[[nodiscard]] flight::String architecture() {
#if defined(__x86_64__) || defined(_M_X64)
  return flight::String("x64");
#elif defined(__i386__) || defined(_M_IX86)
  return flight::String("x86");
#elif defined(__aarch64__) || defined(_M_ARM64)
  return flight::String("arm64");
#elif defined(__arm__) || defined(_M_ARM)
  return flight::String("arm");
#elif defined(__riscv) && __riscv_xlen == 64
  return flight::String("riscv64");
#elif defined(__mips64)
  return flight::String("mips64");
#elif defined(__mips__)
  return flight::String("mips");
#else
  return flight::String();
#endif
}

[[nodiscard]] flight::String preferred_locale() {
  int count = 0;
  SDL_Locale** locales = SDL_GetPreferredLocales(&count);
  if (locales == nullptr || count <= 0 || locales[0] == nullptr ||
      locales[0]->language == nullptr) {
    SDL_free(locales);
    return flight::String();
  }
  std::string locale(locales[0]->language);
  if (locales[0]->country != nullptr && locales[0]->country[0] != '\0') {
    locale.push_back('-');
    locale.append(locales[0]->country);
  }
  SDL_free(locales);
  return flight::String(locale);
}

[[nodiscard]] bool has_touch_device() {
  int count = 0;
  SDL_TouchID* devices = SDL_GetTouchDevices(&count);
  SDL_free(devices);
  return count > 0;
}

} // namespace

flight::types::PlatformBackend SdkPlatformBackend::backend() const {
  flight::types::PlatformBackend result;
  result.entity_runtime_key = std::nullopt;
  result.get_info = [](flight::Ref<flight::types::PlatformInfo> output) {
    if (output == nullptr) return output;
    const char* raw_platform = SDL_GetPlatform();
    output->name = platform_name(raw_platform == nullptr ? std::string_view() : raw_platform);
    output->kind = platform_kind(output->name);
    output->version = flight::String();
    output->arch = architecture();
    output->locale = preferred_locale();
    output->is_touch = has_touch_device();
    output->runtime = flight::String("native");
    output->engine = flight::String("unknown");
    output->engine_version = flight::String();
    output->endianness = flight::String(
        std::endian::native == std::endian::little
            ? "little"
            : (std::endian::native == std::endian::big ? "big" : "unknown"));
    output->pointer_width = static_cast<double>(sizeof(void*) * 8U);
    output->os_build = flight::String();
    output->distro = flight::String();
    output->distro_version = flight::String();
    return output;
  };
  return result;
}

} // namespace flight::host_sdl
