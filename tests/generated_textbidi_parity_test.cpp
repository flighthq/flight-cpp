// Behavioural parity for @flighthq/textbidi's resolveBidiLevels, driven by the upstream Vitest
// fixtures in .dependencies/flight/packages/textbidi/src/resolveBidiLevels.test.ts and
// bidiConformance.test.ts.
//
// This package matters more than most for parity: resolveBidiLevels is supplied by the hand-written
// override overrides/include/flight/textbidi/resolve_bidi_levels.hpp, which replaces the whole module
// and had no behavioural test anywhere. These cases are therefore checking OUR resolver against the
// upstream intent, not the emitter's output.
//
// The fixtures keep the upstream expectations exactly, including the deliberate distinctions that make
// the algorithm's rules observable: W2 (EN after AL becomes AN), W7 (EN stays level 0 after L), N0
// (bracket pairs), N1/N2 (neutrals), X2/X3 (embeddings), X5a/X5b/X5c (isolates), X6 (overrides) and
// L1 (line reset). Character escapes are written out so the fixtures do not depend on this file's
// encoding.

#include <flight/textbidi/bidi_class_kernel.hpp>
#include <flight/textbidi/enable_text_bidi_guards.hpp>
#include <flight/textbidi/get_bidi_runs.hpp>
#include <flight/textbidi/text_bidi_guards.hpp>
#include <flight/log/log.hpp>
#include <flight/types/log.hpp>
#include <flight/textbidi/reorder_bidi_line.hpp>
#include <flight/textbidi/resolve_bidi_levels.hpp>
#include <flight/types/bidi.hpp>

#include <cstdint>
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

// Explicit UTF-16 so the fixtures read the same as upstream regardless of this file's encoding.
flight::String text16(const std::u16string& value) { return flight::String(value); }

// Upstream's strong characters, quoted from the test file's own comments.
const std::u16string HEBREW = u"שלום";   // "שלום" - four strong-R letters
const std::u16string ARABIC = u"ابتثج";  // AL letters
const std::u16string ARABIC_DIGITS = u"٠١٢";  // AN digits
// The conformance fixtures use the two-letter "אב" (U+05D0 U+05D1), NOT the four-letter
// HEBREW above. Using HEBREW here would silently lengthen the expected level arrays, which is
// how this test first failed -- the run reported 6 levels where upstream expects 4.
const std::u16string ALEF_BET = u"אב";

const std::u16string LRE = u"‪";
const std::u16string RLE = u"‫";
const std::u16string PDF = u"‬";
const std::u16string LRO = u"‭";
const std::u16string RLO = u"‮";
const std::u16string LRI = u"⁦";
const std::u16string RLI = u"⁧";
const std::u16string FSI = u"⁨";
const std::u16string PDI = u"⁩";

using KernelRef = flight::Ref<flight::types::BidiClassKernel>;
using KernelRow = flight::StructuralRef<flight::RowReadonly<flight::RowOf<KernelRef>>>;

KernelRow as_row(KernelRef kernel) {
  return flight::structural_ref_cast<KernelRow>(
      flight::StructuralRef<flight::RowWritable<flight::RowOf<KernelRef>>>(kernel));
}

// Upstream's LEFT_TO_RIGHT_BACKEND / RIGHT_TO_LEFT_BACKEND: a kernel that answers one class for
// every code point, which is how the lookups are proven to route through the caller's kernel.
KernelRef constant_kernel(const char* cls) {
  const auto kernel = flight::make_ref<flight::types::BidiClassKernel>();
  const flight::String answer(cls);
  kernel->get_bidi_class = [answer](double) { return answer; };
  return kernel;
}

std::vector<double> levels_of(const std::u16string& text, const char* direction) {
  const flight::Uint8Array levels = flight::textbidi::resolve_bidi_levels(
      as_row(flight::textbidi::compact_bidi_class_kernel),
      text16(text),
      flight::String(direction));
  std::vector<double> out;
  for (double index = 0.0; index < static_cast<double>(levels.length()); index += 1.0) {
    out.push_back(static_cast<double>(levels.get_index(index)));
  }
  return out;
}

