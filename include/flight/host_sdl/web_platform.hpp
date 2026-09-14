#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <utility>
#include <vector>

#include <flight/host_sdl/export.hpp>
#include <flight/host_sdl/input.hpp>
#include <flight/host_sdl/web_platform_types.hpp>
#include <flight/host_sdl/webgl.hpp>
#include <flight/string.hpp>

namespace flight::host_sdl {

// Small browser-shaped application shell for upstream examples. Rendering stays in Flight's
// generated render-gl packages; these types only represent document attachment, event registration,
// and frame scheduling at the native host boundary.
class FLIGHT_HOST_SDL_GL_API DomElement final {
 public:
  String class_name;
  String id;
  String inner_html;
  String max;
  String min;
  String step;
  String text_content;
  String type;
  String value;
  DomStyle style;
  double height{0.0};
  double width{0.0};

  template <typename Child>
  Child append_child(Child child) {
    return child;
  }

  void add_event_listener(const String& type, std::function<void()> callback);

  template <typename Event>
  void add_event_listener(
      const String& type,
      std::function<void(Event)> callback,
      const EventListenerOptions&) {
    add_event_listener(type, std::move(callback));
  }

  template <typename Event>
  void add_event_listener(const String&, std::function<void(Event)>) {}

  template <typename Callback, typename Options>
  void add_event_listener(const String& type, Callback callback, const Options&) {
    add_event_listener(type, std::move(callback));
  }

  [[nodiscard]] ClientRect get_bounding_client_rect() const noexcept;
  void click();

 private:
  std::vector<std::pair<String, std::function<void()>>> listeners_;
};

using HtmlElement = DomElement;
using HtmlInputElement = DomElement;

class FLIGHT_HOST_SDL_GL_API CreatedElement final {
 public:
  explicit CreatedElement(String tag) : tag_(std::move(tag)) {}

  [[nodiscard]] operator DomElement() const;
  [[nodiscard]] operator GlCanvas() const;

 private:
  String tag_;
};

class FLIGHT_HOST_SDL_GL_API Document final {
 public:
  DomElement body;
  DomElement head;

  [[nodiscard]] CreatedElement create_element(String tag) const;
  [[nodiscard]] std::optional<DomElement> get_element_by_id(const String&) const;
  void add_event_listener(const String& type, std::function<void()> callback);
  void emit(const String& type);

 private:
  std::vector<std::pair<String, std::function<void()>>> listeners_;
};

class FLIGHT_HOST_SDL_GL_API WindowFacade final {
 public:
  double device_pixel_ratio{1.0};
  bool flight_capture{false};

  void add_event_listener(const String& type, std::function<void(InputKeyboardData)> callback);
  void emit_keyboard(const String& type, InputKeyboardData event) const;
  void clear_event_listeners();

 private:
  std::vector<std::pair<String, std::function<void(InputKeyboardData)>>> keyboard_listeners_;
};

// Persistent SDL-to-browser-shaped event bridge for one GlCanvas. Keeping one InputDispatcher
// preserves pointer button state, relative motion, and gamepad trigger transitions across events.
class FLIGHT_HOST_SDL_GL_API WebPlatformInput final {
 public:
  explicit WebPlatformInput(GlCanvas canvas);

  WebPlatformInput(const WebPlatformInput&) = delete;
  WebPlatformInput& operator=(const WebPlatformInput&) = delete;
  WebPlatformInput(WebPlatformInput&&) = delete;
  WebPlatformInput& operator=(WebPlatformInput&&) = delete;

  [[nodiscard]] bool dispatch(const SDL_Event& event);

 private:
  GlCanvas canvas_;
  InputDispatcher input_;
};

using AnimationFrameHandle = double;
using AnimationFrameCallback = std::function<void(double)>;

extern FLIGHT_HOST_SDL_GL_API Document document;
extern FLIGHT_HOST_SDL_GL_API WindowFacade window;

[[nodiscard]] FLIGHT_HOST_SDL_GL_API AnimationFrameHandle request_animation_frame(
    AnimationFrameCallback callback);
[[nodiscard]] FLIGHT_HOST_SDL_GL_API AnimationFrameHandle request_animation_frame(
    std::function<void()> callback);
FLIGHT_HOST_SDL_GL_API void cancel_animation_frame(AnimationFrameHandle handle);
[[nodiscard]] FLIGHT_HOST_SDL_GL_API std::size_t pump_animation_frame(double timestamp_ms);
FLIGHT_HOST_SDL_GL_API void reset_web_platform();

} // namespace flight::host_sdl
