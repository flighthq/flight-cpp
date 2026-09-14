#pragma once

#include <optional>

#include <flight/abort.hpp>
#include <flight/string.hpp>

namespace flight::host_sdl {

struct ClientRect final {
  double bottom{0.0};
  double height{0.0};
  double left{0.0};
  double right{0.0};
  double top{0.0};
  double width{0.0};
  double x{0.0};
  double y{0.0};
};

struct DomStyle final {
  String background;
  String color;
  String cursor;
  String display;
  String height;
  String left;
  String margin;
  String position;
  String top;
  String width;
};

struct EventListenerOptions final {
  std::optional<bool> capture;
  std::optional<bool> once;
  std::optional<bool> passive;
  std::optional<AbortSignal> signal;

  [[nodiscard]] operator AbortEventListenerOptions() const noexcept {
    return AbortEventListenerOptions{.once = once.value_or(false)};
  }
};

} // namespace flight::host_sdl
