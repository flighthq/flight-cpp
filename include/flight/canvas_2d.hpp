#pragma once

#include <algorithm>
#include <any>
#include <array>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <memory>
#include <optional>
#include <type_traits>
#include <typeindex>
#include <utility>
#include <variant>
#include <vector>

#include <flight/array.hpp>
#include <flight/dom_exception.hpp>
#include <flight/error.hpp>
#include <flight/image_data.hpp>
#include <flight/string.hpp>
#include <flight/web_types.hpp>

// The Canvas 2D contract, split the way the platform splits it: flight-cpp owns the parts of
// CanvasRenderingContext2D that are the same on every device -- the drawing-state stack, the
// current transformation matrix, path construction, dash lists, gradient stops and pattern
// parameters -- and a provider owns pixels. Nothing here rasterizes, and nothing here pretends to:
// a context with no rasterizer attached throws from every operation that would have produced or
// read pixels rather than silently succeeding.
//
// Path segments retain the transform that was in force when each was added, which is what the Web
// specification means by transforming coordinates at construction time, and keeps arcs exact
// instead of flattening them at a tolerance the runtime cannot know.
namespace flight {

// Thrown by every pixel-producing or pixel-reading operation on a context that has no rasterizer.
// This is deliberately not a DOMException: no Web host can reach this state, so reporting it as one
// would misdescribe a host-integration error as a script-visible one.
class Canvas2DUnattachedError final : public std::logic_error {
 public:
  Canvas2DUnattachedError()
      : std::logic_error("flight::CanvasRenderingContext2D has no attached rasterizer") {}
};

// A drawable source -- an image, a video frame, another canvas -- named by whichever type the
// active binding profile maps CanvasImageSource to. The runtime never interprets it; it carries the
// host's own value to the host's own rasterizer with its type intact.
class Canvas2DImageSource final {
 public:
  Canvas2DImageSource() = default;

  template <typename Source>
    requires(!std::same_as<std::remove_cvref_t<Source>, Canvas2DImageSource> &&
             !std::same_as<std::remove_cvref_t<Source>, std::nullopt_t>)
  Canvas2DImageSource(Source&& source)
      : value_(std::make_shared<const std::any>(std::in_place_type<std::remove_cvref_t<Source>>,
                                                std::forward<Source>(source))) {}

  [[nodiscard]] bool has_value() const noexcept { return value_ != nullptr; }
  [[nodiscard]] explicit operator bool() const noexcept { return has_value(); }

  [[nodiscard]] std::type_index source_type() const noexcept {
    return value_ ? std::type_index(value_->type()) : std::type_index(typeid(void));
  }

  // Recovers the host's own value. Returns nullptr when this source holds a different type, so a
  // rasterizer that understands several source kinds selects rather than guesses.
  template <typename Source>
  [[nodiscard]] const Source* source_if() const noexcept {
    return value_ ? std::any_cast<Source>(value_.get()) : nullptr;
  }

  [[nodiscard]] const void* identity() const noexcept { return value_.get(); }

  [[nodiscard]] friend bool operator==(const Canvas2DImageSource& left,
                                       const Canvas2DImageSource& right) noexcept {
    return left.value_ == right.value_;
  }

 private:
  std::shared_ptr<const std::any> value_;
};

// The surface a 2D context targets. Web hosts spell this HTMLCanvasElement; the portable contract
// exposes the dimensions consumers actually read and write, and retains one shared identity so two
// views of the same surface compare equal.
class Canvas2DSurface final {
 public:
  Canvas2DSurface() noexcept = default;

  [[nodiscard]] static Canvas2DSurface create(double width_value, double height_value) {
    Canvas2DSurface surface;
    surface.state_ = std::make_shared<State>();
    surface.width = width_value;
    surface.height = height_value;
    surface.state_->width = width_value;
    surface.state_->height = height_value;
    return surface;
  }

  double height{};
  double width{};

  [[nodiscard]] explicit operator bool() const noexcept { return state_ != nullptr; }
  [[nodiscard]] const void* identity() const noexcept { return state_.get(); }

  // Publishes this view's dimensions to the shared surface. Emitted code writes `canvas.width`
  // through whichever copy it holds, so the context republishes before every operation that
  // depends on the surface size.
  void publish() const noexcept {
    if (!state_) return;
    state_->width = width;
    state_->height = height;
  }

  [[nodiscard]] double shared_width() const noexcept { return state_ ? state_->width : width; }
  [[nodiscard]] double shared_height() const noexcept { return state_ ? state_->height : height; }

  [[nodiscard]] friend bool operator==(const Canvas2DSurface& left,
                                       const Canvas2DSurface& right) noexcept {
    return left.state_ == right.state_;
  }

 private:
  struct State final {
    double height{};
    double width{};
  };

  std::shared_ptr<State> state_;
};

class DomMatrix;

// DOMMatrix2DInit, with the specification's requirement that an alias pair which is present twice
// must agree. `a` and `m11` name the same component; supplying both with different values is a
// TypeError rather than a silent winner.
struct DomMatrix2DInit final {
  std::optional<double> a;
  std::optional<double> b;
  std::optional<double> c;
  std::optional<double> d;
  std::optional<double> e;
  std::optional<double> f;
  std::optional<double> m11;
  std::optional<double> m12;
  std::optional<double> m21;
  std::optional<double> m22;
  std::optional<double> m41;
  std::optional<double> m42;
};

// The 2D transformation matrix [a c e; b d f]. Only the 2D subset exists here: nothing in the
// Canvas 2D contract produces a 3D matrix, and declaring the 3D components without maintaining
// them would state a capability the runtime does not have.
class DomMatrix final {
 public:
  DomMatrix() = default;

  DomMatrix(double a_value, double b_value, double c_value, double d_value, double e_value,
            double f_value) noexcept
      : a(a_value), b(b_value), c(c_value), d(d_value), e(e_value), f(f_value) {}

  double a{1.0};
  double b{0.0};
  double c{0.0};
  double d{1.0};
  double e{0.0};
  double f{0.0};
  bool is2_d{true};

  [[nodiscard]] double m11() const noexcept { return a; }
  [[nodiscard]] double m12() const noexcept { return b; }
  [[nodiscard]] double m21() const noexcept { return c; }
  [[nodiscard]] double m22() const noexcept { return d; }
  [[nodiscard]] double m41() const noexcept { return e; }
  [[nodiscard]] double m42() const noexcept { return f; }

