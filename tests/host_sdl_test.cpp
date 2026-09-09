#include <flight/host_sdl/host.hpp>
#include <flight/host_sdl/wgpu.hpp>
#include <flight/host_sdl/window.hpp>

#include <SDL3/SDL_events.h>

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
