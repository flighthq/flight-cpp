#include <flight/intl/cache.hpp>

#include <iostream>
#include <optional>

namespace {

bool check(bool condition, const char* message) {
  if (!condition) std::cerr << message << '\n';
  return condition;
}

} // namespace

int main() {
  using flight::String;
  using flight::types::LocaleInput;

  flight::intl::cache_storage.clear();
  const LocaleInput locale{String("en-US")};
  const flight::IntlNumberFormatOptions currency{
      .style = String("currency"),
      .currency = String("USD"),
  };
  const auto key = flight::intl::get_cache_key(
      String("number"), locale,
      std::optional<flight::IntlNumberFormatOptions>{currency});
  const auto same_key = flight::intl::get_cache_key(
      String("number"), locale,
      std::optional<flight::IntlNumberFormatOptions>{currency});
  const auto absent_key = flight::intl::get_cache_key(
      String("number"), locale,
      std::optional<flight::IntlNumberFormatOptions>{});
  const auto array_locale_key = flight::intl::get_cache_key(
      String("number"), LocaleInput{flight::Array<String>{String("en-US"), String("fr-FR")}},
      std::optional<flight::IntlNumberFormatOptions>{currency});

  int builds = 0;
  const auto first = flight::intl::get_cached(key, [&]() {
    ++builds;
    return String("first");
  });
  const auto second = flight::intl::get_cached(same_key, [&]() {
    ++builds;
    return String("second");
  });

  flight::intl::cache_storage.clear();
  for (std::size_t index = 0; index < flight::intl::cache_limit; ++index) {
    const String entry = String("entry-").concat(String::from_number(static_cast<double>(index)));
    flight::intl::get_cached(entry, [entry]() { return entry; });
  }
  flight::intl::get_cached(String("newest"), []() { return String("newest"); });

  return check(key == same_key, "equivalent formatter options produced different keys") &&
                 check(key == String("number|en-US|{\"style\":\"currency\",\"currency\":\"USD\"}"),
                       "formatter key did not preserve the source JSON shape") &&
                 check(key != absent_key, "present and absent formatter options shared a key") &&
                 check(array_locale_key.starts_with(String("number|en-US,fr-FR|")),
                       "locale arrays did not preserve join order") &&
                 check(first == String("first") && second == String("first"),
                       "cache miss did not retain the built value") &&
                 check(builds == 1, "equivalent cache lookup rebuilt its formatter") &&
                 check(!flight::intl::cache_storage.has(String("entry-0")),
                       "cache did not evict its oldest entry") &&
                 check(flight::intl::cache_storage.has(String("entry-1")) &&
                           flight::intl::cache_storage.has(String("newest")),
                       "cache evicted a non-oldest entry")
             ? 0
             : 1;
}
