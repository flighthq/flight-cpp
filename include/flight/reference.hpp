#pragma once

#include <concepts>
#include <iterator>
#include <mutex>
#include <optional>
#include <typeindex>
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

// CONSTRUCTION-TIME CONCRETE OWNER PROVENANCE, AND THE DEFERRED REFINEMENT VIEW.
//
// A derived interface may narrow an inherited reference cell to a native interface derived from the
// base's. Native heritage keeps ONE cell, typed by the base, so the source getter returns whatever
// object is stored: no check, no cast, no throw. A base-typed write is legal and may store either a
// genuine derived allocation or a base-only one, and a later base member read must still work.
//
// The only evidence that survives every write is what the object IS, captured where the object is
// allocated. `make_ref` therefore registers the concrete type of an opted-in type, keyed at each of
// its native base subobject addresses. A widened reference to a genuine derived allocation still
// finds its concrete type; a base-only allocation has no such record. The registration holds the
// object weakly, so it cannot extend a lifetime, and address reuse is checked on every lookup.
//
// `RefinedOwnerView` is the getter's return value. It never checks at construction: base members are
// served through `operator->` for every concrete owner, exactly as the source serves inherited
// members through a widened reference. Only a DERIVED capability is asked for, through `as()`, and
// absence is returned rather than thrown, so a missing derived property fails at the operation that
// requires it, not at the read and not for inherited base members.
template <typename Type>
struct RefinementConstructionRegistration {
  static void register_owner(const std::shared_ptr<Type>&) {}
};

namespace detail {

struct RefinedOwnerMetadata final {
  std::weak_ptr<void> owner;
  std::type_index concrete{typeid(void)};
  bool (*accepts)(std::type_index) noexcept{nullptr};
};

struct RefinedOwnerRegistry final {
  std::mutex mutex;
  std::unordered_map<const void*, RefinedOwnerMetadata> entries;
  std::size_t sweep_threshold{64};
};

[[nodiscard]] inline RefinedOwnerRegistry& refined_owner_registry() {
  static RefinedOwnerRegistry registry;
  return registry;
}

inline void remember_refined_owner(const void* address,
                                   const std::shared_ptr<void>& owner,
                                   std::type_index concrete,
                                   bool (*accepts)(std::type_index) noexcept) {
  auto& registry = refined_owner_registry();
  const std::lock_guard lock(registry.mutex);
  if (registry.entries.size() >= registry.sweep_threshold) {
    for (auto entry = registry.entries.begin(); entry != registry.entries.end();) {
      entry = entry->second.owner.expired() ? registry.entries.erase(entry) : std::next(entry);
    }
    registry.sweep_threshold = registry.entries.size() * 2 + 64;
  }
  registry.entries.insert_or_assign(address, RefinedOwnerMetadata{owner, concrete, accepts});
}

[[nodiscard]] inline std::optional<RefinedOwnerMetadata> recall_refined_owner(const void* address) {
  auto& registry = refined_owner_registry();
  const std::lock_guard lock(registry.mutex);
  const auto found = registry.entries.find(address);
  if (found == registry.entries.end()) return std::nullopt;
  if (found->second.owner.expired()) {
    registry.entries.erase(found);
    return std::nullopt;
  }
  return found->second;
}

} // namespace detail

template <typename... Accepted>
[[nodiscard]] bool refinement_type_matches(std::type_index expected) noexcept {
  static_assert(sizeof...(Accepted) > 0);
  return ((expected == std::type_index(typeid(Accepted))) || ...);
}

// Records the concrete allocation at every native base subobject address the source can widen it
// to. `Concrete` and every `NativeBase` are accepted by the capability query, which is exactly the
// set of reads the concrete type can serve.
template <typename Concrete, typename... NativeBases>
void register_refined_owner(const std::shared_ptr<Concrete>& object) {
  static_assert(sizeof...(NativeBases) > 0);
  static_assert((std::derived_from<Concrete, NativeBases> && ...));
  if (!object) return;
  auto erased = std::static_pointer_cast<void>(object);
  ((detail::remember_refined_owner(
        static_cast<const void*>(static_cast<NativeBases*>(object.get())),
        erased,
        std::type_index(typeid(Concrete)),
        &refinement_type_matches<Concrete, NativeBases...>)),
   ...);
}

template <typename Derived, typename Base>
class RefinedOwnerView final {
 public:
  RefinedOwnerView() = default;
  explicit RefinedOwnerView(Ref<Base> value) : value_(std::optional<Ref<Base>>{std::move(value)}) {}
  // The generated getter hands over the cell's optional directly. An absent slot is an empty view,
  // not a construction failure: absence fails at the member access that requires a value.
  explicit RefinedOwnerView(std::optional<Ref<Base>> value) : value_(std::move(value)) {}

  [[nodiscard]] Ref<Base> base() const { return value_.value_or(nullptr); }
  [[nodiscard]] bool has_value() const noexcept { return value_.has_value() && static_cast<bool>(*value_); }
  [[nodiscard]] explicit operator bool() const noexcept { return has_value(); }

  // Inherited (base) members: served for every concrete owner, including a base-only allocation.
  // No capability check, so this is exactly the source's widened base read.
  [[nodiscard]] Ref<Base> operator->() const { return value_.value(); }

  // A derived capability. Absence is reported, not thrown: the caller's next operation on the value
  // is where the source would first observe the missing capability.
  [[nodiscard]] std::optional<Ref<Derived>> as() const {
    static_assert(std::derived_from<Derived, Base>,
                  "a refined owner view narrows along declared native heritage");
    if (!value_ || !*value_) return std::nullopt;
    const auto metadata = detail::recall_refined_owner(static_cast<const void*>((*value_).get()));
    if (!metadata || !metadata->accepts ||
        !metadata->accepts(std::type_index(typeid(Derived)))) {
      return std::nullopt;
    }
    return std::optional<Ref<Derived>>{std::static_pointer_cast<Derived>(*value_)};
  }

  friend bool operator==(const RefinedOwnerView& left, const RefinedOwnerView& right) noexcept {
    return left.value_ == right.value_;
  }
  friend bool operator==(const RefinedOwnerView& left, const Ref<Base>& right) noexcept {
    return left.value_ == std::optional<Ref<Base>>{right};
  }

 private:
  std::optional<Ref<Base>> value_{};
};

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
    // The concrete allocation is recorded here, before any later widening or base-typed write can
    // erase the only place its type was known. The primary registration is a no-op; only a type the
    // emitter marks as a native refinement owner supplies one.
    RefinementConstructionRegistration<Value>::register_owner(reference);
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
