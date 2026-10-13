#pragma once

#include <concepts>
#include <memory>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <variant>

#include <flight/any.hpp>

namespace flight {

template <typename Alternative, typename... Alternatives>
[[nodiscard]] constexpr Alternative* optional_variant_get_if(
    std::optional<std::variant<Alternatives...>>* value) noexcept {
  return value != nullptr && value->has_value()
             ? std::get_if<Alternative>(&value->value())
             : nullptr;
}

template <typename Alternative, typename... Alternatives>
[[nodiscard]] constexpr const Alternative* optional_variant_get_if(
    const std::optional<std::variant<Alternatives...>>* value) noexcept {
  return value != nullptr && value->has_value()
             ? std::get_if<Alternative>(&value->value())
             : nullptr;
}

namespace detail {

template <typename Value>
inline constexpr bool is_optional_carrier = false;

template <typename Value>
inline constexpr bool is_optional_carrier<std::optional<Value>> = true;

template <typename Value>
inline constexpr bool is_variant_carrier = false;

template <typename... Alternatives>
inline constexpr bool is_variant_carrier<std::variant<Alternatives...>> = true;

template <typename Alternative, typename Value>
[[nodiscard]] const Alternative* carried_kind_if(const Value& value) noexcept {
  using Carried = std::remove_cvref_t<Value>;
  if constexpr (std::same_as<Carried, Alternative>) {
    return std::addressof(value);
  } else if constexpr (std::same_as<Carried, Any>) {
    // The erased carrier keeps whatever a host boxed into it, and the only checked question it answers
    // about a value-shaped type is the external view. An `Any` holding anything else -- a string, a
    // number, another boxed type -- answers null here, which is the false the guard is asking for.
    return value.template external_if<Alternative>();
  } else if constexpr (is_optional_carrier<Carried>) {
    return value.has_value() ? carried_kind_if<Alternative>(value.value()) : nullptr;
  } else if constexpr (is_variant_carrier<Carried>) {
    const Alternative* found = nullptr;
    std::visit(
        [&found](const auto& alternative) {
          if (found == nullptr) found = carried_kind_if<Alternative>(alternative);
        },
        value);
    return found;
  } else {
    return nullptr;
  }
}

}  // namespace detail

// Which kind the carrier holds, asked once and used by both the guard and the projection.
//
// A source guard narrows a union to one of its alternatives, and the emitter stores a segment the source
// spelled `unknown` -- or a member it can only erase -- as the erased carrier BESIDE the named ones. A
// value that answers the guard may therefore sit in its own alternative or inside `Any`, and asking only
// the first would let a guard say true for a value the projection could not then produce. This asks both,
// in one place, so the two answers can only agree.
//
// The answer is a pointer INTO the carrier: the object that was there, never a copy. It is null rather
// than an exception when the carrier holds something else, including nothing at all -- a value the
// source's guard rejected is a false the program asked about, not a failure.
template <typename Alternative, typename Carrier>
[[nodiscard]] const Alternative* union_kind_if(const Carrier& carrier) noexcept {
  return detail::carried_kind_if<Alternative>(carrier);
}

// The same answer where the source's own guard has already proved it, so an empty result is not an
// answer the source could observe: it means the emitter emitted a projection without the guard that
// proves it. That is a compiler defect with no TypeScript shape, so it fails loudly as one --
// `std::logic_error`, never a `flight::Error` a source `catch` could take for an application failure.
template <typename Alternative, typename Carrier>
[[nodiscard]] const Alternative& union_kind_or_throw(const Carrier& carrier) {
  const Alternative* found = union_kind_if<Alternative>(carrier);
  if (found == nullptr) {
    throw std::logic_error(
        "flight: a narrowed union read found no value of the alternative its guard proved; the guard and "
        "the projection disagree, which no source program can cause");
  }
  return *found;
}

}  // namespace flight
