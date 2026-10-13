// Behavioural parity for @flighthq/textshaper, driven by the upstream Vitest fixtures in
// .dependencies/flight/packages/textshaper/src/.
//
// This package is the shaper SEAM: it decides what a host capability can do, bookkeeps the runs handed
// out to callers, and itemizes a string into script/direction runs. All of that is reachable without a
// real font backend -- the pool, the capability queries and the itemizer are algorithms, and every
// upstream fixture drives them the same way, through a supplied (or deliberately partial) host.
//
// The pool section is the one that matters most: acquire/release identity IS the contract here. A run
// must come back as the SAME object, a double release must not double-pool it, and the guard seam must
// fire exactly on the double release. Those are reference-identity semantics, and the contract says
// they are to be tested directly rather than inferred.
//
// NOT covered here, and why -- recorded rather than worked around:
//   * enableTextShaperGuards' log assertions need a structured log entry, and this profile cannot emit
//     one: overrides/include/flight/log/log.hpp defines merge_span_fields as NotImplementedInThisProfile,
//     so logOnce/log_assert build every entry through it and throw. That is the log package's deliberate
//     fail-loud boundary and log is another lane's; asserting "this throws" would certify the gap as
//     correct. The POOL half of the guard (the release guard seam it installs) is tested below, which
//     is the half that does not need the log.
//   * textShaperSignals needs @flighthq/signals' createSignal/clearSignal entity construction.

#include <flight/textshaper/text_shaper.hpp>
#include <flight/textshaper/text_shaper_cluster.hpp>
#include <flight/textshaper/text_shaper_itemize.hpp>
#include <flight/textshaper/text_shaper_pool.hpp>
#include <flight/textshaper/text_shaper_run.hpp>
#include <flight/types/font_metrics.hpp>
#include <flight/types/glyph_extents.hpp>
#include <flight/types/shaped_run.hpp>
#include <flight/types/text_format.hpp>
#include <flight/types/text_item.hpp>
#include <flight/types/text_shaper.hpp>

#include <cmath>
#include <functional>
#include <cstddef>
#include <iostream>
#include <string>
#include <utility>
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

flight::Ref<flight::types::TextFormat> a_format() { return flight::make_ref<flight::types::TextFormat>(); }

// A shaped run built field by field, which is exactly how the upstream fixtures build one: the module
// under test reads the fields and never allocates the run itself.
flight::Ref<flight::types::ShapedRun> make_run(const std::vector<std::pair<double, double>>& glyphs) {
  auto run = flight::make_ref<flight::types::ShapedRun>();
  double advance = 0.0;
  flight::Array<flight::Ref<flight::types::ShapedGlyph>> built;
  for (const auto& glyph : glyphs) {
    auto entry = flight::make_ref<flight::types::ShapedGlyph>();
    entry->cluster = glyph.first;
    entry->glyph_id = glyph.first;
    entry->x_advance = glyph.second;
    built.push(entry);
    advance += glyph.second;
  }
  run->advance_width = advance;
  run->direction = flight::String("LeftToRight");
  run->font = std::nullopt;
  run->glyph_count = static_cast<double>(glyphs.size());
  run->glyphs = built;
  run->script = flight::String("Latn");
  return run;
}

double caret_at(const flight::Array<double>& positions, std::size_t index) {
  return positions.element(static_cast<double>(index));
}

// The seam's guard signature, named locally because the module exposes the variable, not a type.
using ReleaseGuard = std::function<void(
    flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::ShapedRun>>>>)>;

// 'سلام' written as escapes so the fixture does not depend on the source file's encoding.
flight::String arabic_salam() { return flight::String(u"\u0633\u0644\u0627\u0645"); }


// The upstream run fixture: two glyphs, the first of which every backend below claims to know.
flight::Ref<flight::types::ShapedRun> full_backend_shape() {
  const auto run = flight::make_ref<flight::types::ShapedRun>();
  run->advance_width = 15.0;
  run->direction = flight::String("LeftToRight");
  run->font = std::nullopt;
  run->glyph_count = 2.0;
  flight::Array<flight::Ref<flight::types::ShapedGlyph>> glyphs;
  for (double id : {10.0, 20.0}) {
    const auto glyph = flight::make_ref<flight::types::ShapedGlyph>();
    glyph->cluster = id == 10.0 ? 0.0 : 1.0;
    glyph->glyph_id = id;
    glyph->x_advance = id == 10.0 ? 8.0 : 7.0;
    glyphs.push(glyph);
  }
  run->glyphs = glyphs;
  run->script = flight::String("Latn");
  return run;
}