  // this * other, in the specification's order: `other` applies first.
  [[nodiscard]] DomMatrix multiply(const DomMatrix& other) const noexcept {
    return DomMatrix(a * other.a + c * other.b, b * other.a + d * other.b,
                     a * other.c + c * other.d, b * other.c + d * other.d,
                     a * other.e + c * other.f + e, b * other.e + d * other.f + f);
  }

  [[nodiscard]] std::pair<double, double> transform_point(double x, double y) const noexcept {
    return {a * x + c * y + e, b * x + d * y + f};
  }

  [[nodiscard]] static DomMatrix from_init(const DomMatrix2DInit& init) {
    return DomMatrix(resolve(init.a, init.m11, 1.0, "a", "m11"),
                     resolve(init.b, init.m12, 0.0, "b", "m12"),
                     resolve(init.c, init.m21, 0.0, "c", "m21"),
                     resolve(init.d, init.m22, 1.0, "d", "m22"),
                     resolve(init.e, init.m41, 0.0, "e", "m41"),
                     resolve(init.f, init.m42, 0.0, "f", "m42"));
  }

  [[nodiscard]] friend bool operator==(const DomMatrix&, const DomMatrix&) noexcept = default;

 private:
  [[nodiscard]] static double resolve(const std::optional<double>& shorthand,
                                      const std::optional<double>& longhand, double fallback,
                                      const char* shorthand_name, const char* longhand_name) {
    if (shorthand && longhand && !(*shorthand == *longhand)) {
      throw TypeError(String(shorthand_name).concat(String(" and "), String(longhand_name),
                                                    String(" disagree in DOMMatrix2DInit")));
    }
    if (shorthand) return *shorthand;
    if (longhand) return *longhand;
    return fallback;
  }
};

// TypeScript treats a DOMMatrix as a DOMMatrix2DInit, so emitted code passes one where the other is
// declared. This is a free function rather than a converting constructor because DOMMatrix2DInit is
// a dictionary, and emitted dictionary values are aggregate-initialized.
[[nodiscard]] inline DomMatrix2DInit to_matrix_init(const DomMatrix& matrix) {
  return DomMatrix2DInit{matrix.a, matrix.b, matrix.c, matrix.d, matrix.e, matrix.f,
                         std::nullopt, std::nullopt, std::nullopt, std::nullopt,
                         std::nullopt, std::nullopt};
}

struct Canvas2DColorStop final {
  String color;
  double offset{};
};

enum class Canvas2DGradientKind : std::uint8_t { linear, radial, conic };

// A gradient handle. Copies share one stop list and one identity, as they do in JavaScript, so a
// gradient handed to fillStyle keeps receiving stops added through any other copy.
class CanvasGradient final {
 public:
  CanvasGradient() noexcept = default;

  [[nodiscard]] static CanvasGradient linear(double x0, double y0, double x1, double y1) {
    return CanvasGradient(Canvas2DGradientKind::linear, {x0, y0, 0.0, x1, y1, 0.0}, 0.0);
  }

  [[nodiscard]] static CanvasGradient radial(double x0, double y0, double r0, double x1, double y1,
                                             double r1) {
    if (!std::isfinite(r0) || !std::isfinite(r1) || r0 < 0.0 || r1 < 0.0) {
      throw DOMException(String("radial gradient radii must be finite and non-negative"),
                         String("IndexSizeError"));
    }
    return CanvasGradient(Canvas2DGradientKind::radial, {x0, y0, r0, x1, y1, r1}, 0.0);
  }

  [[nodiscard]] static CanvasGradient conic(double start_angle, double x, double y) {
    return CanvasGradient(Canvas2DGradientKind::conic, {x, y, 0.0, 0.0, 0.0, 0.0}, start_angle);
  }

  // Stops are kept in offset order with ties in insertion order, which is the order a conforming
  // rasterizer must interpolate them in.
  void add_color_stop(double offset, const String& color) const {
    auto& state = require_state();
    if (!std::isfinite(offset) || offset < 0.0 || offset > 1.0) {
      throw DOMException(String("color stop offset must be within [0, 1]"),
                         String("IndexSizeError"));
    }
    const auto position =
        std::find_if(state.stops.begin(), state.stops.end(),
                     [&](const Canvas2DColorStop& stop) { return stop.offset > offset; });
    state.stops.insert(position, Canvas2DColorStop{color, offset});
  }

  [[nodiscard]] Canvas2DGradientKind kind() const { return require_state().kind; }
  [[nodiscard]] double start_angle() const { return require_state().start_angle; }

  [[nodiscard]] const std::array<double, 6>& coordinates() const {
    return require_state().coordinates;
  }

  [[nodiscard]] Array<Canvas2DColorStop> color_stops() const {
    const auto& state = require_state();
    return Array<Canvas2DColorStop>(state.stops.begin(), state.stops.end());
  }

  [[nodiscard]] explicit operator bool() const noexcept { return state_ != nullptr; }
  [[nodiscard]] const void* identity() const noexcept { return state_.get(); }

  [[nodiscard]] friend bool operator==(const CanvasGradient& left,
                                       const CanvasGradient& right) noexcept {
    return left.state_ == right.state_;
  }

 private:
  struct State final {
    std::array<double, 6> coordinates{};
    Canvas2DGradientKind kind{Canvas2DGradientKind::linear};
    double start_angle{};
    std::vector<Canvas2DColorStop> stops;
  };

  CanvasGradient(Canvas2DGradientKind kind_value, std::array<double, 6> coordinates_value,
                 double start_angle_value)
      : state_(std::make_shared<State>()) {
    state_->coordinates = coordinates_value;
    state_->kind = kind_value;
    state_->start_angle = start_angle_value;
  }

  [[nodiscard]] State& require_state() const {
    if (!state_) throw Canvas2DUnattachedError();
    return *state_;
  }

  std::shared_ptr<State> state_;
};

// A pattern handle. Like a gradient it shares one identity and one mutable transform across copies.
class CanvasPattern final {
 public:
  CanvasPattern() noexcept = default;

  [[nodiscard]] static CanvasPattern create(Canvas2DImageSource source, String repetition) {
    CanvasPattern pattern;
    pattern.state_ = std::make_shared<State>();
    pattern.state_->repetition = std::move(repetition);
    pattern.state_->source = std::move(source);
    return pattern;
  }

