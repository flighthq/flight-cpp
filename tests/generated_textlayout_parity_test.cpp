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
#include <flight/textlayout/text_format_range.hpp>
#include <flight/textlayout/text_layout.hpp>
#include <flight/textlayout/text_layout_measure.hpp>
#include <flight/textlayout/text_layout_runtime.hpp>
#include <flight/textlayout/text_line_breaks.hpp>
#include <flight/types/rectangle.hpp>
#include <flight/types/text_bounds_spec.hpp>
#include <flight/types/text_format.hpp>
#include <flight/types/text_format_range.hpp>
#include <flight/types/text_label.hpp>
#include <flight/types/text_layout.hpp>
#include <flight/types/text_shaper.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
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

void check_near(double got, double want, double tolerance, const char* message) {
  if (std::abs(got - want) <= tolerance) return;
  std::cerr << "FAIL: " << message << "\n  want " << want << " +/- " << tolerance << "\n  got  " << got
            << '\n';
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

// ---- the layout engine's own fixtures, driven by an injected fixed-width measure ------------------
//
// The upstream suite supplies `fixedMeasure = text => text.length * 10` and a container, so every
// expected pixel value below is arithmetic on the number of characters. That makes the engine's
// measurement, wrapping, alignment and truncation behaviour reachable here without a real shaper --
// which is the honest boundary: what is NOT tested is real font metrics, because this profile has
// none to give (see the shaper/bidi note at the end of this file).

using Params = flight::types::TextLayoutParams;
using Result = flight::types::TextLayoutResult;

flight::types::TextMeasureFunction fixed_measure() {
  return flight::types::TextMeasureFunction(
      [](flight::String text, flight::Ref<flight::types::TextFormat>) {
        return static_cast<double>(text.length()) * 10.0;
      });
}

double text_len(const char* text) { return static_cast<double>(flight::String(text).length()); }

flight::Ref<flight::types::TextFormat> size_format(double size) {
  auto format = flight::make_ref<flight::types::TextFormat>();
  format->size = size;
  return format;
}

// `singleRangeParams` from the upstream suite: one format range spanning the text, a 100px-tall box.
flight::Ref<Params> single_range(const char* text, double width) {
  auto params = flight::make_ref<Params>();
  params->text = flight::String(text);
  params->width = width;
  params->height = 100.0;
  params->measure = fixed_measure();
  params->format_ranges = flight::Array<flight::Ref<flight::types::TextFormatRange>>{
      flight::textlayout::create_text_format_range(size_format(16.0), 0.0, text_len(text))};
  return params;
}

flight::Ref<Result> do_layout(flight::Ref<Params> params) {
  const auto out = flight::textlayout::create_text_layout_result();
  flight::textlayout::compute_text_layout(out, params);
  return out;
}

double group_x(const flight::Ref<Result>& result, std::size_t index) {
  return result->groups.element(static_cast<double>(index))->offset_x;
}

double group_y(const flight::Ref<Result>& result, std::size_t index) {
  return result->groups.element(static_cast<double>(index))->offset_y;
}

// The text a group claims to cover, read back from the source string by its [start, end) span.
flight::String placed_text(const flight::Ref<Params>& params, const flight::Ref<Result>& result) {
  flight::String placed;
  for (std::size_t index = 0; index < static_cast<std::size_t>(result->groups.size()); ++index) {
    const auto& group = result->groups.element(static_cast<double>(index));
    placed = placed + params->text.slice(static_cast<std::ptrdiff_t>(group->start_index),
                                         static_cast<std::ptrdiff_t>(group->end_index));
  }
  return placed;
}

flight::String without_spaces(const flight::String& value) {
  flight::String out;
  for (std::ptrdiff_t index = 0; index < value.length(); ++index) {
    if (value.char_code_at(index) != 0x20) out = out + value.char_at(index);
  }
  return out;
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

  // ---- textLineBreaks.test.ts ------------------------------------------------------------------

  {
    // The binary search answers "first break at or after startIndex", and -1 means "there is none" --
    // which is distinguishable from a break at zero only because -1 is not a valid index.
    const auto breaks = flight::Array<double>{3.0, 7.0, 12.0};
    check_close(flight::textlayout::get_text_line_break_index(breaks, 5.0), 7.0,
                "lineBreaks: the index is the first break at or after the start");
    check_close(flight::textlayout::get_text_line_break_index(flight::Array<double>{3.0, 7.0}, 0.0), 3.0,
                "lineBreaks: an unset start finds the first break");
    check_close(flight::textlayout::get_text_line_break_index(breaks, 7.0), 7.0,
                "lineBreaks: an exact match is a match, not the next one");
    check_close(flight::textlayout::get_text_line_break_index(flight::Array<double>{3.0, 7.0}, 10.0), -1.0,
                "lineBreaks: past the last break there is none");
    check_close(flight::textlayout::get_text_line_break_index(flight::Array<double>{}, 0.0), -1.0,
                "lineBreaks: an empty break list has no break");

    const auto single = flight::Array<double>{5.0};
    check(flight::textlayout::get_text_line_break_index(single, 3.0) == 5.0 &&
              flight::textlayout::get_text_line_break_index(single, 5.0) == 5.0 &&
              flight::textlayout::get_text_line_break_index(single, 6.0) == -1.0,
          "lineBreaks: a one-element list searches correctly on both sides");

    // A hundred-element list, because the fixture is really about the BINARY SEARCH: a linear scan
    // that happened to agree on small inputs is indistinguishable from a correct one until the
    // search has to actually halve.
    flight::Array<double> hundred;
    for (int i = 0; i < 100; ++i) hundred.push(static_cast<double>(i) * 10.0);
    check(flight::textlayout::get_text_line_break_index(hundred, 55.0) == 60.0 &&
              flight::textlayout::get_text_line_break_index(hundred, 60.0) == 60.0 &&
              flight::textlayout::get_text_line_break_index(hundred, 0.0) == 0.0 &&
              flight::textlayout::get_text_line_break_index(hundred, 999.0) == -1.0,
          "lineBreaks: a large sorted list finds the first break at or after the start");
  }

  {
    // The break scan records the POSITION of each newline, and the array is one value that gets
    // truncated rather than reallocated -- so a caller can reuse it, which is why the same `out` is
    // passed seven times in the upstream fixture.
    const auto out = flight::Array<double>{};
    const auto scan = [&out](const char* text) {
      flight::textlayout::get_text_line_breaks(out, flight::String(text));
      return out.size();
    };
    scan("hello world");
    check(static_cast<double>(out.size()) == 0.0, "lineBreaks: text without newlines yields no breaks");
    scan("a\nb\nc");
    check(static_cast<double>(out.size()) == 2.0 && out.element(0.0) == 1.0 && out.element(1.0) == 3.0,
          "lineBreaks: LF positions are recorded");
    scan("a\rb\rc");
    check(static_cast<double>(out.size()) == 2.0 && out.element(0.0) == 1.0 && out.element(1) == 3.0,
          "lineBreaks: CR positions are recorded");
    scan("a\nb\rc");
    check(static_cast<double>(out.size()) == 2.0 && out.element(0.0) == 1.0 && out.element(1) == 3.0,
          "lineBreaks: with both, the earlier of CR and LF wins (LF first)");
    scan("a\rb\nc");
    check(static_cast<double>(out.size()) == 2.0 && out.element(0.0) == 1.0 && out.element(1) == 3.0,
          "lineBreaks: with both, the earlier of CR and LF wins (CR first)");
    scan("ab\n");
    check(static_cast<double>(out.size()) == 1.0 && out.element(0.0) == 2.0,
          "lineBreaks: a trailing newline is a break at the last index");
    scan("");
    check(static_cast<double>(out.size()) == 0.0, "lineBreaks: empty text yields no breaks");
  }

  // ---- textLayoutMeasure.test.ts ---------------------------------------------------------------

  {
    // No provider and no host shaper: the answer is absence, not a zero-measuring stub. This is the
    // initial state, so it is asserted before anything else touches the seam.
    check(!flight::textlayout::get_text_layout_measure_provider(std::nullopt).has_value(),
          "measure: there is no provider before one is supplied");

    const auto shaper = flight::make_ref<flight::types::HostTextShaperCapability>();
    shaper->measure_text = [](flight::String text, flight::Ref<flight::types::TextFormat>) {
      return static_cast<double>(text.length());
    };
    using ShaperSpec = flight::StructuralRef<
        flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::HostTextShaperCapability>>>>;

    const auto bound = flight::textlayout::get_text_layout_measure_provider(
        std::optional<ShaperSpec>{ShaperSpec{shaper}});
    check(bound.has_value(), "measure: a host text shaper yields a measure function");
    check_close(bound.value()(flight::String("abc"), make_format(std::nullopt, std::nullopt)), 3.0,
                "measure: the bound function delegates to the shaper's measureText");

    // An explicitly installed provider wins over the host shaper. Upstream asserts this by function
    // IDENTITY; a std::function has no identity to assert, so the observable is used instead -- the
    // provider that answers is the installed one, not the shaper's.
    flight::textlayout::set_text_layout_measure_provider(
        std::optional<flight::types::TextMeasureFunction>{flight::types::TextMeasureFunction(
            [](flight::String text, flight::Ref<flight::types::TextFormat>) {
              return static_cast<double>(text.length()) * 10.0;
            })});
    const auto preferred = flight::textlayout::get_text_layout_measure_provider(
        std::optional<ShaperSpec>{ShaperSpec{shaper}});
    check(preferred.has_value() &&
              preferred.value()(flight::String("abc"), make_format(std::nullopt, std::nullopt)) == 30.0,
          "measure: an explicitly set provider takes precedence over the host text shaper");

    // Clearing it falls back to the shaper again -- the state is a fallback chain, not a latch.
    flight::textlayout::set_text_layout_measure_provider(std::nullopt);
    check(!flight::textlayout::get_text_layout_measure_provider(std::nullopt).has_value(),
          "measure: clearing the provider returns to absence");

    const auto stored = flight::textlayout::get_text_layout_measure_provider(
        std::optional<ShaperSpec>{ShaperSpec{shaper}});
    check(stored.has_value() &&
              stored.value()(flight::String("abcd"), make_format(std::nullopt, std::nullopt)) == 4.0,
          "measure: clearing falls back to the host shaper rather than to nothing");
    flight::textlayout::set_text_layout_measure_provider(std::nullopt);
  }

  // ---- textLayoutRuntime.test.ts ---------------------------------------------------------------

  {
    // Reference identity is the assertion here, not just non-nullness: a label's layout result is a
    // value the renderer holds across frames, so re-fetching must return the SAME one rather than a
    // fresh equal-looking one. The runtime is a generated entity, so it is built through make_ref.
    const auto runtime = flight::make_ref<flight::types::TextLabelRuntime>();

    const auto attached = flight::textlayout::get_text_layout_result(runtime);
    check(attached != nullptr, "runtime: a layout result is attached to runtime state");
    check(runtime->text_layout.has_value() && runtime->text_layout.value() == attached,
          "runtime: the attached result is the one the runtime now holds");
    check(flight::textlayout::get_text_layout_result(runtime) == attached,
          "runtime: an existing layout result is reused, by identity");

    flight::textlayout::clear_text_layout_result(runtime);
    check(!runtime->text_layout.has_value(), "runtime: clearing detaches the layout result");

    const auto reattached = flight::textlayout::get_text_layout_result(runtime);
    check(reattached != nullptr && reattached != attached,
          "runtime: after a clear a fresh result is attached, not the old one");
  }

  // ---- textLayout.test.ts ------------------------------------------------------------------------

  const auto groups_on_line = [](const flight::Ref<Result>& result, double line) {
    std::vector<flight::Ref<flight::types::TextLayoutGroup>> on_line;
    for (std::size_t index = 0; index < static_cast<std::size_t>(result->groups.size()); ++index) {
      const auto& group = result->groups.element(static_cast<double>(index));
      if (group->line_index == line) on_line.push_back(group);
    }
    return on_line;
  };
  const auto max_line_index = [](const flight::Ref<Result>& result) {
    double highest = -1.0;
    for (std::size_t index = 0; index < static_cast<std::size_t>(result->groups.size()); ++index) {
      highest = std::max(highest, result->groups.element(static_cast<double>(index))->line_index);
    }
    return highest;
  };
  const auto count_zero_length_groups = [](const flight::Ref<Result>& result) {
    std::size_t count = 0;
    for (std::size_t index = 0; index < static_cast<std::size_t>(result->groups.size()); ++index) {
      const auto& group = result->groups.element(static_cast<double>(index));
      if (group->start_index == group->end_index) ++count;
    }
    return count;
  };

  // A simple string lands in one group spanning the whole text, placed at the gutter.
  {
    const auto params = single_range("hello", 1000.0);
    const auto result = do_layout(params);
    check(static_cast<double>(result->groups.size()) == 1.0,
          "layout: a simple string produces exactly one group");
    check(result->groups.element(0.0)->start_index == 0.0 &&
              result->groups.element(0.0)->end_index == 5.0 &&
              result->groups.element(0.0)->line_index == 0.0,
          "layout: the group spans the text on line zero");
    check_close(result->num_lines, 1.0, "layout: a short string is one line");
    check(result->text_height > 0.0, "layout: non-empty text has a positive height");
    check(result->text_width > 0.0, "layout: non-empty text has a positive width");
  }

  {
    // baseX = GUTTER + leftMargin + blockIndent + indent = 2, and the first baseline sits at the
    // gutter too. These are found by field, not derived, so they pin the placement origin.
    const auto result = do_layout(single_range("hi", 1000.0));
    check_close(group_x(result, 0), 2.0, "layout: a group starts at the gutter offsetX");
    check_close(group_y(result, 0), 2.0, "layout: a group starts at the gutter offsetY");
  }

  {
    const auto result = do_layout(single_range("abc", 1000.0));
    check_near(result->groups.element(0.0)->width, 30.0, 0.5,
               "layout: width is the sum of the character advances");
    const auto result_ab = do_layout(single_range("ab", 1000.0));
    check(static_cast<double>(result_ab->groups.element(0.0)->positions.size()) == 2.0,
          "layout: one position is stored per character");
  }

  // Empty input is a zero-group result that still counts one line -- distinct from a null result.
  {
    const auto result = do_layout(single_range("", 1000.0));
    check(static_cast<double>(result->groups.size()) == 0.0, "layout: empty text produces no groups");
    check_close(result->num_lines, 1.0, "layout: empty text still counts one line");

    const auto no_ranges = [] {
      auto params = flight::make_ref<Params>();
      params->text = flight::String("hello");
      params->width = 200.0;
      params->height = 100.0;
      params->measure = fixed_measure();
      return params;
    }();
    check(static_cast<double>(do_layout(no_ranges)->groups.size()) == 0.0,
          "layout: text with no format ranges produces no groups");
  }

  // Explicit line breaks are only honoured when multiline is on, and both LF and CR count.
  {
    const auto multi = [](const char* text) {
      auto params = single_range(text, 1000.0);
      params->multiline = true;
      return params;
    };
    const auto result = do_layout(multi("ab\ncd"));
    check_close(result->num_lines, 2.0, "layout: a newline splits the line when multiline is on");
    const auto lines = groups_on_line(result, 0.0);
    check(!lines.empty() && !groups_on_line(result, 1.0).empty(),
          "layout: both lines carry groups");

    auto flat = multi("ab\ncd");
    flat->multiline = false;
    check_close(do_layout(flat)->num_lines, 1.0,
                "layout: a newline does not split the line when multiline is off");

    check_close(do_layout(multi("ab\rcd"))->num_lines, 2.0, "layout: a carriage return also splits");
    check_close(do_layout(multi("a\n\nb"))->num_lines, 3.0,
                "layout: consecutive breaks each open a line");
  }

  // Per-line metrics are one entry per line, and each line has a positive height.
  {
    auto params = single_range("ab\ncd", 1000.0);
    params->multiline = true;
    const auto result = do_layout(params);
    check(static_cast<double>(result->line_widths.size()) == 2.0, "layout: one width per line");
    check(static_cast<double>(result->line_heights.size()) == 2.0, "layout: one height per line");
    bool heights_positive = true;
    for (std::size_t index = 0; index < static_cast<std::size_t>(result->line_heights.size()); ++index) {
      if (!(result->line_heights.element(static_cast<double>(index)) > 0.0)) heights_positive = false;
    }
    check(heights_positive, "layout: every line has a positive height");
  }

  // Two format ranges produce two groups, each carrying its own format -- a size is not flattened
  // away by neighbours.
  {
    auto params = flight::make_ref<Params>();
    params->text = flight::String("helloworld");
    params->width = 1000.0;
    params->height = 100.0;
    params->measure = fixed_measure();
    params->format_ranges = flight::Array<flight::Ref<flight::types::TextFormatRange>>{
        flight::textlayout::create_text_format_range(size_format(16.0), 0.0, 5.0),
        flight::textlayout::create_text_format_range(size_format(24.0), 5.0, 10.0)};
    const auto result = do_layout(params);
    check(result->groups.size() >= 2, "layout: each format range produces its own group");
    check(result->groups.element(0.0)->format->size == std::optional<double>{16.0} &&
              result->groups.element(1.0)->format->size == std::optional<double>{24.0},
          "layout: each group carries its own format size");
  }

  // Alignment: the golden values are the containers the upstream suite locked in, so they catch a
  // shift that a relative comparison would let through.
  {
    const auto aligned = [](const char* text, double width, const char* align, bool word_wrap) {
      auto params = single_range(text, width);
      params->format_ranges = flight::Array<flight::Ref<flight::types::TextFormatRange>>{
          flight::textlayout::create_text_format_range([&] {
            auto format = size_format(16.0);
            format->align = flight::String(align);
            return format;
          }(),
                                                      0.0, text_len(text))};
      params->word_wrap = word_wrap;
      params->multiline = false;
      return params;
    };

    // slack = 100 - 20 - 2*2 = 76, so a center shift is 38 and a right shift is 76.
    check_close(group_x(do_layout(aligned("hi", 100.0, "center", false)), 0), 40.0,
                "layout: center places a 20px run at 40 in a 100px container");
    check_close(group_x(do_layout(aligned("hi", 100.0, "right", false)), 0), 78.0,
                "layout: right places a 20px run at 78 in a 100px container");
    check(group_x(do_layout(aligned("hi", 100.0, "right", false)), 0) > 2.0,
          "layout: a right-aligned group is shifted right of the gutter");

    // A short single line must center against the REAL width, not a wrap-prevention sentinel: the
    // regression this guards rendered the line thousands of pixels off-screen.
    const auto centered_wide = do_layout(aligned("hi", 200.0, "center", false));
    check_close(group_x(centered_wide, 0), 90.0,
                "layout: a centered short line uses the container width, not a sentinel");
    check(group_x(centered_wide, 0) < 200.0, "layout: the centered line stays inside the container");

    // start/end are direction-relative: start is the gutter in LTR and the far edge in RTL.
    check_close(group_x(do_layout(aligned("hi", 100.0, "start", false)), 0), 2.0,
                "layout: start is the left edge in a LeftToRight context");
    check(group_x(do_layout(aligned("hi", 100.0, "end", false)), 0) > 2.0,
          "layout: end is the right edge in a LeftToRight context");

    auto rtl = aligned("hi", 100.0, "start", false);
    rtl->direction = flight::String("RightToLeft");
    check(group_x(do_layout(rtl), 0) > 2.0,
          "layout: start is the right edge in a RightToLeft context");
  }

  // Vertical alignment distributes the slack: nothing at the top, half at the middle, all of it at
  // the bottom -- and nothing at all when the box is too short to have slack.
  {
    const auto tall = [](const char* vertical_align) {
      auto params = single_range("hi", 100.0);
      params->height = 200.0;
      if (vertical_align != nullptr) params->vertical_align = flight::String(vertical_align);
      return params;
    };
    const auto top = do_layout(tall(nullptr));
    const auto explicit_top = do_layout(tall("top"));
    const auto middle = do_layout(tall("middle"));
    const auto bottom = do_layout(tall("bottom"));
    check_close(group_y(explicit_top, 0), group_y(top, 0),
                "layout: an explicit top matches the implicit default");
    check(group_y(middle, 0) > group_y(top, 0), "layout: middle shifts the block down");
    check(group_y(bottom, 0) > group_y(middle, 0), "layout: bottom shifts further than middle");
    check_near(group_y(middle, 0) - group_y(top, 0),
               (group_y(bottom, 0) - group_y(top, 0)) / 2.0, 1e-9,
               "layout: middle sits at exactly half the bottom shift");
    const double slack = 200.0 - (top->text_height + flight::textlayout::text_layout_gutter * 2.0);
    check_near(group_y(bottom, 0) - group_y(top, 0), slack, 1e-9,
               "layout: the bottom shift is the full vertical slack");

    const auto short_box = [](const char* vertical_align) {
      auto params = single_range("hi", 100.0);
      params->height = 4.0;
      if (vertical_align != nullptr) params->vertical_align = flight::String(vertical_align);
      return params;
    };
    const double short_top = group_y(do_layout(short_box(nullptr)), 0);
    check_close(group_y(do_layout(short_box("middle")), 0), short_top,
                "layout: middle is inert when the box has no slack");
    check_close(group_y(do_layout(short_box("bottom")), 0), short_top,
                "layout: bottom is inert when the box has no slack");
  }

  // Word wrap and hard word-break, and -- the assertion that actually matters -- no character is
  // dropped when the text is reconstructed from the placed group spans.
  {
    const auto wrapped = [](const char* text, double width, bool wrap) {
      auto params = single_range(text, width);
      params->word_wrap = wrap;
      params->multiline = true;
      return params;
    };
    // BLOCKED, and recorded rather than worked around: upstream's own word-boundary fixture --
    // 'hello world' at width 50 with word wrap on -- PANICS this runtime instead of wrapping. The
    // wrap takes the trailing-space trim path before any group exists, and the emitted local is an
    // always-engaged std::optional<Ref<T>> holding null, so `trimTarget &&` reads true and the next
    // `->` dereferences null. It is a mislowering that compiles, so the header gate cannot see it;
    // see docs/compiler-requests/nullish-coalesce-null-into-optional-is-never-absent.md. Asserting
    // "this aborts" here would certify the defect as correct, so the behaviour is covered by inputs
    // whose FIRST segment fits, and the crashing input is named instead.
    check(max_line_index(do_layout(wrapped("aaa bbb ccc", 60.0, true))) >= 1.0,
          "layout: an overlong line wraps at a word boundary");
    check_close(do_layout(wrapped("hello world", 50.0, false))->num_lines, 1.0,
                "layout: no wrapping happens when word wrap is off");
    check(do_layout(wrapped("abcdefghij", 50.0, true))->num_lines > 1.0,
          "layout: a single word longer than the line is broken mid-word");

    const auto simple_wrap = wrapped("aaa bbb ccc", 60.0, true);
    const auto simple_result = do_layout(simple_wrap);
    check(without_spaces(placed_text(simple_wrap, simple_result)) ==
              without_spaces(simple_wrap->text),
          "layout: wrapping drops no character");

    auto rich_wrap = flight::make_ref<Params>();
    rich_wrap->text = flight::String("aaa bbb ccc");
    rich_wrap->width = 60.0;
    rich_wrap->height = 100.0;
    rich_wrap->measure = fixed_measure();
    rich_wrap->word_wrap = true;
    rich_wrap->multiline = true;
    rich_wrap->format_ranges = flight::Array<flight::Ref<flight::types::TextFormatRange>>{
        flight::textlayout::create_text_format_range(size_format(16.0), 0.0, 4.0),
        flight::textlayout::create_text_format_range(size_format(16.0), 4.0, text_len("aaa bbb ccc"))};
    check(without_spaces(placed_text(rich_wrap, do_layout(rich_wrap))) ==
              without_spaces(rich_wrap->text),
          "layout: wrapping drops no character in multi-format text");
  }

  // Bullets: a zero-length group at the gutter, an explicit indent that WINS over the bullet width,
  // and a listMarker of none that suppresses the glyph entirely.
  {
    const auto bullet_params = [](const char* list_marker, std::optional<double> indent) {
      auto params = flight::make_ref<Params>();
      params->text = flight::String("item");
      params->width = 200.0;
      params->height = 100.0;
      params->measure = fixed_measure();
      params->multiline = true;
      params->format_ranges = flight::Array<flight::Ref<flight::types::TextFormatRange>>{
          flight::textlayout::create_text_format_range([&] {
            auto format = size_format(16.0);
            format->bullet = true;
            format->indent = indent;
            if (list_marker != nullptr) format->list_marker = flight::String(list_marker);
            return format;
          }(),
                                                      0.0, 4.0)};
      return params;
    };

    const auto bullet = do_layout(bullet_params(nullptr, std::nullopt));
    check(count_zero_length_groups(bullet) > 0, "layout: a bullet emits a zero-length glyph group");
    bool bullet_on_line_zero = false;
    for (std::size_t index = 0; index < static_cast<std::size_t>(bullet->groups.size()); ++index) {
      const auto& group = bullet->groups.element(static_cast<double>(index));
      if (group->start_index == group->end_index && group->line_index == 0.0) bullet_on_line_zero = true;
    }
    check(bullet_on_line_zero, "layout: the bullet group sits on the first line");

    // An explicit positive indent is authoritative: it is NOT clamped up to the bullet's width, so
    // the text deliberately overlaps the glyph.
    const auto indented = do_layout(bullet_params(nullptr, 1.0));
    flight::Ref<flight::types::TextLayoutGroup> bullet_group;
    flight::Ref<flight::types::TextLayoutGroup> text_group;
    for (std::size_t index = 0; index < static_cast<std::size_t>(indented->groups.size()); ++index) {
      const auto& group = indented->groups.element(static_cast<double>(index));
      if (group->start_index == group->end_index) {
        bullet_group = group;
      } else {
        text_group = group;
      }
    }
    check(bullet_group != nullptr && text_group != nullptr,
          "layout: a bulleted item has both a bullet group and a text group");
    if (bullet_group != nullptr && text_group != nullptr) {
      check_close(bullet_group->offset_x, 2.0, "layout: the bullet sits at the gutter");
      check_close(text_group->offset_x, 3.0,
                  "layout: an explicit indent is honoured, not clamped to the bullet width");
      check(text_group->offset_x < bullet_group->offset_x + bullet_group->width,
            "layout: the explicit indent places the text inside the bullet's span (overlap accepted)");
    }

    check(count_zero_length_groups(do_layout(bullet_params("none", std::nullopt))) == 0,
          "layout: listMarker none suppresses the bullet group");
    check(count_zero_length_groups(do_layout(bullet_params(nullptr, std::nullopt))) > 0,
          "layout: an absent listMarker keeps the default bullet");
  }

  // Codepoints, not code units: an astral character is one position, not two.
  {
    auto params = flight::make_ref<Params>();
    params->text = flight::String(u"\U0001F600ab");
    params->width = 200.0;
    params->height = 100.0;
    params->measure = flight::types::TextMeasureFunction(
        [](flight::String text, flight::Ref<flight::types::TextFormat>) {
          return static_cast<double>(text.length()) * 10.0;
        });
    params->format_ranges = flight::Array<flight::Ref<flight::types::TextFormatRange>>{
        flight::textlayout::create_text_format_range(size_format(16.0), 0.0,
                                                     static_cast<double>(params->text.length()))};
    const auto result = do_layout(params);
    check(static_cast<double>(result->groups.element(0.0)->positions.size()) == 3.0,
          "layout: a surrogate pair is one position, not two");
  }

  // Justification only applies to lines that are not the last of their paragraph.
  {
    const auto justified = [](const char* text, double width) {
      auto params = flight::make_ref<Params>();
      params->text = flight::String(text);
      params->width = width;
      params->height = 200.0;
      params->measure = fixed_measure();
      params->multiline = true;
      params->format_ranges = flight::Array<flight::Ref<flight::types::TextFormatRange>>{
          flight::textlayout::create_text_format_range([&] {
            auto format = size_format(16.0);
            format->align = flight::String("justify");
            return format;
          }(),
                                                      0.0, text_len(text))};
      return params;
    };

    const auto paragraphs = do_layout(justified("aa bb\ncc dd", 100.0));
    check_close(paragraphs->num_lines, 2.0, "layout: the two paragraphs are two lines");
    check(!groups_on_line(paragraphs, 0.0).empty() && !groups_on_line(paragraphs, 1.0).empty(),
          "layout: both paragraph lines carry groups");
    const auto line0 = groups_on_line(paragraphs, 0.0);
    const auto line1 = groups_on_line(paragraphs, 1.0);
    check_close(line0[0]->offset_x, 2.0,
                "layout: a paragraph-last line is not justified (line one)");
    check_close(line1[0]->offset_x, 2.0,
                "layout: a paragraph-last line is not justified (line two)");

    auto wrapped_justify = justified("aa bb cc dd", 56.0);
    wrapped_justify->word_wrap = true;
    const auto wrapped_result = do_layout(wrapped_justify);
    check(wrapped_result->num_lines >= 2.0, "layout: justify with wrap produces multiple lines");
    const auto last_line = groups_on_line(wrapped_result, wrapped_result->num_lines - 1.0);
    if (!last_line.empty()) {
      check_close(last_line[0]->offset_x, 2.0,
                  "layout: the last line of a wrapped paragraph is not justified");
    }

    auto short_para = justified("a b\nc d", 100.0);
    check_close(do_layout(short_para)->num_lines, 2.0, "layout: justify splits the paragraphs");
    check(!groups_on_line(do_layout(short_para), 0.0).empty(),
          "layout: a justified line carries groups");

    // Justification distributes space into the space characters' advances, and it counts SPACE
    // characters rather than group boundaries -- the defect this carries the comment for.
    auto spaces = justified("aa bb cc", 80.0);
    spaces->word_wrap = true;
    const auto spaces_result = do_layout(spaces);
    check(spaces_result->num_lines >= 2.0, "layout: the justified text wraps");
    bool expanded_space = false;
    for (std::size_t index = 0; index < static_cast<std::size_t>(spaces_result->groups.size()); ++index) {
      const auto& group = spaces_result->groups.element(static_cast<double>(index));
      if (group->line_index != 0.0) continue;
      for (std::size_t ci = 0; ci < static_cast<std::size_t>(group->positions.size()); ++ci) {
        const auto source_index = static_cast<std::ptrdiff_t>(group->start_index) +
                                  static_cast<std::ptrdiff_t>(ci);
        if (spaces->text.char_code_at(source_index) == 0x20 &&
            group->positions.element(static_cast<double>(ci)) > 10.0) {
          expanded_space = true;
        }
      }
    }
    check(expanded_space,
          "layout: justify expands the advance of space characters on a mid-paragraph line");
  }

  // Truncation: maxLines clips the result, and isTextLayoutTruncated reports it.
  {
    const auto clipped = [](const char* text, double width, double max_lines) {
      auto params = single_range(text, width);
      params->multiline = true;
      params->word_wrap = true;
      params->max_lines = max_lines;
      return params;
    };

    const auto long_word = do_layout(clipped("abcdefghijklmnop", 60.0, 1.0));
    check(long_word->num_lines <= 1.0, "layout: maxLines clips the line count");
    check(!groups_on_line(long_word, 0.0).empty(), "layout: the truncated line still carries a group");

    const auto overflow = do_layout(clipped("word one word two word three word four", 100.0, 2.0));
    check(overflow->num_lines <= 2.0, "layout: word-wrapped overflow is clipped to maxLines");
    check(!groups_on_line(overflow, overflow->num_lines - 1.0).empty(),
          "layout: the last visible line of a truncated layout carries a group");

    auto straddling_params = clipped("go abcdefghijklmnopqrstuvwxyz", 60.0, 2.0);
    const auto straddling = do_layout(straddling_params);
    check(straddling->num_lines <= 2.0, "layout: a straddling word is clipped");
    check(!groups_on_line(straddling, straddling->num_lines - 1.0).empty(),
          "layout: the straddling word leaves a group on the last line");
    check(flight::textlayout::is_text_layout_truncated(straddling, straddling_params),
          "layout: a clipped layout reports itself truncated");

    auto plain_params = single_range("line one\nline two\nline three", 300.0);
    plain_params->multiline = true;
    plain_params->max_lines = 2.0;
    const auto plain = do_layout(plain_params);
    check(plain->num_lines <= 2.0, "layout: explicit breaks are clipped to maxLines");
    check(!groups_on_line(plain, plain->num_lines - 1.0).empty(),
          "layout: the last visible line carries a group after clipping");
    check(flight::textlayout::is_text_layout_truncated(plain, plain_params),
          "layout: clipping on an explicit-break layout reports truncation");

    auto unlimited = single_range("one line", 1000.0);
    check(!flight::textlayout::is_text_layout_truncated(do_layout(unlimited), unlimited),
          "layout: an unlimited layout does not report truncation");

    auto negative = single_range("hello", 1000.0);
    negative->max_lines = -1.0;
    check(!flight::textlayout::is_text_layout_truncated(do_layout(negative), negative),
          "layout: maxLines of -1 means unlimited");
  }

  // kerning toggles pairwise measurement; with a monospace measure both paths must still place one
  // position per character rather than throwing or dropping one.
  {
    const auto kerned = [](bool kerning) {
      auto params = single_range("ab", 1000.0);
      params->format_ranges = flight::Array<flight::Ref<flight::types::TextFormatRange>>{
          flight::textlayout::create_text_format_range([&] {
            auto format = size_format(16.0);
            format->kerning = kerning;
            return format;
          }(),
                                                      0.0, 2.0)};
      return params;
    };
    check(static_cast<double>(do_layout(kerned(true))->groups.element(0.0)->positions.size()) == 2.0,
          "layout: kerning on still places one position per character");
    check(static_cast<double>(do_layout(kerned(false))->groups.element(0.0)->positions.size()) == 2.0,
          "layout: kerning off still places one position per character");
  }

  // createTextLayoutResult's defaults are the empty layout, and each call is a distinct object --
  // the identity the runtime's reuse depends on.
  {
    const auto fresh = flight::textlayout::create_text_layout_result();
    check(static_cast<double>(fresh->groups.size()) == 0.0 &&
              static_cast<double>(fresh->line_ascents.size()) == 0.0 &&
              static_cast<double>(fresh->line_descents.size()) == 0.0 &&
              static_cast<double>(fresh->line_heights.size()) == 0.0 &&
              static_cast<double>(fresh->line_leadings.size()) == 0.0 &&
              static_cast<double>(fresh->line_widths.size()) == 0.0,
          "layout: a fresh result has no groups and no per-line metrics");
    check_close(fresh->num_lines, 0.0, "layout: a fresh result has no lines");
    check_close(fresh->text_height, 0.0, "layout: a fresh result has no height");
    check_close(fresh->text_width, 0.0, "layout: a fresh result has no width");
    check(flight::textlayout::create_text_layout_result() != flight::textlayout::create_text_layout_result(),
          "layout: each result is a distinct object");
  }

  // The layout gutter is a positive constant that textBounds shares rather than re-declares.
  check(flight::textlayout::text_layout_gutter > 0.0, "layout: the layout gutter is positive");

  return failures == 0 ? 0 : 1;
}