void expect_levels(const std::u16string& text, const char* direction, const std::vector<double>& want,
                   const char* message) {
  const std::vector<double> got = levels_of(text, direction);
  if (got == want) return;
  std::cerr << "FAIL: " << message << "\n  want [";
  for (const double value : want) std::cerr << value << ' ';
  std::cerr << "]\n  got  [";
  for (const double value : got) std::cerr << value << ' ';
  std::cerr << "]\n";
  ++failures;
}

struct Run {
  double start;
  double end;
  double level;
  flight::String direction;

  bool operator==(const Run& other) const {
    return start == other.start && end == other.end && level == other.level &&
           direction == other.direction;
  }
};

void expect_runs(const std::u16string& text, const char* direction, const std::vector<Run>& want,
                 const char* message) {
  const auto runs = flight::textbidi::get_bidi_runs(
      as_row(flight::textbidi::compact_bidi_class_kernel), text16(text), flight::String(direction));
  std::vector<Run> got;
  for (double index = 0.0; index < static_cast<double>(runs.length()); index += 1.0) {
    const auto& run = runs.element(index);
    got.push_back(Run{run->start, run->end, run->level, run->direction});
  }
  bool same = got.size() == want.size();
  for (std::size_t index = 0; same && index < got.size(); ++index) {
    same = got[index].start == want[index].start && got[index].end == want[index].end &&
           got[index].level == want[index].level && got[index].direction == want[index].direction;
  }
  if (same) return;
  std::cerr << "FAIL: " << message << "\n  want ";
  for (const auto& run : want) std::cerr << "(" << run.start << "," << run.end << "," << run.level << ") ";
  std::cerr << "\n  got  ";
  for (const auto& run : got) std::cerr << "(" << run.start << "," << run.end << "," << run.level << ") ";
  std::cerr << "\n";
  ++failures;
}

}  // namespace

