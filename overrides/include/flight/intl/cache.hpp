// Derived from @flighthq/intl/packages/intl/src/cache.ts
// at generated digest 32cf9e6c3c79810812d702aeb636e3e26698f64c8e2e3a83c90b4b62c59530c3.
//
// OVERRIDE: the emitter refuses this module -- a presence test against undefined has no absence
// channel in the emitted C++ storage for the `unknown` value a Map<string, unknown>.get returns -- so
// the emitted header is a PARTIAL that omits getCacheKey and getCached. This supplies both, plus the
// per-option key builders they use.
//
// Re-derived 2026-10-12 by checking it still matches the current source rather than rebuilding it from
// the emitted partial: both exported functions are present, the key is still kind|localeKey|json and
// the cache is still a bounded FIFO of 256 over type-erased values. A target-neutral rewrite of the
// eviction (iterate keys() and delete the first) was tried first and did NOT clear the refusal, which
// is why the override stays.
#pragma once

#include <functional>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

#include <flight/any.hpp>
#include <flight/intl.hpp>
#include <flight/json.hpp>
#include <flight/map.hpp>
#include <flight/runtime.hpp>
#include <flight/types/locale_input.hpp>

static_assert(
    flight::runtime_contract.compiler_contract == "flight-runtime-contract/2",
    "Flight compiler/runtime contract mismatch");
static_assert(
    flight::runtime_contract.cpp_abi == 1,
    "Flight C++ runtime ABI mismatch");

namespace flight::intl {

using flight::types::LocaleInput;

inline flight::Map<flight::String, flight::Any> cache_storage;
inline constexpr std::size_t cache_limit = 256;

template <typename Value>
inline void cache_set_option(
    flight::JsonObject& object, flight::String key, const std::optional<Value>& value) {
  if (value.has_value()) object.set(std::move(key), flight::JsonValue(value.value()));
}

inline flight::String cache_options_key(const flight::IntlCollatorOptions& options) {
  flight::JsonObject object;
  cache_set_option(object, flight::String("sensitivity"), options.sensitivity);
  cache_set_option(object, flight::String("numeric"), options.numeric);
  cache_set_option(object, flight::String("caseFirst"), options.case_first);
  return flight::Json::stringify(object);
}

inline flight::String cache_options_key(const flight::IntlDateTimeFormatOptions& options) {
  flight::JsonObject object;
  cache_set_option(object, flight::String("year"), options.year);
  cache_set_option(object, flight::String("month"), options.month);
  cache_set_option(object, flight::String("day"), options.day);
  cache_set_option(object, flight::String("hour"), options.hour);
  cache_set_option(object, flight::String("minute"), options.minute);
  return flight::Json::stringify(object);
}

inline flight::String cache_options_key(const flight::IntlListFormatOptions& options) {
  flight::JsonObject object;
  cache_set_option(object, flight::String("type"), options.type);
  cache_set_option(object, flight::String("style"), options.style);
  return flight::Json::stringify(object);
}

inline flight::String cache_options_key(const flight::IntlNumberFormatOptions& options) {
  flight::JsonObject object;
  cache_set_option(object, flight::String("notation"), options.notation);
  cache_set_option(object, flight::String("style"), options.style);
  cache_set_option(object, flight::String("currency"), options.currency);
  cache_set_option(object, flight::String("unit"), options.unit);
  return flight::Json::stringify(object);
}

inline flight::String cache_options_key(const flight::IntlPluralRulesOptions& options) {
  flight::JsonObject object;
  cache_set_option(object, flight::String("type"), options.type);
  return flight::Json::stringify(object);
}

inline flight::String cache_options_key(const flight::IntlRelativeTimeFormatOptions& options) {
  flight::JsonObject object;
  cache_set_option(object, flight::String("numeric"), options.numeric);
  return flight::Json::stringify(object);
}

template <typename Options>
inline flight::String cache_options_key(const std::optional<Options>& options) {
  return options.has_value() ? cache_options_key(options.value()) : flight::String();
}

inline flight::String cache_locale_key(const LocaleInput& locale) {
  return std::visit(
      [](const auto& value) -> flight::String {
        using Value = std::remove_cvref_t<decltype(value)>;
        if constexpr (std::same_as<Value, flight::String>) {
          return value;
        } else {
          return flight::String::join(value, flight::String(","));
        }
      },
      locale);
}

template <typename Options>
inline flight::String get_cache_key(
    flight::String kind, LocaleInput locale, Options options) {
  return kind.concat(
      flight::String("|"), cache_locale_key(locale), flight::String("|"),
      cache_options_key(options));
}

template <typename Build>
inline auto get_cached(flight::String key, Build build) -> std::invoke_result_t<Build> {
  using Value = std::invoke_result_t<Build>;
  if (const auto existing = cache_storage.get(key); existing.has_value()) {
    if (const auto* cached = existing->template external_if<Value>()) return *cached;
  }

  Value built = std::invoke(std::move(build));
  if (cache_storage.size() >= cache_limit) {
    const auto oldest = cache_storage.keys().next().value;
    if (oldest.has_value()) cache_storage.erase(oldest.value());
  }
  cache_storage.set(std::move(key), flight::Any::external<Value>(built));
  return built;
}

} // namespace flight::intl