  void set_transform(const DomMatrix2DInit& transform = {}) const {
    require_state().transform = DomMatrix::from_init(transform);
  }

  void set_transform(const DomMatrix& transform) const { require_state().transform = transform; }

  [[nodiscard]] const Canvas2DImageSource& source() const { return require_state().source; }
  [[nodiscard]] const String& repetition() const { return require_state().repetition; }
  [[nodiscard]] const DomMatrix& transform() const { return require_state().transform; }

  [[nodiscard]] explicit operator bool() const noexcept { return state_ != nullptr; }
  [[nodiscard]] const void* identity() const noexcept { return state_.get(); }

  [[nodiscard]] friend bool operator==(const CanvasPattern& left,
                                       const CanvasPattern& right) noexcept {
    return left.state_ == right.state_;
  }

 private:
  struct State final {
    String repetition;
    Canvas2DImageSource source;
    DomMatrix transform;
  };

  [[nodiscard]] State& require_state() const {
    if (!state_) throw Canvas2DUnattachedError();
    return *state_;
  }

  std::shared_ptr<State> state_;
};

// `string | CanvasGradient | CanvasPattern`. Assignment from an absent optional is a no-op, which
// is what `ctx.fillStyle = ctx.createPattern(...)` means when the pattern could not be created:
// the Web platform leaves the previous style in place rather than clearing it.
class CanvasStyle final {
 public:
  CanvasStyle() : value_(String()) {}
  CanvasStyle(String color) : value_(std::move(color)) {}
  CanvasStyle(const char* color) : value_(String(color)) {}
  CanvasStyle(CanvasGradient gradient) : value_(std::move(gradient)) {}
  CanvasStyle(CanvasPattern pattern) : value_(std::move(pattern)) {}

  CanvasStyle& operator=(String color) {
    value_ = std::move(color);
    return *this;
  }

  CanvasStyle& operator=(CanvasGradient gradient) {
    value_ = std::move(gradient);
    return *this;
  }

  CanvasStyle& operator=(CanvasPattern pattern) {
    value_ = std::move(pattern);
    return *this;
  }

  template <typename Value>
    requires std::constructible_from<CanvasStyle, Value>
  CanvasStyle& operator=(const std::optional<Value>& value) {
    if (value) *this = *value;
    return *this;
  }

  [[nodiscard]] bool is_color() const noexcept { return std::holds_alternative<String>(value_); }

  [[nodiscard]] bool is_gradient() const noexcept {
    return std::holds_alternative<CanvasGradient>(value_);
  }

  [[nodiscard]] bool is_pattern() const noexcept {
    return std::holds_alternative<CanvasPattern>(value_);
  }

  [[nodiscard]] const String& color() const { return std::get<String>(value_); }
  [[nodiscard]] const CanvasGradient& gradient() const { return std::get<CanvasGradient>(value_); }
  [[nodiscard]] const CanvasPattern& pattern() const { return std::get<CanvasPattern>(value_); }

  [[nodiscard]] const std::variant<String, CanvasGradient, CanvasPattern>& value() const noexcept {
    return value_;
  }

  [[nodiscard]] friend bool operator==(const CanvasStyle&, const CanvasStyle&) noexcept = default;

 private:
  std::variant<String, CanvasGradient, CanvasPattern> value_;
};

// roundRect's radii argument: one radius, one corner point, or a list of either.
class Canvas2DCornerRadii final {
 public:
  using Radius = std::variant<double, DomPointInit>;

  Canvas2DCornerRadii() = default;
  Canvas2DCornerRadii(double radius) : radii_{Radius(radius)} {}
  Canvas2DCornerRadii(DomPointInit radius) : radii_{Radius(std::move(radius))} {}

  Canvas2DCornerRadii(std::initializer_list<Radius> radii) : radii_(radii) {}

  template <typename Element>
    requires std::constructible_from<Radius, const Element&>
  Canvas2DCornerRadii(const Array<Element>& radii) {
    for (const auto& radius : radii) radii_.emplace_back(radius);
  }

  [[nodiscard]] const std::vector<Radius>& radii() const noexcept { return radii_; }

 private:
  std::vector<Radius> radii_;
};

struct Canvas2DRect final {
  double height{};
  double width{};
  double x{};
  double y{};
};

// Path verbs. An arc stays an arc: the runtime does not choose a flattening tolerance on a
// rasterizer's behalf.
struct Canvas2DMoveSegment final {
  double x{};
  double y{};
};

struct Canvas2DLineSegment final {
  double x{};
  double y{};
};

struct Canvas2DCubicSegment final {
  double control1_x{};
  double control1_y{};
  double control2_x{};
  double control2_y{};
  double x{};
  double y{};
};

struct Canvas2DArcSegment final {
  bool counterclockwise{};
  double end_angle{};
  double radius_x{};
  double radius_y{};
  double rotation{};
  double start_angle{};
  double x{};
  double y{};
};

struct Canvas2DCloseSegment final {};

using Canvas2DVerb = std::variant<Canvas2DMoveSegment, Canvas2DLineSegment, Canvas2DCubicSegment,
                                  Canvas2DArcSegment, Canvas2DCloseSegment>;

// One path element plus the transform that was in force when it was added. Keeping the matrix with
// the segment reproduces the specification's "transform the coordinates by the CTM" without
// pre-multiplying points, which would lose the exact shape of a transformed arc.
struct Canvas2DPathSegment final {
  DomMatrix transform;
  Canvas2DVerb verb;
};

class Canvas2DPath final {
 public:
  [[nodiscard]] const std::vector<Canvas2DPathSegment>& segments() const noexcept {
    return segments_;
  }

  [[nodiscard]] bool empty() const noexcept { return segments_.empty(); }

  void push(DomMatrix transform, Canvas2DVerb verb) {
    segments_.push_back(Canvas2DPathSegment{std::move(transform), std::move(verb)});
  }

  void clear() noexcept { segments_.clear(); }

