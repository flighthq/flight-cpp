#pragma once

#include <any>
#include <cmath>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <typeindex>
#include <utility>
#include <variant>

#include <flight/equality.hpp>
#include <flight/error.hpp>
#include <flight/number.hpp>
#include <flight/presence.hpp>
#include <flight/reference.hpp>
#include <flight/string.hpp>
#include <flight/symbol.hpp>

// The erased dynamic value for the type positions TypeScript writes as `any` or `unknown` and
// deliberately leaves unconstrained -- `AnimationChannel.targetRef`, `NativeWindowHandle`,
// `Node<any>`, and the rest of the SDK's opaque carriers.
//
// The whole point of the type is that it does not decide what it holds, so three things are stated
// rather than approximated:
//
//  * `flight::Ref<void>` is not a substitute. A value written `unknown` may be a number, and
//    representing it as an object reference would silently misstate it. Every ECMAScript language
//    type this runtime has gets its own alternative here.
//  * Absence has two meanings and both survive. `Any` can hold `undefined`, which is a value that
//    is present; a container that has no entry at all reports that through `std::optional<Any>`,
//    which is what `flight::Record::get` already returns. `std::optional<Any>()` is a missing
//    entry, `std::optional<Any>(flight::Any())` is an entry whose value is `undefined`, and the two
//    never collapse into each other.
//  * Nothing is coerced behind the caller's back. Reading a number out of a string throws;
//    ordering a symbol or an object throws, because ECMAScript defines those through ToPrimitive
//    and this runtime has no prototype chain to ask.
//
// Known gap, stated rather than faked: there is no `bigint` alternative, because the runtime has no
// arbitrary-precision integer. A host that needs one carries it as an external value today.
namespace flight {

class BadAnyAccess final : public std::runtime_error {
 public:
  BadAnyAccess() : std::runtime_error("flight::Any does not hold the requested type") {}
};

// The ECMAScript `typeof` domain this runtime represents. `function` is separated from `object`
// because `typeof` separates them, even though both are object references.
enum class AnyKind : std::uint8_t {
  undefined,
  null,
  boolean,
  number,
  string,
  symbol,
  object,
  function,
  external,
};

class Any;

namespace detail {

// An object reference with its concrete type retained, so a value that went in as `Ref<Widget>`
// comes back as `Ref<Widget>` and never as a reinterpreted `Ref<void>`.
struct AnyObject final {
  std::shared_ptr<void> object;
  std::type_index type{typeid(void)};

  [[nodiscard]] friend bool operator==(const AnyObject& left, const AnyObject& right) noexcept {
    return left.object == right.object;
  }
};

// A callable, or a host value that is neither a primitive nor a Flight-owned object. The shared
// holder gives it the reference identity JavaScript gives every non-primitive.
struct AnyOpaque final {
  std::shared_ptr<const std::any> value;

  [[nodiscard]] friend bool operator==(const AnyOpaque& left, const AnyOpaque& right) noexcept {
    return left.value == right.value;
  }
};

template <typename Type>
inline constexpr bool is_any_shared_ptr = false;

template <typename Type>
inline constexpr bool is_any_shared_ptr<std::shared_ptr<Type>> = true;

} // namespace detail

class Any final {
 public:
  // A default-constructed value is `undefined`, matching an unwritten JavaScript binding.
  Any() = default;

  Any(Undefined) noexcept {}
  Any(Null) noexcept : value_(Null{}) {}
  Any(std::nullptr_t) noexcept : value_(Null{}) {}
  Any(bool value) noexcept : value_(value) {}

  template <typename Number>
    requires(std::is_arithmetic_v<Number> && !std::same_as<std::remove_cvref_t<Number>, bool>)
  Any(Number value) : value_(static_cast<double>(value)) {}

  Any(String value) : value_(std::move(value)) {}
  Any(const char* value) : value_(String(value)) {}
  Any(Symbol value) : value_(std::move(value)) {}

