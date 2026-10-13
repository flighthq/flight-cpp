// Behavioural parity for @flighthq/textlayout, driven by the upstream Vitest fixtures in
// .dependencies/flight/packages/textlayout/src/.
//
// textFormat is supplied by the hand-written override textlayout-text-format-keyof-merge, so its section
// checks OUR code against the upstream intent rather than the emitter's output. textBounds emits, but its
// constants and anchoring arithmetic are the kind of thing that can be off by a gutter without any
// typecheck noticing, so it is pinned here too.
//
// The layout engine itself (textLayout.test.ts, 57 cases) needs a measure provider and is a later unit;
// this file deliberately covers the two modules whose whole surface is reachable without one.

#include <flight/textlayout/text_bounds.hpp>
#include <flight/textlayout/text_format.hpp>
#include <flight/textlayout/text_layout.hpp>
#include <flight/types/rectangle.hpp>
#include <flight/types/text_bounds_spec.hpp>
#include <flight/types/text_format.hpp>
#include <flight/types/text_layout.hpp>

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const char* message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

void check_close(double got, double want, const char* message) {
  if (std::abs(got - want) < 1e-9) return;
  std::cerr << "FAIL: " << message << "\n  want " << want << "\n  got  " << got << '\n';
  ++failures;
}

flight::Ref<flight::types::TextFormat> make_format(std::optional<double> size,
                                                   std::optional<double> leading) {
  auto format = flight::make_ref<flight::types::TextFormat>();
  format->size = size;
  format->leading = leading;
  return format;
}

// TextBoundsSpec is a structural row over a generated anonymous shape, so it is built by wrapping that
// shape the same way the generated code wraps it.
using SpecRow = flight::types::TextBoundsSpec;

SpecRow make_spec(const char* auto_size, double height, double width, bool word_wrap) {
  using Shape = flight::types::auto_size_height_width_word_wrap_75a4ff02b472c2de;
  const auto row = flight::make_ref<Shape>(Shape{.auto_size = flight::String(auto_size),
                                                 .height = height,
                                                 .width = width,
                                                 .word_wrap = std::optional<bool>{word_wrap}});
  return flight::structural_ref_cast<SpecRow>(
      flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<Shape>>>>(row));
}

using LayoutRow = flight::StructuralRef<
    flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::TextLayoutResult>>>>;

LayoutRow make_layout(double text_width, double text_height) {
  auto layout = flight::make_ref<flight::types::TextLayoutResult>();
  layout->text_width = text_width;
  layout->text_height = text_height;
  layout->num_lines = 1.0;
  return flight::structural_ref_cast<LayoutRow>(
      flight::StructuralRef<
          flight::RowWritable<flight::RowOf<flight::Ref<flight::types::TextLayoutResult>>>>(layout));
}

}  // namespace

