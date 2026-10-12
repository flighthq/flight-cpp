// Derived from @flighthq/textsegment/packages/textsegment/src/textSegmenterBackend.ts
// at generated digest 8342a1e42b7d77cdbaa206ccadc494bcc9b9cb29bb6948c466d0668d3c55ef35.
//
// OVERRIDE: the emitter refuses this module (typeof requires closed runtime type evidence), so the
// emitted header is a PARTIAL. This supplies the whole module: the web provider value, the two
// exported entry points, and the Intl.Segmenter-backed internals.
//
// Portable-profile boundary, stated deliberately rather than hidden: has_intl_segmenter() is false
// here because this profile has no Intl.Segmenter. Real Unicode segmentation is a host capability --
// an unsupported capability is signalled by the missing seam, not by a stub -- so the web provider
// reports itself unavailable and segment() returns no segments rather than faking them.
//
// Re-derived 2026-10-12 by checking it still matches the current source: webTextSegmenterBackend and
// explainTextSegmenterBackend are both present, with the same declaration order and status reporting.
#pragma once

#include <flight/intl.hpp>
#include <flight/runtime.hpp>
#include <flight/textsegment/text_segment_guards.hpp>
#include <flight/types/text_segment.hpp>

namespace flight::textsegment {

// The portable runtime intentionally exposes IntlSegmenter only as a host-supplied type carrier;
// it does not advertise the ambient Intl.Segmenter constructor used by the web backend.
inline bool has_intl_segmenter() { return false; }

inline std::optional<flight::IntlSegmenter> get_cached_segmenter(
    std::optional<flight::String> locale,
    flight::types::TextSegmentGranularity granularity) {
  (void)locale;
  (void)granularity;
  report_text_segmenter_unavailable();
  return std::nullopt;
}

inline flight::Array<flight::Ref<flight::types::TextSegment>> segment_with_intl_segmenter(
    flight::String text,
    flight::types::TextSegmentGranularity granularity,
    std::optional<flight::String> locale = std::nullopt) {
  (void)text;
  (void)get_cached_segmenter(std::move(locale), std::move(granularity));
  return {};
}

inline void initialize_web_text_segmenter_backend(
    flight::Ref<flight::types::HostTextSegmenterCapability> out) {
  out->segment = segment_with_intl_segmenter;
}

inline flight::Ref<flight::types::HostTextSegmenterCapability>
create_web_text_segmenter_backend() {
  auto out = flight::make_ref<flight::types::HostTextSegmenterCapability>(
      flight::types::HostTextSegmenterCapability{});
  initialize_web_text_segmenter_backend(out);
  return out;
}

inline flight::Ref<flight::types::HostTextSegmenterCapability>
    web_text_segmenter_backend = create_web_text_segmenter_backend();

inline flight::Ref<flight::types::TextSegmenterExplanation>
explain_text_segmenter_backend(
    flight::Ref<flight::types::HostTextSegmenterCapability> text_segmenter) {
  const bool web = text_segmenter == web_text_segmenter_backend;
  return flight::make_ref<flight::types::TextSegmenterExplanation>(
      flight::types::TextSegmenterExplanation{
          .available = !web,
          .backend = web ? flight::String("web-intl") : flight::String("custom"),
          .intl_segmenter_available = false,
      });
}

} // namespace flight::textsegment
