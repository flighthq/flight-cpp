// Derived from @flighthq/textshaper/packages/textshaper/src/textShaperCache.ts.
#pragma once

#include <optional>
#include <string>

#include <flight/entity/entity.hpp>
#include <flight/map.hpp>
#include <flight/runtime.hpp>
#include <flight/structural_ref.hpp>
#include <flight/textshaper/text_shaper_run.hpp>
#include <flight/types/entity.hpp>
#include <flight/types/shaped_run.hpp>
#include <flight/types/text_format.hpp>
#include <flight/types/text_shaper.hpp>
#include <flight/types/text_shaper_cache.hpp>
#include <flight/weak_map.hpp>

static_assert(flight::runtime_contract.compiler_contract == "flight-runtime-contract/2",
              "Flight compiler/runtime contract mismatch");
static_assert(flight::runtime_contract.cpp_abi == 1,
              "Flight C++ runtime ABI mismatch");

namespace flight::textshaper {

using flight::types::HostTextShaperCapability;
using flight::types::ShapeRunOptions;
using flight::types::ShapedRun;
using flight::types::TextFormat;
using flight::types::TextShaperCache;

struct TextShaperCacheRuntime : public flight::ReferenceEnabled {
  flight::Map<flight::String, flight::Ref<ShapedRun>> entries;
};

inline flight::WeakMap<flight::Ref<TextShaperCache>, flight::Ref<TextShaperCacheRuntime>>
    text_shaper_cache_runtimes;

inline void initialize_text_shaper_cache(
    flight::types::EntityConstruction<flight::Ref<TextShaperCache>> out) {
  const auto runtime = flight::make_ref<TextShaperCacheRuntime>();
  const auto entity_runtime = flight::make_ref<flight::types::EntityRuntime>(
      flight::types::EntityRuntime{.binding = std::nullopt, .uid = std::nullopt});
  flight::row_set(out, flight::types::entity_runtime_key,
                  std::optional<flight::Ref<flight::types::EntityRuntime>>{entity_runtime});
  text_shaper_cache_runtimes.set(
      flight::entity::finish_entity<flight::Ref<TextShaperCache>>(out), runtime);
}

inline flight::Ref<TextShaperCache> create_text_shaper_cache() {
  const auto out = flight::entity::allocate_entity<flight::Ref<TextShaperCache>>();
  initialize_text_shaper_cache(out);
  return flight::entity::finish_entity<flight::Ref<TextShaperCache>>(out);
}

inline std::optional<flight::Ref<TextShaperCacheRuntime>> get_text_shaper_cache_runtime(
    flight::Ref<TextShaperCache> cache) {
  return text_shaper_cache_runtimes.get(cache);
}

inline void clear_text_shaper_cache(flight::Ref<TextShaperCache> cache) {
  const auto runtime = get_text_shaper_cache_runtime(cache);
  if (runtime.has_value()) runtime.value()->entries.clear();
}

inline void dispose_text_shaper_cache(flight::Ref<TextShaperCache> cache) {
  const auto runtime = get_text_shaper_cache_runtime(cache);
  if (!runtime.has_value()) return;
  runtime.value()->entries.clear();
  static_cast<void>(text_shaper_cache_runtimes.erase(cache));
  cache->entity_runtime_key = std::nullopt;
}

inline flight::WeakMap<flight::Ref<HostTextShaperCapability>, double> backend_cache_ids;
inline double next_backend_cache_id = 1.0;

inline double get_backend_cache_id(flight::Ref<HostTextShaperCapability> backend) {
  const auto existing = backend_cache_ids.get(backend);
  if (existing.has_value()) return existing.value();
  const double id = next_backend_cache_id++;
  backend_cache_ids.set(backend, id);
  return id;
}

inline flight::String make_cache_key(
    flight::String text,
    flight::Ref<TextFormat> format,
    std::optional<flight::Ref<ShapeRunOptions>> options = std::nullopt) {
  const flight::String separator("\x01");
  const flight::String fmt = format->font.value_or(flight::String("")) + separator +
      flight::to_string(format->size.value_or(12.0)) + separator +
      flight::to_string(format->bold.value_or(false) ? 1.0 : 0.0) + separator +
      flight::to_string(format->italic.value_or(false) ? 1.0 : 0.0) + separator +
      flight::to_string(format->kerning.value_or(false) ? 1.0 : 0.0) + separator +
      flight::to_string(format->letter_spacing.value_or(0.0));
  flight::String opts;
  if (options.has_value()) {
    opts = options.value()->direction.value_or(flight::String("")) + separator +
           options.value()->script.value_or(flight::String(""));
  }
  const flight::String nul(std::u16string(1, u'\0'));
  return text + nul + fmt + nul + opts;
}

inline std::optional<flight::Ref<ShapedRun>> shape_text_run_cached(
    flight::Ref<HostTextShaperCapability> host_text_shaper,
    flight::Ref<TextShaperCache> cache,
    flight::String text,
    flight::Ref<TextFormat> format,
    std::optional<flight::Ref<ShapeRunOptions>> options = std::nullopt) {
  const auto runtime = get_text_shaper_cache_runtime(cache);
  if (!runtime.has_value()) return std::nullopt;
  const flight::String key = flight::to_string(get_backend_cache_id(host_text_shaper)) +
                             flight::String(std::u16string(1, u'\0')) +
                             make_cache_key(text, format, options);
  const auto existing = runtime.value()->entries.get(key);
  if (existing.has_value()) return existing;

  using HostRow = flight::StructuralRef<
      flight::RowReadonly<flight::RowOf<flight::Ref<HostTextShaperCapability>>>>;
  using FormatRow = flight::StructuralRef<
      flight::RowReadonly<flight::RowOf<flight::Ref<TextFormat>>>>;
  const auto host_row = flight::structural_ref_cast<HostRow>(flight::StructuralRef<
      flight::RowWritable<flight::RowOf<flight::Ref<HostTextShaperCapability>>>>(host_text_shaper));
  const auto format_row = flight::structural_ref_cast<FormatRow>(flight::StructuralRef<
      flight::RowWritable<flight::RowOf<flight::Ref<TextFormat>>>>(format));
  const auto result = shape_text_run(host_row, text, format_row, options);
  if (result.has_value()) runtime.value()->entries.set(key, result.value());
  return result;
}

}  // namespace flight::textshaper