 private:
  std::vector<Canvas2DPathSegment> segments_;
};

struct Canvas2DClipRegion final {
  String fill_rule{String("nonzero")};
  Canvas2DPath path;
};

// Everything save() stores and restore() puts back, and everything a rasterizer needs in order to
// execute one drawing operation. It is passed to the rasterizer by reference so a provider reads
// resolved state rather than reaching back into the context.
struct Canvas2DDrawingState final {
  std::vector<double> dash_list;
  String direction{String("inherit")};
  String filter{String("none")};
  CanvasStyle fill_style{String("#000000")};
  String font{String("10px sans-serif")};
  String font_kerning{String("auto")};
  String font_stretch{String("normal")};
  String font_variant_caps{String("normal")};
  double global_alpha{1.0};
  String global_composite_operation{String("source-over")};
  bool image_smoothing_enabled{true};
  String image_smoothing_quality{String("low")};
  String letter_spacing{String("0px")};
  String line_cap{String("butt")};
  double line_dash_offset{0.0};
  String line_join{String("miter")};
  double line_width{1.0};
  double miter_limit{10.0};
  double shadow_blur{0.0};
  String shadow_color{String("rgba(0, 0, 0, 0)")};
  double shadow_offset_x{0.0};
  double shadow_offset_y{0.0};
  CanvasStyle stroke_style{String("#000000")};
  String text_align{String("start")};
  String text_baseline{String("alphabetic")};
  String text_rendering{String("auto")};
  DomMatrix transform;
  String word_spacing{String("0px")};

  // The clip region in force, as the paths that were intersected to build it, each with the fill
  // rule it was applied under. An empty list is the whole surface.
  std::vector<Canvas2DClipRegion> clip_regions;
};

// The pixel provider. flight-cpp resolves state, transform and geometry; everything below this
// interface is the host's.
class Canvas2DRasterizer {
 public:
  Canvas2DRasterizer() = default;
  Canvas2DRasterizer(const Canvas2DRasterizer&) = delete;
  Canvas2DRasterizer& operator=(const Canvas2DRasterizer&) = delete;
  virtual ~Canvas2DRasterizer() = default;

  virtual void clear_rect(const Canvas2DRect& rect, const Canvas2DDrawingState& state) = 0;
  virtual void fill_path(const Canvas2DPath& path, const String& fill_rule,
                         const Canvas2DDrawingState& state) = 0;
  virtual void stroke_path(const Canvas2DPath& path, const Canvas2DDrawingState& state) = 0;
  virtual void draw_image(const Canvas2DImageSource& image, const Canvas2DRect& source,
                          const Canvas2DRect& destination, const Canvas2DDrawingState& state) = 0;
  virtual void draw_text(const String& text, double x, double y, std::optional<double> max_width,
                         bool stroke, const Canvas2DDrawingState& state) = 0;
  [[nodiscard]] virtual WebTextMetrics measure_text(const String& text,
                                                    const Canvas2DDrawingState& state) = 0;
  [[nodiscard]] virtual ImageData read_image_data(const Canvas2DRect& rect,
                                                  const ImageDataSettings& settings) = 0;
  virtual void write_image_data(const ImageData& image, double x, double y,
                                const std::optional<Canvas2DRect>& dirty) = 0;
  [[nodiscard]] virtual bool is_point_in_path(const Canvas2DPath& path, double x, double y,
                                              const String& fill_rule,
                                              const Canvas2DDrawingState& state) = 0;
  [[nodiscard]] virtual bool is_point_in_stroke(const Canvas2DPath& path, double x, double y,
                                                const Canvas2DDrawingState& state) = 0;
};

// CanvasRenderingContext2D. Copies share one drawing surface, one state stack and one rasterizer;
// the public members mirror the Web IDL attributes and are republished into the shared state before
// every operation that consumes them, so a copy that was handed a new fillStyle draws with it.
class CanvasRenderingContext2D final {
 public:
  CanvasRenderingContext2D() noexcept = default;

  [[nodiscard]] static CanvasRenderingContext2D create(
      std::shared_ptr<Canvas2DRasterizer> rasterizer, Canvas2DSurface surface,
      CanvasRenderingContext2DSettings settings = {}) {
    CanvasRenderingContext2D context;
    context.state_ = std::make_shared<State>();
    context.state_->rasterizer = std::move(rasterizer);
    context.state_->settings = std::move(settings);
    context.canvas = std::move(surface);
    return context;
  }

  Canvas2DSurface canvas;

  // Web IDL attributes, in the order the specification groups them.
  String direction{String("inherit")};
  String filter{String("none")};
  CanvasStyle fill_style{String("#000000")};
  String font{String("10px sans-serif")};
  String font_kerning{String("auto")};
  String font_stretch{String("normal")};
  String font_variant_caps{String("normal")};
  double global_alpha{1.0};
  String global_composite_operation{String("source-over")};
  bool image_smoothing_enabled{true};
  String image_smoothing_quality{String("low")};
  String letter_spacing{String("0px")};
  String line_cap{String("butt")};
  double line_dash_offset{0.0};
  String line_join{String("miter")};
  double line_width{1.0};
  double miter_limit{10.0};
  double shadow_blur{0.0};
  String shadow_color{String("rgba(0, 0, 0, 0)")};
  double shadow_offset_x{0.0};
  double shadow_offset_y{0.0};
  CanvasStyle stroke_style{String("#000000")};
  String text_align{String("start")};
  String text_baseline{String("alphabetic")};
  String text_rendering{String("auto")};
  String word_spacing{String("0px")};

  [[nodiscard]] explicit operator bool() const noexcept { return state_ != nullptr; }
  [[nodiscard]] const void* identity() const noexcept { return state_.get(); }

  [[nodiscard]] friend bool operator==(const CanvasRenderingContext2D& left,
                                       const CanvasRenderingContext2D& right) noexcept {
    return left.state_ == right.state_;
  }

  // --- CanvasSettings / CanvasState ------------------------------------------------------------

  [[nodiscard]] CanvasRenderingContext2DSettings get_context_attributes() const {
    return require_state().settings;
  }

  [[nodiscard]] bool is_context_lost() const { return require_state().rasterizer == nullptr; }

  void save() {
    auto& state = require_state();
    publish();
    state.stack.push_back(state.current);
  }

  void restore() {
    auto& state = require_state();
    if (state.stack.empty()) return;
    state.current = state.stack.back();
    state.stack.pop_back();
    adopt(state.current);
  }

  void reset() {
    auto& state = require_state();
    state.current = Canvas2DDrawingState{};
    state.stack.clear();
    state.path.clear();
    state.has_subpath = false;
    adopt(state.current);
  }

