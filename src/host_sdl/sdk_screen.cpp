#include <flight/host_sdl/sdk_screen.hpp>

#include <flight/types/screen.hpp>

#include <algorithm>
#include <cmath>
#include <optional>

#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_video.h>

namespace flight::host_sdl {
namespace {

void initialize_screen(flight::types::ScreenInfo& output) {
  output.id = 0.0;
  output.x = 0.0;
  output.y = 0.0;
  output.width = 0.0;
  output.height = 0.0;
  output.work_width = 0.0;
  output.work_height = 0.0;
  output.scale_factor = 1.0;
  output.is_primary = false;
  output.rotation = -1.0;
  output.orientation = flight::String("Landscape");
  output.refresh_rate = -1.0;
  output.color_depth = -1.0;
  output.pixel_depth = -1.0;
  output.physical_width = -1.0;
  output.physical_height = -1.0;
  output.is_hdr = false;
  output.color_space = flight::String("srgb");
  output.max_luminance = -1.0;
  output.depth_per_component = -1.0;
  output.dpi = -1.0;
  output.label = flight::String();
  output.internal = false;
  output.touch_support = flight::String("unknown");
  output.monochrome = false;
}

void set_orientation(flight::types::ScreenInfo& output, SDL_DisplayOrientation orientation) {
  switch (orientation) {
    case SDL_ORIENTATION_LANDSCAPE:
      output.rotation = 0.0;
      output.orientation = flight::String("Landscape");
      break;
    case SDL_ORIENTATION_LANDSCAPE_FLIPPED:
      output.rotation = 180.0;
      output.orientation = flight::String("LandscapeFlipped");
      break;
    case SDL_ORIENTATION_PORTRAIT:
      output.rotation = 90.0;
      output.orientation = flight::String("Portrait");
      break;
    case SDL_ORIENTATION_PORTRAIT_FLIPPED:
      output.rotation = 270.0;
      output.orientation = flight::String("PortraitFlipped");
      break;
    case SDL_ORIENTATION_UNKNOWN:
      break;
  }
}

void fill_screen(SDL_DisplayID display, SDL_DisplayID primary, flight::types::ScreenInfo& output) {
  initialize_screen(output);
  if (display == 0) return;
  output.id = static_cast<double>(display);
  output.is_primary = display == primary;

  SDL_Rect bounds{};
  if (SDL_GetDisplayBounds(display, &bounds)) {
    output.x = static_cast<double>(bounds.x);
    output.y = static_cast<double>(bounds.y);
    output.width = static_cast<double>(bounds.w);
    output.height = static_cast<double>(bounds.h);
  }
  SDL_Rect work_area{};
  if (SDL_GetDisplayUsableBounds(display, &work_area)) {
    output.work_width = static_cast<double>(work_area.w);
    output.work_height = static_cast<double>(work_area.h);
  }

  const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(display);
  if (mode != nullptr) {
    if (mode->pixel_density > 0.0F) {
      output.scale_factor = static_cast<double>(mode->pixel_density);
      if (output.width > 0.0 && output.height > 0.0) {
        output.physical_width = std::round(output.width * output.scale_factor);
        output.physical_height = std::round(output.height * output.scale_factor);
      }
    }
    if (mode->refresh_rate > 0.0F) {
      output.refresh_rate = static_cast<double>(mode->refresh_rate);
    }
    const SDL_PixelFormatDetails* pixel_format = SDL_GetPixelFormatDetails(mode->format);
    if (pixel_format != nullptr && pixel_format->bits_per_pixel > 0) {
      output.color_depth = static_cast<double>(pixel_format->bits_per_pixel);
      output.pixel_depth = output.color_depth;
      const auto component_depth = std::min({pixel_format->Rbits, pixel_format->Gbits, pixel_format->Bbits});
      if (component_depth > 0) output.depth_per_component = static_cast<double>(component_depth);
    }
  }

  set_orientation(output, SDL_GetCurrentDisplayOrientation(display));
  const SDL_PropertiesID properties = SDL_GetDisplayProperties(display);
  if (properties != 0) {
    output.is_hdr =
        SDL_GetBooleanProperty(properties, SDL_PROP_DISPLAY_HDR_ENABLED_BOOLEAN, false);
  }
  const char* label = SDL_GetDisplayName(display);
  if (label != nullptr) output.label = flight::String(label);
}

} // namespace

flight::types::ScreenQueryBackend SdkScreenBackend::query_backend() const {
  flight::types::ScreenQueryBackend result;
  result.entity_runtime_key = std::nullopt;
  result.destroy = std::nullopt;
  result.get_screens = [](flight::Array<flight::Ref<flight::types::ScreenInfo>> output) {
    output.clear();
    int count = 0;
    SDL_DisplayID* displays = SDL_GetDisplays(&count);
    const SDL_DisplayID primary = SDL_GetPrimaryDisplay();
    for (int index = 0; displays != nullptr && index < count; ++index) {
      auto screen = flight::make_ref<flight::types::ScreenInfo>();
      fill_screen(displays[index], primary, *screen);
      output.push(std::move(screen));
    }
    SDL_free(displays);
    return output;
  };
  result.get_primary_screen = [](flight::Ref<flight::types::ScreenInfo> output) {
    if (output == nullptr) return output;
    const SDL_DisplayID primary = SDL_GetPrimaryDisplay();
    fill_screen(primary, primary, *output);
    return output;
  };
  result.get_cursor_position = [](flight::Ref<flight::types::x_y> output) {
    if (output == nullptr) return output;
    float x = 0.0F;
    float y = 0.0F;
    SDL_GetGlobalMouseState(&x, &y);
    output->x = static_cast<double>(x);
    output->y = static_cast<double>(y);
    return output;
  };
  return result;
}

flight::types::ScreenDetailsBackend SdkScreenBackend::details_backend() const {
  flight::types::ScreenDetailsBackend result;
  result.entity_runtime_key = std::nullopt;
  result.query_permission = [] {
    return flight::Task<flight::types::ScreenPermissionState>::resolve(flight::String("granted"));
  };
  result.request = [] { return flight::Task<bool>::resolve(true); };
  return result;
}

} // namespace flight::host_sdl
