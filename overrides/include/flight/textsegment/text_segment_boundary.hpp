#pragma once

#include <flight/runtime.hpp>
#include <flight/textsegment/text_segment_guards.hpp>
#include <flight/textsegment/text_segmenter_backend.hpp>
#include <flight/types/text_segment.hpp>

namespace flight::textsegment {

inline double clamp_index(double index, double length) {
  if (index < 0.0) return 0.0;
  if (index > length) return length;
  return index;
}

inline double next_segment_boundary(
    flight::Array<flight::Ref<flight::types::TextSegment>> segments,
    double index,
    double length) {
  const double from = clamp_index(index, length);
  if (from >= length) return length;
  for (const auto& segment : segments) {
    if (segment->start > from) return segment->start;
  }
  return length;
}

inline double previous_segment_boundary(
    flight::Array<flight::Ref<flight::types::TextSegment>> segments,
    double index) {
  const double length = segments.empty() ? 0.0 : segments[segments.size() - 1]->end;
  const double from = clamp_index(index, length);
  if (from <= 0.0) return 0.0;
  double previous = 0.0;
  for (const auto& segment : segments) {
    if (segment->start >= from) break;
    previous = segment->start;
  }
  return previous;
}

inline std::optional<flight::Ref<flight::types::TextSegmentRange>> get_word_range_at(
    flight::Ref<flight::types::HostTextSegmenterCapability> text_segmenter,
    flight::String text,
    double index,
    std::optional<flight::String> locale = std::nullopt) {
  if (text.empty()) return std::nullopt;
  const double clamped = clamp_index(index, static_cast<double>(text.length()));
  const double lookup = clamped == static_cast<double>(text.length())
                            ? static_cast<double>(text.length()) - 1.0
                            : clamped;
  if (text_segmenter == web_text_segmenter_backend) {
    report_text_segmenter_unavailable();
    return std::nullopt;
  }
  for (const auto& segment : text_segmenter->segment(text, flight::String("word"), locale)) {
    if (lookup >= segment->start && lookup < segment->end) {
      if (segment->is_word_like == true) {
        return flight::make_ref<flight::types::TextSegmentRange>(
            flight::types::TextSegmentRange{.start = segment->start, .end = segment->end});
      }
      return std::nullopt;
    }
  }
  return std::nullopt;
}

inline double get_next_text_segment_boundary(
    flight::Ref<flight::types::HostTextSegmenterCapability> text_segmenter,
    flight::String text,
    double index,
    flight::types::TextSegmentGranularity granularity,
    std::optional<flight::String> locale) {
  const double length = static_cast<double>(text.length());
  const double from = clamp_index(index, length);
  if (from >= length) return length;
  if (text_segmenter == web_text_segmenter_backend) {
    report_text_segmenter_unavailable();
    return length;
  }
  return next_segment_boundary(text_segmenter->segment(text, granularity, locale), from, length);
}

inline double get_next_grapheme_boundary(
    flight::Ref<flight::types::HostTextSegmenterCapability> text_segmenter,
    flight::String text,
    double index,
    std::optional<flight::String> locale = std::nullopt) {
  return get_next_text_segment_boundary(
      text_segmenter, std::move(text), index, flight::String("grapheme"), std::move(locale));
}

inline double get_next_sentence_boundary(
    flight::Ref<flight::types::HostTextSegmenterCapability> text_segmenter,
    flight::String text,
    double index,
    std::optional<flight::String> locale = std::nullopt) {
  return get_next_text_segment_boundary(
      text_segmenter, std::move(text), index, flight::String("sentence"), std::move(locale));
}

inline double get_next_word_boundary(
    flight::Ref<flight::types::HostTextSegmenterCapability> text_segmenter,
    flight::String text,
    double index,
    std::optional<flight::String> locale = std::nullopt) {
  return get_next_text_segment_boundary(
      text_segmenter, std::move(text), index, flight::String("word"), std::move(locale));
}

inline double get_previous_text_segment_boundary(
    flight::Ref<flight::types::HostTextSegmenterCapability> text_segmenter,
    flight::String text,
    double index,
    flight::types::TextSegmentGranularity granularity,
    std::optional<flight::String> locale) {
  const double from = clamp_index(index, static_cast<double>(text.length()));
  if (from <= 0.0) return 0.0;
  if (text_segmenter == web_text_segmenter_backend) {
    report_text_segmenter_unavailable();
    return 0.0;
  }
  return previous_segment_boundary(text_segmenter->segment(text, granularity, locale), from);
}

inline double get_previous_grapheme_boundary(
    flight::Ref<flight::types::HostTextSegmenterCapability> text_segmenter,
    flight::String text,
    double index,
    std::optional<flight::String> locale = std::nullopt) {
  return get_previous_text_segment_boundary(
      text_segmenter, std::move(text), index, flight::String("grapheme"), std::move(locale));
}

inline double get_previous_sentence_boundary(
    flight::Ref<flight::types::HostTextSegmenterCapability> text_segmenter,
    flight::String text,
    double index,
    std::optional<flight::String> locale = std::nullopt) {
  return get_previous_text_segment_boundary(
      text_segmenter, std::move(text), index, flight::String("sentence"), std::move(locale));
}

inline double get_previous_word_boundary(
    flight::Ref<flight::types::HostTextSegmenterCapability> text_segmenter,
    flight::String text,
    double index,
    std::optional<flight::String> locale = std::nullopt) {
  return get_previous_text_segment_boundary(
      text_segmenter, std::move(text), index, flight::String("word"), std::move(locale));
}

} // namespace flight::textsegment
