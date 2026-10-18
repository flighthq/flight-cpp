#pragma once

#include <concepts>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
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

namespace detail {
// An object address, never a function-pointer cast. Inline specialization identity is shared by
// translation units and signatures; aliases of the same declaration name the same specialization.
template <auto Source>
inline const unsigned char named_function_origin_tag = 0;
} // namespace detail

class NamedFunctionOrigin final {
 public:
  template <auto Source>
  [[nodiscard]] static NamedFunctionOrigin of() noexcept {
    static_assert(std::is_pointer_v<decltype(Source)> &&
                  std::is_function_v<std::remove_pointer_t<decltype(Source)>>);
    static_assert(Source != nullptr);
    return NamedFunctionOrigin(&detail::named_function_origin_tag<Source>);
  }
  [[nodiscard]] friend bool operator==(NamedFunctionOrigin, NamedFunctionOrigin) noexcept = default;

 private:
  explicit NamedFunctionOrigin(const void* token) noexcept : token_(token) {}
  const void* token_;
};

// An opt-in target for a contextual invocation of one original named declaration. The compiler must
// prove that Implementation invokes that declaration's original body over owner-preserving views.
// This adds no conversion or function-pointer reinterpretation. Ordinary Function state/layout and
// std::function storage remain unchanged. Copies share invocation state through the existing Function.
template <typename Signature>
class NamedFunction;

template <typename Result, typename... Parameters>
class NamedFunction<Result(Parameters...)> final {
 public:
  using native_pointer = Result (*)(Parameters...);
  template <auto Source, typename Implementation>
    requires std::is_invocable_r_v<Result, Implementation&, Parameters...>
  [[nodiscard]] static NamedFunction from(Implementation&& implementation) {
    native_pointer native = nullptr;
    if constexpr (std::is_convertible_v<decltype(Source), native_pointer>) native = Source;
    return NamedFunction(NamedFunctionOrigin::of<Source>(),
                         Function<Result(Parameters...)>(std::forward<Implementation>(implementation)), native);
  }

  [[nodiscard]] NamedFunctionOrigin origin() const noexcept { return origin_; }
  [[nodiscard]] bool matches_native(native_pointer pointer) const noexcept {
    return native_ != nullptr && native_ == pointer;
  }
  Result operator()(Parameters... parameters) const {
    return implementation_(std::forward<Parameters>(parameters)...);
  }
  [[nodiscard]] friend bool operator==(const NamedFunction& left, const NamedFunction& right) noexcept {
    return left.origin_ == right.origin_;
  }

 private:
  NamedFunction(NamedFunctionOrigin origin, Function<Result(Parameters...)> implementation, native_pointer native)
      : origin_(origin), implementation_(std::move(implementation)), native_(native) {}
  NamedFunctionOrigin origin_;
  Function<Result(Parameters...)> implementation_;
  native_pointer native_;
};

// JavaScript function equality is reference identity. Identity-bearing runtime callables answer it
// directly; an emitter-erased std::function cannot do so for stateful targets, and guessing from its
// target type would silently disconnect a different closure. Keep that boundary explicit and fail
// before reporting a false match. Plain function pointers are the one std::function target whose
// identity remains recoverable after erasure.
template <typename Callable>
[[nodiscard]] bool callable_identity_equal(const Callable& left, const Callable& right) {
  if constexpr (requires { { left == right } -> std::convertible_to<bool>; }) {
    return left == right;
  } else {
    throw std::logic_error(
        "callable identity is unavailable for this erased callback carrier");
  }
}

template <typename Result, typename... Parameters>
[[nodiscard]] bool callable_identity_equal(
    const std::function<Result(Parameters...)>& left,
    const std::function<Result(Parameters...)>& right) {
  using Pointer = Result (*)(Parameters...);
  const auto* left_pointer = left.template target<Pointer>();
  const auto* right_pointer = right.template target<Pointer>();
  if (left_pointer != nullptr && right_pointer != nullptr) {
    return *left_pointer == *right_pointer;
  }
  using Named = NamedFunction<Result(Parameters...)>;
  const auto* left_named = left.template target<Named>();
  const auto* right_named = right.template target<Named>();
  if (left_named != nullptr && right_named != nullptr) {
    return left_named->origin() == right_named->origin();
  }
  if (left_named != nullptr && right_pointer != nullptr) return left_named->matches_native(*right_pointer);
  if (left_pointer != nullptr && right_named != nullptr) return right_named->matches_native(*left_pointer);
  // An identity-bearing Flight Function can deliberately cross an ABI surface that still spells
  // its callback as std::function. std::function preserves that target object on copy, so recover
  // the retained Function and compare its shared state just as the non-erased overload does. This
  // is the one stateful target whose JavaScript function identity remains represented after the
  // outer erasure; arbitrary lambdas still fail loudly below.
  using Identified = Function<Result(Parameters...)>;
  const auto* left_identified = left.template target<Identified>();
  const auto* right_identified = right.template target<Identified>();
  if (left_identified != nullptr && right_identified != nullptr) {
    return *left_identified == *right_identified;
  }
  // A declared function and a retained closure are distinct source objects. Both identities are
  // represented here; only an unrecognized target below has lost its identity through erasure.
  if ((left_pointer != nullptr && right_identified != nullptr) ||
      (left_identified != nullptr && right_pointer != nullptr) ||
      (left_named != nullptr && right_identified != nullptr) ||
      (left_identified != nullptr && right_named != nullptr)) {
    return false;
  }
  throw std::logic_error(
      "callable identity is unavailable after std::function erasure");
}

// Both signature-specific targets carry the same declaration token. Unknown erased closures still
// fail explicitly: neither their signature nor the shape of their body establishes source identity.
template <typename LeftResult, typename... LeftParameters, typename RightResult, typename... RightParameters>
  requires(!std::same_as<LeftResult(LeftParameters...), RightResult(RightParameters...)>)
[[nodiscard]] bool callable_identity_equal(
    const std::function<LeftResult(LeftParameters...)>& left,
    const std::function<RightResult(RightParameters...)>& right) {
  const auto* left_named = left.template target<NamedFunction<LeftResult(LeftParameters...)>>();
  const auto* right_named = right.template target<NamedFunction<RightResult(RightParameters...)>>();
  if (left_named != nullptr && right_named != nullptr) return left_named->origin() == right_named->origin();
  throw std::logic_error("cross-signature callable identity requires both original named declarations");
}

// A source comparison with a bare declaration supplies that exact declaration at compile time. A
// same-signature raw native pointer remains comparable; no pointer of another signature is cast.
template <auto Source, typename Result, typename... Parameters>
[[nodiscard]] bool callable_identity_equal_named(const std::function<Result(Parameters...)>& callable) {
  if (const auto* named = callable.template target<NamedFunction<Result(Parameters...)>>()) {
    return named->origin() == NamedFunctionOrigin::of<Source>();
  }
  if (const auto* pointer = callable.template target<decltype(Source)>()) return *pointer == Source;
  if (callable.template target<Function<Result(Parameters...)>>() != nullptr) return false;
  throw std::logic_error("callable identity is unavailable after std::function erasure");
}

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
