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

GlobalScope::GlobalScope() noexcept
    : document(::flight::host_sdl::document),
      navigator(::flight::host_sdl::navigator),
      window(::flight::host_sdl::window) {}

flight::Record<flight::String, flight::Any> GlobalScope::named_globals() const {
  // One store for the process, so a value written through one projection of `globalThis` is read
  // back through the next rather than landing in a copy nobody else can see.
  static const flight::Record<flight::String, flight::Any> globals;
  return globals;
}

GlobalScope global_this;

void DomElement::add_event_listener(
    const String& type,
    Function<void()> callback,
    const EventListenerOptions& options) {
  const auto source_identity = callback.identity();
  listeners_.add(
      type,
      Function<void(std::nullptr_t)>(
          [callback = std::move(callback)](std::nullptr_t) mutable { callback(); }),
      options,
      source_identity);
}

void DomElement::remove_event_listener(
    const String& type,
    const Function<void()>& callback,
    bool capture) {
  listeners_.remove(type, callback.identity(), capture);
}

ClientRect DomElement::get_bounding_client_rect() const noexcept {
  return ClientRect{.bottom = height, .height = height, .right = width, .width = width};
}

Array<DomElement> DomElement::query_selector_all(const String&) const {
  // The shell does not materialize an HTML tree. Returning an empty static snapshot is exact for
  // its retained children and lets generated cleanup loops remain deterministic.
  return {};
}

void DomElement::insert_adjacent_html(const String& position, const String& text) {
  if (position == String("beforeend")) {
    inner_html = inner_html.concat(text);
    return;
  }
  if (position == String("afterbegin")) {
    inner_html = text.concat(inner_html);
    return;
  }
  throw std::invalid_argument(
      "SDL DOM shell only represents insertAdjacentHTML within the current element");
}

void DomElement::remove() noexcept {
  inner_html = {};
  text_content = {};
  listeners_.clear();
}

void DomElement::click() {
  listeners_.emit(String("click"), nullptr);
}

CreatedElement::CreatedElement(String tag) : tag_(std::move(tag)) {
  if (same_string(tag_, String("canvas"))) {
    width = 300.0;
    height = 150.0;
  }
}

GlCanvas& CreatedElement::require_canvas() const {
  if (!same_string(tag_, String("canvas"))) {
    throw std::invalid_argument("Only canvas elements convert to an SDL GL surface");
  }
  if (!canvas_) {
    const auto extent = [](double value, int fallback) {
      if (!std::isfinite(value) || value <= 0.0) return fallback;
      return static_cast<int>(std::min(
          std::floor(value), static_cast<double>(std::numeric_limits<int>::max())));
    };
    canvas_ = GlCanvas::create(
        extent(width, 300),
        extent(height, 150),
        1.0,
        String("Flight offscreen surface"),
        true);
  }
  canvas_->width = width;
  canvas_->height = height;
  canvas_->style = style;
  return *canvas_;
}

CreatedElement::operator GlCanvas() const { return require_canvas(); }

WebGl2Context CreatedElement::get_context() const { return require_canvas().get_context(); }

WebGl2Context CreatedElement::get_context(const String& context_id) const {
  return require_canvas().get_context(context_id);
}

std::optional<WebGl2Context> CreatedElement::get_context(
    const String& context_id,
    const WebGlContextAttributes& attributes) const {
  return require_canvas().get_context(context_id, attributes);
}

void CreatedElement::set_pointer_capture(double pointer_id) const {
  require_canvas().set_pointer_capture(pointer_id);
}

void CreatedElement::release_pointer_capture(double pointer_id) const {
  require_canvas().release_pointer_capture(pointer_id);
}

CreatedElement Document::create_element(String tag) const { return CreatedElement(std::move(tag)); }

std::optional<DomElement> Document::get_element_by_id(const String&) const { return std::nullopt; }

bool Document::has_focus() const noexcept { return focused_; }

void Document::add_event_listener(
    const String& type,
    Function<void()> callback,
    const EventListenerOptions& options) {
  const auto source_identity = callback.identity();
  listeners_.add(
      type,
      Function<void(std::nullptr_t)>(
          [callback = std::move(callback)](std::nullptr_t) mutable { callback(); }),
      options,
      source_identity);
}

void Document::remove_event_listener(
    const String& type,
    const Function<void()>& callback,
    bool capture) {
  listeners_.remove(type, callback.identity(), capture);
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
    Function<void()> callback,
    const EventListenerOptions& options) {
  const auto source_identity = callback.identity();
  listeners_.add(
      type,
      Function<void(std::nullptr_t)>(
          [callback = std::move(callback)](std::nullptr_t) mutable { callback(); }),
      options,
      source_identity);
}

void WindowFacade::add_event_listener(
    const String& type,
    Function<void(InputKeyboardData)> callback,
    const EventListenerOptions& options) {
  keyboard_listeners_.add(type, std::move(callback), options);
}

void WindowFacade::remove_event_listener(
    const String& type,
    const Function<void()>& callback,
    bool capture) {
  listeners_.remove(type, callback.identity(), capture);
}

void WindowFacade::remove_event_listener(
    const String& type,
    const Function<void(InputKeyboardData)>& callback,
    bool capture) {
  keyboard_listeners_.remove(type, callback.identity(), capture);
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
