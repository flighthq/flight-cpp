// Runtime completion of the generated partial sceneNode module.
#pragma once

#include <optional>
#include <stdexcept>
#include <type_traits>
#include <variant>

#include <flight/geometry/quaternion.hpp>
#include <flight/geometry/vector3.hpp>
#include <flight/node/node.hpp>
#include <flight/runtime.hpp>
#include <flight/types/entity.hpp>
#include <flight/types/node3_d.hpp>
#include <flight/types/node_signals.hpp>

namespace flight::scene3d {

using Node3DBaseRuntime =
    flight::types::NodeRuntime<flight::Ref<flight::types::Node3DTraits>>;
using Node3DBaseRuntimeOwner = std::shared_ptr<Node3DBaseRuntime>;

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
[[nodiscard]] std::optional<flight::String> optional_string(const Value& value) {
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
[[nodiscard]] std::optional<flight::String> option_name(const Options& options) {
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

template <typename Schema>
[[nodiscard]] Node3DBaseRuntimeOwner base_runtime(
    const flight::StructuralRef<Schema>& source) {
  auto runtime = flight::row_get<Node3DBaseRuntimeOwner>(
      source, flight::types::entity_runtime_key);
  if (runtime) return runtime;
  const auto slot = flight::row_get<std::optional<Node3DBaseRuntimeOwner>>(
      source, flight::types::entity_runtime_key);
  return slot.value_or(nullptr);
}

} // namespace detail

inline flight::Ref<flight::types::Node3DRuntime> create_node3_druntime() {
  auto runtime = flight::make_ref<flight::types::Node3DRuntime>();
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
  runtime->traits = flight::types::node3_dtraits_key;
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
  return runtime;
}

template <typename Options>
inline flight::types::Node3D create_node3_d(
    flight::types::Kind kind, const Options& options) {
  auto runtime = create_node3_druntime();
  auto out = flight::make_structural_ref<flight::types::Node3D::schema_type>(
      flight::row_field<flight::RowKey<"data">>(
          std::optional<flight::Ref<flight::types::NodeData>>{}),
      flight::row_field<flight::RowKey<"enabled">>(
          detail::option_enabled(options).value_or(true)),
      flight::row_field<flight::RowKey<"kind">>(std::move(kind)),
      flight::row_field<flight::RowKey<"name">>(detail::option_name(options)),
      flight::row_field<flight::RowKey<"alpha">>(
          detail::option_alpha(options).value_or(1.0)),
      flight::row_field<flight::RowKey<"visible">>(
          detail::option_visible(options).value_or(true)),
      flight::row_field<flight::RowKey<"position">>(
          flight::geometry::create_vector3(std::nullopt, std::nullopt, std::nullopt)),
      flight::row_field<flight::RowKey<"rotation">>(
          flight::geometry::create_quaternion(
              std::nullopt, std::nullopt, std::nullopt, std::nullopt)),
      flight::row_field<flight::RowKey<"scale">>(
          flight::geometry::create_vector3(1.0, 1.0, 1.0)));
  flight::row_set(
      out, flight::types::entity_runtime_key,
      std::reinterpret_pointer_cast<Node3DBaseRuntime>(runtime));
  return out;
}

inline flight::types::Node3D create_node3_d(
    flight::types::Kind kind = flight::types::node3_dkind) {
  return create_node3_d(kind, std::nullopt);
}

template <typename Schema>
inline flight::Ref<flight::types::Node3DRuntime> get_node3_druntime(
    const flight::StructuralRef<Schema>& source) {
  const auto runtime = detail::base_runtime(source);
  if (!runtime) throw std::logic_error("Node3D has no runtime owner");
  return std::reinterpret_pointer_cast<flight::types::Node3DRuntime>(runtime);
}

template <typename Schema>
inline flight::Ref<flight::types::NodeSignals> enable_node3_dsignals(
    const flight::StructuralRef<Schema>& source) {
  auto runtime = get_node3_druntime(source);
  if (!runtime->node_signals.has_value()) {
    runtime->node_signals = flight::node::create_node_signals();
  }
  return runtime->node_signals.value();
}

template <typename Schema>
inline std::optional<flight::Ref<flight::types::NodeSignals>> get_node3_dsignals(
    const flight::StructuralRef<Schema>& source) {
  return get_node3_druntime(source)->node_signals;
}

} // namespace flight::scene3d
