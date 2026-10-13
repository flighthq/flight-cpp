// Focused native parity for @flighthq/clip at Flight a627849.
// These fixtures pin ownership, identity, conservative composition, transform representation,
// mutation ordering, empty bounds, and the package's opt-in failure seams.
//
// MUTATION SENSITIVITY, measured 2026-10-13 against isolated generated-header changes: four
// one-at-a-time mutants were compiled and run, then discarded. The suite killed removal of clone
// contour copies, minimum-for-maximum intersection ordering, forced scissor retention across a
// rotation, and suppression of double-release detection.
#include <flight/clip/contract.hpp>

#include <cmath>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <string>

namespace {

namespace clip = flight::clip;
namespace geometry = flight::geometry;
namespace types = flight::types;

int failures = 0;
constexpr double tolerance = 1e-8;

void check(bool condition, const std::string& message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

void check_close(double actual, double expected, const std::string& message) {
  check(std::abs(actual - expected) <= tolerance, message);
}

template <typename Type>
auto readonly(const std::shared_ptr<Type>& value) {
  using View = flight::StructuralRef<
      flight::RowReadonly<flight::RowOf<flight::Ref<Type>>>>;
  return flight::structural_ref_cast<View>(value);
}

flight::Ref<types::Rectangle> rectangle(
    double x, double y, double width, double height) {
  return geometry::create_rectangle(x, y, width, height);
}

flight::Array<flight::Array<double>> triangle(
    double x = 0.0, double y = 0.0, double size = 10.0) {
  return flight::Array<flight::Array<double>>{
      flight::Array<double>{x, y, x + size, y, x, y + size}};
}

void ownership_identity_and_pooling() {
  const auto source_rect = rectangle(2.0, 3.0, 8.0, 9.0);
  const auto rectangular = clip::create_clip_region_from_rectangle(readonly(source_rect));
  check(rectangular->rect != source_rect && rectangular->rect->x == 2.0 &&
            !rectangular->contours.has_value() && rectangular->version == 0.0,
        "rectangle construction owns a copied rect and starts in scissor form");
  source_rect->x = 99.0;
  check(rectangular->rect->x == 2.0,
        "later source-rectangle mutation cannot retarget a clip");

  auto source_contours = triangle();
  const void* source_outer = source_contours.identity();
  const void* source_inner = source_contours.element(0.0).identity();
  const auto contour =
      clip::create_clip_region_from_contours(source_contours, flight::String("evenOdd"));
  check(contour->contours.has_value() &&
            contour->contours.value().identity() != source_outer &&
            contour->contours.value().element(0.0).identity() != source_inner,
        "contour construction owns both array levels");
  source_contours.element(0.0).element(0.0) = 77.0;
  check(contour->contours.value().element(0.0).element(0.0) == 0.0,
        "caller contour mutation cannot alias constructed geometry");

  const auto clone = clip::clone_clip_region(readonly(contour));
  check(clone != contour && clone->rect != contour->rect &&
            clone->contours.value().identity() != contour->contours.value().identity() &&
            clone->contours.value().element(0.0).identity() !=
                contour->contours.value().element(0.0).identity() &&
            clip::equals_clip_region(readonly(clone), readonly(contour)),
        "clone owns a deep structural copy while preserving geometry");
  clone->contours.value().element(0.0).element(2.0) = 20.0;
  check(contour->contours.value().element(0.0).element(2.0) == 10.0 &&
            !clip::equals_clip_region(readonly(clone), readonly(contour)),
        "mutating clone geometry neither aliases nor remains structurally equal");

  const auto copied = clip::create_clip_region_from_rectangle(
      readonly(rectangle(-1.0, -1.0, 1.0, 1.0)));
  clip::copy_clip_region(copied, readonly(contour));
  check(copied->version == 1.0 && copied->winding == flight::String("evenOdd") &&
            copied->contours.value().identity() != contour->contours.value().identity(),
        "copy retargets in place, increments once, and deep-copies contours");
  const double version = copied->version;
  const auto rect_owner = copied->rect;
  clip::copy_clip_region(copied, readonly(copied));
  check(copied->version == version && copied->rect == rect_owner,
        "self-copy is an identity-preserving no-op");

  const auto first = clip::acquire_clip_region();
  const auto second = clip::acquire_clip_region();
  check(first != second, "an empty pool allocates distinct clip identities");
  first->rect->x = 12.0;
  first->contours = triangle();
  first->version = 9.0;
  clip::release_clip_region(first);
  const auto reused = clip::acquire_clip_region();
  check(reused == first && reused->rect->x == 0.0 &&
            !reused->contours.has_value() && reused->version == 0.0,
        "pool reuse preserves identity but resets all mutable clip state");
  clip::release_clip_region(reused);
  clip::release_clip_region(second);
}

void nesting_intersection_and_order() {
  const auto outer =
      clip::create_clip_region_from_contours(triangle(0.0, 0.0, 100.0),
                                             flight::String("nonZero"));
  const auto corner = clip::create_clip_region_from_rectangle(
      readonly(rectangle(80.0, 80.0, 10.0, 10.0)));
  check(clip::clip_region_contains_clip_region(readonly(outer), readonly(corner)) &&
            clip::clip_region_intersects_clip_region(readonly(outer), readonly(corner)),
        "contour nesting and overlap deliberately use conservative bounds");
  check(!clip::clip_region_contains_point(readonly(outer), 85.0, 85.0),
        "point containment still evaluates the exact contour winding");

  const auto a = clip::create_clip_region_from_rectangle(
      readonly(rectangle(0.0, 0.0, 10.0, 10.0)));
  const auto b = clip::create_clip_region_from_rectangle(
      readonly(rectangle(4.0, 3.0, 10.0, 10.0)));
  const auto intersection = clip::create_clip_region_from_rectangle(
      readonly(rectangle(0.0, 0.0, 1.0, 1.0)));
  clip::intersect_clip_regions(intersection, readonly(a), readonly(b));
  check(intersection->rect->x == 4.0 && intersection->rect->y == 3.0 &&
            intersection->rect->width == 6.0 && intersection->rect->height == 7.0 &&
            !intersection->contours.has_value() && intersection->version == 1.0,
        "rectangular intersection preserves coordinate order and scissor form");

  const auto alias_a = clip::clone_clip_region(readonly(a));
  const auto alias_b = clip::clone_clip_region(readonly(b));
  clip::intersect_clip_regions(alias_a, readonly(alias_a), readonly(b));
  clip::intersect_clip_regions(alias_b, readonly(a), readonly(alias_b));
  check(clip::equals_clip_region(readonly(alias_a), readonly(intersection)) &&
            clip::equals_clip_region(readonly(alias_b), readonly(intersection)),
        "intersection reads both inputs before either output alias is mutated");

  const auto disjoint = clip::create_clip_region_from_rectangle(
      readonly(rectangle(20.0, 20.0, 2.0, 2.0)));
  clip::intersect_clip_regions(intersection, readonly(a), readonly(disjoint));
  check(clip::is_clip_region_empty(readonly(intersection)) &&
            intersection->rect->x == 0.0 && intersection->rect->y == 0.0 &&
            !intersection->contours.has_value() &&
            intersection->winding == flight::String("nonZero"),
        "disjoint intersection resets the complete result to canonical empty state");

  const auto one = clip::create_clip_region_from_contours(
      triangle(), flight::String("evenOdd"));
  const auto two = clip::create_clip_region_from_contours(
      flight::Array<flight::Array<double>>{
          flight::Array<double>{20.0, 20.0, 30.0, 20.0, 20.0, 30.0},
          flight::Array<double>{22.0, 22.0, 24.0, 22.0, 22.0, 24.0}},
      flight::String("nonZero"));
  const auto combined = clip::create_clip_region_from_rectangle(
      readonly(rectangle(0.0, 0.0, 1.0, 1.0)));
  clip::union_clip_regions(combined, readonly(one), readonly(two));
  check(combined->contours.value().size() == 2 &&
            combined->winding == flight::String("nonZero") &&
            combined->contours.value().identity() != two->contours.value().identity(),
        "union keeps and deep-copies the richer ordered contour operand");
  two->contours.value().element(0.0).element(0.0) = 999.0;
  check(combined->contours.value().element(0.0).element(0.0) == 20.0,
        "union output does not alias retained input contours");
}

void transforms_bounds_and_mutation() {
  const auto source = clip::create_clip_region_from_rectangle(
      readonly(rectangle(1.0, 2.0, 3.0, 4.0)));
  const auto out = clip::create_clip_region_from_rectangle(
      readonly(rectangle(0.0, 0.0, 1.0, 1.0)));
  const auto scale_translate = geometry::create_matrix(2.0, 0.0, 0.0, 3.0, 5.0, 7.0);
  clip::transform_clip_region(out, readonly(source), readonly(scale_translate));
  check(out->rect->x == 7.0 && out->rect->y == 13.0 &&
            out->rect->width == 6.0 && out->rect->height == 12.0 &&
            !out->contours.has_value() && out->version == 1.0,
        "axis-aligned transform keeps a rectangle scissor-eligible");

  const auto rotate = geometry::create_matrix(0.0, 1.0, -1.0, 0.0, 10.0, 0.0);
  clip::transform_clip_region(out, readonly(source), readonly(rotate));
  check(out->contours.has_value() && out->contours.value().size() == 1 &&
            out->contours.value().element(0.0).size() == 8 && out->version == 2.0,
        "rotation promotes a rectangle to an ordered four-corner contour");
  check_close(out->rect->x, 4.0, "rotated bounds use the minimum transformed x");
  check_close(out->rect->y, 1.0, "rotated bounds use the minimum transformed y");
  check_close(out->rect->width, 4.0, "rotated bounds span every transformed corner");
  check_close(out->rect->height, 3.0, "rotated bounds span every transformed corner");

  const auto contour = clip::create_clip_region_from_contours(
      triangle(), flight::String("evenOdd"));
  const auto translate = geometry::create_matrix(1.0, 0.0, 0.0, 1.0, 5.0, 7.0);
  clip::transform_clip_region(contour, readonly(contour), readonly(translate));
  check(contour->contours.value().element(0.0).element(0.0) == 5.0 &&
            contour->contours.value().element(0.0).element(1.0) == 7.0 &&
            contour->winding == flight::String("evenOdd") && contour->version == 1.0,
        "contour transform is alias-safe and preserves winding");

  const auto bounds = rectangle(-1.0, -1.0, -1.0, -1.0);
  clip::get_clip_region_bounds(bounds, readonly(contour));
  check(bounds->x == 5.0 && bounds->y == 7.0 &&
            bounds->width == 10.0 && bounds->height == 10.0,
        "bounds copy the transformed extent into caller-owned storage");

  auto replacement = triangle(2.0, 3.0, 10.0);
  clip::set_clip_region_to_contours(source, replacement, flight::String("evenOdd"));
  replacement.element(0.0).element(0.0) = 100.0;
  check(source->version == 1.0 && source->rect->x == 2.0 && source->rect->y == 3.0 &&
            source->contours.value().element(0.0).element(0.0) == 2.0,
        "set-to-contours recomputes bounds, owns input, then increments version");
  clip::set_clip_region_to_rectangle(
      source, readonly(rectangle(9.0, 8.0, 7.0, 6.0)));
  check(source->version == 2.0 && !source->contours.has_value() &&
            source->winding == flight::String("nonZero") && source->rect->x == 9.0,
        "set-to-rectangle restores canonical scissor state before invalidation");
  source->version = 4294967295.0;
  clip::invalidate_clip_region(source);
  check(source->version == 0.0, "version invalidation wraps as unsigned 32-bit state");

  const auto malformed = clip::create_clip_region_from_contours(
      flight::Array<flight::Array<double>>{flight::Array<double>{0.0, 0.0, 1.0}},
      flight::String("nonZero"));
  check(clip::is_clip_region_empty(readonly(malformed)) &&
            malformed->rect->x == 0.0 && malformed->rect->width == 0.0,
        "malformed contour runs are skipped and yield canonical empty bounds");
}

void failure_guards() {
  int malformed_calls = 0;
  flight::String malformed_reason;
  using ContoursGuard = types::ClipRegionContoursGuard;
  clip::set_clip_region_contours_guard(ContoursGuard(
      [&](auto explanation, auto) {
        ++malformed_calls;
        malformed_reason =
            flight::row_get<flight::RowKey<"reason">>(explanation);
      }));
  static_cast<void>(clip::create_clip_region_from_contours(
      flight::Array<flight::Array<double>>{flight::Array<double>{0.0, 0.0, 1.0}},
      flight::String("nonZero")));
  clip::set_clip_region_contours_guard(std::nullopt);
  static_cast<void>(clip::create_clip_region_from_contours(
      flight::Array<flight::Array<double>>{flight::Array<double>{0.0, 0.0, 1.0}},
      flight::String("nonZero")));
  check(malformed_calls == 1 && malformed_reason == flight::String("odd-coordinate-count"),
        "malformed contour failure invokes the installed guard only");

  int release_calls = 0;
  using ReleaseGuard = types::ClipRegionReleaseGuard;
  clip::set_clip_region_release_guard(
      ReleaseGuard([&](auto) { ++release_calls; }));
  const auto doubled = clip::acquire_clip_region();
  clip::release_clip_region(doubled);
  clip::release_clip_region(doubled);
  clip::set_clip_region_release_guard(std::nullopt);
  check(release_calls == 1,
        "double release reports exactly once before preserving source pool behavior");
  const auto drained_a = clip::acquire_clip_region();
  const auto drained_b = clip::acquire_clip_region();
  check(drained_a == drained_b,
        "unguarded double release demonstrates the exact aliasing failure being reported");

  int use_calls = 0;
  using UseGuard = types::ClipRegionUseGuard;
  clip::set_clip_region_use_guard(UseGuard([&](auto) { ++use_calls; }));
  const auto released = clip::acquire_clip_region();
  clip::release_clip_region(released);
  static_cast<void>(clip::clip_region_contains_point(readonly(released), 0.0, 0.0));
  const auto explanation = clip::explain_clip_region(readonly(released));
  clip::set_clip_region_use_guard(std::nullopt);
  check(use_calls == 1 && explanation->status == flight::String("released"),
        "use-after-release is observable without changing the query's return path");
  static_cast<void>(clip::acquire_clip_region());

  clip::enable_clip_guards();
  check(clip::contours_guard.has_value() && clip::release_guard.has_value() &&
            clip::use_guard.has_value(),
        "public guard enablement installs all three package failure reporters");
  clip::disable_clip_guards();
  check(!clip::contours_guard.has_value() && !clip::release_guard.has_value() &&
            !clip::use_guard.has_value(),
        "public guard disablement removes every reporter");
}

}  // namespace

int main() {
  ownership_identity_and_pooling();
  nesting_intersection_and_order();
  transforms_bounds_and_mutation();
  failure_guards();

  if (failures == 0) {
    std::cout << "clip ownership and composition match Flight\n";
  }
  return failures == 0 ? 0 : 1;
}