int main() {
  // ---- resolveBidiLevels.test.ts -------------------------------------------------------------

  // The kernel is the caller's, not a module global: the same text yields R levels or L levels
  // depending on what the caller hands in, and interleaved calls do not contaminate each other.
  {
    const auto rtl = constant_kernel("R");
    const auto ltr = constant_kernel("L");
    auto levels_for = [](KernelRef kernel, const char* value) {
      const flight::Uint8Array levels = flight::textbidi::resolve_bidi_levels(
          as_row(kernel), text16(std::u16string(value, value + std::char_traits<char>::length(value))),
          flight::String("ltr"));
      std::vector<double> out;
      for (double index = 0.0; index < static_cast<double>(levels.length()); index += 1.0) {
        out.push_back(static_cast<double>(levels.get_index(index)));
      }
      return out;
    };
    check(levels_for(rtl, "abc") == std::vector<double>({1, 1, 1}),
          "an explicit kernel answers every class lookup (R)");
    check(levels_for(ltr, "abc") == std::vector<double>({0, 0, 0}),
          "an explicit kernel answers every class lookup (L)");
    check(levels_for(rtl, "abc") == std::vector<double>({1, 1, 1}),
          "interleaved callers with different kernels are isolated");
  }

  expect_levels(u"hello", "auto", {0, 0, 0, 0, 0}, "pure LTR text is level 0");
  expect_levels(HEBREW, "auto", {1, 1, 1, 1}, "pure Hebrew is auto-detected as RTL (level 1)");
  expect_levels(ARABIC + ARABIC_DIGITS, "auto", {1, 1, 1, 1, 1, 2, 2, 2},
                "Arabic-Indic digits stay at level 1 in an RTL paragraph (W2 bypass)");
  expect_levels(ARABIC + u"123", "auto", {1, 1, 1, 1, 1, 2, 2, 2},
                "EN after AL becomes AN via W2 in an RTL paragraph");
  expect_levels(HEBREW + u" ab", "auto", {1, 1, 1, 1, 1, 2, 2},
                "a leading strong-R derives an rtl paragraph under auto");
  expect_levels(u"hello " + HEBREW + u" world", "auto",
                {0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0},
                "an RTL run embeds at an odd level inside an LTR paragraph");
  expect_levels(HEBREW + u" 123", "auto", {1, 1, 1, 1, 1, 2, 2, 2},
                "European numbers in RTL context take an even level above the RTL text");
  expect_levels(u"12ab", "ltr", {0, 0, 0, 0},
                "an explicit ltr base keeps pure-LTR text at level 0 even with a leading number");
  expect_levels(u"abc 123 def", "auto", {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                "EN stays level 0 after L via W7 in an LTR paragraph");
  expect_levels(HEBREW + LRE + u"ab" + PDF + HEBREW, "auto",
                {1, 1, 1, 1, 1, 2, 2, 1, 1, 1, 1, 1},
                "LRE embeds at level 2 inside an RTL paragraph (X3)");
  expect_levels(u"ab" + RLE + HEBREW + PDF + u"cd", "auto", {0, 0, 0, 1, 1, 1, 1, 0, 0, 0},
                "RLE embeds at level 1 inside an LTR paragraph (X2)");
  // RLE inside an RTL paragraph is where nextOdd's ODD-FORCING becomes observable: the embedding is
  // pushed to next_odd(1) = 3, and L content in an odd-level run is then raised once more to 4 by
  // I1/I2. Every other embedding fixture here starts from an even paragraph level, where
  // `level + 1` and `bitwise_or(level + 1, 1)` agree -- so a nextOdd that merely incremented left the
  // whole suite passing until this case existed.
  expect_levels(HEBREW + RLE + u"abc" + PDF + HEBREW, "auto", {1, 1, 1, 1, 1, 4, 4, 4, 1, 1, 1, 1, 1},
                "an RLE inside an RTL paragraph forces the embedding odd, and L content rises again (X2/I1)");
  expect_levels(u"hello " + HEBREW + u" world", "rtl",
                {2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2},
                "an explicit rtl base raises the paragraph level");
  expect_levels(u"abc " + HEBREW + u"   ", "auto", {0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0},
                "trailing whitespace resets to the paragraph level via L1");

  // The upstream fixture above is faithful but NOT L1-sensitive: the trailing spaces resolve to the
  // paragraph level through N1/N2 anyway, so removing the L1 trailing reset leaves it passing. A
  // mutation check found that, so these two are constructed from the UAX#9 L1 rule itself to make the
  // rule observable -- whitespace INSIDE a still-open embedding at the end of the line must reset to
  // the paragraph level rather than keeping the embedding level. They are marked as constructed, not
  // quoted from upstream.
  expect_levels(u"a" + RLE + HEBREW + u" ", "ltr", {0, 0, 1, 1, 1, 1, 0},
                "L1 (constructed): trailing whitespace inside an open RLE resets to the paragraph level");
  expect_levels(u"a" + LRE + HEBREW + u" ", "ltr", {0, 0, 3, 3, 3, 3, 0},
                "L1 (constructed): trailing whitespace inside an open LRE resets to the paragraph level");
  expect_levels(HEBREW + LRI + u"ab" + PDI + HEBREW, "auto",
                {1, 1, 1, 1, 1, 2, 2, 1, 1, 1, 1, 1},
                "LRI resolves to an even level inside an RTL paragraph (X5b)");
  expect_levels(u"ab" + RLI + HEBREW + PDI + u"cd", "auto", {0, 0, 0, 1, 1, 1, 1, 0, 0, 0},
                "RLI resolves to an odd level inside an LTR paragraph (X5a)");
  expect_levels(u"ab" + FSI + HEBREW + PDI + u"cd", "auto", {0, 0, 0, 1, 1, 1, 1, 0, 0, 0},
                "FSI scores RTL content and resolves as RLI (X5c)");
  expect_levels(HEBREW + FSI + u"ab" + PDI + HEBREW, "auto",
                {1, 1, 1, 1, 1, 2, 2, 1, 1, 1, 1, 1},
                "FSI scores LTR content and resolves as LRI (X5c)");
  // The paragraph level is derived by P2/P3, which SKIP everything between an isolate initiator and
  // its matching PDI. Every isolate fixture above has a strong character OUTSIDE the isolate before
  // the isolate begins, so the skip never changed the answer and a paragraph scan that looked inside
  // isolates left the suite passing. Here the ONLY strong text is inside the isolate, so the two
  // disagree: skipping it leaves no strong character at all and the base is 0, while looking inside
  // derives 1 from the Hebrew. Constructed from the rule rather than quoted from upstream.
  expect_levels(LRI + HEBREW + PDI, "auto", {0, 3, 3, 3, 3, 0},
                "the paragraph level skips text inside an isolate, so a lone isolated R stays base 0 (P2/P3)");
  expect_levels(u"abc!?@def", "auto", {0, 0, 0, 0, 0, 0, 0, 0, 0},
                "neutrals between same-direction strong types resolve via N1 (LTR)");
  expect_levels(HEBREW + u"!?" + HEBREW, "auto", {1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
                "neutrals between R and R resolve to R via N1");
  expect_levels(u"abc!" + HEBREW, "auto", {0, 0, 0, 0, 1, 1, 1, 1},
                "neutrals between opposite strong types take the embedding direction via N2");
  expect_levels(HEBREW + u"(" + HEBREW + u")abc", "ltr",
                {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0},
                "both brackets resolve from enclosed opposite-direction text via N0");
  expect_levels(HEBREW + u"([abc])" + HEBREW, "rtl",
                {1, 1, 1, 1, 1, 1, 2, 2, 2, 1, 1, 1, 1, 1, 1},
                "nested brackets pair within one isolating run sequence");
  expect_levels(u"a" + LRO + HEBREW + PDF + u"b", "auto", {0, 0, 2, 2, 2, 2, 0, 0},
                "an LRO override forces all content to L (X6)");
  expect_levels(HEBREW + RLO + u"ab" + PDF + HEBREW, "auto",
                {1, 1, 1, 1, 1, 3, 3, 1, 1, 1, 1, 1},
                "an RLO override forces all content to R (X6)");

  // Absence: an empty string yields no levels at all, not an error and not a default entry.
  check(levels_of(u"", "auto").empty(), "empty text returns an empty level array");

  // ---- bidiConformance.test.ts ---------------------------------------------------------------

  expect_levels(u"a1-2b", "ltr", {0, 0, 0, 0, 0}, "conformance: LTR weak chain");
  expect_levels(u"!" + ALEF_BET + u"!", "rtl", {1, 1, 1, 1},
                "conformance: RTL neutrals");
  expect_levels(ALEF_BET + u" 12", "rtl", {1, 1, 1, 2, 2}, "conformance: numbers in RTL");
  expect_levels(u"a" + RLI + ALEF_BET + PDI + u"b", "ltr", {0, 0, 1, 1, 0, 0},
                "conformance: isolate inside LTR");

  {
    // Explicit embedding depth is capped at 125 and overflow embeddings are ignored: the 126th RLE
    // still contributes the overflow level, and the resolved level at that index is exactly 126.
    std::u16string text;
    for (int index = 0; index < 126; ++index) text += RLE;
    text += u"a";
    for (int index = 0; index < 126; ++index) text += PDF;
    const std::vector<double> levels = levels_of(text, "ltr");
    check(levels.size() > 126 && levels[126] == 126.0,
          "conformance: explicit embedding depth caps at 126");
  }

  {
    // reorderBidiLine is the ordering half of the same fixture: the resolved levels of "אב 12" under
    // an rtl base reorder to 3,4,2,1,0 -- the digits keep their own order while the whole run flips.
    const std::u16string text = ALEF_BET + u" 12";
    const flight::Uint8Array levels = flight::textbidi::resolve_bidi_levels(
        as_row(flight::textbidi::compact_bidi_class_kernel), text16(text), flight::String("rtl"));
    flight::Array<double> visual;
    flight::textbidi::reorder_bidi_line(levels, 0.0, static_cast<double>(levels.length()), visual);
    std::vector<double> got;
    for (double index = 0.0; index < static_cast<double>(visual.length()); index += 1.0) {
      got.push_back(visual.element(index));
    }
    check(got == std::vector<double>({3, 4, 2, 1, 0}),
          "conformance: reorderBidiLine flips the run while preserving digit order");
  }

  // ---- bidiClassKernel.test.ts ---------------------------------------------------------------

  // The compact table is the shared kernel the bidi operations are handed, and it answers the
  // representative common-script sample the upstream test names explicitly.
  {
    const auto class_of = [](double codepoint) {
      return flight::textbidi::compact_bidi_class_kernel->get_bidi_class(codepoint);
    };
    check(class_of(0x0061) == flight::String("L"), "compact kernel: 'a' (Latn) is L");
    check(class_of(0x005A) == flight::String("L"), "compact kernel: 'Z' is L");
    check(class_of(0x05D0) == flight::String("R"), "compact kernel: U+05D0 Hebrew alef is R");
    check(class_of(0x0627) == flight::String("AL"), "compact kernel: U+0627 Arabic alef is AL");
    check(class_of(0x0035) == flight::String("EN"), "compact kernel: '5' is EN");
    check(class_of(0x0660) == flight::String("AN"), "compact kernel: U+0660 Arabic-Indic zero is AN");
    check(class_of(0x0020) == flight::String("WS"), "compact kernel: space is WS");
    check(class_of(0x0301) == flight::String("NSM"), "compact kernel: U+0301 combining acute is NSM");
    check(class_of(0x2066) == flight::String("LRI"), "compact kernel: U+2066 LRI is LRI");
    check(class_of(0x2069) == flight::String("PDI"), "compact kernel: U+2069 PDI is PDI");
    check(class_of(0x202B) == flight::String("RLE"), "compact kernel: U+202B RLE is RLE");
    // A codepoint outside the compact table resolves to the LTR default rather than failing.
    check(class_of(0x4E2D) == flight::String("L"),
          "compact kernel: an uncovered codepoint (CJK U+4E2D) falls back to L");
  }

  // explainBidiClassKernel describes the boundary a caller is relying on: a compact kernel claims the
  // flattened common-script ranges and a proven-valid table, a custom one claims neither.
  {
    const auto explanation = flight::textbidi::explain_bidi_class_kernel(
        as_row(flight::textbidi::compact_bidi_class_kernel));
    check(explanation->kernel == flight::String("compact"), "explanation: kernel is 'compact'");
    check(explanation->coverage == flight::String("common-script-ranges"),
          "explanation: coverage is 'common-script-ranges'");
    check(explanation->fallback_class == std::optional<flight::String>{flight::String("L")},
          "explanation: fallback class is L");
    check(explanation->table_valid == std::optional<bool>{true},
          "explanation: the compact table reports itself valid");
    const auto& ranges = explanation->covered_code_point_ranges;
    bool ordered = true;
    for (double index = 0.0; index < static_cast<double>(ranges.length()); index += 1.0) {
      const auto& range = ranges.element(index);
      if (range->start > range->end) ordered = false;
      if (index > 0.0 && range->start <= ranges.element(index - 1.0)->end) ordered = false;
    }
    check(ordered && static_cast<double>(ranges.length()) > 0.0,
          "explanation: covered ranges are non-empty, ordered, and never inverted");
  }

  {
    const auto custom = constant_kernel("R");
    const auto explanation = flight::textbidi::explain_bidi_class_kernel(as_row(custom));
    check(explanation->kernel == flight::String("custom"),
          "explanation: a caller-supplied kernel is 'custom'");
    check(explanation->coverage == flight::String("provider-defined"),
          "explanation: a custom kernel claims provider-defined coverage");
    check(static_cast<double>(explanation->covered_code_point_ranges.length()) == 0.0,
          "explanation: a custom kernel claims no covered ranges");
    check(!explanation->fallback_class.has_value() && !explanation->table_valid.has_value(),
          "explanation: a custom kernel claims neither a fallback class nor a valid table");
  }

  // ---- getBidiRuns.test.ts --------------------------------------------------------------------

  // Runs are the level array partitioned into maximal equal-level spans, each labelled by parity.
  {
    const auto runs_for = [](KernelRef kernel, const std::u16string& text, const char* direction) {
      const auto runs = flight::textbidi::get_bidi_runs(as_row(kernel), text16(text),
                                                       flight::String(direction));
      std::vector<Run> out;
      for (double index = 0.0; index < static_cast<double>(runs.length()); index += 1.0) {
        const auto& run = runs.element(index);
        out.push_back(Run{run->start, run->end, run->level, run->direction});
      }
      return out;
    };
    const auto rtl = constant_kernel("R");
    const auto ltr = constant_kernel("L");
    const std::vector<Run> want_rtl{{0, 3, 1, flight::String("rtl")}};
    const std::vector<Run> want_ltr{{0, 3, 0, flight::String("ltr")}};
    check(runs_for(rtl, u"abc", "ltr") == want_rtl,
          "runs: a caller kernel that answers R yields one rtl run");
    check(runs_for(ltr, u"abc", "ltr") == want_ltr,
          "runs: a caller kernel that answers L yields one ltr run");
    check(runs_for(rtl, u"abc", "ltr") == want_rtl,
          "runs: interleaved callers with different kernels are isolated");
  }

  expect_runs(ARABIC + u" 123 " + ARABIC, "auto",
              {{0, 6, 1, flight::String("rtl")}, {6, 9, 2, flight::String("ltr")}, {9, 15, 1, flight::String("rtl")}},
              "runs: Arabic with embedded European numbers yields three level runs");
  expect_runs(u"ab" + RLE + HEBREW + PDF + u"cd", "auto",
              {{0, 3, 0, flight::String("ltr")}, {3, 7, 1, flight::String("rtl")}, {7, 10, 0, flight::String("ltr")}},
              "runs: an RLE embedding in an LTR paragraph splits into ltr/rtl/ltr");
  expect_runs(HEBREW + LRE + u"ab" + PDF + HEBREW, "auto",
              {{0, 5, 1, flight::String("rtl")}, {5, 7, 2, flight::String("ltr")}, {7, 12, 1, flight::String("rtl")}},
              "runs: an LRE embedding in an RTL paragraph splits into rtl/ltr/rtl");
  expect_runs(ARABIC, "auto", {{0, 5, 1, flight::String("rtl")}}, "runs: pure RTL text is a single rtl run");
  expect_runs(u"hello", "auto", {{0, 5, 0, flight::String("ltr")}}, "runs: pure LTR text is a single ltr run");
  expect_runs(u"hello " + HEBREW + u" world", "auto",
              {{0, 6, 0, flight::String("ltr")}, {6, 10, 1, flight::String("rtl")}, {10, 16, 0, flight::String("ltr")}},
              "runs: mixed text splits into ltr / rtl / ltr with correct ranges");
  check(levels_of(u"", "auto").empty() &&
            flight::textbidi::get_bidi_runs(
                as_row(flight::textbidi::compact_bidi_class_kernel), text16(u""),
                flight::String("auto"))
                    .length() == 0.0,
        "runs: empty text yields no runs");

  // ---- reorderBidiLine.test.ts ----------------------------------------------------------------

  // The ordering half: level >= 2 reverses first, then level >= 1, which is what keeps a number run
  // inside RTL text reading left-to-right while the text around it flips.
  {
    const auto reorder = [](const std::vector<double>& levels, double start, double end) {
      const flight::Uint8Array buffer(
          [&levels] {
            std::vector<std::uint8_t> bytes;
            for (const double level : levels) bytes.push_back(static_cast<std::uint8_t>(level));
            return bytes;
          }());
      flight::Array<double> out;
      flight::textbidi::reorder_bidi_line(buffer, start, end, out);
      std::vector<double> got;
      for (double index = 0.0; index < static_cast<double>(out.length()); index += 1.0) {
        got.push_back(out.element(index));
      }
      return got;
    };

    check(reorder({1, 1, 1}, 0.0, 3.0) == std::vector<double>({2, 1, 0}),
          "reorder: a pure-RTL run reverses to visual order");
    check(reorder({0, 0, 0, 0}, 0.0, 4.0) == std::vector<double>({0, 1, 2, 3}),
          "reorder: a pure-LTR line stays in logical order");
    check(reorder({0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0}, 0.0, 16.0) ==
              std::vector<double>({0, 1, 2, 3, 4, 5, 9, 8, 7, 6, 10, 11, 12, 13, 14, 15}),
          "reorder: only the RTL run of a mixed line reverses");
    check(reorder({1, 1, 1, 1, 1, 2, 2, 2}, 0.0, 8.0) ==
              std::vector<double>({5, 6, 7, 4, 3, 2, 1, 0}),
          "reorder: digits nested in RTL text keep their internal LTR order");
    check(reorder({0, 1, 1, 1, 0}, 1.0, 4.0) == std::vector<double>({3, 2, 1}),
          "reorder: a sub-range within a larger level buffer reorders on its own");

    // The out array is reuse, not append: a shorter line must truncate it rather than leave a tail.
    {
      flight::Array<double> out;
      flight::textbidi::reorder_bidi_line(
          flight::Uint8Array(std::vector<std::uint8_t>{1, 1}), 0.0, 2.0, out);
      std::vector<double> first;
      for (double index = 0.0; index < static_cast<double>(out.length()); index += 1.0) {
        first.push_back(out.element(index));
      }
      flight::textbidi::reorder_bidi_line(
          flight::Uint8Array(std::vector<std::uint8_t>{0}), 0.0, 1.0, out);
      std::vector<double> second;
      for (double index = 0.0; index < static_cast<double>(out.length()); index += 1.0) {
        second.push_back(out.element(index));
      }
      check(first == std::vector<double>({1, 0}) && second == std::vector<double>({0}),
            "reorder: one out array can be reused across calls and truncates");
    }
  }

  // ---- textBidiGuards.test.ts -----------------------------------------------------------------

  // No guard installed means the miss report is silent and cannot throw: the compact fallback is not
  // an error, it is the documented answer for a codepoint outside the table.
  {
    flight::textbidi::set_text_bidi_guard(std::nullopt);
    bool threw = false;
    try {
      flight::textbidi::report_text_bidi_compact_table_miss(0x4E2D);
    } catch (...) {
      threw = true;
    }
    check(!threw, "guards: reporting a compact-table miss with no guard is silent");
  }

  // Installing a guard REPLACES the previous one rather than adding to it, so only the live callback
  // observes the report.
  {
    std::vector<double> first;
    std::vector<double> second;
    flight::textbidi::set_text_bidi_guard(
        std::function<void(double)>([&first](double codepoint) { first.push_back(codepoint); }));
    flight::textbidi::set_text_bidi_guard(
        std::function<void(double)>([&second](double codepoint) { second.push_back(codepoint); }));
    flight::textbidi::report_text_bidi_compact_table_miss(0x4E2D);
    check(first.empty() && second == std::vector<double>({0x4E2D}),
          "guards: installing a guard replaces the active callback");
    flight::textbidi::set_text_bidi_guard(std::nullopt);
  }

  // ---- enableTextBidiGuards.test.ts -----------------------------------------------------------

  // UNBLOCKED: the log path this suite could not exercise is now supplied.
  //
  // The upstream enableTextBidiGuards.test.ts cases assert on LOG ENTRIES. That was impossible while the
  // log override stubbed merge_span_fields as NotImplementedInThisProfile -- log_once builds every
  // structured entry through it, so emitting one threw -- and the block that stood here recorded that
  // rather than working around it. The log CORE override now supplies a real merge_span_fields, so the
  // cases are ported.
  {
    std::vector<std::string> entries;
    const flight::types::LogSink sink =
        [&entries](flight::Ref<flight::types::LogEntry> entry) {
          const auto data = entry->data;
          if (data.index() == 0) {
            const auto message = std::get<0>(data).get(flight::String("message"));
            entries.push_back(message.has_value() ? message.value().as_string().to_utf8()
                                                  : std::string("<none>"));
          } else {
            entries.push_back(std::get<1>(data).to_utf8());
          }
        };
    const auto kernel = as_row(flight::textbidi::compact_bidi_class_kernel);

    // disableTextBidiGuards: restores the silent compact fallback.
    entries.clear();
    flight::log::clear_log_once_keys();
    flight::log::set_log_sink(std::optional<flight::types::LogSink>{sink});
    flight::textbidi::enable_text_bidi_guards();
    flight::textbidi::disable_text_bidi_guards();
    static_cast<void>(
        flight::textbidi::resolve_bidi_levels(kernel, text16(u"中"), flight::String("ltr")));
    check(entries.empty(), "guards: a disabled guard restores the silent compact fallback");

    // enableTextBidiGuards: warns ONCE and names the kernel fixing call.
    entries.clear();
    flight::log::clear_log_once_keys();
    flight::textbidi::enable_text_bidi_guards();
    static_cast<void>(
        flight::textbidi::resolve_bidi_levels(kernel, text16(u"中文"), flight::String("ltr")));
    check(entries.size() == 1, "guards: an uncovered code point warns exactly once");
    check(!entries.empty() && entries[0].find("BidiClassKernel") != std::string::npos,
          "and the warning names the kernel fixing call");

    // ...and stays silent for compact-table coverage.
    entries.clear();
    flight::log::clear_log_once_keys();
    flight::textbidi::enable_text_bidi_guards();
    static_cast<void>(flight::textbidi::resolve_bidi_levels(kernel, text16(u"abc שלום ابت"),
                                                            flight::String("auto")));
    check(entries.empty(), "guards: compact-table coverage produces nothing");

    flight::textbidi::disable_text_bidi_guards();
    flight::log::set_log_sink(std::nullopt);
  }

  return failures == 0 ? 0 : 1;
}
