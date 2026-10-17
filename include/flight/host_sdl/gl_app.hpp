#pragma once

#include <chrono>
#include <cstddef>
#include <functional>
#include <optional>
#include <string>

#include <flight/host_sdl/host.hpp>
#include <flight/host_sdl/webgl.hpp>

namespace flight::host_sdl {

struct GlApplicationOptions {
  std::string title{"Flight"};
  int width{800};
  int height{600};
  bool hidden{false};
};

struct GlLoopOptions {
  double maximum_delta_time{0.1};
  std::optional<double> fixed_delta_time;
  // Zero runs until request_shutdown(), window close, Escape, or SDL quit.
  std::size_t maximum_frames{0};
  std::chrono::milliseconds frame_delay{8};
  bool close_on_escape{true};
};

struct GlFrame {
  double delta_time;
  double elapsed_time;
  std::size_t index;
  WindowSize logical_size;
  WindowSize pixel_size;
  // True on the first frame and whenever either window size changes.
  bool resized;
};

// An SDL video lifetime, GLES window and Flight WebGl2 surface. Use on the SDL
// main thread. No scene renderer or painter is selected by this host API.
class FLIGHT_HOST_SDL_GL_API GlApplication final {
 public:
  using FrameCallback = std::function<void(const GlFrame&)>;
  using EventCallback = std::function<void(const SDL_Event&)>;

  explicit GlApplication(const GlApplicationOptions& options = {});

  GlApplication(const GlApplication&) = delete;
  GlApplication& operator=(const GlApplication&) = delete;
  GlApplication(GlApplication&&) = delete;
  GlApplication& operator=(GlApplication&&) = delete;

  [[nodiscard]] GlCanvas& canvas() noexcept;
  [[nodiscard]] const GlCanvas& canvas() const noexcept;
  [[nodiscard]] const WebGl2Context& context() const noexcept;
  void resize(int width, int height);
  void present() const;
  void request_shutdown() noexcept;

  // Poll events, make the context current, call frame, then present once.
  // Event callbacks run before their frame. SDL quit or this window's close
  // request stops before another frame. Exceptions propagate after loop cleanup.
  // A completed/stopped loop can be run again; recursive run() is rejected.
  std::size_t run(
      FrameCallback frame,
      const GlLoopOptions& options = {},
      EventCallback event = {});

 private:
  Host host_;
  GlCanvas canvas_;
  WebGl2Context context_;
  bool running_{false};
  bool stop_requested_{false};
};

} // namespace flight::host_sdl
