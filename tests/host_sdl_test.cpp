#include <flight/host/timers.hpp>
#include <flight/host_sdl/audio.hpp>
#include <flight/host_sdl/cursor.hpp>
#include <flight/host_sdl/host.hpp>
#include <flight/host_sdl/input.hpp>
#include <flight/host_sdl/sdk_audio.hpp>
#include <flight/host_sdl/sdk_clipboard.hpp>
#include <flight/host_sdl/sdk_cursor.hpp>
#include <flight/host_sdl/sdk_device.hpp>
#include <flight/host_sdl/sdk_haptics.hpp>
#include <flight/host_sdl/sdk_keyboard.hpp>
#include <flight/host_sdl/sdk_platform.hpp>
#include <flight/host_sdl/sdk_screen.hpp>
#include <flight/host_sdl/sdk_window.hpp>
#include <flight/host_sdl/web_platform.hpp>
#include <flight/host_sdl/webgl.hpp>
#include <flight/host_sdl/wgpu.hpp>
#include <flight/host_sdl/window.hpp>
#include <flight/weak_map.hpp>
#include <flight/types/audio_device_backend.hpp>
#include <flight/types/application_visibility_backend.hpp>
#include <flight/types/application_window_target_backend.hpp>
#include <flight/types/clipboard.hpp>
#include <flight/types/cursor.hpp>
#include <flight/types/device.hpp>
#include <flight/types/fullscreen_backend.hpp>
#include <flight/types/haptics.hpp>
#include <flight/types/input_target_backend.hpp>
#include <flight/types/keyboard.hpp>
#include <flight/types/platform.hpp>
#include <flight/types/screen.hpp>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_mouse.h>

#include <array>
#include <chrono>
#include <limits>
#include <stdexcept>
#include <thread>
#include <utility>

namespace {

void expect(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}

struct SurfaceState {
  int creates{};
  int destroys{};
};

struct WgpuObjectState {
  int releases{};
};

void release_wgpu_object(void* userdata, void* object) noexcept {
  auto* state = static_cast<WgpuObjectState*>(userdata);
  if (object == state) ++state->releases;
}

flight::host_sdl::WgpuSurfaceHandle create_surface(
    void* userdata,
    flight::host_sdl::WgpuInstanceHandle instance,
    SDL_Window* window) {
  auto* state = static_cast<SurfaceState*>(userdata);
  expect(instance == state, "WGPU bridge changed the native instance handle");
  expect(window != nullptr, "WGPU bridge did not provide the SDL window");
  ++state->creates;
  return state;
}

void destroy_surface(
    void* userdata,
    flight::host_sdl::WgpuInstanceHandle instance,
    flight::host_sdl::WgpuSurfaceHandle surface) noexcept {
  auto* state = static_cast<SurfaceState*>(userdata);
  if (instance == state && surface == state) ++state->destroys;
}

} // namespace

