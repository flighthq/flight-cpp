#pragma once

#include <concepts>
#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>

namespace flight {

template <typename Callable>
struct callable_signature_v1;

template <typename Result, typename... Parameters>
struct callable_signature_v1<std::function<Result(Parameters...)>> {
  using parameter_types = std::tuple<Parameters...>;
  using result_type = Result;
  static constexpr std::size_t arity = sizeof...(Parameters);

  template <typename... Arguments>
  static constexpr bool accepts = [] {
    if constexpr (sizeof...(Arguments) != sizeof...(Parameters)) {
      return false;
    } else {
      return []<std::size_t... Index>(std::index_sequence<Index...>) {
        using ArgumentsTuple = std::tuple<std::remove_cvref_t<Arguments>...>;
        return (std::same_as<std::tuple_element_t<Index, ArgumentsTuple>,
                             std::tuple_element_t<Index, parameter_types>> && ...);
      }(std::make_index_sequence<sizeof...(Parameters)>{});
    }
  }();

  template <typename Implementation>
  [[nodiscard]] static std::function<Result(Parameters...)> bind(Implementation&& implementation) {
    using Function = std::decay_t<Implementation>;
    return [function = Function(std::forward<Implementation>(implementation))](Parameters... parameters) mutable -> Result {
      if constexpr (std::invocable<Function&, Parameters...>) {
        return std::invoke(function, std::forward<Parameters>(parameters)...);
      } else if constexpr (std::invocable<Function&>) {
        return std::invoke(function);
      } else {
        static_assert(std::invocable<Function&, Parameters...> || std::invocable<Function&>,
                      "bound Flight callable cannot accept the emitted signature");
      }
    };
  }
};

template <typename Callable, typename Implementation>
[[nodiscard]] Callable bind_callable_v1(Implementation&& implementation) {
  return callable_signature_v1<Callable>::bind(std::forward<Implementation>(implementation));
}

} // namespace flight
