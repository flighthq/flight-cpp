#pragma once

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include <flight/host_sdl/export.hpp>
#include <flight/host_sdl/input.hpp>
#include <flight/host_sdl/web_platform_types.hpp>
#include <flight/host_sdl/webgl.hpp>
#include <flight/string.hpp>

namespace flight::host_sdl {

template <typename Event>
class EventListenerCollection final {
 private:
  struct Listener final {
    std::uint64_t id;
    String type;
    std::function<void(Event)> callback;
    bool once;
    std::function<void()> abort_callback;
  };

  struct State final {
    std::mutex mutex;
    std::uint64_t next_id{1};
    std::vector<std::shared_ptr<Listener>> listeners;
  };

 public:
  EventListenerCollection() : state_(std::make_shared<State>()) {}

  void add(
      String type,
      std::function<void(Event)> callback,
      const EventListenerOptions& options = {}) {
    if (!callback) throw std::invalid_argument("DOM event callback cannot be empty");
    if (options.signal && static_cast<bool>(options.signal->aborted)) return;

    std::shared_ptr<Listener> listener;
    {
      const std::scoped_lock lock(state_->mutex);
      if (state_->next_id == std::numeric_limits<std::uint64_t>::max()) {
        throw std::length_error("DOM event listener identity space is exhausted");
      }
      listener = std::make_shared<Listener>(Listener{
          .id = state_->next_id++,
          .type = std::move(type),
          .callback = std::move(callback),
          .once = options.once.value_or(false),
          .abort_callback = {},
      });
      state_->listeners.push_back(listener);
    }

    if (!options.signal) return;
    const std::weak_ptr<State> weak_state = state_;
    const auto id = listener->id;
    listener->abort_callback = [weak_state, id] {
      const auto state = weak_state.lock();
      if (!state) return;
      const std::scoped_lock lock(state->mutex);
      std::erase_if(state->listeners, [id](const auto& candidate) {
        return candidate->id == id;
      });
    };
    options.signal->add_event_listener(
        String("abort"), listener->abort_callback, AbortEventListenerOptions{.once = true});
    if (static_cast<bool>(options.signal->aborted)) listener->abort_callback();
  }

  void emit(const String& type, Event event) const {
    std::vector<std::uint64_t> dispatch;
    {
      const std::scoped_lock lock(state_->mutex);
      dispatch.reserve(state_->listeners.size());
      for (const auto& listener : state_->listeners) {
        if (listener->type == type) dispatch.push_back(listener->id);
      }
    }

    for (const auto id : dispatch) {
      std::function<void(Event)> callback;
      {
        const std::scoped_lock lock(state_->mutex);
        const auto found = std::ranges::find_if(state_->listeners, [id](const auto& listener) {
          return listener->id == id;
        });
        if (found == state_->listeners.end()) continue;
        callback = (*found)->callback;
        if ((*found)->once) state_->listeners.erase(found);
      }
      callback(event);
    }
  }

  void clear() const {
    const std::scoped_lock lock(state_->mutex);
    state_->listeners.clear();
  }

 private:
  std::shared_ptr<State> state_;
};

using SimpleEventListenerCollection = EventListenerCollection<std::nullptr_t>;

// Small browser-shaped application shell for upstream examples. Rendering stays in Flight's
// generated render-gl packages; these types only represent document attachment, event registration,
// and frame scheduling at the native host boundary.
class FLIGHT_HOST_SDL_GL_API DomElement {
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

  void add_event_listener(
      const String& type,
      std::function<void()> callback,
      const EventListenerOptions& options = {});

  template <typename Event>
  void add_event_listener(
      const String& type,
      std::function<void(Event)> callback,
      const EventListenerOptions& options) {
    listeners_.add(
        type,
        [callback = std::move(callback)](std::nullptr_t) mutable { callback(Event{}); },
        options);
  }

  template <typename Event>
  void add_event_listener(const String& type, std::function<void(Event)> callback) {
    add_event_listener(type, std::move(callback), {});
  }

  template <typename Callback, typename Options>
    requires std::invocable<Callback&>
  void add_event_listener(const String& type, Callback callback, const Options& options) {
    add_event_listener(
        type,
        std::function<void()>(std::move(callback)),
        static_cast<const EventListenerOptions&>(options));
  }

  [[nodiscard]] ClientRect get_bounding_client_rect() const noexcept;
  void click();

 private:
  SimpleEventListenerCollection listeners_;
};

using HtmlElement = DomElement;
using HtmlButtonElement = DomElement;
using HtmlDetailsElement = DomElement;
using HtmlDivElement = DomElement;
using HtmlHeadingElement = DomElement;
using HtmlInputElement = DomElement;
using HtmlLabelElement = DomElement;
using HtmlOptionElement = DomElement;
using HtmlParagraphElement = DomElement;
using HtmlSelectElement = DomElement;
using HtmlSpanElement = DomElement;
using HtmlStyleElement = DomElement;

// document.createElement has a tag-dependent TypeScript return type, while generated C++ retains
// `auto` for the call. This facade therefore exposes the common DOM surface and lazily materializes
// an SDL GL canvas when canvas-only operations are reached.
class FLIGHT_HOST_SDL_GL_API CreatedElement final : public DomElement {
 public:
  explicit CreatedElement(String tag);

  [[nodiscard]] operator GlCanvas() const;
  [[nodiscard]] WebGl2Context get_context() const;
  [[nodiscard]] WebGl2Context get_context(const String& context_id) const;
  [[nodiscard]] std::optional<WebGl2Context> get_context(
      const String& context_id,
      const WebGlContextAttributes& attributes) const;
  void set_pointer_capture(double pointer_id) const;
  void release_pointer_capture(double pointer_id) const;

 private:
  [[nodiscard]] GlCanvas& require_canvas() const;

  String tag_;
  mutable std::optional<GlCanvas> canvas_;
};

class FLIGHT_HOST_SDL_GL_API Document final {
 public:
  DomElement body;
  DomElement head;
  bool hidden{false};

  [[nodiscard]] CreatedElement create_element(String tag) const;
  [[nodiscard]] std::optional<DomElement> get_element_by_id(const String&) const;
  [[nodiscard]] bool has_focus() const noexcept;
  void add_event_listener(
      const String& type,
      std::function<void()> callback,
      const EventListenerOptions& options = {});
  void emit(const String& type);
  void set_focus(bool focused) noexcept;
  void set_hidden(bool next_hidden);

 private:
  bool focused_{true};
  SimpleEventListenerCollection listeners_;
};

class FLIGHT_HOST_SDL_GL_API WindowFacade final {
 public:
  double device_pixel_ratio{1.0};
  bool flight_capture{false};

  void add_event_listener(
      const String& type,
      std::function<void()> callback,
      const EventListenerOptions& options = {});
  void add_event_listener(
      const String& type,
      std::function<void(InputKeyboardData)> callback,
      const EventListenerOptions& options = {});
  void emit(const String& type) const;
  void emit_keyboard(const String& type, InputKeyboardData event) const;
  void clear_event_listeners();

 private:
  SimpleEventListenerCollection listeners_;
  EventListenerCollection<InputKeyboardData> keyboard_listeners_;
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
