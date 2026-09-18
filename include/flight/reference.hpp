#pragma once

#include <concepts>
#include <memory>
#include <type_traits>
#include <utility>

namespace flight {

// Marks generated object and class shapes while preserving aggregate initialization.
struct ReferenceEnabled {};

namespace detail {

// Completeness is asked through overload resolution rather than through a partial specialization of
// a class template keyed on the type.
//
// The distinction is the whole fix. Two records that name each other -- `Node<T>` holding a
// `Ref<NodeRuntime<T>>` while `NodeRuntime<T>` holds a callable taking `Ref<Node<T>>` -- make the
// question re-enter itself: deciding `Ref<Node<T>>` instantiates `Node<T>`, which asks for
// `Ref<NodeRuntime<T>>`, which instantiates `NodeRuntime<T>`, which asks for `Ref<Node<T>>` again.
// A class template specialization cannot be used while it is still being selected, so the inner
// question failed outright, and no include order, forward declaration, or member ordering could
// help. A function template has no such state: the inner call re-runs overload resolution, finds
// `Node<T>` incomplete because it is mid-definition, and takes the fallback.
//
// The answer it lands on is the right one rather than a lucky one. A record is owned through a
// shared pointer whether it is complete or not, so the inner query and the outer query agree, and
// the type has one meaning throughout the program.
template <typename Value>
auto complete_probe(int) -> decltype(sizeof(Value), std::true_type{});

template <typename Value>
auto complete_probe(...) -> std::false_type;

// Incomplete: a record, owned through a shared pointer. The base-class question is asked only in the
// complete specialization below, so it is never put to a type that cannot answer it.
template <typename Value, bool Complete>
struct reference_shape {
  using type = std::shared_ptr<Value>;
};

// A shared_ptr is a complete class that is not a record, so it falls out of this unchanged, which is
// the idempotence interface projections rely on: applying Ref to something already reference-backed
// preserves the original owner instead of manufacturing a nested shared_ptr layer.
template <typename Value>
struct reference_shape<Value, true> {
  using type = std::conditional_t<std::derived_from<Value, ReferenceEnabled>, std::shared_ptr<Value>, Value>;
};


template <typename Value>
inline constexpr bool is_shared_ptr = false;

template <typename Value>
inline constexpr bool is_shared_ptr<std::shared_ptr<Value>> = true;

} // namespace detail

// The alias names `reference_shape` directly. An intermediate class template keyed on `Value` would
// be the very entity the recursive query re-enters, which is what made the earlier form fail.
template <typename Value>
using Ref =
    typename detail::reference_shape<Value, decltype(detail::complete_probe<Value>(0))::value>::type;

template <typename Value, typename... Arguments>
[[nodiscard]] Ref<Value> make_ref(Arguments&&... arguments) {
  // A type that names itself through `Ref`, directly or through another type that names it back, is
  // asked about while it is still incomplete, and the only answer available then is "record". That
  // is the right answer for a record and the wrong one for a value shape, and the disagreement would
  // otherwise be silent: the same spelling would mean a shared pointer inside the definition and a
  // value outside it. Deriving from `ReferenceEnabled` states the intent and makes both answers the
  // same.
  static_assert(std::derived_from<Value, ReferenceEnabled> ==
                    std::same_as<Ref<Value>, std::shared_ptr<Value>>,
                "this type is referenced before it is complete but is not marked ReferenceEnabled, "
                "so flight::Ref names two different types for it; derive it from "
                "flight::ReferenceEnabled");
  if constexpr (detail::is_shared_ptr<Value>) {
    return Value(std::forward<Arguments>(arguments)...);
  } else if constexpr (std::same_as<Ref<Value>, Value>) {
    return Value(std::forward<Arguments>(arguments)...);
  } else {
    return std::make_shared<Value>(std::forward<Arguments>(arguments)...);
  }
}

template <typename Value>
class BindingCell {
 public:
  explicit BindingCell(Value value) : value_(std::make_shared<Value>(std::move(value))) {}

  [[nodiscard]] Value read_binding() const { return *value_; }

  void rebind(Value value) const { *value_ = std::move(value); }

  template <typename Update>
  decltype(auto) update_binding(Update&& update) const {
    return std::forward<Update>(update)(*value_);
  }

 private:
  std::shared_ptr<Value> value_;
};

template <typename Value>
[[nodiscard]] BindingCell<Value> make_binding_cell(Value value) {
  return BindingCell<Value>(std::move(value));
}

} // namespace flight
