#pragma once

#include <concepts>
#include <iterator>
#include <mutex>
#include <unordered_map>
#include <memory>
#include <type_traits>
#include <utility>

namespace flight {

// Marks generated object and class shapes while preserving aggregate initialization.
struct ReferenceEnabled {};

// Opt-in: this type can recover its own owning reference from `this`.
//
// The obvious implementation -- deriving `ReferenceEnabled` from `std::enable_shared_from_this` --
// does not work here, and the reason is worth recording so nobody tries it twice. Emitted objects
// are AGGREGATES, initialized as `AudioChannelRuntime{.backend = b, .device = d}`, and
// `enable_shared_from_this` has a protected default constructor, so such an initializer stops
// compiling:
//
//     error: 'enable_shared_from_this()' is protected within this context
//     error: missing initializer for member '...::enable_shared_from_this'
//
// Registering every object in a side table instead costs 13x on construction -- measured, 200k
// objects, 2.8ms to 37.1ms -- and retains an entry for every object whether or not anything ever
// asks. That is a heavy toll for a capability a handful of types need.
//
// So the marker is EMPTY and OPT-IN. An empty base keeps the aggregate an aggregate and adds no
// bytes, and `make_ref` registers only types that carry it, so a type that never asks pays
// nothing at all. The compiler emits this base only on shapes whose methods pass the receiver on.
struct SelfReferencing {};

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
namespace detail {

// Weak by construction: remembering an owner must not become a reason the object cannot die. An
// address can be reused once its object is gone, so expiry is checked on every lookup and the
// sweep only keeps the table from growing without bound.
struct SelfReferenceRegistry final {
  std::mutex mutex;
  std::unordered_map<const void*, std::weak_ptr<void>> entries;
  std::size_t sweep_threshold{64};
};

[[nodiscard]] inline SelfReferenceRegistry& self_reference_registry() {
  static SelfReferenceRegistry registry;
  return registry;
}

template <typename Value>
void remember_self_reference(const std::shared_ptr<Value>& reference) {
  auto& registry = self_reference_registry();
  const std::lock_guard lock(registry.mutex);
  if (registry.entries.size() >= registry.sweep_threshold) {
    for (auto entry = registry.entries.begin(); entry != registry.entries.end();) {
      entry = entry->second.expired() ? registry.entries.erase(entry) : std::next(entry);
    }
    registry.sweep_threshold = registry.entries.size() * 2 + 64;
  }
  registry.entries.insert_or_assign(static_cast<const void*>(reference.get()),
                                    std::static_pointer_cast<void>(reference));
}

[[nodiscard]] inline std::shared_ptr<void> recall_self_reference(const void* self) {
  auto& registry = self_reference_registry();
  const std::lock_guard lock(registry.mutex);
  const auto found = registry.entries.find(self);
  if (found == registry.entries.end()) return {};
  auto owner = found->second.lock();
  if (!owner) registry.entries.erase(found);
  return owner;
}

} // namespace detail

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
    auto reference = std::make_shared<Value>(std::forward<Arguments>(arguments)...);
    if constexpr (std::derived_from<Value, SelfReferencing>) {
      detail::remember_self_reference(reference);
    }
    return reference;
  }
}

// Recovers the owning reference a `SelfReferencing` object was created with -- the runtime half of
// `this->add(this)`, where the receiver has to be passed on as an owning reference and a raw
// `this` cannot be one.
//
// Returns EMPTY rather than throwing when the object has no owner to recover: one built on the
// stack, or built by some path other than `make_ref`. That case is real and a caller can act on
// it, whereas a fabricated reference would hand out a second owner for an object that already has
// one -- or none -- and the double free would land far from here.
template <typename Value>
[[nodiscard]] Ref<Value> ref_from_this(const Value* self) {
  static_assert(std::derived_from<Value, SelfReferencing>,
                "flight::ref_from_this needs the type to derive from flight::SelfReferencing, "
                "which is what makes make_ref remember its owner");
  auto erased = detail::recall_self_reference(static_cast<const void*>(self));
  if (!erased) return {};
  return std::static_pointer_cast<Value>(std::move(erased));
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
