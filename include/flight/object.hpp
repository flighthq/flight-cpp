#pragma once

#include <type_traits>

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

} // namespace flight
