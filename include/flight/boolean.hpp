#pragma once

#include <cmath>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

#include <flight/presence.hpp>
#include <flight/string.hpp>

namespace flight {
namespace detail {

template <typename>
inline constexpr bool boolean_is_optional = false;

template <typename Value>
inline constexpr bool boolean_is_optional<std::optional<Value>> = true;

template <typename>
inline constexpr bool boolean_is_variant = false;

template <typename... Values>
inline constexpr bool boolean_is_variant<std::variant<Values...>> = true;

template <typename>
inline constexpr bool boolean_is_shared_pointer = false;

template <typename Value>
inline constexpr bool boolean_is_shared_pointer<std::shared_ptr<Value>> = true;

template <typename>
inline constexpr bool boolean_is_unique_pointer = false;

template <typename Value, typename Deleter>
inline constexpr bool boolean_is_unique_pointer<std::unique_ptr<Value, Deleter>> = true;

} // namespace detail

// Callable object rather than an overloaded free function so an emitted `filter(Boolean)` can pass
// the same name as a first-class predicate. Represented JavaScript objects are always truthy,
// including empty collections; nullable and union carriers recurse into their active source value.
struct ToBoolean final {
  template <typename Value>
  [[nodiscard]] bool operator()(const Value& value) const noexcept {
    using Type = std::remove_cvref_t<Value>;
    if constexpr (std::same_as<Type, bool>) {
      return value;
    } else if constexpr (std::floating_point<Type>) {
      return value != Type{0} && !std::isnan(value);
    } else if constexpr (std::integral<Type>) {
      return value != Type{0};
    } else if constexpr (std::is_enum_v<Type>) {
      return (*this)(static_cast<std::underlying_type_t<Type>>(value));
    } else if constexpr (std::same_as<Type, String>) {
      return !value.empty();
    } else if constexpr (
        std::same_as<Type, Undefined> || std::same_as<Type, Null> ||
        std::same_as<Type, std::nullptr_t> || std::same_as<Type, std::monostate>) {
      return false;
    } else if constexpr (detail::boolean_is_optional<Type>) {
      return value.has_value() && (*this)(*value);
    } else if constexpr (detail::boolean_is_variant<Type>) {
      return std::visit([this](const auto& active) { return (*this)(active); }, value);
    } else if constexpr (std::is_pointer_v<Type>) {
      return value != nullptr;
    } else if constexpr (
        detail::boolean_is_shared_pointer<Type> || detail::boolean_is_unique_pointer<Type>) {
      return static_cast<bool>(value);
    } else {
      // Every remaining compiler representation is a non-null JavaScript object or callable value.
      return true;
    }
  }
};

inline constexpr ToBoolean to_boolean{};

// Closed source unions use std::variant. The compiler emits ordinary logical negation for those
// values, so route it through the same JavaScript truthiness operation used by Boolean(value).
template <typename... Values>
[[nodiscard]] bool operator!(const std::variant<Values...>& value) noexcept {
  return !to_boolean(value);
}

} // namespace flight