  // An object reference keeps its concrete type. A null reference is `null`, not an object,
  // because that is what a JavaScript object-typed binding holding nothing is.
  template <typename Object>
  [[nodiscard]] static Any object(std::shared_ptr<Object> reference) {
    if (!reference) return Any(null);
    Any result;
    result.value_ = detail::AnyObject{std::static_pointer_cast<void>(std::move(reference)),
                                      std::type_index(typeid(Object))};
    return result;
  }

  // A callable. `typeof` reports `function`, and copies of one callable share one identity.
  template <typename Callable>
  [[nodiscard]] static Any function(Callable callable) {
    Any result;
    result.value_ = Callable_{detail::AnyOpaque{std::make_shared<const std::any>(
        std::in_place_type<Callable>, std::move(callable))}};
    return result;
  }

  // A host value the runtime never interprets: a native window handle, a device token, an
  // animation target the animation core is documented never to read. `typeof` reports `object`,
  // which is what a host object is, and the host recovers its own type with `external_if`.
  template <typename External>
  [[nodiscard]] static Any external(External value) {
    Any result;
    result.value_ = detail::AnyOpaque{
        std::make_shared<const std::any>(std::in_place_type<External>, std::move(value))};
    return result;
  }

  [[nodiscard]] AnyKind kind() const noexcept {
    switch (value_.index()) {
      case 0:
        return AnyKind::undefined;
      case 1:
        return AnyKind::null;
      case 2:
        return AnyKind::boolean;
      case 3:
        return AnyKind::number;
      case 4:
        return AnyKind::string;
      case 5:
        return AnyKind::symbol;
      case 6:
        return AnyKind::object;
      case 7:
        return AnyKind::function;
      default:
        return AnyKind::external;
    }
  }

  // ECMAScript `typeof`. `null` reports `object`, preserving the language's own well-known result
  // rather than correcting it.
  [[nodiscard]] String type_of() const {
    switch (kind()) {
      case AnyKind::undefined:
        return String("undefined");
      case AnyKind::boolean:
        return String("boolean");
      case AnyKind::number:
        return String("number");
      case AnyKind::string:
        return String("string");
      case AnyKind::symbol:
        return String("symbol");
      case AnyKind::function:
        return String("function");
      case AnyKind::null:
      case AnyKind::object:
      case AnyKind::external:
        break;
    }
    return String("object");
  }

  [[nodiscard]] bool is_undefined() const noexcept { return kind() == AnyKind::undefined; }
  [[nodiscard]] bool is_null() const noexcept { return kind() == AnyKind::null; }

  // `== null` in TypeScript: the one test that admits both absent values and nothing else.
  [[nodiscard]] bool is_nullish() const noexcept { return is_undefined() || is_null(); }

  [[nodiscard]] bool as_boolean() const { return require<bool>(); }
  [[nodiscard]] double as_number() const { return require<double>(); }
  [[nodiscard]] const String& as_string() const { return require<String>(); }
  [[nodiscard]] const Symbol& as_symbol() const { return require<Symbol>(); }

  // ECMAScript ToBoolean. Total, because the language defines it for every value.
  [[nodiscard]] bool to_boolean() const {
    switch (kind()) {
      case AnyKind::undefined:
      case AnyKind::null:
        return false;
      case AnyKind::boolean:
        return std::get<bool>(value_);
      case AnyKind::number: {
        const double number = std::get<double>(value_);
        return number != 0.0 && !std::isnan(number);
      }
      case AnyKind::string:
        return std::get<String>(value_).length() > 0;
      case AnyKind::symbol:
      case AnyKind::object:
      case AnyKind::function:
      case AnyKind::external:
        break;
    }
    return true;
  }

  // Recovers an object reference at the exact type it was stored as. An empty result means this
  // value holds something else, so a caller selects rather than reinterprets.
  template <typename Object>
  [[nodiscard]] Ref<Object> object_if() const noexcept {
    const auto* stored = std::get_if<detail::AnyObject>(&value_);
    if (!stored || stored->type != std::type_index(typeid(Object))) return {};
    return std::static_pointer_cast<Object>(stored->object);
  }