// A backend that supplies every OPTIONAL operation, keyed on glyph id 10 / code point 65.
flight::Ref<flight::types::HostTextShaperCapability> full_backend() {
  const auto backend = flight::make_ref<flight::types::HostTextShaperCapability>();
  backend->measure_text = [](flight::String text, flight::Ref<flight::types::TextFormat>) {
    return static_cast<double>(text.length()) * 8.0;
  };
  backend->get_code_point_for_glyph = [](double glyph_id) { return glyph_id == 10.0 ? 65.0 : -1.0; };
  backend->get_glyph_index_for_code_point = [](double code_point) {
    return code_point == 65.0 ? 10.0 : -1.0;
  };
  backend->get_glyph_name = [](double glyph_id) {
    return glyph_id == 10.0 ? flight::String("A") : flight::String("");
  };
  backend->get_font_metrics = [](auto) {
    const auto metrics = flight::make_ref<flight::types::FontMetrics>();
    metrics->ascent = 10.0;
    metrics->cap_height = 8.0;
    metrics->descent = 3.0;
    metrics->line_gap = 1.0;
    metrics->underline_position = -2.0;
    metrics->underline_thickness = 1.0;
    metrics->units_per_em = 1000.0;
    metrics->x_height = 5.0;
    return std::optional<flight::Ref<flight::types::FontMetrics>>{metrics};
  };
  backend->get_glyph_extents = [](double glyph_id) -> std::optional<flight::Ref<flight::types::GlyphExtents>> {
    if (glyph_id != 10.0) return std::nullopt;
    const auto extents = flight::make_ref<flight::types::GlyphExtents>();
    extents->height = 10.0;
    extents->width = 6.0;
    extents->x_bearing = 0.0;
    extents->y_bearing = -8.0;
    return extents;
  };
  backend->shape_run = [](flight::String, auto, auto) { return full_backend_shape(); };
  return backend;
}

// The host that can only measure advances: every optional operation is ABSENT, which is compliant
// rather than broken, and every query below must say so instead of inventing an answer.
flight::Ref<flight::types::HostTextShaperCapability> advances_only_backend() {
  const auto backend = flight::make_ref<flight::types::HostTextShaperCapability>();
  backend->measure_text = [](flight::String text, flight::Ref<flight::types::TextFormat>) {
    return static_cast<double>(text.length());
  };
  return backend;
}

}  // namespace

