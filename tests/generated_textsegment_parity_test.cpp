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

  return failures == 0 ? 0 : 1;
}
