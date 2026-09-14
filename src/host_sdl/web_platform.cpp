#include <flight/host_sdl/web_platform.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace flight::host_sdl {
namespace {

struct PendingFrame final {
  AnimationFrameHandle handle;
  AnimationFrameCallback callback;
};

thread_local std::vector<PendingFrame> pending_frames;
thread_local std::uint64_t next_frame_handle{1};

bool same_string(const String& left, const String& right) { return left == right; }

} // namespace

Document document;
WindowFacade window;

void DomElement::add_event_listener(
    const String& type,
    std::function<void()> callback,
    const EventListenerOptions& options) {
  listeners_.add(
      type,
      [callback = std::move(callback)](std::nullptr_t) mutable { callback(); },
      options);
}

ClientRect DomElement::get_bounding_client_rect() const noexcept {
  return ClientRect{.bottom = height, .height = height, .right = width, .width = width};
}

void DomElement::click() {
  listeners_.emit(String("click"), nullptr);
}

CreatedElement::operator DomElement() const { return {}; }

CreatedElement::operator GlCanvas() const {
  if (!same_string(tag_, String("canvas"))) {
    throw std::invalid_argument("Only canvas elements convert to an SDL GL surface");
  }
  return GlCanvas::create(1, 1, 1.0, String("Flight offscreen surface"), true);
}

CreatedElement Document::create_element(String tag) const { return CreatedElement(std::move(tag)); }

std::optional<DomElement> Document::get_element_by_id(const String&) const { return std::nullopt; }

bool Document::has_focus() const noexcept { return focused_; }

void Document::add_event_listener(
    const String& type,
    std::function<void()> callback,
    const EventListenerOptions& options) {
  listeners_.add(
      type,
      [callback = std::move(callback)](std::nullptr_t) mutable { callback(); },
      options);
}

void Document::emit(const String& emitted_type) {
  listeners_.emit(emitted_type, nullptr);
}

void Document::set_focus(bool focused) noexcept { focused_ = focused; }

void Document::set_hidden(bool next_hidden) {
  if (hidden == next_hidden) return;
  hidden = next_hidden;
  emit(String("visibilitychange"));
}

void WindowFacade::add_event_listener(
    const String& type,
    std::function<void()> callback,
    const EventListenerOptions& options) {
  listeners_.add(
      type,
      [callback = std::move(callback)](std::nullptr_t) mutable { callback(); },
      options);
}

void WindowFacade::add_event_listener(
    const String& type,
    std::function<void(InputKeyboardData)> callback,
    const EventListenerOptions& options) {
  keyboard_listeners_.add(type, std::move(callback), options);
}

void WindowFacade::emit(const String& emitted_type) const {
  listeners_.emit(emitted_type, nullptr);
}

void WindowFacade::emit_keyboard(const String& emitted_type, InputKeyboardData event) const {
  keyboard_listeners_.emit(emitted_type, std::move(event));
}

void WindowFacade::clear_event_listeners() {
  listeners_.clear();
  keyboard_listeners_.clear();
}

WebPlatformInput::WebPlatformInput(GlCanvas canvas)
    : canvas_(std::move(canvas)),
      input_(
          SDL_GetWindowID(canvas_.native_window()),
          [this] {
            InputSink sink;
            sink.key_down = [](const InputKeyboardData& event) {
              window.emit_keyboard(String("keydown"), event);
            };
            sink.key_up = [](const InputKeyboardData& event) {
              window.emit_keyboard(String("keyup"), event);
            };
            sink.pointer_cancel = [this](const InputPointerData& event) {
              canvas_.emit_pointer(String("pointercancel"), event);
            };
            sink.pointer_down = [this](const InputPointerData& event) {
              canvas_.emit_pointer(String("pointerdown"), event);
            };
            sink.pointer_move = [this](const InputPointerData& event) {
              canvas_.emit_pointer(String("pointermove"), event);
            };
            sink.pointer_move_relative = [this](const InputPointerData& event) {
              canvas_.emit_pointer(String("pointermove"), event);
            };
            sink.pointer_up = [this](const InputPointerData& event) {
              canvas_.emit_pointer(String("pointerup"), event);
            };
            sink.wheel = [this](const InputPointerData& event) {
              canvas_.emit_pointer(String("wheel"), event);
            };
            return sink;
          }()) {}

bool WebPlatformInput::dispatch(const SDL_Event& event) {
  bool handled = false;
  if (event.type >= SDL_EVENT_WINDOW_FIRST && event.type <= SDL_EVENT_WINDOW_LAST &&
      event.window.windowID == SDL_GetWindowID(canvas_.native_window())) {
    switch (event.type) {
      case SDL_EVENT_WINDOW_FOCUS_GAINED:
        if (!document.has_focus()) {
          document.set_focus(true);
          window.emit(String("focus"));
        }
        handled = true;
        break;
      case SDL_EVENT_WINDOW_FOCUS_LOST:
        if (document.has_focus()) {
          document.set_focus(false);
          window.emit(String("blur"));
        }
        handled = true;
        break;
      case SDL_EVENT_WINDOW_HIDDEN:
      case SDL_EVENT_WINDOW_MINIMIZED:
        if (document.has_focus()) {
          document.set_focus(false);
          window.emit(String("blur"));
        }
        if (!document.hidden) {
          document.set_hidden(true);
          window.emit(String("pagehide"));
        }
        handled = true;
        break;
      case SDL_EVENT_WINDOW_SHOWN:
      case SDL_EVENT_WINDOW_RESTORED:
        if (document.hidden) {
          document.set_hidden(false);
          window.emit(String("pageshow"));
        }
        handled = true;
        break;
      default:
        break;
    }
  }
  const auto input_handled = input_.dispatch(event);
  return handled || input_handled;
}

AnimationFrameHandle request_animation_frame(AnimationFrameCallback callback) {
  if (!callback) throw std::invalid_argument("animation frame callback cannot be empty");
  if (next_frame_handle > (std::uint64_t{1} << 53U) - 1U) {
    throw std::length_error("animation frame handle space is exhausted");
  }
  const auto handle = static_cast<AnimationFrameHandle>(next_frame_handle++);
  pending_frames.push_back(PendingFrame{handle, std::move(callback)});
  return handle;
}

AnimationFrameHandle request_animation_frame(std::function<void()> callback) {
  if (!callback) throw std::invalid_argument("animation frame callback cannot be empty");
  return request_animation_frame([callback = std::move(callback)](double) { callback(); });
}

void cancel_animation_frame(AnimationFrameHandle handle) {
  if (!std::isfinite(handle)) return;
  std::erase_if(pending_frames, [handle](const PendingFrame& frame) { return frame.handle == handle; });
}

std::size_t pump_animation_frame(double timestamp_ms) {
  auto ready = std::exchange(pending_frames, {});
  for (const auto& frame : ready) frame.callback(timestamp_ms);
  return ready.size();
}

void reset_web_platform() {
  pending_frames.clear();
  next_frame_handle = 1;
  document = Document{};
  window = WindowFacade{};
}

} // namespace flight::host_sdl
