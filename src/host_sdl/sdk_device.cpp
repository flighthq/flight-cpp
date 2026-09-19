#include <flight/host_sdl/sdk_device.hpp>

#include <flight/host_sdl/window.hpp>
#include <flight/types/device.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string_view>

#include <SDL3/SDL_cpuinfo.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_platform.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_video.h>

namespace flight::host_sdl {
namespace {

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

[[nodiscard]] flight::String os_name(std::string_view platform) {
  if (platform == "Windows") return flight::String("Windows");
  if (platform == "macOS") return flight::String("macOS");
  if (platform == "Linux") return flight::String("Linux");
  if (platform == "iOS") return flight::String("iOS");
  if (platform == "Android") return flight::String("Android");
  return flight::String();
}

[[nodiscard]] flight::types::DeviceFormFactor form_factor(std::string_view platform) {
  if (platform == "Windows" || platform == "macOS" || platform == "Linux") {
    return flight::types::device_form_factor_desktop;
  }
  return flight::types::device_form_factor_unknown;
}

template <typename Id, typename Enumerate>
[[nodiscard]] bool has_device(Enumerate enumerate) {
  int count = 0;
  Id* ids = enumerate(&count);
  SDL_free(ids);
  return count > 0;
}

void initialize_display_metrics(flight::types::DeviceDisplayMetrics& output) {
  output.color_depth = -1.0;
  output.density_dpi = -1.0;
  output.logical_height = -1.0;
  output.logical_width = -1.0;
  output.physical_height = -1.0;
  output.physical_width = -1.0;
  output.pixel_ratio = -1.0;
}

void initialize_device_info(flight::types::DeviceInfo& output) {
  output.arch = flight::String();
  output.available_memory = -1.0;
  output.board_name = flight::String();
  output.color_gamut = flight::String();
  output.cpu_cores = -1.0;
  output.font_scale = -1.0;
  output.form_factor = flight::types::device_form_factor_unknown;
  output.gpu_renderer = flight::String();
  output.gpu_vendor = flight::String();
  output.is_hdr = false;
  output.is_jailbroken = false;
  output.is_low_end_device = false;
  output.is_rooted = false;
  output.is_virtual = false;
  output.manufacturer = flight::String();
  output.marketing_name = flight::String();
  output.model = flight::String();
  output.os_build = flight::String();
  output.os_name = flight::String();
  output.os_version = flight::String();
  output.platform_string = flight::String();
  output.product_name = flight::String();
  output.supported_abis = flight::Array<flight::String>();
  output.total_memory = -1.0;
  output.web_view_version = flight::String();
}

[[nodiscard]] SDL_Window* resolve_window(std::uint32_t window_id) {
  return SDL_GetWindowFromID(static_cast<SDL_WindowID>(window_id));
}

} // namespace

SdkDeviceBackend::SdkDeviceBackend(const Window& window)
    : window_id_(static_cast<std::uint32_t>(window.id())) {}

flight::types::HostDeviceCapability SdkDeviceBackend::backend() const {
  flight::types::HostDeviceCapability result;
  result.get_capabilities = [](flight::Ref<flight::types::DeviceCapabilities> output) {
    if (output == nullptr) return output;
    output->has_keyboard = has_device<SDL_KeyboardID>(SDL_GetKeyboards);
    output->has_mouse = has_device<SDL_MouseID>(SDL_GetMice);
    output->has_stylus = false;
    return output;
  };
  result.get_display_metrics = [window_id = window_id_](
                                   flight::Ref<flight::types::DeviceDisplayMetrics> output) {
    if (output == nullptr) return output;
    initialize_display_metrics(*output);
    SDL_Window* window = resolve_window(window_id);
    if (window == nullptr) return output;
    const SDL_DisplayID display = SDL_GetDisplayForWindow(window);
    if (display == 0) return output;
    const SDL_DisplayMode* mode = SDL_GetDesktopDisplayMode(display);
    if (mode == nullptr || mode->w <= 0 || mode->h <= 0) return output;

    output->logical_width = static_cast<double>(mode->w);
    output->logical_height = static_cast<double>(mode->h);
    if (mode->pixel_density > 0.0F) {
      output->pixel_ratio = static_cast<double>(mode->pixel_density);
      output->physical_width = std::round(output->logical_width * output->pixel_ratio);
      output->physical_height = std::round(output->logical_height * output->pixel_ratio);
    }
    const int bits_per_pixel = SDL_BITSPERPIXEL(mode->format);
    if (bits_per_pixel > 0) output->color_depth = static_cast<double>(bits_per_pixel);
    return output;
  };
  result.get_id = [] { return flight::String(); };
  result.get_info = [](flight::Ref<flight::types::DeviceInfo> output) {
    if (output == nullptr) return output;
    initialize_device_info(*output);
    const char* raw_platform = SDL_GetPlatform();
    const std::string_view platform = raw_platform == nullptr ? std::string_view() : raw_platform;
    const int cpu_cores = SDL_GetNumLogicalCPUCores();
    const int system_ram_mib = SDL_GetSystemRAM();

    output->arch = architecture();
    if (cpu_cores > 0) output->cpu_cores = static_cast<double>(cpu_cores);
    output->form_factor = form_factor(platform);
    output->os_name = os_name(platform);
    output->platform_string = flight::String(platform);
    if (system_ram_mib > 0) {
      output->total_memory = static_cast<double>(system_ram_mib) * 1024.0 * 1024.0;
    }
    output->is_low_end_device =
        (system_ram_mib > 0 && system_ram_mib <= 1024) || (cpu_cores > 0 && cpu_cores <= 2);
    return output;
  };
  result.get_safe_area_insets = [window_id = window_id_](
                                    flight::Ref<flight::types::SafeAreaInsets> output) {
    if (output == nullptr) return output;
    output->top = 0.0;
    output->right = 0.0;
    output->bottom = 0.0;
    output->left = 0.0;
    SDL_Window* window = resolve_window(window_id);
    if (window == nullptr) return output;

    SDL_Rect safe_area{};
    int width = 0;
    int height = 0;
    if (!SDL_GetWindowSafeArea(window, &safe_area) || !SDL_GetWindowSize(window, &width, &height)) {
      return output;
    }
    output->top = static_cast<double>(std::max(safe_area.y, 0));
    output->left = static_cast<double>(std::max(safe_area.x, 0));
    output->right = static_cast<double>(std::max(width - safe_area.x - safe_area.w, 0));
    output->bottom = static_cast<double>(std::max(height - safe_area.y - safe_area.h, 0));
    return output;
  };
  result.refresh = std::nullopt;
  return result;
}

} // namespace flight::host_sdl
