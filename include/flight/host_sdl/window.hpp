#pragma once

#include <string>
#include <string_view>

#include <SDL3/SDL_video.h>

#include <flight/host_sdl/export.hpp>

namespace flight::host_sdl {

enum class GraphicsApi { none, open_gl, vulkan };

enum class OpenGlProfile { core, compatibility, es };

struct OpenGlOptions {
  int major_version{3};
  int minor_version{0};
  OpenGlProfile profile{OpenGlProfile::es};
  int red_bits{8};
  int green_bits{8};
  int blue_bits{8};
  int alpha_bits{8};
  int depth_bits{24};
  int stencil_bits{8};
  int multisample_buffers{0};
  int multisample_samples{0};
  bool double_buffer{true};
  bool debug{false};
  bool forward_compatible{false};
};

struct WindowOptions {
  std::string title{"Flight"};
  int width{1280};
  int height{720};
  GraphicsApi graphics_api{GraphicsApi::none};
  OpenGlOptions open_gl{};
  SDL_WindowFlags extra_flags{0};
  bool resizable{true};
  bool high_pixel_density{true};
  bool hidden{false};
};

struct WindowSize {
  int width{};
  int height{};

  [[nodiscard]] friend constexpr bool operator==(WindowSize, WindowSize) noexcept = default;
};

class FLIGHT_HOST_SDL_API Window final {
 public:
  explicit Window(const WindowOptions& options = {});
  ~Window() noexcept;

  Window(const Window&) = delete;
  Window& operator=(const Window&) = delete;
  Window(Window&& other) noexcept;
  Window& operator=(Window&& other) noexcept;

  [[nodiscard]] SDL_Window* native_handle() const noexcept;
  [[nodiscard]] SDL_WindowID id() const;
  [[nodiscard]] GraphicsApi graphics_api() const noexcept;
  [[nodiscard]] WindowSize size() const;
  [[nodiscard]] WindowSize pixel_size() const;

  void set_title(std::string_view title);
  void show();
  void hide();

 private:
  void reset() noexcept;

  SDL_Window* window_{nullptr};
  GraphicsApi graphics_api_{GraphicsApi::none};
};

} // namespace flight::host_sdl
