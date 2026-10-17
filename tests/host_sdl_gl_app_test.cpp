#include <flight/host_sdl/gl_app.hpp>

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {
void require(bool value, const char* message) {
  if (!value) throw std::runtime_error(message);
}
}

int main() {
  try {
    using namespace flight::host_sdl;
    GlApplication app({.title = "Flight installed GL app test", .width = 64, .height = 48, .hidden = true});
    const auto& gl = app.context();
    const auto id = SDL_GetWindowID(app.canvas().native_window());
    SDL_Event foreign_close{};
    foreign_close.type = SDL_EVENT_WINDOW_CLOSE_REQUESTED;
    foreign_close.window.windowID = id + 1;
    require(SDL_PushEvent(&foreign_close), "could not queue foreign-window event");
    bool forwarded = false, rejected_recursive = false;
    GlLoopOptions loop;
    loop.fixed_delta_time = 0.025;
    loop.maximum_frames = 3;
    loop.frame_delay = std::chrono::milliseconds(0);
    const auto frames = app.run([&](const GlFrame& frame) {
      require(frame.delta_time == 0.025 && std::abs(frame.elapsed_time - static_cast<double>(frame.index) * 0.025) < 1e-12,
              "fixed frame timing or index changed");
      if (frame.index == 0) {
        require(forwarded && frame.resized && frame.logical_size == WindowSize{64, 48},
                "first-frame sizes or event-before-frame ordering changed");
        app.resize(96, 64);
        try { app.run([](const GlFrame&) {}, loop); }
        catch (const std::logic_error&) { rejected_recursive = true; }
      } else {
        require(frame.resized && frame.logical_size == WindowSize{96, 64} &&
                    frame.pixel_size == app.canvas().pixel_size() &&
                    app.canvas().width == 96 && app.canvas().height == 64,
                "resized window did not reach the next frame and canvas");
        app.request_shutdown();
      }
      gl.viewport(0, 0, frame.pixel_size.width, frame.pixel_size.height);
      gl.clear_color(1, 0, 1, 1);
      gl.clear(gl.color_buffer_bit);
      flight::Uint8Array pixel(4U);
      gl.read_pixels(0, 0, 1, 1, gl.rgba, gl.unsigned_byte, pixel);
      require(gl.get_error() == gl.no_error && pixel[0] == 255 && pixel[1] == 0 && pixel[2] == 255,
              "frame callback did not render into the current real GL surface");
    }, loop, [&](const SDL_Event& event) {
      if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID != id) forwarded = true;
    });
    require(frames == 2 && rejected_recursive, "shutdown or recursive-loop guard changed");

    bool propagated = false;
    try { app.run([](const GlFrame&) { throw std::runtime_error("frame failure"); }, loop); }
    catch (const std::runtime_error& error) { propagated = std::string_view(error.what()) == "frame failure"; }
    require(propagated, "frame exception was swallowed");
    loop.maximum_frames = 1;
    require(app.run([](const GlFrame&) {}, loop) == 1, "exception left the loop running");
    SDL_Event close{};
    close.type = SDL_EVENT_WINDOW_CLOSE_REQUESTED;
    close.window.windowID = id;
    require(SDL_PushEvent(&close), "could not queue close event");
    require(app.run([](const GlFrame&) { throw std::runtime_error("rendered after window close"); }, loop) == 0,
            "own-window close did not stop before another frame");
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
