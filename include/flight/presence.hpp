#pragma once

#include <compare>
#include <cstddef>
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

} // namespace flight