int main() {
  // ---- textShaper.test.ts ------------------------------------------------------------------------

  // The optional operations, as the upstream fixture names them. A host that omits them is COMPLIANT,
  // not broken -- the query exists to make that absence observable instead of an error.
  const std::vector<const char*> optional_operations = {
      "getCodePointForGlyph", "getFontMetrics",       "getGlyphExtents",
      "getGlyphIndexForCodePoint", "getGlyphName", "shapeRun"};

  {
    // A backend with only the REQUIRED member: every optional operation reads as absent.
    const auto partial = flight::make_ref<flight::types::HostTextShaperCapability>();
    partial->measure_text = [](flight::String text, flight::Ref<flight::types::TextFormat>) {
      return static_cast<double>(text.length());
    };

    bool all_absent = true;
    for (const char* name : optional_operations) {
      const auto explanation =
          flight::textshaper::explain_text_shaper_operation(partial, flight::String(name));
      if (explanation->implemented || explanation->operation != flight::String(name) ||
          explanation->layer != flight::String("none")) {
        all_absent = false;
      }
      if (flight::textshaper::has_text_shaper_operation(partial, flight::String(name))) {
        all_absent = false;
      }
    }
    check(all_absent,
          "textShaper: every operation the host omits reports layer none and implemented false");

    // The provided operation reports itself as hosted, naming the operation it answered for.
    const auto hosting = flight::make_ref<flight::types::HostTextShaperCapability>();
    hosting->measure_text = [](flight::String, flight::Ref<flight::types::TextFormat>) { return 0.0; };
    hosting->get_glyph_name = [](double) { return flight::String("A"); };
    const auto named =
        flight::textshaper::explain_text_shaper_operation(hosting, flight::String("getGlyphName"));
    check(named->implemented && named->layer == flight::String("host") &&
              named->operation == flight::String("getGlyphName"),
          "textShaper: a provided operation reports the host layer");
    check(flight::textshaper::has_text_shaper_operation(hosting, flight::String("getGlyphName")),
          "textShaper: hasOperation agrees with the explanation for a provided operation");
    // ...and a host that provides one operation does not thereby provide the others.
    check(!flight::textshaper::has_text_shaper_operation(hosting, flight::String("shapeRun")),
          "textShaper: providing one operation does not imply the rest");
  }

  {
    // measureText delegates straight to the host, and two hosts do not bleed into each other -- the
    // seam carries no ambient "current backend".
    const auto first = flight::make_ref<flight::types::HostTextShaperCapability>();
    first->measure_text = [](flight::String, flight::Ref<flight::types::TextFormat>) { return 1.0; };
    const auto second = flight::make_ref<flight::types::HostTextShaperCapability>();
    second->measure_text = [](flight::String, flight::Ref<flight::types::TextFormat>) { return 2.0; };

    check_close(flight::textshaper::measure_text(first, flight::String("x"), a_format()), 1.0,
                "textShaper: measureText reaches the first host");
    check_close(flight::textshaper::measure_text(second, flight::String("x"), a_format()), 2.0,
                "textShaper: measureText reaches the second host");
    check_close(flight::textshaper::measure_text(first, flight::String("x"), a_format()), 1.0,
                "textShaper: interleaved measureText calls stay isolated per host");

    const auto scaled = flight::make_ref<flight::types::HostTextShaperCapability>();
    scaled->measure_text = [](flight::String text, flight::Ref<flight::types::TextFormat>) {
      return static_cast<double>(text.length()) * 7.0;
    };
    check_close(flight::textshaper::measure_text(scaled, flight::String("abc"), a_format()), 21.0,
                "textShaper: measureText returns the host's own measurement");
  }

  // ---- textShaperPool.test.ts --------------------------------------------------------------------

  // The pool is process-global state, so it is drained at the start of each block that depends on
  // knowing what is in it. That is not test scaffolding around a defect: a shared pool IS the design,
  // and a caller cannot assume anything about its contents.
  {
    flight::textshaper::pool.resize(0.0);

    const auto run = flight::textshaper::acquire_shaped_run();
    check(run != nullptr, "pool: an empty pool still yields a run");
    check(static_cast<double>(run->glyphs.size()) == 0.0 && run->glyph_count == 0.0 &&
              run->advance_width == 0.0,
          "pool: a freshly allocated run is zeroed");
    flight::textshaper::release_shaped_run(run);

    // Identity, not merely equality: the run a caller releases is the object the next caller gets.
    check(flight::textshaper::acquire_shaped_run() == run,
          "pool: a released run is handed back by identity");

    // Two acquires from an empty pool must be two DISTINCT runs -- otherwise two callers shape into
    // one buffer.
    flight::textshaper::pool.resize(0.0);
    const auto r1 = flight::textshaper::acquire_shaped_run();
    const auto r2 = flight::textshaper::acquire_shaped_run();
    check(r1 != r2, "pool: two acquires from an empty pool are distinct objects");
    flight::textshaper::release_shaped_run(r1);
    flight::textshaper::release_shaped_run(r2);
  }

  {
    // The invariant the whole design turns on: an entry appears in the pool AT MOST ONCE. Honouring a
    // second release would pool the run twice, and the next two acquires would hand one object to two
    // callers who then shape into the same buffer.
    flight::textshaper::pool.resize(0.0);

    const auto run = flight::textshaper::acquire_shaped_run();
    flight::textshaper::release_shaped_run(run);
    flight::textshaper::release_shaped_run(run);

    const auto first = flight::textshaper::acquire_shaped_run();
    const auto second = flight::textshaper::acquire_shaped_run();
    check(first != second,
          "pool: a second release is ignored, so the next two acquires stay distinct");

    // The membership record must be cleared on the way OUT too, or a run legitimately released after a
    // later acquire is mistaken for a double release and dropped from the pool.
    flight::textshaper::pool.resize(0.0);
    const auto cycled = flight::textshaper::acquire_shaped_run();
    flight::textshaper::release_shaped_run(cycled);
    const auto reacquired = flight::textshaper::acquire_shaped_run();
    check(reacquired == cycled, "pool: a released run is re-acquired by identity");
    flight::textshaper::release_shaped_run(reacquired);
    check(flight::textshaper::acquire_shaped_run() == cycled,
          "pool: a run re-released after re-acquiring is accepted, not treated as a double release");

    flight::textshaper::pool.resize(0.0);
    const auto a = flight::textshaper::acquire_shaped_run();
    const auto b = flight::textshaper::acquire_shaped_run();
    flight::textshaper::release_shaped_run(a);
    flight::textshaper::release_shaped_run(b);
    const auto c = flight::textshaper::acquire_shaped_run();
    const auto d = flight::textshaper::acquire_shaped_run();
    check(c != d, "pool: distinct runs stay distinct through an acquire-release cycle");
    flight::textshaper::pool.resize(0.0);
  }

  {
    // The guard seam reports a REJECTED release -- it is the observable that a caller bug unbalanced
    // the bracket, which the pool itself silently recovers from.
    flight::textshaper::pool.resize(0.0);
    // BLOCKED, and recorded rather than worked around: the guard IS handed the run and fires exactly
    // once (asserted below), but it cannot READ it. The guard's parameter is a structural row over
    // ShapedRun, and the run the pool allocates is an instance of the anonymous carrier that DERIVES
    // from ShapedRun; shared_object() accepts an object only when the owner's native type is EXACTLY
    // the row's subject, so the view resolves to nothing and `->` panics. Measured: the identical
    // construction from a make_ref<ShapedRun> run is visible, the pooled one is not.
    // See docs/compiler-requests/structural-view-of-a-derived-carrier-is-invisible.md.
    // Upstream's assertion `expect(seen).toEqual([run])` reads through the view, so it cannot be
    // asserted here; the callback below counts without dereferencing, which is the observable that
    // upstream's OTHER guard fixture turns on.
    std::size_t fires = 0;
    const auto run = flight::textshaper::acquire_shaped_run();
    flight::textshaper::set_shaped_run_release_guard(
        std::optional<ReleaseGuard>{ReleaseGuard([&fires](auto) { ++fires; })});
    flight::textshaper::release_shaped_run(run);
    check(fires == 0, "pool: a first release is accepted and reports nothing");
    flight::textshaper::release_shaped_run(run);
    check(fires == 1, "pool: the guard fires exactly once, for the rejected release");
    flight::textshaper::set_shaped_run_release_guard(std::nullopt);
    flight::textshaper::pool.resize(0.0);
  }

  {
    // Paired reuse is silent, and clearing the guard silences it permanently.
    flight::textshaper::pool.resize(0.0);
    std::size_t fires = 0;
    const auto run = flight::textshaper::acquire_shaped_run();
    flight::textshaper::set_shaped_run_release_guard(
        std::optional<ReleaseGuard>{ReleaseGuard([&fires](auto) { ++fires; })});
    flight::textshaper::release_shaped_run(run);
    const auto reacquired = flight::textshaper::acquire_shaped_run();
    flight::textshaper::release_shaped_run(reacquired);
    check(fires == 0, "pool: a correctly paired acquire/release never reports");

    flight::textshaper::set_shaped_run_release_guard(std::nullopt);
    flight::textshaper::release_shaped_run(reacquired);  // a genuine double release, with no guard
    check(fires == 0, "pool: clearing the guard makes a later double release silent");
    flight::textshaper::pool.resize(0.0);
  }

  // ---- textShaperCluster.test.ts -----------------------------------------------------------------

  {
    // Caret positions are the pen before each glyph: one more position than there are glyphs, starting
    // at zero and ending at the run's total advance.
    const auto empty = make_run({});
    const auto empty_positions = flight::textshaper::get_caret_positions_for_run(empty);
    check(static_cast<double>(empty_positions.size()) == 1.0 && caret_at(empty_positions, 0) == 0.0,
          "cluster: an empty run still has a single caret position at zero");

    const auto three = make_run({{0.0, 8.0}, {1.0, 7.0}, {2.0, 9.0}});
    const auto three_positions = flight::textshaper::get_caret_positions_for_run(three);
    check(static_cast<double>(three_positions.size()) == 4.0,
          "cluster: caret positions are one more than the glyph count");
    check_close(caret_at(three_positions, 0), 0.0, "cluster: the first caret is at the run origin");
    check_close(caret_at(three_positions, 1), 8.0, "cluster: the second caret is after one advance");
    check_close(caret_at(three_positions, 2), 15.0, "cluster: positions accumulate the advances");
    check_close(caret_at(three_positions, 3), 24.0, "cluster: the last caret is the total advance");
    check_close(caret_at(three_positions, static_cast<std::size_t>(three_positions.size()) - 1),
                three->advance_width, "cluster: the final position equals the run's advance width");

    // A glyph's xOffset moves its DRAWING origin, not the pen, so it must not move a caret.
    const auto offset_run = make_run({{0.0, 8.0}, {1.0, 7.0}});
    offset_run->glyphs.element(0.0)->x_offset = 3.0;
    offset_run->glyphs.element(1.0)->x_offset = -2.0;
    const auto offset_positions = flight::textshaper::get_caret_positions_for_run(offset_run);
    check(static_cast<double>(offset_positions.size()) == 3.0 &&
              caret_at(offset_positions, 0) == 0.0 && caret_at(offset_positions, 1) == 8.0 &&
              caret_at(offset_positions, 2) == 15.0,
          "cluster: drawing offsets do not displace caret positions");
  }

  {
    // The cluster covering an index is the LAST glyph whose cluster is at or before it -- so an index
    // inside a multi-code-unit cluster resolves back to that cluster's start.
    const auto empty = make_run({});
    check_close(flight::textshaper::get_cluster_for_index(empty, 0.0), -1.0,
                "cluster: an empty run has no cluster");

    const auto single = make_run({{0.0, 8.0}});
    check_close(flight::textshaper::get_cluster_for_index(single, -1.0), -1.0,
                "cluster: a negative index has no cluster");

    const auto spread = make_run({{0.0, 8.0}, {2.0, 7.0}, {4.0, 9.0}});
    check_close(flight::textshaper::get_cluster_for_index(spread, 0.0), 0.0,
                "cluster: the first cluster covers its own index");
    check_close(flight::textshaper::get_cluster_for_index(spread, 1.0), 0.0,
                "cluster: an index inside a cluster resolves to that cluster");
    check_close(flight::textshaper::get_cluster_for_index(spread, 2.0), 2.0,
                "cluster: the next cluster covers its start index");
    check_close(flight::textshaper::get_cluster_for_index(spread, 3.0), 2.0,
                "cluster: the next cluster covers its interior index");
    check_close(flight::textshaper::get_cluster_for_index(spread, 4.0), 4.0,
                "cluster: the last cluster covers its start index");
    check_close(flight::textshaper::get_cluster_for_index(spread, 10.0), 4.0,
                "cluster: an index past the end resolves to the nearest preceding cluster");
  }

  {
    // A cluster's range ends at the next DISTINCT cluster, and the final cluster's end is either the
    // caller's string length or the conservative cluster + 1.
    const auto empty = make_run({});
    check(!flight::textshaper::get_index_range_for_cluster(empty, 0.0).has_value(),
          "cluster: an empty run has no cluster range");

    const auto single = make_run({{0.0, 8.0}});
    check(!flight::textshaper::get_index_range_for_cluster(single, 5.0).has_value(),
          "cluster: a cluster the run does not contain has no range");

    const auto spread = make_run({{0.0, 8.0}, {2.0, 7.0}, {4.0, 9.0}});
    const auto first = flight::textshaper::get_index_range_for_cluster(spread, 0.0);
    check(first.has_value() && first.value().element(0.0) == 0.0 && first.value().element(1.0) == 2.0,
          "cluster: a non-final cluster ends where the next one begins");
    const auto middle = flight::textshaper::get_index_range_for_cluster(spread, 2.0);
    check(middle.has_value() && middle.value().element(0.0) == 2.0 &&
              middle.value().element(1.0) == 4.0,
          "cluster: the middle cluster ends at the following cluster");

    const auto pair = make_run({{0.0, 8.0}, {2.0, 7.0}});
    const auto exact = flight::textshaper::get_index_range_for_cluster(pair, 2.0, 6.0);
    check(exact.has_value() && exact.value().element(0.0) == 2.0 && exact.value().element(1.0) == 6.0,
          "cluster: a supplied string length ends the final cluster exactly");
    const auto estimated = flight::textshaper::get_index_range_for_cluster(pair, 2.0);
    check(estimated.has_value() && estimated.value().element(0) == 2.0 &&
              estimated.value().element(1) == 3.0,
          "cluster: without a string length the final cluster ends at cluster + 1");
  }

  // ---- textShaperItemize.test.ts -----------------------------------------------------------------

  {
    const auto format = a_format();
    check(static_cast<double>(flight::textshaper::itemize_text(flight::String(""), format).size()) == 0.0,
          "itemize: an empty string yields no items");

    // A pure Latin string is one run covering the whole string.
    const auto latin = flight::textshaper::itemize_text(flight::String("Hello"), format);
    check(static_cast<double>(latin.size()) == 1.0, "itemize: a Latin string is a single run");
    check(latin.element(0.0)->start == 0.0 && latin.element(0.0)->end == 5.0 &&
              latin.element(0.0)->script == flight::String("Latn") &&
              latin.element(0.0)->direction == flight::String("LeftToRight"),
          "itemize: the Latin run covers the string with the Latin script and LTR direction");

    // A script whose recognition is the ONLY way to reach it: an unrecognised code point reads as
    // Common and then inherits the default "Latn", so a Latin fixture alone would pass even if no
    // Latin letter were recognised at all.
    const auto cyrillic = flight::textshaper::itemize_text(flight::String(u"\u041F\u0440\u0438\u0432\u0435\u0442"),
                                                           format);
    check(static_cast<double>(cyrillic.size()) == 1.0 &&
              cyrillic.element(0.0)->script == flight::String("Cyrl") &&
              cyrillic.element(0.0)->direction == flight::String("LeftToRight"),
          "itemize: a Cyrillic string is recognised as Cyrillic, not left to the default script");

    // Latin recognition is only OBSERVABLE after a different script: an unrecognised code point reads
    // as Common and inherits the run's script, and the default is already "Latn", so a Latin-only
    // string looks identical whether or not Latin is recognised. Cyrillic followed by a Latin letter
    // is the case that separates them -- recognised, the letter opens a Latn run; unrecognised, it
    // is Common and is absorbed into the Cyrillic run.
    flight::String after_cyrillic(u"\u041F\u0440\u0438\u0432\u0435\u0442");
    after_cyrillic = after_cyrillic + flight::String("M");
    const auto trailing_latin = flight::textshaper::itemize_text(after_cyrillic, format);
    check(static_cast<double>(trailing_latin.size()) == 2.0 &&
              trailing_latin.element(1.0)->script == flight::String("Latn"),
          "itemize: a Latin letter after Cyrillic opens a Latin run rather than being absorbed");

    // A neutral character belongs to the run it appears in, so the space does not split anything.
    const auto spaced = flight::textshaper::itemize_text(flight::String("Hello World"), format);
    check(static_cast<double>(spaced.size()) == 1.0 && spaced.element(0.0)->start == 0.0 &&
              spaced.element(0.0)->end == 11.0,
          "itemize: a neutral space joins the current run rather than splitting it");

    // Latin then Arabic: the script AND direction both change, so the string is two runs.
    flight::String mixed("AB");
    mixed = mixed + arabic_salam();
    const auto split = flight::textshaper::itemize_text(mixed, format);
    check(static_cast<double>(split.size()) == 2.0, "itemize: Latin then Arabic is two runs");
    check(split.element(0.0)->direction == flight::String("LeftToRight") &&
              split.element(0.0)->script == flight::String("Latn"),
          "itemize: the Latin run keeps LTR and Latn");
    check(split.element(1.0)->direction == flight::String("RightToLeft") &&
              split.element(1.0)->script == flight::String("Arab"),
          "itemize: the Arabic run is RTL and Arab");

    // An explicit base direction is only a FALLBACK: strong characters still decide their own run.
    auto options = flight::make_ref<flight::types::TextShaperOptions>();
    options->direction = flight::String("RightToLeft");
    const auto overridden = flight::textshaper::itemize_text(flight::String("Hello"), format, options);
    check(static_cast<double>(overridden.size()) == 1.0 &&
              overridden.element(0.0)->direction == flight::String("LeftToRight"),
          "itemize: a strong LTR string overrides an RTL base direction");

    // The runs must tile the string: no gaps, no overlaps.
    flight::String tiled("Hi ");
    tiled = tiled + arabic_salam();
    tiled = tiled + flight::String(" end");
    const auto items = flight::textshaper::itemize_text(tiled, format);
    double covered = 0.0;
    bool contiguous = true;
    for (std::size_t index = 0; index < static_cast<std::size_t>(items.size()); ++index) {
      const auto& item = items.element(static_cast<double>(index));
      covered += item->end - item->start;
      if (index > 0 && item->start != items.element(static_cast<double>(index) - 1.0)->end) {
        contiguous = false;
      }
    }
    check_close(covered, static_cast<double>(tiled.length()),
                "itemize: the runs cover every code unit of the string");
    check(contiguous, "itemize: consecutive runs meet with no gap");
  }

  // shapeTextRuns threads a supplied host through each item; there is no ambient backend.
  {
    const auto format = a_format();
    const auto shaping = flight::make_ref<flight::types::HostTextShaperCapability>();
    shaping->measure_text = [](flight::String text, flight::Ref<flight::types::TextFormat>) {
      return static_cast<double>(text.length()) * 8.0;
    };
    std::vector<std::pair<flight::String, flight::String>> captured;
    shaping->shape_run = [&captured](flight::String, auto, auto options) {
      const auto run = flight::make_ref<flight::types::ShapedRun>();
      if (options.has_value()) {
        run->direction = options.value()->direction.value_or(flight::String("LeftToRight"));
        run->script = options.value()->script.value_or(flight::String("Latn"));
        captured.emplace_back(run->direction, run->script);
      }
      return run;
    };

    flight::String mixed("AB");
    mixed = mixed + arabic_salam();

    const auto runs = flight::textshaper::shape_text_runs(shaping, mixed, format);
    check(static_cast<double>(runs.size()) == 2.0,
          "shapeTextRuns: a mixed-script string shapes one run per item");
    check(runs.element(0.0)->direction == flight::String("LeftToRight") &&
              runs.element(1.0)->direction == flight::String("RightToLeft"),
          "shapeTextRuns: each shaped run carries its item's direction");
    check(captured.size() == 2 && captured[0].first == flight::String("LeftToRight") &&
              captured[1].first == flight::String("RightToLeft") &&
              captured[1].second == flight::String("Arab"),
          "shapeTextRuns: each item's direction and script reach the host");

    check(static_cast<double>(
              flight::textshaper::shape_text_runs(shaping, flight::String(""), format).size()) == 0.0,
          "shapeTextRuns: an empty string shapes nothing");

    // An advances-only host cannot shape: the seam returns nothing rather than synthesising runs.
    const auto advances_only = flight::make_ref<flight::types::HostTextShaperCapability>();
    advances_only->measure_text = [](flight::String text, flight::Ref<flight::types::TextFormat>) {
      return static_cast<double>(text.length());
    };
    check(static_cast<double>(flight::textshaper::shape_text_runs(advances_only, flight::String("Hello"),
                                                                  format)
                                  .size()) == 0.0,
          "shapeTextRuns: a host that cannot shape yields no runs, not a fake one");
  }


  // ---- textShaperRun.test.ts ---------------------------------------------------------------------
  //
  // This is where the HOST BOUNDARY is stated: a host that does not implement an optional operation is
  // compliant, and every query below answers with the absence marker (-1, null, "", false) rather than
  // inventing a value. The `_backend`/`_advancesOnly` pair from the upstream fixture is built here as
  // full_backend()/advances_only_backend().

  {
    const auto run = flight::textshaper::create_shaped_run();
    run->advance_width = 42.0;
    run->glyphs.push(flight::make_ref<flight::types::ShapedGlyph>());
    const auto returned = flight::textshaper::clear_shaped_run(run);
    check_close(run->advance_width, 0.0, "run: clearing resets the advance width");
    check_close(run->glyph_count, 0.0, "run: clearing resets the glyph count");
    check(static_cast<double>(run->glyphs.size()) == 0.0, "run: clearing empties the glyph array");
    check(run->direction == flight::String("LeftToRight"), "run: clearing restores the default direction");
    check(run->script == flight::String(""), "run: clearing empties the script");
    check(!run->font.has_value(), "run: clearing drops the font");
    check(returned == run, "run: clearing returns the same run it was given");
  }

  {
    // The glyph array is a shared value, so clearing truncates the one the run already held rather
    // than installing a new one -- a caller's retained handle keeps seeing the run's glyphs.
    const auto run = flight::textshaper::create_shaped_run();
    const auto retained = run->glyphs;
    run->glyphs.push(flight::make_ref<flight::types::ShapedGlyph>());
    check(static_cast<double>(retained.size()) == 1.0,
          "run: a retained handle to the glyphs array observes the run's own array");
    flight::textshaper::clear_shaped_run(run);
    check(static_cast<double>(retained.size()) == 0.0,
          "run: clearing truncates the run's existing glyphs array rather than replacing it");
  }

  {
    const auto fresh = flight::textshaper::create_shaped_run();
    check_close(fresh->advance_width, 0.0, "run: a new run has no advance width");
    check_close(fresh->glyph_count, 0.0, "run: a new run has no glyphs");
    check(static_cast<double>(fresh->glyphs.size()) == 0.0, "run: a new run's glyph array is empty");
    check(fresh->direction == flight::String("LeftToRight"), "run: a new run starts left-to-right");
    check(!fresh->font.has_value(), "run: a new run has no font");
    check(fresh->script == flight::String(""), "run: a new run has no script");
    check(flight::textshaper::create_shaped_run() != fresh, "run: each call allocates a distinct run");
  }

  {
    const auto backend = full_backend();
    const auto advances_only = advances_only_backend();
    const auto format = a_format();

    check_close(flight::textshaper::get_code_point_for_glyph(advances_only, 10.0, format), -1.0,
                "run: an unimplemented codepoint lookup answers -1");
    check_close(flight::textshaper::get_code_point_for_glyph(backend, 10.0, format), 65.0,
                "run: a codepoint lookup delegates to the host");
    check_close(flight::textshaper::get_code_point_for_glyph(backend, 999.0, format), -1.0,
                "run: a glyph the host cannot reverse-map answers -1");

    check(!flight::textshaper::get_font_metrics(advances_only, format).has_value(),
          "run: an advances-only host has no font metrics");
    const auto metrics = flight::textshaper::get_font_metrics(backend, format);
    check(metrics.has_value() && metrics.value()->ascent == 10.0 && metrics.value()->units_per_em == 1000.0,
          "run: font metrics delegate to the host");

    // The _into variant is the mutation-in-place form: on absence it reports false and leaves the
    // caller's object ALONE, which is what lets a caller keep a previous value on failure.
    {
      const auto out = flight::make_ref<flight::types::FontMetrics>();
      out->ascent = 99.0;
      check(!flight::textshaper::get_font_metrics_into(advances_only, format, out),
            "run: metrics-into reports false when the host has none");
      check_close(out->ascent, 99.0, "run: metrics-into leaves the caller's object untouched on failure");
      check(flight::textshaper::get_font_metrics_into(backend, format, out),
            "run: metrics-into reports true when the host has metrics");
      check_close(out->ascent, 10.0, "run: metrics-into writes the ascent through");
      check_close(out->cap_height, 8.0, "run: metrics-into writes every field");
    }

    // The unit scale is size / unitsPerEm, with the 12pt default when the format names no size.
    check_close(flight::textshaper::get_font_unit_scale(advances_only, format), -1.0,
                "run: unit scale is -1 when the host has no metrics");
    check_close(flight::textshaper::get_font_unit_scale(backend, format), 12.0 / 1000.0,
                "run: unit scale uses the default size when the format names none");
    const auto sized = flight::make_ref<flight::types::TextFormat>();
    sized->size = 20.0;
    check_close(flight::textshaper::get_font_unit_scale(backend, sized), 20.0 / 1000.0,
                "run: unit scale uses the format's size");

    check(!flight::textshaper::get_glyph_extents(advances_only, 10.0, format).has_value(),
          "run: an advances-only host has no glyph extents");
    const auto extents = flight::textshaper::get_glyph_extents(backend, 10.0, format);
    check(extents.has_value() && extents.value()->width == 6.0, "run: glyph extents delegate to the host");
    check(!flight::textshaper::get_glyph_extents(backend, 999.0, format).has_value(),
          "run: an unknown glyph has no extents");

    {
      // The batch form fills a caller-sized array: known glyphs get the host's extents, unknown ones
      // get a ZEROED entry so the output is fully populated, and the return counts only the known.
      flight::Array<double> ids{10.0, 999.0};
      flight::Array<flight::Ref<flight::types::GlyphExtents>> out;
      out.resize(2.0);
      check_close(flight::textshaper::get_glyph_extents_batch(backend, ids, format, out), 1.0,
                  "run: the extents batch counts only the glyphs that resolved");
      check(static_cast<double>(out.size()) == 2.0, "run: the extents batch populates the whole array");
      check_close(out.element(0.0)->width, 6.0, "run: a known glyph gets the host's extents");
      check_close(out.element(1.0)->width, 0.0, "run: an unknown glyph gets a zeroed entry");

      flight::Array<double> reversed{999.0, 10.0};
      flight::Array<flight::Ref<flight::types::GlyphExtents>> reversed_out;
      reversed_out.resize(2.0);
      flight::textshaper::get_glyph_extents_batch(backend, reversed, format, reversed_out);
      check_close(reversed_out.element(0.0)->height, 0.0, "run: the zeroed entry is at the unknown index");
      check_close(reversed_out.element(1.0)->height, 10.0, "run: the resolved entry is at the known index");

      flight::Array<double> none;
      flight::Array<flight::Ref<flight::types::GlyphExtents>> empty_out;
      check_close(flight::textshaper::get_glyph_extents_batch(backend, none, format, empty_out), 0.0,
                  "run: an empty glyph list resolves nothing");
      check(static_cast<double>(empty_out.size()) == 0.0, "run: an empty glyph list writes nothing");

      flight::Array<double> ids_absent{10.0};
      flight::Array<flight::Ref<flight::types::GlyphExtents>> out_absent;
      out_absent.resize(1.0);
      out_absent.element(0.0) = flight::make_ref<flight::types::GlyphExtents>();
      out_absent.element(0.0)->width = 7.0;
      check_close(flight::textshaper::get_glyph_extents_batch(advances_only, ids_absent, format, out_absent),
                  0.0,
                  "run: the extents batch resolves nothing without the host operation");
      check_close(out_absent.element(0.0)->width, 7.0,
                  "run: the extents batch leaves the caller's array untouched without the host operation");
    }

    {
      const auto out = flight::make_ref<flight::types::GlyphExtents>();
      out->width = 1.0;
      check(!flight::textshaper::get_glyph_extents_into(advances_only, 10.0, format, out),
            "run: extents-into reports false when the host has none");
      check_close(out->width, 1.0, "run: extents-into leaves the caller's object untouched on failure");
      check(flight::textshaper::get_glyph_extents_into(backend, 10.0, format, out),
            "run: extents-into reports true when the host has extents");
      check_close(out->width, 6.0, "run: extents-into writes the width through");
      check_close(out->y_bearing, -8.0, "run: extents-into writes every field");
    }

    check_close(flight::textshaper::get_glyph_index_for_code_point(advances_only, 65.0, format), -1.0,
                "run: an unimplemented glyph index answers -1");
    check_close(flight::textshaper::get_glyph_index_for_code_point(backend, 65.0, format), 10.0,
                "run: a glyph index delegates to the host");
    check_close(flight::textshaper::get_glyph_index_for_code_point(backend, 9731.0, format), -1.0,
                "run: a code point with no glyph answers -1");

    check(flight::textshaper::get_glyph_name(advances_only, 10.0, format) == flight::String(""),
          "run: an unimplemented glyph name is the empty string");
    check(flight::textshaper::get_glyph_name(backend, 10.0, format) == flight::String("A"),
          "run: a glyph name delegates to the host");
    check(flight::textshaper::get_glyph_name(backend, 999.0, format) == flight::String(""),
          "run: a glyph the host cannot name is the empty string");
  }

  {
    const auto backend = full_backend();
    const auto advances_only = advances_only_backend();
    const auto format = a_format();

    // An advances-only host cannot shape, and the seam says so rather than synthesising a run.
    check(!flight::textshaper::shape_text_run(advances_only, flight::String("hi"), format).has_value(),
          "run: an advances-only host cannot shape a run");
    const auto shaped = flight::textshaper::shape_text_run(backend, flight::String("ab"), format);
    check(shaped.has_value() && shaped.value()->glyph_count == 2.0 &&
              shaped.value()->direction == flight::String("LeftToRight"),
          "run: shaping delegates to the host");

    // The options object reaches the host unchanged.
    std::optional<std::pair<flight::String, flight::String>> captured;
    const auto capturing = flight::make_ref<flight::types::HostTextShaperCapability>();
    capturing->measure_text = [](flight::String, flight::Ref<flight::types::TextFormat>) { return 0.0; };
    capturing->shape_run = [&captured](flight::String, auto, auto options) {
      if (options.has_value()) {
        captured = std::make_pair(options.value()->direction.value_or(flight::String("")),
                                  options.value()->script.value_or(flight::String("")));
      }
      return full_backend_shape();
    };
    auto options = flight::make_ref<flight::types::ShapeRunOptions>();
    options->direction = flight::String("RightToLeft");
    options->script = flight::String("Arab");
    flight::textshaper::shape_text_run(capturing, flight::String("x"), format, options);
    check(captured.has_value() && captured->first == flight::String("RightToLeft") &&
              captured->second == flight::String("Arab"),
          "run: the shape options reach the host unchanged");
  }

  {
    const auto backend = full_backend();
    const auto advances_only = advances_only_backend();
    const auto format = a_format();

    {
      const auto out = flight::textshaper::create_shaped_run();
      check(!flight::textshaper::shape_text_run_into(advances_only, flight::String("hi"), format, out),
            "run: shaping-into reports false without the host operation");
      check_close(out->glyph_count, 0.0, "run: shaping-into leaves the run untouched on failure");
    }

    {
      const auto out = flight::textshaper::create_shaped_run();
      check(flight::textshaper::shape_text_run_into(backend, flight::String("ab"), format, out),
            "run: shaping-into reports true with a shaping host");
      check_close(out->advance_width, 15.0, "run: shaping-into writes the advance width");
      check_close(out->glyph_count, 2.0, "run: shaping-into writes the glyph count");
      check(static_cast<double>(out->glyphs.size()) == 2.0, "run: shaping-into writes the glyphs");
      check_close(out->glyphs.element(0.0)->glyph_id, 10.0, "run: shaping-into writes each glyph");
      check(out->script == flight::String("Latn"), "run: shaping-into writes the script");
    }

    {
      // The glyphs array is filled IN PLACE: the run keeps the array it already had, so a handle taken
      // before the shape sees the new contents.
      const auto out = flight::textshaper::create_shaped_run();
      const auto retained = out->glyphs;
      flight::textshaper::shape_text_run_into(backend, flight::String("ab"), format, out);
      check(static_cast<double>(retained.size()) == 2.0,
            "run: shaping-into retains the run's existing glyphs array rather than replacing it");
    }

    {
      std::optional<std::pair<flight::String, flight::String>> captured;
      const auto capturing = flight::make_ref<flight::types::HostTextShaperCapability>();
      capturing->measure_text = [](flight::String, flight::Ref<flight::types::TextFormat>) { return 0.0; };
      capturing->shape_run = [&captured](flight::String, auto, auto options) {
        if (options.has_value()) {
          captured = std::make_pair(options.value()->direction.value_or(flight::String("")),
                                    options.value()->script.value_or(flight::String("")));
        }
        return full_backend_shape();
      };
      const auto out = flight::textshaper::create_shaped_run();
      auto options = flight::make_ref<flight::types::ShapeRunOptions>();
      options->direction = flight::String("RightToLeft");
      options->script = flight::String("Arab");
      flight::textshaper::shape_text_run_into(capturing, flight::String("x"), format, out, options);
      check(captured.has_value() && captured->first == flight::String("RightToLeft") &&
                captured->second == flight::String("Arab"),
            "run: shaping-into forwards the options to the host");
    }
  }

  return failures == 0 ? 0 : 1;
}
