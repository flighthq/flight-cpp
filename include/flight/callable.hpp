#pragma once

#include <concepts>
#include <functional>
#include <type_traits>
#include <utility>

namespace flight {

template <typename Callable>
struct callable_signature_v1;

template <typename Result, typename... Parameters>
struct callable_signature_v1<std::function<Result(Parameters...)>> {
  template <typename... Arguments>
  static constexpr bool accepts = std::invocable<const std::function<Result(Parameters...)>&, Arguments...>;

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
