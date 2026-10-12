// Runtime completion of the generated partial displayObject module.
#pragma once

#include <optional>
#include <flight/runtime.hpp>
#include <flight/types/display_object.hpp>
#include <flight/types/entity.hpp>
#include <flight/types/material.hpp>
#include <flight/types/material2_d.hpp>
#include <flight/types/node2_d.hpp>

namespace flight::scene2d {

using Node2DRuntimeOwner =
    std::shared_ptr<flight::types::NodeRuntime<flight::Ref<flight::types::Node2DTraits>>>;

inline Node2DRuntimeOwner create_node2_d_runtime() {
  auto runtime = std::make_shared<
      flight::types::NodeRuntime<flight::Ref<flight::types::Node2DTraits>>>();
  runtime->appearance_id = 0.0;
  runtime->bounds_using_local_bounds_id = -1.0;
  runtime->bounds_using_local_transform_id = -1.0;
  runtime->can_add_child = [](const auto&, const auto&) { return true; };
  runtime->children = std::nullopt;
  runtime->children_id = 0.0;
  runtime->color_adjustments = std::nullopt;
  runtime->color_adjustments_unsupported = false;
  runtime->interaction_signals = std::nullopt;
  runtime->interaction_state = std::nullopt;
  runtime->local_bounds_id = 0.0;
  runtime->local_bounds_using_local_bounds_id = -1.0;
  runtime->local_content_id = 0.0;
  runtime->local_transform_id = 0.0;
  runtime->local_transform_using_local_transform_id = -1.0;
  runtime->node_signals = std::nullopt;
  runtime->parent = std::nullopt;
  runtime->parent_reference_id = 0.0;
  runtime->resolved_color_matrix = std::nullopt;
  runtime->resolved_color_scale_bias = std::nullopt;
  runtime->traits = flight::types::node2_dtraits_key;
  runtime->world_bounds_using_local_bounds_id = -1.0;
  runtime->world_bounds_using_world_transform_id = -1.0;
  runtime->world_transform_id = 0.0;
  runtime->world_transform_using_local_transform_id = -1.0;
  runtime->world_transform_using_parent_transform_id = -1.0;
  return runtime;
}

inline flight::types::DisplayObject make_display_object() {
  auto out = flight::make_structural_ref<flight::types::DisplayObject::schema_type>(
      flight::row_field<flight::RowKey<"enabled">>(true),
      flight::row_field<flight::RowKey<"kind">>(flight::types::display_object_kind),
      flight::row_field<flight::RowKey<"name">>(std::optional<flight::String>{}),
      flight::row_field<flight::RowKey<"alpha">>(1.0),
      flight::row_field<flight::RowKey<"visible">>(true),
      flight::row_field<flight::RowKey<"blendMode">>(std::optional<flight::String>{}),
      flight::row_field<flight::RowKey<"clip">>(
          std::optional<flight::Ref<flight::types::ClipRegion>>{}),
      flight::row_field<flight::RowKey<"material">>(
          std::optional<flight::Ref<flight::types::Material2D>>{}),
      flight::row_field<flight::RowKey<"materialData">>(
          std::optional<flight::Ref<flight::types::MaterialData>>{}),
      flight::row_field<flight::RowKey<"pivotX">>(0.0),
      flight::row_field<flight::RowKey<"pivotY">>(0.0),
      flight::row_field<flight::RowKey<"rotation">>(0.0),
      flight::row_field<flight::RowKey<"scaleX">>(1.0),
      flight::row_field<flight::RowKey<"scaleY">>(1.0),
      flight::row_field<flight::RowKey<"skewX">>(0.0),
      flight::row_field<flight::RowKey<"skewY">>(0.0),
      flight::row_field<flight::RowKey<"x">>(0.0),
      flight::row_field<flight::RowKey<"y">>(0.0));
  flight::row_set(out, flight::types::entity_runtime_key, create_node2_d_runtime());
  return out;
}

template <typename Options>
inline flight::types::DisplayObject create_node2_d(
    const flight::String&, Options&&, std::nullptr_t = nullptr, std::nullptr_t = nullptr) {
  return make_display_object();
}

template <typename Options>
inline flight::types::DisplayObject create_display_object(Options&&) {
  return make_display_object();
}

inline flight::types::DisplayObject create_display_object() {
  return make_display_object();
}

inline Node2DRuntimeOwner get_node2_d_runtime(const flight::types::Node2D& source) {
  return flight::row_get<Node2DRuntimeOwner>(source, flight::types::entity_runtime_key);
}

inline Node2DRuntimeOwner get_display_object_runtime(
    const flight::types::DisplayObject& source) {
  return get_node2_d_runtime(source);
}

inline bool is_node2_d(const flight::types::Node2D& node) {
  const auto runtime = get_node2_d_runtime(node);
  return runtime && runtime->traits.has_value() &&
         runtime->traits.value() == flight::types::node2_dtraits_key;
}

inline void set_node2_dclip(
    flight::types::Node2D source,
    std::optional<flight::Ref<flight::types::ClipRegion>> value) {
  flight::row_set<flight::RowKey<"clip">>(source, std::move(value));
  if (const auto runtime = get_node2_d_runtime(source)) {
    runtime->appearance_id = flight::unsigned_right_shift(runtime->appearance_id + 1.0, 0.0);
  }
}

} // namespace flight::scene2d
