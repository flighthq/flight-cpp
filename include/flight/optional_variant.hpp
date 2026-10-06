#pragma once

#include <optional>
#include <variant>

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

}  // namespace flight
