#pragma once

#include <any>
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

struct RuntimeHierarchy final {
  std::mutex mutex;
  std::unordered_map<const void*, std::any> parents;
  std::unordered_map<const void*, std::vector<const void*>> children;
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
  hierarchy.parents.insert_or_assign(child_id, target);
  hierarchy.children[target_id].push_back(child_id);
  return child;
}

template <typename Source>
  requires requires(const Source& source) { detail::node_identity(source); }
inline std::optional<Source> get_node_parent(const Source& source) {
  auto& hierarchy = detail::runtime_hierarchy();
  const std::lock_guard lock(hierarchy.mutex);
  const auto found = hierarchy.parents.find(detail::node_identity(source));
  if (found == hierarchy.parents.end()) return std::nullopt;
  if (const auto* parent = std::any_cast<Source>(&found->second)) return *parent;
  return std::nullopt;
}

} // namespace flight::node
