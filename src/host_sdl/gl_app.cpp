#include <flight/host_sdl/gl_app.hpp>

#include "detail.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <thread>

#include <SDL3/SDL_keycode.h>

namespace flight::host_sdl {

GlApplication::GlApplication(const GlApplicationOptions& options)
    : canvas_(GlCanvas::create(
          options.width, options.height, 1.0, String(options.title), options.hidden)),
      context_(canvas_.get_context()) {
  context_.make_current();
}

GlCanvas& GlApplication::canvas() noexcept { return canvas_; }
const GlCanvas& GlApplication::canvas() const noexcept { return canvas_; }
const WebGl2Context& GlApplication::context() const noexcept { return context_; }

void GlApplication::resize(int width, int height) {
  if (width <= 0 || height <= 0) {
    throw std::invalid_argument("SDL GL window dimensions must be positive");
  }
  detail::require_sdl(
      SDL_SetWindowSize(canvas_.native_window(), width, height), "SDL_SetWindowSize");
}

void GlApplication::present() const {
  context_.make_current();
  context_.present();
}

void GlApplication::request_shutdown() noexcept { stop_requested_ = true; }

std::size_t GlApplication::run(
    FrameCallback frame, const GlLoopOptions& options, EventCallback event_callback) {
  if (running_) throw std::logic_error("SDL GL frame loop is already running");
  if (!frame) throw std::invalid_argument("SDL GL frame callback is empty");
  if (!std::isfinite(options.maximum_delta_time) || options.maximum_delta_time <= 0 ||
      (options.fixed_delta_time &&
       (!std::isfinite(*options.fixed_delta_time) || *options.fixed_delta_time <= 0)) ||
      options.frame_delay.count() < 0) {
    throw std::invalid_argument("SDL GL frame timing must be finite and positive");
  }
  running_ = true;
  stop_requested_ = false;
  struct Reset final {
    bool& running;
    ~Reset() { running = false; }
  } reset{running_};

  std::size_t frames = 0;
  double elapsed = 0;
  auto previous = Host::ticks_nanoseconds();
  std::optional<WindowSize> previous_logical, previous_pixels;
  const auto window_id = SDL_GetWindowID(canvas_.native_window());
  while (!stop_requested_ &&
         (options.maximum_frames == 0 || frames < options.maximum_frames)) {
    SDL_Event event{};
    while (host_.poll_event(event)) {
      if (event.type == SDL_EVENT_QUIT ||
          (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == window_id) ||
          (options.close_on_escape && event.type == SDL_EVENT_KEY_DOWN &&
           event.key.windowID == window_id && event.key.key == SDLK_ESCAPE)) {
        request_shutdown();
      }
      if (event_callback) event_callback(event);
    }
    if (stop_requested_) break;
    const auto now = Host::ticks_nanoseconds();
    const double delta = options.fixed_delta_time.value_or(
        std::min(static_cast<double>(now - previous) / 1'000'000'000.0,
                 options.maximum_delta_time));
    previous = now;
    const auto logical = canvas_.logical_size();
    const auto pixels = canvas_.pixel_size();
    canvas_.width = static_cast<double>(logical.width);
    canvas_.height = static_cast<double>(logical.height);
    context_.make_current();
    frame(GlFrame{delta, elapsed, frames, logical, pixels,
                  previous_logical != logical || previous_pixels != pixels});
    present();
    previous_logical = logical;
    previous_pixels = pixels;
    elapsed += delta;
    ++frames;
    if (!stop_requested_ &&
        (options.maximum_frames == 0 || frames < options.maximum_frames) &&
        options.frame_delay.count() > 0) {
      std::this_thread::sleep_for(options.frame_delay);
    }
  }
  return frames;
}

} // namespace flight::host_sdl
