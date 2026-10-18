#pragma once

#include <concepts>
#include <iterator>
#include <memory>
#include <mutex>
#include <optional>
#include <type_traits>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <utility>

#include <flight/out_of_profile.hpp>

namespace flight {

// Marks generated object and class shapes while preserving aggregate initialization. It stays EMPTY: the
// exact-allocation provenance make_ref records lives in the shared_ptr control block (see
// detail::AllocationRecordingDeleter), not in the object.
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
namespace detail {

// A record emitted with native interface heritage names its one primary base as `flight_native_base`. The
// edge only extends the CAST chain recover_owner can walk; every make_ref allocation is recorded regardless.
template <typename Value>
concept has_native_base = requires { typename Value::flight_native_base; };

// Walks the allocated type's declared native base chain, returning the subobject whose type is `target`.
template <typename Value>
void* native_owner_cast(Value* object, std::type_index target) noexcept {
  if (std::type_index(typeid(Value)) == target) return object;
  if constexpr (has_native_base<Value>) {
    return native_owner_cast<typename Value::flight_native_base>(
        static_cast<typename Value::flight_native_base*>(object), target);
  } else {
    return nullptr;
  }
}

// Row metadata for an allocation's EXACT type, without naming RowOwner here. The primary reports none;
// flight/structural_ref.hpp supplies the real answer by a constrained partial specialization, the same
// shape AnyRowOwnerFactory uses. `view` builds an UNREGISTERED owner for one read (no registry write), and
// `own` returns the registered owner a write must go through. Both are erased to void.
template <typename Object>
struct RecoverableRowOwnerFactory {
  [[nodiscard]] static std::shared_ptr<void> view(const std::shared_ptr<void>&) { return nullptr; }
  [[nodiscard]] static std::shared_ptr<void> own(const std::shared_ptr<void>&) { return nullptr; }
};

// The static description of one exact allocated type. One instance per type, never per object.
struct AllocationDescriptor final {
  std::type_index type;
  // From the complete object to the subobject of `target` along the declared native base chain, or null.
  void* (*cast)(void*, std::type_index) noexcept;
  std::shared_ptr<void> (*view_row_owner)(const std::shared_ptr<void>&);
  std::shared_ptr<void> (*own_row_owner)(const std::shared_ptr<void>&);
};

template <typename Value>
inline const AllocationDescriptor allocation_descriptor{
    std::type_index(typeid(Value)),
    [](void* object, std::type_index target) noexcept -> void* {
      return native_owner_cast<Value>(static_cast<Value*>(object), target);
    },
    &RecoverableRowOwnerFactory<Value>::view,
    &RecoverableRowOwnerFactory<Value>::own};

// The deleter make_ref allocates every record with. It deletes as the exact type, and it is also where the
// allocation's provenance lives: the static descriptor of the type allocated, and the complete object.
// std::get_deleter finds it from ANY Ref sharing the control block -- a base conversion, an aliasing
// reference -- and finds nothing for a Ref that make_ref did not create.
struct AllocationRecordingDeleter final {
  const AllocationDescriptor* descriptor{nullptr};
  void* object{nullptr};
  template <typename Value>
  void operator()(Value* value) const noexcept {
    delete value;
  }
};

// The exact allocated type and the complete object behind a reference, as the SAME allocation (an aliasing
// reference sharing the original control block). Empty when the object was not allocated by make_ref.
struct RecoverableOwnerIdentity final {
  std::shared_ptr<void> object;
  std::type_index type{typeid(void)};
  std::shared_ptr<void> (*view_row_owner)(const std::shared_ptr<void>&){nullptr};
  std::shared_ptr<void> (*own_row_owner)(const std::shared_ptr<void>&){nullptr};
};

template <typename Source>
[[nodiscard]] std::optional<RecoverableOwnerIdentity> recoverable_owner_identity(const std::shared_ptr<Source>& source) {
  const auto* record = std::get_deleter<AllocationRecordingDeleter>(source);
  if (record == nullptr || record->descriptor == nullptr) return std::nullopt;
  return RecoverableOwnerIdentity{std::shared_ptr<void>(source, record->object), record->descriptor->type,
                                  record->descriptor->view_row_owner, record->descriptor->own_row_owner};
}

[[nodiscard]] inline const AllocationDescriptor* allocation_descriptor_of(const std::shared_ptr<void>& source) {
  const auto* record = std::get_deleter<AllocationRecordingDeleter>(source);
  return record == nullptr ? nullptr : record->descriptor;
}

} // namespace detail


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
    auto* raw = new Value(std::forward<Arguments>(arguments)...);
    std::shared_ptr<Value> reference(raw, detail::AllocationRecordingDeleter{&detail::allocation_descriptor<Value>,
                                                                             static_cast<void*>(raw)});
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

// The exact owner behind a reference, as `Target`, when the object was allocated as `Target` or as a
// type whose native base chain reaches it. It is the SAME allocation -- an aliasing reference sharing the
// original control block -- so identity, lifetime and every live cell are the source's own. Empty when
// the allocation is not a `Target` (or was not created through `make_ref`): this is a checked recovery,
// never a cast, and an empty answer is the caller's to act on.
template <typename Target, typename Source>
[[nodiscard]] Ref<Target> recover_owner(const std::shared_ptr<Source>& source) {
  const auto identity = detail::recoverable_owner_identity(source);
  if (!identity) return {};
  const auto* descriptor = detail::allocation_descriptor_of(source);
  void* object = descriptor->cast(identity->object.get(), std::type_index(typeid(Target)));
  if (object == nullptr) return {};
  return Ref<Target>(identity->object, static_cast<Target*>(object));
}

// The escape form of recover_owner, for a native boundary that REQUIRES the derived owner: the same
// allocation as `Target`, or the shared out-of-profile error naming what the object really is. It never
// answers with a copy, an empty reference, or a default.
template <typename Target, typename Source>
[[nodiscard]] Ref<Target> require_owner(const std::shared_ptr<Source>& source) {
  if (auto owner = recover_owner<Target>(source)) return owner;
  const auto identity = detail::recoverable_owner_identity(source);
  throw NativeScalarEscapeError(identity ? identity->type.name() : "an object not allocated by flight::make_ref");
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
