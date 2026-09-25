#pragma once

#include <tuple>
#include <concepts>
#include <type_traits>
#include <utility>

#include <flight/array.hpp>
#include <flight/record.hpp>

namespace flight {

struct Object {};

template <typename Key, typename Value>
[[nodiscard]] Array<String> object_keys(const Record<Key, Value>& entries) {
  return entries.enumerable_keys();
}

template <typename Key, typename Value>
[[nodiscard]] Array<std::tuple<String, Value>> object_entries(const Record<Key, Value>& entries) {
  return entries.enumerable_entries();
}

template <typename Key, typename Value>
[[nodiscard]] Array<Value> object_values(const Record<Key, Value>& entries) {
  Array<Value> result;
  for (const auto& [key, value] : entries.enumerable_entries()) {
    static_cast<void>(key);
    result.push(value);
  }
  return result;
}

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

template <typename Entries>
[[nodiscard]] auto object_values(const Entries& entries) {
  using Value = std::remove_cvref_t<decltype(entries.begin()->second)>;
  Array<Value> result;
  for (const auto& entry : entries) result.push(entry.second);
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

// `Object.hasOwn(target, key)`: whether the target carries the key as its OWN property.
//
// Own, not inherited and not merely readable: a record answers from its own entries. This is the
// membership test the SDK writes when comparing two field bags key by key -- `material.ts` walks
// one bag's keys and asks whether the other declares each one -- so answering true for a key the
// target does not have would make two different materials compare equal.
template <typename Key, typename Value, typename LookupKey>
[[nodiscard]] bool object_has_own(const Record<Key, Value>& target, const LookupKey& key) {
  return target.has(key);
}

// The same question for anything else that answers membership for itself. A structural subject is
// reached through `flight::named_properties`, which lives with the row machinery rather than here
// -- this header must not depend on it.
template <typename Target, typename LookupKey>
  requires requires(const Target& target, const LookupKey& key) {
    { target.has(key) } -> std::convertible_to<bool>;
  }
[[nodiscard]] bool object_has_own(const Target& target, const LookupKey& key) {
  return target.has(key);
}

} // namespace flight
