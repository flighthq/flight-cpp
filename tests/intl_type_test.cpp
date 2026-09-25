#include <flight/intl.hpp>

#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

static_assert(std::variant_size_v<flight::IntlLocalesArgument> == 2);
static_assert(std::is_same_v<decltype(flight::IntlCollatorOptions::sensitivity),
                             std::optional<flight::String>>);
static_assert(
    std::is_same_v<decltype(flight::IntlCollatorOptions::numeric), std::optional<bool>>);
static_assert(std::is_same_v<decltype(flight::IntlDateTimeFormatOptions::minute),
                             std::optional<flight::String>>);
static_assert(std::is_same_v<decltype(flight::IntlListFormatOptions::style),
                             std::optional<flight::String>>);
static_assert(std::is_same_v<decltype(flight::IntlNumberFormatOptions::currency),
                             std::optional<flight::String>>);
static_assert(std::is_same_v<decltype(flight::IntlPluralRulesOptions::type),
                             std::optional<flight::String>>);
static_assert(std::is_same_v<decltype(flight::IntlRelativeTimeFormatOptions::numeric),
                             std::optional<flight::String>>);
static_assert(std::is_same_v<decltype(flight::IntlSegmenterOptions::granularity),
                             std::optional<flight::String>>);
static_assert(std::is_same_v<decltype(flight::IntlSegmentData::index), double>);
static_assert(std::is_same_v<decltype(flight::IntlSegmentData::is_word_like),
                             std::optional<bool>>);
static_assert(!std::is_constructible_v<flight::IntlSegmenter, flight::String,
                                       flight::IntlSegmenterOptions>);

int main() {
  flight::IntlLocalesArgument one_locale{flight::String("en")};
  flight::IntlLocalesArgument many_locales{
      flight::Array<flight::String>{flight::String("en"), flight::String("fr")}};
  if (!std::holds_alternative<flight::String>(one_locale) ||
      !std::holds_alternative<flight::Array<flight::String>>(many_locales)) {
    return 1;
  }

  const flight::IntlSegmenter host_segmenter([](const flight::String& input) {
    return flight::IntlSegments{
        flight::IntlSegmentData{.segment = input, .index = 0.0, .is_word_like = true}};
  });
  const auto segments = host_segmenter.segment(flight::String("Flight"));
  return segments.size() == 1 && segments[0].segment == flight::String("Flight") &&
                 segments[0].index == 0.0 && segments[0].is_word_like == true
             ? 0
             : 1;
}
