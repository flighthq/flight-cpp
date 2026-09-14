#include <flight/host/timers.hpp>
#include <flight/host_sdl/host.hpp>
#include <flight/host_sdl/input.hpp>
#include <flight/host_sdl/web_platform.hpp>
#include <flight/host_sdl/webgl.hpp>
#include <flight/host_sdl/wgpu.hpp>
#include <flight/host_sdl/window.hpp>
#include <flight/weak_map.hpp>

#include <SDL3/SDL_events.h>

#include <limits>
#include <stdexcept>
#include <utility>

namespace {

void expect(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}

struct SurfaceState {
  int creates{};
  int destroys{};
};

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
  flight::host_sdl::reset_web_platform();
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

  const flight::host_sdl::GlAnisotropyExtension anisotropy;
  expect(anisotropy.texture_max_anisotropy_ext == 0x84FE,
         "GL anisotropy texture parameter changed");
  expect(anisotropy.max_texture_max_anisotropy_ext == 0x84FF,
         "GL anisotropy maximum query changed");

  auto pixels = flight::Uint8ClampedArray{255, 0, 0, 255, 0, 255, 0, 255};
  auto image = flight::host_sdl::GlImageSource::rgba8(2, 1, std::move(pixels));
  const auto weak_image = image.weaken();
  auto image_alias = image;
  expect(image.width() == 2 && image.height() == 1, "GL image dimensions changed");
  expect(image.rgba8_pixels().size() == 8, "GL image pixel storage changed");
  expect(image.identity() == image_alias.identity(), "GL image copy changed host identity");
  flight::WeakMap<
      flight::host_sdl::GlImageSource,
      int,
      flight::host_sdl::GlImageSourceWeakPolicy>
      image_cache;
  image_cache.set(image, 7);
  expect(image_cache.get(image) == 7, "GL image weak cache lost a live entry");
  image = {};
  expect(flight::host_sdl::GlImageSource::lock_weak(weak_image).has_value(),
         "GL image alias did not retain host identity");
  image_alias = {};
  expect(!flight::host_sdl::GlImageSource::lock_weak(weak_image).has_value(),
         "GL image weak identity retained expired storage");

  flight::host_sdl::Host host;
  expect((host.subsystems() & SDL_INIT_VIDEO) != 0, "SDL video subsystem was not recorded");

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
}
