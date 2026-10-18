#pragma once

#include <algorithm>
#include <any>
#include <memory>
#include <mutex>
#include <optional>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace flight::node {

namespace detail {

template <typename Value>
[[nodiscard]] const void* node_identity(const std::shared_ptr<Value>& value) noexcept {
  return value.get();
}

template <typename Schema>
[[nodiscard]] const void* node_identity(const flight::StructuralRef<Schema>& value) noexcept {
  return value.shared_owner().get();
}

// The allocation a node identity stands for, held weakly. An address names a node only while that node is
// alive: once it dies its storage may be reused by a new node, and an entry keyed by the bare address would
// then answer for an object it never described. Every entry keeps this witness, and a lookup accepts it only
// when the witness is the SAME allocation as the node asking (same control block), never by address alone.
template <typename Value>
[[nodiscard]] std::weak_ptr<void> node_liveness(const std::shared_ptr<Value>& value) noexcept {
  return std::static_pointer_cast<void>(std::const_pointer_cast<std::remove_const_t<Value>>(value));
}

template <typename Schema>
[[nodiscard]] std::weak_ptr<void> node_liveness(const flight::StructuralRef<Schema>& value) noexcept {
  return std::static_pointer_cast<void>(value.shared_owner());
}

template <typename Source>
[[nodiscard]] bool same_node_allocation(const std::weak_ptr<void>& witness, const Source& source) noexcept {
  const auto current = node_liveness(source);
  return !witness.expired() && !witness.owner_before(current) && !current.owner_before(witness);
}

struct RuntimeParentEntry final {
  std::weak_ptr<void> child;
  std::any parent;
};

struct RuntimeChildrenEntry final {
  std::weak_ptr<void> parent;
  std::vector<const void*> children;
};

struct RuntimeHierarchy final {
  std::mutex mutex;
  std::unordered_map<const void*, RuntimeParentEntry> parents;
  std::unordered_map<const void*, RuntimeChildrenEntry> children;
};

inline RuntimeHierarchy& runtime_hierarchy() {
  static RuntimeHierarchy hierarchy;
  return hierarchy;
}

template <typename Target>
concept ConcreteRuntimeNode = requires(const Target& target) {
  target->entity_runtime_key;
};

template <ConcreteRuntimeNode Target>
[[nodiscard]] auto concrete_node_runtime(const Target& target) {
  using Runtime = typename std::remove_cvref_t<decltype(target->entity_runtime_key)>::value_type;
  return target && target->entity_runtime_key.has_value() ? target->entity_runtime_key.value()
                                                          : Runtime{};
}

template <ConcreteRuntimeNode Target, typename Member>
inline void increment_runtime_revision(const Target& target, Member member) {
  if (auto runtime = concrete_node_runtime(target)) {
    runtime.get()->*member = flight::unsigned_right_shift((runtime.get()->*member) + 1.0, 0.0);
  }
}

} // namespace detail

// Generated concrete node shapes flatten TypeScript intersections instead of inheriting from
// Node<Traits>. These overloads preserve the revision contract for those flattened owners.
template <detail::ConcreteRuntimeNode Target>
inline void invalidate_node_appearance(const Target& target) {
  if (auto runtime = detail::concrete_node_runtime(target)) {
    runtime->appearance_id = flight::unsigned_right_shift(runtime->appearance_id + 1.0, 0.0);
  }
}

template <detail::ConcreteRuntimeNode Target>
inline void invalidate_node_local_bounds(const Target& target) {
  if (auto runtime = detail::concrete_node_runtime(target)) {
    runtime->local_bounds_id = flight::unsigned_right_shift(runtime->local_bounds_id + 1.0, 0.0);
  }
}

template <detail::ConcreteRuntimeNode Target>
inline void invalidate_node_local_content(const Target& target) {
  if (auto runtime = detail::concrete_node_runtime(target)) {
    runtime->local_content_id = flight::unsigned_right_shift(runtime->local_content_id + 1.0, 0.0);
  }
}

