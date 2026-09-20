#pragma once

#include <concepts>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>

namespace flight {

template <typename Signature>
class Function;

template <typename Result, typename... Parameters>
class Function<Result(Parameters...)> {
 private:
  struct State final {
    explicit State(std::function<Result(Parameters...)> value)
        : callable(std::move(value)) {}

    std::function<Result(Parameters...)> callable;
  };

 public:
  using weak_type = std::weak_ptr<State>;

  Function() noexcept = default;
  Function(std::nullptr_t) noexcept {}

  template <typename Implementation>
    requires(
        !std::same_as<std::remove_cvref_t<Implementation>, Function> &&
        std::is_invocable_r_v<Result, std::remove_reference_t<Implementation>&, Parameters...>)
  Function(Implementation&& implementation)
      : state_(std::make_shared<State>(
            std::function<Result(Parameters...)>(std::forward<Implementation>(implementation)))) {}

  Function& operator=(std::nullptr_t) noexcept {
    state_.reset();
    return *this;
  }

  template <typename Implementation>
    requires(
        !std::same_as<std::remove_cvref_t<Implementation>, Function> &&
        std::is_invocable_r_v<Result, std::remove_reference_t<Implementation>&, Parameters...>)
  Function& operator=(Implementation&& implementation) {
    Function replacement(std::forward<Implementation>(implementation));
    state_.swap(replacement.state_);
    return *this;
  }

  [[nodiscard]] explicit operator bool() const noexcept {
    return state_ != nullptr;
  }

  [[nodiscard]] const void* identity() const noexcept {
    return state_.get();
  }

  [[nodiscard]] weak_type weaken() const noexcept {
    return state_;
  }

  [[nodiscard]] static std::optional<Function> lock_weak(const weak_type& weak) noexcept {
    auto state = weak.lock();
    return state ? std::optional<Function>(Function(std::move(state))) : std::nullopt;
  }

  Result operator()(Parameters... parameters) const {
    if (!state_) throw std::bad_function_call();
    if constexpr (std::is_void_v<Result>) {
      state_->callable(std::forward<Parameters>(parameters)...);
    } else {
      return state_->callable(std::forward<Parameters>(parameters)...);
    }
  }

  [[nodiscard]] friend bool operator==(const Function& left, const Function& right) noexcept {
    return left.state_ == right.state_;
  }

  [[nodiscard]] friend bool operator==(const Function& function, std::nullptr_t) noexcept {
    return !function;
  }

  [[nodiscard]] friend bool operator==(std::nullptr_t, const Function& function) noexcept {
    return !function;
  }

 private:
  explicit Function(std::shared_ptr<State> state) noexcept : state_(std::move(state)) {}

  std::shared_ptr<State> state_;
};

// Whether one argument satisfies one callback parameter.
//
// The rule is exact-match, with a single deliberate exception: a parameter that is a STRUCTURAL ROW
// over an object accepts a reference to that very object. That is the runtime's own projection of a
// subject into its row -- the same object, no conversion of any value, no second identity, and no
// widening, because the argument's type must be exactly the row's own object type.
//
// It is admitted because the compiler emits both halves of that seam: a signal whose listener takes
// `Readonly<T>` and an emitter that holds the `T`. Refusing it would be the runtime declining a
// projection it defines. Nothing else is relaxed -- an `int` still does not satisfy a `double`
// parameter, because that conversion loses information and ECMAScript does not perform it here.
template <typename Argument, typename Parameter>
concept callable_argument_v1 =
    std::same_as<std::remove_cvref_t<Argument>, Parameter> ||
    (requires {
       typename Parameter::schema_type;
       typename Parameter::object_type;
     } &&
     std::same_as<std::remove_cvref_t<Argument>, std::shared_ptr<typename Parameter::object_type>> &&
     std::constructible_from<Parameter, std::remove_cvref_t<Argument>>);

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
        return (callable_argument_v1<std::tuple_element_t<Index, ArgumentsTuple>,
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

template <typename Result, typename... Parameters>
struct callable_signature_v1<Function<Result(Parameters...)>> {
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
        return (callable_argument_v1<std::tuple_element_t<Index, ArgumentsTuple>,
                                     std::tuple_element_t<Index, parameter_types>> && ...);
      }(std::make_index_sequence<sizeof...(Parameters)>{});
    }
  }();

  template <typename Implementation>
  [[nodiscard]] static Function<Result(Parameters...)> bind(Implementation&& implementation) {
    using ImplementationType = std::decay_t<Implementation>;
    return Function<Result(Parameters...)>(
        [function = ImplementationType(std::forward<Implementation>(implementation))](
            Parameters... parameters) mutable -> Result {
          if constexpr (std::invocable<ImplementationType&, Parameters...>) {
            return std::invoke(function, std::forward<Parameters>(parameters)...);
          } else if constexpr (std::invocable<ImplementationType&>) {
            return std::invoke(function);
          } else {
            static_assert(
                std::invocable<ImplementationType&, Parameters...> ||
                    std::invocable<ImplementationType&>,
                "bound Flight callable cannot accept the emitted signature");
          }
        });
  }
};

template <typename Callable, typename Implementation>
[[nodiscard]] Callable bind_callable_v1(Implementation&& implementation) {
  return callable_signature_v1<Callable>::bind(std::forward<Implementation>(implementation));
}

} // namespace flight

namespace std {

template <typename Result, typename... Parameters>
struct hash<flight::Function<Result(Parameters...)>> {
  [[nodiscard]] size_t operator()(
      const flight::Function<Result(Parameters...)>& function) const noexcept {
    return hash<const void*>{}(function.identity());
  }
};

} // namespace std
