#pragma once

#include <cmath>
#include <concepts>
#include <variant>

namespace flight {

namespace detail {

// The subject a reference-backed carrier stands for, and which accessor answered for it.
//
// A structural row view answers with the owner it projects -- the same owner for every view of one
// object, whatever schema the view was spelled with -- and a shared reference answers with the object
// itself. Those two answers are not comparable addresses, so a subject is only comparable to another
// subject that came through the same accessor; a carrier with neither accessor (a value, an erased
// `Any`, a callable) has no subject at all and is left to its own equality.
struct SameValueZeroSubject {
  const void* accessor = nullptr;
  const void* address = nullptr;

  [[nodiscard]] explicit operator bool() const noexcept {
    return accessor != nullptr && address != nullptr;
  }
};

inline const char same_value_zero_row_accessor = 0;
inline const char same_value_zero_reference_accessor = 0;

template <typename Value>
[[nodiscard]] SameValueZeroSubject same_value_zero_subject(const Value& value) noexcept {
  if constexpr (requires { value.shared_owner(); value.identity(); }) {
    return {&same_value_zero_row_accessor, value.identity()};
  } else if constexpr (requires { static_cast<const void*>(value.get()); }) {
    return {&same_value_zero_reference_accessor, static_cast<const void*>(value.get())};
  } else {
    return {};
  }
}

template <typename... Alternatives>
[[nodiscard]] SameValueZeroSubject same_value_zero_subject(
    const std::variant<Alternatives...>& value) noexcept {
  return std::visit([](const auto& alternative) { return same_value_zero_subject(alternative); }, value);
}

} // namespace detail

template <typename Value>
struct SameValueZero {
  bool operator()(const Value& left, const Value& right) const noexcept(noexcept(left == right)) {
    if constexpr (std::floating_point<Value>) {
      return left == right || (std::isnan(left) && std::isnan(right));
    } else {
      return left == right;
    }
  }
};

// A union of reference-backed subjects answers identity ACROSS its arms.
//
// The arms of such a union are views of one subject -- the emitter builds `Set<variant<ViewA, ViewB>>`
// when one call takes two capability slots, and a caller that passes one object through both slots hands
// over two views of it. C++ compares a variant's index before its value, so those two views are unequal
// by `==` and the subject would be counted twice, while the source's `Set` holds the subject itself and
// counts it once. Identity is the answer for reference-backed arms, and it is the answer the source
// already had.
//
// Nothing else about variant equality moves: a variant whose active alternatives are not
// reference-backed carriers -- a union of numbers, strings, records by value -- keeps `==`, which is
// index-then-value, and two subjects that arrived through *different* accessors are not compared at all,
// because their answers are different shapes of address.
template <typename... Alternatives>
struct SameValueZero<std::variant<Alternatives...>> {
  bool operator()(const std::variant<Alternatives...>& left,
                  const std::variant<Alternatives...>& right) const noexcept {
    const auto leftSubject = detail::same_value_zero_subject(left);
    const auto rightSubject = detail::same_value_zero_subject(right);
    if (leftSubject && rightSubject && leftSubject.accessor == rightSubject.accessor) {
      return leftSubject.address == rightSubject.address;
    }
    if (left.index() != right.index()) return false;
    // The same alternative on both sides: its own SameValueZero, so a `double` alternative keeps the
    // NaN clause the scalar rule has, and a variant of values answers exactly as the values do. A
    // variant comparable by `==` has comparable alternatives, so this is never a wider requirement than
    // the fallback it replaces.
    return std::visit(
        [](const auto& leftValue, const auto& rightValue) -> bool {
          if constexpr (std::same_as<std::remove_cvref_t<decltype(leftValue)>,
                                     std::remove_cvref_t<decltype(rightValue)>>) {
            return SameValueZero<std::remove_cvref_t<decltype(leftValue)>>{}(leftValue, rightValue);
          }
          return false;
        },
        left, right);
  }
};

template <typename Value>
bool same_value_zero(const Value& left, const Value& right)
    noexcept(noexcept(SameValueZero<Value>{}(left, right))) {
  return SameValueZero<Value>{}(left, right);
}

} // namespace flight
