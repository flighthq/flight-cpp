// Behavioural parity for @flighthq/textsegment, driven by the upstream Vitest fixtures in
// .dependencies/flight/packages/textsegment/src/.
//
// This package is where the PORTABLE-PROFILE BOUNDARY is most visible, and the first section is written
// to pin that boundary rather than to pretend it away. Upstream's textSegmenterBackend.test.ts runs on a
// host with a real Intl.Segmenter: it expects the bundled web provider to report itself available and to
// cluster a ZWJ family emoji as one grapheme. This profile has no Intl.Segmenter, and the runtime's
// stated stance is that an unsupported capability is signalled by the MISSING SEAM rather than by a stub:
// has_intl_segmenter() is false, the web provider reports itself unavailable, and segment() returns no
// segments instead of faking them. A test that copied the upstream expectations here would either fail
// or, worse, invite someone to "fix" it with a look-alike segmenter.
//
// So the divergence is asserted EXPLICITLY, with the upstream expectation named beside it, and the
// behaviours that are genuinely this profile's are tested directly.

#include <flight/textsegment/text_segment_boundary.hpp>
#include <flight/textsegment/text_segment_guards.hpp>
#include <flight/textsegment/text_segmenter_backend.hpp>
#include <flight/types/text_segment.hpp>

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

// The caller-supplied capability upstream's recordingBackend() stands for: a host that really can
// segment, handed in through the seam. Its answers must be used as given.
flight::Ref<flight::types::HostTextSegmenterCapability> custom_backend(int* calls) {
  const auto out = flight::make_ref<flight::types::HostTextSegmenterCapability>(
      flight::types::HostTextSegmenterCapability{});
  out->segment = [calls](flight::String text, flight::types::TextSegmentGranularity granularity,
                         std::optional<flight::String>) {
    if (calls != nullptr) *calls += 1;
    flight::Array<flight::Ref<flight::types::TextSegment>> segments;
    segments.push(flight::make_ref<flight::types::TextSegment>(flight::types::TextSegment{
        .start = 0.0, .end = static_cast<double>(text.length()), .text = text}));
    (void)granularity;
    return segments;
  };
  return out;
}


// A backend that segments exactly as a real one does for the fixtures below: CONTIGUOUS spans between
// consecutive boundaries. Contiguity is not incidental -- the boundary queries step to the next segment's
// start, so a backend whose word segments omitted the spaces would answer a different question than the
// upstream fixtures ask. Intl.Segmenter includes the spaces, and so does this.
flight::Ref<flight::types::HostTextSegmenterCapability> backend_with(
    const std::vector<double>& boundaries, std::optional<std::vector<bool>> word_like = std::nullopt) {
  const auto out = flight::make_ref<flight::types::HostTextSegmenterCapability>(
      flight::types::HostTextSegmenterCapability{});
  out->segment = [boundaries, word_like](flight::String text,
                                         flight::types::TextSegmentGranularity,
                                         std::optional<flight::String>) {
    flight::Array<flight::Ref<flight::types::TextSegment>> segments;
    for (std::size_t index = 0; index + 1 < boundaries.size(); ++index) {
      const double start = boundaries[index];
      const double end = boundaries[index + 1];
      flight::types::TextSegment segment;
      segment.start = start;
      segment.end = end;
      // The boundary queries read only the offsets, so the span text is carried whole rather than
      // sliced: slicing UTF-8 by UTF-16 offsets would cut a surrogate pair in half.
      segment.text = text;
      if (word_like.has_value()) segment.is_word_like = (*word_like)[index];
      segments.push(flight::make_ref<flight::types::TextSegment>(segment));
    }
    return segments;
  };
  return out;
}

double boundary_of(std::optional<flight::Ref<flight::types::TextSegmentRange>> range, bool start) {
  if (!range.has_value()) return -1.0;
  return start ? range.value()->start : range.value()->end;
}

}  // namespace

