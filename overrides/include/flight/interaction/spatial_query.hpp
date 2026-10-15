// Derived from the a627849 Interaction emission; preserves the existing explicit checked bounds-owner boundary.
#pragma once
#include <cmath>
#include <cstdint>
#include <flight/structural_ref.hpp>
#include <optional>
#include <random>
#include <flight/runtime.hpp>

static_assert(flight::runtime_contract.compiler_contract == "flight-runtime-contract/2", "Flight compiler/runtime contract mismatch");
static_assert(flight::runtime_contract.cpp_abi == 1, "Flight C++ runtime ABI mismatch");

#include <flight/types/node.hpp>

#include <flight/types/entity.hpp>

namespace flight::types { struct Node2DTraits; }
namespace flight::types { struct Rectangle; }
namespace flight::types { template <typename Traits> struct Node; }
namespace flight::types { template <typename Traits> struct NodeRuntime; }

#include <flight/geometry/rectangle.hpp>
#include <flight/node/bounds_rectangle.hpp>
#include <flight/node/bounds_rectangle_access.hpp>
#include <flight/node/node.hpp>
#include <flight/types/contract.hpp>

namespace flight::interaction {

using flight::types::NodeRuntime;
using flight::types::entity_runtime_key;

#ifndef FLIGHT_COMPILER_ANONYMOUS__FLIGHTHQ_INTERACTION_DATA_ENABLED_KIND_NAME_ENTITY_RUNTIME_KEY_ALPHA_VISIBLE_BLEND_MODE_CLIP_MATERIAL_MATERIAL_DATA_PIVOT_X_PIVOT_Y_ROTATION_SCALE_X_SCALE_Y_SKEW_X_SKEW_Y_X_Y_9C7FB764043005B0
#define FLIGHT_COMPILER_ANONYMOUS__FLIGHTHQ_INTERACTION_DATA_ENABLED_KIND_NAME_ENTITY_RUNTIME_KEY_ALPHA_VISIBLE_BLEND_MODE_CLIP_MATERIAL_MATERIAL_DATA_PIVOT_X_PIVOT_Y_ROTATION_SCALE_X_SCALE_Y_SKEW_X_SKEW_Y_X_Y_9C7FB764043005B0
struct data_enabled_kind_name_entity_runtime_key_alpha_visible_blend_mode_clip_material_material_data_pivot_x_pivot_y_rotation_scale_x_scale_y_skew_x_skew_y_x_y_9c7fb764043005b0 : public flight::ReferenceEnabled {
  std::optional<flight::Ref<flight::types::Node2DData>> data;
  bool enabled;
  flight::String kind;
  std::optional<flight::String> name;
  std::optional<std::shared_ptr<flight::types::NodeRuntime<flight::Ref<flight::types::Node2DTraits>>>> entity_runtime_key;
  double alpha;
  bool visible;
  std::optional<flight::String> blend_mode;
  std::optional<flight::Ref<flight::types::ClipRegion>> clip;
  std::optional<flight::Ref<flight::types::Material2D>> material;
  std::optional<flight::Ref<flight::types::MaterialData>> material_data;
  double pivot_x;
  double pivot_y;
  double rotation;
  double scale_x;
  double scale_y;
  double skew_x;
  double skew_y;
  double x;
  double y;
};
#endif // FLIGHT_COMPILER_ANONYMOUS__FLIGHTHQ_INTERACTION_DATA_ENABLED_KIND_NAME_ENTITY_RUNTIME_KEY_ALPHA_VISIBLE_BLEND_MODE_CLIP_MATERIAL_MATERIAL_DATA_PIVOT_X_PIVOT_Y_ROTATION_SCALE_X_SCALE_Y_SKEW_X_SKEW_Y_X_Y_9C7FB764043005B0

inline flight::Array<flight::types::Node2D> hit_test_area_query(flight::types::Node2D root, flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::Rectangle>>>> rect, std::optional<flight::Array<flight::types::Node2D>> out = std::nullopt) {
  out = out.value_or(flight::Array<flight::types::Node2D>{});
  if (!flight::row_get<flight::RowKey<"enabled">>(root)) {
    return out.value();
  }
  flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::Rectangle>>>> world_bounds = flight::node::get_node_world_bounds_rectangle(root);
  if (flight::geometry::intersects_rectangle(flight::structural_ref_cast<flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::types::RectangleLike>>>>(world_bounds), flight::structural_ref_cast<flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::types::RectangleLike>>>>(rect))) {
    out.value().push(root);
  }
  std::optional<flight::Array<flight::types::NodeOf<flight::Ref<flight::types::Node2DTraits>>>> children = flight::row_get<flight::RowKey<"children">>(flight::node::get_node_runtime<flight::Ref<flight::types::Node2DTraits>>(root));
  if (children.has_value()) {
    for (auto child : children.value()) {
      hit_test_area_query(flight::structural_ref_cast<flight::types::Node2D>(child), rect, out.value());
    }
  }
  return out.value();
}

inline flight::Array<flight::types::Node2D> hit_test_area_query_circle(flight::types::Node2D root, double cx, double cy, double radius, std::optional<flight::Array<flight::types::Node2D>> out = std::nullopt) {
  out = out.value_or(flight::Array<flight::types::Node2D>{});
  if (!flight::row_get<flight::RowKey<"enabled">>(root)) {
    return out.value();
  }
  flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::Rectangle>>>> b = flight::node::get_node_world_bounds_rectangle(root);
  const double near_x = flight::maximum(flight::row_get<flight::RowKey<"x">>(b), flight::minimum(cx, (flight::row_get<flight::RowKey<"x">>(b) + flight::row_get<flight::RowKey<"width">>(b))));
  const double near_y = flight::maximum(flight::row_get<flight::RowKey<"y">>(b), flight::minimum(cy, (flight::row_get<flight::RowKey<"y">>(b) + flight::row_get<flight::RowKey<"height">>(b))));
  const double dx = (cx - near_x);
  const double dy = (cy - near_y);
  if ((((dx * dx) + (dy * dy)) <= (radius * radius))) {
    out.value().push(root);
  }
  std::optional<flight::Array<flight::types::NodeOf<flight::Ref<flight::types::Node2DTraits>>>> children = flight::row_get<flight::RowKey<"children">>(flight::node::get_node_runtime<flight::Ref<flight::types::Node2DTraits>>(root));
  if (children.has_value()) {
    for (auto child : children.value()) {
      hit_test_area_query_circle(flight::structural_ref_cast<flight::types::Node2D>(child), cx, cy, radius, out.value());
    }
  }
  return out.value();
}

} // namespace flight::interaction
