#pragma once

#include <tuple>
#include <type_traits>
#include <utility>

#include <flight/array.hpp>

namespace flight {

struct Object {};

template <typename Entries>
[[nodiscard]] auto object_keys(const Entries& entries) {
  using Key = std::remove_cvref_t<decltype(entries.begin()->first)>;
  Array<Key> keys;
  for (const auto& entry : entries) keys.push(entry.first);
  return keys;
}

template <typename Entries>
[[nodiscard]] auto object_entries(const Entries& entries) {
  using Key = std::remove_cvref_t<decltype(entries.begin()->first)>;
  using Value = std::remove_cvref_t<decltype(entries.begin()->second)>;
  Array<std::tuple<Key, Value>> result;
  for (const auto& [key, value] : entries) result.push(std::tuple<Key, Value>(key, value));
  return result;
}

namespace detail {

template <typename Target, typename Key, typename Value>
void assign_object_entry(Target& target, const Key& key, const Value& value) {
  if constexpr (requires { target.set(key, value); }) {
    target.set(key, value);
  } else {
    target[key] = value;
  }
}

template <typename Target, typename Source>
void assign_object_source(Target& target, const Source& source) {
  for (const auto& [key, value] : source) assign_object_entry(target, key, value);
}

} // namespace detail

template <typename Target, typename... Sources>
[[nodiscard]] Target& object_assign(Target& target, const Sources&... sources) {
  (detail::assign_object_source(target, sources), ...);
  return target;
}

} // namespace flight
