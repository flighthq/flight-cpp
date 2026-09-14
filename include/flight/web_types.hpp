#pragma once

#include <memory>
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

struct WebImageEncodeOptions final {
  std::optional<double> quality;
  std::optional<String> type;
};

struct WebPermissionDescriptor final {
  String name;
};

struct WebPositionOptions final {
  std::optional<bool> enable_high_accuracy;
  std::optional<double> maximum_age;
  std::optional<double> timeout;
};

// Canvas implementations produce TextMetrics, while its measured values and object identity are
// independent of the rendering provider. A provider fills these fields before returning the value.
class WebTextMetrics final {
 public:
  WebTextMetrics() : identity_(std::make_shared<Identity>()) {}

  double actual_bounding_box_ascent{};
  double actual_bounding_box_descent{};
  double actual_bounding_box_left{};
  double actual_bounding_box_right{};
  double alphabetic_baseline{};
  double em_height_ascent{};
  double em_height_descent{};
  double font_bounding_box_ascent{};
  double font_bounding_box_descent{};
  double hanging_baseline{};
  double ideographic_baseline{};
  double width{};

  [[nodiscard]] const void* identity() const noexcept { return identity_.get(); }

  [[nodiscard]] friend bool operator==(const WebTextMetrics& left,
                                       const WebTextMetrics& right) noexcept {
    return left.identity_ == right.identity_;
  }

 private:
  struct Identity final {};
  std::shared_ptr<Identity> identity_;
};

} // namespace flight
