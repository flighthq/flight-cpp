#pragma once

#include <compare>
#include <concepts>
#include <cstddef>
#include <memory>
#include <optional>
#include <type_traits>
#include <typeindex>
#include <utility>

#include <flight/reference.hpp>

// An object reference whose static type is erased and whose ACTUAL type is not.
//
// This is the representation TypeScript's `object` needs in a position that is later narrowed back
// down — the entity binding slot, which `attachEntityBinding` fills with an arbitrary object and
// `getEntityBindingAs<Type>` reads as a `Type`.
//
// `flight::Ref<void>` cannot serve that position, and no runtime operation can rescue it. A
// `std::shared_ptr<void>` has already forgotten what it pointed at by the time it is stored, so a
// conversion out of it would be `std::static_pointer_cast`: it always "succeeds", and a binding read
// at the wrong type hands back a pointer to an object of another type that the caller then uses. The
// type has to be captured where it is still known — at the call that erases it — which is what the
// converting constructor below does and what `Ref<void>` has no place to record.
//
// Nothing here is a side table. The type travels inside the value, so there is no registry keyed on
// object identity, nothing to keep entries alive, and nothing to go stale.
namespace flight {

class ErasedRef final {
 public:
  ErasedRef() = default;

  ErasedRef(std::nullptr_t) noexcept {}

  // Erases a typed reference, recording the type it came in as. A null reference erases to an empty
  // `ErasedRef`, because a slot holding nothing holds nothing of any type.
  template <typename Object>
    requires(!std::same_as<std::remove_cvref_t<Object>, void>)
  ErasedRef(std::shared_ptr<Object> reference)
      : object_(reference ? std::static_pointer_cast<void>(std::move(reference))
                          : std::shared_ptr<void>()),
        type_(object_ ? std::type_index(typeid(Object)) : std::type_index(typeid(void))) {}

  [[nodiscard]] explicit operator bool() const noexcept { return static_cast<bool>(object_); }
  [[nodiscard]] bool has_value() const noexcept { return static_cast<bool>(object_); }

  // Recovers the reference at the type it was erased from, and NOTHING else. An empty result means
  // the slot holds an object of a different type, which is a question the caller answers rather
  // than a cast the runtime performs on its behalf.
  template <typename Object>
  [[nodiscard]] Ref<Object> as() const noexcept {
    if (!object_ || type_ != std::type_index(typeid(Object))) return {};
    return std::static_pointer_cast<Object>(object_);
  }

  // Whether `as<Object>()` would answer, without producing the reference.
  template <typename Object>
  [[nodiscard]] bool holds() const noexcept {
    return object_ && type_ == std::type_index(typeid(Object));
  }

  // The type the reference went in as; `typeid(void)` when it holds nothing.
  [[nodiscard]] std::type_index held_type() const noexcept { return type_; }

  // The erased pointer, for identity alone. It is deliberately not a way to reach the object at a
  // type: `as` is, and it checks.
  [[nodiscard]] const void* identity() const noexcept { return object_.get(); }

  [[nodiscard]] friend bool operator==(const ErasedRef& left, const ErasedRef& right) noexcept {
    return left.object_ == right.object_;
  }

  [[nodiscard]] friend bool operator==(const ErasedRef& left, std::nullptr_t) noexcept {
    return !left.object_;
  }

 private:
  std::shared_ptr<void> object_;
  std::type_index type_{typeid(void)};
};

// `getEntityBindingAs<Type>(source)`: the binding read at one type, or nothing. The optional is the
// slot's own absence — `binding` is `object | null` and may simply be empty — and an engaged
// optional holding an `ErasedRef` of another type answers empty here too, because a binding that is
// not a `Type` is not a `Type` however it came to be stored.
template <typename Object>
[[nodiscard]] Ref<Object> erased_ref_as(const std::optional<ErasedRef>& binding) noexcept {
  if (!binding) return {};
  return binding->template as<Object>();
}

template <typename Object>
[[nodiscard]] Ref<Object> erased_ref_as(const ErasedRef& binding) noexcept {
  return binding.template as<Object>();
}

} // namespace flight