int main() {
  // ---- textFormat.test.ts (override-supplied) --------------------------------------------------

  check_close(flight::textlayout::get_text_format_ascent(make_format(24.0, std::nullopt)), 24.0,
              "textFormat: ascent returns the specified size");
  check_close(flight::textlayout::get_text_format_ascent(make_format(std::nullopt, std::nullopt)), 12.0,
              "textFormat: ascent returns 12 when size is absent");
  check_close(flight::textlayout::get_text_format_descent(make_format(100.0, std::nullopt)), 18.5,
              "textFormat: descent is 18.5% of size");
  check_close(flight::textlayout::get_text_format_descent(make_format(std::nullopt, std::nullopt)),
              12.0 * 0.185, "textFormat: descent uses the default size when absent");
  check_close(flight::textlayout::get_text_format_height(make_format(10.0, 4.0)), 10.0 + 10.0 * 0.185 + 4.0,
              "textFormat: height sums ascent, descent and leading");
  check_close(flight::textlayout::get_text_format_height(make_format(10.0, std::nullopt)),
              10.0 + 10.0 * 0.185, "textFormat: height treats absent leading as zero");
  check_close(flight::textlayout::get_text_format_leading(make_format(std::nullopt, 6.0)), 6.0,
              "textFormat: leading returns the specified value");
  check_close(flight::textlayout::get_text_format_leading(make_format(std::nullopt, std::nullopt)), 0.0,
              "textFormat: leading returns 0 when absent");

  {
    // Non-null override fields land on the base, and the base is not mutated: the merge returns a new
    // format rather than editing the caller's.
    auto base = make_format(12.0, std::nullopt);
    base->bold = false;
    auto override_format = make_format(std::nullopt, std::nullopt);
    override_format->bold = true;
    override_format->color = 4278190335.0;  // 0xFF0000FF, opaque red

    const auto merged = flight::textlayout::merge_text_format(base, override_format);
    check_close(merged->size.value_or(-1.0), 12.0, "textFormat: merge keeps the base size");
    check(merged->bold == std::optional<bool>{true}, "textFormat: merge applies a non-null override field");
    check(merged->color == std::optional<double>{4278190335.0},
          "textFormat: merge applies an override field the base did not have");
    check_close(base->size.value_or(-1.0), 12.0, "textFormat: merge does not mutate the base");
    check(base->bold == std::optional<bool>{false}, "textFormat: merge leaves the base's own value alone");
    check(merged != base, "textFormat: merge returns a distinct format object");
  }

  {
    // Absent override fields are skipped rather than written as absence-on-top-of-a-value.
    auto base = make_format(12.0, std::nullopt);
    auto override_format = make_format(std::nullopt, std::nullopt);
    const auto merged = flight::textlayout::merge_text_format(base, override_format);
    check_close(merged->size.value_or(-1.0), 12.0, "textFormat: an absent override size leaves the base size");
    check(!merged->bold.has_value(), "textFormat: an absent override field does not appear");
  }

  // ---- textBounds.test.ts ----------------------------------------------------------------------

  // Height: the declared box when autoSize is none, the measured text plus a gutter on each side
  // otherwise -- rounded up, which is what keeps a fractional measurement from clipping.
  check_close(flight::textlayout::compute_text_bounds_height(make_spec("none", 80.0, 200.0, false),
                                                             make_layout(50.0, 20.0)),
              80.0, "textBounds: height uses the spec height when autoSize is none");
  check_close(flight::textlayout::compute_text_bounds_height(make_spec("left", 100.0, 200.0, false),
                                                             make_layout(50.0, 18.0)),
              22.0, "textBounds: height is text height plus both gutters under autoSize");

  // Width: the declared width when autoSize is none OR word wrap is on; the measured width plus gutters
  // otherwise.
  check_close(flight::textlayout::compute_text_bounds_width(make_spec("none", 100.0, 120.0, false),
                                                            make_layout(30.0, 20.0)),
              120.0, "textBounds: width uses the spec width when autoSize is none");
  check_close(flight::textlayout::compute_text_bounds_width(make_spec("left", 100.0, 120.0, true),
                                                            make_layout(30.0, 20.0)),
              120.0, "textBounds: width keeps the spec width when word wrap is enabled");
  check_close(flight::textlayout::compute_text_bounds_width(make_spec("left", 100.0, 120.0, false),
                                                            make_layout(30.0, 20.0)),
              34.0, "textBounds: width is text width plus both gutters under autoSize");

  // Offset: the anchor moves the box within the slack, and the three anchors disagree about what to do
  // with it.
  check_close(flight::textlayout::compute_text_bounds_offset_x(make_spec("left", 100.0, 120.0, false),
                                                               make_layout(30.0, 20.0)),
              0.0, "textBounds: a left anchor leaves the box at the origin");
  check_close(flight::textlayout::compute_text_bounds_offset_x(make_spec("right", 100.0, 120.0, false),
                                                               make_layout(30.0, 20.0)),
              86.0, "textBounds: a right anchor shifts the box fully into the slack");
  check_close(flight::textlayout::compute_text_bounds_offset_x(make_spec("center", 100.0, 120.0, false),
                                                               make_layout(30.0, 20.0)),
              43.0, "textBounds: a center anchor splits the slack");

  {
    // The rectangle threads width, height and offset together, so it is the one place the arithmetic is
    // observed as a whole box rather than field by field.
    const auto out = flight::make_ref<flight::types::Rectangle>();
    flight::textlayout::compute_text_bounds_rectangle(out, make_spec("none", 80.0, 120.0, false),
                                                      make_layout(50.0, 20.0));
    check_close(out->width, 120.0, "textBounds: a none box takes the declared width");
    check_close(out->height, 80.0, "textBounds: a none box takes the declared height");
    check_close(out->x, 0.0, "textBounds: a none box sits at the origin");

    const auto anchored = flight::make_ref<flight::types::Rectangle>();
    flight::textlayout::compute_text_bounds_rectangle(anchored, make_spec("right", 100.0, 120.0, false),
                                                      make_layout(30.0, 18.0));
    check_close(anchored->width, 34.0, "textBounds: an autoSize box takes the measured width");
    check_close(anchored->height, 22.0, "textBounds: an autoSize box takes the measured height");
    check_close(anchored->x, 86.0, "textBounds: an anchored box carries its anchor offset");
  }

  // The bounds gutter is the layout gutter rather than a second constant that happens to agree today.
  check(flight::textlayout::text_bounds_gutter == flight::textlayout::text_layout_gutter,
        "textBounds: the bounds gutter is the layout gutter (de-duplicated constant)");
  check(flight::textlayout::text_bounds_gutter > 0.0, "textBounds: the gutter is positive");

  return failures == 0 ? 0 : 1;
}
