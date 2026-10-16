// Re-derived from the generated partial displayObject module.
#pragma once

// PARTIAL: flattened Node2D storage retains NodeRuntime<Node2DTraits>, not the
// exact Node2DRuntime/DisplayObjectRuntime owner. The representable plain-node
// behavior stays available; exact-runtime and custom-factory paths fail loudly.
#include <flight/structural_ref.hpp>
#include <flight/symbol.hpp>
#include <functional>
#include <optional>
#include <utility>
#include <flight/runtime.hpp>

static_assert(flight::runtime_contract.compiler_contract == "flight-runtime-contract/2", "Flight compiler/runtime contract mismatch");
static_assert(flight::runtime_contract.cpp_abi == 1, "Flight C++ runtime ABI mismatch");

#include <flight/types/scene_2d.hpp>
#include <flight/types/rectangle.hpp>
#include <flight/types/node_2d.hpp>
#include <flight/types/node.hpp>
#include <flight/types/clip_region.hpp>
#include <flight/types/display_object.hpp>

namespace flight::types { struct ClipRegion; }
namespace flight::types { struct DisplayObjectRuntime; }
namespace flight::types { struct Node2DData; }
namespace flight::types { struct Node2DTraits; }
namespace flight::types { struct NodeTraits; }
namespace flight::types { template <typename Traits> struct Node; }
namespace flight::types { template <typename Traits> struct NodeRuntime; }

#include <flight/node/node.hpp>
#include <flight/node/revision.hpp>
#include <flight/types/contract.hpp>

