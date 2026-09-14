#pragma once

#include <optional>

#include <flight/string.hpp>

namespace flight {

// Portable value dictionaries from the Web IDL surface used by Flight. Optional fields preserve
// the distinction between an omitted dictionary member and an explicitly supplied false or zero.
// The API consuming the dictionary remains responsible for applying its Web-defined defaults.
struct DomPointInit final {
  std::optional<double> w;
  std::optional<double> x;
  std::optional<double> y;
  std::optional<double> z;
};

struct CanvasRenderingContext2DSettings final {
  std::optional<bool> alpha;
  std::optional<String> color_space;
  std::optional<bool> desynchronized;
  std::optional<bool> will_read_frequently;
};

} // namespace flight