  // --- CanvasTransform -------------------------------------------------------------------------

  [[nodiscard]] DomMatrix get_transform() const { return require_state().current.transform; }

  void reset_transform() const { require_state().current.transform = DomMatrix(); }

  void rotate(double angle) const {
    const double cosine = std::cos(angle);
    const double sine = std::sin(angle);
    multiply_transform(DomMatrix(cosine, sine, -sine, cosine, 0.0, 0.0));
  }

  void scale(double x, double y) const { multiply_transform(DomMatrix(x, 0.0, 0.0, y, 0.0, 0.0)); }

  void translate(double x, double y) const {
    multiply_transform(DomMatrix(1.0, 0.0, 0.0, 1.0, x, y));
  }

  void transform(double a, double b, double c, double d, double e, double f) const {
    multiply_transform(DomMatrix(a, b, c, d, e, f));
  }

  void set_transform(double a, double b, double c, double d, double e, double f) const {
    require_state().current.transform = DomMatrix(a, b, c, d, e, f);
  }

  void set_transform(const DomMatrix2DInit& transform = {}) const {
    require_state().current.transform = DomMatrix::from_init(transform);
  }

  void set_transform(const DomMatrix& transform) const {
    require_state().current.transform = transform;
  }

  // --- CanvasFillStrokeStyles ------------------------------------------------------------------

  [[nodiscard]] CanvasGradient create_linear_gradient(double x0, double y0, double x1,
                                                      double y1) const {
    static_cast<void>(require_state());
    return CanvasGradient::linear(x0, y0, x1, y1);
  }

  [[nodiscard]] CanvasGradient create_radial_gradient(double x0, double y0, double r0, double x1,
                                                      double y1, double r1) const {
    static_cast<void>(require_state());
    return CanvasGradient::radial(x0, y0, r0, x1, y1, r1);
  }

  [[nodiscard]] CanvasGradient create_conic_gradient(double start_angle, double x,
                                                     double y) const {
    static_cast<void>(require_state());
    return CanvasGradient::conic(start_angle, x, y);
  }

  // Returns an absent pattern for an unusable source or an unrecognised repetition, which is the
  // null the Web platform returns rather than a pattern that draws nothing.
  [[nodiscard]] std::optional<CanvasPattern> create_pattern(
      const Canvas2DImageSource& image, const std::optional<String>& repetition) const {
    static_cast<void>(require_state());
    if (!image.has_value()) return std::nullopt;
    const String resolved = repetition && repetition->length() > 0 ? *repetition : String("repeat");
    if (resolved != String("repeat") && resolved != String("repeat-x") &&
        resolved != String("repeat-y") && resolved != String("no-repeat")) {
      throw DOMException(String("unrecognized pattern repetition"), String("SyntaxError"));
    }
    return CanvasPattern::create(image, resolved);
  }

  // --- CanvasPathDrawingStyles -----------------------------------------------------------------

  void set_line_dash(const Array<double>& segments) const {
    auto& state = require_state();
    std::vector<double> dashes(segments.begin(), segments.end());
    for (const double segment : dashes) {
      if (!std::isfinite(segment) || segment < 0.0) return;
    }
    if (dashes.size() % 2 == 1) dashes.insert(dashes.end(), dashes.begin(), dashes.end());
    state.current.dash_list = std::move(dashes);
  }

  [[nodiscard]] Array<double> get_line_dash() const {
    const auto& dashes = require_state().current.dash_list;
    return Array<double>(dashes.begin(), dashes.end());
  }

  // --- CanvasPath ------------------------------------------------------------------------------

  void begin_path() const {
    auto& state = require_state();
    state.path.clear();
    state.has_subpath = false;
  }

  void close_path() const {
    auto& state = require_state();
    if (!state.has_subpath) return;
    state.path.push(state.current.transform, Canvas2DCloseSegment{});
  }

  void move_to(double x, double y) const {
    auto& state = require_state();
    if (!std::isfinite(x) || !std::isfinite(y)) return;
    state.path.push(state.current.transform, Canvas2DMoveSegment{x, y});
    state.has_subpath = true;
  }

  void line_to(double x, double y) const {
    auto& state = require_state();
    if (!std::isfinite(x) || !std::isfinite(y)) return;
    if (!state.has_subpath) {
      move_to(x, y);
      return;
    }
    state.path.push(state.current.transform, Canvas2DLineSegment{x, y});
  }

  void bezier_curve_to(double cp1x, double cp1y, double cp2x, double cp2y, double x,
                       double y) const {
    auto& state = require_state();
    if (!std::isfinite(cp1x) || !std::isfinite(cp1y) || !std::isfinite(cp2x) ||
        !std::isfinite(cp2y) || !std::isfinite(x) || !std::isfinite(y)) {
      return;
    }
    if (!state.has_subpath) move_to(cp1x, cp1y);
    state.path.push(state.current.transform,
                    Canvas2DCubicSegment{cp1x, cp1y, cp2x, cp2y, x, y});
  }

  // Quadratic curves are raised to their exact cubic equivalent rather than sampled: the control
  // points below reproduce the same curve, they do not approximate it.
  void quadratic_curve_to(double cpx, double cpy, double x, double y) const {
    auto& state = require_state();
    if (!std::isfinite(cpx) || !std::isfinite(cpy) || !std::isfinite(x) || !std::isfinite(y)) {
      return;
    }
    if (!state.has_subpath) move_to(cpx, cpy);
    const auto [x0, y0] = current_point(state);
    state.path.push(state.current.transform,
                    Canvas2DCubicSegment{x0 + (2.0 / 3.0) * (cpx - x0),
                                         y0 + (2.0 / 3.0) * (cpy - y0),
                                         x + (2.0 / 3.0) * (cpx - x),
                                         y + (2.0 / 3.0) * (cpy - y), x, y});
  }

  void arc(double x, double y, double radius, double start_angle, double end_angle,
           bool counterclockwise = false) const {
    if (radius < 0.0) {
      throw DOMException(String("arc radius must be non-negative"), String("IndexSizeError"));
    }
    ellipse(x, y, radius, radius, 0.0, start_angle, end_angle, counterclockwise);
  }