int main() {
  // ---- the portable-profile boundary, stated rather than hidden ---------------------------------

  {
    const auto explanation = flight::textsegment::explain_text_segmenter_backend(
        flight::textsegment::web_text_segmenter_backend);
    // Upstream expects { available: true, backend: 'web-intl', intlSegmenterAvailable: true }. On this
    // profile the last two fields are the boundary: there is no Intl.Segmenter, and the bundled
    // provider says so instead of claiming a capability it does not have.
    check(explanation->backend == flight::String("web-intl"),
          "backend: the bundled provider identifies as web-intl");
    check(explanation->intl_segmenter_available == false,
          "backend: intlSegmenterAvailable is false (upstream: true on a host with Intl.Segmenter)");
    check(explanation->available == false,
          "backend: the bundled provider reports itself UNAVAILABLE, not faked (upstream: true)");
  }

  {
    int calls = 0;
    const auto custom = custom_backend(&calls);
    const auto explanation = flight::textsegment::explain_text_segmenter_backend(custom);
    // A caller-supplied capability is available regardless of Intl: the host supplied it.
    check(explanation->backend == flight::String("custom"),
          "backend: a caller-supplied capability identifies as custom");
    check(explanation->available == true,
          "backend: a caller-supplied capability is available");
    check(explanation->intl_segmenter_available == false,
          "backend: Intl availability is reported independently of a custom capability");
  }

  {
    // The bundled provider is a stable single value: explain decides "web" by identity, so a fresh
    // copy that merely behaves the same would be reported as custom. This pins that it is one value.
    const auto again =
        flight::textsegment::explain_text_segmenter_backend(flight::textsegment::web_text_segmenter_backend);
    check(again->backend == flight::String("web-intl"),
          "backend: the bundled provider is a stable identity across calls");
  }

  // ---- the unsupported seam answers with nothing, not with a substitute -------------------------

  {
    // Upstream expects ['a', '<zwj family>', 'b'] from a real segmenter. Here the provider has no
    // segmenter, so it returns no segments and reports the unavailability through the guard seam --
    // which is the documented signal for a capability this profile does not provide.
    // The guard reports that the segmenter is unavailable; it carries no codepoint, because an
    // unavailable backend has none to report.
    int unavailable = 0;
    flight::textsegment::set_text_segment_guard(std::optional<flight::types::TextSegmentGuard>{
        flight::types::TextSegmentGuard([&unavailable]() { unavailable += 1; })});
    const auto segments = flight::textsegment::web_text_segmenter_backend->segment(
        flight::String(u"a‍"), flight::String("grapheme"), std::nullopt);
    check(static_cast<double>(segments.length()) == 0.0,
          "provider: the bundled provider returns NO segments rather than a look-alike result");
    check(unavailable > 0,
          "provider: the provider reports the unavailable segmenter through the guard seam");
    flight::textsegment::set_text_segment_guard(std::nullopt);
  }

  {
    // A caller-supplied capability's own answer is used as given, whatever it is.
    int calls = 0;
    const auto custom = custom_backend(&calls);
    const auto segments =
        custom->segment(flight::String("Hi there"), flight::String("word"), std::nullopt);
    check(calls == 1 && static_cast<double>(segments.length()) == 1.0,
          "provider: a caller-supplied capability segments through the seam unchanged");
  }

  // ---- textSegmentBoundary.test.ts ------------------------------------------------------------

  // The boundary queries are algorithms over the segments a backend returns, so they can be tested
  // faithfully by supplying the segments -- which is what a real segmenter would return -- without
  // needing Intl.Segmenter. The emoji fixture's offsets are upstream's: "a<family>b" where the ZWJ
  // family cluster spans UTF-16 units 1..9.
  {
    const auto graphemes = backend_with({0.0, 1.0, 9.0, 10.0});
    const auto text = [] {
      return flight::String(u"a\U0001F468\u200D\U0001F469\u200D\U0001F467b");
    }();

    check(flight::textsegment::get_next_grapheme_boundary(graphemes, text, 1.0, std::nullopt) == 9.0,
          "boundary: a next-grapheme step crosses the whole ZWJ cluster at once");
    check(flight::textsegment::get_next_grapheme_boundary(graphemes, text, 0.0, std::nullopt) == 1.0,
          "boundary: a next-grapheme step from the start advances one plain cluster");
    check(flight::textsegment::get_next_grapheme_boundary(graphemes, text, 10.0, std::nullopt) == 10.0 &&
              flight::textsegment::get_next_grapheme_boundary(graphemes, text, 999.0, std::nullopt) == 10.0,
          "boundary: a next-grapheme step clamps at the text length");
    check(flight::textsegment::get_previous_grapheme_boundary(graphemes, text, 10.0, std::nullopt) == 9.0,
          "boundary: a previous-grapheme step from the end lands on the last cluster start");
    check(flight::textsegment::get_previous_grapheme_boundary(graphemes, text, 9.0, std::nullopt) == 1.0,
          "boundary: a previous-grapheme step inverts the forward step over the cluster");
    check(flight::textsegment::get_previous_grapheme_boundary(graphemes, text, 0.0, std::nullopt) == 0.0 &&
              flight::textsegment::get_previous_grapheme_boundary(graphemes, text, -5.0, std::nullopt) == 0.0,
          "boundary: a previous-grapheme step clamps at zero");
  }

  {
    const auto sentences = backend_with({0.0, 7.0, 14.0});
    const flight::String text("First. Second!");
    check(flight::textsegment::get_next_sentence_boundary(sentences, text, 0.0, std::nullopt) == 7.0,
          "boundary: a next-sentence step advances to the next sentence start");
    check(flight::textsegment::get_next_sentence_boundary(sentences, text, 99.0, std::nullopt) == 14.0,
          "boundary: a next-sentence step clamps at the text length");
    check(flight::textsegment::get_previous_sentence_boundary(sentences, text, 14.0, std::nullopt) == 7.0,
          "boundary: a previous-sentence step returns the current sentence start");
    check(flight::textsegment::get_previous_sentence_boundary(sentences, text, -1.0, std::nullopt) == 0.0,
          "boundary: a previous-sentence step clamps at zero");
  }

  {
    // Word granularity includes the spaces as non-word segments, which is why a next-word step lands on
    // the END of the current word rather than on the start of the next one.
    const auto words = backend_with({0.0, 3.0, 4.0, 7.0});
    const flight::String text("foo bar");
    check(flight::textsegment::get_next_word_boundary(words, text, 0.0, std::nullopt) == 3.0,
          "boundary: a next-word step jumps to the end of the current word");
    check(flight::textsegment::get_next_word_boundary(words, text, 7.0, std::nullopt) == 7.0 &&
              flight::textsegment::get_next_word_boundary(words, text, 42.0, std::nullopt) == 7.0,
          "boundary: a next-word step clamps at the text length");
    check(flight::textsegment::get_previous_word_boundary(words, text, 7.0, std::nullopt) == 4.0,
          "boundary: a previous-word step jumps to the start of the current word");
    check(flight::textsegment::get_previous_word_boundary(words, text, 0.0, std::nullopt) == 0.0,
          "boundary: a previous-word step clamps at zero");
  }

  // getWordRangeAt takes the SAME seam, so its cases run here through an explicit host.
  {
    // Upstream's `explicit` case: one word-like segment covering the whole text.
    const auto whole = backend_with({0.0, 4.0}, std::vector<bool>{true});
    const auto range = flight::textsegment::get_word_range_at(whole, flight::String("word"), 1.0,
                                                              std::nullopt);
    check(boundary_of(range, true) == 0.0 && boundary_of(range, false) == 4.0,
          "wordRange: an explicit host supplies the range under an index");
  }

  {
    const auto words = backend_with({0.0, 3.0, 4.0, 7.0}, std::vector<bool>{true, false, true});
    const flight::String text("foo bar");
    const auto first = flight::textsegment::get_word_range_at(words, text, 1.0, std::nullopt);
    check(boundary_of(first, true) == 0.0 && boundary_of(first, false) == 3.0,
          "wordRange: the range under an index is the enclosing word");
    // Double-clicking a space selects no word.
    check(!flight::textsegment::get_word_range_at(words, text, 3.0, std::nullopt).has_value(),
          "wordRange: an index inside whitespace selects no word");
    // An index at the very end resolves through the trailing boundary rather than falling off.
    const auto last = flight::textsegment::get_word_range_at(words, text, 7.0, std::nullopt);
    check(boundary_of(last, true) == 4.0 && boundary_of(last, false) == 7.0,
          "wordRange: an index at the end resolves the final word");
    check(!flight::textsegment::get_word_range_at(words, flight::String(""), 0.0, std::nullopt).has_value(),
          "wordRange: empty text selects no word");
  }

  {
    // The locale reaches the backend unchanged.
    std::string seen = "unset";
    const auto locale_backend = flight::make_ref<flight::types::HostTextSegmenterCapability>(
        flight::types::HostTextSegmenterCapability{});
    locale_backend->segment = [&seen](flight::String text, flight::types::TextSegmentGranularity,
                                      std::optional<flight::String> locale) {
      if (locale.has_value()) seen = locale.value().to_utf8();
      flight::Array<flight::Ref<flight::types::TextSegment>> segments;
      segments.push(flight::make_ref<flight::types::TextSegment>(flight::types::TextSegment{
          .start = 0.0, .end = static_cast<double>(text.length()), .text = text,
          .is_word_like = true}));
      return segments;
    };
    static_cast<void>(flight::textsegment::get_word_range_at(locale_backend, flight::String("word"), 1.0,
                                                             flight::String("ja-JP")));
    check(seen == "ja-JP", "wordRange: the requested locale reaches the active backend");
  }

  // The bundled provider has no segmenter, so the same queries answer with the clamp and with null
  // rather than fabricating boundaries -- the portable stance, and the reason upstream's expectations
  // for these functions cannot be copied here.
  {
    const flight::String text("First. Second!");
    check(flight::textsegment::get_next_sentence_boundary(flight::textsegment::web_text_segmenter_backend,
                                                          text, 0.0, std::nullopt) == 14.0,
          "portable: a next-boundary query on the segmenter-less provider returns the clamp, not a boundary");
    check(!flight::textsegment::get_word_range_at(flight::textsegment::web_text_segmenter_backend, text,
                                                  1.0, std::nullopt)
               .has_value(),
          "portable: getWordRangeAt on the segmenter-less provider returns null, not a fabricated range");
  }

  return failures == 0 ? 0 : 1;
}