template <detail::ConcreteRuntimeNode Target>
inline void invalidate_content(const Target& target) {
  invalidate_node_local_content(target);
  invalidate_node_local_bounds(target);
}

template <detail::ConcreteRuntimeNode Target>
inline void invalidate_node_local_transform(const Target& target) {
  if (auto runtime = detail::concrete_node_runtime(target)) {
    runtime->local_transform_id =
        flight::unsigned_right_shift(runtime->local_transform_id + 1.0, 0.0);
  }
}

template <detail::ConcreteRuntimeNode Target>
inline void invalidate_node_parent_reference(const Target& target) {
  if (auto runtime = detail::concrete_node_runtime(target)) {
    runtime->parent_reference_id =
        flight::unsigned_right_shift(runtime->parent_reference_id + 1.0, 0.0);
    runtime->world_transform_using_parent_transform_id = -1.0;
  }
}

template <detail::ConcreteRuntimeNode Target>
inline void invalidate_node_render(const Target& target) {
  invalidate_node_appearance(target);
  invalidate_node_local_transform(target);
}

template <detail::ConcreteRuntimeNode Target>
inline void invalidate_node_world_bounds(const Target& target) {
  if (auto runtime = detail::concrete_node_runtime(target)) {
    runtime->world_bounds_using_local_bounds_id = -1.0;
    runtime->world_bounds_using_world_transform_id = -1.0;
  }
}

template <detail::ConcreteRuntimeNode Target>
inline void invalidate_node(const Target& target) {
  invalidate_node_appearance(target);
  invalidate_node_local_bounds(target);
  invalidate_node_local_content(target);
  invalidate_node_local_transform(target);
  invalidate_node_parent_reference(target);
  invalidate_node_world_bounds(target);
}

// Mixed generated node shapes cannot inhabit NodeOf<Traits>' nominal child array. Keep their
// identity graph in a type-erased runtime side table while preserving the public value types.
template <typename Target, typename Child>
  requires requires(const Target& target, const Child& child) {
    detail::node_identity(target);
    detail::node_identity(child);
  }
inline Child add_node_child(const Target& target, const Child& child) {
  auto& hierarchy = detail::runtime_hierarchy();
  const auto target_id = detail::node_identity(target);
  const auto child_id = detail::node_identity(child);
  const std::lock_guard lock(hierarchy.mutex);
  hierarchy.parents.insert_or_assign(child_id, detail::RuntimeParentEntry{detail::node_liveness(child), target});
  auto& children = hierarchy.children[target_id];
  // A list left by a dead node at this address describes that node, not this one.
  if (!detail::same_node_allocation(children.parent, target)) children = {detail::node_liveness(target), {}};
  children.children.push_back(child_id);
  return child;
}

template <typename Source>
  requires requires(const Source& source) { detail::node_identity(source); }
inline std::optional<Source> get_node_parent(const Source& source) {
  auto& hierarchy = detail::runtime_hierarchy();
  const std::lock_guard lock(hierarchy.mutex);
  const auto found = hierarchy.parents.find(detail::node_identity(source));
  if (found == hierarchy.parents.end()) return std::nullopt;
  if (!detail::same_node_allocation(found->second.child, source)) {
    // The entry belonged to a node that died at this address; it is not this node's parent.
    hierarchy.parents.erase(found);
    return std::nullopt;
  }
  if (const auto* parent = std::any_cast<Source>(&found->second.parent)) return *parent;
  return std::nullopt;
}

} // namespace flight::node

namespace flight::media {

// The portable runtime has no host audio graph. Bus values remain authoritative CPU state; these
// hooks deliberately do nothing until a host capability supplies graph-node handles.
template <typename Host, typename Bus>
inline void update_bus_gain_node(const Host&, const Bus&) {}

template <typename Host, typename Bus>
inline void update_bus_panner_node(const Host&, const Bus&) {}

inline double clamp(double value, double low, double high) {
  return std::clamp(value, low, high);
}

} // namespace flight::media