  void ellipse(double x, double y, double radius_x, double radius_y, double rotation,
               double start_angle, double end_angle, bool counterclockwise = false) const {
    auto& state = require_state();
    if (radius_x < 0.0 || radius_y < 0.0) {
      throw DOMException(String("ellipse radii must be non-negative"), String("IndexSizeError"));
    }
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(radius_x) ||
        !std::isfinite(radius_y) || !std::isfinite(rotation) || !std::isfinite(start_angle) ||
        !std::isfinite(end_angle)) {
      return;
    }
    state.path.push(state.current.transform,
                    Canvas2DArcSegment{counterclockwise, end_angle, radius_x, radius_y, rotation,
                                       start_angle, x, y});
    state.has_subpath = true;
  }

  // The specification's arcTo: degenerate configurations become a straight line to (x1, y1).
  void arc_to(double x1, double y1, double x2, double y2, double radius) const {
    auto& state = require_state();
    if (radius < 0.0) {
      throw DOMException(String("arcTo radius must be non-negative"), String("IndexSizeError"));
    }
    if (!std::isfinite(x1) || !std::isfinite(y1) || !std::isfinite(x2) || !std::isfinite(y2) ||
        !std::isfinite(radius)) {
      return;
    }
    if (!state.has_subpath) {
      move_to(x1, y1);
      return;
    }
    const auto [x0, y0] = current_point(state);
    const double a_x = x0 - x1;
    const double a_y = y0 - y1;
    const double b_x = x2 - x1;
    const double b_y = y2 - y1;
    const double a_length = std::hypot(a_x, a_y);
    const double b_length = std::hypot(b_x, b_y);
    const double cross = a_x * b_y - a_y * b_x;
    if (a_length == 0.0 || b_length == 0.0 || cross == 0.0 || radius == 0.0) {
      line_to(x1, y1);
      return;
    }
    const double a_unit_x = a_x / a_length;
    const double a_unit_y = a_y / a_length;
    const double b_unit_x = b_x / b_length;
    const double b_unit_y = b_y / b_length;
    const double cosine = std::clamp(a_unit_x * b_unit_x + a_unit_y * b_unit_y, -1.0, 1.0);
    const double tangent = radius / std::tan(std::acos(cosine) / 2.0);
    const double start_x = x1 + a_unit_x * tangent;
    const double start_y = y1 + a_unit_y * tangent;
    const double end_x = x1 + b_unit_x * tangent;
    const double end_y = y1 + b_unit_y * tangent;
    const bool counterclockwise = cross > 0.0;
    const double normal = counterclockwise ? 1.0 : -1.0;
    const double center_x = start_x - a_unit_y * radius * normal;
    const double center_y = start_y + a_unit_x * radius * normal;
    line_to(start_x, start_y);
    state.path.push(state.current.transform,
                    Canvas2DArcSegment{counterclockwise,
                                       std::atan2(end_y - center_y, end_x - center_x), radius,
                                       radius, 0.0,
                                       std::atan2(start_y - center_y, start_x - center_x),
                                       center_x, center_y});
  }

