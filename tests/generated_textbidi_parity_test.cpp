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

  return failures == 0 ? 0 : 1;
}
