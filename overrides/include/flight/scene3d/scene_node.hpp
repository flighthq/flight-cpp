// Runtime completion of scene3d/src/sceneNode.ts for flattened C++ owners.
#pragma once

#include <algorithm>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <variant>

#include <flight/geometry/quaternion.hpp>
#include <flight/geometry/vector3.hpp>
#include <flight/node/node.hpp>
#include <flight/runtime.hpp>
#include <flight/signals/emitter.hpp>
#include <flight/types/entity.hpp>
#include <flight/types/node_3d.hpp>
#include <flight/types/node_signals.hpp>

namespace flight::scene3d {

using flight::types::node_3d_kind;

using Node3DTraits = flight::Ref<flight::types::Node3DTraits>;
using Node3DView = flight::types::NodeOf<Node3DTraits>;
using Node3DBaseRuntime = flight::types::NodeRuntime<Node3DTraits>;
using Node3DBaseRuntimeOwner = flight::Ref<Node3DBaseRuntime>;

#ifndef FLIGHT_COMPILER_ANONYMOUS__FLIGHTHQ_SCENE3D_ENABLED_NAME_ALPHA_VISIBLE_6EED4C0384CBBF3F
#define FLIGHT_COMPILER_ANONYMOUS__FLIGHTHQ_SCENE3D_ENABLED_NAME_ALPHA_VISIBLE_6EED4C0384CBBF3F
struct enabled_name_alpha_visible_6eed4c0384cbbf3f
    : public flight::ReferenceEnabled {
  std::optional<bool> enabled;
  std::variant<flight::String, flight::Null, flight::Undefined> name{
      std::in_place_type<flight::Undefined>, flight::undefined};
  std::optional<double> alpha;
  std::optional<bool> visible;
};

#endif // FLIGHT_COMPILER_ANONYMOUS__FLIGHTHQ_SCENE3D_ENABLED_NAME_ALPHA_VISIBLE_6EED4C0384CBBF3F

namespace detail {

template <typename Value>
[[nodiscard]] std::optional<bool> optional_bool(const Value& value) {
  if constexpr (std::same_as<std::remove_cvref_t<Value>, bool>) {
    return value;
  } else if constexpr (requires { value.has_value(); value.value(); }) {
    if (!value.has_value()) return std::nullopt;
    return optional_bool(value.value());
  } else {
    return std::nullopt;
  }
}

template <typename Value>
[[nodiscard]] std::optional<double> optional_number(const Value& value) {
  if constexpr (std::same_as<std::remove_cvref_t<Value>, double>) {
    return value;
  } else if constexpr (requires { value.has_value(); value.value(); }) {
    if (!value.has_value()) return std::nullopt;
    return optional_number(value.value());
  } else {
    return std::nullopt;
  }
}

template <typename Value>
[[nodiscard]] std::optional<flight::String> optional_string(
    const Value& value) {
  if constexpr (std::same_as<std::remove_cvref_t<Value>, flight::String>) {
    return value;
  } else if constexpr (requires { value.has_value(); value.value(); }) {
    if (!value.has_value()) return std::nullopt;
    return optional_string(value.value());
  } else if constexpr (requires { std::get_if<flight::String>(&value); }) {
    if (const auto* string = std::get_if<flight::String>(&value)) return *string;
    return std::nullopt;
  } else {
    return std::nullopt;
  }
}

template <typename Options>
[[nodiscard]] std::optional<bool> option_enabled(const Options& options) {
  if constexpr (requires { options.has_value(); options.value(); }) {
    if (!options.has_value()) return std::nullopt;
    return option_enabled(options.value());
  } else if constexpr (requires { options->enabled; }) {
    return optional_bool(options->enabled);
  } else {
    return std::nullopt;
  }
}

template <typename Options>
[[nodiscard]] std::optional<flight::String> option_name(
    const Options& options) {
  if constexpr (requires { options.has_value(); options.value(); }) {
    if (!options.has_value()) return std::nullopt;
    return option_name(options.value());
  } else if constexpr (requires { options->name; }) {
    return optional_string(options->name);
  } else {
    return std::nullopt;
  }
}

template <typename Options>
[[nodiscard]] std::optional<double> option_alpha(const Options& options) {
  if constexpr (requires { options.has_value(); options.value(); }) {
    if (!options.has_value()) return std::nullopt;
    return option_alpha(options.value());
  } else if constexpr (requires { options->alpha; }) {
    return optional_number(options->alpha);
  } else {
    return std::nullopt;
  }
}

template <typename Options>
[[nodiscard]] std::optional<bool> option_visible(const Options& options) {
  if constexpr (requires { options.has_value(); options.value(); }) {
    if (!options.has_value()) return std::nullopt;
    return option_visible(options.value());
  } else if constexpr (requires { options->visible; }) {
    return optional_bool(options->visible);
  } else {
    return std::nullopt;
  }
}

template <typename Runtime>
[[nodiscard]] Node3DBaseRuntimeOwner base_runtime_alias(
    const std::shared_ptr<Runtime>& runtime) {
  return std::reinterpret_pointer_cast<Node3DBaseRuntime>(runtime);
}

template <typename Runtime>
inline void bind_base_runtime_alias(
    const std::shared_ptr<flight::RowOwner>& owner,
    const std::shared_ptr<Runtime>& runtime) {
  if (!owner) throw std::bad_weak_ptr();
  owner->set_dynamic_value(
      flight::types::entity_runtime_key,
      std::optional<Node3DBaseRuntimeOwner>{base_runtime_alias(runtime)});
}

template <typename Value, typename Runtime>
inline void bind_base_runtime_alias(const std::shared_ptr<Value>& source,
                                    const std::shared_ptr<Runtime>& runtime) {
  const auto row = flight::StructuralRef<
      flight::RowWritable<flight::RowOf<flight::Ref<Value>>>>(source);
  bind_base_runtime_alias(row.shared_owner(), runtime);
}

}  // namespace detail

template <typename Runtime>
inline void initialize_node_3d_runtime(const std::shared_ptr<Runtime>& runtime) {
  runtime->binding = std::nullopt;
  runtime->uid = std::nullopt;
  runtime->color_adjustments = std::nullopt;
  runtime->color_adjustments_unsupported = false;
  runtime->resolved_color_matrix = std::nullopt;
  runtime->resolved_color_scale_bias = std::nullopt;
  runtime->appearance_id = 0.0;
  runtime->bounds_using_local_bounds_id = -1.0;
  runtime->bounds_using_local_transform_id = -1.0;
  runtime->can_add_child = [](const auto&, const auto&) { return true; };
  runtime->children = std::nullopt;
  runtime->children_id = 0.0;
  runtime->traits = flight::types::node_3d_traits_key;
  runtime->interaction_signals = std::nullopt;
  runtime->local_bounds_id = 0.0;
  runtime->local_bounds_using_local_bounds_id = -1.0;
  runtime->local_content_id = 0.0;
  runtime->local_transform_id = 0.0;
  runtime->local_transform_using_local_transform_id = -1.0;
  runtime->node_signals = std::nullopt;
  runtime->interaction_state = std::nullopt;
  runtime->parent = std::nullopt;
  runtime->parent_reference_id = 0.0;
  runtime->world_bounds_using_local_bounds_id = -1.0;
  runtime->world_bounds_using_world_transform_id = -1.0;
  runtime->world_transform_id = 0.0;
  runtime->world_transform_using_local_transform_id = -1.0;
  runtime->world_transform_using_parent_transform_id = -1.0;
  runtime->world_alpha = std::nullopt;
  runtime->world_alpha_using_appearance_id = -1.0;
  runtime->world_alpha_using_parent_appearance_id = -1.0;
  runtime->world_appearance_id = 0.0;
  runtime->local_matrix4 = std::nullopt;
  runtime->local_matrix4_detached = false;
  runtime->world_matrix4 = std::nullopt;
}

inline flight::Ref<flight::types::Node3DRuntime> create_node_3d_runtime() {
  auto runtime = flight::make_ref<flight::types::Node3DRuntime>();
  initialize_node_3d_runtime(runtime);
  return runtime;
}

template <typename Runtime, typename Out, typename Options, typename Factory>
inline void initialize_node_3d(const Out& out, flight::String kind,
                               const Options& options, Factory create_runtime) {
  // The current emitter gives runtime factories one optional traits argument.
  // Older generated callers and the exact-owner factories have zero arguments.
  const auto runtime = [&] {
    if constexpr (std::is_invocable_v<Factory, std::nullopt_t>) {
      return create_runtime(std::nullopt);
    } else {
      return create_runtime();
    }
  }();
  flight::row_set<flight::RowKey<"data">>(
      out, std::optional<flight::Ref<flight::types::NodeData>>{});
  flight::row_set<flight::RowKey<"enabled">>(
      out, detail::option_enabled(options).value_or(true));
  flight::row_set<flight::RowKey<"kind">>(out, std::move(kind));
  flight::row_set<flight::RowKey<"name">>(out, detail::option_name(options));
  flight::row_set<flight::RowKey<"alpha">>(
      out, detail::option_alpha(options).value_or(1.0));
  flight::row_set<flight::RowKey<"visible">>(
      out, detail::option_visible(options).value_or(true));
  flight::row_set<flight::RowKey<"position">>(
      out, flight::geometry::create_vector3(std::nullopt, std::nullopt,
                                            std::nullopt));
  flight::row_set<flight::RowKey<"rotation">>(
      out, flight::geometry::create_quaternion(
               std::nullopt, std::nullopt, std::nullopt, std::nullopt));
  flight::row_set<flight::RowKey<"scale">>(
      out, flight::geometry::create_vector3(1.0, 1.0, 1.0));
  flight::row_set(out, flight::types::entity_runtime_key, runtime);
  detail::bind_base_runtime_alias(out.shared_owner(), runtime);
}

template <typename Options>
inline flight::Ref<flight::types::Node3D> create_node_3d(
    flight::String kind, const Options& options) {
  auto runtime = create_node_3d_runtime();
  auto out = flight::make_ref<flight::types::Node3D>(flight::types::Node3D{
      .data = std::nullopt,
      .enabled = detail::option_enabled(options).value_or(true),
      .kind = std::move(kind),
      .name = detail::option_name(options),
      .entity_runtime_key = runtime,
      .alpha = detail::option_alpha(options).value_or(1.0),
      .visible = detail::option_visible(options).value_or(true),
      .position = flight::geometry::create_vector3(
          std::nullopt, std::nullopt, std::nullopt),
      .rotation = flight::geometry::create_quaternion(
          std::nullopt, std::nullopt, std::nullopt, std::nullopt),
      .scale = flight::geometry::create_vector3(1.0, 1.0, 1.0),
  });
  detail::bind_base_runtime_alias(out, runtime);
  return out;
}

inline flight::Ref<flight::types::Node3D> create_node_3d(
    std::optional<flight::String> kind = std::nullopt,
    std::optional<flight::Ref<enabled_name_alpha_visible_6eed4c0384cbbf3f>>
        options = std::nullopt) {
  return create_node_3d(kind.value_or(flight::types::node_3d_kind), options);
}

template <typename Value>
inline Node3DView node3_dview(const std::shared_ptr<Value>& source) {
  if (!source) throw std::invalid_argument("Node3D source must be non-null");
  if constexpr (requires { source->entity_runtime_key; }) {
    if (!source->entity_runtime_key.has_value() ||
        !source->entity_runtime_key.value()) {
      throw std::logic_error("Node3D has no runtime owner");
    }
    detail::bind_base_runtime_alias(source,
                                    source->entity_runtime_key.value());
  }
  const auto row = flight::StructuralRef<
      flight::RowWritable<flight::RowOf<flight::Ref<Value>>>>(source);
  return Node3DView::from_owner(row.shared_owner());
}

inline Node3DView node3_dview(const Node3DView& source) { return source; }

template <typename Schema>
inline flight::Ref<flight::types::Node3DRuntime> get_node_3d_runtime(
    const flight::StructuralRef<Schema>& source) {
  const auto runtime = flight::row_get<std::optional<Node3DBaseRuntimeOwner>>(
      source, flight::types::entity_runtime_key);
  if (!runtime.has_value() || !runtime.value()) {
    throw std::logic_error("Node3D has no runtime owner");
  }
  return std::reinterpret_pointer_cast<flight::types::Node3DRuntime>(
      runtime.value());
}

template <typename Value>
inline flight::Ref<flight::types::Node3DRuntime> get_node_3d_runtime(
    const std::shared_ptr<Value>& source) {
  return get_node_3d_runtime(node3_dview(source));
}

template <typename Source>
inline flight::Ref<flight::types::NodeSignals> enable_node_3d_signals(
    const Source& source) {
  auto runtime = get_node_3d_runtime(source);
  if (!runtime->node_signals.has_value()) {
    runtime->node_signals = flight::node::create_node_signals();
  }
  return runtime->node_signals.value();
}

template <typename Source>
inline std::optional<flight::Ref<flight::types::NodeSignals>>
get_node_3d_signals(const Source& source) {
  return get_node_3d_runtime(source)->node_signals;
}

template <typename Source>
inline std::optional<Node3DView> get_node3_dparent(const Source& source) {
  return get_node_3d_runtime(source)->parent;
}

template <typename Source>
inline double get_node3_dchild_count(const Source& source) {
  const auto& children = get_node_3d_runtime(source)->children;
  return children.has_value() ? static_cast<double>(children.value().size())
                              : 0.0;
}

template <typename Source>
inline std::optional<Node3DView> get_node3_dchild_at(const Source& source,
                                                    double index) {
  const auto& children = get_node_3d_runtime(source)->children;
  if (!children.has_value() || index < 0.0 ||
      index >= static_cast<double>(children.value().size())) {
    return std::nullopt;
  }
  return children.value().element(index);
}

template <typename Value>
inline bool is_node_3d(const std::shared_ptr<Value>& source) {
  return static_cast<bool>(source) && requires { source->position; source->rotation;
                                                 source->scale; };
}

inline flight::types::NodeAny node3_dany(const Node3DView& source) {
  auto native = source.shared_native_object();
  if (!native) return nullptr;
  auto* node = reinterpret_cast<flight::types::Node<flight::Any>*>(native.get());
  return flight::types::NodeAny(std::move(native), node);
}

inline void invalidate_node3_dworld_bounds(const Node3DView& source) {
  auto current = std::optional<Node3DView>{source};
  while (current.has_value()) {
    const auto runtime = get_node_3d_runtime(current.value());
    runtime->world_bounds_using_world_transform_id = -1.0;
    runtime->world_bounds_using_local_bounds_id = -1.0;
    current = runtime->parent;
  }
}

inline Node3DView remove_node3_dchild(const Node3DView& target,
                                     const Node3DView& child) {
  if (!child) return child;

  const auto target_runtime = get_node_3d_runtime(target);
  const auto child_runtime = get_node_3d_runtime(child);
  if (!target_runtime->children.has_value() ||
      !child_runtime->parent.has_value() ||
      child_runtime->parent.value() != target) {
    return child;
  }

  auto& children = target_runtime->children.value();
  child_runtime->parent = std::nullopt;
  if (child_runtime->node_signals.has_value()) {
    flight::signals::emit_signal<std::function<void()>>(
        child_runtime->node_signals.value()->on_parent_changed);
  }
  child_runtime->parent_reference_id = flight::unsigned_right_shift(
      child_runtime->parent_reference_id + 1.0, 0.0);
  child_runtime->world_transform_using_parent_transform_id = -1.0;

  const double index = children.index_of(child);
  if (index != -1.0) {
    (void)children.splice(index, 1.0);
    target_runtime->children_id = flight::unsigned_right_shift(
        target_runtime->children_id + 1.0, 0.0);
    invalidate_node3_dworld_bounds(target);
  }

  if (target_runtime->node_signals.has_value()) {
    const auto& signals = target_runtime->node_signals.value();
    flight::signals::emit_signal<std::function<void(flight::types::NodeAny)>>(
        signals->on_child_removed, node3_dany(child));
    flight::signals::emit_signal<std::function<void()>>(
        signals->on_children_changed);
  }
  return child;
}

inline Node3DView add_node3_dchild_at(const Node3DView& target,
                                     const Node3DView& child, double index) {
  if (!child) {
    throw flight::TypeError(flight::String("Parameter child must be non-null"));
  }
  if (child == target) {
    throw flight::TypeError(
        flight::String("An object cannot be added as a child of itself"));
  }
  for (auto current = std::optional<Node3DView>{target}; current.has_value();
       current = get_node_3d_runtime(current.value())->parent) {
    if (current.value() == child) {
      throw flight::TypeError(flight::String(
          "An ancestor cannot be added as a child of its descendant"));
    }
  }

  const auto target_runtime = get_node_3d_runtime(target);
  const double child_count = target_runtime->children.has_value()
                                 ? static_cast<double>(
                                       target_runtime->children.value().size())
                                 : 0.0;
  if (index < 0.0 || index > child_count) {
    throw flight::RangeError(
        flight::String("The supplied index is out of bounds."));
  }
  if (!target_runtime->can_add_child(target, child)) {
    throw flight::TypeError(
        flight::String("The specified parent object cannot add this child"));
  }
  if (!target_runtime->children.has_value()) {
    target_runtime->children = flight::Array<Node3DView>{};
  }

  auto& children = target_runtime->children.value();
  const auto child_runtime = get_node_3d_runtime(child);
  const auto previous_parent = child_runtime->parent;
  bool was_child = false;
  if (previous_parent.has_value() && previous_parent.value() == target) {
    const double previous_index = children.index_of(child);
    if (previous_index != -1.0) {
      was_child = true;
      const double destination =
          std::min(index, static_cast<double>(children.size()) - 1.0);
      if (previous_index == destination) return child;
      (void)children.splice(previous_index, 1.0);
    }
  } else if (previous_parent.has_value()) {
    remove_node3_dchild(previous_parent.value(), child);
  }

  (void)children.splice(index, 0.0, child);
  target_runtime->children_id = flight::unsigned_right_shift(
      target_runtime->children_id + 1.0, 0.0);
  if (!was_child) invalidate_node3_dworld_bounds(target);

  if (target_runtime->node_signals.has_value()) {
    flight::signals::emit_signal<std::function<void()>>(
        target_runtime->node_signals.value()->on_children_changed);
  }
  if (!previous_parent.has_value() || previous_parent.value() != target) {
    child_runtime->parent = target;
    if (target_runtime->node_signals.has_value()) {
      flight::signals::emit_signal<std::function<void(flight::types::NodeAny)>>(
          target_runtime->node_signals.value()->on_child_added,
          node3_dany(child));
    }
    if (child_runtime->node_signals.has_value()) {
      flight::signals::emit_signal<std::function<void()>>(
          child_runtime->node_signals.value()->on_parent_changed);
    }
    child_runtime->parent_reference_id = flight::unsigned_right_shift(
        child_runtime->parent_reference_id + 1.0, 0.0);
    child_runtime->world_transform_using_parent_transform_id = -1.0;
  }
  return child;
}

}  // namespace flight::scene3d

