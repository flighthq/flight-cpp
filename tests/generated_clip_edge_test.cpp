// Additive native parity for @flighthq/clip at Flight a627849.
//
// The integrated 0b2fdfe1 generated_clip_test.cpp remains the ownership and composition baseline.
// This target contains only semantic axes absent from that 306-line suite: shape constructors and
// default tolerance, rectangle predicates, winding-rule disagreement, rectangular union, richer
// contour intersection, normalization, and empty contour representation.
#include <flight/clip/contract.hpp>

#include <cmath>
#include <iostream>
#include <memory>
#include <optional>
#include <string>

namespace {

namespace clip = flight::clip;
namespace geometry = flight::geometry;
namespace path = flight::path;
namespace types = flight::types;

int failures = 0;
constexpr double tolerance = 1e-6;

void check(bool condition, const std::string& message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

bool close(double actual, double expected) {
  return std::abs(actual - expected) <= tolerance;
}

template <typename Type>
auto readonly(const std::shared_ptr<Type>& value) {
  using View = flight::StructuralRef<
      flight::RowReadonly<flight::RowOf<flight::Ref<Type>>>>;
  return flight::structural_ref_cast<View>(value);
}

flight::Ref<types::Rectangle> rectangle(double x, double y, double width,
                                        double height) {
  return geometry::create_rectangle(x, y, width, height);
}

flight::Array<double> triangle(double x = 0.0, double y = 0.0,
                               double size = 10.0) {
  return flight::Array<double>{x, y, x + size, y, x, y + size};
}

flight::Array<double> quad(double x = 0.0, double y = 0.0,
                           double width = 10.0, double height = 10.0) {
  return flight::Array<double>{x,         y,          x + width, y,
                               x + width, y + height, x,         y + height};
}

void construction_shapes_and_defaults() {
  const auto circle_default =
      clip::create_clip_region_from_circle(12.0, 13.0, 4.0);
  const auto circle_explicit =
      clip::create_clip_region_from_circle(12.0, 13.0, 4.0, 0.25);
  check(circle_default->contours.has_value() &&
            clip::equals_clip_region(readonly(circle_default),
                                     readonly(circle_explicit)) &&
            close(circle_default->rect->x, 8.0) &&
            close(circle_default->rect->y, 9.0) &&
            close(circle_default->rect->width, 8.0) &&
            close(circle_default->rect->height, 8.0),
        "circle construction applies the documented 0.25 default tolerance");

  const auto ellipse_source = rectangle(3.0, 5.0, 12.0, 8.0);
  const auto ellipse = clip::create_clip_region_from_ellipse(readonly(ellipse_source));
  check(ellipse->contours.has_value() && close(ellipse->rect->x, 3.0) &&
            close(ellipse->rect->y, 5.0) && close(ellipse->rect->width, 12.0) &&
            close(ellipse->rect->height, 8.0),
        "ellipse construction preserves source bounds in contour form");

  const auto rounded = clip::create_clip_region_from_rounded_rectangle(
      readonly(rectangle(1.0, 2.0, 20.0, 12.0)), 3.0);
  const auto square_corner = clip::create_clip_region_from_rounded_rectangle(
      readonly(rectangle(1.0, 2.0, 20.0, 12.0)), 0.0);
  check(rounded->contours.has_value() && close(rounded->rect->x, 1.0) &&
            close(rounded->rect->y, 2.0) &&
            close(rounded->rect->width, 20.0) &&
            close(rounded->rect->height, 12.0) &&
            !square_corner->contours.has_value(),
        "rounded construction uses contours only for a positive radius");

  const auto authored = path::create_path(flight::String("evenOdd"));
  path::append_path_move_to(authored, -2.0, -1.0);
  path::append_path_line_to(authored, 8.0, -1.0);
  path::append_path_line_to(authored, 3.0, 9.0);
  const auto from_path = clip::create_clip_region_from_path(readonly(authored));
  check(from_path->contours.has_value() &&
            from_path->winding == flight::String("evenOdd") &&
            close(from_path->rect->x, -2.0) &&
            close(from_path->rect->y, -1.0) &&
            close(from_path->rect->width, 10.0) &&
            close(from_path->rect->height, 10.0),
        "path construction retains winding and flattened bounds");
}

void rectangle_predicates_and_winding() {
  const auto rectangular = clip::create_clip_region_from_rectangle(
      readonly(rectangle(0.0, 0.0, 10.0, 10.0)));
  check(clip::clip_region_contains_rectangle(
            readonly(rectangular), readonly(rectangle(2.0, 2.0, 3.0, 4.0))) &&
            !clip::clip_region_contains_rectangle(
                readonly(rectangular),
                readonly(rectangle(8.0, 8.0, 4.0, 4.0))),
        "rectangle containment distinguishes enclosed from merely overlapping bounds");
  check(clip::clip_region_intersects_rectangle(
            readonly(rectangular), readonly(rectangle(8.0, 8.0, 4.0, 4.0))) &&
            !clip::clip_region_intersects_rectangle(
                readonly(rectangular),
                readonly(rectangle(20.0, 20.0, 2.0, 2.0))),
        "rectangle intersection distinguishes overlap from disjoint bounds");

  const auto first = triangle();
  const auto repeated = flight::Array<flight::Array<double>>{first, first.slice()};
  const auto even_odd = clip::create_clip_region_from_contours(
      repeated, flight::String("evenOdd"));
  const auto non_zero = clip::create_clip_region_from_contours(
      repeated, flight::String("nonZero"));
  check(!clip::clip_region_contains_point(readonly(even_odd), 1.0, 1.0) &&
            clip::clip_region_contains_point(readonly(non_zero), 1.0, 1.0),
        "two same-order contours separate evenOdd parity from nonZero winding");

  const auto contour_explanation = clip::explain_clip_region(readonly(non_zero));
  const auto rectangle_explanation = clip::explain_clip_region(readonly(rectangular));
  check(contour_explanation->conservative &&
            contour_explanation->status == flight::String("active") &&
            !rectangle_explanation->conservative &&
            rectangle_explanation->status == flight::String("active"),
        "active explanations distinguish conservative contour predicates");
}

void composition_and_normalization() {
  const auto a = clip::create_clip_region_from_rectangle(
      readonly(rectangle(0.0, 0.0, 10.0, 10.0)));
  const auto b = clip::create_clip_region_from_rectangle(
      readonly(rectangle(5.0, -5.0, 20.0, 10.0)));
  const auto expected = clip::create_clip_region_from_rectangle(
      readonly(rectangle(0.0, 0.0, 1.0, 1.0)));
  clip::union_clip_regions(expected, readonly(a), readonly(b));
  check(expected->rect->x == 0.0 && expected->rect->y == -5.0 &&
            expected->rect->width == 25.0 && expected->rect->height == 15.0 &&
            !expected->contours.has_value() && expected->version == 1.0,
        "rectangular union spans both inputs and remains scissor eligible");
  const auto alias_a = clip::clone_clip_region(readonly(a));
  const auto alias_b = clip::clone_clip_region(readonly(b));
  clip::union_clip_regions(alias_a, readonly(alias_a), readonly(b));
  clip::union_clip_regions(alias_b, readonly(a), readonly(alias_b));
  check(clip::equals_clip_region(readonly(alias_a), readonly(expected)) &&
            clip::equals_clip_region(readonly(alias_b), readonly(expected)),
        "rectangular union reads both operands before either output alias is written");

  const auto one = clip::create_clip_region_from_contours(
      flight::Array<flight::Array<double>>{triangle(0.0, 0.0, 20.0)},
      flight::String("evenOdd"));
  const auto two = clip::create_clip_region_from_contours(
      flight::Array<flight::Array<double>>{triangle(0.0, 0.0, 20.0),
                                           triangle(2.0, 2.0, 2.0)},
      flight::String("nonZero"));
  const auto intersection = clip::create_clip_region_from_rectangle(
      readonly(rectangle(0.0, 0.0, 1.0, 1.0)));
  clip::intersect_clip_regions(intersection, readonly(one), readonly(two));
  check(intersection->contours.has_value() &&
            intersection->contours.value().size() == 2 &&
            intersection->winding == flight::String("nonZero") &&
            intersection->contours.value().identity() !=
                two->contours.value().identity(),
        "contour intersection keeps and deep-copies the richer ordered operand");

  const auto mixed = clip::create_clip_region_from_rectangle(
      readonly(rectangle(0.0, 0.0, 1.0, 1.0)));
  clip::intersect_clip_regions(
      mixed, readonly(two),
      readonly(clip::create_clip_region_from_rectangle(
          readonly(rectangle(1.0, 1.0, 5.0, 5.0)))));
  check(mixed->contours.has_value() && mixed->contours.value().size() == 2 &&
            mixed->winding == flight::String("nonZero") && mixed->rect->x == 1.0 &&
            mixed->rect->y == 1.0 && mixed->rect->width == 5.0 &&
            mixed->rect->height == 5.0,
        "mixed intersection retains contour order while clipping conservative bounds");

  const auto contour_quad = clip::create_clip_region_from_contours(
      flight::Array<flight::Array<double>>{quad(2.0, 3.0, 8.0, 6.0)},
      flight::String("evenOdd"));
  const auto normalized = clip::create_clip_region_from_rectangle(
      readonly(rectangle(0.0, 0.0, 1.0, 1.0)));
  clip::normalize_clip_region(normalized, readonly(contour_quad));
  check(!normalized->contours.has_value() && normalized->rect->x == 2.0 &&
            normalized->rect->y == 3.0 && normalized->rect->width == 8.0 &&
            normalized->rect->height == 6.0 &&
            normalized->winding == flight::String("nonZero") &&
            normalized->version == 1.0,
        "normalization promotes an axis-aligned four-corner contour to scissor form");

  const auto irregular = clip::create_clip_region_from_contours(
      flight::Array<flight::Array<double>>{triangle()},
      flight::String("evenOdd"));
  clip::normalize_clip_region(irregular, readonly(irregular));
  check(irregular->contours.has_value() && irregular->contours.value().size() == 1 &&
            irregular->winding == flight::String("evenOdd") &&
            irregular->version == 1.0,
        "normalization is alias-safe and preserves a non-rectangular contour");
}

void empty_degenerate_and_diagnostics() {
  const auto empty_contours = clip::create_clip_region_from_contours(
      flight::Array<flight::Array<double>>{}, flight::String("evenOdd"));
  const auto empty_rectangle = clip::create_clip_region_from_rectangle(
      readonly(rectangle(0.0, 0.0, 0.0, 4.0)));
  const auto full_rectangle = clip::create_clip_region_from_rectangle(
      readonly(rectangle(0.0, 0.0, 4.0, 4.0)));
  check(empty_contours->contours.has_value() &&
            empty_contours->contours.value().empty() &&
            clip::is_clip_region_empty(readonly(empty_contours)) &&
            !clip::is_clip_region_rectangular(readonly(empty_contours)),
        "an empty contour region remains distinct from rectangular scissor form");
  check(clip::is_clip_region_empty(readonly(empty_rectangle)) &&
            clip::is_clip_region_rectangular(readonly(empty_rectangle)) &&
            !clip::is_clip_region_empty(readonly(full_rectangle)) &&
            clip::is_clip_region_rectangular(readonly(full_rectangle)),
        "rectangle emptiness and representation are independent predicates");

  const auto odd = clip::explain_clip_region_contours(
      flight::Array<flight::Array<double>>{flight::Array<double>{0.0, 0.0, 1.0}});
  const auto short_even = clip::explain_clip_region_contours(
      flight::Array<flight::Array<double>>{
          flight::Array<double>{0.0, 0.0, 1.0, 1.0}});
  const auto valid = clip::explain_clip_region_contours(
      flight::Array<flight::Array<double>>{triangle()});
  check(odd.has_value() && odd.value()->contour_index == 0.0 &&
            odd.value()->coordinate_count == 3.0 &&
            odd.value()->reason == flight::String("odd-coordinate-count") &&
            short_even.has_value() &&
            short_even.value()->reason == flight::String("too-few-points") &&
            !valid.has_value(),
        "contour diagnostics separate odd, short-even, and valid coordinate runs");

  int guard_calls = 0;
  flight::String guard_reason;
  using ContoursGuard = types::ClipRegionContoursGuard;
  clip::set_clip_region_contours_guard(ContoursGuard(
      [&](auto explanation, auto) {
        ++guard_calls;
        guard_reason = flight::row_get<flight::RowKey<"reason">>(explanation);
      }));
  static_cast<void>(clip::create_clip_region_from_contours(
      flight::Array<flight::Array<double>>{
          flight::Array<double>{0.0, 0.0, 1.0, 1.0}},
      flight::String("nonZero")));
  static_cast<void>(clip::create_clip_region_from_contours(
      flight::Array<flight::Array<double>>{triangle()},
      flight::String("nonZero")));
  clip::set_clip_region_contours_guard(std::nullopt);
  check(guard_calls == 1 && guard_reason == flight::String("too-few-points"),
        "the optional contour guard reports short-even input but ignores valid input");
}

}  // namespace

int main() {
  construction_shapes_and_defaults();
  rectangle_predicates_and_winding();
  composition_and_normalization();
  empty_degenerate_and_diagnostics();

  if (failures == 0) {
    std::cout << "clip additive edge parity matches Flight\n";
  }
  return failures == 0 ? 0 : 1;
}