  // Recovers a host value or a callable at the exact type it was stored as.
  template <typename External>
  [[nodiscard]] const External* external_if() const noexcept {
    if (const auto* opaque = std::get_if<detail::AnyOpaque>(&value_)) {
      return opaque->value ? std::any_cast<External>(opaque->value.get()) : nullptr;
    }
    if (const auto* callable = std::get_if<Callable_>(&value_)) {
      return callable->held.value ? std::any_cast<External>(callable->held.value.get()) : nullptr;
    }
    return nullptr;
  }

  // The type a non-primitive went in as. `typeid(void)` for every primitive.
  [[nodiscard]] std::type_index held_type() const noexcept {
    if (const auto* stored = std::get_if<detail::AnyObject>(&value_)) return stored->type;
    if (const auto* opaque = std::get_if<detail::AnyOpaque>(&value_)) {
      return opaque->value ? std::type_index(opaque->value->type()) : std::type_index(typeid(void));
    }
    if (const auto* callable = std::get_if<Callable_>(&value_)) {
      return callable->held.value ? std::type_index(callable->held.value->type())
                                  : std::type_index(typeid(void));
    }
    return std::type_index(typeid(void));
  }

  // Reference identity, and only reference identity: null for every primitive, because primitives
  // do not have it.
  [[nodiscard]] const void* identity() const noexcept {
    if (const auto* stored = std::get_if<detail::AnyObject>(&value_)) return stored->object.get();
    if (const auto* opaque = std::get_if<detail::AnyOpaque>(&value_)) return opaque->value.get();
    if (const auto* callable = std::get_if<Callable_>(&value_)) return callable->held.value.get();
    return nullptr;
  }

  // ECMAScript `===`. NaN is not equal to itself; +0 and -0 are equal; non-primitives compare by
  // reference identity.
  [[nodiscard]] bool strict_equals(const Any& other) const {
    if (value_.index() != other.value_.index()) return false;
    switch (kind()) {
      case AnyKind::undefined:
      case AnyKind::null:
        return true;
      case AnyKind::boolean:
        return std::get<bool>(value_) == std::get<bool>(other.value_);
      case AnyKind::number:
        return std::get<double>(value_) == std::get<double>(other.value_);
      case AnyKind::string:
        return std::get<String>(value_) == std::get<String>(other.value_);
      case AnyKind::symbol:
        return std::get<Symbol>(value_) == std::get<Symbol>(other.value_);
      case AnyKind::object:
      case AnyKind::function:
      case AnyKind::external:
        break;
    }
    return identity() == other.identity();
  }

  // SameValueZero, the comparison Map keys, Set members, and Array.includes use: `===` except that
  // NaN matches NaN.
  [[nodiscard]] bool same_value_zero(const Any& other) const {
    if (kind() == AnyKind::number && other.kind() == AnyKind::number) {
      const double left = std::get<double>(value_);
      const double right = std::get<double>(other.value_);
      return left == right || (std::isnan(left) && std::isnan(right));
    }
    return strict_equals(other);
  }

  // Object.is: SameValueZero, except that +0 and -0 are distinguished.
  [[nodiscard]] bool same_value(const Any& other) const {
    if (kind() == AnyKind::number && other.kind() == AnyKind::number) {
      const double left = std::get<double>(value_);
      const double right = std::get<double>(other.value_);
      if (left == 0.0 && right == 0.0) return std::signbit(left) == std::signbit(right);
    }
    return same_value_zero(other);
  }

  // `==` is `===`. The loose equality operator is deliberately absent: it is defined through
  // ToPrimitive, and this runtime has no prototype chain to ask for one.
  [[nodiscard]] friend bool operator==(const Any& left, const Any& right) {
    return left.strict_equals(right);
  }

 private:
  // Distinguishes a callable from an ordinary host value while sharing its storage, so `typeof`
  // can answer `function` without a second erasure mechanism.
  struct Callable_ final {
    detail::AnyOpaque held;

    [[nodiscard]] friend bool operator==(const Callable_& left, const Callable_& right) noexcept {
      return left.held == right.held;
    }
  };