namespace flight::node {

inline flight::scene3d::Node3DView add_node_child(
    const flight::scene3d::Node3DView& target,
    const flight::scene3d::Node3DView& child) {
  return flight::scene3d::add_node3_dchild_at(
      target, child, flight::scene3d::get_node3_dchild_count(target));
}

template <typename Parent, typename Child>
inline flight::scene3d::Node3DView add_node_child(
    const std::shared_ptr<Parent>& target,
    const std::shared_ptr<Child>& child) {
  const auto target_view = flight::scene3d::node3_dview(target);
  const auto child_view = flight::scene3d::node3_dview(child);
  return flight::scene3d::add_node3_dchild_at(
      target_view, child_view,
      flight::scene3d::get_node3_dchild_count(target_view));
}

inline flight::scene3d::Node3DView add_node_child_at(
    const flight::scene3d::Node3DView& target,
    const flight::scene3d::Node3DView& child, double index) {
  return flight::scene3d::add_node3_dchild_at(target, child, index);
}

template <typename Parent, typename Child>
inline flight::scene3d::Node3DView add_node_child_at(
    const std::shared_ptr<Parent>& target,
    const std::shared_ptr<Child>& child,
    double index) {
  const auto target_view = flight::scene3d::node3_dview(target);
  const auto child_view = flight::scene3d::node3_dview(child);
  return flight::scene3d::add_node3_dchild_at(target_view, child_view, index);
}

inline std::optional<flight::scene3d::Node3DView> get_node_parent(
    const flight::scene3d::Node3DView& source) {
  return flight::scene3d::get_node3_dparent(source);
}

template <typename Source>
inline std::optional<flight::scene3d::Node3DView> get_node_parent(
    const std::shared_ptr<Source>& source) {
  return flight::scene3d::get_node3_dparent(source);
}

inline double get_node_child_count(
    const flight::scene3d::Node3DView& source) {
  return flight::scene3d::get_node3_dchild_count(source);
}

template <typename Source>
inline double get_node_child_count(const std::shared_ptr<Source>& source) {
  return flight::scene3d::get_node3_dchild_count(source);
}

inline std::optional<flight::scene3d::Node3DView> get_node_child_at(
    const flight::scene3d::Node3DView& source, double index) {
  return flight::scene3d::get_node3_dchild_at(source, index);
}

template <typename Source>
inline std::optional<flight::scene3d::Node3DView> get_node_child_at(
    const std::shared_ptr<Source>& source, double index) {
  return flight::scene3d::get_node3_dchild_at(source, index);
}

inline flight::scene3d::Node3DView remove_node_child(
    const flight::scene3d::Node3DView& target,
    const flight::scene3d::Node3DView& child) {
  return flight::scene3d::remove_node3_dchild(target, child);
}

template <typename Parent, typename Child>
inline flight::scene3d::Node3DView remove_node_child(
    const std::shared_ptr<Parent>& target,
    const std::shared_ptr<Child>& child) {
  const auto target_view = flight::scene3d::node3_dview(target);
  const auto child_view = flight::scene3d::node3_dview(child);
  return flight::scene3d::remove_node3_dchild(target_view, child_view);
}

}  // namespace flight::node
