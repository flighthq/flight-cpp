#include <flight/textshaper/text_shaper_cache.hpp>

#include <iostream>
#include <optional>

namespace {

bool check(bool condition, const char* message) {
  if (!condition) std::cerr << message << '\n';
  return condition;
}

}  // namespace

int main() {
  using flight::String;
  using namespace flight::types;

  int first_calls = 0;
  const auto first = flight::make_ref<HostTextShaperCapability>();
  first->measure_text = [](String text, flight::Ref<TextFormat>) {
    return static_cast<double>(text.length());
  };
  first->shape_run = [&](String, auto, auto) {
    ++first_calls;
    return flight::make_ref<ShapedRun>(ShapedRun{
        .advance_width = 15.0,
        .direction = String("LeftToRight"),
        .font = std::nullopt,
        .glyph_count = 2.0,
        .glyphs = {},
        .script = String("Latn"),
    });
  };
  const auto second = flight::make_ref<HostTextShaperCapability>();
  second->measure_text = first->measure_text;
  second->shape_run = [](String, auto, auto) {
    return flight::make_ref<ShapedRun>(ShapedRun{
        .advance_width = 22.0,
        .direction = String("LeftToRight"),
        .font = std::nullopt,
        .glyph_count = 1.0,
        .glyphs = {},
        .script = String("Latn"),
    });
  };

  const auto cache = flight::textshaper::create_text_shaper_cache();
  const auto format = flight::make_ref<TextFormat>();
  const auto first_result = flight::textshaper::shape_text_run_cached(
      first, cache, String("hi"), format);
  const auto hit = flight::textshaper::shape_text_run_cached(
      first, cache, String("hi"), format);
  const auto other_backend = flight::textshaper::shape_text_run_cached(
      second, cache, String("hi"), format);
  if (!check(first_result.has_value() && hit == first_result && first_calls == 1,
             "text shaper cache did not preserve a hit identity") ||
      !check(other_backend.has_value() && other_backend.value()->advance_width == 22.0,
             "text shaper cache crossed backend identities")) {
    return 1;
  }

  flight::textshaper::clear_text_shaper_cache(cache);
  static_cast<void>(flight::textshaper::shape_text_run_cached(first, cache, String("hi"), format));
  if (!check(first_calls == 2, "clear did not evict cached shaped runs")) return 1;

  const auto options = flight::make_ref<ShapeRunOptions>();
  options->direction = String("RightToLeft");
  static_cast<void>(flight::textshaper::shape_text_run_cached(
      first, cache, String("hi"), format, options));
  if (!check(first_calls == 3, "direction was omitted from the cache key")) return 1;

  flight::textshaper::dispose_text_shaper_cache(cache);
  const auto after_dispose = flight::textshaper::shape_text_run_cached(
      first, cache, String("hi"), format);
  if (!check(!after_dispose.has_value() && first_calls == 3,
             "disposed cache remained usable or called the backend")) {
    return 1;
  }
  flight::textshaper::dispose_text_shaper_cache(cache);

  const auto advances_only = flight::make_ref<HostTextShaperCapability>();
  advances_only->measure_text = first->measure_text;
  const auto fresh = flight::textshaper::create_text_shaper_cache();
  if (!check(!flight::textshaper::shape_text_run_cached(
                   advances_only, fresh, String("hi"), format)
                   .has_value(),
             "advances-only backend returned a shaped run")) {
    return 1;
  }
  return 0;
}