  template <typename Held>
  [[nodiscard]] const Held& require() const {
    const auto* held = std::get_if<Held>(&value_);
    if (!held) throw BadAnyAccess();
    return *held;
  }

  std::variant<Undefined, Null, bool, double, String, Symbol, detail::AnyObject, Callable_,
               detail::AnyOpaque>
      value_;
};

// The ECMAScript abstract relational comparison, for the primitive domain it is defined over
// without a prototype chain. Two numbers where either is NaN are `unordered`, which is exactly
// what makes every relational operator on them false.
//
// A symbol operand throws TypeError, as it does in JavaScript. An object, function, or external
// operand also throws, rather than being compared through a fabricated "[object Object]": these
// values have no toString the runtime can consult, so any answer would be invented.
[[nodiscard]] inline std::partial_ordering relational_compare(const Any& left, const Any& right) {
  const auto reject = [](const Any& value) {
    switch (value.kind()) {
      case AnyKind::symbol:
        throw TypeError(String("a symbol cannot be converted to a primitive for comparison"));
      case AnyKind::object:
      case AnyKind::function:
      case AnyKind::external:
        throw TypeError(
            String("flight::Any cannot order a non-primitive: the runtime has no ToPrimitive"));
      case AnyKind::undefined:
      case AnyKind::null:
      case AnyKind::boolean:
      case AnyKind::number:
      case AnyKind::string:
        break;
    }
  };
  reject(left);
  reject(right);

  if (left.kind() == AnyKind::string && right.kind() == AnyKind::string) {
    const auto& first = left.as_string();
    const auto& second = right.as_string();
    if (first == second) return std::partial_ordering::equivalent;
    return first < second ? std::partial_ordering::less : std::partial_ordering::greater;
  }

  // ToNumber for the remaining primitive alternatives, exactly as the language defines it.
  const auto to_double = [](const Any& value) -> double {
    switch (value.kind()) {
      case AnyKind::undefined:
        return std::numeric_limits<double>::quiet_NaN();
      case AnyKind::null:
        return 0.0;
      case AnyKind::boolean:
        return value.as_boolean() ? 1.0 : 0.0;
      case AnyKind::number:
        return value.as_number();
      case AnyKind::string:
        return to_number(value.as_string());
      default:
        return std::numeric_limits<double>::quiet_NaN();
    }
  };
  const double first = to_double(left);
  const double second = to_double(right);
  if (std::isnan(first) || std::isnan(second)) return std::partial_ordering::unordered;
  if (first < second) return std::partial_ordering::less;
  if (first > second) return std::partial_ordering::greater;
  return std::partial_ordering::equivalent;
}

// Map keys, Set members, and Array.includes go through SameValueZero, so the runtime's own
// equality policy answers for erased values too.
template <>
struct SameValueZero<Any> {
  bool operator()(const Any& left, const Any& right) const { return left.same_value_zero(right); }
};

// A container entry that may be missing. `std::optional<Any>()` is "no such entry" and
// `AnySlot(Any())` is "an entry whose value is undefined"; nothing in the runtime conflates them.
using AnySlot = std::optional<Any>;

} // namespace flight

namespace std {

template <>
struct hash<flight::Any> {
  [[nodiscard]] size_t operator()(const flight::Any& value) const {
    switch (value.kind()) {
      case flight::AnyKind::undefined:
        return 0;
      case flight::AnyKind::null:
        return 1;
      case flight::AnyKind::boolean:
        return hash<bool>{}(value.as_boolean());
      case flight::AnyKind::number: {
        // NaN hashes to one bucket so SameValueZero equality and hashing agree.
        const double number = value.as_number();
        if (std::isnan(number)) return 2;
        return hash<double>{}(number == 0.0 ? 0.0 : number);
      }
      case flight::AnyKind::string:
        return hash<string>{}(value.as_string().to_utf8());
      case flight::AnyKind::symbol:
        return hash<const void*>{}(value.as_symbol().identity());
      case flight::AnyKind::object:
      case flight::AnyKind::function:
      case flight::AnyKind::external:
        break;
    }
    return hash<const void*>{}(value.identity());
  }
};

} // namespace std
