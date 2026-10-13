#pragma once

#include <compare>
#include <concepts>
#include <cstddef>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

namespace flight {

struct Undefined {
  constexpr auto operator<=>(const Undefined&) const noexcept = default;
};

struct Null {
  constexpr Null() noexcept = default;
  // TypeScript's `x.name = null` lowers to `= nullptr`, and a three-state
  // `variant<Value, Null, Undefined>` has no alternative constructible from `std::nullptr_t` without
  // this. `Any` already carries the same conversion -- `Any(std::nullptr_t) : value_(Null{})` -- so this
  // makes the two agree rather than inventing a rule. Implicit on purpose: the emitter writes the bare
  // literal and an explicit constructor would not be selected by the variant's converting assignment.
  constexpr Null(std::nullptr_t) noexcept {}
  constexpr auto operator<=>(const Null&) const noexcept = default;
};

inline constexpr Undefined undefined{};
inline constexpr Null null{};

template <typename Value>
using Presence = std::variant<Undefined, Null, Value>;

// `left ?? fallback` where storage is known only after template instantiation. Absence means an empty
// optional or null reference; other values are present. The fallback is evaluated lazily.
namespace detail {
template <typename Value> inline constexpr bool is_std_optional_v = false;
template <typename Value> inline constexpr bool is_std_optional_v<std::optional<Value>> = true;
template <typename Value> inline constexpr bool is_shared_ptr_v = false;
template <typename Value> inline constexpr bool is_shared_ptr_v<std::shared_ptr<Value>> = true;
} // namespace detail

template <typename Target, typename Left, typename Fallback>
[[nodiscard]] Target nullish_or(Left&& left, Fallback&& fallback) {
  using Stored = std::remove_cvref_t<Left>;
  if constexpr (detail::is_std_optional_v<Stored>) {
    if (left.has_value()) return Target(*std::forward<Left>(left));
  } else if constexpr (detail::is_shared_ptr_v<Stored>) {
    if (left) return Target(std::forward<Left>(left));
  } else {
    return Target(std::forward<Left>(left));
  }
  return Target(std::forward<Fallback>(fallback)());
}

// JavaScript `===` between a nullable, optional value -- the three-state carrier `std::variant<Value, Null,
// Undefined>` the emitter writes for `T | null | undefined` -- and a present `Value`: true exactly when the
// carrier holds a `Value` equal to it. Null and undefined never equal a present value, so strict and loose
// equality agree here and one operator serves `==` and `===`. C++20 rewrites `!=` and the reversed operand
// order from it. Found by argument-dependent lookup through `flight::Null`.
// See docs/compiler-requests/nullish-variant-strict-equality.md.
template <typename Value>
  requires(!std::same_as<Value, Null> && !std::same_as<Value, Undefined> && std::equality_comparable<Value>)
[[nodiscard]] bool operator==(const std::variant<Value, Null, Undefined>& carrier, const Value& value) {
  const auto* present = std::get_if<Value>(&carrier);
  return present != nullptr && *present == value;
}

} // namespace flight