  void rect(double x, double y, double width, double height) const {
    auto& state = require_state();
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(width) ||
        !std::isfinite(height)) {
      return;
    }
    const DomMatrix matrix = state.current.transform;
    state.path.push(matrix, Canvas2DMoveSegment{x, y});
    state.path.push(matrix, Canvas2DLineSegment{x + width, y});
    state.path.push(matrix, Canvas2DLineSegment{x + width, y + height});
    state.path.push(matrix, Canvas2DLineSegment{x, y + height});
    state.path.push(matrix, Canvas2DCloseSegment{});
    state.has_subpath = true;
  }

  void round_rect(double x, double y, double width, double height,
                  const Canvas2DCornerRadii& radii = Canvas2DCornerRadii(0.0)) const {
    auto& state = require_state();
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(width) ||
        !std::isfinite(height)) {
      return;
    }
    const auto corners = resolve_corners(radii, width, height);
    const DomMatrix matrix = state.current.transform;
    const double left = width < 0.0 ? x + width : x;
    const double top = height < 0.0 ? y + height : y;
    const double span_x = std::abs(width);
    const double span_y = std::abs(height);
    const double right = left + span_x;
    const double bottom = top + span_y;
    state.path.push(matrix, Canvas2DMoveSegment{left + corners[0].first, top});
    state.path.push(matrix, Canvas2DLineSegment{right - corners[1].first, top});
    push_corner(state, matrix, right - corners[1].first, top + corners[1].second, corners[1],
                -half_pi, 0.0);
    state.path.push(matrix, Canvas2DLineSegment{right, bottom - corners[2].second});
    push_corner(state, matrix, right - corners[2].first, bottom - corners[2].second, corners[2],
                0.0, half_pi);
    state.path.push(matrix, Canvas2DLineSegment{left + corners[3].first, bottom});
    push_corner(state, matrix, left + corners[3].first, bottom - corners[3].second, corners[3],
                half_pi, 2.0 * half_pi);
    state.path.push(matrix, Canvas2DLineSegment{left, top + corners[0].second});
    push_corner(state, matrix, left + corners[0].first, top + corners[0].second, corners[0],
                2.0 * half_pi, 3.0 * half_pi);
    state.path.push(matrix, Canvas2DCloseSegment{});
    state.has_subpath = true;
  }

  // --- CanvasDrawPath / CanvasRect -------------------------------------------------------------

  void fill(const String& fill_rule = String("nonzero")) const {
    auto& state = publish_state();
    require_rasterizer(state).fill_path(state.path, fill_rule, state.current);
  }

  void stroke() const {
    auto& state = publish_state();
    require_rasterizer(state).stroke_path(state.path, state.current);
  }

  void clip(const String& fill_rule = String("nonzero")) const {
    auto& state = publish_state();
    state.current.clip_regions.push_back(Canvas2DClipRegion{fill_rule, state.path});
  }

  [[nodiscard]] bool is_point_in_path(double x, double y,
                                      const String& fill_rule = String("nonzero")) const {
    auto& state = publish_state();
    return require_rasterizer(state).is_point_in_path(state.path, x, y, fill_rule, state.current);
  }

  [[nodiscard]] bool is_point_in_stroke(double x, double y) const {
    auto& state = publish_state();
    return require_rasterizer(state).is_point_in_stroke(state.path, x, y, state.current);
  }

  void clear_rect(double x, double y, double width, double height) const {
    auto& state = publish_state();
    require_rasterizer(state).clear_rect(Canvas2DRect{.height = height, .width = width, .x = x, .y = y}, state.current);
  }

  void fill_rect(double x, double y, double width, double height) const {
    auto& state = publish_state();
    Canvas2DPath rectangle;
    push_rect(rectangle, state.current.transform, x, y, width, height);
    require_rasterizer(state).fill_path(rectangle, String("nonzero"), state.current);
  }

  void stroke_rect(double x, double y, double width, double height) const {
    auto& state = publish_state();
    Canvas2DPath rectangle;
    push_rect(rectangle, state.current.transform, x, y, width, height);
    require_rasterizer(state).stroke_path(rectangle, state.current);
  }

  // --- CanvasDrawImage -------------------------------------------------------------------------

  void draw_image(const Canvas2DImageSource& image, double dx, double dy) const {
    auto& state = publish_state();
    require_rasterizer(state).draw_image(image, Canvas2DRect{}, Canvas2DRect{.height = 0.0, .width = 0.0, .x = dx, .y = dy},
                                         state.current);
  }

  void draw_image(const Canvas2DImageSource& image, double dx, double dy, double dw,
                  double dh) const {
    auto& state = publish_state();
    require_rasterizer(state).draw_image(image, Canvas2DRect{}, Canvas2DRect{.height = dh, .width = dw, .x = dx, .y = dy},
                                         state.current);
  }

  void draw_image(const Canvas2DImageSource& image, double sx, double sy, double sw, double sh,
                  double dx, double dy, double dw, double dh) const {
    auto& state = publish_state();
    if (sw == 0.0 || sh == 0.0) {
      throw DOMException(String("drawImage source rectangle is empty"),
                         String("IndexSizeError"));
    }
    require_rasterizer(state).draw_image(image, Canvas2DRect{.height = sh, .width = sw, .x = sx, .y = sy},
                                         Canvas2DRect{.height = dh, .width = dw, .x = dx, .y = dy}, state.current);
  }

  // --- CanvasText ------------------------------------------------------------------------------

  void fill_text(const String& text, double x, double y,
                 std::optional<double> max_width = std::nullopt) const {
    auto& state = publish_state();
    require_rasterizer(state).draw_text(text, x, y, max_width, false, state.current);
  }

  void stroke_text(const String& text, double x, double y,
                   std::optional<double> max_width = std::nullopt) const {
    auto& state = publish_state();
    require_rasterizer(state).draw_text(text, x, y, max_width, true, state.current);
  }

  [[nodiscard]] WebTextMetrics measure_text(const String& text) const {
    auto& state = publish_state();
    return require_rasterizer(state).measure_text(text, state.current);
  }

  // --- CanvasImageData -------------------------------------------------------------------------

  [[nodiscard]] ImageData create_image_data(double width, double height,
                                            ImageDataSettings settings = {}) const {
    static_cast<void>(require_state());
    return ImageData(width, height, std::move(settings));
  }

  [[nodiscard]] ImageData create_image_data(const ImageData& source) const {
    static_cast<void>(require_state());
    return ImageData(source.width, source.height, ImageDataSettings{std::optional<String>(source.color_space)});
  }

  [[nodiscard]] ImageData get_image_data(double sx, double sy, double sw, double sh,
                                         const ImageDataSettings& settings = {}) const {
    auto& state = publish_state();
    if (sw == 0.0 || sh == 0.0) {
      throw DOMException(String("getImageData rectangle is empty"), String("IndexSizeError"));
    }
    return require_rasterizer(state).read_image_data(Canvas2DRect{.height = sh, .width = sw, .x = sx, .y = sy}, settings);
  }

  void put_image_data(const ImageData& image, double dx, double dy) const {
    auto& state = publish_state();
    require_rasterizer(state).write_image_data(image, dx, dy, std::nullopt);
  }

  void put_image_data(const ImageData& image, double dx, double dy, double dirty_x, double dirty_y,
                      double dirty_width, double dirty_height) const {
    auto& state = publish_state();
    require_rasterizer(state).write_image_data(
        image, dx, dy, Canvas2DRect{.height = dirty_height, .width = dirty_width, .x = dirty_x, .y = dirty_y});
  }

  // --- introspection used by hosts and tests ---------------------------------------------------

  [[nodiscard]] const Canvas2DPath& current_path() const { return require_state().path; }

  [[nodiscard]] const Canvas2DDrawingState& drawing_state() const {
    return require_state().current;
  }

  [[nodiscard]] std::size_t saved_state_depth() const { return require_state().stack.size(); }

  // Republishes this view's attribute members into the shared drawing state. Every operation that
  // consumes state does this first; it is public so a host that reads `drawing_state()` directly
  // can be sure it is reading what the caller last assigned.
  void publish() const {
    auto& state = require_state();
    canvas.publish();
    state.current.direction = direction;
    state.current.fill_style = fill_style;
    state.current.filter = filter;
    state.current.font = font;
    state.current.font_kerning = font_kerning;
    state.current.font_stretch = font_stretch;
    state.current.font_variant_caps = font_variant_caps;
    state.current.global_alpha = global_alpha;
    state.current.global_composite_operation = global_composite_operation;
    state.current.image_smoothing_enabled = image_smoothing_enabled;
    state.current.image_smoothing_quality = image_smoothing_quality;
    state.current.letter_spacing = letter_spacing;
    state.current.line_cap = line_cap;
    state.current.line_dash_offset = line_dash_offset;
    state.current.line_join = line_join;
    state.current.line_width = line_width;
    state.current.miter_limit = miter_limit;
    state.current.shadow_blur = shadow_blur;
    state.current.shadow_color = shadow_color;
    state.current.shadow_offset_x = shadow_offset_x;
    state.current.shadow_offset_y = shadow_offset_y;
    state.current.stroke_style = stroke_style;
    state.current.text_align = text_align;
    state.current.text_baseline = text_baseline;
    state.current.text_rendering = text_rendering;
    state.current.word_spacing = word_spacing;
  }

 private:
  struct State final {
    Canvas2DDrawingState current;
    bool has_subpath{false};
    Canvas2DPath path;
    std::shared_ptr<Canvas2DRasterizer> rasterizer;
    CanvasRenderingContext2DSettings settings;
    std::vector<Canvas2DDrawingState> stack;
  };

  static constexpr double half_pi = 1.57079632679489661923;

  [[nodiscard]] State& require_state() const {
    if (!state_) throw Canvas2DUnattachedError();
    return *state_;
  }

  [[nodiscard]] State& publish_state() const {
    publish();
    return *state_;
  }

  [[nodiscard]] static Canvas2DRasterizer& require_rasterizer(const State& state) {
    if (!state.rasterizer) throw Canvas2DUnattachedError();
    return *state.rasterizer;
  }

  // Copies restored state back onto this view so the caller's next read sees it.
  void adopt(const Canvas2DDrawingState& restored) {
    direction = restored.direction;
    fill_style = restored.fill_style;
    filter = restored.filter;
    font = restored.font;
    font_kerning = restored.font_kerning;
    font_stretch = restored.font_stretch;
    font_variant_caps = restored.font_variant_caps;
    global_alpha = restored.global_alpha;
    global_composite_operation = restored.global_composite_operation;
    image_smoothing_enabled = restored.image_smoothing_enabled;
    image_smoothing_quality = restored.image_smoothing_quality;
    letter_spacing = restored.letter_spacing;
    line_cap = restored.line_cap;
    line_dash_offset = restored.line_dash_offset;
    line_join = restored.line_join;
    line_width = restored.line_width;
    miter_limit = restored.miter_limit;
    shadow_blur = restored.shadow_blur;
    shadow_color = restored.shadow_color;
    shadow_offset_x = restored.shadow_offset_x;
    shadow_offset_y = restored.shadow_offset_y;
    stroke_style = restored.stroke_style;
    text_align = restored.text_align;
    text_baseline = restored.text_baseline;
    text_rendering = restored.text_rendering;
    word_spacing = restored.word_spacing;
  }

  void multiply_transform(const DomMatrix& matrix) const {
    auto& state = require_state();
    state.current.transform = state.current.transform.multiply(matrix);
  }

  // The last point named in user space. Path segments retain their own transform, so the untransformed
  // point is what the next segment's control points are computed from.
  [[nodiscard]] static std::pair<double, double> current_point(const State& state) {
    for (auto segment = state.path.segments().rbegin(); segment != state.path.segments().rend();
         ++segment) {
      if (const auto* move = std::get_if<Canvas2DMoveSegment>(&segment->verb)) {
        return {move->x, move->y};
      }
      if (const auto* line = std::get_if<Canvas2DLineSegment>(&segment->verb)) {
        return {line->x, line->y};
      }
      if (const auto* cubic = std::get_if<Canvas2DCubicSegment>(&segment->verb)) {
        return {cubic->x, cubic->y};
      }
      if (const auto* arc = std::get_if<Canvas2DArcSegment>(&segment->verb)) {
        return {arc->x + arc->radius_x * std::cos(arc->end_angle),
                arc->y + arc->radius_y * std::sin(arc->end_angle)};
      }
    }
    return {0.0, 0.0};
  }

  static void push_rect(Canvas2DPath& path, const DomMatrix& matrix, double x, double y,
                        double width, double height) {
    path.push(matrix, Canvas2DMoveSegment{x, y});
    path.push(matrix, Canvas2DLineSegment{x + width, y});
    path.push(matrix, Canvas2DLineSegment{x + width, y + height});
    path.push(matrix, Canvas2DLineSegment{x, y + height});
    path.push(matrix, Canvas2DCloseSegment{});
  }

  static void push_corner(State& state, const DomMatrix& matrix, double center_x, double center_y,
                          const std::pair<double, double>& radius, double start_angle,
                          double end_angle) {
    if (radius.first == 0.0 && radius.second == 0.0) return;
    state.path.push(matrix, Canvas2DArcSegment{false, end_angle, radius.first, radius.second, 0.0,
                                               start_angle, center_x, center_y});
  }

  // roundRect's radii list expands to four corners in the order the specification gives, and is
  // then scaled down uniformly when opposite radii would overlap.
  [[nodiscard]] static std::array<std::pair<double, double>, 4> resolve_corners(
      const Canvas2DCornerRadii& radii, double width, double height) {
    const auto to_pair = [](const Canvas2DCornerRadii::Radius& radius) {
      if (const auto* scalar = std::get_if<double>(&radius)) {
        if (*scalar < 0.0) {
          throw RangeError(String("roundRect radii must be non-negative"));
        }
        return std::pair<double, double>{*scalar, *scalar};
      }
      const auto& point = std::get<DomPointInit>(radius);
      const double x = point.x.value_or(0.0);
      const double y = point.y.value_or(0.0);
      if (x < 0.0 || y < 0.0) {
        throw RangeError(String("roundRect radii must be non-negative"));
      }
      return std::pair<double, double>{x, y};
    };

    const auto& list = radii.radii();
    std::array<std::pair<double, double>, 4> corners{};
    if (list.empty()) {
      corners.fill(std::pair<double, double>{0.0, 0.0});
    } else if (list.size() == 1) {
      corners.fill(to_pair(list[0]));
    } else if (list.size() == 2) {
      corners[0] = corners[2] = to_pair(list[0]);
      corners[1] = corners[3] = to_pair(list[1]);
    } else if (list.size() == 3) {
      corners[0] = to_pair(list[0]);
      corners[1] = corners[3] = to_pair(list[1]);
      corners[2] = to_pair(list[2]);
    } else if (list.size() == 4) {
      for (std::size_t index = 0; index < 4; ++index) corners[index] = to_pair(list[index]);
    } else {
      throw RangeError(String("roundRect accepts at most four radii"));
    }

    const double span_x = std::abs(width);
    const double span_y = std::abs(height);
    double scale = 1.0;
    const auto limit = [&](double used, double available) {
      if (used > 0.0 && available >= 0.0) scale = std::min(scale, available / used);
    };
    limit(corners[0].first + corners[1].first, span_x);
    limit(corners[3].first + corners[2].first, span_x);
    limit(corners[0].second + corners[3].second, span_y);
    limit(corners[1].second + corners[2].second, span_y);
    if (scale < 1.0) {
      for (auto& corner : corners) {
        corner.first *= scale;
        corner.second *= scale;
      }
    }
    return corners;
  }

  std::shared_ptr<State> state_;
};

} // namespace flight