namespace flight::scene2d {

using flight::types::Node;
using flight::types::Node2D;
using flight::types::Node2DTraits;

using Node2DRuntimeOwner =
    std::shared_ptr<flight::types::NodeRuntime<flight::Ref<Node2DTraits>>>;

inline Node2DRuntimeOwner create_node_2d_runtime() {
  auto runtime = flight::node::create_node_runtime<flight::Ref<Node2DTraits>>();
  runtime->traits = flight::types::node_2d_traits_key;
  return runtime;
}

inline Node2DRuntimeOwner create_node_2d_runtime(std::nullopt_t) {
  return create_node_2d_runtime();
}

template <typename Methods>
inline Node2DRuntimeOwner create_node_2d_runtime(Methods&&) {
  throw flight::Error(flight::String(
      "createNode2DRuntime methods require the unavailable exact Node2DRuntime owner"));
}

inline Node2DRuntimeOwner create_display_object_runtime() {
  return create_node_2d_runtime();
}

template <typename Key, typename Schema, typename Value>
inline Value node_2d_option_value_or(
    const std::optional<flight::StructuralRef<Schema>>& obj, Value fallback) {
  if (!obj.has_value()) return fallback;
  const auto value = flight::row_get<Key>(obj.value());
  return value.has_value() ? value.value() : fallback;
}

template <typename Key, typename Schema, typename Value>
inline std::optional<Value> node_2d_optional_value(
    const std::optional<flight::StructuralRef<Schema>>& obj) {
  if (!obj.has_value()) return std::nullopt;
  return flight::row_get<Key>(obj.value());
}

template <typename Schema>
inline Node2D make_node_2d(
    flight::String kind,
    const std::optional<flight::StructuralRef<Schema>>& obj) {
  auto out = flight::make_structural_ref<Node2D::schema_type>(
      flight::row_field<flight::RowKey<"enabled">>(
          node_2d_option_value_or<flight::RowKey<"enabled">>(obj, true)),
      flight::row_field<flight::RowKey<"kind">>(std::move(kind)),
      flight::row_field<flight::RowKey<"name">>(
          node_2d_optional_value<flight::RowKey<"name">, Schema, flight::String>(obj)),
      flight::row_field<flight::RowKey<"alpha">>(
          node_2d_option_value_or<flight::RowKey<"alpha">>(obj, 1.0)),
      flight::row_field<flight::RowKey<"visible">>(
          node_2d_option_value_or<flight::RowKey<"visible">>(obj, true)),
      flight::row_field<flight::RowKey<"blendMode">>(
          node_2d_optional_value<flight::RowKey<"blendMode">, Schema, flight::String>(obj)),
      flight::row_field<flight::RowKey<"clip">>(
          node_2d_optional_value<flight::RowKey<"clip">, Schema,
                                 flight::Ref<flight::types::ClipRegion>>(obj)),
      flight::row_field<flight::RowKey<"material">>(
          node_2d_optional_value<flight::RowKey<"material">, Schema,
                                 flight::Ref<flight::types::Material2D>>(obj)),
      flight::row_field<flight::RowKey<"materialData">>(
          node_2d_optional_value<flight::RowKey<"materialData">, Schema,
                                 flight::Ref<flight::types::MaterialData>>(obj)),
      flight::row_field<flight::RowKey<"pivotX">>(
          node_2d_option_value_or<flight::RowKey<"pivotX">>(obj, 0.0)),
      flight::row_field<flight::RowKey<"pivotY">>(
          node_2d_option_value_or<flight::RowKey<"pivotY">>(obj, 0.0)),
      flight::row_field<flight::RowKey<"rotation">>(
          node_2d_option_value_or<flight::RowKey<"rotation">>(obj, 0.0)),
      flight::row_field<flight::RowKey<"scaleX">>(
          node_2d_option_value_or<flight::RowKey<"scaleX">>(obj, 1.0)),
      flight::row_field<flight::RowKey<"scaleY">>(
          node_2d_option_value_or<flight::RowKey<"scaleY">>(obj, 1.0)),
      flight::row_field<flight::RowKey<"skewX">>(
          node_2d_option_value_or<flight::RowKey<"skewX">>(obj, 0.0)),
      flight::row_field<flight::RowKey<"skewY">>(
          node_2d_option_value_or<flight::RowKey<"skewY">>(obj, 0.0)),
      flight::row_field<flight::RowKey<"x">>(
          node_2d_option_value_or<flight::RowKey<"x">>(obj, 0.0)),
      flight::row_field<flight::RowKey<"y">>(
          node_2d_option_value_or<flight::RowKey<"y">>(obj, 0.0)));
  flight::row_set(out, flight::types::entity_runtime_key, create_node_2d_runtime());
  return out;
}

using Node2DOptions = std::optional<flight::StructuralRef<flight::RowReadonly<
    flight::RowPartial<flight::RowOf<flight::Ref<Node2DTraits>>>>>>;

inline Node2D create_node_2d(flight::String kind, Node2DOptions obj = std::nullopt) {
  return make_node_2d(std::move(kind), obj);
}

template <typename Runtime, typename Options, typename DataFactory,
          typename RuntimeFactory>
inline Node2D create_node_2d(flight::String, Options&&, DataFactory&&,
                             RuntimeFactory&&) {
  throw flight::Error(flight::String(
      "createNode2D custom data/runtime factories require the unavailable exact Node2DRuntime owner"));
}

using DisplayObjectOptions = Node2DOptions;

inline flight::types::DisplayObject create_display_object(
    DisplayObjectOptions obj = std::nullopt) {
  return flight::structural_ref_cast<flight::types::DisplayObject>(
      make_node_2d(flight::types::display_object_kind, obj));
}

inline Node2DRuntimeOwner get_node_2d_runtime(const Node2D& source) {
  return flight::row_get<Node2DRuntimeOwner>(source, flight::types::entity_runtime_key);
}

inline Node2DRuntimeOwner get_display_object_runtime(
    const flight::types::DisplayObject& source) {
  return get_node_2d_runtime(source);
}

inline bool is_node_2d(flight::types::NodeAny node) {
  if (!node || !node->entity_runtime_key.has_value()) return false;
  const auto& runtime = node->entity_runtime_key.value();
  return runtime && runtime->traits.has_value() &&
         runtime->traits.value() == flight::types::node_2d_traits_key;
}

inline bool is_node_2d(const Node2D& node) {
  const auto runtime = get_node_2d_runtime(node);
  return runtime && runtime->traits.has_value() &&
         runtime->traits.value() == flight::types::node_2d_traits_key;
}

inline void set_node_2d_clip(
    Node2D source, std::optional<flight::Ref<flight::types::ClipRegion>> value) {
  flight::row_set<flight::RowKey<"clip">>(source, std::move(value));
  if (const auto runtime = get_node_2d_runtime(source)) {
    runtime->appearance_id =
        flight::unsigned_right_shift(runtime->appearance_id + 1.0, 0.0);
  }
}

} // namespace flight::scene2d