int main() {
  flight::host_sdl::SdlAudioDeviceBackend audio;
  const auto audio_device = audio.create_device(48'000);
  expect(static_cast<bool>(audio_device), "SDL audio device was not created");
  expect(audio.get_device_time({}) == 0.0, "invalid SDL audio device reported time");

  std::array<float, 48> tone_samples{};
  for (std::size_t i = 0; i < tone_samples.size(); ++i) {
    tone_samples[i] = static_cast<float>(i) / static_cast<float>(tone_samples.size());
  }
  const std::array<std::span<const float>, 1> tone_channels{
      std::span<const float>(tone_samples)};
  const auto audio_buffer = audio.create_buffer(
      audio_device,
      1,
      tone_samples.size(),
      48'000,
      tone_channels);
  expect(static_cast<bool>(audio_buffer), "SDL audio buffer was not created");
  expect(
      !audio.create_buffer(audio_device, 3, tone_samples.size(), 48'000, tone_channels),
      "SDL audio accepted an unsupported channel layout");
  const auto audio_source = audio.create_source(audio_device, audio_buffer);
  const auto second_audio_source = audio.create_source(audio_device, audio_buffer);
  const auto cancelled_audio_source = audio.create_source(audio_device, audio_buffer);
  expect(
      audio_source && second_audio_source && cancelled_audio_source,
      "SDL audio sources were not created");
  audio.destroy_buffer(audio_buffer);
  expect(
      !audio.create_source(audio_device, audio_buffer),
      "SDL audio created a source from a destroyed buffer handle");
  const auto callback_thread = std::this_thread::get_id();
  int audio_completions = 0;
  const auto completed = [&] {
    expect(
        std::this_thread::get_id() == callback_thread,
        "SDL audio completion escaped the pumping thread");
    ++audio_completions;
  };
  audio.on_source_ended(audio_source, completed);
  audio.on_source_ended(second_audio_source, completed);
  audio.on_source_ended(cancelled_audio_source, [&] { audio_completions += 100; });
  audio.set_source_gain(audio_source, 0.5);
  audio.set_source_pan(audio_source, -0.25);
  audio.start_source(audio_source, 0.0, 0.0);
  audio.set_source_playback_rate(audio_source, 2.0);
  audio.set_source_pan(second_audio_source, 0.75);
  audio.start_source(second_audio_source, 0.0, 0.0005);
  audio.start_source(cancelled_audio_source, 1.0, 0.0);
  audio.stop_source(cancelled_audio_source);
  expect(audio.pump() == 0, "SDL audio delivered a stopped source completion");
  audio.resume_device(audio_device);
  for (int attempt = 0; attempt < 100 && audio_completions < 2; ++attempt) {
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    static_cast<void>(audio.pump());
  }
  expect(audio_completions == 2, "SDL audio did not complete concurrent acquired-buffer sources");
  expect(audio.pump() == 0, "SDL audio delivered a completion more than once");
  expect(audio.get_device_time(audio_device) > 0.0, "SDL audio device clock did not advance");
  audio.destroy_source(audio_source);
  audio.destroy_source(second_audio_source);
  audio.destroy_source(cancelled_audio_source);
  audio.destroy_device(audio_device);
  expect(audio.get_device_time(audio_device) == 0.0, "destroyed SDL audio device reported time");

  flight::host_sdl::SdkAudioDeviceBackend sdk_audio;
  const auto sdk_audio_copy = sdk_audio;
  auto sdk_backend = sdk_audio.backend();
  const auto sdk_device = sdk_backend.create_device(48'000);
  expect(sdk_device != 0.0, "SDL SDK audio adapter did not create a device");
  const flight::Float32Array sdk_samples{0.0F, 0.25F, -0.25F, 0.0F};
  const flight::Array<flight::Float32Array> sdk_channels{sdk_samples};
  const auto sdk_buffer = sdk_backend.create_buffer(
      sdk_device,
      1.0,
      static_cast<double>(sdk_samples.size()),
      48'000,
      sdk_channels);
  expect(sdk_buffer != 0.0, "SDL SDK audio adapter did not copy a Flight Float32Array");
  const auto sdk_source = sdk_backend.create_source(sdk_device, sdk_buffer);
  expect(sdk_source != 0.0, "SDL SDK audio adapter did not create a source");
  int sdk_audio_completions = 0;
  sdk_backend.on_source_ended(sdk_source, [&] { ++sdk_audio_completions; });
  sdk_backend.start_source(sdk_source, 1.0, 0.0);
  expect(
      sdk_audio_copy.pump() == 1 && sdk_audio_completions == 1,
      "SDL SDK audio adapter did not share or pump its native backend");
  sdk_backend.destroy_source(sdk_source);
  sdk_backend.destroy_buffer(sdk_buffer);
  sdk_backend.destroy_device(sdk_device);

  flight::host_sdl::reset_web_platform();
  expect(
      flight::host_sdl::keyboard_location_standard == 0.0 &&
          flight::host_sdl::keyboard_location_left == 1.0 &&
          flight::host_sdl::keyboard_location_right == 2.0 &&
          flight::host_sdl::keyboard_location_numpad == 3.0,
      "SDL host keyboard locations do not match the DOM constants");
  expect(
      flight::host_sdl::wheel_delta_pixel == 0.0 &&
          flight::host_sdl::wheel_delta_line == 1.0 &&
          flight::host_sdl::wheel_delta_page == 2.0,
      "SDL host wheel modes do not match the DOM constants");
  flight::host_sdl::DomEvent browser_event;
  browser_event.alt_key = true;
  browser_event.caps_lock = true;
  browser_event.key = flight::String("A");
  browser_event.pointer_id = 7.0;
  browser_event.prevent_default();
  const auto browser_keyboard = static_cast<flight::host_sdl::InputKeyboardData>(browser_event);
  const auto browser_pointer = static_cast<flight::host_sdl::InputPointerData>(browser_event);
  expect(
      browser_keyboard.alt_key && browser_keyboard.default_prevented &&
          browser_keyboard.get_modifier_state(flight::String("CapsLock")) &&
          browser_keyboard.key == flight::String("A"),
      "SDL DOM event lost keyboard fields during narrowing");
  expect(
      browser_pointer.pointer_id == 7.0 && browser_pointer.default_prevented &&
          browser_pointer.get_coalesced_events().empty(),
      "SDL DOM event lost pointer fields during narrowing");
  struct NativeCustomDetail final : flight::ReferenceEnabled {
    flight::String pressure;
  };
  const auto native_detail = flight::make_ref<NativeCustomDetail>();
  native_detail->pressure = flight::String("firm");
  browser_event.detail = native_detail;
  const auto custom_event =
      static_cast<flight::host_sdl::DomCustomEvent<decltype(native_detail)>>(browser_event);
  expect(
      custom_event.detail->pressure == flight::String("firm"),
      "SDL DOM event lost custom detail during narrowing");
  int frame_calls = 0;
  const auto cancelled_frame = flight::host_sdl::request_animation_frame([&] { frame_calls += 100; });
  flight::host_sdl::cancel_animation_frame(cancelled_frame);
  static_cast<void>(flight::host_sdl::request_animation_frame([&](double timestamp) {
    expect(timestamp == 12.5, "animation frame timestamp changed");
    ++frame_calls;
    static_cast<void>(flight::host_sdl::request_animation_frame([&] { ++frame_calls; }));
  }));
  expect(flight::host_sdl::pump_animation_frame(12.5) == 1 && frame_calls == 1,
         "animation frame turn did not preserve cancellation or callback ordering");
  expect(flight::host_sdl::pump_animation_frame(13.5) == 1 && frame_calls == 2,
         "animation frame scheduled during a callback ran in the same turn");

  auto element = static_cast<flight::host_sdl::DomElement>(
      flight::host_sdl::document.create_element(flight::String("button")));
  int click_calls = 0;
  element.add_event_listener(flight::String("click"), [&] { ++click_calls; });
  element.click();
  expect(click_calls == 1, "SDL document shell lost a registered listener");

  int removed_click_calls = 0;
  flight::Function<void()> removable_click = [&] { ++removed_click_calls; };
  element.add_event_listener(flight::String("click"), removable_click);
  element.add_event_listener(flight::String("click"), removable_click);
  const auto captured_click = removable_click;
  removable_click = nullptr;
  element.remove_event_listener(flight::String("click"), captured_click);
  element.click();
  expect(
      removed_click_calls == 0,
      "SDL element did not deduplicate and remove a copied identity-preserving listener");

  int captured_click_calls = 0;
  const flight::Function<void()> captured_listener = [&] { ++captured_click_calls; };
  flight::host_sdl::EventListenerOptions capture_options;
  capture_options.capture = true;
  element.add_event_listener(flight::String("click"), captured_listener, capture_options);
  element.remove_event_listener(flight::String("click"), captured_listener);
  element.click();
  element.remove_event_listener(flight::String("click"), captured_listener, true);
  element.click();
  expect(
      captured_click_calls == 1,
      "SDL listener removal did not distinguish the DOM capture flag");

  auto created_panel = flight::host_sdl::document.create_element(flight::String("div"));
  auto created_label = flight::host_sdl::document.create_element(flight::String("span"));
  created_panel.class_name = flight::String("controls");
  created_label.text_content = flight::String("Flight");
  const auto appended_label = created_panel.append_child(created_label);
  const flight::host_sdl::HtmlDivElement panel = created_panel;
  expect(
      panel.class_name == flight::String("controls") &&
          appended_label.text_content == flight::String("Flight"),
      "SDL createElement facade lost tag-specific DOM fields or appendChild values");
  created_panel.insert_adjacent_html(flight::String("beforeend"), flight::String("<span>A</span>"));
  created_panel.insert_adjacent_html(flight::String("afterbegin"), flight::String("<span>B</span>"));
  expect(
      created_panel.inner_html == flight::String("<span>B</span><span>A</span>") &&
          created_panel.query_selector_all(flight::String("[data-input-overlay]")).empty(),
      "SDL DOM shell changed adjacent markup order or retained nonexistent query results");
  created_panel.remove();
  expect(created_panel.inner_html.empty(), "SDL DOM shell remove retained element content");

  int once_calls = 0;
  flight::host_sdl::EventListenerOptions once_options;
  once_options.once = true;
  element.add_event_listener(
      flight::String("click"), [&] { ++once_calls; }, once_options);
  element.click();
  element.click();
  expect(once_calls == 1, "SDL document shell repeated a once listener");

  flight::AbortController event_abort;
  int aborted_listener_calls = 0;
  flight::host_sdl::EventListenerOptions abort_options;
  abort_options.signal = event_abort.signal;
  element.add_event_listener(
      flight::String("click"),
      [&] { ++aborted_listener_calls; },
      abort_options);
  event_abort.abort();
  element.click();
  expect(aborted_listener_calls == 0, "SDL document shell retained an aborted listener");

  int visibility_calls = 0;
  flight::host_sdl::document.add_event_listener(
      flight::String("visibilitychange"), [&] { ++visibility_calls; });
  flight::host_sdl::document.set_focus(false);
  flight::host_sdl::document.set_hidden(true);
  flight::host_sdl::document.set_hidden(true);
  expect(
      flight::host_sdl::document.hidden && !flight::host_sdl::document.has_focus() &&
          visibility_calls == 1,
      "SDL document shell lost visibility or focus state");
  flight::host_sdl::document.set_hidden(false);
  flight::host_sdl::document.set_focus(true);

  int removed_visibility_calls = 0;
  flight::Function<void()> visibility_listener = [&] { ++removed_visibility_calls; };
  flight::host_sdl::document.add_event_listener(
      flight::String("visibilitychange"), visibility_listener);
  const auto captured_visibility_listener = visibility_listener;
  visibility_listener = nullptr;
  flight::host_sdl::document.remove_event_listener(
      flight::String("visibilitychange"), captured_visibility_listener);
  flight::host_sdl::document.set_hidden(true);
  expect(
      removed_visibility_calls == 0,
      "SDL document retained a copied identity-preserving listener after removal");
  flight::host_sdl::document.set_hidden(false);

  int window_lifecycle_calls = 0;
  flight::host_sdl::window.add_event_listener(
      flight::String("pagehide"), [&] { ++window_lifecycle_calls; });
  flight::host_sdl::window.emit(flight::String("pagehide"));
  expect(window_lifecycle_calls == 1, "SDL window shell lost a lifecycle listener");

  int removed_window_calls = 0;
  flight::Function<void()> removable_window = [&] { ++removed_window_calls; };
  flight::host_sdl::window.add_event_listener(
      flight::String("pagehide"), removable_window);
  const auto captured_window = removable_window;
  removable_window = nullptr;
  flight::host_sdl::window.remove_event_listener(
      flight::String("pagehide"), captured_window);
  flight::host_sdl::window.emit(flight::String("pagehide"));
  expect(
      removed_window_calls == 0,
      "SDL window retained a copied identity-preserving listener after removal");

  int window_key_calls = 0;
  flight::host_sdl::window.add_event_listener(
      flight::String("keydown"),
      [&](flight::host_sdl::InputKeyboardData event) {
        expect(event.key == flight::String("Enter"), "SDL window changed a keyboard event");
        ++window_key_calls;
      });
  flight::host_sdl::InputKeyboardData synthetic_key;
  synthetic_key.key = flight::String("Enter");
  flight::host_sdl::window.emit_keyboard(flight::String("keydown"), synthetic_key);
  expect(window_key_calls == 1, "SDL window shell did not deliver a keyboard event");

  int removed_window_key_calls = 0;
  flight::Function<void(flight::host_sdl::InputKeyboardData)> removable_window_key =
      [&](flight::host_sdl::InputKeyboardData) { ++removed_window_key_calls; };
  flight::host_sdl::window.add_event_listener(
      flight::String("keydown"), removable_window_key);
  const auto captured_window_key = removable_window_key;
  removable_window_key = nullptr;
  flight::host_sdl::window.remove_event_listener(
      flight::String("keydown"), captured_window_key);
  flight::host_sdl::window.emit_keyboard(flight::String("keydown"), synthetic_key);
  expect(
      removed_window_key_calls == 0,
      "SDL window retained a copied identity-preserving keyboard listener after removal");

  const flight::host_sdl::GlAnisotropyExtension anisotropy;
  expect(anisotropy.texture_max_anisotropy_ext == 0x84FE,
         "GL anisotropy texture parameter changed");
  expect(anisotropy.max_texture_max_anisotropy_ext == 0x84FF,
         "GL anisotropy maximum query changed");

  auto pixels = flight::Uint8ClampedArray{255, 0, 0, 255, 0, 255, 0, 255};
  auto image = flight::host_sdl::ImageSource::rgba8(
      2,
      1,
      std::move(pixels),
      flight::host_sdl::ImageSourceKind::image_bitmap);
  const auto weak_image = image.weaken();
  flight::host_sdl::GlImageSource image_alias = image;
  flight::host_sdl::WgpuExternalImageSource wgpu_image = image;
  expect(
      image.kind() == flight::host_sdl::ImageSourceKind::image_bitmap && image.width() == 2 &&
          image.height() == 1,
      "SDL image source kind or dimensions changed");
  expect(image.rgba8_pixels().size() == 8, "SDL image pixel storage changed");
  expect(
      image.identity() == image_alias.identity() && image.identity() == wgpu_image.identity(),
      "GL and WebGPU image views did not preserve shared host identity");
  wgpu_image = {};
  flight::WeakMap<
      flight::host_sdl::GlImageSource,
      int,
      flight::host_sdl::GlImageSourceWeakPolicy>
      image_cache;
  image_cache.set(image, 7);
  expect(image_cache.get(image) == 7, "SDL image weak cache lost a live entry");
  image = {};
  expect(flight::host_sdl::GlImageSource::lock_weak(weak_image).has_value(),
         "SDL image alias did not retain host identity");
  image_alias = {};
  expect(!flight::host_sdl::GlImageSource::lock_weak(weak_image).has_value(),
         "SDL image weak identity retained expired storage");

  flight::host_sdl::Host host;
  expect((host.subsystems() & SDL_INIT_VIDEO) != 0, "SDL video subsystem was not recorded");

  flight::host_sdl::SdkClipboardTextBackend sdk_clipboard;
  auto clipboard_backend = sdk_clipboard.backend();
  expect(
      clipboard_backend.write_text(flight::String("Flight clipboard \u2603")).get(),
      "SDL SDK clipboard did not accept UTF-8 text");
  expect(clipboard_backend.has_text().get(), "SDL SDK clipboard lost written text");
  expect(
      clipboard_backend.read_text().get() == flight::String("Flight clipboard \u2603"),
      "SDL SDK clipboard changed written UTF-8 text");
  expect(clipboard_backend.clear().get(), "SDL SDK clipboard clear failed");
  expect(
      !clipboard_backend.has_text().get() && clipboard_backend.read_text().get().empty(),
      "SDL SDK clipboard retained text after clear");

  flight::host_sdl::SdkPlatformBackend sdk_platform;
  auto platform_backend = sdk_platform.backend();
  auto platform_info = flight::make_ref<flight::types::PlatformInfo>();
  expect(
      platform_backend.get_info(platform_info) == platform_info,
      "SDL SDK platform backend replaced its caller-owned output");
  const bool known_platform =
      platform_info->name == flight::String("windows") ||
      platform_info->name == flight::String("macos") ||
      platform_info->name == flight::String("linux") ||
      platform_info->name == flight::String("ios") ||
      platform_info->name == flight::String("android");
  expect(
      known_platform || platform_info->name == flight::String("unknown"),
      "SDL SDK platform returned a name outside Flight's closed domain");
  expect(
      platform_info->runtime == flight::String("native") &&
          platform_info->engine == flight::String("unknown") &&
          (platform_info->endianness == flight::String("little") ||
           platform_info->endianness == flight::String("big") ||
           platform_info->endianness == flight::String("unknown")) &&
          (platform_info->pointer_width == 32.0 || platform_info->pointer_width == 64.0),
      "SDL SDK platform lost native runtime, byte-order, or pointer-width facts");
  expect(
      !platform_backend.get_info(nullptr),
      "SDL SDK platform manufactured output for a null caller-owned record");

  flight::host_sdl::SdkHapticsBackend sdk_haptics;
  const auto sdk_haptics_copy = sdk_haptics;
  auto haptics_backend = sdk_haptics.backend();
  auto haptics_capabilities = flight::make_ref<flight::types::HapticsCapabilities>();
  expect(
      haptics_backend.capabilities(haptics_capabilities) == haptics_capabilities,
      "SDL SDK haptics replaced its caller-owned capabilities output");
  const bool haptics_supported = haptics_backend.is_supported();
  expect(
      haptics_capabilities->supported == haptics_supported &&
          haptics_capabilities->amplitude_control == haptics_supported &&
          haptics_capabilities->intensity == haptics_supported &&
          !haptics_capabilities->custom_events && !haptics_capabilities->patterns &&
          sdk_haptics_copy.backend().is_supported() == haptics_supported,
      "SDL SDK haptics capabilities disagree with its selected rumble device");
  expect(
      haptics_backend.prepare.has_value() &&
          !haptics_backend.vibrate_pattern(flight::Array<double>{10.0, 10.0}) &&
          !haptics_backend.vibrate_waveform.has_value() && !haptics_backend.capabilities(nullptr),
      "SDL SDK haptics advertised an unsupported waveform or null output");
  (*haptics_backend.prepare)();
  if (!haptics_supported) {
    expect(
        !haptics_backend.cancel() && !haptics_backend.impact(flight::String("light"), 1.0) &&
            !haptics_backend.notification(flight::String("success")) &&
            !haptics_backend.selection() && !haptics_backend.vibrate(10.0),
        "SDL SDK haptics accepted an operation without a rumble device");
  }

  flight::host_sdl::SdkScreenBackend sdk_screen;
  auto screen_query = sdk_screen.query_backend();
  flight::Array<flight::Ref<flight::types::ScreenInfo>> screens;
  screens.push(flight::make_ref<flight::types::ScreenInfo>());
  const void* screen_array_identity = screens.identity();
  const auto returned_screens = screen_query.get_screens(screens);
  expect(
      returned_screens.identity() == screen_array_identity && !screens.empty(),
      "SDL SDK screen query replaced or emptied its caller-owned array");
  for (const auto& screen : screens) {
    expect(
        screen != nullptr && screen->id > 0.0 && screen->width > 0.0 && screen->height > 0.0 &&
            screen->work_width > 0.0 && screen->work_height > 0.0 &&
            screen->scale_factor > 0.0 && screen->physical_width > 0.0 &&
            screen->physical_height > 0.0 && screen->dpi == -1.0,
        "SDL SDK screen query returned invalid display geometry or sentinels");
  }
  auto primary_screen = flight::make_ref<flight::types::ScreenInfo>();
  expect(
      screen_query.get_primary_screen(primary_screen) == primary_screen &&
          primary_screen->is_primary && primary_screen->id > 0.0,
      "SDL SDK screen query did not fill its caller-owned primary display");
  using CursorPositionRef = decltype(screen_query.get_cursor_position)::result_type;
  auto cursor_position = flight::make_ref<typename CursorPositionRef::element_type>();
  expect(
      screen_query.get_cursor_position(cursor_position) == cursor_position,
      "SDL SDK screen query replaced its caller-owned cursor position");
  expect(
      !screen_query.get_primary_screen(nullptr) && !screen_query.get_cursor_position(nullptr),
      "SDL SDK screen query manufactured output for a null caller-owned record");
  auto screen_details = sdk_screen.details_backend();
  expect(
      screen_details.query_permission().get() == flight::String("granted") &&
          screen_details.request().get(),
      "SDL SDK screen details introduced a permission gate for native enumeration");
  int screen_change_calls = 0;
  flight::String screen_change_kind;
  double changed_screen_id = 0.0;
  bool screen_change_had_metrics = false;
  auto release_screen_change = sdk_screen.change_backend().subscribe([&](auto event) {
    ++screen_change_calls;
    screen_change_kind = event->kind;
    changed_screen_id = event->screen->id;
    screen_change_had_metrics = event->changed_metrics.has_value();
  });
  SDL_Event screen_change_event{};
  screen_change_event.type = SDL_EVENT_DISPLAY_CURRENT_MODE_CHANGED;
  screen_change_event.display.displayID = static_cast<SDL_DisplayID>(primary_screen->id);
  expect(
      sdk_screen.dispatch(screen_change_event),
      "SDL SDK screen backend rejected a native display event");
  expect(
      screen_change_calls == 1 && screen_change_kind == flight::String("ScreenMetricsChanged") &&
          changed_screen_id == primary_screen->id && screen_change_had_metrics,
      "SDL SDK screen backend changed its generated metrics event");
  release_screen_change();
  release_screen_change();
  expect(
      sdk_screen.dispatch(screen_change_event) && screen_change_calls == 1,
      "released SDL screen subscription was invoked");
  screen_change_event.type = SDL_EVENT_SYSTEM_THEME_CHANGED;
  expect(!sdk_screen.dispatch(screen_change_event), "SDL screen backend accepted a foreign event");

  flight::host_sdl::SdlCursorBackend native_cursor;
  const auto native_cursor_copy = native_cursor;
  native_cursor.set_cursor(flight::String("pointer"));
  expect(
      native_cursor_copy.current_cursor() == flight::String("pointer") && SDL_CursorVisible(),
      "SDL cursor copies did not share a visible pointer selection");
  native_cursor_copy.set_cursor(flight::String("none"));
  expect(
      native_cursor.current_cursor() == flight::String("none") && !SDL_CursorVisible(),
      "SDL cursor did not implement the CSS none selection");
  native_cursor.set_cursor(std::nullopt);
  expect(
      !native_cursor_copy.current_cursor().has_value() && SDL_CursorVisible(),
      "SDL cursor did not restore the host default");

  flight::host_sdl::SdkCursorBackend sdk_cursor;
  const auto sdk_cursor_copy = sdk_cursor;
  auto sdk_cursor_backend = sdk_cursor.backend();
  sdk_cursor_backend.set_cursor(flight::String("ew-resize"));
  expect(
      sdk_cursor_copy.current_cursor() == flight::String("ew-resize"),
      "SDL SDK cursor record did not retain the generated cursor value");
  sdk_cursor_backend.set_cursor(std::nullopt);
  expect(
      !sdk_cursor.current_cursor().has_value(),
      "SDL SDK cursor record did not clear to the host default");

  flight::host_sdl::WindowOptions options;
  options.title = "Flight host test";
  options.width = 320;
  options.height = 180;
  options.hidden = true;
  options.resizable = false;
  options.high_pixel_density = false;
  flight::host_sdl::Window window(options);

  expect(window.native_handle() != nullptr, "SDL window was not created");
  expect(window.id() != 0, "SDL window has no id");
  expect(window.graphics_api() == flight::host_sdl::GraphicsApi::none, "wrong graphics API");
  expect(window.size() == flight::host_sdl::WindowSize{320, 180}, "logical window size changed");
  expect(window.pixel_size().width > 0 && window.pixel_size().height > 0, "pixel size is empty");
  window.set_title("Flight host test renamed");

  flight::host_sdl::SdkSoftKeyboardBackend sdk_soft_keyboard(window);
  auto soft_keyboard_info_backend = sdk_soft_keyboard.info_backend();
  auto soft_keyboard_info = flight::make_ref<flight::types::SoftKeyboardInfo>();
  expect(
      soft_keyboard_info_backend.get_info(soft_keyboard_info) == soft_keyboard_info &&
          soft_keyboard_info->height == 0.0 && soft_keyboard_info->x == 0.0 &&
          soft_keyboard_info->y == 0.0 && soft_keyboard_info->width == 0.0,
      "SDL SDK soft-keyboard backend replaced its output or invented geometry");
  expect(
      !soft_keyboard_info_backend.get_info(nullptr),
      "SDL SDK soft-keyboard backend manufactured a null info output");

  int soft_keyboard_changes = 0;
  auto soft_keyboard_subscription =
      sdk_soft_keyboard.change_backend().subscribe([&] { ++soft_keyboard_changes; }).get();
  SDL_Event soft_keyboard_event{};
  soft_keyboard_event.type = SDL_EVENT_SCREEN_KEYBOARD_SHOWN;
  expect(
      sdk_soft_keyboard.dispatch(soft_keyboard_event),
      "SDL SDK soft-keyboard backend rejected a native visibility event");
  if (SDL_HasScreenKeyboardSupport()) {
    expect(
        soft_keyboard_subscription->result == flight::types::soft_keyboard_attach_ok_kind &&
            soft_keyboard_subscription->unsubscribe.has_value() && soft_keyboard_changes == 1,
        "SDL SDK soft-keyboard backend did not attach its supported change subscription");
    (*soft_keyboard_subscription->unsubscribe)();
    (*soft_keyboard_subscription->unsubscribe)();
    expect(
        sdk_soft_keyboard.dispatch(soft_keyboard_event),
        "SDL SDK soft-keyboard backend stopped recognizing visibility events");
    expect(soft_keyboard_changes == 1, "released SDL soft-keyboard subscription was invoked");
  } else {
    expect(
        soft_keyboard_subscription->result ==
                flight::types::soft_keyboard_attach_acquisition_failed_kind &&
            !soft_keyboard_subscription->unsubscribe.has_value() && soft_keyboard_changes == 0,
        "SDL SDK soft-keyboard backend advertised an unavailable subscription");
  }
  auto soft_keyboard_visibility = sdk_soft_keyboard.visibility_backend();
  const auto show_keyboard = soft_keyboard_visibility.show().get();
  expect(
      show_keyboard == flight::types::soft_keyboard_visibility_ok_kind ||
          show_keyboard == flight::types::soft_keyboard_visibility_operation_failed_kind,
      "SDL SDK soft-keyboard show returned an outcome outside Flight's domain");
  const auto hide_keyboard = soft_keyboard_visibility.hide().get();
  expect(
      hide_keyboard == flight::types::soft_keyboard_visibility_ok_kind ||
          hide_keyboard == flight::types::soft_keyboard_visibility_operation_failed_kind,
      "SDL SDK soft-keyboard hide returned an outcome outside Flight's domain");

  flight::host_sdl::SdkDeviceBackend sdk_device_snapshot(window);
  auto device_backend = sdk_device_snapshot.backend();
  auto device_capabilities = flight::make_ref<flight::types::DeviceCapabilities>();
  expect(
      device_backend.get_capabilities(device_capabilities) == device_capabilities,
      "SDL SDK device backend replaced its caller-owned capability output");
  expect(!device_capabilities->has_stylus, "SDL SDK device claimed an unenumerated stylus");

  auto display_metrics = flight::make_ref<flight::types::DeviceDisplayMetrics>();
  expect(
      device_backend.get_display_metrics(display_metrics) == display_metrics,
      "SDL SDK device backend replaced its caller-owned display output");
  expect(
      display_metrics->logical_width > 0.0 && display_metrics->logical_height > 0.0 &&
          display_metrics->physical_width > 0.0 && display_metrics->physical_height > 0.0 &&
          display_metrics->pixel_ratio > 0.0 && display_metrics->density_dpi == -1.0,
      "SDL SDK device did not report its display dimensions or DPI sentinel");

  auto device_info = flight::make_ref<flight::types::DeviceInfo>();
  expect(
      device_backend.get_info(device_info) == device_info,
      "SDL SDK device backend replaced its caller-owned info output");
  expect(
      (device_info->cpu_cores > 0.0 || device_info->cpu_cores == -1.0) &&
          (device_info->total_memory > 0.0 || device_info->total_memory == -1.0) &&
          device_info->available_memory == -1.0 && device_info->font_scale == -1.0 &&
          device_info->supported_abis.empty(),
      "SDL SDK device lost native system facts or unknown-value sentinels");
  expect(
      device_info->form_factor == flight::types::device_form_factor_desktop ||
          device_info->form_factor == flight::types::device_form_factor_unknown,
      "SDL SDK device returned a form factor outside its native mapping");
  expect(device_backend.get_id().empty(), "SDL SDK device manufactured a durable install id");

  auto safe_area = flight::make_ref<flight::types::SafeAreaInsets>();
  expect(
      device_backend.get_safe_area_insets(safe_area) == safe_area && safe_area->top >= 0.0 &&
          safe_area->right >= 0.0 && safe_area->bottom >= 0.0 && safe_area->left >= 0.0,
      "SDL SDK device returned invalid safe-area insets");
  expect(
      !device_backend.get_capabilities(nullptr) && !device_backend.get_display_metrics(nullptr) &&
          !device_backend.get_info(nullptr) && !device_backend.get_safe_area_insets(nullptr),
      "SDL SDK device manufactured output for a null caller-owned record");

  flight::host_sdl::SdkWindowBackend sdk_window(window);
  const auto sdk_window_copy = sdk_window;
  auto visibility_backend = sdk_window.visibility_backend();
  expect(!visibility_backend.is_visible(), "hidden SDL window reported visible to Flight");
  window.show();
  expect(
      sdk_window_copy.visibility_backend().is_visible(),
      "SDL SDK window copies did not share a visible window");
  window.hide();
  expect(!visibility_backend.is_visible(), "hidden SDL window remained visible to Flight");
  window.show();

  auto fullscreen_backend = sdk_window.fullscreen_backend();
  const auto fullscreen_target = sdk_window_copy.fullscreen_target();
  expect(
      fullscreen_target->brand == flight::String("FullscreenTargetHandle"),
      "SDL fullscreen target lost its generated brand");
  const auto foreign_fullscreen_target =
      flight::make_ref<flight::types::FullscreenTargetHandle>();
  foreign_fullscreen_target->brand = flight::String("FullscreenTargetHandle");
  expect(
      !fullscreen_backend.request(foreign_fullscreen_target).get(),
      "SDL fullscreen accepted an unregistered Flight target");
  const bool entered_fullscreen = fullscreen_backend.request(fullscreen_target).get();
  if (entered_fullscreen) {
    expect(
        (SDL_GetWindowFlags(window.native_handle()) & SDL_WINDOW_FULLSCREEN) != 0,
        "SDL fullscreen request succeeded without changing the native window");
    expect(fullscreen_backend.exit().get(), "SDL fullscreen exit failed after a successful request");
    expect(
        (SDL_GetWindowFlags(window.native_handle()) & SDL_WINDOW_FULLSCREEN) == 0,
        "SDL fullscreen exit left the native window fullscreen");
  }

  const auto input_target = sdk_window.input_target();
  expect(
      input_target->brand == flight::String("InputTargetHandle"),
      "SDL input target lost its generated brand");
  auto input_target_backend = sdk_window.input_target_backend();
  input_target_backend.prepare(input_target);
  input_target_backend.prepare(flight::make_ref<flight::types::InputTargetHandle>());

  int sdk_focus_calls = 0;
  int sdk_blur_calls = 0;
  auto focus_backend = sdk_window.input_focus_backend();
  auto release_focus = focus_backend.subscribe(
      input_target,
      [&] { ++sdk_focus_calls; },
      [&] { ++sdk_blur_calls; });
  SDL_Event sdk_focus_event{};
  sdk_focus_event.type = SDL_EVENT_WINDOW_FOCUS_GAINED;
  sdk_focus_event.window.windowID = window.id();
  expect(sdk_window.dispatch(sdk_focus_event), "SDL SDK window rejected its focus event");
  sdk_focus_event.type = SDL_EVENT_WINDOW_FOCUS_LOST;
  expect(sdk_window_copy.dispatch(sdk_focus_event), "SDL SDK window rejected its blur event");
  expect(
      sdk_focus_calls == 1 && sdk_blur_calls == 1,
      "SDL SDK focus backend did not route generated callbacks");
  release_focus();
  expect(sdk_window.dispatch(sdk_focus_event), "released SDL focus event was not recognized");
  expect(sdk_blur_calls == 1, "released SDL focus subscription was invoked");

  flight::String dropped_path;
  auto drop_file_backend = sdk_window.input_drop_file_backend();
  auto release_drop_file = drop_file_backend.subscribe(
      input_target,
      [&](flight::String path) { dropped_path = std::move(path); });
  SDL_Event sdk_drop_event{};
  sdk_drop_event.type = SDL_EVENT_DROP_FILE;
  sdk_drop_event.drop.windowID = window.id();
  sdk_drop_event.drop.data = "/tmp/flight.txt";
  expect(sdk_window.dispatch(sdk_drop_event), "SDL SDK window rejected its file drop event");
  expect(
      dropped_path == flight::String("/tmp/flight.txt"),
      "SDL SDK file-drop backend changed the native path");
  release_drop_file();
  dropped_path = flight::String();
  expect(sdk_window.dispatch(sdk_drop_event), "released SDL file-drop event was not recognized");
  expect(dropped_path.empty(), "released SDL file-drop subscription was invoked");
  sdk_drop_event.drop.windowID = window.id() + 1;
  expect(!sdk_window.dispatch(sdk_drop_event), "SDL SDK window accepted a foreign drop event");

  auto pointer_lock_backend = sdk_window.input_pointer_lock_backend();
  const auto missing_pointer_lock = pointer_lock_backend
                                        .request(flight::make_ref<flight::types::InputTargetHandle>())
                                        .get();
  expect(
      missing_pointer_lock->reason == flight::String("target-not-found"),
      "SDL pointer lock accepted an unregistered Flight target");
  const auto pointer_lock = pointer_lock_backend.request(input_target).get();
  expect(
      pointer_lock->reason == flight::String("ok") ||
          pointer_lock->reason == flight::String("operation-failed"),
      "SDL pointer lock returned an outcome outside its generated contract");
  if (pointer_lock->reason == flight::String("ok")) {
    expect(
        SDL_GetWindowRelativeMouseMode(window.native_handle()),
        "SDL pointer-lock success did not enable relative mouse mode");
  }
  const auto pointer_unlock = pointer_lock_backend.exit().get();
  expect(
      pointer_unlock->reason == flight::String("ok"),
      "SDL pointer-lock exit did not return the generated success outcome");

  flight::types::ApplicationVisibilityBackend expired_visibility;
  flight::types::DeviceBackend expired_device;
  flight::types::FullscreenBackend expired_fullscreen;
  flight::types::SoftKeyboardInfoBackend expired_keyboard_info;
  flight::types::SoftKeyboardVisibilityBackend expired_keyboard_visibility;
  flight::Ref<flight::types::FullscreenTargetHandle> expired_target;
  {
    flight::host_sdl::WindowOptions transient_options = options;
    transient_options.title = "Flight transient SDK target";
    flight::host_sdl::Window transient_window(transient_options);
    flight::host_sdl::SdkDeviceBackend transient_sdk_device(transient_window);
    flight::host_sdl::SdkSoftKeyboardBackend transient_sdk_keyboard(transient_window);
    flight::host_sdl::SdkWindowBackend transient_sdk_window(transient_window);
    expired_device = transient_sdk_device.backend();
    expired_visibility = transient_sdk_window.visibility_backend();
    expired_fullscreen = transient_sdk_window.fullscreen_backend();
    expired_keyboard_info = transient_sdk_keyboard.info_backend();
    expired_keyboard_visibility = transient_sdk_keyboard.visibility_backend();
    expired_target = transient_sdk_window.fullscreen_target();
  }
  expect(!expired_visibility.is_visible(), "destroyed SDL window remained visible to Flight");
  auto expired_soft_keyboard = flight::make_ref<flight::types::SoftKeyboardInfo>();
  expired_keyboard_info.get_info(expired_soft_keyboard);
  expect(
      !expired_soft_keyboard->visible && expired_keyboard_visibility.show().get() ==
                                              flight::types::soft_keyboard_visibility_operation_failed_kind,
      "destroyed SDL window retained soft-keyboard visibility operations");
  auto expired_metrics = flight::make_ref<flight::types::DeviceDisplayMetrics>();
  expired_device.get_display_metrics(expired_metrics);
  expect(
      expired_metrics->logical_width == -1.0 && expired_metrics->logical_height == -1.0 &&
          expired_metrics->physical_width == -1.0 && expired_metrics->physical_height == -1.0 &&
          expired_metrics->pixel_ratio == -1.0,
      "destroyed SDL window retained generated display metrics");
  expect(
      !expired_fullscreen.request(expired_target).get() && !expired_fullscreen.exit().get(),
      "destroyed SDL window accepted a fullscreen operation");

  flight::host_sdl::InputKeyboardData keyboard_data;
  flight::host_sdl::InputPointerData pointer_data;
  flight::host_sdl::InputTextData text_data;
  int keyboard_calls = 0;
  int pointer_calls = 0;
  int wheel_calls = 0;
  int text_calls = 0;
  int gamepad_axis_calls = 0;
  int gamepad_down_calls = 0;
  int gamepad_up_calls = 0;
  flight::host_sdl::InputGamepadAxisData gamepad_axis_data;
  flight::host_sdl::InputGamepadButtonData gamepad_button_data;
  flight::host_sdl::InputSink input_sink;
  input_sink.key_down = [&](const auto& data) {
    keyboard_data = data;
    ++keyboard_calls;
  };
  input_sink.pointer_move = [&](const auto& data) {
    pointer_data = data;
    ++pointer_calls;
  };
  input_sink.wheel = [&](const auto& data) {
    pointer_data = data;
    ++wheel_calls;
  };
  input_sink.text_input = [&](const auto& data) {
    text_data = data;
    ++text_calls;
  };
  input_sink.gamepad_axis_move = [&](const auto& data) {
    gamepad_axis_data = data;
    ++gamepad_axis_calls;
  };
  input_sink.gamepad_button_down = [&](const auto& data) {
    gamepad_button_data = data;
    ++gamepad_down_calls;
  };
  input_sink.gamepad_button_up = [&](const auto& data) {
    gamepad_button_data = data;
    ++gamepad_up_calls;
  };
  flight::host_sdl::InputDispatcher input(window.id(), std::move(input_sink));

  SDL_Event key_event{};
  key_event.type = SDL_EVENT_KEY_DOWN;
  key_event.key.timestamp = 2'000'000;
  key_event.key.windowID = window.id();
  key_event.key.scancode = SDL_SCANCODE_A;
  key_event.key.key = SDLK_A;
  key_event.key.mod = SDL_KMOD_LSHIFT;
  key_event.key.repeat = true;
  expect(input.dispatch(key_event), "SDL key event was not translated");
  expect(
      keyboard_calls == 1 && keyboard_data.key_code == SDLK_A && keyboard_data.shift_key &&
          keyboard_data.modifier == SDL_KMOD_LSHIFT && keyboard_data.repeat &&
          keyboard_data.time_stamp == 2.0,
      "SDL key translation changed Flight keyboard semantics");

  SDL_Event motion_event{};
  motion_event.type = SDL_EVENT_MOUSE_MOTION;
  motion_event.motion.timestamp = 3'000'000;
  motion_event.motion.windowID = window.id();
  motion_event.motion.which = 7;
  motion_event.motion.state = SDL_BUTTON_LMASK | SDL_BUTTON_RMASK;
  motion_event.motion.x = 11.0F;
  motion_event.motion.y = 13.0F;
  motion_event.motion.xrel = 2.0F;
  motion_event.motion.yrel = -3.0F;
  expect(input.dispatch(motion_event), "SDL pointer event was not translated");
  expect(
      pointer_calls == 1 && pointer_data.buttons == 3.0 && pointer_data.x == 11.0 &&
          pointer_data.y == 13.0 && pointer_data.delta_x == 2.0 && pointer_data.delta_y == -3.0,
      "SDL pointer translation changed Flight pointer semantics");

  SDL_Event wheel_event{};
  wheel_event.type = SDL_EVENT_MOUSE_WHEEL;
  wheel_event.wheel.timestamp = 4'000'000;
  wheel_event.wheel.windowID = window.id();
  wheel_event.wheel.x = 2.0F;
  wheel_event.wheel.y = 3.0F;
  wheel_event.wheel.direction = SDL_MOUSEWHEEL_NORMAL;
  wheel_event.wheel.mouse_x = 17.0F;
  wheel_event.wheel.mouse_y = 19.0F;
  expect(input.dispatch(wheel_event), "SDL wheel event was not translated");
  expect(
      wheel_calls == 1 && pointer_data.delta_x == 2.0 && pointer_data.delta_y == -3.0 &&
          pointer_data.wheel_mode == flight::String("lines"),
      "SDL wheel translation changed Flight wheel semantics");

  SDL_Event text_event{};
  text_event.type = SDL_EVENT_TEXT_INPUT;
  text_event.text.windowID = window.id();
  text_event.text.text = "Flight";
  expect(input.dispatch(text_event), "SDL text event was not translated");
  expect(
      text_calls == 1 && !text_data.is_composing && text_data.text == flight::String("Flight"),
      "SDL text translation changed Flight text semantics");

  SDL_Event foreign_event = key_event;
  foreign_event.key.windowID = window.id() + 1;
  expect(!input.dispatch(foreign_event), "SDL input accepted an event from another window");

  SDL_Event gamepad_axis_event{};
  gamepad_axis_event.type = SDL_EVENT_GAMEPAD_AXIS_MOTION;
  gamepad_axis_event.gaxis.timestamp = 5'000'000;
  gamepad_axis_event.gaxis.which = 4;
  gamepad_axis_event.gaxis.axis = static_cast<Uint8>(SDL_GAMEPAD_AXIS_LEFTX);
  gamepad_axis_event.gaxis.value = std::numeric_limits<Sint16>::max();
  expect(input.dispatch(gamepad_axis_event), "SDL gamepad axis was not translated");
  expect(
      gamepad_axis_calls == 1 && gamepad_axis_data.axis == 0.0 &&
          gamepad_axis_data.gamepad == 4.0 && gamepad_axis_data.value == 1.0,
      "SDL gamepad axis changed Flight standard mapping");

  gamepad_axis_event.gaxis.axis = static_cast<Uint8>(SDL_GAMEPAD_AXIS_LEFT_TRIGGER);
  expect(input.dispatch(gamepad_axis_event), "SDL gamepad trigger press was not translated");
  expect(
      gamepad_down_calls == 1 && gamepad_button_data.button == 6.0 &&
          gamepad_button_data.value == 1.0,
      "SDL gamepad trigger changed Flight standard button mapping");
  expect(!input.dispatch(gamepad_axis_event), "unchanged SDL trigger emitted a duplicate transition");
  gamepad_axis_event.gaxis.value = 0;
  expect(input.dispatch(gamepad_axis_event), "SDL gamepad trigger release was not translated");
  expect(gamepad_up_calls == 1 && gamepad_button_data.button == 6.0,
         "SDL gamepad trigger release changed Flight standard mapping");

  SDL_Event gamepad_button_event{};
  gamepad_button_event.type = SDL_EVENT_GAMEPAD_BUTTON_DOWN;
  gamepad_button_event.gbutton.which = 4;
  gamepad_button_event.gbutton.button = static_cast<Uint8>(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
  gamepad_button_event.gbutton.down = true;
  expect(input.dispatch(gamepad_button_event), "SDL gamepad button was not translated");
  expect(gamepad_down_calls == 2 && gamepad_button_data.button == 4.0,
         "SDL gamepad button changed Flight standard mapping");

  const auto gamepads = flight::host_sdl::navigator.get_gamepads();
  expect(gamepads.size() == 1 && gamepads[0].has_value(),
         "SDL navigator did not expose the translated gamepad");
  const auto& gamepad = *gamepads[0];
  expect(
      gamepad.index == 4.0 && gamepad.mapping == flight::String("standard") &&
          gamepad.axes[0] == 1.0 && gamepad.buttons[4].pressed &&
          gamepad.buttons[4].touched && !gamepad.buttons[6].pressed &&
          !gamepad.buttons[6].touched,
      "SDL navigator gamepad snapshot changed standard axes or buttons");
  gamepad.axes[0] = -1.0;
  expect((*flight::host_sdl::navigator.get_gamepads()[0]).axes[0] == 1.0,
         "SDL navigator exposed mutable shared snapshot storage");

  SDL_Event gamepad_removed_event{};
  gamepad_removed_event.type = SDL_EVENT_GAMEPAD_REMOVED;
  gamepad_removed_event.gdevice.which = 4;
  static_cast<void>(input.dispatch(gamepad_removed_event));
  expect(flight::host_sdl::navigator.get_gamepads().empty(),
         "SDL navigator retained a removed gamepad");

  SDL_Event received{};
  while (host.poll_event(received)) {}
  SDL_Event event{};
  event.type = SDL_EVENT_USER;
  expect(SDL_PushEvent(&event), "could not enqueue an SDL event");
  bool received_user_event = false;
  for (int attempt = 0; attempt < 10 && !received_user_event; ++attempt) {
    received_user_event = host.wait_event_for(received, std::chrono::milliseconds(50)) &&
                          received.type == SDL_EVENT_USER;
  }
  expect(received_user_event, "SDL user event timed out");
  expect(flight::host_sdl::Host::ticks_nanoseconds() > 0, "SDL monotonic clock did not advance");
  int timer_calls = 0;
  static_cast<void>(flight::host::set_timeout([&] { ++timer_calls; }, 0.0));
  expect(host.pump_timers() == 1 && timer_calls == 1,
         "SDL host loop did not pump the headless timer queue");

  SurfaceState surface_state;
  const flight::host_sdl::WgpuSurfaceCallbacks callbacks{
      &surface_state,
      create_surface,
      destroy_surface,
  };
  {
    flight::host_sdl::WgpuSurface surface(window, &surface_state, callbacks);
    expect(surface.native_handle() == &surface_state, "WGPU bridge changed the native surface handle");
    flight::host_sdl::WgpuSurface moved(std::move(surface));
    expect(surface.native_handle() == nullptr, "moved WGPU surface retained ownership");
    expect(moved.instance() == &surface_state, "moved WGPU surface lost its instance");
  }
  expect(surface_state.creates == 1, "WGPU bridge did not create exactly one surface");
  expect(surface_state.destroys == 1, "WGPU bridge did not destroy exactly one surface");

  WgpuObjectState object_state;
  flight::host_sdl::WgpuDevice::weak_type weak_device;
  {
    auto device = flight::host_sdl::WgpuDevice::adopt(
        &object_state,
        flight::host_sdl::WgpuObjectCallbacks{&object_state, release_wgpu_object});
    const auto alias = device;
    weak_device = device.weaken();
    expect(
        alias == device && device.identity() == alias.identity() &&
            device.native_handle() == &object_state &&
            flight::host_sdl::WgpuDeviceWeakPolicy::lock(weak_device).has_value(),
        "WGPU object carrier lost shared native identity");
  }
  expect(
      object_state.releases == 1 && !flight::host_sdl::WgpuDevice::lock_weak(weak_device),
      "WGPU object carrier did not release exactly once or expire its weak identity");
  const flight::host_sdl::WgpuBlendComponent blend_component{
      .src_factor = flight::String("one"),
      .dst_factor = flight::String("one-minus-src-alpha"),
      .operation = flight::String("add"),
  };
  const flight::host_sdl::WgpuBlendState blend_state{
      .color = blend_component,
      .alpha = blend_component,
  };
  const flight::host_sdl::WgpuStencilFaceState stencil_state{
      .compare = flight::String("always"),
      .pass_op = flight::String("replace"),
      .fail_op = std::nullopt,
      .depth_fail_op = std::nullopt,
  };
  const flight::host_sdl::WgpuSamplerDescriptor sampler_descriptor{
      .address_mode_u = flight::String("repeat"),
      .address_mode_v = std::nullopt,
      .address_mode_w = std::nullopt,
      .mag_filter = flight::String("linear"),
      .min_filter = std::nullopt,
      .mipmap_filter = std::nullopt,
      .lod_min_clamp = 1.0,
      .lod_max_clamp = std::nullopt,
      .compare = flight::String("less-equal"),
      .max_anisotropy = 8.0,
      .label = flight::String("material"),
  };
  expect(
      blend_state.color.dst_factor == flight::String("one-minus-src-alpha") &&
          stencil_state.pass_op == flight::String("replace") &&
          !stencil_state.depth_fail_op.has_value(),
      "WGPU pipeline dictionaries lost explicit values or omitted-member presence");
  expect(
      sampler_descriptor.address_mode_u == flight::String("repeat") &&
          !sampler_descriptor.address_mode_v.has_value() &&
          sampler_descriptor.mag_filter == flight::String("linear") &&
          !sampler_descriptor.min_filter.has_value() && sampler_descriptor.lod_min_clamp == 1.0 &&
          !sampler_descriptor.lod_max_clamp.has_value() &&
          sampler_descriptor.compare == flight::String("less-equal") &&
          sampler_descriptor.max_anisotropy == 8.0 &&
          sampler_descriptor.label == flight::String("material"),
      "WGPU sampler descriptors lost explicit values or omitted-member presence");
  const flight::host_sdl::WgpuBufferDescriptor buffer_descriptor{
      .size = 4096.0,
      .usage = flight::host_sdl::wgpu_buffer_usage_vertex,
      .mapped_at_creation = false,
      .label = flight::String("vertices"),
  };
  const flight::host_sdl::WgpuExtent3DDictionary extent_dictionary{
      .width = 64.0,
      .height = 32.0,
      .depth_or_array_layers = std::nullopt,
  };
  const flight::host_sdl::WgpuOrigin3DDictionary omitted_origin{};
  const flight::host_sdl::WgpuExternalImageSourceInfo external_source_info{};
  const flight::host_sdl::WgpuExternalImageDestinationInfo external_destination_info{};
  const flight::host_sdl::WgpuTexelCopyBufferLayout copy_layout{
      .offset = 16.0,
      .bytes_per_row = 256.0,
      .rows_per_image = std::nullopt,
  };
  const flight::host_sdl::WgpuTextureDescriptor texture_descriptor{
      .size = extent_dictionary,
      .mip_level_count = 4.0,
      .sample_count = std::nullopt,
      .dimension = flight::String("2d"),
      .format = flight::String("rgba8unorm"),
      .usage = flight::host_sdl::wgpu_texture_usage_texture_binding,
      .view_formats = flight::Iterable<flight::String>(
          flight::Array<flight::String>{flight::String("rgba8unorm-srgb")}),
      .texture_binding_view_dimension = std::nullopt,
      .label = flight::String("atlas"),
  };
  flight::host_sdl::WgpuExtent3D iterable_extent =
      flight::Array<double>{128.0, 64.0, 2.0};
  auto iterable_extent_values = std::get<flight::Iterable<double>>(iterable_extent);
  auto iterable_extent_iterator = iterable_extent_values.begin();
  const flight::SharedArrayBuffer shared_buffer(32.0);
  const flight::host_sdl::WgpuAllowSharedBufferSource shared_buffer_source = shared_buffer;
  const flight::Uint8Array upload_bytes(flight::Array<double>{1.0, 2.0, 3.0});
  const flight::ArrayBufferView upload_view(upload_bytes);
  const flight::host_sdl::WgpuAllowSharedBufferSource view_source = upload_view;
  expect(
      buffer_descriptor.size == 4096.0 && buffer_descriptor.mapped_at_creation == false &&
          copy_layout.bytes_per_row == 256.0 && !copy_layout.rows_per_image.has_value() &&
          !omitted_origin.x.has_value() && !omitted_origin.y.has_value() &&
          !omitted_origin.z.has_value() && !external_source_info.origin.has_value() &&
          !external_source_info.flip_y.has_value() &&
          !external_destination_info.mip_level.has_value() &&
          !external_destination_info.origin.has_value() &&
          !external_destination_info.aspect.has_value() &&
          !external_destination_info.color_space.has_value() &&
          !external_destination_info.premultiplied_alpha.has_value() &&
          std::get<flight::host_sdl::WgpuExtent3DDictionary>(texture_descriptor.size).width ==
              64.0 &&
          texture_descriptor.format == flight::String("rgba8unorm") &&
          iterable_extent_iterator != std::default_sentinel && *iterable_extent_iterator == 128.0 &&
          std::holds_alternative<flight::ArrayBufferLike>(shared_buffer_source) &&
          std::get<flight::ArrayBufferLike>(shared_buffer_source).kind() ==
              flight::ArrayBufferKind::shared_array_buffer &&
          std::get<flight::ArrayBufferLike>(shared_buffer_source).identity() ==
              shared_buffer.identity() &&
          std::holds_alternative<flight::ArrayBufferView>(view_source) &&
          std::get<flight::ArrayBufferView>(view_source).byte_length == 3 &&
          std::get<flight::ArrayBufferView>(view_source).identity() == upload_view.identity(),
      "WGPU transfer descriptors lost required values, optional presence, or iterable domains");
  WgpuObjectState binding_object_state;
  {
    const auto buffer = flight::host_sdl::WgpuBuffer::adopt(
        &binding_object_state,
        flight::host_sdl::WgpuObjectCallbacks{
            &binding_object_state, release_wgpu_object});
    const flight::host_sdl::WgpuBufferBinding buffer_binding{
        .buffer = buffer,
        .offset = 16.0,
        .size = 64.0,
    };
    const flight::host_sdl::WgpuBindGroupEntry binding_entry{
        .binding = 2.0,
        .resource = buffer_binding,
    };
    const flight::host_sdl::WgpuBufferBindingLayout buffer_layout{
        .type = flight::String("uniform"),
        .has_dynamic_offset = true,
        .min_binding_size = 64.0,
    };
    const flight::host_sdl::WgpuBindGroupLayoutEntry layout_entry{
        .binding = 2.0,
        .visibility = flight::host_sdl::wgpu_shader_stage_vertex,
        .buffer = buffer_layout,
        .sampler = std::nullopt,
        .texture = std::nullopt,
        .storage_texture = std::nullopt,
        .external_texture = std::nullopt,
    };
    const flight::host_sdl::WgpuStorageTextureBindingLayout storage_layout{
        .access = flight::String("write-only"),
        .format = flight::String("rgba8unorm"),
        .view_dimension = std::nullopt,
    };
    const flight::host_sdl::WgpuBindGroupDescriptor bind_group_descriptor{
        .layout = {},
        .entries = flight::Array<flight::host_sdl::WgpuBindGroupEntry>{binding_entry},
        .label = flight::String("material"),
    };
    const flight::host_sdl::WgpuBindGroupLayoutDescriptor layout_descriptor{
        .entries = flight::Array<flight::host_sdl::WgpuBindGroupLayoutEntry>{layout_entry},
        .label = flight::String("material-layout"),
    };
    auto bind_group_entries = bind_group_descriptor.entries.begin();
    auto layout_entries = layout_descriptor.entries.begin();
    expect(
        std::holds_alternative<flight::host_sdl::WgpuBufferBinding>(binding_entry.resource) &&
            std::get<flight::host_sdl::WgpuBufferBinding>(binding_entry.resource)
                    .buffer.identity() == buffer.identity() &&
            layout_entry.buffer->type == flight::String("uniform") &&
            layout_entry.buffer->has_dynamic_offset == true &&
            !layout_entry.texture.has_value() &&
            storage_layout.access == flight::String("write-only") &&
            !storage_layout.view_dimension.has_value() &&
            bind_group_entries != std::default_sentinel && bind_group_entries->binding == 2.0 &&
            bind_group_descriptor.label == flight::String("material") &&
            layout_entries != std::default_sentinel && layout_entries->visibility ==
                flight::host_sdl::wgpu_shader_stage_vertex &&
            layout_descriptor.label == flight::String("material-layout"),
        "WGPU bind-group descriptors lost resource identity or optional layout members");
  }
  expect(
      binding_object_state.releases == 1,
      "WGPU binding resources did not preserve provider-owned handle lifetime");
  const flight::host_sdl::WgpuVertexAttribute vertex_attribute{
      .format = flight::String("float32x3"),
      .offset = 0.0,
      .shader_location = 0.0,
  };
  const flight::host_sdl::WgpuVertexBufferLayout vertex_buffer_layout{
      .array_stride = 48.0,
      .step_mode = std::nullopt,
      .attributes = flight::Array<flight::host_sdl::WgpuVertexAttribute>{vertex_attribute},
  };
  const flight::host_sdl::WgpuVertexState vertex_state{
      .module = {},
      .entry_point = flight::String("vs_main"),
      .constants = std::nullopt,
      .buffers = flight::Array<std::optional<flight::host_sdl::WgpuVertexBufferLayout>>{
          vertex_buffer_layout},
  };
  const flight::host_sdl::WgpuColorTargetState color_target{
      .format = flight::String("bgra8unorm"),
      .blend = blend_state,
      .write_mask = flight::host_sdl::wgpu_color_write_all,
  };
  const flight::host_sdl::WgpuFragmentState fragment_state{
      .module = {},
      .entry_point = flight::String("fs_main"),
      .constants = std::nullopt,
      .targets = flight::Array<std::optional<flight::host_sdl::WgpuColorTargetState>>{
          color_target},
  };
  const flight::host_sdl::WgpuRenderPipelineDescriptor pipeline_descriptor{
      .layout = flight::String("auto"),
      .vertex = vertex_state,
      .fragment = fragment_state,
      .primitive = flight::host_sdl::WgpuPrimitiveState{
          .topology = flight::String("triangle-list"),
          .strip_index_format = std::nullopt,
          .front_face = flight::String("ccw"),
          .cull_mode = flight::String("back"),
          .unclipped_depth = std::nullopt,
      },
      .depth_stencil = flight::host_sdl::WgpuDepthStencilState{
          .format = flight::String("depth24plus-stencil8"),
          .depth_write_enabled = true,
          .depth_compare = flight::String("less"),
          .stencil_front = std::nullopt,
          .stencil_back = std::nullopt,
          .stencil_read_mask = std::nullopt,
          .stencil_write_mask = std::nullopt,
          .depth_bias = std::nullopt,
          .depth_bias_slope_scale = std::nullopt,
          .depth_bias_clamp = std::nullopt,
      },
      .multisample = flight::host_sdl::WgpuMultisampleState{
          .count = 4.0,
          .mask = std::nullopt,
          .alpha_to_coverage_enabled = false,
      },
      .label = flight::String("main-pipeline"),
  };
  auto vertex_buffers = pipeline_descriptor.vertex.buffers->begin();
  auto color_targets = pipeline_descriptor.fragment->targets.begin();
  expect(
      std::holds_alternative<flight::String>(pipeline_descriptor.layout) &&
          std::get<flight::String>(pipeline_descriptor.layout) == flight::String("auto") &&
          vertex_buffers != std::default_sentinel && vertex_buffers->has_value() &&
          (*vertex_buffers)->attributes.begin()->format == flight::String("float32x3") &&
          !(*vertex_buffers)->step_mode.has_value() && color_targets != std::default_sentinel &&
          color_targets->has_value() &&
          (*color_targets)->write_mask == flight::host_sdl::wgpu_color_write_all &&
          pipeline_descriptor.primitive->cull_mode == flight::String("back") &&
          pipeline_descriptor.depth_stencil->depth_compare == flight::String("less") &&
          pipeline_descriptor.multisample->count == 4.0 &&
          pipeline_descriptor.label == flight::String("main-pipeline"),
      "WGPU render-pipeline descriptors lost nested values, iterable slots, or optional presence");
  {
    flight::Set<flight::String> features;
    features.add(flight::String("timestamp-query"));
    const auto adapter = flight::host_sdl::WgpuAdapter::adopt(
        &object_state,
        flight::host_sdl::WgpuObjectCallbacks{&object_state, release_wgpu_object},
        std::move(features),
        flight::host_sdl::WgpuSupportedLimits{16'384.0});
    expect(
        adapter.features.has(flight::String("timestamp-query")) &&
            adapter.limits.max_texture_dimension2_d == 16'384.0,
        "WGPU adapter carrier lost provider capability metadata");
  }
  expect(object_state.releases == 2, "WGPU adapter did not release its provider handle");
  expect(
      flight::host_sdl::wgpu_buffer_usage_vertex == 0x20 &&
          flight::host_sdl::wgpu_texture_usage_render_attachment == 0x10 &&
          flight::host_sdl::wgpu_shader_stage_fragment == 0x2 &&
          flight::host_sdl::wgpu_color_write_all == 0xf &&
          flight::host_sdl::wgpu_map_mode_read == 0x1,
      "WGPU usage flags do not match the WebGPU constants");
}
